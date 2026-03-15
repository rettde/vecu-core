//! SIL Kit runtime adapter – connects the vECU runtime to a SIL Kit simulation.
//!
//! The adapter registers as a **SIL Kit participant**, creates a lifecycle
//! service with virtual time synchronisation, and maps CAN frame I/O
//! between the SIL Kit bus and the vECU runtime via [`OpenSutApi`].
//!
//! Frame routing is delegated to [`SilKitBus`](crate::bus::SilKitBus),
//! which implements [`OpenSutApi`] and is set on the [`Runtime`] before
//! the lifecycle starts.

use std::ffi::{c_void, CString};
use std::ptr;
use std::sync::atomic::{AtomicBool, Ordering};

use vecu_abi::{BusType, VecuFrame};
use vecu_runtime::{Runtime, RuntimeAdapter, RuntimeError};

use crate::bus::{new_rx_buffer, SharedRxBuffer, SilKitBus};
use crate::sys::{
    self, SilKitApi, SilKit_CanController, SilKit_CanFrameEvent, SilKit_EthernetController,
    SilKit_EthernetFrameEvent, SilKit_FlexrayController, SilKit_FlexrayFrameEvent,
    SilKit_LifecycleConfiguration, SilKit_LifecycleService, SilKit_LinController,
    SilKit_LinFrameStatusEvent, SilKit_NanosecondsTime, SilKit_Participant,
    SilKit_ParticipantConfiguration, SilKit_StructHeader, SilKit_TimeSyncService, SK_OK,
    SK_OPERATION_MODE_COORDINATED,
};

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

/// Configuration for the SIL Kit adapter.
#[derive(Debug, Clone)]
pub struct SilKitConfig {
    /// Path to the SIL Kit shared library.
    pub library_path: String,
    /// SIL Kit registry URI (e.g. `silkit://localhost:8500`).
    pub registry_uri: String,
    /// Participant name within the SIL Kit simulation.
    pub participant_name: String,
    /// CAN network name for frame exchange.
    pub can_network: String,
    /// CAN controller name.
    pub can_controller_name: String,
    /// Ethernet network + controller name (optional).
    pub eth_network: Option<String>,
    /// Ethernet controller name (optional).
    pub eth_controller_name: Option<String>,
    /// LIN network name (optional).
    pub lin_network: Option<String>,
    /// LIN controller name (optional).
    pub lin_controller_name: Option<String>,
    /// `FlexRay` network name (optional).
    pub flexray_network: Option<String>,
    /// `FlexRay` controller name (optional).
    pub flexray_controller_name: Option<String>,
    /// Simulation step size in nanoseconds.
    pub step_size_ns: u64,
    /// Participant configuration JSON (empty = default).
    pub participant_config_json: String,
    /// Whether to use coordinated (true) or autonomous (false) lifecycle.
    pub coordinated: bool,
}

impl Default for SilKitConfig {
    fn default() -> Self {
        Self {
            library_path: default_library_path().to_string(),
            registry_uri: "silkit://localhost:8500".to_string(),
            participant_name: "vECU".to_string(),
            can_network: "CAN1".to_string(),
            can_controller_name: "CAN1".to_string(),
            eth_network: None,
            eth_controller_name: None,
            lin_network: None,
            lin_controller_name: None,
            flexray_network: None,
            flexray_controller_name: None,
            step_size_ns: 1_000_000, // 1 ms
            participant_config_json: String::new(),
            coordinated: true,
        }
    }
}

/// Return the platform‑default SIL Kit library file name.
fn default_library_path() -> &'static str {
    if cfg!(target_os = "windows") {
        "SilKit"
    } else if cfg!(target_os = "macos") {
        "libSilKit.dylib"
    } else {
        "libSilKit.so"
    }
}

// ---------------------------------------------------------------------------
// Context passed into SIL Kit callbacks
// ---------------------------------------------------------------------------

/// Shared context accessible from all SIL Kit callbacks.
///
/// # Safety
///
/// This is passed as `*mut c_void` to SIL Kit. The adapter guarantees the
/// pointer is valid for the entire lifetime of the SIL Kit participant.
struct CallbackContext {
    /// Mutable pointer to the vECU runtime – valid between init and shutdown.
    runtime: *mut Runtime,
    /// Flag: has communication been initialised?
    comm_ready: AtomicBool,
    /// Flag: has the simulation been stopped?
    stopped: AtomicBool,
    /// Shared inbound frame buffer (filled by CAN callback, drained by
    /// [`SilKitBus::recv_inbound`]).
    rx_buffer: SharedRxBuffer,
}

