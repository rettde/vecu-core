//! Minimal FFI bindings for the Vector SIL Kit C API.
//!
//! Only the subset required by the vECU runtime adapter is declared here.
//! All symbols are loaded **dynamically** via [`libloading`] so the SIL Kit
//! shared library is an optional runtime dependency, not a build‑time one.

#![allow(non_camel_case_types, clippy::upper_case_acronyms)]

use std::ffi::c_char;
use std::os::raw::c_void;

// ---------------------------------------------------------------------------
// Primitive type aliases (mirror SIL Kit C API)
// ---------------------------------------------------------------------------

/// SIL Kit return code (`int32_t`).
pub type SilKit_ReturnCode = i32;

/// Simulation time in nanoseconds (`uint64_t`).
pub type SilKit_NanosecondsTime = u64;

/// Participant state enum (`int32_t`).
pub type SilKit_ParticipantState = i32;

/// Operation mode enum (`int32_t`).
pub type SilKit_OperationMode = i32;

/// Handler identifier.
pub type SilKit_HandlerId = u64;

/// CAN frame flags.
pub type SilKit_CanFrameFlag = u32;

/// CAN transmit status.
pub type SilKit_CanTransmitStatus = i32;

/// Direction indicator.
pub type SilKit_Direction = i32;

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

/// Success.
pub const SK_OK: SilKit_ReturnCode = 0;

/// Coordinated operation mode – participants synchronise lifecycle.
pub const SK_OPERATION_MODE_COORDINATED: SilKit_OperationMode = 10;

/// Autonomous operation mode – participant runs independently.
pub const SK_OPERATION_MODE_AUTONOMOUS: SilKit_OperationMode = 20;

/// Participant state: running.
pub const SK_STATE_RUNNING: SilKit_ParticipantState = 50;
/// Participant state: stopped.
pub const SK_STATE_STOPPED: SilKit_ParticipantState = 80;
/// Participant state: shutdown.
pub const SK_STATE_SHUTDOWN: SilKit_ParticipantState = 110;

// ---------------------------------------------------------------------------
// Opaque handles (SIL Kit objects are always behind pointers)
// ---------------------------------------------------------------------------

/// Opaque SIL Kit Participant.
#[repr(C)]
pub struct SilKit_Participant {
    _private: [u8; 0],
}

/// Opaque participant configuration.
#[repr(C)]
pub struct SilKit_ParticipantConfiguration {
    _private: [u8; 0],
}

/// Opaque lifecycle service.
#[repr(C)]
pub struct SilKit_LifecycleService {
    _private: [u8; 0],
}

/// Opaque time sync service.
#[repr(C)]
pub struct SilKit_TimeSyncService {
    _private: [u8; 0],
}

/// Opaque CAN controller.
#[repr(C)]
pub struct SilKit_CanController {
    _private: [u8; 0],
}

/// Opaque Ethernet controller.
#[repr(C)]
pub struct SilKit_EthernetController {
    _private: [u8; 0],
}

/// Opaque LIN controller.
#[repr(C)]
pub struct SilKit_LinController {
    _private: [u8; 0],
}

/// Opaque `FlexRay` controller (`void*` in the SIL Kit C API).
#[repr(C)]
pub struct SilKit_FlexrayController {
    _private: [u8; 0],
}

// ---------------------------------------------------------------------------
// Struct header (SIL Kit struct versioning)
// ---------------------------------------------------------------------------

/// Versioned struct header present at the start of every SIL Kit struct.
#[repr(C, packed(8))]
#[derive(Debug, Clone, Copy)]
pub struct SilKit_StructHeader {
    /// Struct version identifier.
    pub version: u32,
    /// Reserved padding.
    pub pad0: u32,
}

impl SilKit_StructHeader {
    /// Create a zeroed header (version 1).
    #[must_use]
    pub fn v1() -> Self {
        Self {
            version: 1,
            pad0: 0,
        }
    }
}

// ---------------------------------------------------------------------------
// Byte vector (variable‑length data)
// ---------------------------------------------------------------------------

/// Variable‑length byte buffer used by SIL Kit for frame payloads.
#[repr(C, packed(8))]
pub struct SilKit_ByteVector {
    /// Pointer to the data bytes.
    pub data: *const u8,
    /// Number of bytes.
    pub size: u32,
    /// Padding.
    pub pad0: u32,
}

// ---------------------------------------------------------------------------
// CAN frame
// ---------------------------------------------------------------------------

/// SIL Kit CAN frame (subset of fields we use).
#[repr(C, packed(8))]
pub struct SilKit_CanFrame {
    /// Struct version header.
    pub struct_header: SilKit_StructHeader,
    /// CAN identifier.
    pub id: u32,
    /// CAN flags.
    pub flags: SilKit_CanFrameFlag,
    /// Data length code.
    pub dlc: u16,
    /// SDU type (XL only).
    pub sdt: u8,
    /// Virtual CAN network ID (XL only).
    pub vcid: u8,
    /// Acceptance field (XL only).
    pub af: u32,
    /// Payload data.
    pub data: SilKit_ByteVector,
}

