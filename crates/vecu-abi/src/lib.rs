//! ABI definitions for the vECU execution system (ADR‑001 / ADR‑003).
//!
//! Every plugin exports exactly **one** symbol: [`PLUGIN_ENTRY_SYMBOL`]
//! (`vecu_get_api`). The loader calls it to obtain a [`VecuPluginApi`]
//! function table and then drives the simulation via that table.
//!
//! All public types are `#[repr(C)]` and must remain ABI‑stable.

use bytemuck::{Pod, Zeroable};
use serde::{Deserialize, Serialize};

// ---------------------------------------------------------------------------
// Version
// ---------------------------------------------------------------------------

/// ABI major version – bumped on breaking changes.
pub const ABI_VERSION_MAJOR: u16 = 1;
/// ABI minor version – bumped on backward‑compatible additions.
pub const ABI_VERSION_MINOR: u16 = 1;

/// Pack major/minor into a single `u32` for the ABI boundary.
#[must_use]
pub const fn pack_version(major: u16, minor: u16) -> u32 {
    ((major as u32) << 16) | (minor as u32)
}

/// Unpack a version `u32` into `(major, minor)`.
#[must_use]
pub const fn unpack_version(packed: u32) -> (u16, u16) {
    #[allow(clippy::cast_possible_truncation)]
    let minor = packed as u16;
    #[allow(clippy::cast_possible_truncation)]
    let major = (packed >> 16) as u16;
    (major, minor)
}

/// The packed ABI version constant used by the loader.
pub const ABI_VERSION: u32 = pack_version(ABI_VERSION_MAJOR, ABI_VERSION_MINOR);

// ---------------------------------------------------------------------------
// Symbol name
// ---------------------------------------------------------------------------

/// Name of the single exported symbol every plugin must provide (NUL‑terminated).
pub const PLUGIN_ENTRY_SYMBOL: &[u8] = b"vecu_get_api\0";

// ---------------------------------------------------------------------------
// Status codes  (C ABI return values)
// ---------------------------------------------------------------------------

/// Status codes returned across the C ABI boundary.
///
/// Convention: 0 = success, negative = error.
pub mod status {
    /// Success.
    pub const OK: i32 = 0;
    /// The requested ABI version is not supported by the plugin.
    pub const VERSION_MISMATCH: i32 = -1;
    /// A null or otherwise invalid argument was passed.
    pub const INVALID_ARGUMENT: i32 = -2;
    /// Module initialisation failed.
    pub const INIT_FAILED: i32 = -3;
    /// Requested capability is not supported.
    pub const NOT_SUPPORTED: i32 = -4;
    /// Generic module‑level error.
    pub const MODULE_ERROR: i32 = -5;
}

// ---------------------------------------------------------------------------
// Capability flags
// ---------------------------------------------------------------------------

/// No optional capabilities.
pub const CAP_NONE: u32 = 0;
/// Plugin supports frame I/O (`push_frame` / `poll_frame`).
pub const CAP_FRAME_IO: u32 = 1 << 0;
/// Plugin supports diagnostic requests.
pub const CAP_DIAGNOSTICS: u32 = 1 << 1;
/// HSM supports seed/key challenge‑response.
pub const CAP_HSM_SEED_KEY: u32 = 1 << 2;
/// HSM supports sign/verify.
pub const CAP_SIGN_VERIFY: u32 = 1 << 3;
/// HSM supports AES‑128 encrypt/decrypt.
pub const CAP_HSM_ENCRYPT: u32 = 1 << 4;
/// HSM supports random number generation.
pub const CAP_HSM_RNG: u32 = 1 << 5;
/// HSM supports cryptographic hash (SHA‑256).
pub const CAP_HSM_HASH: u32 = 1 << 6;

/// SHA‑256 algorithm identifier for `hsm_hash`.
pub const HSM_HASH_SHA256: u32 = 0;
/// SHA‑256 digest size in bytes.
pub const SHA256_DIGEST_SIZE: usize = 32;

// ---------------------------------------------------------------------------
// Module kind
// ---------------------------------------------------------------------------

