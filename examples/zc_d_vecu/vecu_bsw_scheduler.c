/* vecu_bsw_scheduler.c — Cyclic BSW MainFunction dispatcher for vECU.
 *
 * Replicates the Core0 Rte task dispatch pattern from the generated
 * Rte_OS_Application_Core0_QM.c.  On the real target these are OS tasks
 * triggered by alarms; here they are called deterministically from
 * Base_Step() based on the tick counter.
 *
 * Task mapping (from GenData Rte_OS_Application_Core0_QM.c):
 *
 *   BswCom_SyncTask_HighPrio_Core0_QM  — 5 ms, 2 schedule-table steps
 *   BswNvm_SyncTask_LowPrio_Core0_QM   — 5 ms, 1 step
 *   BswSm_SyncTask_LowPrio_Core0_QM    — 10 ms, 2 schedule-table steps
 *   Bsw_SyncTask_HighPrio_Core0_QM     — 10 ms, 2 schedule-table steps
 *   Bsw_SyncTask_LowPrio_Core0_QM      — 10 ms, 3 schedule-table steps
 *
 * Assumes tick_interval_us = 1000 (1 ms per tick).
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "vecu_bsw_scheduler.h"

#ifdef VECU_BUILD

/* -----------------------------------------------------------------------
 * Forward declarations — all symbols exist in the linked dylib, either
 * as real MICROSAR BSW implementations or as no-op stubs in
 * vecu_appl_stubs.c.  We declare them here to avoid pulling in the
 * full BSW header graph.
 * ----------------------------------------------------------------------- */

/* BswCom_SyncTask_HighPrio_Core0_QM (5 ms, 2 steps) */
extern void EthIf_RxQueueProcMainFunction_EthIfRxQueueProcMainFunction_EcucPartition_Core0_QM(void);
extern void TcpIp_MainFunctionRx_EcucPartition_Core0_QM(void);
extern void SoAd_MainFunctionRx_SoAdInstance_OS_Application_Core0_QM(void);
extern void PduR_MainFunction(void);
extern void IpduM_MainFunctionRx_EcucPartition_Core0_QM(void);
extern void SecOC_MainFunctionRx_SecOCMainFunctionRx(void);
extern void TcpIp_MainFunctionState_EcucPartition_Core0_QM(void);
extern void SoAd_MainFunctionState_SoAdInstance_OS_Application_Core0_QM(void);
extern void Com_MainFunctionRx_ComMainFunctionRx(void);
extern void Com_MainFunctionRouteSignals_ComMainFunctionRouteSignals(void);
extern void Com_MainFunctionTx_ComMainFunctionTx(void);
extern void SecOC_MainFunctionTx_SecOCMainFunctionTx(void);
extern void IpduM_MainFunctionTx_EcucPartition_Core0_QM(void);
extern void EthIf_TxQueueProcMainFunction_EthIfTxQueueProcMainFunction_EcucPartition_Core0_QM(void);
extern void TcpIp_MainFunctionTx_EcucPartition_Core0_QM(void);
extern void DoIP_MainFunction(void);
extern void SoAd_MainFunctionTx_SoAdInstance_OS_Application_Core0_QM(void);
extern void CanTp_MainFunction(void);

/* BswNvm_SyncTask_LowPrio_Core0_QM (5 ms, 1 step) */
extern void NvM_MainFunction(void);

/* Virtual-MCAL CAN driver MainFunctions (replaces Can_30_Core_MainFunction_*
 * which are declared — not macro-mapped — in Can_30_Core.h and resolve to
 * no-op stubs.  We call the vmcal versions directly.) */
extern void Can_MainFunction_Read(void);
extern void Can_MainFunction_Write(void);
extern void Can_MainFunction_Mode(void);
extern void Can_MainFunction_BusOff(void);

