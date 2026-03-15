/* Dcm.c — Diagnostic Communication Manager implementation (ADR-005 / P6).
 *
 * UDS service dispatch with session management.  Processes requests
 * synchronously and produces responses per ISO 14229-1.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Dcm.h"
#include "Dem.h"
#include "Csm.h"
#include "NvM.h"
#include "EcuM.h"
#include <string.h>
#include <stddef.h>

/* ── Internal state ─────────────────────────────────────────────── */

static boolean g_initialized = FALSE;
static const Dcm_ConfigType* g_config = NULL;

static uint8  g_session = DCM_SESSION_DEFAULT;
static uint8  g_security_level = DCM_SECURITY_LOCKED;
static boolean g_seed_pending = FALSE;
static uint8  g_seed[16];
static uint32 g_seed_len = 0;

/* ── Helpers ────────────────────────────────────────────────────── */

static uint16 nrc_response(uint8* resp, uint8 sid, uint8 nrc) {
    resp[0] = 0x7Fu;
    resp[1] = sid;
    resp[2] = nrc;
    return 3;
}

static uint16 positive_response(uint8* resp, uint8 sid) {
    resp[0] = sid + 0x40u;
    return 1;
}

/* ── DID lookup ─────────────────────────────────────────────────── */

static const Dcm_DidEntryType* find_did(uint16 did) {
    if (g_config == NULL) { return NULL; }
    for (uint16 i = 0; i < g_config->numDids; i++) {
        if (g_config->dids[i].did == did) {
            return &g_config->dids[i];
        }
    }
    return NULL;
}

static const Dcm_RoutineEntryType* find_routine(uint16 routineId) {
    if (g_config == NULL) { return NULL; }
    for (uint16 i = 0; i < g_config->numRoutines; i++) {
        if (g_config->routines[i].routineId == routineId) {
            return &g_config->routines[i];
        }
    }
    return NULL;
}

/* ── Service handlers ───────────────────────────────────────────── */

static uint16 handle_session_control(const uint8* req, uint16 reqLen,
                                      uint8* resp, uint16 bufSize)
{
    (void)bufSize;
    if (reqLen < 2u) { return nrc_response(resp, DCM_SID_DIAGNOSTIC_SESSION_CONTROL, DCM_NRC_INCORRECT_LENGTH); }
    uint8 sub = req[1];
    if (sub != DCM_SESSION_DEFAULT && sub != DCM_SESSION_EXTENDED && sub != DCM_SESSION_PROGRAMMING) {
        return nrc_response(resp, DCM_SID_DIAGNOSTIC_SESSION_CONTROL, DCM_NRC_SUBFUNCTION_NOT_SUPPORTED);
    }
    g_session = sub;
    /* On session change, reset security */
    if (sub == DCM_SESSION_DEFAULT) {
        g_security_level = DCM_SECURITY_LOCKED;
    }
    resp[0] = DCM_SID_DIAGNOSTIC_SESSION_CONTROL + 0x40u;
    resp[1] = sub;
    return 2;
}

static uint16 handle_ecu_reset(const uint8* req, uint16 reqLen,
                                uint8* resp, uint16 bufSize)
{
    (void)bufSize;
    if (reqLen < 2u) { return nrc_response(resp, DCM_SID_ECU_RESET, DCM_NRC_INCORRECT_LENGTH); }
    uint8 sub = req[1];
    resp[0] = DCM_SID_ECU_RESET + 0x40u;
    resp[1] = sub;
    /* Actual reset would be triggered after response is sent */
    return 2;
}

static uint16 handle_clear_dtc(const uint8* req, uint16 reqLen,
                                uint8* resp, uint16 bufSize)
{
    (void)bufSize;
    if (reqLen < 4u) { return nrc_response(resp, DCM_SID_CLEAR_DTC, DCM_NRC_INCORRECT_LENGTH); }
    uint32 dtcGroup = ((uint32)req[1] << 16u) | ((uint32)req[2] << 8u) | (uint32)req[3];
    Dem_ClearDTC(dtcGroup);
    return positive_response(resp, DCM_SID_CLEAR_DTC);
}

