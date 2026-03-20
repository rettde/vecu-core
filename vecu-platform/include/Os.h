/* Os.h -- AUTOSAR OS / OSEK user interface for host (vECU) compilation.
 *
 * Provides AUTOSAR OS types, macros, and API stubs needed by SchM_*.h
 * and other BSW modules. Replaces VttOs / MICROSAR OS for host builds.
 *
 * Based on ISO 17356 (OSEK/VDX) and AUTOSAR SWS_OS (R4.x / R20-11).
 * Independently authored -- no vendor-derived content.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */
#ifndef OS_H
#define OS_H

#include "Std_Types.h"

/* ---- Compiler attributes -------------------------------------------- */
#ifndef OS_ALWAYS_INLINE
# define OS_ALWAYS_INLINE
#endif
#ifndef OS_LOCAL_INLINE
# define OS_LOCAL_INLINE  LOCAL_INLINE
#endif
#ifndef OS_PURE
# define OS_PURE
#endif
#ifndef OS_NORETURN
# define OS_NORETURN
#endif
#ifndef OS_LIKELY
# define OS_LIKELY(CONDITION)    (CONDITION)
#endif
#ifndef OS_UNLIKELY
# define OS_UNLIKELY(CONDITION)  (CONDITION)
#endif

/* ---- Memory classes ------------------------------------------------- */
#ifndef OS_CODE
# define OS_CODE
#endif
#ifndef OS_CONST
# define OS_CONST
#endif
#ifndef OS_VAR_NOINIT
# define OS_VAR_NOINIT
#endif
#ifndef OS_VAR_NOINIT_FAST
# define OS_VAR_NOINIT_FAST
#endif
#ifndef OS_VAR_ZERO_INIT
# define OS_VAR_ZERO_INIT
#endif
#ifndef OS_APPL_DATA
# define OS_APPL_DATA
#endif
#ifndef OS_APPL_VAR
# define OS_APPL_VAR
#endif
#ifndef OS_APPL_CODE
# define OS_APPL_CODE
#endif
#ifndef OS_VAR_INIT
# define OS_VAR_INIT
#endif

/* ---- OSEK TASK / ISR body macros ------------------------------------ */
#ifndef TASK
# define TASK(TaskName)  void Os_Task_##TaskName(void)
#endif
#ifndef ISR
# define ISR(IsrName)    void Os_Isr_##IsrName(void)
#endif

/* ---- Function attribute declaration/definition macros --------------- */
#ifndef OS_FUNC_ATTRIBUTE_DECLARATION
# define OS_FUNC_ATTRIBUTE_DECLARATION(rettype, memclass, attribute, functionName, arguments) \
    FUNC(rettype, memclass) functionName arguments
#endif

#ifndef OS_FUNC_ATTRIBUTE_DEFINITION
# define OS_FUNC_ATTRIBUTE_DEFINITION(rettype, memclass, attribute, functionName, arguments) \
    FUNC(rettype, memclass) functionName arguments
#endif

/* ---- AUTOSAR OS basic types ----------------------------------------- */
#ifndef STATUSTYPEDEFINED
# define STATUSTYPEDEFINED
typedef unsigned char StatusType;
# define E_OK  ((StatusType)0x00)
#endif

typedef uint32 TaskStateType;
typedef TaskStateType *TaskStateRefType;

typedef uint64 EventMaskType;

typedef uint32 TickType;
typedef TickType *TickRefType;

typedef struct {
    TickType maxallowedvalue;
    TickType ticksperbase;
    TickType mincycle;
} AlarmBaseType;
typedef AlarmBaseType *AlarmBaseRefType;

typedef uint8 ScheduleTableStatusType;
typedef ScheduleTableStatusType *ScheduleTableStatusRefType;

typedef uint8 ApplicationStateType;
typedef ApplicationStateType *ApplicationStateRefType;

typedef uint8 ObjectTypeType;
typedef uint32 ObjectAccessType;
typedef uint32 AccessType;
typedef uint8 RestartType;

typedef uint32 PhysicalTimeType;
typedef void *TrustedFunctionParameterRefType;
typedef void *NonTrustedFunctionParameterRefType;

typedef uint8 TryToGetSpinlockType;
typedef void *Os_NonTrustedFunctionParameterRefType;

typedef uint32 IdleModeType;
#define IDLE_NO_HALT  ((IdleModeType)0)

typedef uint32 CoreIdType;

typedef uint32 AppModeType;

/* ---- Project-specific OS object IDs from GenData -------------------- */
/* Os_Types_Lcfg.h defines TaskType, ResourceType, AlarmType, etc.
 * from GenData. If not present, provide safe defaults. */