/// Incoming CAN frame event.
#[repr(C, packed(8))]
pub struct SilKit_CanFrameEvent {
    /// Struct version header.
    pub struct_header: SilKit_StructHeader,
    /// Reception timestamp (ns).
    pub timestamp: SilKit_NanosecondsTime,
    /// Pointer to the CAN frame.
    pub frame: *const SilKit_CanFrame,
    /// Direction (TX/RX).
    pub direction: SilKit_Direction,
    /// User context.
    pub user_context: *mut c_void,
}

/// CAN frame transmit acknowledgement.
#[repr(C, packed(8))]
pub struct SilKit_CanFrameTransmitEvent {
    /// Struct version header.
    pub struct_header: SilKit_StructHeader,
    /// User context.
    pub user_context: *mut c_void,
    /// Timestamp (ns).
    pub timestamp: SilKit_NanosecondsTime,
    /// Transmit status.
    pub status: SilKit_CanTransmitStatus,
    /// CAN id reference.
    pub can_id: u32,
}

// ---------------------------------------------------------------------------
// Ethernet frame
// ---------------------------------------------------------------------------

/// SIL Kit raw Ethernet frame.
#[repr(C, packed(8))]
pub struct SilKit_EthernetFrame {
    /// Struct version header.
    pub struct_header: SilKit_StructHeader,
    /// Raw Ethernet frame bytes (including MAC headers).
    pub raw: SilKit_ByteVector,
}

/// Incoming Ethernet frame event.
#[repr(C, packed(8))]
pub struct SilKit_EthernetFrameEvent {
    /// Struct version header.
    pub struct_header: SilKit_StructHeader,
    /// Reception timestamp (ns).
    pub timestamp: SilKit_NanosecondsTime,
    /// Pointer to the Ethernet frame.
    pub ethernet_frame: *const SilKit_EthernetFrame,
    /// Direction (TX/RX).
    pub direction: SilKit_Direction,
    /// User context.
    pub user_context: *mut c_void,
}

// ---------------------------------------------------------------------------
// LIN types and frame
// ---------------------------------------------------------------------------

/// LIN identifier (0‑63).
pub type SilKit_LinId = u8;

/// LIN checksum model.
pub type SilKit_LinChecksumModel = i32;

/// LIN data length.
pub type SilKit_LinDataLength = u8;

/// LIN frame response type.
pub type SilKit_LinFrameResponseType = i32;

/// LIN frame status.
pub type SilKit_LinFrameStatus = i32;

/// LIN controller mode.
pub type SilKit_LinControllerMode = i32;

/// LIN frame response mode.
pub type SilKit_LinFrameResponseMode = i32;

/// LIN baud rate.
pub type SilKit_LinBaudRate = u32;

/// LIN controller mode: Master.
pub const SK_LIN_MODE_MASTER: SilKit_LinControllerMode = 1;

/// LIN checksum model: Enhanced.
pub const SK_LIN_CHECKSUM_ENHANCED: SilKit_LinChecksumModel = 1;

/// LIN frame response type: `MasterResponse`.
pub const SK_LIN_FRAME_RESPONSE_MASTER: SilKit_LinFrameResponseType = 0;

/// SIL Kit LIN frame.
#[repr(C, packed(8))]
pub struct SilKit_LinFrame {
    /// Struct version header.
    pub struct_header: SilKit_StructHeader,
    /// LIN identifier (0‑63).
    pub id: SilKit_LinId,
    /// Checksum model.
    pub checksum_model: SilKit_LinChecksumModel,
    /// Data length (≤ 8).
    pub data_length: SilKit_LinDataLength,
    /// Payload (max 8 bytes).
    pub data: [u8; 8],
}

/// Incoming LIN frame status event.
#[repr(C, packed(8))]
pub struct SilKit_LinFrameStatusEvent {
    /// Struct version header.
    pub struct_header: SilKit_StructHeader,
    /// Timestamp (ns).
    pub timestamp: SilKit_NanosecondsTime,
    /// Pointer to the LIN frame.
    pub frame: *const SilKit_LinFrame,
    /// Frame status.
    pub status: SilKit_LinFrameStatus,
}

/// LIN frame response configuration.
#[repr(C, packed(8))]
pub struct SilKit_LinFrameResponse {
    /// Struct version header.
    pub struct_header: SilKit_StructHeader,
    /// The frame (provides LIN ID, data if TX).
    pub frame: *mut SilKit_LinFrame,
    /// Response mode (Unused / Rx / `TxUnconditional`).
    pub response_mode: SilKit_LinFrameResponseMode,
}

/// LIN controller configuration.
#[repr(C, packed(8))]
pub struct SilKit_LinControllerConfig {
    /// Struct version header.
    pub struct_header: SilKit_StructHeader,
    /// Master or Slave mode.
    pub controller_mode: SilKit_LinControllerMode,
    /// Operational baud rate.
    pub baud_rate: SilKit_LinBaudRate,
    /// Number of frame response entries.
    pub num_frame_responses: usize,
    /// Pointer to frame response array.
    pub frame_responses: *const SilKit_LinFrameResponse,
}

// ---------------------------------------------------------------------------
// FlexRay types and frame
// ---------------------------------------------------------------------------