// SAFETY: The context is only accessed from the single SIL Kit callback
// thread (no parallel invocations within a coordinated simulation).
#[allow(unsafe_code)]
unsafe impl Send for CallbackContext {}
#[allow(unsafe_code)]
unsafe impl Sync for CallbackContext {}

// ---------------------------------------------------------------------------
// SilKitAdapter
// ---------------------------------------------------------------------------

/// Runtime adapter that drives the vECU simulation via Vector SIL Kit.
pub struct SilKitAdapter {
    config: SilKitConfig,
}

impl SilKitAdapter {
    /// Create a new SIL Kit adapter with the given configuration.
    #[must_use]
    pub fn new(config: SilKitConfig) -> Self {
        Self { config }
    }
}

impl RuntimeAdapter for SilKitAdapter {
    #[allow(unsafe_code, clippy::too_many_lines)]
    fn run(&mut self, runtime: &mut Runtime) -> Result<(), RuntimeError> {
        // 1. Load SIL Kit library.
        let api = SilKitApi::load(&self.config.library_path).map_err(|_e| {
            RuntimeError::Abi(vecu_abi::AbiError::ModuleError(
                vecu_abi::status::MODULE_ERROR,
            ))
        })?;

        // SIL Kit return code checking via `check_silkit` (module-level fn).

        // 2. Create participant configuration.
        let config_json = if self.config.participant_config_json.is_empty() {
            CString::new("{}").unwrap()
        } else {
            CString::new(self.config.participant_config_json.as_str()).unwrap()
        };
        let mut part_config: *mut SilKit_ParticipantConfiguration = ptr::null_mut();

        unsafe {
            check_silkit(
                (api.participant_configuration_from_string)(&mut part_config, config_json.as_ptr()),
                "ParticipantConfiguration_FromString",
            )?;
        }

        // 3. Create participant.
        let part_name = CString::new(self.config.participant_name.as_str()).unwrap();
        let registry_uri = CString::new(self.config.registry_uri.as_str()).unwrap();
        let mut participant: *mut SilKit_Participant = ptr::null_mut();

        unsafe {
            let rc = (api.participant_create)(
                &mut participant,
                part_config,
                part_name.as_ptr(),
                registry_uri.as_ptr(),
            );
            check_silkit(rc, "Participant_Create")?;
        }

        tracing::info!(
            name = %self.config.participant_name,
            registry = %self.config.registry_uri,
            "SIL Kit participant created"
        );

        // 4. Create lifecycle service.
        let op_mode = if self.config.coordinated {
            SK_OPERATION_MODE_COORDINATED
        } else {
            sys::SK_OPERATION_MODE_AUTONOMOUS
        };
        let lifecycle_cfg = SilKit_LifecycleConfiguration {
            struct_header: SilKit_StructHeader::v1(),
            operation_mode: op_mode,
            pad0: 0,
        };
        let mut lifecycle: *mut SilKit_LifecycleService = ptr::null_mut();

        unsafe {
            check_silkit(
                (api.lifecycle_service_create)(&mut lifecycle, participant, &lifecycle_cfg),
                "LifecycleService_Create",
            )?;
        }

        // 5. Create time sync service.
        let mut time_sync: *mut SilKit_TimeSyncService = ptr::null_mut();
        unsafe {
            check_silkit(
                (api.time_sync_create)(&mut time_sync, lifecycle),
                "TimeSyncService_Create",
            )?;
        }

        // 6. Create CAN controller.
        let can_name = CString::new(self.config.can_controller_name.as_str()).unwrap();
        let can_network = CString::new(self.config.can_network.as_str()).unwrap();
        let mut can_ctrl: *mut SilKit_CanController = ptr::null_mut();

        unsafe {
            check_silkit(
                (api.can_controller_create)(
                    &mut can_ctrl,
                    participant,
                    can_name.as_ptr(),
                    can_network.as_ptr(),
                ),
                "CanController_Create",
            )?;
        }

        // 6b. Optionally create Ethernet controller.
        let (opt_eth, _eth_strings) = create_eth_controller(
            &api,
            participant,
            self.config.eth_network.as_deref(),
            self.config.eth_controller_name.as_deref(),
        )?;

        // 6c. Optionally create LIN controller.
        let (opt_lin, _lin_strings) = create_lin_controller(
            &api,
            participant,
            self.config.lin_network.as_deref(),
            self.config.lin_controller_name.as_deref(),
        )?;

        // 6d. Optionally create `FlexRay` controller.
        let (opt_fr, _fr_strings) = create_flexray_controller(
            &api,
            participant,
            self.config.flexray_network.as_deref(),
            self.config.flexray_controller_name.as_deref(),
        )?;

        // 7. Create shared RX buffer and OpenSutApi bus.
        let rx_buffer = new_rx_buffer();

        // SAFETY: `api` and all controller pointers live on the stack of
        // `run()`, which blocks until the simulation completes. The bus is
        // dropped before they go out of scope.
        #[allow(unsafe_code)]
        let silkit_bus = unsafe {
            SilKitBus::new(
                rx_buffer.clone(),
                ptr::addr_of!(api),
                Some(can_ctrl),
                opt_eth,
                opt_lin,
                opt_fr,
            )
        };
        runtime.set_appl_bus(Box::new(silkit_bus));

        // 8. Build callback context.
        let mut ctx = Box::new(CallbackContext {
            runtime: runtime as *mut Runtime,
            comm_ready: AtomicBool::new(false),
            stopped: AtomicBool::new(false),
            rx_buffer,
        });
        let ctx_ptr: *mut c_void = (ctx.as_mut() as *mut CallbackContext).cast();

        // 9. Register lifecycle handlers.
        unsafe {
            check_silkit(
                (api.lifecycle_set_communication_ready_handler)(
                    lifecycle,
                    ctx_ptr,
                    on_communication_ready,
                ),
                "SetCommunicationReadyHandler",
            )?;

            check_silkit(
                (api.lifecycle_set_stop_handler)(lifecycle, ctx_ptr, on_stop),
                "SetStopHandler",
            )?;

            check_silkit(
                (api.lifecycle_set_shutdown_handler)(lifecycle, ctx_ptr, on_shutdown),
                "SetShutdownHandler",
            )?;
        }

        // 10. Register simulation step handler.
        unsafe {
            check_silkit(
                (api.time_sync_set_step_handler)(
                    time_sync,
                    ctx_ptr,
                    on_simulation_step,
                    self.config.step_size_ns,
                ),
                "SetSimulationStepHandler",
            )?;
        }

        // 11. Register frame receive handlers for each bus type.
        let mut handler_id: sys::SilKit_HandlerId = 0;
        unsafe {
            // CAN
            check_silkit(
                (api.can_controller_add_frame_handler)(
                    can_ctrl,
                    ctx_ptr,
                    on_can_frame_received,
                    2, // RX direction
                    &mut handler_id,
                ),
                "CanController_AddFrameHandler",
            )?;

            // ETH
            if let Some(ctrl) = opt_eth {
                check_silkit(
                    (api.ethernet_controller_add_frame_handler)(
                        ctrl,
                        ctx_ptr,
                        on_eth_frame_received,
                        2, // RX direction
                        &mut handler_id,
                    ),
                    "EthernetController_AddFrameHandler",
                )?;
            }

            // LIN
            if let Some(ctrl) = opt_lin {
                check_silkit(
                    (api.lin_controller_add_frame_status_handler)(
                        ctrl,
                        ctx_ptr,
                        on_lin_frame_status,
                        &mut handler_id,
                    ),
                    "LinController_AddFrameStatusHandler",
                )?;
            }

            // FlexRay
            if let Some(ctrl) = opt_fr {
                check_silkit(
                    (api.flexray_controller_add_frame_handler)(
                        ctrl,
                        ctx_ptr,
                        on_flexray_frame_received,
                        &mut handler_id,
                    ),
                    "FlexrayController_AddFrameHandler",
                )?;
            }
        }

        // 12. Start controllers.
        unsafe {
            check_silkit((api.can_controller_start)(can_ctrl), "CanController_Start")?;

            if let Some(ctrl) = opt_eth {
                check_silkit(
                    (api.ethernet_controller_activate)(ctrl),
                    "EthernetController_Activate",
                )?;
            }

            // LIN is started via Init (already done above).

            if let Some(_ctrl) = opt_fr {
                // NOTE: FlexRay requires Configure() with cluster/node
                // parameters before ExecuteCmd(RUN). Since we do not yet
                // support FlexRay configuration, we skip the RUN command
                // and only register the frame handler (passive listen).
                tracing::warn!(
                    "FlexRay controller created but not started: \
                     Configure() with cluster/node params required first"
                );
            }
        }

        // 13. Start lifecycle (non‑blocking).
        unsafe {
            check_silkit(
                (api.lifecycle_start)(lifecycle),
                "LifecycleService_StartLifecycle",
            )?;
        }

        tracing::info!("SIL Kit simulation started, waiting for completion…");

        // 14. Block until simulation completes.
        let mut final_state: sys::SilKit_ParticipantState = 0;
        unsafe {
            check_silkit(
                (api.lifecycle_wait_for_complete)(lifecycle, &mut final_state),
                "WaitForLifecycleToComplete",
            )?;
        }

        tracing::info!(final_state, "SIL Kit simulation completed");

        // 15. Cleanup.
        unsafe {
            let _ = (api.can_controller_stop)(can_ctrl);
            if let Some(ctrl) = opt_eth {
                let _ = (api.ethernet_controller_deactivate)(ctrl);
            }
            if let Some(ctrl) = opt_fr {
                let _ = (api.flexray_controller_execute_cmd)(ctrl, sys::SK_FLEXRAY_CHI_CMD_HALT);
            }
            let _ = (api.participant_destroy)(participant);
            let _ = (api.participant_configuration_destroy)(part_config);
        }

        // Keep ctx alive until cleanup is done.
        drop(ctx);

        Ok(())
    }
}

