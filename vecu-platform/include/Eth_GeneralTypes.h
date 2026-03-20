/* Eth_GeneralTypes.h -- AUTOSAR Ethernet General Types for host compilation.
 *
 * Provides Eth_RxStatusType, Eth_FilterActionType and related types
 * used by Eth driver and EthIf modules.
 * Based on AUTOSAR SWS_EthGeneralTypes (R4.x / R20-11).
 *
 * Independently authored -- no vendor-derived content.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef ETH_GENERAL_TYPES_H
#define ETH_GENERAL_TYPES_H

#include "Std_Types.h"

typedef uint8  Eth_BufIdxType;
typedef uint16 Eth_FrameType;
typedef uint8  Eth_DataType;

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
    ETH_ADD_TO_FILTER      = 0x00u,
    ETH_REMOVE_FROM_FILTER = 0x01u
} Eth_FilterActionType;

typedef enum {
    ETH_STATE_UNINIT  = 0x00u,
    ETH_STATE_INIT    = 0x01u,
    ETH_STATE_ACTIVE  = 0x02u
} Eth_StateType;

typedef struct {
    uint8 numCtrl;
} Eth_ConfigType;

typedef uint16 Eth_RxBufferSizeType;
typedef uint16 Eth_TxBufferSizeType;

#endif /* ETH_GENERAL_TYPES_H */
