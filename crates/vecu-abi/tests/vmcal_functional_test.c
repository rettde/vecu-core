/* vmcal_functional_test.c — Functional tests for Virtual-MCAL modules.
 *
 * Tests the full lifecycle: Init → MainFunction → Shutdown
 * with mock vecu_base_context_t callbacks and CanIf/EthIf stubs.
 *
 * Returns 0 on success, non-zero on failure.
 *
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include "Std_Types.h"
#include "vecu_base_context.h"
#include "vecu_frame.h"
#include "vecu_status.h"
#include "VMcal_Context.h"
#include "Can.h"
#include "Eth.h"
#include "Fls.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ── Mock frame queues ─────────────────────────────────────────────── */

#define MOCK_QUEUE_SIZE 16

static vecu_frame_t g_tx_queue[MOCK_QUEUE_SIZE];
static int          g_tx_count = 0;

static vecu_frame_t g_rx_queue[MOCK_QUEUE_SIZE];
static int          g_rx_head  = 0;
static int          g_rx_count = 0;

static int mock_push_tx(const vecu_frame_t* frame) {
    if (g_tx_count >= MOCK_QUEUE_SIZE) { return VECU_MODULE_ERROR; }
    memcpy(&g_tx_queue[g_tx_count], frame, sizeof(vecu_frame_t));
    g_tx_count++;
    return VECU_OK;
}

static int mock_pop_rx(vecu_frame_t* frame) {
    if (g_rx_count == 0) { return VECU_NOT_SUPPORTED; }
    memcpy(frame, &g_rx_queue[g_rx_head], sizeof(vecu_frame_t));
    g_rx_head = (g_rx_head + 1) % MOCK_QUEUE_SIZE;
    g_rx_count--;
    return VECU_OK;
}

static void mock_inject_rx(uint32_t id, uint32_t bus_type,
                            const uint8_t* data, uint32_t len) {
    int idx = (g_rx_head + g_rx_count) % MOCK_QUEUE_SIZE;
    memset(&g_rx_queue[idx], 0, sizeof(vecu_frame_t));
    g_rx_queue[idx].id       = id;
    g_rx_queue[idx].bus_type = bus_type;
    g_rx_queue[idx].len      = len;
    if (data != NULL && len > 0) {
        memcpy(g_rx_queue[idx].data, data, len);
    }
    g_rx_count++;
}

/* ── Mock SHM ──────────────────────────────────────────────────────── */

#define MOCK_SHM_SIZE 4096
static uint8_t g_mock_shm[MOCK_SHM_SIZE];

/* ── Mock log ──────────────────────────────────────────────────────── */

static void mock_log(uint32_t level, const char* msg) {
    (void)level;
    (void)msg;
}

/* ── CanIf callback tracking ───────────────────────────────────────── */

static int g_canif_rx_count = 0;
static uint32 g_canif_last_rx_id = 0;
static uint8  g_canif_last_rx_dlc = 0;
static uint8  g_canif_last_rx_data[64];

static int g_canif_tx_conf_count = 0;
static Can_HwHandleType g_canif_last_tx_hth = 0;

static int g_canif_mode_ind_count = 0;
static Can_ControllerStateType g_canif_last_mode = CAN_CS_UNINIT;

void CanIf_RxIndication(Can_HwHandleType Hrh, uint32 CanId,
                         uint8 CanDlc, const uint8* CanSduPtr) {
    (void)Hrh;
    g_canif_rx_count++;
    g_canif_last_rx_id = CanId;
    g_canif_last_rx_dlc = CanDlc;
    if (CanSduPtr != NULL && CanDlc > 0) {
        memcpy(g_canif_last_rx_data, CanSduPtr, CanDlc);
    }
}

void CanIf_TxConfirmation(Can_HwHandleType Hth) {
    g_canif_tx_conf_count++;
    g_canif_last_tx_hth = Hth;
}