// ---------------------------------------------------------------------------
// Helper: check SIL Kit return code
// ---------------------------------------------------------------------------

/// Convert a SIL Kit return code into a [`RuntimeError`].
fn check_silkit(rc: sys::SilKit_ReturnCode, op: &str) -> Result<(), RuntimeError> {
    if rc == SK_OK {
        Ok(())
    } else {
        tracing::error!(rc, op, "SIL Kit call failed");
        Err(RuntimeError::Abi(vecu_abi::AbiError::ModuleError(rc)))
    }
}

// ---------------------------------------------------------------------------
// Controller creation helpers
// ---------------------------------------------------------------------------

/// `CString` pair kept alive while the controller pointer is in use.
type CStringPair = Option<(CString, CString)>;

/// Optionally create an Ethernet controller.
#[allow(unsafe_code)]
fn create_eth_controller(
    api: &SilKitApi,
    participant: *mut SilKit_Participant,
    network: Option<&str>,
    name: Option<&str>,
) -> Result<(Option<*mut SilKit_EthernetController>, CStringPair), RuntimeError> {
    let (Some(net), Some(ctrl_name)) = (network, name) else {
        return Ok((None, None));
    };
    let net_c = CString::new(net).unwrap();
    let name_c = CString::new(ctrl_name).unwrap();
    let mut ctrl: *mut SilKit_EthernetController = ptr::null_mut();
    unsafe {
        check_silkit(
            (api.ethernet_controller_create)(
                &mut ctrl,
                participant,
                name_c.as_ptr(),
                net_c.as_ptr(),
            ),
            "EthernetController_Create",
        )?;
    }
    tracing::info!(network = net, "Ethernet controller created");
    Ok((Some(ctrl), Some((name_c, net_c))))
}

