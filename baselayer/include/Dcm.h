/* Dcm.h — Diagnostic Communication Manager (ADR-005 / P6).
 *
 * UDS service dispatch with session management.  Processes diagnostic
 * requests and produces responses per ISO 14229-1.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef DCM_H
#define DCM_H

#include "Std_Types.h"

/* ── Constants ─────────────────────────────────────────────────────── */

/* UDS Service IDs */
#define DCM_SID_DIAGNOSTIC_SESSION_CONTROL  0x10u
#define DCM_SID_ECU_RESET                   0x11u
#define DCM_SID_CLEAR_DTC                   0x14u
#define DCM_SID_READ_DTC_INFO               0x19u
#define DCM_SID_READ_DATA_BY_ID             0x22u
#define DCM_SID_SECURITY_ACCESS             0x27u
#define DCM_SID_WRITE_DATA_BY_ID            0x2Eu
#define DCM_SID_ROUTINE_CONTROL             0x31u
#define DCM_SID_TESTER_PRESENT              0x3Eu

/* UDS Negative Response Codes */
#define DCM_NRC_GENERAL_REJECT              0x10u
#define DCM_NRC_SERVICE_NOT_SUPPORTED       0x11u
#define DCM_NRC_SUBFUNCTION_NOT_SUPPORTED   0x12u
#define DCM_NRC_INCORRECT_LENGTH            0x13u
#define DCM_NRC_CONDITIONS_NOT_CORRECT      0x22u
#define DCM_NRC_SECURITY_ACCESS_DENIED      0x33u
#define DCM_NRC_INVALID_KEY                 0x35u
#define DCM_NRC_EXCEEDED_ATTEMPTS           0x36u
#define DCM_NRC_REQUEST_OUT_OF_RANGE        0x31u
#define DCM_NRC_RESPONSE_PENDING            0x78u

/* Session types */
#define DCM_SESSION_DEFAULT                 0x01u
#define DCM_SESSION_EXTENDED                0x03u
#define DCM_SESSION_PROGRAMMING             0x02u

/* Security levels */
#define DCM_SECURITY_LOCKED                 0x00u
#define DCM_SECURITY_LEVEL1                 0x01u

/* Maximum request/response buffer */
#define DCM_MAX_BUFFER                      256u

/* ── DID table ─────────────────────────────────────────────────────── */

#define DCM_MAX_DIDS                        32u

typedef Std_ReturnType (*Dcm_ReadDidFn)(uint8* data, uint16* length);
typedef Std_ReturnType (*Dcm_WriteDidFn)(const uint8* data, uint16 length);

typedef struct {
    uint16 did;
    uint16 length;          /* fixed length in bytes */
    Dcm_ReadDidFn readFn;   /* NULL if not readable */
    Dcm_WriteDidFn writeFn; /* NULL if not writable */
} Dcm_DidEntryType;

/* ── Routine table ─────────────────────────────────────────────────── */

#define DCM_MAX_ROUTINES                    16u

typedef Std_ReturnType (*Dcm_RoutineStartFn)(const uint8* optionRecord, uint16 optLen,
                                              uint8* statusRecord, uint16* statusLen);

typedef struct {
    uint16 routineId;
    uint16 _pad;
    Dcm_RoutineStartFn startFn;
} Dcm_RoutineEntryType;

/* ── Configuration ─────────────────────────────────────────────────── */

typedef struct {
    const Dcm_DidEntryType*     dids;
    uint16                      numDids;
    const Dcm_RoutineEntryType* routines;
    uint16                      numRoutines;
    uint16                      sessionTimeoutMs;  /* 0 = no timeout */
    uint16                      _pad;
} Dcm_ConfigType;

/* ── Lifecycle ─────────────────────────────────────────────────────── */

void Dcm_Init(const Dcm_ConfigType* config);
void Dcm_DeInit(void);
void Dcm_MainFunction(void);

/* ── Processing API ────────────────────────────────────────────────── */

/** Process a UDS request and produce a response.
 *  Returns the response length (0 = suppress response). */
uint16 Dcm_ProcessRequest(const uint8* reqData, uint16 reqLen,
                          uint8* respData, uint16 respBufSize);

/* ── Query API ─────────────────────────────────────────────────────── */

uint8 Dcm_GetActiveSession(void);
uint8 Dcm_GetSecurityLevel(void);

#endif /* DCM_H */
