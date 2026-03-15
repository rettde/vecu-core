//! `OpenSUT` Communication API – formal abstraction over bus I/O.
//!
//! This trait defines the **explicit interface** between the vECU runtime
//! environment and external communication infrastructure (SIL Kit, hardware
//! backends, test stubs, etc.).
//!
//! # Architecture (per `OpenSUT` / SIL Kit Use-Case diagram)
//!
//! ```text
//! ┌─────────────────────────────────────────────────────┐
//! │  vECU Runtime Env                                   │
//! │                                                     │
//! │  APPL.dll ──┐                                       │
//! │             ├──→ [OpenSutApi] ──→ CAN/ETH/LIN/FR ──┼──→ SIL Kit
//! │  HSM.dll  ──┘                                       │
//! └─────────────────────────────────────────────────────┘
//! ```
//!
//! # Separation of Concerns
//!
//! | Trait | Responsibility |
//! |-------|---------------|
//! | [`RuntimeAdapter`](super::RuntimeAdapter) | **When** ticks happen (timer, SIL Kit `TimeSyncService`) |
//! | [`OpenSutApi`] | **Where** frames go (SHM queues, SIL Kit CAN, hardware) |

use vecu_abi::VecuFrame;

use crate::RuntimeError;

// ---------------------------------------------------------------------------
// OpenSutApi trait
// ---------------------------------------------------------------------------

/// Formal abstraction layer for external bus communication.
///
/// Implementations bridge the vECU runtime to a specific communication
/// backend. The [`Runtime`](super::Runtime) calls these methods during each
/// tick to exchange frames with the outside world.
///
/// When no `OpenSutApi` is set on the runtime, it falls back to the
/// shared-memory RX/TX queues (standalone behaviour).
pub trait OpenSutApi: Send {
    /// Collect inbound frames from external buses.
    ///
    /// Called **once per tick**, before `push_frame` calls to modules.
    /// Implementations should drain their internal receive buffers and
    /// append all pending frames to `out`.
    ///
    /// # Errors
    ///
    /// Returns [`RuntimeError`] if the bus backend encounters a fatal error.
    fn recv_inbound(&mut self, out: &mut Vec<VecuFrame>) -> Result<(), RuntimeError>;

    /// Dispatch outbound frames to external buses.
    ///
    /// Called **once per tick**, after `poll_frame` calls from modules.
    /// Implementations route each frame to the appropriate bus controller
    /// based on [`VecuFrame::bus_type`](vecu_abi::VecuFrame::bus_type).
    ///
    /// # Errors
    ///
    /// Returns [`RuntimeError`] if the bus backend encounters a fatal error.
    fn dispatch_outbound(&mut self, frames: &[VecuFrame]) -> Result<(), RuntimeError>;

    /// Called once when the simulation starts (after module `init`).
    ///
    /// # Errors
    ///
    /// Returns [`RuntimeError`] on failure.
    fn on_start(&mut self) -> Result<(), RuntimeError>;

    /// Called once when the simulation stops (before module `shutdown`).
    ///
    /// # Errors
    ///
    /// Returns [`RuntimeError`] on failure.
    fn on_stop(&mut self) -> Result<(), RuntimeError>;
}

// ---------------------------------------------------------------------------
// NullBus – no-op implementation (test / standalone fallback)
// ---------------------------------------------------------------------------

/// No-op bus implementation.
///
/// Never produces inbound frames and silently drops outbound frames.
/// Useful for unit tests or when no external bus is connected.
pub struct NullBus;

impl OpenSutApi for NullBus {
    fn recv_inbound(&mut self, _out: &mut Vec<VecuFrame>) -> Result<(), RuntimeError> {
        Ok(())
    }

    fn dispatch_outbound(&mut self, _frames: &[VecuFrame]) -> Result<(), RuntimeError> {
        Ok(())
    }

    fn on_start(&mut self) -> Result<(), RuntimeError> {
        Ok(())
    }

    fn on_stop(&mut self) -> Result<(), RuntimeError> {
        Ok(())
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn null_bus_recv_is_empty() {
        let mut bus = NullBus;
        let mut frames = Vec::new();
        bus.recv_inbound(&mut frames).unwrap();
        assert!(frames.is_empty());
    }

    #[test]
    fn null_bus_dispatch_is_noop() {
        let mut bus = NullBus;
        let frame = VecuFrame::new(0x100);
        bus.dispatch_outbound(&[frame]).unwrap();
    }

    #[test]
    fn null_bus_lifecycle() {
        let mut bus = NullBus;
        bus.on_start().unwrap();
        bus.on_stop().unwrap();
    }

    #[test]
    fn null_bus_is_send() {
        fn assert_send<T: Send>() {}
        assert_send::<NullBus>();
    }

    #[test]
    fn trait_object_is_send() {
        fn assert_send<T: Send + ?Sized>() {}
        assert_send::<Box<dyn OpenSutApi>>();
    }
}