/// `FlexRay` channel identifier.
pub type SilKit_FlexrayChannel = u8;

/// `FlexRay` micro‑tick.
pub type SilKit_FlexrayMicroTick = u32;

/// `FlexRay` clock period.
pub type SilKit_FlexrayClockPeriod = u8;

/// `FlexRay` transmission mode.
pub type SilKit_FlexrayTransmissionMode = u8;

/// `FlexRay` CHI command.
pub type SilKit_FlexrayChiCommand = u8;

/// Boolean used by SIL Kit structs.
pub type SilKit_Bool = u8;

/// `FlexRay` channel A.
pub const SK_FLEXRAY_CHANNEL_A: SilKit_FlexrayChannel = 0x01;

/// `FlexRay` `CHI_CMD RUN`.
pub const SK_FLEXRAY_CHI_CMD_RUN: SilKit_FlexrayChiCommand = 0x01;

/// `FlexRay` `CHI_CMD HALT`.
pub const SK_FLEXRAY_CHI_CMD_HALT: SilKit_FlexrayChiCommand = 0x06;

/// `FlexRay` cluster parameters.
#[repr(C, packed(8))]
#[allow(missing_docs)]
pub struct SilKit_FlexrayClusterParameters {
    /// Struct version header.
    pub struct_header: SilKit_StructHeader,
    pub g_coldstart_attempts: u8,
    pub g_cycle_count_max: u8,
    pub gd_action_point_offset: u16,
    pub gd_dynamic_slot_idle_phase: u16,
    pub gd_mini_slot: u16,
    pub gd_mini_slot_action_point_offset: u16,
    pub gd_static_slot: u16,
    pub gd_symbol_window: u16,
    pub gd_symbol_window_action_point_offset: u16,
    pub gd_tss_transmitter: u16,
    pub gd_wakeup_tx_active: u16,
    pub gd_wakeup_tx_idle: u16,
    pub g_listen_noise: u8,
    pub g_macro_per_cycle: u16,
    pub g_max_without_clock_correction_fatal: u8,
    pub g_max_without_clock_correction_passive: u8,
    pub g_number_of_mini_slots: u16,
    pub g_number_of_static_slots: u16,
    pub g_payload_length_static: u16,
    pub g_sync_frame_id_count_max: u8,
}

/// `FlexRay` node parameters.
#[repr(C, packed(8))]
#[allow(missing_docs)]
pub struct SilKit_FlexrayNodeParameters {
    /// Struct version header.
    pub struct_header: SilKit_StructHeader,
    pub p_allow_halt_due_to_clock: u8,
    pub p_allow_passive_to_active: u8,
    pub p_channels: SilKit_FlexrayChannel,
    pub p_cluster_drift_damping: u8,
    pub pd_accepted_startup_range: SilKit_FlexrayMicroTick,
    pub pd_listen_timeout: SilKit_FlexrayMicroTick,
    pub p_key_slot_id: u16,
    pub p_key_slot_only_enabled: u8,
    pub p_key_slot_used_for_startup: u8,
    pub p_key_slot_used_for_sync: u8,
    pub p_latest_tx: u16,
    pub p_macro_initial_offset_a: u8,
    pub p_macro_initial_offset_b: u8,
    pub p_micro_initial_offset_a: SilKit_FlexrayMicroTick,
    pub p_micro_initial_offset_b: SilKit_FlexrayMicroTick,
    pub p_micro_per_cycle: SilKit_FlexrayMicroTick,
    pub p_offset_correction_out: SilKit_FlexrayMicroTick,
    pub p_offset_correction_start: u16,
    pub p_rate_correction_out: SilKit_FlexrayMicroTick,
    pub p_wakeup_channel: SilKit_FlexrayChannel,
    pub p_wakeup_pattern: u8,
    pub pd_microtick: SilKit_FlexrayClockPeriod,
    pub p_samples_per_microtick: u8,
    pub p_second_key_slot_id: u16,
    pub p_two_key_slot_mode: u8,
}

/// `FlexRay` TX buffer configuration.
#[repr(C, packed(8))]
pub struct SilKit_FlexrayTxBufferConfig {
    /// Struct version header.
    pub struct_header: SilKit_StructHeader,
    /// Channel(s) (A, B, AB).
    pub channels: SilKit_FlexrayChannel,
    /// Slot ID (1‑2047).
    pub slot_id: u16,
    /// Base offset for cycle multiplexing (0‑63).
    pub offset: u8,
    /// Repetition (1,2,4,8,16,32,64).
    pub repetition: u8,
    /// Payload preamble indicator.
    pub has_payload_preamble_indicator: SilKit_Bool,
    /// Header CRC (11 bits).
    pub header_crc: u16,
    /// `SingleShot` (0) or Continuous (1).
    pub transmission_mode: SilKit_FlexrayTransmissionMode,
}

/// `FlexRay` controller configuration.
#[repr(C, packed(8))]
pub struct SilKit_FlexrayControllerConfig {
    /// Struct version header.
    pub struct_header: SilKit_StructHeader,
    /// Cluster parameters.
    pub cluster_params: *const SilKit_FlexrayClusterParameters,
    /// Node parameters.
    pub node_params: *const SilKit_FlexrayNodeParameters,
    /// Number of TX buffer configs.
    pub num_buffer_configs: u32,
    /// TX buffer config array.
    pub buffer_configs: *const SilKit_FlexrayTxBufferConfig,
}