static uint16 handle_read_dtc_info(const uint8* req, uint16 reqLen,
                                    uint8* resp, uint16 bufSize)
{
    if (reqLen < 2u) { return nrc_response(resp, DCM_SID_READ_DTC_INFO, DCM_NRC_INCORRECT_LENGTH); }
    uint8 sub = req[1];

    if (sub == 0x01u) {
        /* reportNumberOfDTCByStatusMask */
        if (reqLen < 3u) { return nrc_response(resp, DCM_SID_READ_DTC_INFO, DCM_NRC_INCORRECT_LENGTH); }
        uint8 mask = req[2];
        uint16 count = Dem_GetNumberOfDTCByStatusMask(mask);
        resp[0] = DCM_SID_READ_DTC_INFO + 0x40u;
        resp[1] = sub;
        resp[2] = 0xFFu; /* statusAvailabilityMask */
        resp[3] = 0x00u; /* DTCFormatIdentifier */
        resp[4] = (uint8)(count >> 8u);
        resp[5] = (uint8)(count & 0xFFu);
        return 6;
    }

    if (sub == 0x02u) {
        /* reportDTCByStatusMask */
        if (reqLen < 3u) { return nrc_response(resp, DCM_SID_READ_DTC_INFO, DCM_NRC_INCORRECT_LENGTH); }
        uint8 mask = req[2];
        resp[0] = DCM_SID_READ_DTC_INFO + 0x40u;
        resp[1] = sub;
        resp[2] = 0xFFu; /* statusAvailabilityMask */
        uint16 pos = 3;
        uint16 numDtcs = Dem_GetNumberOfDTCByStatusMask(mask);
        for (uint16 i = 0; i < numDtcs && pos + 4u <= bufSize; i++) {
            uint32 dtcNum = 0;
            if (Dem_GetDTCByOccurrenceIndex(i, &dtcNum) == E_OK) {
                uint8 st = 0;
                Dem_GetDTCStatus(dtcNum, &st);
                if ((st & mask) != 0u) {
                    resp[pos++] = (uint8)(dtcNum >> 16u);
                    resp[pos++] = (uint8)(dtcNum >> 8u);
                    resp[pos++] = (uint8)(dtcNum);
                    resp[pos++] = st;
                }
            }
        }
        return pos;
    }

    return nrc_response(resp, DCM_SID_READ_DTC_INFO, DCM_NRC_SUBFUNCTION_NOT_SUPPORTED);
}

static uint16 handle_read_did(const uint8* req, uint16 reqLen,
                               uint8* resp, uint16 bufSize)
{
    if (reqLen < 3u) { return nrc_response(resp, DCM_SID_READ_DATA_BY_ID, DCM_NRC_INCORRECT_LENGTH); }
    uint16 did = ((uint16)req[1] << 8u) | (uint16)req[2];
    const Dcm_DidEntryType* entry = find_did(did);
    if (entry == NULL || entry->readFn == NULL) {
        return nrc_response(resp, DCM_SID_READ_DATA_BY_ID, DCM_NRC_REQUEST_OUT_OF_RANGE);
    }
    resp[0] = DCM_SID_READ_DATA_BY_ID + 0x40u;
    resp[1] = req[1];
    resp[2] = req[2];
    uint16 dataLen = entry->length;
    if (3u + dataLen > bufSize) {
        return nrc_response(resp, DCM_SID_READ_DATA_BY_ID, DCM_NRC_GENERAL_REJECT);
    }
    if (entry->readFn(&resp[3], &dataLen) != E_OK) {
        return nrc_response(resp, DCM_SID_READ_DATA_BY_ID, DCM_NRC_CONDITIONS_NOT_CORRECT);
    }
    return 3u + dataLen;
}