/// Discriminant for plugin module type.
#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum ModuleKind {
    /// Application logic module.
    Appl = 0,
    /// Hardware Security Module simulation.
    Hsm = 1,
}

impl ModuleKind {
    /// Convert from raw `u32` discriminant.
    #[must_use]
    pub fn from_raw(raw: u32) -> Option<Self> {
        match raw {
            0 => Some(Self::Appl),
            1 => Some(Self::Hsm),
            _ => None,
        }
    }
}

// ---------------------------------------------------------------------------
// Plugin entry‑point signature
// ---------------------------------------------------------------------------

/// Signature of the single exported symbol `vecu_get_api`.
///
/// # Safety
///
/// `out_api` must point to a valid, writeable [`VecuPluginApi`].
pub type VecuGetApiFn =
    unsafe extern "C" fn(requested_version: u32, out_api: *mut VecuPluginApi) -> i32;

// ---------------------------------------------------------------------------
// Plugin API table
// ---------------------------------------------------------------------------

/// Function table populated by the plugin via `vecu_get_api`.
///
/// `Option<fn>` fields are nullable – `None` means "not provided".
#[repr(C)]
pub struct VecuPluginApi {
    /// ABI version the plugin was compiled against (packed).
    pub abi_version: u32,
    /// Module kind discriminant (see [`ModuleKind`]).
    pub module_kind: u32,
    /// Bitfield of `CAP_*` flags.
    pub capabilities: u32,
    /// Reserved – must be zero.
    pub reserved: u32,

    // -- Common lifecycle ------------------------------------------------
    /// Called once after loading.
    ///
    /// # Safety
    ///
    /// `ctx` must point to a valid [`VecuRuntimeContext`].
    pub init: Option<unsafe extern "C" fn(ctx: *const VecuRuntimeContext) -> i32>,
    /// Called once before unloading.
    pub shutdown: Option<extern "C" fn()>,

    // -- Simulation ------------------------------------------------------
    /// Called every scheduled tick.
    ///
    /// # Safety
    ///
    /// Must only be called after a successful `init`.
    pub step: Option<unsafe extern "C" fn(tick: u64) -> i32>,

    // -- Frame I/O (optional, requires [`CAP_FRAME_IO`]) -----------------
    /// Deliver an inbound frame to the plugin.
    ///
    /// # Safety
    ///
    /// `frame` must point to a valid [`VecuFrame`].
    pub push_frame: Option<unsafe extern "C" fn(frame: *const VecuFrame) -> i32>,
    /// Collect an outbound frame from the plugin.
    /// Returns [`status::OK`] when a frame was written, [`status::NOT_SUPPORTED`]
    /// when no frame is available.
    ///
    /// # Safety
    ///
    /// `frame` must point to a writeable [`VecuFrame`].
    pub poll_frame: Option<unsafe extern "C" fn(frame: *mut VecuFrame) -> i32>,

    // -- HSM Security (optional, requires [`CAP_HSM_SEED_KEY`]) -----------
    /// Generate a seed for `SecurityAccess` challenge.
    ///
    /// Writes the seed into `out_seed` (max [`HSM_BUF_SIZE`] bytes) and
    /// sets `*out_len` to the actual seed length.
    ///
    /// # Safety
    ///
    /// `out_seed` must point to a writeable buffer of at least [`HSM_BUF_SIZE`] bytes.
    /// `out_len` must point to a writeable `u32`.
    pub seed: Option<unsafe extern "C" fn(out_seed: *mut u8, out_len: *mut u32) -> i32>,
    /// Validate a key response against the last generated seed.
    ///
    /// Returns [`status::OK`] on success, [`status::MODULE_ERROR`] on mismatch.
    ///
    /// # Safety
    ///
    /// `key_buf` must point to a valid buffer of at least `key_len` bytes.
    pub key: Option<unsafe extern "C" fn(key_buf: *const u8, key_len: u32) -> i32>,