/* BswSm_SyncTask_LowPrio_Core0_QM (10 ms, 2 steps) */
extern void EcuM_MainFunction(void);
extern void ComM_MainFunction_0(void);
extern void ComM_MainFunction_1(void);
extern void ComM_MainFunction_2(void);
extern void ComM_MainFunction_3(void);
extern void ComM_MainFunction_4(void);
extern void ComM_MainFunction_5(void);
extern void ComM_MainFunction_6(void);
extern void ComM_MainFunction_7(void);
extern void ComM_MainFunction_8(void);
extern void ComM_MainFunction_9(void);
extern void ComM_MainFunction_10(void);
extern void ComM_MainFunction_11(void);
extern void ComM_MainFunction_12(void);
extern void ComM_MainFunction_13(void);
extern void ComM_MainFunction_14(void);
extern void ComM_MainFunction_15(void);
extern void ComM_MainFunction_16(void);
extern void ComM_MainFunction_17(void);
extern void ComM_MainFunction_18(void);
extern void ComM_MainFunction_19(void);
extern void ComM_MainFunction_20(void);
extern void ComM_MainFunction_21(void);
extern void ComM_MainFunction_22(void);
extern void ComM_MainFunction_23(void);
extern void CanNm_MainFunction(void);
extern void CanSM_MainFunction(void);
extern void CanTrcv_30_Tja1043_MainFunction(void);
extern void Nm_MainFunction_OS_Application_Core0_QM(void);

/* Bsw_SyncTask_HighPrio_Core0_QM (10 ms, 2 steps) */
extern void BswM_MainFunction_OS_Application_Core0_QM(void);
extern void veGwM_MainFunction_App_OS_Application_Core0_QM(void);
extern void StbM_MainFunction_OS_Application_Core0_QM(void);
extern void TcpIpXcp_MainFunction(void);
extern void Dcm_MainFunctionTimer(void);

/* Bsw_SyncTask_LowPrio_Core0_QM (10 ms, 3 steps) */
extern void IdsM_MainFunction(void);
extern void KeyM_MainFunction(void);
extern void Csm_MainFunction(void);
extern void Crypto_30_vHsm_MainFunction(void);
extern void Dem_MasterMainFunction(void);
extern void Dem_SatelliteMainFunction(void);
extern void Spi_MainFunction_Handling(void);
extern void Dlt_MainFunction_OS_Application_Core0_QM(void);
extern void Xcp_MainFunction(void);
extern void Dcm_MainFunctionWorker(void);

/* -----------------------------------------------------------------------
 * Core1 — BswCom_SyncTask_HighPrio_Core1_QM (5 ms)
 * ----------------------------------------------------------------------- */
extern void Com_MainFunctionRx_ComMainFunctionRx_Core1(void);
extern void Com_MainFunctionRouteSignals_ComMainFunctionRouteSignalsCore1(void);
extern void Com_MainFunctionTx_ComMainFunctionTx_Core1(void);

/* Core1 — BswLin_SyncTask_HighPrio_Core1_QM (5 ms, 4-step schedule table) */
extern void LinSM_MainFunction(void);
extern void LinIf_MainFunction_FWLP_c6d0c047(void);
extern void LinIf_MainFunction_HVAC_COCKPIT_28c6cbd7(void);
extern void LinIf_MainFunction_HVAC_CTRL_1_a3d2a764(void);
extern void LinIf_MainFunction_HVAC_CTRL_2_2219f17c(void);
extern void LinIf_MainFunction_HVAC_DVC_1_36bcc97e(void);
extern void LinIf_MainFunction_HVAC_DVC_2_6bbf67f7(void);
extern void LinIf_MainFunction_HVAC_STEPPER_1_15166812(void);
extern void LinIf_MainFunction_HVAC_STEPPER_2_2e04b343(void);
extern void LinIf_MainFunction_INTERIOR_LIGHT_1_32fc6313(void);
extern void LinIf_MainFunction_INTERIOR_LIGHT_2_92e8f330(void);

/* Core1 — BswSm_SyncTask_LowPrio_Core1_QM (10 ms, 2-step schedule table) */
extern void ComM_MainFunction_8_OS_Application_Core1_QM(void);
extern void ComM_MainFunction_9_OS_Application_Core1_QM(void);
extern void ComM_MainFunction_10_OS_Application_Core1_QM(void);
extern void ComM_MainFunction_11_OS_Application_Core1_QM(void);
extern void ComM_MainFunction_12_OS_Application_Core1_QM(void);
extern void ComM_MainFunction_13_OS_Application_Core1_QM(void);
extern void ComM_MainFunction_14_OS_Application_Core1_QM(void);
extern void ComM_MainFunction_15_OS_Application_Core1_QM(void);
extern void ComM_MainFunction_16_OS_Application_Core1_QM(void);
extern void ComM_MainFunction_17_OS_Application_Core1_QM(void);
extern void Rtm_MainFunction_1(void);
extern void Dlt_MainFunction_OS_Application_Core1_QM(void);

