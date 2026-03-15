/* Dem.c — Diagnostic Event Manager implementation (ADR-005 / P6).
 *
 * DTC storage with ISO 14229 status bits.  Fixed-size DTC table
 * with snapshot support.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Dem.h"
#include <string.h>
#include <stddef.h>

static boolean g_initialized = FALSE;
static Dem_DtcEntryType g_dtcs[DEM_MAX_DTCS];
static uint16 g_numDtcs = 0;

/* ── Helpers ────────────────────────────────────────────────────── */

static Dem_DtcEntryType* find_dtc(uint32 dtcNumber) {
    for (uint16 i = 0; i < g_numDtcs; i++) {
        if (g_dtcs[i].dtcNumber == dtcNumber) {
            return &g_dtcs[i];
        }
    }
    return NULL;
}

static Dem_DtcEntryType* alloc_dtc(uint32 dtcNumber) {
    if (g_numDtcs >= DEM_MAX_DTCS) { return NULL; }
    Dem_DtcEntryType* e = &g_dtcs[g_numDtcs++];
    memset(e, 0, sizeof(*e));
    e->dtcNumber = dtcNumber;
    return e;
}

/* ── Lifecycle ──────────────────────────────────────────────────── */

void Dem_Init(void) {
    memset(g_dtcs, 0, sizeof(g_dtcs));
    g_numDtcs = 0;
    g_initialized = TRUE;
}

void Dem_DeInit(void) {
    g_initialized = FALSE;
}

void Dem_MainFunction(void) {
    /* Aging / debouncing would go here. */
    (void)0;
}

/* ── Reporting ──────────────────────────────────────────────────── */

Std_ReturnType Dem_ReportErrorStatus(uint32 dtcNumber, uint8 status) {
    if (!g_initialized) { return E_NOT_OK; }

    Dem_DtcEntryType* e = find_dtc(dtcNumber);
    if (e == NULL) { e = alloc_dtc(dtcNumber); }
    if (e == NULL) { return E_NOT_OK; }

    if (status != 0u) {
        e->statusMask |= DEM_STATUS_TEST_FAILED |
                         DEM_STATUS_TEST_FAILED_THIS_CYCLE |
                         DEM_STATUS_PENDING |
                         DEM_STATUS_CONFIRMED;
    } else {
        e->statusMask &= (uint8)~(DEM_STATUS_TEST_FAILED |
                                    DEM_STATUS_TEST_FAILED_THIS_CYCLE);
    }
    return E_OK;
}

Std_ReturnType Dem_ReportErrorStatusWithSnapshot(uint32 dtcNumber, uint8 status,
                                                  const uint8* snapshot, uint8 snapLen)
{
    Std_ReturnType rc = Dem_ReportErrorStatus(dtcNumber, status);
    if (rc != E_OK) { return rc; }

    Dem_DtcEntryType* e = find_dtc(dtcNumber);
    if (e == NULL) { return E_NOT_OK; }

    uint8 copyLen = (snapLen > DEM_SNAPSHOT_SIZE) ? DEM_SNAPSHOT_SIZE : snapLen;
    memcpy(e->snapshot, snapshot, copyLen);
    return E_OK;
}

/* ── Query ──────────────────────────────────────────────────────── */

Std_ReturnType Dem_GetDTCStatus(uint32 dtcNumber, uint8* statusPtr) {
    if (!g_initialized || statusPtr == NULL) { return E_NOT_OK; }
    const Dem_DtcEntryType* e = find_dtc(dtcNumber);
    if (e == NULL) { *statusPtr = 0; return E_OK; }
    *statusPtr = e->statusMask;
    return E_OK;
}

uint16 Dem_GetNumberOfDTCByStatusMask(uint8 statusMask) {
    if (!g_initialized) { return 0; }
    uint16 count = 0;
    for (uint16 i = 0; i < g_numDtcs; i++) {
        if ((g_dtcs[i].statusMask & statusMask) != 0u) {
            count++;
        }
    }
    return count;
}

Std_ReturnType Dem_GetDTCByOccurrenceIndex(uint16 index, uint32* dtcNumberPtr) {
    if (!g_initialized || dtcNumberPtr == NULL) { return E_NOT_OK; }
    if (index >= g_numDtcs) { return E_NOT_OK; }
    *dtcNumberPtr = g_dtcs[index].dtcNumber;
    return E_OK;
}

Std_ReturnType Dem_GetDTCSnapshot(uint32 dtcNumber, uint8* bufPtr, uint8* lenPtr) {
    if (!g_initialized || bufPtr == NULL || lenPtr == NULL) { return E_NOT_OK; }
    const Dem_DtcEntryType* e = find_dtc(dtcNumber);
    if (e == NULL) { *lenPtr = 0; return E_NOT_OK; }
    uint8 copyLen = (*lenPtr > DEM_SNAPSHOT_SIZE) ? DEM_SNAPSHOT_SIZE : *lenPtr;
    memcpy(bufPtr, e->snapshot, copyLen);
    *lenPtr = copyLen;
    return E_OK;
}

Std_ReturnType Dem_ClearDTC(uint32 dtcGroup) {
    if (!g_initialized) { return E_NOT_OK; }
    if (dtcGroup == 0xFFFFFFu) {
        /* Clear all */
        memset(g_dtcs, 0, sizeof(g_dtcs));
        g_numDtcs = 0;
    } else {
        /* Clear specific DTC */
        for (uint16 i = 0; i < g_numDtcs; i++) {
            if (g_dtcs[i].dtcNumber == dtcGroup) {
                /* Shift remaining entries */
                for (uint16 j = i; j + 1u < g_numDtcs; j++) {
                    g_dtcs[j] = g_dtcs[j + 1u];
                }
                g_numDtcs--;
                memset(&g_dtcs[g_numDtcs], 0, sizeof(Dem_DtcEntryType));
                break;
            }
        }
    }
    return E_OK;
}
