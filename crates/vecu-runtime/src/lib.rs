//! Deterministic tick orchestrator for the vECU execution system (ADR‑001).
//!
//! Implements the **mandatory tick sequence** defined in ADR‑001:
//!
//! ```text
//! for each simulation tick:
//!   1. Loader → APPL:  push_frame(inbound)   [0..N]
//!   2. Loader → HSM:   step(tick)
//!   3. Loader → APPL:  step(tick)
//!   4. Loader ← APPL:  poll_frame(outbound)  [0..N]
//! ```
//!
//! The runtime holds references to exactly one APPL and one HSM plugin API,
//! plus the shared memory. It is **single‑threaded** and **deterministic**.

use vecu_abi::{status, AbiError, VecuFrame, VecuPluginApi};
use vecu_shm::SharedMemory;

pub mod opensut;
pub use opensut::{NullBus, OpenSutApi};

// ---------------------------------------------------------------------------
// RuntimeAdapter trait – abstraction over tick source / execution model
// ---------------------------------------------------------------------------

/// Abstraction over the execution model.
///
/// The adapter decides **when** ticks happen and **how** inbound/outbound
/// frames are exchanged with the external environment (e.g. standalone
/// timer loop, SIL Kit virtual time sync, etc.).
///
/// The [`Runtime`] owns the deterministic tick *sequence* (push → HSM.step →
/// APPL.step → poll); the adapter owns the tick *source*.
pub trait RuntimeAdapter {
    /// Drive the simulation to completion.
    ///
    /// # Errors
    ///
    /// Implementations should forward any [`RuntimeError`] from the runtime.
    fn run(&mut self, runtime: &mut Runtime) -> Result<(), RuntimeError>;
}

// ---------------------------------------------------------------------------
// StandaloneAdapter – simple counted‑loop adapter
// ---------------------------------------------------------------------------

/// Adapter that executes a fixed number of ticks in a tight loop.
///
/// This is the default for `ExecMode::Standalone`.
pub struct StandaloneAdapter {
    /// Number of ticks to execute (0 = none).
    ticks: u64,
}

impl StandaloneAdapter {
    /// Create a new standalone adapter that runs `ticks` iterations.
    #[must_use]
    pub fn new(ticks: u64) -> Self {
        Self { ticks }
    }
}

impl RuntimeAdapter for StandaloneAdapter {
    fn run(&mut self, runtime: &mut Runtime) -> Result<(), RuntimeError> {
        runtime.run(self.ticks)
    }
}

// ---------------------------------------------------------------------------
// PluginSlot – thin wrapper around a VecuPluginApi
// ---------------------------------------------------------------------------

/// Wrapper that stores a plugin's API table and provides safe‑ish call helpers.
pub struct PluginSlot {
    api: VecuPluginApi,
}

impl PluginSlot {
    /// Wrap a validated [`VecuPluginApi`].
    #[must_use]
    pub fn new(api: VecuPluginApi) -> Self {
        Self { api }
    }

    /// Access the raw API table.
    #[must_use]
    pub fn api(&self) -> &VecuPluginApi {
        &self.api
    }

    /// Call `init(ctx)`.
    ///
    /// # Errors
    ///
    /// Returns [`AbiError::ModuleError`] on non‑zero return, or
    /// [`AbiError::MissingFunction`] if `init` is `None`.
    pub fn init(&self, ctx: &vecu_abi::VecuRuntimeContext) -> Result<(), AbiError> {
        let f = self
            .api
            .init
            .ok_or_else(|| AbiError::MissingFunction("init".into()))?;
        #[allow(unsafe_code)]
        // SAFETY: ctx is a valid reference, converted to a const pointer.
        let rc = unsafe { f(ctx) };
        if rc != status::OK {
            return Err(AbiError::ModuleError(rc));
        }
        Ok(())
    }

    /// Call `step(tick)`.
    ///
    /// # Errors
    ///
    /// Returns [`AbiError::ModuleError`] on non‑zero return.
    pub fn step(&self, tick: u64) -> Result<(), AbiError> {
        let f = self
            .api
            .step
            .ok_or_else(|| AbiError::MissingFunction("step".into()))?;
        #[allow(unsafe_code)]
        // SAFETY: init was called before step (enforced by orchestrator).
        let rc = unsafe { f(tick) };
        if rc != status::OK {
            return Err(AbiError::ModuleError(rc));
        }
        Ok(())
    }