/* -----------------------------------------------------------------------
 * Core2 — Bsw_veGw_Core2_QM (extended task, flattened to 5/10 ms cycles)
 * ----------------------------------------------------------------------- */
extern void EthIf_RxQueueProcMainFunction_EthIfRxQueueProcMainFunction_EcucPartition_Core2_QM(void);
extern void TcpIp_MainFunctionRx_EcucPartition_Core2_QM(void);
extern void IpduM_MainFunctionRx_EcucPartition_Core2_QM(void);
extern void SoAd_MainFunctionRx_SoAdInstance_OS_Application_Core2_QM(void);
extern void TcpIp_MainFunctionState_EcucPartition_Core2_QM(void);
extern void SoAd_MainFunctionState_SoAdInstance_OS_Application_Core2_QM(void);
extern void EthIf_MainFunctionTx(void);
extern void EthIf_MainFunctionRx(void);
extern void EthIf_MainFunctionState(void);
extern void SoAd_MainFunctionTx_SoAdInstance_OS_Application_Core2_QM(void);
extern void TcpIp_MainFunctionTx_EcucPartition_Core2_QM(void);
extern void EthIf_TxQueueProcMainFunction_EthIfTxQueueProcMainFunction_EcucPartition_Core2_QM(void);
extern void IpduM_MainFunctionTx_EcucPartition_Core2_QM(void);
extern void veGwM_MainFunction_GW_Core1_Polling(void);
extern void veGwM_MainFunction_GW_Core1_Cyclic(void);
extern void CanTSyn_MainFunction_OS_Application_Core2_QM(void);
extern void BswM_MainFunction_OS_Application_Core2_QM(void);
extern void Sd_MainFunction(void);
extern void Eth_30_RSwt23_MainFunction(void);
extern void EthSwt_30_RSwt23_MainFunctionUlSharedMgmtObj(void);
extern void Rtm_MainFunction_2(void);

/* -----------------------------------------------------------------------
 * Core3 — BswCom_SyncTask_HighPrio_Core3_QM (5 ms + 10 ms events)
 * ----------------------------------------------------------------------- */
extern void EthIf_RxQueueProcMainFunction_EthIfRxQueueProcMainFunction_EcucPartition_Core3_QM(void);
extern void TcpIp_MainFunctionRx_EcucPartition_Core3_QM(void);
extern void SoAd_MainFunctionRx_SoAdInstance_OS_Application_Core3_QM(void);
extern void Com_MainFunctionRx_ComMainFunctionRx_Core3(void);
extern void Com_MainFunctionRouteSignals_Com_MainFunctionRouteSignals_Core3(void);
extern void Com_MainFunctionTx_ComMainFunctionTx_Core3(void);
extern void EthIf_TxQueueProcMainFunction_EthIfTxQueueProcMainFunction_EcucPartition_Core3_QM(void);
extern void EthTSyn_MainFunction(void);
extern void TcpIp_MainFunctionTx_EcucPartition_Core3_QM(void);
extern void StbM_MainFunction_OS_Application_Core3_QM(void);

/* Core3 — BswSm_SyncTask_LowPrio_Core3_QM (10 ms, 2-step schedule table) */
extern void UdpNm_MainFunction_0(void);
extern void EthSM_MainFunction(void);
extern void Nm_MainFunction_OS_Application_Core3_QM(void);
extern void Rtm_MainFunction_3(void);

/* Core3 — Bsw_SyncTask_HighPrio_Core3_QM (10 ms, 2-step schedule table) */
extern void BswM_MainFunction_OS_Application_Core3_QM(void);
extern void ComM_MainFunction_18_OS_Application_Core3_QM(void);
extern void ComM_MainFunction_19_OS_Application_Core3_QM(void);
extern void ComM_MainFunction_20_OS_Application_Core3_QM(void);
extern void ComM_MainFunction_21_OS_Application_Core3_QM(void);
extern void ComM_MainFunction_22_OS_Application_Core3_QM(void);
extern void ComM_MainFunction_23_OS_Application_Core3_QM(void);
extern void Dlt_MainFunction_OS_Application_Core3_QM(void);

