/* Std_Types.h — AUTOSAR-compatible standard type definitions.
 *
 * Provides the basic types and return codes used by all BSW modules.
 * Based on AUTOSAR SWS_Std_Types (R4.x).
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef STD_TYPES_H
#define STD_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* AUTOSAR shorthand type aliases. */
typedef uint8_t   uint8;
typedef uint16_t  uint16;
typedef uint32_t  uint32;
typedef int8_t    sint8;
typedef int16_t   sint16;
typedef int32_t   sint32;

/* Standard return type for AUTOSAR APIs. */
typedef uint8_t Std_ReturnType;

#define E_OK       ((Std_ReturnType)0x00u)
#define E_NOT_OK   ((Std_ReturnType)0x01u)

/* Standard version info type. */
typedef struct {
    uint16_t vendorID;
    uint16_t moduleID;
    uint8_t  sw_major_version;
    uint8_t  sw_minor_version;
    uint8_t  sw_patch_version;
} Std_VersionInfoType;

/* Boolean compatibility. */
#ifndef TRUE
  #define TRUE  ((boolean)1u)
#endif
#ifndef FALSE
  #define FALSE ((boolean)0u)
#endif

typedef uint8_t boolean;

/* Standard ON/OFF type. */
#define STD_ON   1u
#define STD_OFF  0u

/* Null pointer definition. */
#ifndef NULL_PTR
  #define NULL_PTR ((void*)0)
#endif

#endif /* STD_TYPES_H */