static uint16 handle_security_access(const uint8* req, uint16 reqLen,
                                      uint8* resp, uint16 bufSize)
{
    (void)bufSize;
    if (reqLen < 2u) { return nrc_response(resp, DCM_SID_SECURITY_ACCESS, DCM_NRC_INCORRECT_LENGTH); }
    if (g_session == DCM_SESSION_DEFAULT) {
        return nrc_response(resp, DCM_SID_SECURITY_ACCESS, DCM_NRC_CONDITIONS_NOT_CORRECT);
    }

    uint8 sub = req[1];

    if (sub == 0x01u) {
        /* Request seed */
        g_seed_len = 16;
        if (Csm_SeedGenerate(g_seed, &g_seed_len) != E_OK) {
            return nrc_response(resp, DCM_SID_SECURITY_ACCESS, DCM_NRC_CONDITIONS_NOT_CORRECT);
        }
        g_seed_pending = TRUE;
        resp[0] = DCM_SID_SECURITY_ACCESS + 0x40u;
        resp[1] = sub;
        memcpy(&resp[2], g_seed, g_seed_len);
        return 2u + (uint16)g_seed_len;
    }

    if (sub == 0x02u) {
        /* Send key */
        if (!g_seed_pending) {
            return nrc_response(resp, DCM_SID_SECURITY_ACCESS, DCM_NRC_CONDITIONS_NOT_CORRECT);
        }
        g_seed_pending = FALSE;
        uint16 keyLen = reqLen - 2u;
        if (Csm_KeyValidate(&req[2], (uint32)keyLen) != E_OK) {
            return nrc_response(resp, DCM_SID_SECURITY_ACCESS, DCM_NRC_INVALID_KEY);
        }
        g_security_level = DCM_SECURITY_LEVEL1;
        resp[0] = DCM_SID_SECURITY_ACCESS + 0x40u;
        resp[1] = sub;
        return 2;
    }

    return nrc_response(resp, DCM_SID_SECURITY_ACCESS, DCM_NRC_SUBFUNCTION_NOT_SUPPORTED);
}

static uint16 handle_write_did(const uint8* req, uint16 reqLen,
                                uint8* resp, uint16 bufSize)
{
    (void)bufSize;
    if (reqLen < 3u) { return nrc_response(resp, DCM_SID_WRITE_DATA_BY_ID, DCM_NRC_INCORRECT_LENGTH); }
    uint16 did = ((uint16)req[1] << 8u) | (uint16)req[2];
    const Dcm_DidEntryType* entry = find_did(did);
    if (entry == NULL || entry->writeFn == NULL) {
        return nrc_response(resp, DCM_SID_WRITE_DATA_BY_ID, DCM_NRC_REQUEST_OUT_OF_RANGE);
    }
    uint16 dataLen = reqLen - 3u;
    if (entry->writeFn(&req[3], dataLen) != E_OK) {
        return nrc_response(resp, DCM_SID_WRITE_DATA_BY_ID, DCM_NRC_CONDITIONS_NOT_CORRECT);
    }
    resp[0] = DCM_SID_WRITE_DATA_BY_ID + 0x40u;
    resp[1] = req[1];
    resp[2] = req[2];
    return 3;
}

