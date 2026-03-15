/* Dem.h — Diagnostic Event Manager (ADR-005 / P6).
 *
 * DTC storage with ISO 14229 status bits.  Backed by NvM for
 * persistent storage of DTC snapshots.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef DEM_H
#define DEM_H

#include "Std_Types.h"

/* ── Constants ─────────────────────────────────────────────────────── */

#define DEM_MAX_DTCS            64u
#define DEM_SNAPSHOT_SIZE       8u   /* bytes per snapshot record */

/* ISO 14229 DTC status bits */
#define DEM_STATUS_TEST_FAILED              0x01u
#define DEM_STATUS_TEST_FAILED_THIS_CYCLE   0x02u
#define DEM_STATUS_PENDING                  0x04u
#define DEM_STATUS_CONFIRMED                0x08u
#define DEM_STATUS_NOT_COMPLETED_SINCE_CLEAR 0x10u
#define DEM_STATUS_TEST_NOT_COMPLETED       0x20u
#define DEM_STATUS_WARNING_INDICATOR        0x40u

/* ── Types ─────────────────────────────────────────────────────────── */

typedef struct {
    uint32 dtcNumber;       /* 3-byte DTC (e.g. 0xC07300) */
    uint8  statusMask;      /* ISO 14229 status byte */
    uint8  snapshot[DEM_SNAPSHOT_SIZE];
    uint8  _pad[3];
} Dem_DtcEntryType;

/* ── Lifecycle ─────────────────────────────────────────────────────── */

void Dem_Init(void);
void Dem_DeInit(void);
void Dem_MainFunction(void);

/* ── Reporting API ─────────────────────────────────────────────────── */

/** Report a DTC event (test failed). */
Std_ReturnType Dem_ReportErrorStatus(uint32 dtcNumber, uint8 status);

/** Report a DTC event with snapshot data. */
Std_ReturnType Dem_ReportErrorStatusWithSnapshot(uint32 dtcNumber, uint8 status,
                                                  const uint8* snapshot, uint8 snapLen);

/* ── Query API ─────────────────────────────────────────────────────── */

/** Get status byte for a specific DTC. */
Std_ReturnType Dem_GetDTCStatus(uint32 dtcNumber, uint8* statusPtr);

/** Get number of DTCs matching a status mask. */
uint16 Dem_GetNumberOfDTCByStatusMask(uint8 statusMask);

/** Get DTC number by index (filtered by status mask). */
Std_ReturnType Dem_GetDTCByOccurrenceIndex(uint16 index, uint32* dtcNumberPtr);

/** Get snapshot data for a DTC. */
Std_ReturnType Dem_GetDTCSnapshot(uint32 dtcNumber, uint8* bufPtr, uint8* lenPtr);

/** Clear all DTCs (or a specific group). 0xFFFFFF = all. */
Std_ReturnType Dem_ClearDTC(uint32 dtcGroup);

#endif /* DEM_H */