/* -----------------------------------------------------------------------
 * Core4 — BswCom_SyncTask_HighPrio_Core4_QM (5 ms)
 * ----------------------------------------------------------------------- */
extern void Com_MainFunctionRx_ComMainFunctionRx_Core4(void);
extern void Com_MainFunctionTx_ComMainFunctionTx_Core4(void);

/* Core4 — Bsw_SyncTask_HighPrio_Core4_QM (10 ms) */
extern void BswM_MainFunction_OS_Application_Core4_QM(void);
extern void Dlt_MainFunction_OS_Application_Core4_QM(void);
extern void Rtm_MainFunction_4(void);

/* ----------------------------------------------------------------------- */

static uint32_t s_bswcom_step;
static uint32_t s_bswsm_step;
static uint32_t s_bsw_highprio_step;
static uint32_t s_bsw_lowprio_step;

static uint32_t s_c1_lin_step;
static uint32_t s_c1_sm_step;
static uint32_t s_c3_sm_step;
static uint32_t s_c3_highprio_step;

void VecuBswScheduler_Init(void) {
    s_bswcom_step       = 0u;
    s_bswsm_step        = 0u;
    s_bsw_highprio_step = 0u;
    s_bsw_lowprio_step  = 0u;
    s_c1_lin_step       = 0u;
    s_c1_sm_step        = 0u;
    s_c3_sm_step        = 0u;
    s_c3_highprio_step  = 0u;
}

/* -----------------------------------------------------------------------
 * BswCom_SyncTask_HighPrio_Core0_QM — 5 ms period, 2 schedule-table steps
 * Source: Rte_OS_Application_Core0_QM.c lines 85653-85783
 *
 * Step 0: Full RX→Route→TX chain
 * Step 1: Abbreviated RX→TX chain (no Com/SecOC/IpduM)
 * ----------------------------------------------------------------------- */
static void BswCom_Dispatch(void) {
    if (s_bswcom_step == 0u) {
        EthIf_RxQueueProcMainFunction_EthIfRxQueueProcMainFunction_EcucPartition_Core0_QM();
        TcpIp_MainFunctionRx_EcucPartition_Core0_QM();
        SoAd_MainFunctionRx_SoAdInstance_OS_Application_Core0_QM();
        PduR_MainFunction();
        IpduM_MainFunctionRx_EcucPartition_Core0_QM();
        SecOC_MainFunctionRx_SecOCMainFunctionRx();
        TcpIp_MainFunctionState_EcucPartition_Core0_QM();
        SoAd_MainFunctionState_SoAdInstance_OS_Application_Core0_QM();
        Com_MainFunctionRx_ComMainFunctionRx();
        Com_MainFunctionRouteSignals_ComMainFunctionRouteSignals();
        Com_MainFunctionTx_ComMainFunctionTx();
        SecOC_MainFunctionTx_SecOCMainFunctionTx();
        IpduM_MainFunctionTx_EcucPartition_Core0_QM();
        EthIf_TxQueueProcMainFunction_EthIfTxQueueProcMainFunction_EcucPartition_Core0_QM();
        TcpIp_MainFunctionTx_EcucPartition_Core0_QM();
        DoIP_MainFunction();
        SoAd_MainFunctionTx_SoAdInstance_OS_Application_Core0_QM();
        s_bswcom_step = 1u;
    } else {
        EthIf_RxQueueProcMainFunction_EthIfRxQueueProcMainFunction_EcucPartition_Core0_QM();
        TcpIp_MainFunctionRx_EcucPartition_Core0_QM();
        SoAd_MainFunctionRx_SoAdInstance_OS_Application_Core0_QM();
        PduR_MainFunction();
        IpduM_MainFunctionRx_EcucPartition_Core0_QM();
        SecOC_MainFunctionRx_SecOCMainFunctionRx();
        TcpIp_MainFunctionState_EcucPartition_Core0_QM();
        SoAd_MainFunctionState_SoAdInstance_OS_Application_Core0_QM();
        Com_MainFunctionRx_ComMainFunctionRx();
        Com_MainFunctionRouteSignals_ComMainFunctionRouteSignals();
        Com_MainFunctionTx_ComMainFunctionTx();
        SecOC_MainFunctionTx_SecOCMainFunctionTx();
        IpduM_MainFunctionTx_EcucPartition_Core0_QM();
        EthIf_TxQueueProcMainFunction_EthIfTxQueueProcMainFunction_EcucPartition_Core0_QM();
        TcpIp_MainFunctionTx_EcucPartition_Core0_QM();
        DoIP_MainFunction();
        SoAd_MainFunctionTx_SoAdInstance_OS_Application_Core0_QM();
        s_bswcom_step = 0u;
    }
}