    /// Call `push_frame(frame)`. No‑op if the plugin lacks `push_frame`.
    ///
    /// # Errors
    ///
    /// Returns [`AbiError::ModuleError`] on non‑zero return.
    pub fn push_frame(&self, frame: &VecuFrame) -> Result<(), AbiError> {
        if let Some(f) = self.api.push_frame {
            #[allow(unsafe_code)]
            // SAFETY: frame is a valid reference to a Pod type.
            let rc = unsafe { f(frame) };
            if rc != status::OK {
                return Err(AbiError::ModuleError(rc));
            }
        }
        Ok(())
    }

    /// Call `poll_frame(frame)`. Returns `None` if no frame or no function.
    pub fn poll_frame(&self) -> Option<VecuFrame> {
        let f = self.api.poll_frame?;
        let mut frame = VecuFrame::new(0);
        #[allow(unsafe_code)]
        // SAFETY: frame is a valid mutable Pod value on the stack.
        let rc = unsafe { f(&mut frame) };
        if rc == status::OK {
            Some(frame)
        } else {
            None
        }
    }

    /// Call `shutdown()`.
    pub fn shutdown(&self) {
        if let Some(f) = self.api.shutdown {
            f();
        }
    }
}

// ---------------------------------------------------------------------------
// Runtime
// ---------------------------------------------------------------------------

/// Deterministic, single‑threaded tick orchestrator.
///
/// Drives the ADR‑001 tick sequence with exactly one APPL and one HSM slot.
pub struct Runtime {
    appl: Option<PluginSlot>,
    hsm: Option<PluginSlot>,
    shm: SharedMemory,
    tick: u64,
    /// Per‑module [`OpenSutApi`] bus for the APPL module.
    appl_bus: Option<Box<dyn OpenSutApi>>,
    /// Per‑module [`OpenSutApi`] bus for the HSM module.
    hsm_bus: Option<Box<dyn OpenSutApi>>,
    /// Reusable buffers to avoid per‑tick allocations.
    inbound_buf: Vec<VecuFrame>,
    outbound_buf: Vec<VecuFrame>,
}