void CanIf_ControllerModeIndication(uint8 ControllerId,
                                     Can_ControllerStateType ControllerMode) {
    (void)ControllerId;
    g_canif_mode_ind_count++;
    g_canif_last_mode = ControllerMode;
}

/* ── EthIf callback tracking ───────────────────────────────────────── */

static int g_ethif_rx_count = 0;
static uint16 g_ethif_last_rx_len = 0;

static int g_ethif_tx_conf_count = 0;
static int g_ethif_mode_ind_count = 0;
static Eth_ModeType g_ethif_last_mode = ETH_MODE_DOWN;

void EthIf_RxIndication(uint8 CtrlIdx, Eth_FrameType FrameType,
                         boolean IsBroadcast, const uint8* PhysAddrPtr,
                         const uint8* DataPtr, uint16 LenByte) {
    (void)CtrlIdx;
    (void)FrameType;
    (void)IsBroadcast;
    (void)PhysAddrPtr;
    (void)DataPtr;
    g_ethif_rx_count++;
    g_ethif_last_rx_len = LenByte;
}

void EthIf_TxConfirmation(uint8 CtrlIdx, Eth_BufIdxType BufIdx) {
    (void)CtrlIdx;
    (void)BufIdx;
    g_ethif_tx_conf_count++;
}

void EthIf_CtrlModeIndication(uint8 CtrlIdx, Eth_ModeType CtrlMode) {
    (void)CtrlIdx;
    g_ethif_mode_ind_count++;
    g_ethif_last_mode = CtrlMode;
}

/* ── Fls callback tracking ─────────────────────────────────────────── */

static int g_fls_end_count = 0;
static int g_fls_error_count = 0;

static void fls_job_end(void) { g_fls_end_count++; }
static void fls_job_error(void) { g_fls_error_count++; }

/* ── Assertion helper ──────────────────────────────────────────────── */

static int g_test_failures = 0;

#define ASSERT_EQ(actual, expected, msg) do { \
    if ((actual) != (expected)) { \
        fprintf(stderr, "FAIL: %s: expected %d, got %d\n", \
                (msg), (int)(expected), (int)(actual)); \
        g_test_failures++; \
    } \
} while(0)

#define ASSERT_MEM_EQ(a, b, n, msg) do { \
    if (memcmp((a), (b), (n)) != 0) { \
        fprintf(stderr, "FAIL: %s: memory mismatch\n", (msg)); \
        g_test_failures++; \
    } \
} while(0)

/* ── Test: CAN controller state machine ────────────────────────────── */

static void test_can_controller_state(void) {
    Can_ConfigType cfg = {1, 1};
    Can_Init(&cfg);
    ASSERT_EQ(Can_GetControllerMode(0), CAN_CS_STOPPED, "Can after init");

    ASSERT_EQ(Can_SetControllerMode(0, CAN_T_START), E_OK, "Can start");
    ASSERT_EQ(Can_GetControllerMode(0), CAN_CS_STARTED, "Can started");
    ASSERT_EQ(g_canif_mode_ind_count, 1, "mode indication count");
    ASSERT_EQ(g_canif_last_mode, CAN_CS_STARTED, "mode indication value");

    ASSERT_EQ(Can_SetControllerMode(0, CAN_T_STOP), E_OK, "Can stop");
    ASSERT_EQ(Can_GetControllerMode(0), CAN_CS_STOPPED, "Can stopped");

    ASSERT_EQ(Can_SetControllerMode(0, CAN_T_SLEEP), E_OK, "Can sleep");
    ASSERT_EQ(Can_GetControllerMode(0), CAN_CS_SLEEP, "Can sleeping");

    ASSERT_EQ(Can_SetControllerMode(0, CAN_T_WAKEUP), E_OK, "Can wakeup");
    ASSERT_EQ(Can_GetControllerMode(0), CAN_CS_STOPPED, "Can woke");

    ASSERT_EQ(Can_SetControllerMode(0, CAN_T_SLEEP), E_OK, "sleep again");
    ASSERT_EQ(Can_SetControllerMode(0, CAN_T_START), E_NOT_OK, "start from sleep");

    Can_DeInit();
    ASSERT_EQ(Can_GetControllerMode(0), CAN_CS_UNINIT, "Can after deinit");
}

