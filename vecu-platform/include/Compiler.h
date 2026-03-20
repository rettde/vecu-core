/* Compiler.h -- AUTOSAR Compiler Abstraction for host compilation.
 *
 * Replaces target-specific Compiler.h (GreenHills/TASKING/IAR/MSVC-target).
 * Works with GCC, Clang (incl. Apple Clang), and MSVC on host.
 * All memory-class qualifiers expand to nothing on the host.
 *
 * Based on AUTOSAR SWS_CompilerAbstraction (R4.x / R20-11).
 * Independently authored -- no vendor-derived content.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef COMPILER_H
#define COMPILER_H

#include "Compiler_Cfg.h"

#define COMPILER_VENDOR_ID    99u
#define COMPILER_MODULE_ID    198u

#define COMPILER_AR_RELEASE_MAJOR_VERSION       (4u)
#define COMPILER_AR_RELEASE_MINOR_VERSION       (7u)
#define COMPILER_AR_RELEASE_REVISION_VERSION    (0u)

#define COMPILER_SW_MAJOR_VERSION       (2u)
#define COMPILER_SW_MINOR_VERSION       (0u)
#define COMPILER_SW_PATCH_VERSION       (0u)

/* MICROSAR SIP headers check for _GREENHILLS_C_RH850_ to select
 * include paths (e.g. Compiler_Cfg_MSR4_MPP_RH850_U2B_GreenHills.h).
 * Defining this macro lets the SIP's _Compiler_Cfg.h resolve without
 * errors. The actual compiler abstraction macros below override
 * any target-specific definitions. */
#define _GREENHILLS_C_RH850_

#define AUTOMATIC
#define TYPEDEF

#ifndef NULL_PTR
  #define NULL_PTR  ((void *)0)
#endif

#ifndef INLINE
  #define INLINE  static inline
#endif

#ifndef LOCAL_INLINE
  #define LOCAL_INLINE  static inline
#endif

#define FUNC(rettype, memclass)                         rettype
#define FUNC_P2CONST(rettype, ptrclass, memclass)       const rettype*
#define FUNC_P2VAR(rettype, ptrclass, memclass)         rettype*

#define P2VAR(ptrtype, memclass, ptrclass)              ptrtype*
#define P2CONST(ptrtype, memclass, ptrclass)            const ptrtype*
#define CONSTP2VAR(ptrtype, memclass, ptrclass)         ptrtype *const
#define CONSTP2CONST(ptrtype, memclass, ptrclass)       const ptrtype *const

#define P2FUNC(rettype, ptrclass, fctname)              rettype (* fctname)
#define CONSTP2FUNC(rettype, ptrclass, fctname)         rettype (*const fctname)

#define CONST(type, memclass)                           const type
#define VAR(vartype, memclass)                          vartype

#endif /* COMPILER_H */