/// Errors produced by the runtime.
#[derive(Debug, thiserror::Error)]
pub enum RuntimeError {
    /// ABI‑level error.
    #[error(transparent)]
    Abi(#[from] AbiError),
    /// A required module slot is not set.
    #[error("missing module: {0}")]
    MissingModule(&'static str),
}

impl Runtime {
    /// Create a new runtime with the given shared memory.
    #[must_use]
    pub fn new(shm: SharedMemory) -> Self {
        Self {
            appl: None,
            hsm: None,
            shm,
            tick: 0,
            appl_bus: None,
            hsm_bus: None,
            inbound_buf: Vec::new(),
            outbound_buf: Vec::new(),
        }
    }

    /// Set the APPL plugin slot.
    pub fn set_appl(&mut self, slot: PluginSlot) {
        self.appl = Some(slot);
    }

    /// Set the HSM plugin slot.
    pub fn set_hsm(&mut self, slot: PluginSlot) {
        self.hsm = Some(slot);
    }

    /// Set the [`OpenSutApi`] bus for the **APPL** module.
    ///
    /// When set, `tick()` uses this bus for APPL inbound/outbound frame
    /// exchange instead of the SHM RX/TX queues.
    pub fn set_appl_bus(&mut self, bus: Box<dyn OpenSutApi>) {
        self.appl_bus = Some(bus);
    }

    /// Set the [`OpenSutApi`] bus for the **HSM** module.
    ///
    /// When set, `tick()` delivers inbound frames to HSM and collects
    /// outbound frames from HSM via this bus.
    pub fn set_hsm_bus(&mut self, bus: Box<dyn OpenSutApi>) {
        self.hsm_bus = Some(bus);
    }

    /// Convenience: set a single bus for APPL (backward‑compatible).
    ///
    /// Equivalent to [`set_appl_bus`](Self::set_appl_bus).
    pub fn set_bus(&mut self, bus: Box<dyn OpenSutApi>) {
        self.appl_bus = Some(bus);
    }

    /// Initialise both modules by calling `init(ctx)`.
    ///
    /// # Errors
    ///
    /// Returns on the first init failure.
    pub fn init_all(&mut self, ctx: &vecu_abi::VecuRuntimeContext) -> Result<(), RuntimeError> {
        if let Some(hsm) = &self.hsm {
            hsm.init(ctx)?;
        }
        if let Some(appl) = &self.appl {
            appl.init(ctx)?;
        }
        // Notify buses that simulation is starting.
        if let Some(bus) = &mut self.appl_bus {
            bus.on_start()?;
        }
        if let Some(bus) = &mut self.hsm_bus {
            bus.on_start()?;
        }
        Ok(())
    }

    /// Execute a single deterministic tick per ADR‑001 sequence.
    ///
    /// # Errors
    ///
    /// Returns on the first module error.
    pub fn tick(&mut self) -> Result<(), RuntimeError> {
        let current_tick = self.tick;

        // 1a. Collect APPL inbound: appl_bus or SHM RX queue.
        self.inbound_buf.clear();
        if let Some(bus) = &mut self.appl_bus {
            bus.recv_inbound(&mut self.inbound_buf)?;
        } else {
            while let Some(frame) = self.shm.rx_pop() {
                self.inbound_buf.push(frame);
            }
        }

        // 1b. Loader → APPL: push_frame(inbound) [0..N]
        if let Some(appl) = &self.appl {
            for frame in &self.inbound_buf {
                appl.push_frame(frame)?;
            }
        }

        // 2a. Collect HSM inbound: hsm_bus (no SHM fallback for HSM).
        self.inbound_buf.clear();
        if let Some(bus) = &mut self.hsm_bus {
            bus.recv_inbound(&mut self.inbound_buf)?;
        }

        // 2b. Loader → HSM: push_frame(inbound) [0..N]
        if let Some(hsm) = &self.hsm {
            for frame in &self.inbound_buf {
                hsm.push_frame(frame)?;
            }
        }

        // 3. Loader → HSM: step(tick)
        if let Some(hsm) = &self.hsm {
            hsm.step(current_tick)?;
        }

        // 4. Loader → APPL: step(tick)
        if let Some(appl) = &self.appl {
            appl.step(current_tick)?;
        }

        // 5a. Loader ← HSM: poll_frame(outbound) [0..N]
        self.outbound_buf.clear();
        if let Some(hsm) = &self.hsm {
            while let Some(frame) = hsm.poll_frame() {
                self.outbound_buf.push(frame);
            }
        }

        // 5b. Dispatch HSM outbound: hsm_bus (no SHM fallback for HSM).
        if let Some(bus) = &mut self.hsm_bus {
            bus.dispatch_outbound(&self.outbound_buf)?;
        }

        // 6a. Loader ← APPL: poll_frame(outbound) [0..N]
        self.outbound_buf.clear();
        if let Some(appl) = &self.appl {
            while let Some(frame) = appl.poll_frame() {
                self.outbound_buf.push(frame);
            }
        }

        // 6b. Dispatch APPL outbound: appl_bus or SHM TX queue.
        if let Some(bus) = &mut self.appl_bus {
            bus.dispatch_outbound(&self.outbound_buf)?;
        } else {
            for frame in &self.outbound_buf {
                // Best‑effort: silently drop if TX queue is full.
                let _ = self.shm.tx_push(frame);
            }
        }

        self.tick += 1;
        Ok(())
    }

    /// Run `n` ticks sequentially.
    ///
    /// # Errors
    ///
    /// Stops and returns on the first error.
    pub fn run(&mut self, n: u64) -> Result<(), RuntimeError> {
        for _ in 0..n {
            self.tick()?;
        }
        Ok(())
    }

    /// Shut down all modules (reverse order: APPL first, then HSM).
    pub fn shutdown_all(&mut self) {
        // Notify buses that simulation is stopping.
        if let Some(bus) = &mut self.appl_bus {
            let _ = bus.on_stop();
        }
        if let Some(bus) = &mut self.hsm_bus {
            let _ = bus.on_stop();
        }
        if let Some(appl) = &self.appl {
            appl.shutdown();
        }
        if let Some(hsm) = &self.hsm {
            hsm.shutdown();
        }
    }

    /// Current tick counter.
    #[must_use]
    pub fn current_tick(&self) -> u64 {
        self.tick
    }

    /// Pointer to the HSM plugin's [`VecuPluginApi`], or null if no HSM is set.
    ///
    /// Used by the loader to populate [`VecuRuntimeContext::hsm_api`] so
    /// that the APPL plugin can wire HSM callbacks into the `BaseLayer`.
    #[must_use]
    pub fn hsm_api_ptr(&self) -> *const vecu_abi::VecuPluginApi {
        self.hsm
            .as_ref()
            .map_or(core::ptr::null(), |s| s.api() as *const _)
    }

    /// Borrow the shared memory (read‑only).
    #[must_use]
    pub fn shm(&self) -> &SharedMemory {
        &self.shm
    }

    /// Borrow the shared memory (mutable).
    pub fn shm_mut(&mut self) -> &mut SharedMemory {
        &mut self.shm
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicU64, Ordering};
    use std::sync::Mutex;
    use vecu_abi::{ModuleKind, VecuPluginApi, VecuRuntimeContext, ABI_VERSION, CAP_FRAME_IO};

    // -- shared test state ------------------------------------------------

    static TEST_LOCK: Mutex<()> = Mutex::new(());
    static STEP_TICK: AtomicU64 = AtomicU64::new(0);
    static INIT_CALLED: std::sync::atomic::AtomicBool = std::sync::atomic::AtomicBool::new(false);

    fn reset_state() {
        STEP_TICK.store(0, Ordering::SeqCst);
        INIT_CALLED.store(false, Ordering::SeqCst);
    }

    // -- fake plugin functions --------------------------------------------

    #[allow(unsafe_code)]
    unsafe extern "C" fn fake_init(_ctx: *const VecuRuntimeContext) -> i32 {
        INIT_CALLED.store(true, Ordering::SeqCst);
        status::OK
    }

    extern "C" fn fake_shutdown() {}

    #[allow(unsafe_code)]
    unsafe extern "C" fn fake_step(tick: u64) -> i32 {
        STEP_TICK.store(tick, Ordering::SeqCst);
        status::OK
    }

    #[allow(unsafe_code)]
    unsafe extern "C" fn failing_step(_tick: u64) -> i32 {
        status::MODULE_ERROR
    }

    fn make_appl_api() -> VecuPluginApi {
        let mut api = VecuPluginApi::zeroed();
        api.abi_version = ABI_VERSION;
        api.module_kind = ModuleKind::Appl as u32;
        api.capabilities = CAP_FRAME_IO;
        api.init = Some(fake_init);
        api.shutdown = Some(fake_shutdown);
        api.step = Some(fake_step);
        api
    }

    fn make_hsm_api() -> VecuPluginApi {
        let mut api = VecuPluginApi::zeroed();
        api.abi_version = ABI_VERSION;
        api.module_kind = ModuleKind::Hsm as u32;
        api.capabilities = 0;
        api.init = Some(fake_init);
        api.shutdown = Some(fake_shutdown);
        api.step = Some(fake_step);
        api
    }

    fn make_ctx() -> VecuRuntimeContext {
        VecuRuntimeContext {
            shm_base: core::ptr::null_mut(),
            shm_size: 0,
            pad0: 0,
            tick_interval_us: 1000,
            log_fn: None,
            hsm_api: core::ptr::null(),
        }
    }

    // -- tests ------------------------------------------------------------

    #[test]
    fn runtime_tick_increments() {
        let _g = TEST_LOCK.lock().unwrap();
        reset_state();
        let shm = SharedMemory::anonymous();
        let mut rt = Runtime::new(shm);
        rt.set_appl(PluginSlot::new(make_appl_api()));
        rt.set_hsm(PluginSlot::new(make_hsm_api()));

        let ctx = make_ctx();
        rt.init_all(&ctx).unwrap();
        rt.tick().unwrap();
        assert_eq!(rt.current_tick(), 1);
    }

    #[test]
    fn runtime_run_executes_n_ticks() {
        let _g = TEST_LOCK.lock().unwrap();
        reset_state();
        let shm = SharedMemory::anonymous();
        let mut rt = Runtime::new(shm);
        rt.set_appl(PluginSlot::new(make_appl_api()));
        rt.set_hsm(PluginSlot::new(make_hsm_api()));

        let ctx = make_ctx();
        rt.init_all(&ctx).unwrap();
        rt.run(10).unwrap();
        assert_eq!(rt.current_tick(), 10);
        // Last step received tick = 9 (0‑indexed).
        assert_eq!(STEP_TICK.load(Ordering::SeqCst), 9);
    }

    #[test]
    fn init_sets_flag() {
        let _g = TEST_LOCK.lock().unwrap();
        reset_state();
        let shm = SharedMemory::anonymous();
        let mut rt = Runtime::new(shm);
        rt.set_appl(PluginSlot::new(make_appl_api()));

        let ctx = make_ctx();
        rt.init_all(&ctx).unwrap();
        assert!(INIT_CALLED.load(Ordering::SeqCst));
    }

    #[test]
    fn step_error_propagates() {
        let _g = TEST_LOCK.lock().unwrap();
        reset_state();
        let mut api = make_appl_api();
        api.step = Some(failing_step);

        let shm = SharedMemory::anonymous();
        let mut rt = Runtime::new(shm);
        rt.set_appl(PluginSlot::new(api));

        let ctx = make_ctx();
        rt.init_all(&ctx).unwrap();
        let result = rt.tick();
        assert!(result.is_err());
    }

    #[test]
    fn shutdown_does_not_panic() {
        let _g = TEST_LOCK.lock().unwrap();
        reset_state();
        let shm = SharedMemory::anonymous();
        let mut rt = Runtime::new(shm);
        rt.set_appl(PluginSlot::new(make_appl_api()));
        rt.set_hsm(PluginSlot::new(make_hsm_api()));

        let ctx = make_ctx();
        rt.init_all(&ctx).unwrap();
        rt.shutdown_all();
    }

    #[test]
    fn runtime_works_without_hsm() {
        let _g = TEST_LOCK.lock().unwrap();
        reset_state();
        let shm = SharedMemory::anonymous();
        let mut rt = Runtime::new(shm);
        rt.set_appl(PluginSlot::new(make_appl_api()));

        let ctx = make_ctx();
        rt.init_all(&ctx).unwrap();
        rt.run(3).unwrap();
        assert_eq!(rt.current_tick(), 3);
    }

    #[test]
    fn plugin_slot_missing_step_errors() {
        let mut api = VecuPluginApi::zeroed();
        api.abi_version = ABI_VERSION;
        let slot = PluginSlot::new(api);
        let result = slot.step(0);
        assert!(matches!(result, Err(AbiError::MissingFunction(_))));
    }

    // -- RuntimeAdapter tests ------------------------------------------------

    #[test]
    fn standalone_adapter_runs_ticks() {
        let _g = TEST_LOCK.lock().unwrap();
        reset_state();
        let shm = SharedMemory::anonymous();
        let mut rt = Runtime::new(shm);
        rt.set_appl(PluginSlot::new(make_appl_api()));

        let ctx = make_ctx();
        rt.init_all(&ctx).unwrap();

        let mut adapter = StandaloneAdapter::new(5);
        adapter.run(&mut rt).unwrap();
        assert_eq!(rt.current_tick(), 5);
    }

    #[test]
    fn standalone_adapter_zero_ticks_is_noop() {
        let _g = TEST_LOCK.lock().unwrap();
        reset_state();
        let shm = SharedMemory::anonymous();
        let mut rt = Runtime::new(shm);
        rt.set_appl(PluginSlot::new(make_appl_api()));

        let ctx = make_ctx();
        rt.init_all(&ctx).unwrap();

        let mut adapter = StandaloneAdapter::new(0);
        adapter.run(&mut rt).unwrap();
        assert_eq!(rt.current_tick(), 0);
    }

    // -- OpenSutApi tests ----------------------------------------------------

    /// A recording bus that captures all outbound frames and injects
    /// pre-loaded inbound frames. Used to verify the `OpenSutApi`
    /// integration in `Runtime::tick()`.
    struct RecordingBus {
        inbound: Vec<VecuFrame>,
        outbound: Vec<VecuFrame>,
        started: bool,
        stopped: bool,
    }

    impl RecordingBus {
        fn new() -> Self {
            Self {
                inbound: Vec::new(),
                outbound: Vec::new(),
                started: false,
                stopped: false,
            }
        }

        fn _with_inbound(frames: Vec<VecuFrame>) -> Self {
            Self {
                inbound: frames,
                outbound: Vec::new(),
                started: false,
                stopped: false,
            }
        }
    }

    impl OpenSutApi for RecordingBus {
        fn recv_inbound(&mut self, out: &mut Vec<VecuFrame>) -> Result<(), RuntimeError> {
            out.append(&mut self.inbound);
            Ok(())
        }

        fn dispatch_outbound(&mut self, frames: &[VecuFrame]) -> Result<(), RuntimeError> {
            self.outbound.extend_from_slice(frames);
            Ok(())
        }

        fn on_start(&mut self) -> Result<(), RuntimeError> {
            self.started = true;
            Ok(())
        }

        fn on_stop(&mut self) -> Result<(), RuntimeError> {
            self.stopped = true;
            Ok(())
        }
    }

    #[test]
    fn tick_with_bus_dispatches_outbound() {
        let _g = TEST_LOCK.lock().unwrap();
        reset_state();
        let shm = SharedMemory::anonymous();
        let mut rt = Runtime::new(shm);
        rt.set_appl(PluginSlot::new(make_appl_api()));
        rt.set_bus(Box::new(RecordingBus::new()));

        let ctx = make_ctx();
        rt.init_all(&ctx).unwrap();
        rt.tick().unwrap();

        // The APPL echo plugin may not produce frames without input,
        // but the tick should complete without error.
        assert_eq!(rt.current_tick(), 1);
    }

    #[test]
    fn tick_without_bus_uses_shm_fallback() {
        let _g = TEST_LOCK.lock().unwrap();
        reset_state();
        let shm = SharedMemory::anonymous();
        let mut rt = Runtime::new(shm);
        rt.set_appl(PluginSlot::new(make_appl_api()));
        // No set_bus() call — SHM fallback.

        let ctx = make_ctx();
        rt.init_all(&ctx).unwrap();
        rt.tick().unwrap();
        assert_eq!(rt.current_tick(), 1);
    }

    #[test]
    fn null_bus_works_as_bus() {
        let _g = TEST_LOCK.lock().unwrap();
        reset_state();
        let shm = SharedMemory::anonymous();
        let mut rt = Runtime::new(shm);
        rt.set_appl(PluginSlot::new(make_appl_api()));
        rt.set_bus(Box::new(NullBus));

        let ctx = make_ctx();
        rt.init_all(&ctx).unwrap();
        rt.run(3).unwrap();
        assert_eq!(rt.current_tick(), 3);
    }

    #[test]
    fn per_module_buses_both_called() {
        let _g = TEST_LOCK.lock().unwrap();
        reset_state();
        let shm = SharedMemory::anonymous();
        let mut rt = Runtime::new(shm);
        rt.set_appl(PluginSlot::new(make_appl_api()));
        rt.set_hsm(PluginSlot::new(make_hsm_api()));
        rt.set_appl_bus(Box::new(RecordingBus::new()));
        rt.set_hsm_bus(Box::new(RecordingBus::new()));

        let ctx = make_ctx();
        rt.init_all(&ctx).unwrap();
        rt.tick().unwrap();
        assert_eq!(rt.current_tick(), 1);
    }

    #[test]
    fn hsm_bus_without_appl_bus() {
        let _g = TEST_LOCK.lock().unwrap();
        reset_state();
        let shm = SharedMemory::anonymous();
        let mut rt = Runtime::new(shm);
        rt.set_appl(PluginSlot::new(make_appl_api()));
        rt.set_hsm(PluginSlot::new(make_hsm_api()));
        // Only HSM has a bus; APPL uses SHM fallback.
        rt.set_hsm_bus(Box::new(NullBus));

        let ctx = make_ctx();
        rt.init_all(&ctx).unwrap();
        rt.run(3).unwrap();
        assert_eq!(rt.current_tick(), 3);
    }
}