/// `FlexRay` TX buffer update.
#[repr(C, packed(8))]
pub struct SilKit_FlexrayTxBufferUpdate {
    /// Struct version header.
    pub struct_header: SilKit_StructHeader,
    /// TX buffer index.
    pub tx_buffer_index: u16,
    /// Payload data valid flag.
    pub payload_data_valid: SilKit_Bool,
    /// Payload bytes.
    pub payload: SilKit_ByteVector,
}

/// `FlexRay` frame header.
#[repr(C, packed(8))]
pub struct SilKit_FlexrayHeader {
    /// Struct version header.
    pub struct_header: SilKit_StructHeader,
    /// Flags bit‑map.
    pub flags: u8,
    /// Slot ID (1‑2047).
    pub frame_id: u16,
    /// Payload length in 16‑bit words (0‑127).
    pub payload_length: u8,
    /// Header CRC (11 bits).
    pub header_crc: u16,
    /// Cycle count (0‑63).
    pub cycle_count: u8,
}

/// `FlexRay` frame.
#[repr(C, packed(8))]
pub struct SilKit_FlexrayFrame {
    /// Struct version header.
    pub struct_header: SilKit_StructHeader,
    /// Frame header.
    pub header: *const SilKit_FlexrayHeader,
    /// Payload data.
    pub payload: SilKit_ByteVector,
}

/// Incoming `FlexRay` frame event.
#[repr(C, packed(8))]
pub struct SilKit_FlexrayFrameEvent {
    /// Struct version header.
    pub struct_header: SilKit_StructHeader,
    /// Timestamp (ns).
    pub timestamp: SilKit_NanosecondsTime,
    /// Channel (A or B).
    pub channel: SilKit_FlexrayChannel,
    /// Pointer to the `FlexRay` frame.
    pub frame: *const SilKit_FlexrayFrame,
}

// ---------------------------------------------------------------------------
// Lifecycle configuration
// ---------------------------------------------------------------------------

/// Lifecycle configuration passed to `SilKit_LifecycleService_Create`.
#[repr(C, packed(8))]
pub struct SilKit_LifecycleConfiguration {
    /// Struct version header.
    pub struct_header: SilKit_StructHeader,
    /// Operation mode.
    pub operation_mode: SilKit_OperationMode,
    /// Padding.
    pub pad0: u32,
}

// ---------------------------------------------------------------------------
// Callback types
// ---------------------------------------------------------------------------

/// Lifecycle `CommunicationReady` handler.
pub type CommunicationReadyHandler =
    unsafe extern "C" fn(context: *mut c_void, lifecycle: *mut SilKit_LifecycleService);

/// Lifecycle `Stop` handler.
pub type StopHandler =
    unsafe extern "C" fn(context: *mut c_void, lifecycle: *mut SilKit_LifecycleService);

/// Lifecycle `Shutdown` handler.
pub type ShutdownHandler =
    unsafe extern "C" fn(context: *mut c_void, lifecycle: *mut SilKit_LifecycleService);

/// Lifecycle `Abort` handler.
pub type AbortHandler = unsafe extern "C" fn(
    context: *mut c_void,
    lifecycle: *mut SilKit_LifecycleService,
    last_state: SilKit_ParticipantState,
);

/// `TimeSyncService` simulation step handler.
pub type SimulationStepHandler = unsafe extern "C" fn(
    context: *mut c_void,
    time_sync: *mut SilKit_TimeSyncService,
    now: SilKit_NanosecondsTime,
    duration: SilKit_NanosecondsTime,
);

/// CAN frame receive handler.
pub type CanFrameHandler = unsafe extern "C" fn(
    context: *mut c_void,
    controller: *mut SilKit_CanController,
    frame_event: *const SilKit_CanFrameEvent,
);

/// CAN frame transmit acknowledge handler.
pub type CanFrameTransmitHandler = unsafe extern "C" fn(
    context: *mut c_void,
    controller: *mut SilKit_CanController,
    transmit_event: *const SilKit_CanFrameTransmitEvent,
);

/// Ethernet frame receive handler.
pub type EthernetFrameHandler = unsafe extern "C" fn(
    context: *mut c_void,
    controller: *mut SilKit_EthernetController,
    frame_event: *const SilKit_EthernetFrameEvent,
);

/// LIN frame status handler.
pub type LinFrameStatusHandler = unsafe extern "C" fn(
    context: *mut c_void,
    controller: *mut SilKit_LinController,
    frame_status_event: *const SilKit_LinFrameStatusEvent,
);

/// `FlexRay` frame receive handler.
pub type FlexrayFrameHandler = unsafe extern "C" fn(
    context: *mut c_void,
    controller: *mut SilKit_FlexrayController,
    frame_event: *const SilKit_FlexrayFrameEvent,
);

// ---------------------------------------------------------------------------
// Dynamic function table
// ---------------------------------------------------------------------------