/* ── Test: CAN TX → TxConfirmation ─────────────────────────────────── */

static void test_can_tx_confirmation(void) {
    g_tx_count = 0;
    g_canif_tx_conf_count = 0;

    Can_ConfigType cfg = {1, 1};
    Can_Init(&cfg);
    Can_SetControllerMode(0, CAN_T_START);

    uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
    Can_PduType pdu;
    pdu.id = 0x100;
    pdu.length = 4;
    memset(pdu._pad, 0, sizeof(pdu._pad));
    pdu.sdu = payload;

    ASSERT_EQ(Can_Write(5, &pdu), E_OK, "Can_Write");
    ASSERT_EQ(g_tx_count, 1, "TX queue count");
    ASSERT_EQ(g_tx_queue[0].id, 0x100, "TX frame id");
    ASSERT_EQ(g_tx_queue[0].bus_type, VECU_BUS_CAN, "TX bus type");
    ASSERT_EQ(g_tx_queue[0].len, 4, "TX frame len");
    ASSERT_MEM_EQ(g_tx_queue[0].data, payload, 4, "TX frame data");

    Can_MainFunction_Write();
    ASSERT_EQ(g_canif_tx_conf_count, 1, "TxConfirmation count");
    ASSERT_EQ(g_canif_last_tx_hth, 5, "TxConfirmation HTH");

    Can_MainFunction_Write();
    ASSERT_EQ(g_canif_tx_conf_count, 1, "no extra TxConf");

    Can_DeInit();
}

/* ── Test: CAN RX → RxIndication ───────────────────────────────────── */

static void test_can_rx_indication(void) {
    g_canif_rx_count = 0;
    g_rx_head = 0;
    g_rx_count = 0;

    Can_ConfigType cfg = {1, 1};
    Can_Init(&cfg);
    Can_SetControllerMode(0, CAN_T_START);

    uint8_t rx_data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    mock_inject_rx(0x200, VECU_BUS_CAN, rx_data, 8);

    Can_MainFunction_Read();
    ASSERT_EQ(g_canif_rx_count, 1, "RxIndication count");
    ASSERT_EQ(g_canif_last_rx_id, 0x200, "RxIndication CAN ID");
    ASSERT_EQ(g_canif_last_rx_dlc, 8, "RxIndication DLC");
    ASSERT_MEM_EQ(g_canif_last_rx_data, rx_data, 8, "RxIndication data");

    Can_MainFunction_Read();
    ASSERT_EQ(g_canif_rx_count, 1, "no extra RxInd");

    Can_DeInit();
}

/* ── Test: CAN write fails in STOPPED state ────────────────────────── */

static void test_can_write_requires_started(void) {
    g_tx_count = 0;

    Can_ConfigType cfg = {1, 1};
    Can_Init(&cfg);

    uint8_t payload[] = {0xFF};
    Can_PduType pdu;
    pdu.id = 0x300;
    pdu.length = 1;
    memset(pdu._pad, 0, sizeof(pdu._pad));
    pdu.sdu = payload;

    ASSERT_EQ(Can_Write(0, &pdu), E_NOT_OK, "Write in STOPPED");
    ASSERT_EQ(g_tx_count, 0, "no TX in STOPPED");

    Can_DeInit();
}

/* ── Test: Eth controller mode + TX/RX ─────────────────────────────── */