#if __has_include("Os_Types_Lcfg.h")
# include "Os_Types_Lcfg.h"
#else
typedef uint32 TaskType;
typedef uint32 ResourceType;
typedef uint32 AlarmType;
typedef uint32 CounterType;
typedef uint32 ScheduleTableType;
typedef uint32 SpinlockIdType;
typedef uint32 ApplicationType;
typedef uint32 TrustedFunctionIndexType;
typedef uint32 NonTrustedFunctionIndexType;
typedef uint32 Os_NonTrustedFunctionIndexType;
# define INVALID_TASK  ((TaskType)0xFFFFFFFFul)
#endif

typedef TaskType *TaskRefType;
typedef EventMaskType *EventMaskRefType;

#ifndef Os_NonTrustedFunctionIndexType
typedef Os_NonTrustedFunctionIndexType NonTrustedFunctionIndexType;
#endif

/* ---- OS Error codes ------------------------------------------------- */
#define E_OS_ACCESS                    ((StatusType)0x01)
#define E_OS_CALLEVEL                  ((StatusType)0x02)
#define E_OS_ID                        ((StatusType)0x03)
#define E_OS_LIMIT                     ((StatusType)0x04)
#define E_OS_NOFUNC                    ((StatusType)0x05)
#define E_OS_RESOURCE                  ((StatusType)0x06)
#define E_OS_STATE                     ((StatusType)0x07)
#define E_OS_VALUE                     ((StatusType)0x08)
#define E_OS_SERVICEID                 ((StatusType)0x09)
#define E_OS_ILLEGAL_ADDRESS           ((StatusType)0x0A)
#define E_OS_MISSINGEND                ((StatusType)0x0B)
#define E_OS_DISABLEDINT               ((StatusType)0x0C)
#define E_OS_STACKFAULT                ((StatusType)0x0D)
#define E_OS_PROTECTION_MEMORY         ((StatusType)0x0E)
#define E_OS_PROTECTION_TIME           ((StatusType)0x0F)
#define E_OS_PROTECTION_ARRIVAL        ((StatusType)0x10)
#define E_OS_PROTECTION_LOCKED         ((StatusType)0x11)
#define E_OS_PROTECTION_EXCEPTION      ((StatusType)0x12)
#define E_OS_INTERFERENCE_DEADLOCK     ((StatusType)0x13)
#define E_OS_NESTING_DEADLOCK          ((StatusType)0x14)
#define E_OS_SPINLOCK                  ((StatusType)0x15)
#define E_OS_CORE                      ((StatusType)0x16)
#define E_OS_PARAM_POINTER             ((StatusType)0x17)
#define E_OS_SYS_DISABLED              ((StatusType)0xF1)

/* ---- Task states ---------------------------------------------------- */
#define RUNNING    ((TaskStateType)0)
#define WAITING    ((TaskStateType)1)
#define READY      ((TaskStateType)2)
#define SUSPENDED  ((TaskStateType)3)

/* ---- Application states --------------------------------------------- */
#define APPLICATION_ACCESSIBLE         ((ApplicationStateType)0)
#define APPLICATION_RESTARTING         ((ApplicationStateType)1)
#define APPLICATION_TERMINATED         ((ApplicationStateType)2)

/* ---- Schedule table states ------------------------------------------ */
#define SCHEDULETABLE_STOPPED                  ((ScheduleTableStatusType)0x01)
#define SCHEDULETABLE_NEXT                     ((ScheduleTableStatusType)0x02)
#define SCHEDULETABLE_WAITING                  ((ScheduleTableStatusType)0x04)
#define SCHEDULETABLE_RUNNING                  ((ScheduleTableStatusType)0x08)
#define SCHEDULETABLE_RUNNING_AND_SYNCHRONOUS  ((ScheduleTableStatusType)0x10)

/* ---- Spinlock ------------------------------------------------------- */
#define TRYTOGETSPINLOCK_SUCCESS   ((TryToGetSpinlockType)0)
#define TRYTOGETSPINLOCK_NOSUCCESS ((TryToGetSpinlockType)1)

/* ---- GenData OS configuration --------------------------------------- */
/* Os_Cfg.h provides Rte_Ev_*, task/event/alarm IDs from GenData.
 * If not present, provide empty defaults. */
#if __has_include("Os_Cfg.h")
# include "Os_Cfg.h"
#else
# ifndef OS_CFG_H
#  define OS_CFG_H
# endif
#endif

/* ---- OS core defines ------------------------------------------------ */
#ifndef OS_CORE_ID_MASTER
# define OS_CORE_ID_MASTER  0
#endif
#ifndef OS_CORE_ID_0
# define OS_CORE_ID_0  0
# define OS_CORE_ID_1  1
# define OS_CORE_ID_2  2
# define OS_CORE_ID_3  3
# define OS_CORE_ID_4  4
#endif
#ifndef OS_NUMBER_OF_CORES
# define OS_NUMBER_OF_CORES 5
#endif

