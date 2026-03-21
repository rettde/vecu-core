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

/* ----------------------------------------------------------------------- */

static uint32_t s_bswcom_step;
static uint32_t s_bswsm_step;
static uint32_t s_bsw_highprio_step;
static uint32_t s_bsw_lowprio_step;

void VecuBswScheduler_Init(void) {
    s_bswcom_step       = 0u;
    s_bswsm_step        = 0u;
    s_bsw_highprio_step = 0u;
    s_bsw_lowprio_step  = 0u;
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

/* -----------------------------------------------------------------------
 * Main entry point — called from Base_Step(tick) every 1 ms.
 *
 * Dispatches the Core0 BSW tasks at their configured cycle times,
 * respecting priority order (higher priority tasks run first).
 * ----------------------------------------------------------------------- */
void VecuBswScheduler_Step(uint64_t tick) {
    /* 5 ms tasks */
    if ((tick % 5u) == 0u) {
        BswCom_Dispatch();          /* prio: highest among 5ms */
        BswNvm_Dispatch();          /* prio: 102 */
    }

    /* 10 ms tasks */
    if ((tick % 10u) == 0u) {
        BswHighPrio_Dispatch();     /* prio: NON (non-preemptive) */
        BswSm_Dispatch();           /* prio: 103 */
        BswLowPrio_Dispatch();      /* prio: 100 (FULL) */
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
