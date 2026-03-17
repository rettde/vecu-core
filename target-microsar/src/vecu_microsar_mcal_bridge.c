/* vecu_microsar_mcal_bridge.c — MCAL bypass for Vector MICROSAR.
 *
 * Initializes the Virtual-MCAL layer and polls RX frames each tick.
 * MICROSAR's CanIf/EthIf/FrIf call the Virtual-MCAL Can/Eth/Fr drivers
 * instead of real hardware MCAL drivers.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "vecu_microsar_mcal_bridge.h"
#include "VMcal_Context.h"
#include "Can.h"
#include "Eth.h"
#include "Fr.h"
#include "Dio.h"
#include "Port.h"
#include "Spi.h"
#include "Gpt.h"
#include "Mcu.h"
#include "Fls.h"

#include <stddef.h>

static const vecu_base_context_t* g_bridge_ctx = NULL;

void MCALBridge_Init(const vecu_base_context_t* ctx) {
    g_bridge_ctx = ctx;

    VMcal_SetCtx(ctx);

    Mcu_Init(NULL);
    Mcu_InitClock(0);
    Port_Init(NULL);

    Can_Init(NULL);

    {
        Eth_ConfigType eth_cfg;
        eth_cfg.numCtrl = 1;
        Eth_Init(&eth_cfg);
        Eth_SetControllerMode(0, 1);
    }

    {
        Fr_ConfigType fr_cfg;
        fr_cfg.numCtrl = 1;
        Fr_Init(&fr_cfg);
    }

    Spi_Init(NULL);
    Gpt_Init(NULL);
    Fls_Init(NULL);
}

void MCALBridge_MainFunction(void) {
    if (g_bridge_ctx == NULL) { return; }

    Gpt_Tick();

    Can_MainFunction_Read();
    Can_MainFunction_Write();
    Eth_MainFunction();
    Fr_MainFunction();
    Spi_MainFunction();
    Fls_MainFunction();
}