    // -- HSM Crypto (optional, requires [`CAP_SIGN_VERIFY`]) --------------
    /// Sign `data` and write the signature into `out_sig`.
    ///
    /// # Safety
    ///
    /// `data` must point to `data_len` readable bytes.
    /// `out_sig` must point to a writeable buffer of at least [`HSM_BUF_SIZE`] bytes.
    /// `out_sig_len` must point to a writeable `u32`.
    pub sign: Option<
        unsafe extern "C" fn(
            data: *const u8,
            data_len: u32,
            out_sig: *mut u8,
            out_sig_len: *mut u32,
        ) -> i32,
    >,
    /// Verify a `signature` over `data`.
    ///
    /// Returns [`status::OK`] if valid, [`status::MODULE_ERROR`] if invalid.
    ///
    /// # Safety
    ///
    /// `data` must point to `data_len` readable bytes.
    /// `sig` must point to `sig_len` readable bytes.
    pub verify: Option<
        unsafe extern "C" fn(data: *const u8, data_len: u32, sig: *const u8, sig_len: u32) -> i32,
    >,

    // -- HSM SHE Crypto (optional, requires [`CAP_HSM_ENCRYPT`]) -------------
    /// AES‑128 encrypt (ECB or CBC).
    ///
    /// `mode`: [`SHE_MODE_ECB`] or [`SHE_MODE_CBC`].\
    /// `iv`: ignored for ECB, must point to 16 bytes for CBC.
    ///
    /// # Safety
    ///
    /// `data` must point to `data_len` readable bytes (multiple of 16).
    /// `iv` must be null (ECB) or point to 16 readable bytes (CBC).
    /// `out` must point to a writeable buffer of at least `data_len` bytes.
    /// `out_len` must point to a writeable `u32`.
    pub encrypt: Option<
        unsafe extern "C" fn(
            key_slot: u32,
            mode: u32,
            data: *const u8,
            data_len: u32,
            iv: *const u8,
            out: *mut u8,
            out_len: *mut u32,
        ) -> i32,
    >,
    /// AES‑128 decrypt (ECB or CBC).
    ///
    /// See [`VecuPluginApi::encrypt`] for parameter semantics.
    ///
    /// # Safety
    ///
    /// Same requirements as [`encrypt`](VecuPluginApi::encrypt).
    pub decrypt: Option<
        unsafe extern "C" fn(
            key_slot: u32,
            mode: u32,
            data: *const u8,
            data_len: u32,
            iv: *const u8,
            out: *mut u8,
            out_len: *mut u32,
        ) -> i32,
    >,
    /// Generate an AES‑128‑CMAC over `data`.
    ///
    /// Writes 16‑byte MAC to `out_mac`.
    ///
    /// # Safety
    ///
    /// `data` must point to `data_len` readable bytes.
    /// `out_mac` must point to a writeable buffer of at least 16 bytes.
    /// `out_mac_len` must point to a writeable `u32`.
    pub generate_mac: Option<
        unsafe extern "C" fn(
            key_slot: u32,
            data: *const u8,
            data_len: u32,
            out_mac: *mut u8,
            out_mac_len: *mut u32,
        ) -> i32,
    >,
    /// Verify an AES‑128‑CMAC over `data`.
    ///
    /// Returns [`status::OK`] if MAC matches, [`status::MODULE_ERROR`] if not.
    ///
    /// # Safety
    ///
    /// `data` must point to `data_len` readable bytes.
    /// `mac` must point to `mac_len` readable bytes.
    pub verify_mac: Option<
        unsafe extern "C" fn(
            key_slot: u32,
            data: *const u8,
            data_len: u32,
            mac: *const u8,
            mac_len: u32,
        ) -> i32,
    >,
    /// Load a plain key into a key slot.
    ///
    /// # Safety
    ///
    /// `key_data` must point to `key_len` readable bytes.
    pub load_key: Option<unsafe extern "C" fn(slot: u32, key_data: *const u8, key_len: u32) -> i32>,
    /// Generate cryptographically secure random bytes.
    ///
    /// # Safety
    ///
    /// `out_buf` must point to a writeable buffer of at least `buf_len` bytes.
    pub rng: Option<unsafe extern "C" fn(out_buf: *mut u8, buf_len: u32) -> i32>,
    /// Compute a cryptographic hash (e.g. SHA‑256).
    ///
    /// `algorithm`: [`HSM_HASH_SHA256`] (0) for SHA‑256.
    ///
    /// # Safety
    ///
    /// `data` must point to `data_len` readable bytes.
    /// `out` must point to a writeable buffer of at least 32 bytes (SHA‑256).
    /// `out_len` must point to a writeable `u32`.
    pub hash: Option<
        unsafe extern "C" fn(
            algorithm: u32,
            data: *const u8,
            data_len: u32,
            out: *mut u8,
            out_len: *mut u32,
        ) -> i32,
    >,
}

