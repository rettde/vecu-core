/* Std_Types.h -- AUTOSAR Standard Types for host compilation.
 *
 * Provides Std_ReturnType, Std_VersionInfoType and basic type aliases
 * used by all BSW modules. Includes Platform_Types.h and Compiler.h
 * as required by AUTOSAR SWS_Std_Types (R4.x / R20-11).
 *
 * Independently authored -- no vendor-derived content.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef STD_TYPES_H
#define STD_TYPES_H

#include "Platform_Types.h"
#include "Compiler.h"

#define STD_TYPES_VENDOR_ID    99u
#define STD_TYPES_MODULE_ID    197u

#define STD_TYPES_AR_RELEASE_MAJOR_VERSION      (4u)
#define STD_TYPES_AR_RELEASE_MINOR_VERSION      (8u)
#define STD_TYPES_AR_RELEASE_REVISION_VERSION   (0u)

#define STD_TYPES_SW_MAJOR_VERSION       (2u)
#define STD_TYPES_SW_MINOR_VERSION       (0u)
#define STD_TYPES_SW_PATCH_VERSION       (0u)

#ifndef STATUSTYPEDEFINED
# define STATUSTYPEDEFINED
typedef unsigned char StatusType;
# define E_OK  0u
#endif

typedef uint8 Std_ReturnType;

#define E_NOT_OK  1u

typedef struct {
    uint16 vendorID;
    uint16 moduleID;
    uint8  sw_major_version;
    uint8  sw_minor_version;
    uint8  sw_patch_version;
} Std_VersionInfoType;

typedef uint8 Std_TransformerErrorCode;
typedef uint8 Std_TransformerForwardCode;
typedef uint8 Std_TransformerClass;
typedef uint8 Std_MessageTypeType;
typedef uint8 Std_MessageResultType;

#define STD_TRANSFORMER_UNMODIFIED   ((Std_TransformerForwardCode)0x00u)
#define STD_TRANSFORMER_ERROR        ((Std_TransformerErrorCode)0x01u)

#define STD_MESSAGETYPE_REQUEST      ((Std_MessageTypeType)0x00u)
#define STD_MESSAGETYPE_RESPONSE     ((Std_MessageTypeType)0x01u)

#define STD_MESSAGERESULT_OK         ((Std_MessageResultType)0x00u)
#define STD_MESSAGERESULT_ERROR      ((Std_MessageResultType)0x01u)

#define STD_ON   1u
#define STD_OFF  0u

#define STD_HIGH 1u
#define STD_LOW  0u

#define STD_ACTIVE  1u
#define STD_IDLE    0u

#endif /* STD_TYPES_H */
