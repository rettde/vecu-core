//! Vector SIL Kit runtime adapter for the vECU execution system.
//!
//! This crate provides [`SilKitAdapter`], an implementation of
//! [`vecu_runtime::RuntimeAdapter`] that connects the deterministic vECU
//! tick loop to a SIL Kit co‑simulation environment.
//!
//! # Design
//!
//! - The SIL Kit shared library is loaded **dynamically** at runtime via
//!   [`libloading`], so it is an optional dependency – no SIL Kit headers or
//!   libraries are required at build time.
//! - The vECU Loader registers as a **SIL Kit participant** with:
//!   - A [`LifecycleService`](sys::SilKit_LifecycleService) for coordinated
//!     start / stop / shutdown.
//!   - A [`TimeSyncService`](sys::SilKit_TimeSyncService) whose simulation
//!     step handler calls [`Runtime::tick()`](vecu_runtime::Runtime::tick) once
//!     per virtual time step.
//!   - A [`CanController`](sys::SilKit_CanController) that maps SIL Kit CAN
//!     frames to [`VecuFrame`](vecu_abi::VecuFrame) I/O via the shared‑memory
//!     RX / TX queues.
//!
//! # Frame Mapping
//!
//! | Direction | SIL Kit → vECU | vECU → SIL Kit |
//! |-----------|---------------|---------------|
//! | Inbound   | `CanFrameHandler` → `SilKitBus::recv_inbound` → `APPL.push_frame` | – |
//! | Outbound  | – | `APPL.poll_frame` → `SilKitBus::dispatch_outbound` → `CanController.SendFrame` |

pub mod adapter;
pub mod bus;
pub mod sys;

pub use adapter::{SilKitAdapter, SilKitConfig};
pub use bus::SilKitBus;

// ---------------------------------------------------------------------------
// Error types
// ---------------------------------------------------------------------------

/// Errors specific to the SIL Kit adapter.
#[derive(Debug, thiserror::Error)]
pub enum SilKitError {
    /// The SIL Kit shared library could not be loaded.
    #[error("failed to load SIL Kit library '{path}': {source}")]
    LibraryLoad {
        /// Library path that was attempted.
        path: String,
        /// Underlying error.
        source: libloading::Error,
    },
    /// A SIL Kit C API call returned an error code.
    #[error("SIL Kit API call '{operation}' failed with code {code}")]
    ApiCall {
        /// Name of the operation that failed.
        operation: String,
        /// SIL Kit return code.
        code: i32,
    },
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn default_config_has_sane_values() {
        let cfg = SilKitConfig::default();
        assert_eq!(cfg.registry_uri, "silkit://localhost:8500");
        assert_eq!(cfg.participant_name, "vECU");
        assert_eq!(cfg.can_network, "CAN1");
        assert_eq!(cfg.step_size_ns, 1_000_000);
        assert!(cfg.coordinated);
    }

    #[test]
    fn config_library_path_is_platform_specific() {
        let cfg = SilKitConfig::default();
        let path = std::path::Path::new(&cfg.library_path);
        if cfg!(target_os = "windows") {
            assert!(cfg.library_path.contains("SilKit"));
        } else if cfg!(target_os = "macos") {
            assert_eq!(path.extension().and_then(|e| e.to_str()), Some("dylib"));
        } else {
            assert_eq!(path.extension().and_then(|e| e.to_str()), Some("so"));
        }
    }

    #[test]
    fn silkit_adapter_creates() {
        let cfg = SilKitConfig::default();
        let adapter = SilKitAdapter::new(cfg);
        // Adapter creation should always succeed (library is loaded lazily in run()).
        let _ = adapter;
    }

    #[test]
    fn silkit_error_display() {
        let err = SilKitError::ApiCall {
            operation: "Participant_Create".into(),
            code: -1,
        };
        let msg = format!("{err}");
        assert!(msg.contains("Participant_Create"));
        assert!(msg.contains("-1"));
    }

    #[test]
    fn struct_header_v1() {
        let hdr = sys::SilKit_StructHeader::v1();
        assert_eq!(hdr.version, 1);
    }

    #[test]
    fn default_config_optional_buses_are_none() {
        let cfg = SilKitConfig::default();
        assert!(cfg.eth_network.is_none());
        assert!(cfg.eth_controller_name.is_none());
        assert!(cfg.lin_network.is_none());
        assert!(cfg.lin_controller_name.is_none());
        assert!(cfg.flexray_network.is_none());
        assert!(cfg.flexray_controller_name.is_none());
    }

    #[test]
    fn config_with_all_buses() {
        let cfg = SilKitConfig {
            eth_network: Some("ETH1".into()),
            eth_controller_name: Some("ETH1".into()),
            lin_network: Some("LIN1".into()),
            lin_controller_name: Some("LIN1".into()),
            flexray_network: Some("FR1".into()),
            flexray_controller_name: Some("FR1".into()),
            ..SilKitConfig::default()
        };
        assert_eq!(cfg.eth_network.as_deref(), Some("ETH1"));
        assert_eq!(cfg.lin_network.as_deref(), Some("LIN1"));
        assert_eq!(cfg.flexray_network.as_deref(), Some("FR1"));
    }

    #[test]
    fn bus_type_enum_values_match_abi() {
        use vecu_abi::BusType;
        assert_eq!(BusType::Can as u32, 0);
        assert_eq!(BusType::Eth as u32, 1);
        assert_eq!(BusType::Lin as u32, 2);
        assert_eq!(BusType::FlexRay as u32, 3);
    }

    #[test]
    fn shared_rx_buffer_push_and_drain() {
        let buf = bus::new_rx_buffer();
        {
            let mut lock = buf.lock().unwrap();
            lock.push(vecu_abi::VecuFrame::new(42));
            lock.push(vecu_abi::VecuFrame::new(99));
        }
        let mut out = Vec::new();
        {
            let mut lock = buf.lock().unwrap();
            out.append(&mut *lock);
        }
        assert_eq!(out.len(), 2);
        assert_eq!(out[0].id, 42);
        assert_eq!(out[1].id, 99);
    }
}