impl VecuPluginApi {
    /// Create a zeroed (empty) API table suitable for passing to `vecu_get_api`.
    #[must_use]
    pub fn zeroed() -> Self {
        // SAFETY: All‑zero bit pattern is valid for this repr(C) struct:
        // integers → 0, Option<fn> → None (null pointer).
        #[allow(unsafe_code)]
        unsafe {
            core::mem::zeroed()
        }
    }
}

impl std::fmt::Debug for VecuPluginApi {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("VecuPluginApi")
            .field("abi_version", &self.abi_version)
            .field("module_kind", &ModuleKind::from_raw(self.module_kind))
            .field("capabilities", &format_args!("0x{:08x}", self.capabilities))
            .field("init", &self.init.is_some())
            .field("shutdown", &self.shutdown.is_some())
            .field("step", &self.step.is_some())
            .field("push_frame", &self.push_frame.is_some())
            .field("poll_frame", &self.poll_frame.is_some())
            .field("seed", &self.seed.is_some())
            .field("key", &self.key.is_some())
            .field("sign", &self.sign.is_some())
            .field("verify", &self.verify.is_some())
            .field("encrypt", &self.encrypt.is_some())
            .field("decrypt", &self.decrypt.is_some())
            .field("generate_mac", &self.generate_mac.is_some())
            .field("verify_mac", &self.verify_mac.is_some())
            .field("load_key", &self.load_key.is_some())
            .field("rng", &self.rng.is_some())
            .field("hash", &self.hash.is_some())
            .finish_non_exhaustive()
    }
}

// ---------------------------------------------------------------------------
// Runtime context (Loader → Plugin)
// ---------------------------------------------------------------------------

/// Context the loader passes to the plugin during init.
///
/// The plugin stores these values internally and uses them during `step`.
/// The loader is the **sole owner** of the shared memory.
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct VecuRuntimeContext {
    /// Base pointer to the shared memory region.
    pub shm_base: *mut u8,
    /// Size of the shared memory region in bytes.
    pub shm_size: u32,
    /// Padding for 8‑byte alignment.
    pub pad0: u32,
    /// Simulation tick interval in microseconds.
    pub tick_interval_us: u64,
    /// Optional logging callback provided by the loader.
    ///
    /// # Safety
    ///
    /// `msg` must be a valid NUL‑terminated C string.
    pub log_fn: Option<unsafe extern "C" fn(level: i32, msg: *const core::ffi::c_char)>,
    /// Optional pointer to the HSM plugin's [`VecuPluginApi`].
    ///
    /// When non‑null the APPL plugin can read HSM function pointers
    /// (encrypt, decrypt, MAC, seed, key, rng, hash) and inject them
    /// into the `vecu_base_context_t` passed to `Base_Init`.
    ///
    /// # Safety
    ///
    /// Must be null or point to a valid, fully‑initialised [`VecuPluginApi`]
    /// that outlives the simulation.
    pub hsm_api: *const VecuPluginApi,
}

// ---------------------------------------------------------------------------
// Bus type discriminator
// ---------------------------------------------------------------------------

/// Communication bus type for [`VecuFrame`] routing.
///
/// Discriminates which SIL Kit controller (or physical bus) a frame
/// belongs to.
#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum BusType {
    /// Controller Area Network.
    Can = 0,
    /// Automotive Ethernet.
    Eth = 1,
    /// Local Interconnect Network.
    Lin = 2,
    /// `FlexRay`.
    FlexRay = 3,
}