/* -----------------------------------------------------------------------
 * BswNvm_SyncTask_LowPrio_Core0_QM — 5 ms period, no schedule table
 * Source: Rte_OS_Application_Core0_QM.c lines 85798-85807
 * ----------------------------------------------------------------------- */
static void BswNvm_Dispatch(void) {
    NvM_MainFunction();
}

/* -----------------------------------------------------------------------
 * BswSm_SyncTask_LowPrio_Core0_QM — 10 ms period, 2 schedule-table steps
 * Source: Rte_OS_Application_Core0_QM.c lines 85821-86001
 *
 * Both steps call all 24 ComM channels + Nm.
 * Step 0 additionally calls EcuM, CanNm, CanSM, CanTrcv, Rtm.
 * ----------------------------------------------------------------------- */
static void BswSm_Dispatch(void) {
    if (s_bswsm_step == 0u) {
        EcuM_MainFunction();
        ComM_MainFunction_0();
        ComM_MainFunction_1();
        ComM_MainFunction_2();
        ComM_MainFunction_3();
        ComM_MainFunction_4();
        ComM_MainFunction_5();
        ComM_MainFunction_6();
        ComM_MainFunction_7();
        ComM_MainFunction_8();
        ComM_MainFunction_9();
        ComM_MainFunction_10();
        ComM_MainFunction_11();
        ComM_MainFunction_12();
        ComM_MainFunction_13();
        ComM_MainFunction_14();
        ComM_MainFunction_15();
        ComM_MainFunction_16();
        ComM_MainFunction_17();
        ComM_MainFunction_18();
        ComM_MainFunction_19();
        ComM_MainFunction_20();
        ComM_MainFunction_21();
        ComM_MainFunction_22();
        ComM_MainFunction_23();
        CanNm_MainFunction();
        CanSM_MainFunction();
        CanTrcv_30_Tja1043_MainFunction();
        Nm_MainFunction_OS_Application_Core0_QM();
        s_bswsm_step = 1u;
    } else {
        ComM_MainFunction_0();
        ComM_MainFunction_1();
        ComM_MainFunction_2();
        ComM_MainFunction_3();
        ComM_MainFunction_4();
        ComM_MainFunction_5();
        ComM_MainFunction_6();
        ComM_MainFunction_7();
        ComM_MainFunction_8();
        ComM_MainFunction_9();
        ComM_MainFunction_10();
        ComM_MainFunction_11();
        ComM_MainFunction_12();
        ComM_MainFunction_13();
        ComM_MainFunction_14();
        ComM_MainFunction_15();
        ComM_MainFunction_16();
        ComM_MainFunction_17();
        ComM_MainFunction_18();
        ComM_MainFunction_19();
        ComM_MainFunction_20();
        ComM_MainFunction_21();
        ComM_MainFunction_22();
        ComM_MainFunction_23();
        Nm_MainFunction_OS_Application_Core0_QM();
        s_bswsm_step = 0u;
    }
}

/* -----------------------------------------------------------------------
 * Bsw_SyncTask_HighPrio_Core0_QM — 10 ms period, 2 schedule-table steps
 * Source: Rte_OS_Application_Core0_QM.c lines 86069-86102
 *
 * Step 0: BswM, veGwM, StbM, TcpIpXcp, Dcm
 * Step 1: TcpIpXcp only
 * ----------------------------------------------------------------------- */
static void BswHighPrio_Dispatch(void) {
    if (s_bsw_highprio_step == 0u) {
        BswM_MainFunction_OS_Application_Core0_QM();
        veGwM_MainFunction_App_OS_Application_Core0_QM();
        StbM_MainFunction_OS_Application_Core0_QM();
        TcpIpXcp_MainFunction();
        Dcm_MainFunctionTimer();
        s_bsw_highprio_step = 1u;
    } else {
        TcpIpXcp_MainFunction();
        s_bsw_highprio_step = 0u;
    }
}

