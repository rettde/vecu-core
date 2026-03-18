/* Eth_GeneralTypes.h — AUTOSAR Ethernet General Types.
 *
 * Provides Eth_RxStatusType, Eth_FilterActionType and related types
 * used by Eth driver and EthIf modules.
 * Based on AUTOSAR SWS_EthGeneralTypes (R4.x / R20-11).
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef ETH_GENERAL_TYPES_H
#define ETH_GENERAL_TYPES_H

#include "Std_Types.h"

typedef uint8  Eth_BufIdxType;
typedef uint16 Eth_FrameType;

typedef enum {
    ETH_MODE_DOWN   = 0x00u,
    ETH_MODE_ACTIVE = 0x01u
} Eth_ModeType;

typedef enum {
    ETH_RECEIVED     = 0x00u,
    ETH_NOT_RECEIVED = 0x01u,
    ETH_RECEIVED_MORE_DATA_AVAILABLE = 0x02u
} Eth_RxStatusType;

typedef enum {
    ETH_ADD_TO_FILTER     = 0x00u,
    ETH_REMOVE_FROM_FILTER = 0x01u
} Eth_FilterActionType;

typedef struct {
    uint8 numCtrl;
} Eth_ConfigType;

#endif /* ETH_GENERAL_TYPES_H */