impl BusType {
    /// Convert a raw `u32` to a [`BusType`], returning `None` for unknown values.
    #[must_use]
    pub fn from_raw(raw: u32) -> Option<Self> {
        match raw {
            0 => Some(Self::Can),
            1 => Some(Self::Eth),
            2 => Some(Self::Lin),
            3 => Some(Self::FlexRay),
            _ => None,
        }
    }
}

// ---------------------------------------------------------------------------
// Frame type
// ---------------------------------------------------------------------------

/// Maximum payload bytes in a [`VecuFrame`].
///
/// Sized to accommodate full Ethernet frames (1518 bytes incl. header).
/// Also covers `FlexRay` (≤ 254), CAN‑FD (≤ 64), and LIN (≤ 8).
pub const MAX_FRAME_DATA: usize = 1536;

/// A communication frame for inter‑module / bus I/O.
#[repr(C)]
#[derive(Debug, Clone, Copy, Pod, Zeroable)]
pub struct VecuFrame {
    /// Frame / message identifier (e.g. CAN ID).
    pub id: u32,
    /// Number of valid bytes in [`data`](VecuFrame::data) (≤ [`MAX_FRAME_DATA`]).
    pub len: u32,
    /// Bus type discriminator (see [`BusType`]).
    ///
    /// Use [`BusType::from_raw`] to interpret. Defaults to `0` (`Can`).
    pub bus_type: u32,
    /// Reserved padding (must be zero).
    pub pad0: u32,
    /// Payload bytes.
    pub data: [u8; MAX_FRAME_DATA],
    /// Monotonic tick at which the frame was created.
    pub timestamp: u64,
}

static_assertions::assert_eq_size!(VecuFrame, [u8; 1560]);

impl VecuFrame {
    /// Create an empty frame with the given `id`.
    #[must_use]
    pub fn new(id: u32) -> Self {
        Self {
            id,
            len: 0,
            bus_type: 0,
            pad0: 0,
            data: [0; MAX_FRAME_DATA],
            timestamp: 0,
        }
    }

    /// Create a frame with payload.
    ///
    /// # Panics
    ///
    /// Panics if `payload.len()` exceeds [`MAX_FRAME_DATA`].
    #[must_use]
    pub fn with_data(id: u32, payload: &[u8], tick: u64) -> Self {
        Self::with_bus_data(BusType::Can, id, payload, tick)
    }

    /// Create a frame with bus type, payload and timestamp.
    ///
    /// # Panics
    ///
    /// Panics if `payload.len()` exceeds [`MAX_FRAME_DATA`].
    #[must_use]
    pub fn with_bus_data(bus: BusType, id: u32, payload: &[u8], tick: u64) -> Self {
        assert!(payload.len() <= MAX_FRAME_DATA, "frame payload too large");
        let mut frame = Self::new(id);
        frame.bus_type = bus as u32;
        frame.data[..payload.len()].copy_from_slice(payload);
        #[allow(clippy::cast_possible_truncation)]
        {
            frame.len = payload.len() as u32;
        }
        frame.timestamp = tick;
        frame
    }

    /// Return the valid payload slice.
    #[must_use]
    pub fn payload(&self) -> &[u8] {
        &self.data[..self.len as usize]
    }
}

// ---------------------------------------------------------------------------
// Shared‑memory header  (ADR‑003: vecu_shm_header_t)
// ---------------------------------------------------------------------------

/// Magic value for the shared‑memory header: ASCII `VECU` in little‑endian.
pub const SHM_MAGIC: u32 = 0x5543_4556; // 'V'=0x56,'E'=0x45,'C'=0x43,'U'=0x55

/// Header at byte offset 0 of the shared‑memory region.
///
/// Layout per ADR‑003. Must stay ABI‑stable; new fields are appended only
/// with a version bump.
#[repr(C)]
#[derive(Debug, Clone, Copy, Pod, Zeroable)]
pub struct VecuShmHeader {
    /// Magic value – must equal [`SHM_MAGIC`].
    pub magic: u32,
    /// ABI major version.
    pub abi_major: u16,
    /// ABI minor version.
    pub abi_minor: u16,
    /// Total size of the shared‑memory region in bytes.
    pub total_size: u64,