/// Optionally create and initialise a LIN controller (master mode).
#[allow(unsafe_code)]
fn create_lin_controller(
    api: &SilKitApi,
    participant: *mut SilKit_Participant,
    network: Option<&str>,
    name: Option<&str>,
) -> Result<(Option<*mut SilKit_LinController>, CStringPair), RuntimeError> {
    let (Some(net), Some(ctrl_name)) = (network, name) else {
        return Ok((None, None));
    };
    let net_c = CString::new(net).unwrap();
    let name_c = CString::new(ctrl_name).unwrap();
    let mut ctrl: *mut SilKit_LinController = ptr::null_mut();
    unsafe {
        check_silkit(
            (api.lin_controller_create)(&mut ctrl, participant, name_c.as_ptr(), net_c.as_ptr()),
            "LinController_Create",
        )?;
    }
    let lin_cfg = sys::SilKit_LinControllerConfig {
        struct_header: SilKit_StructHeader::v1(),
        controller_mode: sys::SK_LIN_MODE_MASTER,
        baud_rate: 19_200,
        num_frame_responses: 0,
        frame_responses: ptr::null(),
    };
    unsafe {
        check_silkit(
            (api.lin_controller_init)(ctrl, &lin_cfg),
            "LinController_Init",
        )?;
    }
    tracing::info!(network = net, "LIN controller created (master)");
    Ok((Some(ctrl), Some((name_c, net_c))))
}

