/* vmcal_compile_check.c — Compile-time validation for Virtual-MCAL,
 * vHsm adapter, and OS-Semantics Mapping (ADR-006).
 *
 * This file includes all public headers and exercises type definitions
 * to verify API consistency at compile time.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Std_Types.h"
#include "ComStack_Types.h"
#include "Can_GeneralTypes.h"
#include "Eth_GeneralTypes.h"
#include "MemIf_Types.h"
#include "vecu_frame.h"
#include "vecu_status.h"
#include "vecu_base_context.h"

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
#include "Adc.h"
#include "Pwm.h"
#include "Wdg.h"
#include "Lin.h"
#include "Icu.h"

#include "Crypto_30_vHsm.h"

#include "Os_Task.h"
#include "Os_Mapping.h"

_Static_assert(sizeof(vecu_frame_t) == 1560,
               "vecu_frame_t must be 1560 bytes");

_Static_assert(sizeof(Can_PduType) > 0,
               "Can_PduType must be defined");

_Static_assert(sizeof(Crypto_JobType) > 0,
               "Crypto_JobType must be defined");

_Static_assert(sizeof(Os_TaskConfigType) > 0,
               "Os_TaskConfigType must be defined");

_Static_assert(DIO_MAX_CHANNELS == 256u,
               "DIO_MAX_CHANNELS must be 256");

_Static_assert(GPT_MAX_CHANNELS == 16u,
               "GPT_MAX_CHANNELS must be 16");

_Static_assert(OS_MAX_TASKS == 64u,
               "OS_MAX_TASKS must be 64");

_Static_assert(OS_MAX_ALARMS == 32u,
               "OS_MAX_ALARMS must be 32");

static void smoke_test_types(void) {
    Can_PduType can_pdu;
    can_pdu.swPduHandle = 0;
    can_pdu.id = 0x123;
    can_pdu.length = 8;
    can_pdu.sdu = (uint8*)0;
    (void)can_pdu;

    PduInfoType pdu_info;
    pdu_info.SduDataPtr = (uint8*)0;
    pdu_info.MetaDataPtr = (uint8*)0;
    pdu_info.SduLength = 0;
    (void)pdu_info;

    Can_HwType hw;
    hw.CanId = 0;
    hw.Hoh = 0;
    hw.ControllerId = 0;
    (void)hw;

    Can_ErrorStateType err = CAN_ERRORSTATE_ACTIVE;
    (void)err;

    Eth_ConfigType eth_cfg;
    eth_cfg.numCtrl = 1;
    (void)eth_cfg;

    Fr_ConfigType fr_cfg;
    fr_cfg.numCtrl = 1;
    (void)fr_cfg;

    Dio_LevelType lvl = Dio_ReadChannel(0);
    (void)lvl;

    Gpt_ValueType tv = 0;
    (void)tv;

    Mcu_ResetType rst = MCU_POWER_ON_RESET;
    (void)rst;

    MemIf_StatusType fls_st = MEMIF_UNINIT;
    (void)fls_st;

    Adc_StatusType adc_st = ADC_IDLE;
    (void)adc_st;

    Pwm_DutyType pwm_d = 0;
    (void)pwm_d;

    Wdg_ModeType wdg_m = WDG_MODE_OFF;
    (void)wdg_m;

    Lin_StatusType lin_st = LIN_NOT_OK;
    (void)lin_st;

    Icu_InputStateType icu_st = ICU_IDLE;
    (void)icu_st;

    Icu_ActivationType icu_act = ICU_BOTH_EDGES;
    (void)icu_act;

    Crypto_30_vHsm_ConfigType hsm_cfg;
    hsm_cfg.numKeys = 20;
    (void)hsm_cfg;

    Crypto_AlgorithmFamilyType sha_fam = CRYPTO_ALGOFAM_SHA2_256;
    (void)sha_fam;

    Os_PhaseType phase = OS_PHASE_UNINIT;
    (void)phase;

    Os_ConfigType os_cfg;
    os_cfg.tasks = (const Os_TaskConfigType*)0;
    os_cfg.numTasks = 0;
    os_cfg.alarms = (const Os_AlarmConfigType*)0;
    os_cfg.numAlarms = 0;
    os_cfg.counters = (const Os_CounterConfigType*)0;
    os_cfg.numCounters = 0;
    (void)os_cfg;
}
