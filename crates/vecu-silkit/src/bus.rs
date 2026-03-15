//! [`OpenSutApi`] implementation backed by SIL Kit bus controllers.
//!
//! [`SilKitBus`] provides the bridge between the vECU runtime's frame I/O
//! (steps 1 & 6 of the tick sequence) and the SIL Kit bus controllers
//! (CAN, Ethernet, LIN, `FlexRay`).
//!
//! Inbound frames arrive via a shared buffer filled by the various
//! frame-receive callbacks; outbound frames are dispatched to the
//! matching controller based on [`BusType`].

use std::sync::{Arc, Mutex};

use vecu_abi::{BusType, VecuFrame};
use vecu_runtime::{OpenSutApi, RuntimeError};

use crate::sys::{
    SilKitApi, SilKit_ByteVector, SilKit_CanController, SilKit_CanFrame, SilKit_EthernetController,
    SilKit_EthernetFrame, SilKit_FlexrayController, SilKit_FlexrayTxBufferUpdate,
    SilKit_LinController, SilKit_LinFrame, SilKit_StructHeader, SK_LIN_CHECKSUM_ENHANCED,
    SK_LIN_FRAME_RESPONSE_MASTER, SK_OK,
};

// ---------------------------------------------------------------------------
// Shared RX buffer
// ---------------------------------------------------------------------------

/// Thread-safe buffer for inbound frames, shared between the SIL Kit
/// callbacks and the [`SilKitBus`].
pub type SharedRxBuffer = Arc<Mutex<Vec<VecuFrame>>>;

/// Create a new shared RX buffer.
#[must_use]
pub fn new_rx_buffer() -> SharedRxBuffer {
    Arc::new(Mutex::new(Vec::new()))
}

// ---------------------------------------------------------------------------
// SilKitBus
// ---------------------------------------------------------------------------

/// [`OpenSutApi`] implementation for SIL Kit.
///
/// Routes outbound [`VecuFrame`]s to the appropriate SIL Kit bus controller
/// based on `bus_type`, and drains the shared RX buffer for inbound frames.
///
/// Controllers are optional – only those created by the adapter are set.
/// Frames targeting a controller that is `None` are logged and dropped.
pub struct SilKitBus {
    /// Shared buffer filled by all frame-receive callbacks.
    rx_buffer: SharedRxBuffer,
    /// Pointer to the dynamically loaded SIL Kit API.
    api: *const SilKitApi,
    /// CAN controller handle (optional).
    can_controller: Option<*mut SilKit_CanController>,
    /// Ethernet controller handle (optional).
    eth_controller: Option<*mut SilKit_EthernetController>,
    /// LIN controller handle (optional).
    lin_controller: Option<*mut SilKit_LinController>,
    /// `FlexRay` controller handle (optional).
    flexray_controller: Option<*mut SilKit_FlexrayController>,
}

impl SilKitBus {
    /// Create a new SIL Kit bus with the given controllers.
    ///
    /// # Safety
    ///
    /// `api` and all controller pointers must remain valid for the lifetime
    /// of this object (guaranteed by `SilKitAdapter::run()` keeping them
    /// alive on the stack).
    #[allow(unsafe_code)]
    pub unsafe fn new(
        rx_buffer: SharedRxBuffer,
        api: *const SilKitApi,
        can_controller: Option<*mut SilKit_CanController>,
        eth_controller: Option<*mut SilKit_EthernetController>,
        lin_controller: Option<*mut SilKit_LinController>,
        flexray_controller: Option<*mut SilKit_FlexrayController>,
    ) -> Self {
        Self {
            rx_buffer,
            api,
            can_controller,
            eth_controller,
            lin_controller,
            flexray_controller,
        }
    }
}

// SAFETY: `SilKitBus` is used exclusively within the `SilKitAdapter::run()`
// scope where all raw pointers are guaranteed valid. The `Arc<Mutex<…>>`
// provides the necessary synchronisation for the RX buffer.
#[allow(unsafe_code)]
unsafe impl Send for SilKitBus {}

impl OpenSutApi for SilKitBus {
    fn recv_inbound(&mut self, out: &mut Vec<VecuFrame>) -> Result<(), RuntimeError> {
        if let Ok(mut buf) = self.rx_buffer.lock() {
            out.append(&mut buf);
        }
        Ok(())
    }

    #[allow(unsafe_code, clippy::cast_possible_truncation)]
    fn dispatch_outbound(&mut self, frames: &[VecuFrame]) -> Result<(), RuntimeError> {
        let api = unsafe { &*self.api };
        for frame in frames {
            match BusType::from_raw(frame.bus_type) {
                Some(BusType::Can) | None => {
                    if let Some(ctrl) = self.can_controller {
                        send_can_frame(api, ctrl, frame);
                    }
                }
                Some(BusType::Eth) => {
                    if let Some(ctrl) = self.eth_controller {
                        send_eth_frame(api, ctrl, frame);
                    } else {
                        tracing::debug!(id = frame.id, "ETH frame dropped: no controller");
                    }
                }
                Some(BusType::Lin) => {
                    if let Some(ctrl) = self.lin_controller {
                        send_lin_frame(api, ctrl, frame);
                    } else {
                        tracing::debug!(id = frame.id, "LIN frame dropped: no controller");
                    }
                }
                Some(BusType::FlexRay) => {
                    if let Some(ctrl) = self.flexray_controller {
                        send_flexray_frame(api, ctrl, frame);
                    } else {
                        tracing::debug!(id = frame.id, "FlexRay frame dropped: no controller");
                    }
                }
            }
        }
        Ok(())
    }