/* -----------------------------------------------------------------------
 * Bsw_SyncTask_LowPrio_Core0_QM — 10 ms period, 3 schedule-table steps
 * Source: Rte_OS_Application_Core0_QM.c lines 86116-86300+
 *
 * Step 0: IdsM, KeyM, Csm, Crypto, Dem, Spi, Dlt, Xcp, Dcm(Worker)
 * Step 1: Spi, Xcp
 * Step 2: IdsM, KeyM, Csm, Crypto, Dem, Spi, Dlt, Xcp
 * ----------------------------------------------------------------------- */
static void BswLowPrio_Dispatch(void) {
    if (s_bsw_lowprio_step == 0u) {
        IdsM_MainFunction();
        KeyM_MainFunction();
        Csm_MainFunction();
        Crypto_30_vHsm_MainFunction();
        Dem_MasterMainFunction();
        Dem_SatelliteMainFunction();
        Spi_MainFunction_Handling();
        Dlt_MainFunction_OS_Application_Core0_QM();
        Xcp_MainFunction();
        Dcm_MainFunctionWorker();
        s_bsw_lowprio_step = 1u;
    } else if (s_bsw_lowprio_step == 1u) {
        Spi_MainFunction_Handling();
        Xcp_MainFunction();
        s_bsw_lowprio_step = 2u;
    } else {
        IdsM_MainFunction();
        KeyM_MainFunction();
        Csm_MainFunction();
        Crypto_30_vHsm_MainFunction();
        Dem_MasterMainFunction();
        Dem_SatelliteMainFunction();
        Spi_MainFunction_Handling();
        Dlt_MainFunction_OS_Application_Core0_QM();
        Xcp_MainFunction();
        s_bsw_lowprio_step = 0u;
    }
}

/* =======================================================================
 * Core1 dispatch functions
 * ======================================================================= */

static void BswCom_Core1_Dispatch(void) {
    Com_MainFunctionRx_ComMainFunctionRx_Core1();
    Com_MainFunctionRouteSignals_ComMainFunctionRouteSignalsCore1();
    Com_MainFunctionTx_ComMainFunctionTx_Core1();
}

static void BswLin_Core1_Dispatch(void) {
    if (s_c1_lin_step == 0u) {
        LinSM_MainFunction();
        s_c1_lin_step = 1u;
    } else if (s_c1_lin_step <= 2u) {
        LinIf_MainFunction_FWLP_c6d0c047();
        LinIf_MainFunction_HVAC_COCKPIT_28c6cbd7();
        LinIf_MainFunction_HVAC_CTRL_1_a3d2a764();
        LinIf_MainFunction_HVAC_CTRL_2_2219f17c();
        LinIf_MainFunction_HVAC_DVC_1_36bcc97e();
        LinIf_MainFunction_HVAC_DVC_2_6bbf67f7();
        LinIf_MainFunction_HVAC_STEPPER_1_15166812();
        LinIf_MainFunction_HVAC_STEPPER_2_2e04b343();
        LinIf_MainFunction_INTERIOR_LIGHT_1_32fc6313();
        LinIf_MainFunction_INTERIOR_LIGHT_2_92e8f330();
        s_c1_lin_step = (s_c1_lin_step == 1u) ? 2u : 3u;
    } else {
        LinSM_MainFunction();
        s_c1_lin_step = 1u;
    }
}