/// Optionally create a `FlexRay` controller.
#[allow(unsafe_code)]
fn create_flexray_controller(
    api: &SilKitApi,
    participant: *mut SilKit_Participant,
    network: Option<&str>,
    name: Option<&str>,
) -> Result<(Option<*mut SilKit_FlexrayController>, CStringPair), RuntimeError> {
    let (Some(net), Some(ctrl_name)) = (network, name) else {
        return Ok((None, None));
    };
    let net_c = CString::new(net).unwrap();
    let name_c = CString::new(ctrl_name).unwrap();
    let mut ctrl: *mut SilKit_FlexrayController = ptr::null_mut();
    unsafe {
        check_silkit(
            (api.flexray_controller_create)(
                &mut ctrl,
                participant,
                name_c.as_ptr(),
                net_c.as_ptr(),
            ),
            "FlexrayController_Create",
        )?;
    }
    tracing::info!(network = net, "`FlexRay` controller created");
    Ok((Some(ctrl), Some((name_c, net_c))))
}

// ---------------------------------------------------------------------------
// SIL Kit Callbacks (extern "C")
// ---------------------------------------------------------------------------

/// Called when communication channels are ready → start CAN controller.
#[allow(unsafe_code)]
unsafe extern "C" fn on_communication_ready(
    context: *mut c_void,
    _lifecycle: *mut SilKit_LifecycleService,
) {
    let ctx = unsafe { &*context.cast::<CallbackContext>() };
    ctx.comm_ready.store(true, Ordering::Release);
    tracing::info!("SIL Kit: communication ready");
}

/// Called on simulation stop.
#[allow(unsafe_code)]
unsafe extern "C" fn on_stop(context: *mut c_void, _lifecycle: *mut SilKit_LifecycleService) {
    let ctx = unsafe { &*context.cast::<CallbackContext>() };
    ctx.stopped.store(true, Ordering::Release);
    tracing::info!("SIL Kit: stop requested");
}

/// Called on simulation shutdown.
#[allow(unsafe_code)]
unsafe extern "C" fn on_shutdown(context: *mut c_void, _lifecycle: *mut SilKit_LifecycleService) {
    let ctx = unsafe { &*context.cast::<CallbackContext>() };
    // Call module shutdown via runtime.
    let runtime = unsafe { &mut *ctx.runtime };
    runtime.shutdown_all();
    tracing::info!("SIL Kit: shutdown complete");
}

/// Called each simulation tick by the SIL Kit time sync service.
///
/// This is the **core integration point**: it executes one vECU tick
/// per SIL Kit simulation step, maintaining deterministic behaviour.
///
/// Frame I/O is handled by the [`OpenSutApi`](vecu_runtime::OpenSutApi)
/// implementation ([`SilKitBus`]) set on the runtime — the callback
/// only needs to call `tick()`.
#[allow(unsafe_code)]
unsafe extern "C" fn on_simulation_step(
    context: *mut c_void,
    _time_sync: *mut SilKit_TimeSyncService,
    now: SilKit_NanosecondsTime,
    _duration: SilKit_NanosecondsTime,
) {
    let ctx = unsafe { &mut *context.cast::<CallbackContext>() };
    if !ctx.comm_ready.load(Ordering::Acquire) || ctx.stopped.load(Ordering::Acquire) {
        return;
    }

    let runtime = unsafe { &mut *ctx.runtime };

    // Execute one deterministic tick.
    // Inbound frames are pulled from SilKitBus::recv_inbound().
    // Outbound frames are dispatched via SilKitBus::dispatch_outbound().
    if let Err(e) = runtime.tick() {
        tracing::error!(%e, now, "tick failed during SIL Kit step");
    }
}

/// Called when a CAN frame is received from the SIL Kit bus.
///
/// Converts the SIL Kit CAN frame to a [`VecuFrame`] and pushes it into
/// the shared RX buffer. The [`SilKitBus`] drains this buffer in
/// [`recv_inbound()`](vecu_runtime::OpenSutApi::recv_inbound) during
/// the next `tick()`.
#[allow(unsafe_code, clippy::cast_possible_truncation)]
unsafe extern "C" fn on_can_frame_received(
    context: *mut c_void,
    _controller: *mut SilKit_CanController,
    frame_event: *const SilKit_CanFrameEvent,
) {
    let ctx = unsafe { &*context.cast::<CallbackContext>() };
    let event = unsafe { &*frame_event };

    if event.frame.is_null() {
        return;
    }
    let sk_frame = unsafe { &*event.frame };

    // Convert SIL Kit CAN frame → VecuFrame (tagged as CAN).
    let mut vecu_frame = VecuFrame::new(sk_frame.id);
    vecu_frame.bus_type = BusType::Can as u32;
    let copy_len = (sk_frame.data.size as usize).min(vecu_frame.data.len());
    if !sk_frame.data.data.is_null() && copy_len > 0 {
        let src = unsafe { std::slice::from_raw_parts(sk_frame.data.data, copy_len) };
        vecu_frame.data[..copy_len].copy_from_slice(src);
    }
    vecu_frame.len = u32::from(sk_frame.dlc);

    // Push into shared RX buffer (best‑effort).
    if let Ok(mut buf) = ctx.rx_buffer.lock() {
        buf.push(vecu_frame);
    }
}