/// Dynamically loaded SIL Kit C API function pointers.
///
/// Loaded at runtime from the SIL Kit shared library (e.g.
/// `libSilKit.so`, `SilKit.dll`, `libSilKit.dylib`).
///
/// Field documentation mirrors the SIL Kit C API headers verbatim.
#[allow(missing_docs)]
pub struct SilKitApi {
    _library: libloading::Library,

    // -- Participant ---------------------------------------------------------
    pub participant_create: unsafe extern "C" fn(
        out: *mut *mut SilKit_Participant,
        config: *mut SilKit_ParticipantConfiguration,
        name: *const c_char,
        registry_uri: *const c_char,
    ) -> SilKit_ReturnCode,

    pub participant_destroy:
        unsafe extern "C" fn(participant: *mut SilKit_Participant) -> SilKit_ReturnCode,

    // -- Participant configuration -------------------------------------------
    pub participant_configuration_from_string: unsafe extern "C" fn(
        out: *mut *mut SilKit_ParticipantConfiguration,
        json: *const c_char,
    ) -> SilKit_ReturnCode,

    pub participant_configuration_destroy:
        unsafe extern "C" fn(config: *mut SilKit_ParticipantConfiguration) -> SilKit_ReturnCode,

    // -- Lifecycle service ---------------------------------------------------
    pub lifecycle_service_create: unsafe extern "C" fn(
        out: *mut *mut SilKit_LifecycleService,
        participant: *mut SilKit_Participant,
        config: *const SilKit_LifecycleConfiguration,
    ) -> SilKit_ReturnCode,

    pub lifecycle_set_communication_ready_handler: unsafe extern "C" fn(
        lifecycle: *mut SilKit_LifecycleService,
        context: *mut c_void,
        handler: CommunicationReadyHandler,
    ) -> SilKit_ReturnCode,

    pub lifecycle_set_stop_handler: unsafe extern "C" fn(
        lifecycle: *mut SilKit_LifecycleService,
        context: *mut c_void,
        handler: StopHandler,
    ) -> SilKit_ReturnCode,

    pub lifecycle_set_shutdown_handler: unsafe extern "C" fn(
        lifecycle: *mut SilKit_LifecycleService,
        context: *mut c_void,
        handler: ShutdownHandler,
    ) -> SilKit_ReturnCode,

    pub lifecycle_start:
        unsafe extern "C" fn(lifecycle: *mut SilKit_LifecycleService) -> SilKit_ReturnCode,

    pub lifecycle_wait_for_complete: unsafe extern "C" fn(
        lifecycle: *mut SilKit_LifecycleService,
        out_state: *mut SilKit_ParticipantState,
    ) -> SilKit_ReturnCode,

    pub lifecycle_stop: unsafe extern "C" fn(
        lifecycle: *mut SilKit_LifecycleService,
        reason: *const c_char,
    ) -> SilKit_ReturnCode,

    // -- Time sync service ---------------------------------------------------
    pub time_sync_create: unsafe extern "C" fn(
        out: *mut *mut SilKit_TimeSyncService,
        lifecycle: *mut SilKit_LifecycleService,
    ) -> SilKit_ReturnCode,

    pub time_sync_set_step_handler: unsafe extern "C" fn(
        time_sync: *mut SilKit_TimeSyncService,
        context: *mut c_void,
        handler: SimulationStepHandler,
        initial_step_size: SilKit_NanosecondsTime,
    ) -> SilKit_ReturnCode,

    pub time_sync_now: unsafe extern "C" fn(
        time_sync: *mut SilKit_TimeSyncService,
        out: *mut SilKit_NanosecondsTime,
    ) -> SilKit_ReturnCode,

    // -- CAN controller ------------------------------------------------------
    pub can_controller_create: unsafe extern "C" fn(
        out: *mut *mut SilKit_CanController,
        participant: *mut SilKit_Participant,
        name: *const c_char,
        network: *const c_char,
    ) -> SilKit_ReturnCode,

    pub can_controller_start:
        unsafe extern "C" fn(controller: *mut SilKit_CanController) -> SilKit_ReturnCode,

    pub can_controller_stop:
        unsafe extern "C" fn(controller: *mut SilKit_CanController) -> SilKit_ReturnCode,

    pub can_controller_send_frame: unsafe extern "C" fn(
        controller: *mut SilKit_CanController,
        frame: *const SilKit_CanFrame,
        user_context: *mut c_void,
    ) -> SilKit_ReturnCode,

    pub can_controller_add_frame_handler: unsafe extern "C" fn(
        controller: *mut SilKit_CanController,
        context: *mut c_void,
        handler: CanFrameHandler,
        direction_mask: SilKit_Direction,
        out_handler_id: *mut SilKit_HandlerId,
    ) -> SilKit_ReturnCode,

    pub can_controller_add_frame_transmit_handler: unsafe extern "C" fn(
        controller: *mut SilKit_CanController,
        context: *mut c_void,
        handler: CanFrameTransmitHandler,
        status_mask: SilKit_CanTransmitStatus,
        out_handler_id: *mut SilKit_HandlerId,
    ) -> SilKit_ReturnCode,

