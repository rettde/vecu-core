/* Platform_Types.h -- AUTOSAR Platform Types for host compilation.
 *
 * Replaces target-specific Platform_Types.h (RH850/TriCore/S32K).
 * Uses <stdint.h> for portable, width-correct types on any host
 * (LP64 macOS/Linux, LLP64 Windows).
 *
 * Based on AUTOSAR SWS_PlatformTypes (R4.x / R20-11).
 * Independently authored -- no vendor-derived content.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef PLATFORM_TYPES_H
#define PLATFORM_TYPES_H

#include <stdint.h>

#define PLATFORM_VENDOR_ID    99u
#define PLATFORM_MODULE_ID    199u

#define PLATFORM_AR_RELEASE_MAJOR_VERSION      (4u)
#define PLATFORM_AR_RELEASE_MINOR_VERSION      (8u)
#define PLATFORM_AR_RELEASE_REVISION_VERSION   (0u)

#define PLATFORM_SW_MAJOR_VERSION       (2u)
#define PLATFORM_SW_MINOR_VERSION       (0u)
#define PLATFORM_SW_PATCH_VERSION       (0u)

#define CPU_TYPE_8       8u
#define CPU_TYPE_16      16u
#define CPU_TYPE_32      32u
#define CPU_TYPE_64      64u

#define MSB_FIRST        0u
#define LSB_FIRST        1u

#define HIGH_BYTE_FIRST  0u
#define LOW_BYTE_FIRST   1u

#ifndef TRUE
# define TRUE            1u
#endif

#ifndef FALSE
# define FALSE           0u
#endif

/* Force CPU_TYPE_32 for vECU builds to match target ECU ABI.
 * MICROSAR BSW uses CPU_TYPE in struct layout decisions --
 * CPU_TYPE_64 on a 64-bit host would change padding/alignment. */
#define CPU_TYPE        CPU_TYPE_32

#define CPU_BIT_ORDER    LSB_FIRST
#define CPU_BYTE_ORDER   LOW_BYTE_FIRST

#define UINT8_MIN        0u
#ifndef UINT8_MAX
# define UINT8_MAX       255u
#endif
#define UINT16_MIN       0u
#ifndef UINT16_MAX
# define UINT16_MAX      65535u
#endif
#define UINT32_MIN       0ul
#ifndef UINT32_MAX
# define UINT32_MAX      4294967295ul
#endif
#define UINT64_MIN       0ull
#ifndef UINT64_MAX
# define UINT64_MAX      18446744073709551615ull
#endif

#define SINT8_MIN        (-128)
#define SINT8_MAX        127
#define SINT16_MIN       (-32768)
#define SINT16_MAX       32767
#define SINT32_MIN       (-2147483647 - 1)
#define SINT32_MAX       2147483647
#define SINT64_MIN       (-9223372036854775807LL - 1)
#define SINT64_MAX       9223372036854775807LL

#define FLOAT32_MIN      1.17549435e-38f
#define FLOAT32_MAX      3.40282347e+38f
#define FLOAT32_EPSILON  1.19209290e-07f

#define FLOAT64_MIN      2.2250738585072014e-308
#define FLOAT64_MAX      1.7976931348623157e+308
#define FLOAT64_EPSILON  2.2204460492503131e-16

typedef unsigned char       boolean;

typedef int8_t              sint8;
typedef uint8_t             uint8;
typedef int16_t             sint16;
typedef uint16_t            uint16;
typedef int32_t             sint32;
typedef uint32_t            uint32;

typedef int                 sint8_least;
typedef unsigned int        uint8_least;
typedef int                 sint16_least;
typedef unsigned int        uint16_least;
typedef int                 sint32_least;
typedef unsigned int        uint32_least;

#define PLATFORM_SUPPORT_SINT64_UINT64
typedef int64_t             sint64;
typedef uint64_t            uint64;

typedef float               float32;
typedef double              float64;

typedef void*               VoidPtr;
typedef const void*         ConstVoidPtr;

#endif /* PLATFORM_TYPES_H */