static void test_eth_lifecycle(void) {
    g_tx_count = 0;
    g_rx_head = 0;
    g_rx_count = 0;
    g_ethif_rx_count = 0;
    g_ethif_tx_conf_count = 0;
    g_ethif_mode_ind_count = 0;

    Eth_ConfigType eth_cfg = {1};
    Eth_Init(&eth_cfg);
    ASSERT_EQ(Eth_GetControllerMode(0), ETH_MODE_DOWN, "Eth init mode");

    Eth_BufIdxType buf_idx;
    uint8* buf_ptr;
    uint16 buf_len;
    ASSERT_EQ(Eth_ProvideTxBuffer(0, &buf_idx, &buf_ptr, &buf_len), E_NOT_OK,
              "ProvideTxBuffer in DOWN");

    Eth_SetControllerMode(0, ETH_MODE_ACTIVE);
    ASSERT_EQ(Eth_GetControllerMode(0), ETH_MODE_ACTIVE, "Eth active");
    ASSERT_EQ(g_ethif_mode_ind_count, 1, "EthIf mode ind count");
    ASSERT_EQ(g_ethif_last_mode, ETH_MODE_ACTIVE, "EthIf mode value");

    ASSERT_EQ(Eth_ProvideTxBuffer(0, &buf_idx, &buf_ptr, &buf_len), E_OK,
              "ProvideTxBuffer in ACTIVE");
    buf_ptr[0] = 0xAA;
    buf_ptr[1] = 0xBB;
    ASSERT_EQ(Eth_Transmit(0, buf_idx, 0x0800, TRUE, 2, NULL), E_OK,
              "Eth_Transmit");
    ASSERT_EQ(g_tx_count, 1, "ETH TX count");
    ASSERT_EQ(g_tx_queue[0].bus_type, VECU_BUS_ETH, "ETH TX bus type");

    Eth_MainFunction();
    ASSERT_EQ(g_ethif_tx_conf_count, 1, "EthIf TxConf count");

    uint8_t eth_rx[] = {0x11, 0x22, 0x33};
    mock_inject_rx(0, VECU_BUS_ETH, eth_rx, 3);
    Eth_MainFunction();
    ASSERT_EQ(g_ethif_rx_count, 1, "EthIf RxIndication count");
    ASSERT_EQ(g_ethif_last_rx_len, 3, "EthIf RxIndication len");

    Eth_DeInit();
}

/* ── Test: Fls async job model ─────────────────────────────────────── */