/// Called when an Ethernet frame is received from the SIL Kit bus.
#[allow(unsafe_code, clippy::cast_possible_truncation)]
unsafe extern "C" fn on_eth_frame_received(
    context: *mut c_void,
    _controller: *mut SilKit_EthernetController,
    frame_event: *const SilKit_EthernetFrameEvent,
) {
    let ctx = unsafe { &*context.cast::<CallbackContext>() };
    let event = unsafe { &*frame_event };

    if event.ethernet_frame.is_null() {
        return;
    }
    let sk_frame = unsafe { &*event.ethernet_frame };

    let mut vecu_frame = VecuFrame::new(0);
    vecu_frame.bus_type = BusType::Eth as u32;
    let copy_len = (sk_frame.raw.size as usize).min(vecu_frame.data.len());
    if !sk_frame.raw.data.is_null() && copy_len > 0 {
        let src = unsafe { std::slice::from_raw_parts(sk_frame.raw.data, copy_len) };
        vecu_frame.data[..copy_len].copy_from_slice(src);
    }
    vecu_frame.len = copy_len as u32;

    if let Ok(mut buf) = ctx.rx_buffer.lock() {
        buf.push(vecu_frame);
    }
}

/// Called when a LIN frame status event is received.
///
/// We only forward frames with an RX‑OK status.
#[allow(unsafe_code, clippy::cast_possible_truncation)]
unsafe extern "C" fn on_lin_frame_status(
    context: *mut c_void,
    _controller: *mut SilKit_LinController,
    frame_status_event: *const SilKit_LinFrameStatusEvent,
) {
    const LIN_RX_OK: i32 = 5; // SilKit_LinFrameStatus_LIN_RX_OK

    let ctx = unsafe { &*context.cast::<CallbackContext>() };
    let event = unsafe { &*frame_status_event };

    if event.status != LIN_RX_OK || event.frame.is_null() {
        return;
    }
    let sk_frame = unsafe { &*event.frame };

    let mut vecu_frame = VecuFrame::new(u32::from(sk_frame.id));
    vecu_frame.bus_type = BusType::Lin as u32;
    let copy_len = (sk_frame.data_length as usize)
        .min(8)
        .min(vecu_frame.data.len());
    vecu_frame.data[..copy_len].copy_from_slice(&sk_frame.data[..copy_len]);
    vecu_frame.len = copy_len as u32;

    if let Ok(mut buf) = ctx.rx_buffer.lock() {
        buf.push(vecu_frame);
    }
}

/// Called when a `FlexRay` frame is received from the SIL Kit bus.
#[allow(unsafe_code, clippy::cast_possible_truncation)]
unsafe extern "C" fn on_flexray_frame_received(
    context: *mut c_void,
    _controller: *mut SilKit_FlexrayController,
    frame_event: *const SilKit_FlexrayFrameEvent,
) {
    let ctx = unsafe { &*context.cast::<CallbackContext>() };
    let event = unsafe { &*frame_event };

    if event.frame.is_null() {
        return;
    }
    let sk_frame = unsafe { &*event.frame };

    // Use slot ID from header as frame id (if header present).
    let frame_id = if sk_frame.header.is_null() {
        0
    } else {
        u32::from(unsafe { &*sk_frame.header }.frame_id)
    };

    let mut vecu_frame = VecuFrame::new(frame_id);
    vecu_frame.bus_type = BusType::FlexRay as u32;
    let copy_len = (sk_frame.payload.size as usize).min(vecu_frame.data.len());
    if !sk_frame.payload.data.is_null() && copy_len > 0 {
        let src = unsafe { std::slice::from_raw_parts(sk_frame.payload.data, copy_len) };
        vecu_frame.data[..copy_len].copy_from_slice(src);
    }
    vecu_frame.len = copy_len as u32;

    if let Ok(mut buf) = ctx.rx_buffer.lock() {
        buf.push(vecu_frame);
    }
}
