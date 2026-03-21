/* vecu_bsw_scheduler.h — Cyclic BSW MainFunction dispatcher for vECU.
 *
 * On the real target the MICROSAR OS runs cyclic tasks that call BSW
 * MainFunctions at configured intervals.  On vECU there is no real OS,
 * so this scheduler replicates the Core0 Rte task dispatch pattern
 * extracted from the generated Rte_OS_Application_Core0_QM.c.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */
#ifndef VECU_BSW_SCHEDULER_H
#define VECU_BSW_SCHEDULER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void VecuBswScheduler_Init(void);
void VecuBswScheduler_Step(uint64_t tick);

#ifdef __cplusplus
}
#endif

#endif /* VECU_BSW_SCHEDULER_H */