    // -- Offsets from base -----------------------------------------------
    /// Byte offset to the RX frame queue (Loader → APPL).
    pub off_rx_frames: u64,
    /// Byte offset to the TX frame queue (APPL → Loader).
    pub off_tx_frames: u64,
    /// Byte offset to the diagnostic mailbox.
    pub off_diag_mb: u64,
    /// Byte offset to the variable / state block.
    pub off_vars: u64,

    // -- Section sizes ---------------------------------------------------
    /// Size of the RX frame queue region in bytes.
    pub size_rx_frames: u32,
    /// Size of the TX frame queue region in bytes.
    pub size_tx_frames: u32,
    /// Size of the diagnostic mailbox region in bytes.
    pub size_diag_mb: u32,
    /// Size of the variable / state block in bytes.
    pub size_vars: u32,

    /// Flags – reserved for future use.
    pub flags: u32,
    /// Reserved – must be zero.
    pub reserved: u32,
}

static_assertions::assert_eq_size!(VecuShmHeader, [u8; 72]);

// ---------------------------------------------------------------------------
// Frame‑queue header (used inside RX / TX regions)
// ---------------------------------------------------------------------------

/// Ring‑buffer header preceding the frame array in each queue region.
///
/// Single‑writer / single‑reader – no locks required (ADR‑003).
#[repr(C)]
#[derive(Debug, Clone, Copy, Pod, Zeroable)]
pub struct FrameQueueHeader {
    /// Write index (modulo capacity).
    pub write_idx: u32,
    /// Read index (modulo capacity).
    pub read_idx: u32,
    /// Maximum number of frames in this queue.
    pub capacity: u32,
    /// Reserved – must be zero.
    pub reserved: u32,
}

static_assertions::assert_eq_size!(FrameQueueHeader, [u8; 16]);

// ---------------------------------------------------------------------------
// Diagnostic mailbox (ADR‑003)
// ---------------------------------------------------------------------------

/// Size of the diagnostic mailbox data area in bytes.
pub const DIAG_DATA_SIZE: usize = 496;

/// Diagnostic mailbox for indirect APPL ↔ HSM communication.
///
/// APPL writes a request, HSM processes it during its `step`, result is
/// deposited in the same region. The loader guarantees tick ordering.
#[repr(C)]
#[derive(Debug, Clone, Copy, Pod, Zeroable)]
pub struct DiagMailbox {
    /// Non‑zero when a request is pending.
    pub request_pending: u32,
    /// Non‑zero when a response is ready.
    pub response_ready: u32,
    /// Application‑defined request type discriminant.
    pub request_type: u32,
    /// Response status code.
    pub response_status: u32,
    /// Payload bytes.
    pub data: [u8; DIAG_DATA_SIZE],
}

static_assertions::assert_eq_size!(DiagMailbox, [u8; 512]);

// ---------------------------------------------------------------------------
// Default sizes (configurable via loader config)
// ---------------------------------------------------------------------------

/// Default number of frames per RX / TX queue.
pub const DEFAULT_QUEUE_CAPACITY: u32 = 64;
/// Default size of the variable / state block in bytes.
pub const DEFAULT_VARS_SIZE: u32 = 4096;

/// Maximum buffer size for HSM seed / key / signature operations.
pub const HSM_BUF_SIZE: usize = 256;

/// Number of key slots in the SHE‑compatible key store.
pub const SHE_NUM_KEY_SLOTS: usize = 20;
/// AES‑128 key size in bytes.
pub const AES128_KEY_SIZE: usize = 16;
/// AES‑128 block size in bytes.
pub const AES128_BLOCK_SIZE: usize = 16;
/// AES‑128‑CMAC tag size in bytes.
pub const AES128_CMAC_SIZE: usize = 16;

/// SHE cipher mode: ECB.
pub const SHE_MODE_ECB: u32 = 0;
/// SHE cipher mode: CBC.
pub const SHE_MODE_CBC: u32 = 1;

// ---------------------------------------------------------------------------
// Result / error types
// ---------------------------------------------------------------------------