/* ---- Minimal OS API stubs (single-threaded vECU) -------------------- */
static inline StatusType ActivateTask(TaskType t) { (void)t; return E_OK; }
static inline StatusType TerminateTask(void) { return E_OK; }
static inline StatusType ChainTask(TaskType t) { (void)t; return E_OK; }
static inline StatusType Schedule(void) { return E_OK; }
static inline StatusType GetTaskID(TaskRefType r) { if(r) *r = INVALID_TASK; return E_OK; }
static inline StatusType GetTaskState(TaskType t, TaskStateRefType s) { (void)t; if(s) *s = SUSPENDED; return E_OK; }

static inline void EnableAllInterrupts(void) {}
static inline void DisableAllInterrupts(void) {}
static inline void ResumeAllInterrupts(void) {}
static inline void SuspendAllInterrupts(void) {}
static inline void ResumeOSInterrupts(void) {}
static inline void SuspendOSInterrupts(void) {}

static inline StatusType SetEvent(TaskType t, EventMaskType m) { (void)t; (void)m; return E_OK; }
static inline StatusType ClearEvent(EventMaskType m) { (void)m; return E_OK; }
static inline StatusType GetEvent(TaskType t, EventMaskRefType r) { (void)t; if(r) *r = 0; return E_OK; }
static inline StatusType WaitEvent(EventMaskType m) { (void)m; return E_OK; }

static inline StatusType GetResource(ResourceType r) { (void)r; return E_OK; }
static inline StatusType ReleaseResource(ResourceType r) { (void)r; return E_OK; }

static inline StatusType SetRelAlarm(AlarmType a, TickType inc, TickType cyc) { (void)a; (void)inc; (void)cyc; return E_OK; }
static inline StatusType SetAbsAlarm(AlarmType a, TickType s, TickType cyc) { (void)a; (void)s; (void)cyc; return E_OK; }
static inline StatusType CancelAlarm(AlarmType a) { (void)a; return E_OK; }
static inline StatusType GetAlarm(AlarmType a, TickRefType t) { (void)a; if(t) *t = 0; return E_OK; }
static inline StatusType GetAlarmBase(AlarmType a, AlarmBaseRefType b) { (void)a; (void)b; return E_OK; }

static inline StatusType IncrementCounter(CounterType c) { (void)c; return E_OK; }
static inline StatusType GetCounterValue(CounterType c, TickRefType v) { (void)c; if(v) *v = 0; return E_OK; }
static inline StatusType GetElapsedValue(CounterType c, TickRefType v, TickRefType e) { (void)c; (void)v; (void)e; return E_OK; }

static inline ApplicationType GetApplicationID(void) { return 0; }
static inline ApplicationType GetCurrentApplicationID(void) { return 0; }
static inline CoreIdType GetCoreID(void) { return 0; }
static inline void StartCore(CoreIdType c, StatusType *s) { (void)c; if(s) *s = E_OK; }
static inline void StartNonAutosarCore(CoreIdType c, StatusType *s) { (void)c; if(s) *s = E_OK; }

static inline StatusType GetSpinlock(SpinlockIdType s) { (void)s; return E_OK; }
static inline StatusType ReleaseSpinlock(SpinlockIdType s) { (void)s; return E_OK; }
static inline StatusType TryToGetSpinlock(SpinlockIdType s, TryToGetSpinlockType *r) { (void)s; if(r) *r = TRYTOGETSPINLOCK_SUCCESS; return E_OK; }

static inline StatusType CallTrustedFunction(TrustedFunctionIndexType f, TrustedFunctionParameterRefType p) { (void)f; (void)p; return E_OK; }
static inline StatusType CallNonTrustedFunction(NonTrustedFunctionIndexType f, NonTrustedFunctionParameterRefType p) { (void)f; (void)p; return E_OK; }

static inline ObjectAccessType CheckObjectAccess(ApplicationType a, ObjectTypeType ot, ...) { (void)a; (void)ot; return (ObjectAccessType)1; }
static inline ApplicationType CheckObjectOwnership(ObjectTypeType ot, ...) { (void)ot; return 0; }

static inline StatusType StartScheduleTableRel(ScheduleTableType s, TickType o) { (void)s; (void)o; return E_OK; }
static inline StatusType StartScheduleTableAbs(ScheduleTableType s, TickType o) { (void)s; (void)o; return E_OK; }
static inline StatusType StopScheduleTable(ScheduleTableType s) { (void)s; return E_OK; }
static inline StatusType NextScheduleTable(ScheduleTableType c, ScheduleTableType n) { (void)c; (void)n; return E_OK; }
static inline StatusType GetScheduleTableStatus(ScheduleTableType s, ScheduleTableStatusRefType r) { (void)s; if(r) *r = SCHEDULETABLE_STOPPED; return E_OK; }

static inline void ShutdownOS(StatusType e) { (void)e; }

#endif /* OS_H */