static void BswSm_Core1_Dispatch(void) {
    if (s_c1_sm_step == 0u) {
        ComM_MainFunction_8_OS_Application_Core1_QM();
        ComM_MainFunction_9_OS_Application_Core1_QM();
        ComM_MainFunction_10_OS_Application_Core1_QM();
        ComM_MainFunction_11_OS_Application_Core1_QM();
        ComM_MainFunction_12_OS_Application_Core1_QM();
        ComM_MainFunction_13_OS_Application_Core1_QM();
        ComM_MainFunction_14_OS_Application_Core1_QM();
        ComM_MainFunction_15_OS_Application_Core1_QM();
        ComM_MainFunction_16_OS_Application_Core1_QM();
        ComM_MainFunction_17_OS_Application_Core1_QM();
        Rtm_MainFunction_1();
        s_c1_sm_step = 1u;
    } else {
        ComM_MainFunction_8_OS_Application_Core1_QM();
        ComM_MainFunction_9_OS_Application_Core1_QM();
        ComM_MainFunction_10_OS_Application_Core1_QM();
        ComM_MainFunction_11_OS_Application_Core1_QM();
        ComM_MainFunction_12_OS_Application_Core1_QM();
        ComM_MainFunction_13_OS_Application_Core1_QM();
        ComM_MainFunction_14_OS_Application_Core1_QM();
        ComM_MainFunction_15_OS_Application_Core1_QM();
        ComM_MainFunction_16_OS_Application_Core1_QM();
        ComM_MainFunction_17_OS_Application_Core1_QM();
        s_c1_sm_step = 0u;
    }
}

/* =======================================================================
 * Core2 dispatch — Bsw_veGw_Core2_QM flattened from event-driven to cyclic
 * ======================================================================= */

static void BswVeGw_Core2_5ms(void) {
    EthIf_RxQueueProcMainFunction_EthIfRxQueueProcMainFunction_EcucPartition_Core2_QM();
    TcpIp_MainFunctionRx_EcucPartition_Core2_QM();
    IpduM_MainFunctionRx_EcucPartition_Core2_QM();
    SoAd_MainFunctionRx_SoAdInstance_OS_Application_Core2_QM();
    TcpIp_MainFunctionState_EcucPartition_Core2_QM();
    SoAd_MainFunctionState_SoAdInstance_OS_Application_Core2_QM();
    EthIf_MainFunctionTx();
    EthIf_MainFunctionRx();
    PduR_MainFunction();
    EthIf_TxQueueProcMainFunction_EthIfTxQueueProcMainFunction_EcucPartition_Core2_QM();
    IpduM_MainFunctionTx_EcucPartition_Core2_QM();
    SoAd_MainFunctionTx_SoAdInstance_OS_Application_Core2_QM();
    TcpIp_MainFunctionTx_EcucPartition_Core2_QM();
}

static void BswVeGw_Core2_10ms(void) {
    veGwM_MainFunction_GW_Core1_Polling();
    veGwM_MainFunction_GW_Core1_Cyclic();
    CanTSyn_MainFunction_OS_Application_Core2_QM();
    BswM_MainFunction_OS_Application_Core2_QM();
    Sd_MainFunction();
    EthIf_MainFunctionState();
    Eth_30_RSwt23_MainFunction();
    EthSwt_30_RSwt23_MainFunctionUlSharedMgmtObj();
    Rtm_MainFunction_2();
}

/* =======================================================================
 * Core3 dispatch functions
 * ======================================================================= */

static void BswCom_Core3_5ms(void) {
    EthIf_RxQueueProcMainFunction_EthIfRxQueueProcMainFunction_EcucPartition_Core3_QM();
    TcpIp_MainFunctionRx_EcucPartition_Core3_QM();
    SoAd_MainFunctionRx_SoAdInstance_OS_Application_Core3_QM();
    Com_MainFunctionRx_ComMainFunctionRx_Core3();
    Com_MainFunctionRouteSignals_Com_MainFunctionRouteSignals_Core3();
    Com_MainFunctionTx_ComMainFunctionTx_Core3();
    EthIf_TxQueueProcMainFunction_EthIfTxQueueProcMainFunction_EcucPartition_Core3_QM();
    EthTSyn_MainFunction();
    TcpIp_MainFunctionTx_EcucPartition_Core3_QM();
}

static void BswCom_Core3_10ms(void) {
    StbM_MainFunction_OS_Application_Core3_QM();
}

static void BswSm_Core3_Dispatch(void) {
    if (s_c3_sm_step == 0u) {
        UdpNm_MainFunction_0();
        EthSM_MainFunction();
        Nm_MainFunction_OS_Application_Core3_QM();
        Rtm_MainFunction_3();
        s_c3_sm_step = 1u;
    } else {
        EthSM_MainFunction();
        Nm_MainFunction_OS_Application_Core3_QM();
        s_c3_sm_step = 0u;
    }
}