    // -- Ethernet controller -------------------------------------------------
    pub ethernet_controller_create: unsafe extern "C" fn(
        out: *mut *mut SilKit_EthernetController,
        participant: *mut SilKit_Participant,
        name: *const c_char,
        network: *const c_char,
    ) -> SilKit_ReturnCode,

    pub ethernet_controller_activate:
        unsafe extern "C" fn(controller: *mut SilKit_EthernetController) -> SilKit_ReturnCode,

    pub ethernet_controller_deactivate:
        unsafe extern "C" fn(controller: *mut SilKit_EthernetController) -> SilKit_ReturnCode,

    pub ethernet_controller_send_frame: unsafe extern "C" fn(
        controller: *mut SilKit_EthernetController,
        frame: *mut SilKit_EthernetFrame,
        user_context: *mut c_void,
    ) -> SilKit_ReturnCode,

    pub ethernet_controller_add_frame_handler: unsafe extern "C" fn(
        controller: *mut SilKit_EthernetController,
        context: *mut c_void,
        handler: EthernetFrameHandler,
        direction_mask: SilKit_Direction,
        out_handler_id: *mut SilKit_HandlerId,
    ) -> SilKit_ReturnCode,

    // -- LIN controller ------------------------------------------------------
    pub lin_controller_create: unsafe extern "C" fn(
        out: *mut *mut SilKit_LinController,
        participant: *mut SilKit_Participant,
        name: *const c_char,
        network: *const c_char,
    ) -> SilKit_ReturnCode,

    pub lin_controller_init: unsafe extern "C" fn(
        controller: *mut SilKit_LinController,
        config: *const SilKit_LinControllerConfig,
    ) -> SilKit_ReturnCode,

    pub lin_controller_send_frame: unsafe extern "C" fn(
        controller: *mut SilKit_LinController,
        frame: *const SilKit_LinFrame,
        response_type: SilKit_LinFrameResponseType,
    ) -> SilKit_ReturnCode,

    pub lin_controller_add_frame_status_handler: unsafe extern "C" fn(
        controller: *mut SilKit_LinController,
        context: *mut c_void,
        handler: LinFrameStatusHandler,
        out_handler_id: *mut SilKit_HandlerId,
    ) -> SilKit_ReturnCode,

    // -- FlexRay controller --------------------------------------------------
    pub flexray_controller_create: unsafe extern "C" fn(
        out: *mut *mut SilKit_FlexrayController,
        participant: *mut SilKit_Participant,
        name: *const c_char,
        network: *const c_char,
    ) -> SilKit_ReturnCode,

    pub flexray_controller_configure: unsafe extern "C" fn(
        controller: *mut SilKit_FlexrayController,
        config: *const SilKit_FlexrayControllerConfig,
    ) -> SilKit_ReturnCode,

    pub flexray_controller_update_tx_buffer: unsafe extern "C" fn(
        controller: *mut SilKit_FlexrayController,
        update: *const SilKit_FlexrayTxBufferUpdate,
    ) -> SilKit_ReturnCode,

    pub flexray_controller_execute_cmd: unsafe extern "C" fn(
        controller: *mut SilKit_FlexrayController,
        cmd: SilKit_FlexrayChiCommand,
    ) -> SilKit_ReturnCode,

    pub flexray_controller_add_frame_handler: unsafe extern "C" fn(
        controller: *mut SilKit_FlexrayController,
        context: *mut c_void,
        handler: FlexrayFrameHandler,
        out_handler_id: *mut SilKit_HandlerId,
    ) -> SilKit_ReturnCode,
}

