/* vecu_prefix.h -- Force-included before every translation unit.
 *
 * Ensures host-compatible Platform_Types.h and Compiler.h are loaded
 * before any AUTOSAR BSW header can pull in target-specific versions.
 * Also provides common fixups for MICROSAR BSW compilation on host.
 *
 * Usage: add -include vecu_prefix.h to compiler flags, or
 *        #include "vecu_prefix.h" as the first include in every .c file.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */
#ifndef VECU_PREFIX_H
#define VECU_PREFIX_H

#define VECU_BUILD 1

/* Prevent MICROSAR Det from entering an endless loop on error.
 * In vECU context we log errors and continue execution. */
#define DET_REGRESSION_TEST

#include "Platform_Types.h"
#include "Compiler.h"
#include "Std_Types.h"
#include "ComStack_Types.h"
#include "CanTrcv_GeneralTypes.h"

/* Target intrinsics that don't exist on host */
#ifndef nop
# define nop()  ((void)0)
#endif
#ifndef __nop
# define __nop()  ((void)0)
#endif

/* CanSM partition types and macros (MICROSAR >= 34.x) */
#ifndef CANSM_LOCAL_INLINE
# define CANSM_LOCAL_INLINE  LOCAL_INLINE
#endif
#ifndef CANSM_LOCAL
# define CANSM_LOCAL  static
#endif
#if !defined(CANSM_NO_PARTITION_IDX)
typedef uint8 CanSM_PartitionIdxType;
# define CANSM_NO_PARTITION_IDX                          0u
# define CANSM_FINISHED_PARTITIONINITIALIZATIONSTATE     2u
# define CANSM_NOT_CALLED_PARTITIONINITIALIZATIONSTATE   0u
# define CANSM_PREVENT_BUS_SLEEP_MODE                    0u
# define CANSM_PRE_INIT_FINISHED                         1u
# define CANSM_PRE_INIT_NOT_CALLED                       0u
# define CANSM_PRE_INIT_PARTITIONINITIALIZATIONSTATE     1u
# define CANSM_RUNNING_PARTITIONINITIALIZATIONSTATE      3u
#endif

/* Can_ReturnType (deprecated AUTOSAR 4.0 compatibility) */
#ifndef Can_ReturnType
typedef Std_ReturnType Can_ReturnType;
# define CAN_OK      E_OK
# define CAN_NOT_OK  E_NOT_OK
# define CAN_BUSY    ((Std_ReturnType)2u)
#endif

/* AppModeType (OSEK/AUTOSAR OS application mode) */
#if !defined(APPMODETYP)
typedef uint32 AppModeType;
#endif

/* Can_Config_Ptr -- MICROSAR CanIf references this; vmcal doesn't need it */
#ifndef Can_Config_Ptr
# define Can_Config_Ptr  NULL_PTR
#endif

/* RTE return codes (in case Rte_StdTypes.h is not available) */
#ifndef RTE_E_OK
# define RTE_E_OK          ((Std_ReturnType)0u)
# define RTE_E_COM_BUSY    ((Std_ReturnType)141u)
#endif

#endif /* VECU_PREFIX_H */