static void BswHighPrio_Core3_Dispatch(void) {
    if (s_c3_highprio_step == 0u) {
        BswM_MainFunction_OS_Application_Core3_QM();
        ComM_MainFunction_18_OS_Application_Core3_QM();
        ComM_MainFunction_19_OS_Application_Core3_QM();
        ComM_MainFunction_20_OS_Application_Core3_QM();
        ComM_MainFunction_21_OS_Application_Core3_QM();
        ComM_MainFunction_22_OS_Application_Core3_QM();
        ComM_MainFunction_23_OS_Application_Core3_QM();
        Dlt_MainFunction_OS_Application_Core3_QM();
        s_c3_highprio_step = 1u;
    } else {
        ComM_MainFunction_18_OS_Application_Core3_QM();
        ComM_MainFunction_19_OS_Application_Core3_QM();
        ComM_MainFunction_20_OS_Application_Core3_QM();
        ComM_MainFunction_21_OS_Application_Core3_QM();
        ComM_MainFunction_22_OS_Application_Core3_QM();
        ComM_MainFunction_23_OS_Application_Core3_QM();
        s_c3_highprio_step = 0u;
    }
}

/* =======================================================================
 * Core4 dispatch functions
 * ======================================================================= */

static void BswCom_Core4_Dispatch(void) {
    Com_MainFunctionRx_ComMainFunctionRx_Core4();
    Com_MainFunctionTx_ComMainFunctionTx_Core4();
}

static void BswHighPrio_Core4_Dispatch(void) {
    BswM_MainFunction_OS_Application_Core4_QM();
    Dlt_MainFunction_OS_Application_Core4_QM();
    Rtm_MainFunction_4();
}

/* -----------------------------------------------------------------------
 * Main entry point — called from Base_Step(tick) every 1 ms.
 *
 * Dispatches all 5 cores' BSW tasks at their configured cycle times.
 * Core0 tasks run first (highest priority), followed by Core1-4.
 * ----------------------------------------------------------------------- */
void VecuBswScheduler_Step(uint64_t tick) {
    /* === 5 ms tasks === */
    if ((tick % 5u) == 0u) {
        BswCom_Dispatch();          /* Core0 prio: highest among 5ms */
        BswNvm_Dispatch();          /* Core0 prio: 102 */
        BswCom_Core1_Dispatch();    /* Core1 5ms: Com RX/Route/TX */
        BswLin_Core1_Dispatch();    /* Core1 5ms: LinIf/LinSM */
        BswVeGw_Core2_5ms();        /* Core2 5ms: EthIf/TcpIp/SoAd/IpduM */
        BswCom_Core3_5ms();         /* Core3 5ms: EthIf/Com/EthTSyn/TcpIp */
        BswCom_Core4_Dispatch();    /* Core4 5ms: Com RX/TX */
    }

    /* === 10 ms tasks === */
    if ((tick % 10u) == 0u) {
        BswHighPrio_Dispatch();     /* Core0 prio: NON */
        BswSm_Dispatch();           /* Core0 prio: 103 */
        BswLowPrio_Dispatch();      /* Core0 prio: 100 */
        BswSm_Core1_Dispatch();     /* Core1 10ms: ComM channels 8-17 */
        BswVeGw_Core2_10ms();       /* Core2 10ms: veGwM/CanTSyn/BswM/Sd */
        BswCom_Core3_10ms();        /* Core3 10ms: StbM */
        BswSm_Core3_Dispatch();     /* Core3 10ms: UdpNm/EthSM/Nm */
        BswHighPrio_Core3_Dispatch(); /* Core3 10ms: BswM/ComM/Dlt */
        BswHighPrio_Core4_Dispatch(); /* Core4 10ms: BswM/Dlt */
    }

    /* Virtual-MCAL CAN driver: Read + Write every tick (matching the
     * LeanScheduler_Bsw_veGw_Core2_QM polling frequency on the real
     * target), Mode + BusOff every 10 ms. */
    Can_MainFunction_Read();
    Can_MainFunction_Write();

    if ((tick % 10u) == 0u) {
        Can_MainFunction_Mode();
        Can_MainFunction_BusOff();
    }

    /* CanTp requires frequent processing (called in veGwM polling loop
     * on real target, ~1 ms effective).  Call every tick. */
    CanTp_MainFunction();
}

#endif /* VECU_BUILD */