impl SilKitApi {
    /// Load the SIL Kit shared library and resolve all required symbols.
    ///
    /// # Errors
    ///
    /// Returns an error if the library cannot be found or a symbol is missing.
    ///
    /// # Safety
    ///
    /// Loading a shared library is inherently unsafe. The caller must ensure
    /// the library is a genuine SIL Kit build.
    #[allow(unsafe_code, clippy::too_many_lines)]
    pub fn load(lib_path: &str) -> Result<Self, libloading::Error> {
        // SAFETY: Loading a shared library is inherently unsafe.
        // We resolve well‑known SIL Kit C API symbols and trust the ABI.
        unsafe {
            let lib = libloading::Library::new(lib_path)?;

            // Helper: resolve a symbol and transmute to the expected fn‑pointer type.
            // `libloading::Library::get` returns `Symbol<T>` which derefs to `T`.
            macro_rules! load_fn {
                ($lib:expr, $sym:expr, $ty:ty) => {{
                    let f: libloading::Symbol<'_, $ty> = $lib.get($sym)?;
                    *f
                }};
            }

            let api = Self {
                participant_create: load_fn!(
                    lib,
                    b"SilKit_Participant_Create\0",
                    unsafe extern "C" fn(
                        *mut *mut SilKit_Participant,
                        *mut SilKit_ParticipantConfiguration,
                        *const c_char,
                        *const c_char,
                    ) -> SilKit_ReturnCode
                ),
                participant_destroy: load_fn!(
                    lib,
                    b"SilKit_Participant_Destroy\0",
                    unsafe extern "C" fn(*mut SilKit_Participant) -> SilKit_ReturnCode
                ),
                participant_configuration_from_string: load_fn!(
                    lib,
                    b"SilKit_ParticipantConfiguration_FromString\0",
                    unsafe extern "C" fn(
                        *mut *mut SilKit_ParticipantConfiguration,
                        *const c_char,
                    ) -> SilKit_ReturnCode
                ),
                participant_configuration_destroy: load_fn!(
                    lib,
                    b"SilKit_ParticipantConfiguration_Destroy\0",
                    unsafe extern "C" fn(*mut SilKit_ParticipantConfiguration) -> SilKit_ReturnCode
                ),
                lifecycle_service_create: load_fn!(
                    lib,
                    b"SilKit_LifecycleService_Create\0",
                    unsafe extern "C" fn(
                        *mut *mut SilKit_LifecycleService,
                        *mut SilKit_Participant,
                        *const SilKit_LifecycleConfiguration,
                    ) -> SilKit_ReturnCode
                ),
                lifecycle_set_communication_ready_handler: load_fn!(
                    lib,
                    b"SilKit_LifecycleService_SetCommunicationReadyHandler\0",
                    unsafe extern "C" fn(
                        *mut SilKit_LifecycleService,
                        *mut c_void,
                        CommunicationReadyHandler,
                    ) -> SilKit_ReturnCode
                ),
                lifecycle_set_stop_handler: load_fn!(
                    lib,
                    b"SilKit_LifecycleService_SetStopHandler\0",
                    unsafe extern "C" fn(
                        *mut SilKit_LifecycleService,
                        *mut c_void,
                        StopHandler,
                    ) -> SilKit_ReturnCode
                ),
                lifecycle_set_shutdown_handler: load_fn!(
                    lib,
                    b"SilKit_LifecycleService_SetShutdownHandler\0",
                    unsafe extern "C" fn(
                        *mut SilKit_LifecycleService,
                        *mut c_void,
                        ShutdownHandler,
                    ) -> SilKit_ReturnCode
                ),
                lifecycle_start: load_fn!(
                    lib,
                    b"SilKit_LifecycleService_StartLifecycle\0",
                    unsafe extern "C" fn(*mut SilKit_LifecycleService) -> SilKit_ReturnCode
                ),
                lifecycle_wait_for_complete: load_fn!(
                    lib,
                    b"SilKit_LifecycleService_WaitForLifecycleToComplete\0",
                    unsafe extern "C" fn(
                        *mut SilKit_LifecycleService,
                        *mut SilKit_ParticipantState,
                    ) -> SilKit_ReturnCode
                ),
                lifecycle_stop: load_fn!(
                    lib,
                    b"SilKit_LifecycleService_Stop\0",
                    unsafe extern "C" fn(
                        *mut SilKit_LifecycleService,
                        *const c_char,
                    ) -> SilKit_ReturnCode
                ),
                time_sync_create: load_fn!(
                    lib,
                    b"SilKit_TimeSyncService_Create\0",
                    unsafe extern "C" fn(
                        *mut *mut SilKit_TimeSyncService,
                        *mut SilKit_LifecycleService,
                    ) -> SilKit_ReturnCode
                ),
                time_sync_set_step_handler: load_fn!(
                    lib,
                    b"SilKit_TimeSyncService_SetSimulationStepHandler\0",
                    unsafe extern "C" fn(
                        *mut SilKit_TimeSyncService,
                        *mut c_void,
                        SimulationStepHandler,
                        SilKit_NanosecondsTime,
                    ) -> SilKit_ReturnCode
                ),
                time_sync_now: load_fn!(
                    lib,
                    b"SilKit_TimeSyncService_Now\0",
                    unsafe extern "C" fn(
                        *mut SilKit_TimeSyncService,
                        *mut SilKit_NanosecondsTime,
                    ) -> SilKit_ReturnCode
                ),
                can_controller_create: load_fn!(
                    lib,
                    b"SilKit_CanController_Create\0",
                    unsafe extern "C" fn(
                        *mut *mut SilKit_CanController,
                        *mut SilKit_Participant,
                        *const c_char,
                        *const c_char,
                    ) -> SilKit_ReturnCode
                ),
                can_controller_start: load_fn!(
                    lib,
                    b"SilKit_CanController_Start\0",
                    unsafe extern "C" fn(*mut SilKit_CanController) -> SilKit_ReturnCode
                ),
                can_controller_stop: load_fn!(
                    lib,
                    b"SilKit_CanController_Stop\0",
                    unsafe extern "C" fn(*mut SilKit_CanController) -> SilKit_ReturnCode
                ),
                can_controller_send_frame: load_fn!(
                    lib,
                    b"SilKit_CanController_SendFrame\0",
                    unsafe extern "C" fn(
                        *mut SilKit_CanController,
                        *const SilKit_CanFrame,
                        *mut c_void,
                    ) -> SilKit_ReturnCode
                ),
                can_controller_add_frame_handler: load_fn!(
                    lib,
                    b"SilKit_CanController_AddFrameHandler\0",
                    unsafe extern "C" fn(
                        *mut SilKit_CanController,
                        *mut c_void,
                        CanFrameHandler,
                        SilKit_Direction,
                        *mut SilKit_HandlerId,
                    ) -> SilKit_ReturnCode
                ),
                can_controller_add_frame_transmit_handler: load_fn!(
                    lib,
                    b"SilKit_CanController_AddFrameTransmitHandler\0",
                    unsafe extern "C" fn(
                        *mut SilKit_CanController,
                        *mut c_void,
                        CanFrameTransmitHandler,
                        SilKit_CanTransmitStatus,
                        *mut SilKit_HandlerId,
                    ) -> SilKit_ReturnCode
                ),

                // -- Ethernet
                ethernet_controller_create: load_fn!(
                    lib,
                    b"SilKit_EthernetController_Create\0",
                    unsafe extern "C" fn(
                        *mut *mut SilKit_EthernetController,
                        *mut SilKit_Participant,
                        *const c_char,
                        *const c_char,
                    ) -> SilKit_ReturnCode
                ),
                ethernet_controller_activate: load_fn!(
                    lib,
                    b"SilKit_EthernetController_Activate\0",
                    unsafe extern "C" fn(*mut SilKit_EthernetController) -> SilKit_ReturnCode
                ),
                ethernet_controller_deactivate: load_fn!(
                    lib,
                    b"SilKit_EthernetController_Deactivate\0",
                    unsafe extern "C" fn(*mut SilKit_EthernetController) -> SilKit_ReturnCode
                ),
                ethernet_controller_send_frame: load_fn!(
                    lib,
                    b"SilKit_EthernetController_SendFrame\0",
                    unsafe extern "C" fn(
                        *mut SilKit_EthernetController,
                        *mut SilKit_EthernetFrame,
                        *mut c_void,
                    ) -> SilKit_ReturnCode
                ),
                ethernet_controller_add_frame_handler: load_fn!(
                    lib,
                    b"SilKit_EthernetController_AddFrameHandler\0",
                    unsafe extern "C" fn(
                        *mut SilKit_EthernetController,
                        *mut c_void,
                        EthernetFrameHandler,
                        SilKit_Direction,
                        *mut SilKit_HandlerId,
                    ) -> SilKit_ReturnCode
                ),

                // -- LIN
                lin_controller_create: load_fn!(
                    lib,
                    b"SilKit_LinController_Create\0",
                    unsafe extern "C" fn(
                        *mut *mut SilKit_LinController,
                        *mut SilKit_Participant,
                        *const c_char,
                        *const c_char,
                    ) -> SilKit_ReturnCode
                ),
                lin_controller_init: load_fn!(
                    lib,
                    b"SilKit_LinController_Init\0",
                    unsafe extern "C" fn(
                        *mut SilKit_LinController,
                        *const SilKit_LinControllerConfig,
                    ) -> SilKit_ReturnCode
                ),
                lin_controller_send_frame: load_fn!(
                    lib,
                    b"SilKit_LinController_SendFrame\0",
                    unsafe extern "C" fn(
                        *mut SilKit_LinController,
                        *const SilKit_LinFrame,
                        SilKit_LinFrameResponseType,
                    ) -> SilKit_ReturnCode
                ),
                lin_controller_add_frame_status_handler: load_fn!(
                    lib,
                    b"SilKit_LinController_AddFrameStatusHandler\0",
                    unsafe extern "C" fn(
                        *mut SilKit_LinController,
                        *mut c_void,
                        LinFrameStatusHandler,
                        *mut SilKit_HandlerId,
                    ) -> SilKit_ReturnCode
                ),

                // -- FlexRay
                flexray_controller_create: load_fn!(
                    lib,
                    b"SilKit_FlexrayController_Create\0",
                    unsafe extern "C" fn(
                        *mut *mut SilKit_FlexrayController,
                        *mut SilKit_Participant,
                        *const c_char,
                        *const c_char,
                    ) -> SilKit_ReturnCode
                ),
                flexray_controller_configure: load_fn!(
                    lib,
                    b"SilKit_FlexrayController_Configure\0",
                    unsafe extern "C" fn(
                        *mut SilKit_FlexrayController,
                        *const SilKit_FlexrayControllerConfig,
                    ) -> SilKit_ReturnCode
                ),
                flexray_controller_update_tx_buffer: load_fn!(
                    lib,
                    b"SilKit_FlexrayController_UpdateTxBuffer\0",
                    unsafe extern "C" fn(
                        *mut SilKit_FlexrayController,
                        *const SilKit_FlexrayTxBufferUpdate,
                    ) -> SilKit_ReturnCode
                ),
                flexray_controller_execute_cmd: load_fn!(
                    lib,
                    b"SilKit_FlexrayController_ExecuteCmd\0",
                    unsafe extern "C" fn(
                        *mut SilKit_FlexrayController,
                        SilKit_FlexrayChiCommand,
                    ) -> SilKit_ReturnCode
                ),
                flexray_controller_add_frame_handler: load_fn!(
                    lib,
                    b"SilKit_FlexrayController_AddFrameHandler\0",
                    unsafe extern "C" fn(
                        *mut SilKit_FlexrayController,
                        *mut c_void,
                        FlexrayFrameHandler,
                        *mut SilKit_HandlerId,
                    ) -> SilKit_ReturnCode
                ),

                _library: lib,
            };
            Ok(api)
        }
    }
}
