/* vecu_microsar_mcal_bridge.h — MCAL bypass for Vector MICROSAR on vecu-core.
 *
 * Redirects MICROSAR's MCAL calls to the Virtual-MCAL layer (ADR-002).
 * This allows running MICROSAR BSW on a host PC without real hardware.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef VECU_MICROSAR_MCAL_BRIDGE_H
#define VECU_MICROSAR_MCAL_BRIDGE_H

#include "vecu_base_context.h"

void MCALBridge_Init(const vecu_base_context_t* ctx);
void MCALBridge_MainFunction(void);

#endif /* VECU_MICROSAR_MCAL_BRIDGE_H */