/// Standard result type for ABI‑boundary operations.
pub type AbiResult<T> = Result<T, AbiError>;

/// Errors that can occur at the ABI boundary.
#[derive(Debug, Clone, PartialEq, Eq, thiserror::Error)]
pub enum AbiError {
    /// Module ABI version does not match the loader.
    #[error("ABI version mismatch: expected {expected:#010x}, got {got:#010x}")]
    VersionMismatch {
        /// Expected version (packed).
        expected: u32,
        /// Actual version reported by the module (packed).
        got: u32,
    },
    /// The required `vecu_get_api` symbol was not found.
    #[error("missing entry‑point symbol: vecu_get_api")]
    MissingEntryPoint,
    /// Module returned a non‑zero error code.
    #[error("module error code: {0}")]
    ModuleError(i32),
    /// A required function pointer is `None` in the plugin API table.
    #[error("required function not provided: {0}")]
    MissingFunction(String),
    /// Plugin reported an unexpected module kind.
    #[error("unknown module kind: {0}")]
    UnknownModuleKind(u32),
    /// Shared‑memory magic validation failed.
    #[error("invalid SHM magic: expected {expected:#010x}, got {got:#010x}")]
    InvalidShmMagic {
        /// Expected magic.
        expected: u32,
        /// Actual magic found.
        got: u32,
    },
    /// An offset in the SHM header exceeds the total region size.
    #[error("SHM offset out of bounds: offset {offset} + size {size} > total {total}")]
    ShmOffsetOutOfBounds {
        /// Byte offset.
        offset: u64,
        /// Section size.
        size: u64,
        /// Total SHM size.
        total: u64,
    },
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn version_pack_unpack_round_trip() {
        let packed = pack_version(1, 2);
        assert_eq!(unpack_version(packed), (1, 2));
    }

    #[test]
    fn abi_version_constant_matches() {
        let (major, minor) = unpack_version(ABI_VERSION);
        assert_eq!(major, ABI_VERSION_MAJOR);
        assert_eq!(minor, ABI_VERSION_MINOR);
    }

    #[test]
    fn module_kind_round_trip() {
        assert_eq!(ModuleKind::from_raw(0), Some(ModuleKind::Appl));
        assert_eq!(ModuleKind::from_raw(1), Some(ModuleKind::Hsm));
        assert_eq!(ModuleKind::from_raw(99), None);
    }

    #[test]
    fn plugin_api_zeroed_is_empty() {
        let api = VecuPluginApi::zeroed();
        assert_eq!(api.abi_version, 0);
        assert_eq!(api.module_kind, 0);
        assert_eq!(api.capabilities, 0);
        assert!(api.init.is_none());
        assert!(api.shutdown.is_none());
        assert!(api.step.is_none());
        assert!(api.push_frame.is_none());
        assert!(api.poll_frame.is_none());
    }

    #[test]
    fn plugin_api_debug_format() {
        let api = VecuPluginApi::zeroed();
        let dbg = format!("{api:?}");
        assert!(dbg.contains("VecuPluginApi"));
    }

    #[test]
    fn frame_size_is_1560_bytes() {
        assert_eq!(core::mem::size_of::<VecuFrame>(), 1560);
    }

    #[test]
    fn frame_new_is_empty() {
        let f = VecuFrame::new(0x100);
        assert_eq!(f.id, 0x100);
        assert_eq!(f.len, 0);
        assert_eq!(f.payload(), &[]);
    }

    #[test]
    fn frame_with_data_stores_payload() {
        let payload = [0xDE, 0xAD, 0xBE, 0xEF];
        let f = VecuFrame::with_data(0x42, &payload, 10);
        assert_eq!(f.id, 0x42);
        assert_eq!(f.len, 4);
        assert_eq!(f.bus_type, BusType::Can as u32);
        assert_eq!(f.timestamp, 10);
        assert_eq!(f.payload(), &payload);
    }