static void test_fls_async(void) {
    g_fls_end_count = 0;
    g_fls_error_count = 0;
    memset(g_mock_shm, 0xFF, MOCK_SHM_SIZE);

    Fls_ConfigType fls_cfg;
    fls_cfg.flashSize = MOCK_SHM_SIZE;
    fls_cfg.jobEndNotification = fls_job_end;
    fls_cfg.jobErrorNotification = fls_job_error;
    Fls_Init(&fls_cfg);

    ASSERT_EQ(Fls_GetStatus(), MEMIF_IDLE, "Fls init idle");
    ASSERT_EQ(Fls_GetJobResult(), MEMIF_JOB_OK, "Fls init job ok");

    uint8_t write_data[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    ASSERT_EQ(Fls_Write(0x100, write_data, 6), E_OK, "Fls_Write accept");
    ASSERT_EQ(Fls_GetStatus(), MEMIF_BUSY, "Fls busy after write");
    ASSERT_EQ(Fls_GetJobResult(), MEMIF_JOB_PENDING, "Fls pending");

    ASSERT_EQ(Fls_Write(0x200, write_data, 6), E_NOT_OK, "reject while busy");

    Fls_MainFunction();
    ASSERT_EQ(Fls_GetStatus(), MEMIF_IDLE, "Fls idle after main");
    ASSERT_EQ(Fls_GetJobResult(), MEMIF_JOB_OK, "Fls job ok after main");
    ASSERT_EQ(g_fls_end_count, 1, "Fls end notification");
    ASSERT_MEM_EQ(g_mock_shm + 0x100, write_data, 6, "SHM write verify");

    uint8_t read_buf[6] = {0};
    ASSERT_EQ(Fls_Read(0x100, read_buf, 6), E_OK, "Fls_Read accept");
    ASSERT_EQ(Fls_GetStatus(), MEMIF_BUSY, "Fls busy after read");
    Fls_MainFunction();
    ASSERT_EQ(g_fls_end_count, 2, "Fls end notification 2");
    ASSERT_MEM_EQ(read_buf, write_data, 6, "Fls read data");

    ASSERT_EQ(Fls_Erase(0x100, 6), E_OK, "Fls_Erase accept");
    Fls_MainFunction();
    ASSERT_EQ(g_fls_end_count, 3, "Fls end notification 3");
    uint8_t erased[6];
    memset(erased, 0xFF, 6);
    ASSERT_MEM_EQ(g_mock_shm + 0x100, erased, 6, "SHM erase verify");

    Fls_DeInit();
    ASSERT_EQ(Fls_GetStatus(), MEMIF_UNINIT, "Fls deinit");
}

/* ── Test: Fls cancel ──────────────────────────────────────────────── */

static void test_fls_cancel(void) {
    g_fls_end_count = 0;
    g_fls_error_count = 0;

    Fls_ConfigType fls_cfg;
    fls_cfg.flashSize = MOCK_SHM_SIZE;
    fls_cfg.jobEndNotification = fls_job_end;
    fls_cfg.jobErrorNotification = fls_job_error;
    Fls_Init(&fls_cfg);

    uint8_t data[] = {0x01, 0x02};
    Fls_Write(0, data, 2);
    ASSERT_EQ(Fls_GetStatus(), MEMIF_BUSY, "busy before cancel");

    Fls_Cancel();
    ASSERT_EQ(Fls_GetStatus(), MEMIF_IDLE, "idle after cancel");
    ASSERT_EQ(Fls_GetJobResult(), MEMIF_JOB_CANCELED, "job canceled");

    Fls_MainFunction();
    ASSERT_EQ(g_fls_end_count, 0, "no end notif after cancel");

    Fls_DeInit();
}

/* ── Test: Fls error on out-of-bounds ──────────────────────────────── */

static void test_fls_error(void) {
    g_fls_end_count = 0;
    g_fls_error_count = 0;

    Fls_ConfigType fls_cfg;
    fls_cfg.flashSize = MOCK_SHM_SIZE;
    fls_cfg.jobEndNotification = fls_job_end;
    fls_cfg.jobErrorNotification = fls_job_error;
    Fls_Init(&fls_cfg);

    uint8_t buf[4];
    ASSERT_EQ(Fls_Read(MOCK_SHM_SIZE - 2, buf, 4), E_OK, "accept OOB read");
    Fls_MainFunction();
    ASSERT_EQ(Fls_GetJobResult(), MEMIF_JOB_FAILED, "OOB read fails");
    ASSERT_EQ(g_fls_error_count, 1, "error notification");

    Fls_DeInit();
}

/* ── main ──────────────────────────────────────────────────────────── */

int main(void) {
    vecu_base_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.push_tx_frame  = mock_push_tx;
    ctx.pop_rx_frame   = mock_pop_rx;
    ctx.shm_vars       = g_mock_shm;
    ctx.shm_vars_size  = MOCK_SHM_SIZE;
    ctx.log_fn         = mock_log;
    ctx.tick_interval_us = 1000;

    VMcal_Init(&ctx);

    test_can_controller_state();
    test_can_tx_confirmation();
    test_can_rx_indication();
    test_can_write_requires_started();
    test_eth_lifecycle();
    test_fls_async();
    test_fls_cancel();
    test_fls_error();

    if (g_test_failures == 0) {
        printf("vmcal_functional_test: ALL PASSED\n");
    } else {
        fprintf(stderr, "vmcal_functional_test: %d FAILURES\n", g_test_failures);
    }
    return g_test_failures;
}