    fn on_start(&mut self) -> Result<(), RuntimeError> {
        tracing::debug!("SilKitBus: on_start");
        Ok(())
    }

    fn on_stop(&mut self) -> Result<(), RuntimeError> {
        tracing::debug!("SilKitBus: on_stop");
        Ok(())
    }
}

// ---------------------------------------------------------------------------
// Frame conversion helpers
// ---------------------------------------------------------------------------

/// Send a [`VecuFrame`] as a SIL Kit CAN frame.
#[allow(unsafe_code, clippy::cast_possible_truncation)]
pub(crate) fn send_can_frame(
    api: &SilKitApi,
    controller: *mut SilKit_CanController,
    frame: &VecuFrame,
) {
    let data_len = (frame.len as usize).min(frame.data.len());

    let byte_vec = SilKit_ByteVector {
        data: frame.data.as_ptr(),
        size: data_len as u32,
        pad0: 0,
    };

    let sk_frame = SilKit_CanFrame {
        struct_header: SilKit_StructHeader::v1(),
        id: frame.id,
        flags: 0,
        dlc: frame.len as u16,
        sdt: 0,
        vcid: 0,
        af: 0,
        data: byte_vec,
    };

    #[allow(unsafe_code)]
    unsafe {
        let rc = (api.can_controller_send_frame)(controller, &sk_frame, std::ptr::null_mut());
        if rc != SK_OK {
            tracing::warn!(
                rc,
                can_id = frame.id,
                "failed to send CAN frame via SIL Kit"
            );
        }
    }
}

/// Send a [`VecuFrame`] as a SIL Kit Ethernet frame.
#[allow(unsafe_code, clippy::cast_possible_truncation)]
pub(crate) fn send_eth_frame(
    api: &SilKitApi,
    controller: *mut SilKit_EthernetController,
    frame: &VecuFrame,
) {
    let data_len = (frame.len as usize).min(frame.data.len());

    let byte_vec = SilKit_ByteVector {
        data: frame.data.as_ptr(),
        size: data_len as u32,
        pad0: 0,
    };

    let mut sk_frame = SilKit_EthernetFrame {
        struct_header: SilKit_StructHeader::v1(),
        raw: byte_vec,
    };

    #[allow(unsafe_code)]
    unsafe {
        let rc =
            (api.ethernet_controller_send_frame)(controller, &mut sk_frame, std::ptr::null_mut());
        if rc != SK_OK {
            tracing::warn!(rc, id = frame.id, "failed to send ETH frame via SIL Kit");
        }
    }
}

/// Send a [`VecuFrame`] as a SIL Kit LIN frame (master response).
#[allow(unsafe_code, clippy::cast_possible_truncation)]
pub(crate) fn send_lin_frame(
    api: &SilKitApi,
    controller: *mut SilKit_LinController,
    frame: &VecuFrame,
) {
    let data_len = (frame.len as usize).min(8);
    let mut data = [0u8; 8];
    data[..data_len].copy_from_slice(&frame.data[..data_len]);

    let sk_frame = SilKit_LinFrame {
        struct_header: SilKit_StructHeader::v1(),
        id: (frame.id & 0x3F) as u8,
        checksum_model: SK_LIN_CHECKSUM_ENHANCED,
        data_length: data_len as u8,
        data,
    };

    #[allow(unsafe_code)]
    unsafe {
        let rc =
            (api.lin_controller_send_frame)(controller, &sk_frame, SK_LIN_FRAME_RESPONSE_MASTER);
        if rc != SK_OK {
            tracing::warn!(
                rc,
                lin_id = frame.id,
                "failed to send LIN frame via SIL Kit"
            );
        }
    }
}

/// Send a [`VecuFrame`] as a SIL Kit `FlexRay` TX buffer update.
///
/// The `VecuFrame.id` is interpreted as the TX buffer index.
#[allow(unsafe_code, clippy::cast_possible_truncation)]
pub(crate) fn send_flexray_frame(
    api: &SilKitApi,
    controller: *mut SilKit_FlexrayController,
    frame: &VecuFrame,
) {
    let data_len = (frame.len as usize).min(frame.data.len());

    let byte_vec = SilKit_ByteVector {
        data: frame.data.as_ptr(),
        size: data_len as u32,
        pad0: 0,
    };

    let update = SilKit_FlexrayTxBufferUpdate {
        struct_header: SilKit_StructHeader::v1(),
        tx_buffer_index: frame.id as u16,
        payload_data_valid: 1,
        payload: byte_vec,
    };

    #[allow(unsafe_code)]
    unsafe {
        let rc = (api.flexray_controller_update_tx_buffer)(controller, &update);
        if rc != SK_OK {
            tracing::warn!(
                rc,
                tx_buf = frame.id,
                "failed to update FlexRay TX buffer via SIL Kit"
            );
        }
    }
}