    #[test]
    fn frame_with_bus_data_sets_bus_type() {
        let payload = [1, 2, 3];
        let f = VecuFrame::with_bus_data(BusType::Eth, 0x10, &payload, 5);
        assert_eq!(f.bus_type, BusType::Eth as u32);
        assert_eq!(BusType::from_raw(f.bus_type), Some(BusType::Eth));
        assert_eq!(f.payload(), &payload);
    }

    #[test]
    fn bus_type_from_raw() {
        assert_eq!(BusType::from_raw(0), Some(BusType::Can));
        assert_eq!(BusType::from_raw(1), Some(BusType::Eth));
        assert_eq!(BusType::from_raw(2), Some(BusType::Lin));
        assert_eq!(BusType::from_raw(3), Some(BusType::FlexRay));
        assert_eq!(BusType::from_raw(99), None);
    }

    #[test]
    fn frame_new_defaults_to_can() {
        let f = VecuFrame::new(0);
        assert_eq!(f.bus_type, 0);
        assert_eq!(BusType::from_raw(f.bus_type), Some(BusType::Can));
    }

    #[test]
    fn shm_header_size() {
        assert_eq!(core::mem::size_of::<VecuShmHeader>(), 72);
    }

    #[test]
    fn shm_header_is_pod() {
        let h = VecuShmHeader::zeroed();
        let bytes = bytemuck::bytes_of(&h);
        assert_eq!(bytes.len(), 72);
    }

    #[test]
    fn frame_queue_header_size() {
        assert_eq!(core::mem::size_of::<FrameQueueHeader>(), 16);
    }

    #[test]
    fn diag_mailbox_size() {
        assert_eq!(core::mem::size_of::<DiagMailbox>(), 512);
    }

    #[test]
    fn abi_error_display() {
        let err = AbiError::VersionMismatch {
            expected: pack_version(1, 0),
            got: pack_version(2, 0),
        };
        let msg = err.to_string();
        assert!(msg.contains("mismatch"));
    }

    #[test]
    fn capability_flags_are_distinct() {
        let all = [
            CAP_FRAME_IO,
            CAP_DIAGNOSTICS,
            CAP_HSM_SEED_KEY,
            CAP_SIGN_VERIFY,
            CAP_HSM_ENCRYPT,
            CAP_HSM_RNG,
            CAP_HSM_HASH,
        ];
        for (i, a) in all.iter().enumerate() {
            for b in &all[i + 1..] {
                assert_eq!(a & b, 0, "capability flags overlap: {a:#x} & {b:#x}");
            }
        }
    }

    #[test]
    fn she_constants() {
        assert_eq!(AES128_KEY_SIZE, 16);
        assert_eq!(AES128_BLOCK_SIZE, 16);
        assert_eq!(AES128_CMAC_SIZE, 16);
        assert_eq!(SHE_NUM_KEY_SLOTS, 20);
        assert_eq!(SHE_MODE_ECB, 0);
        assert_eq!(SHE_MODE_CBC, 1);
    }

    #[test]
    fn shm_magic_value() {
        let bytes = SHM_MAGIC.to_le_bytes();
        assert_eq!(&bytes, b"VECU");
    }
}

#[cfg(test)]
mod proptests {
    use super::*;
    use proptest::prelude::*;

    proptest! {
        #[test]
        fn version_pack_unpack_any(major in 0_u16..=u16::MAX, minor in 0_u16..=u16::MAX) {
            let packed = pack_version(major, minor);
            let (m, n) = unpack_version(packed);
            prop_assert_eq!(m, major);
            prop_assert_eq!(n, minor);
        }

        #[test]
        fn module_kind_from_raw_is_total(raw in 0_u32..100) {
            let result = ModuleKind::from_raw(raw);
            match raw {
                0 => prop_assert_eq!(result, Some(ModuleKind::Appl)),
                1 => prop_assert_eq!(result, Some(ModuleKind::Hsm)),
                _ => prop_assert_eq!(result, None),
            }
        }

        #[test]
        fn frame_payload_slice_matches_len(len in 0_usize..=MAX_FRAME_DATA) {
            let data = vec![0xAB_u8; len];
            let f = VecuFrame::with_data(1, &data, 0);
            prop_assert_eq!(f.payload().len(), len);
        }
    }
}