static uint16 handle_routine_control(const uint8* req, uint16 reqLen,
                                      uint8* resp, uint16 bufSize)
{
    if (reqLen < 4u) { return nrc_response(resp, DCM_SID_ROUTINE_CONTROL, DCM_NRC_INCORRECT_LENGTH); }
    uint8 sub = req[1]; /* 0x01=start, 0x02=stop, 0x03=requestResults */
    uint16 routineId = ((uint16)req[2] << 8u) | (uint16)req[3];
    const Dcm_RoutineEntryType* entry = find_routine(routineId);
    if (entry == NULL || entry->startFn == NULL) {
        return nrc_response(resp, DCM_SID_ROUTINE_CONTROL, DCM_NRC_REQUEST_OUT_OF_RANGE);
    }
    if (sub != 0x01u) {
        return nrc_response(resp, DCM_SID_ROUTINE_CONTROL, DCM_NRC_SUBFUNCTION_NOT_SUPPORTED);
    }
    resp[0] = DCM_SID_ROUTINE_CONTROL + 0x40u;
    resp[1] = sub;
    resp[2] = req[2];
    resp[3] = req[3];
    uint16 statusLen = bufSize - 4u;
    const uint8* optRec = (reqLen > 4u) ? &req[4] : NULL;
    uint16 optLen = (reqLen > 4u) ? (reqLen - 4u) : 0u;
    if (entry->startFn(optRec, optLen, &resp[4], &statusLen) != E_OK) {
        return nrc_response(resp, DCM_SID_ROUTINE_CONTROL, DCM_NRC_CONDITIONS_NOT_CORRECT);
    }
    return 4u + statusLen;
}

static uint16 handle_tester_present(const uint8* req, uint16 reqLen,
                                     uint8* resp, uint16 bufSize)
{
    (void)bufSize;
    if (reqLen < 2u) { return nrc_response(resp, DCM_SID_TESTER_PRESENT, DCM_NRC_INCORRECT_LENGTH); }
    uint8 sub = req[1];
    if (sub == 0x80u) {
        /* Suppress positive response */
        return 0;
    }
    resp[0] = DCM_SID_TESTER_PRESENT + 0x40u;
    resp[1] = 0x00u;
    return 2;
}

/* ── Lifecycle ──────────────────────────────────────────────────── */

void Dcm_Init(const Dcm_ConfigType* config) {
    g_config = config;
    g_session = DCM_SESSION_DEFAULT;
    g_security_level = DCM_SECURITY_LOCKED;
    g_seed_pending = FALSE;
    g_seed_len = 0;
    g_initialized = TRUE;
}

void Dcm_DeInit(void) {
    g_initialized = FALSE;
    g_config = NULL;
}

void Dcm_MainFunction(void) {
    /* Session timeout handling would go here. */
    (void)0;
}

/* ── Processing ─────────────────────────────────────────────────── */

uint16 Dcm_ProcessRequest(const uint8* reqData, uint16 reqLen,
                          uint8* respData, uint16 respBufSize)
{
    if (!g_initialized || reqData == NULL || respData == NULL || reqLen == 0u) {
        return 0;
    }

    uint8 sid = reqData[0];

    switch (sid) {
        case DCM_SID_DIAGNOSTIC_SESSION_CONTROL:
            return handle_session_control(reqData, reqLen, respData, respBufSize);
        case DCM_SID_ECU_RESET:
            return handle_ecu_reset(reqData, reqLen, respData, respBufSize);
        case DCM_SID_CLEAR_DTC:
            return handle_clear_dtc(reqData, reqLen, respData, respBufSize);
        case DCM_SID_READ_DTC_INFO:
            return handle_read_dtc_info(reqData, reqLen, respData, respBufSize);
        case DCM_SID_READ_DATA_BY_ID:
            return handle_read_did(reqData, reqLen, respData, respBufSize);
        case DCM_SID_SECURITY_ACCESS:
            return handle_security_access(reqData, reqLen, respData, respBufSize);
        case DCM_SID_WRITE_DATA_BY_ID:
            return handle_write_did(reqData, reqLen, respData, respBufSize);
        case DCM_SID_ROUTINE_CONTROL:
            return handle_routine_control(reqData, reqLen, respData, respBufSize);
        case DCM_SID_TESTER_PRESENT:
            return handle_tester_present(reqData, reqLen, respData, respBufSize);
        default:
            return nrc_response(respData, sid, DCM_NRC_SERVICE_NOT_SUPPORTED);
    }
}

uint8 Dcm_GetActiveSession(void) {
    return g_session;
}

uint8 Dcm_GetSecurityLevel(void) {
    return g_security_level;
}
