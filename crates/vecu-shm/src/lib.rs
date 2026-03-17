//! Shared‑memory manager for the vECU execution system (ADR‑003).
//!
//! Implements the full ADR‑003 layout:
//!
//! ```text
//! [ VecuShmHeader ][ RX Frame Queue ][ TX Frame Queue ][ DiagMailbox ][ Vars ]
//! ```
//!
//! The **loader** is the sole owner of the shared‑memory region.
//! Modules receive a raw base pointer + size via [`VecuRuntimeContext`](vecu_abi::VecuRuntimeContext).

use std::path::Path;

use bytemuck::bytes_of;
use memmap2::MmapMut;
use vecu_abi::{
    AbiError, AbiResult, DiagMailbox, FrameQueueHeader, VecuFrame, VecuShmHeader,
    ABI_VERSION_MAJOR, ABI_VERSION_MINOR, DEFAULT_QUEUE_CAPACITY, DEFAULT_VARS_SIZE, SHM_MAGIC,
};

// ---------------------------------------------------------------------------
// Layout helpers
// ---------------------------------------------------------------------------

/// Compute the byte size of a frame‑queue region (header + N frames).
const fn queue_region_size(capacity: u32) -> usize {
    core::mem::size_of::<FrameQueueHeader>() + capacity as usize * core::mem::size_of::<VecuFrame>()
}

/// Configuration for the shared‑memory layout sizes.
#[derive(Debug, Clone, Copy)]
pub struct ShmLayout {
    /// Number of frames in each queue (RX and TX).
    pub queue_capacity: u32,
    /// Size of the variable / state block in bytes.
    pub vars_size: u32,
}

impl Default for ShmLayout {
    fn default() -> Self {
        Self {
            queue_capacity: DEFAULT_QUEUE_CAPACITY,
            vars_size: DEFAULT_VARS_SIZE,
        }
    }
}

impl ShmLayout {
    /// Total byte size of the shared‑memory region.
    #[must_use]
    pub fn total_size(self) -> usize {
        let header = core::mem::size_of::<VecuShmHeader>();
        let rx = queue_region_size(self.queue_capacity);
        let tx = queue_region_size(self.queue_capacity);
        let diag = core::mem::size_of::<DiagMailbox>();
        let vars = self.vars_size as usize;
        header + rx + tx + diag + vars
    }

    /// Compute all section offsets.
    fn offsets(self) -> ShmOffsets {
        let header_size = core::mem::size_of::<VecuShmHeader>();
        let rx_size = queue_region_size(self.queue_capacity);
        let tx_size = queue_region_size(self.queue_capacity);
        let diag_size = core::mem::size_of::<DiagMailbox>();

        let off_rx = header_size;
        let off_tx = off_rx + rx_size;
        let off_diag = off_tx + tx_size;
        let off_vars = off_diag + diag_size;

        ShmOffsets {
            off_rx,
            off_tx,
            off_diag,
            off_vars,
            size_rx: rx_size,
            size_tx: tx_size,
            size_diag: diag_size,
            size_vars: self.vars_size as usize,
        }
    }
}

struct ShmOffsets {
    off_rx: usize,
    off_tx: usize,
    off_diag: usize,
    off_vars: usize,
    size_rx: usize,
    size_tx: usize,
    size_diag: usize,
    size_vars: usize,
}

// ---------------------------------------------------------------------------
// Storage backend
// ---------------------------------------------------------------------------

enum Storage {
    Mapped(MmapMut),
    Anonymous(Vec<u8>),
}

impl Storage {
    fn as_slice(&self) -> &[u8] {
        match self {
            Self::Mapped(m) => m.as_ref(),
            Self::Anonymous(v) => v.as_slice(),
        }
    }

    fn as_mut_slice(&mut self) -> &mut [u8] {
        match self {
            Self::Mapped(m) => m.as_mut(),
            Self::Anonymous(v) => v.as_mut_slice(),
        }
    }

    fn len(&self) -> usize {
        match self {
            Self::Mapped(m) => m.len(),
            Self::Anonymous(v) => v.len(),
        }
    }
}

// ---------------------------------------------------------------------------
// SharedMemory
// ---------------------------------------------------------------------------

/// Shared‑memory region implementing the ADR‑003 layout.
///
/// The loader creates this and passes a raw pointer to plugins via
/// [`VecuRuntimeContext`](vecu_abi::VecuRuntimeContext).
pub struct SharedMemory {
    storage: Storage,
    layout: ShmLayout,
}

impl SharedMemory {
    /// Create an anonymous (heap‑backed) shared‑memory region with default layout.
    #[must_use]
    pub fn anonymous() -> Self {
        Self::with_layout(ShmLayout::default())
    }

    /// Create an anonymous shared‑memory region with a custom layout.
    #[must_use]
    pub fn with_layout(layout: ShmLayout) -> Self {
        let total = layout.total_size();
        let buf = vec![0_u8; total];
        let mut shm = Self {
            storage: Storage::Anonymous(buf),
            layout,
        };
        shm.init_header();
        shm
    }

    /// Open or create a file‑backed shared‑memory region.
    ///
    /// # Errors
    ///
    /// Returns an error if the file cannot be created/opened or mapped.
    pub fn from_file(path: &Path, layout: ShmLayout) -> Result<Self, ShmError> {
        let file = std::fs::OpenOptions::new()
            .read(true)
            .write(true)
            .create(true)
            .truncate(false)
            .open(path)
            .map_err(ShmError::Io)?;

        let total = layout.total_size();
        let metadata = file.metadata().map_err(ShmError::Io)?;
        if metadata.len() == 0 {
            #[allow(clippy::cast_possible_truncation)]
            file.set_len(total as u64).map_err(ShmError::Io)?;
        }

        // SAFETY: We own the file and control all access through this struct.
        #[allow(unsafe_code)]
        let mmap = unsafe { MmapMut::map_mut(&file).map_err(ShmError::Io)? };

        let mut shm = Self {
            storage: Storage::Mapped(mmap),
            layout,
        };

        // Initialise header if magic is absent (fresh file).
        if shm.header().magic != SHM_MAGIC {
            shm.init_header();
        }

        Ok(shm)
    }

    /// Write the initial header into the region.
    fn init_header(&mut self) {
        let offsets = self.layout.offsets();
        let total = self.layout.total_size();
        let header = VecuShmHeader {
            magic: SHM_MAGIC,
            abi_major: ABI_VERSION_MAJOR,
            abi_minor: ABI_VERSION_MINOR,
            #[allow(clippy::cast_possible_truncation)]
            total_size: total as u64,
            off_rx_frames: offsets.off_rx as u64,
            off_tx_frames: offsets.off_tx as u64,
            off_diag_mb: offsets.off_diag as u64,
            off_vars: offsets.off_vars as u64,
            #[allow(clippy::cast_possible_truncation)]
            size_rx_frames: offsets.size_rx as u32,
            #[allow(clippy::cast_possible_truncation)]
            size_tx_frames: offsets.size_tx as u32,
            #[allow(clippy::cast_possible_truncation)]
            size_diag_mb: offsets.size_diag as u32,
            #[allow(clippy::cast_possible_truncation)]
            size_vars: offsets.size_vars as u32,
            flags: 0,
            reserved: 0,
        };
        let hdr_bytes = bytes_of(&header);
        self.storage.as_mut_slice()[..hdr_bytes.len()].copy_from_slice(hdr_bytes);

        // Initialise RX queue header.
        self.init_queue_header(offsets.off_rx);
        // Initialise TX queue header.
        self.init_queue_header(offsets.off_tx);
    }

    /// Write a zeroed `FrameQueueHeader` with the configured capacity.
    fn init_queue_header(&mut self, offset: usize) {
        let qh = FrameQueueHeader {
            write_idx: 0,
            read_idx: 0,
            capacity: self.layout.queue_capacity,
            reserved: 0,
        };
        let qh_bytes = bytes_of(&qh);
        self.storage.as_mut_slice()[offset..offset + qh_bytes.len()].copy_from_slice(qh_bytes);
    }

    // -- Accessors -------------------------------------------------------

    /// Return a reference to the SHM header.
    #[must_use]
    pub fn header(&self) -> &VecuShmHeader {
        let size = core::mem::size_of::<VecuShmHeader>();
        bytemuck::from_bytes(&self.storage.as_slice()[..size])
    }

    /// Raw base pointer and total size – for building [`VecuRuntimeContext`](vecu_abi::VecuRuntimeContext).
    pub fn raw_parts(&mut self) -> (*mut u8, u32) {
        let size = self.storage.len();
        let ptr = self.storage.as_mut_slice().as_mut_ptr();
        #[allow(clippy::cast_possible_truncation)]
        (ptr, size as u32)
    }

    /// Return the layout configuration.
    #[must_use]
    pub fn layout(&self) -> ShmLayout {
        self.layout
    }

    // -- Frame queue operations ------------------------------------------

    /// Push a frame into the RX queue (Loader → APPL).
    ///
    /// # Errors
    ///
    /// Returns [`ShmError::QueueFull`] if the queue is at capacity.
    #[allow(clippy::cast_possible_truncation)]
    pub fn rx_push(&mut self, frame: &VecuFrame) -> Result<(), ShmError> {
        let off = self.header().off_rx_frames as usize;
        self.queue_push(off, frame)
    }

    /// Pop a frame from the RX queue (APPL reads).
    #[must_use]
    #[allow(clippy::cast_possible_truncation)]
    pub fn rx_pop(&mut self) -> Option<VecuFrame> {
        let off = self.header().off_rx_frames as usize;
        self.queue_pop(off)
    }

    /// Push a frame into the TX queue (APPL → Loader).
    ///
    /// # Errors
    ///
    /// Returns [`ShmError::QueueFull`] if the queue is at capacity.
    #[allow(clippy::cast_possible_truncation)]
    pub fn tx_push(&mut self, frame: &VecuFrame) -> Result<(), ShmError> {
        let off = self.header().off_tx_frames as usize;
        self.queue_push(off, frame)
    }

    /// Pop a frame from the TX queue (Loader reads).
    #[must_use]
    #[allow(clippy::cast_possible_truncation)]
    pub fn tx_pop(&mut self) -> Option<VecuFrame> {
        let off = self.header().off_tx_frames as usize;
        self.queue_pop(off)
    }

    /// Generic ring‑buffer push.
    fn queue_push(&mut self, queue_offset: usize, frame: &VecuFrame) -> Result<(), ShmError> {
        let buf = self.storage.as_mut_slice();
        let qh: &mut FrameQueueHeader = bytemuck::from_bytes_mut(
            &mut buf[queue_offset..queue_offset + core::mem::size_of::<FrameQueueHeader>()],
        );
        let cap = qh.capacity;
        if qh.write_idx.wrapping_sub(qh.read_idx) >= cap {
            return Err(ShmError::QueueFull);
        }
        let slot = (qh.write_idx % cap) as usize;
        qh.write_idx = qh.write_idx.wrapping_add(1);
        let frame_size = core::mem::size_of::<VecuFrame>();
        let data_start =
            queue_offset + core::mem::size_of::<FrameQueueHeader>() + slot * frame_size;
        buf[data_start..data_start + frame_size].copy_from_slice(bytes_of(frame));
        Ok(())
    }

    /// Generic ring‑buffer pop.
    fn queue_pop(&mut self, queue_offset: usize) -> Option<VecuFrame> {
        let buf = self.storage.as_mut_slice();
        let qh: &mut FrameQueueHeader = bytemuck::from_bytes_mut(
            &mut buf[queue_offset..queue_offset + core::mem::size_of::<FrameQueueHeader>()],
        );
        if qh.read_idx == qh.write_idx {
            return None;
        }
        let cap = qh.capacity;
        let slot = (qh.read_idx % cap) as usize;
        qh.read_idx = qh.read_idx.wrapping_add(1);
        let frame_size = core::mem::size_of::<VecuFrame>();
        let data_start =
            queue_offset + core::mem::size_of::<FrameQueueHeader>() + slot * frame_size;
        let frame: &VecuFrame = bytemuck::from_bytes(&buf[data_start..data_start + frame_size]);
        Some(*frame)
    }

    // -- Diagnostic mailbox ----------------------------------------------

    /// Return a reference to the diagnostic mailbox.
    #[must_use]
    #[allow(clippy::cast_possible_truncation)]
    pub fn diag_mailbox(&self) -> &DiagMailbox {
        let off = self.header().off_diag_mb as usize;
        let size = core::mem::size_of::<DiagMailbox>();
        bytemuck::from_bytes(&self.storage.as_slice()[off..off + size])
    }

    /// Return a mutable reference to the diagnostic mailbox.
    #[allow(clippy::cast_possible_truncation)]
    pub fn diag_mailbox_mut(&mut self) -> &mut DiagMailbox {
        let off = self.header().off_diag_mb as usize;
        let size = core::mem::size_of::<DiagMailbox>();
        bytemuck::from_bytes_mut(&mut self.storage.as_mut_slice()[off..off + size])
    }

    // -- Variable / state block ------------------------------------------

    /// Return a read‑only slice of the variable / state block.
    #[must_use]
    #[allow(clippy::cast_possible_truncation)]
    pub fn vars(&self) -> &[u8] {
        let hdr = self.header();
        let off = hdr.off_vars as usize;
        let size = hdr.size_vars as usize;
        &self.storage.as_slice()[off..off + size]
    }

    /// Return a mutable slice of the variable / state block.
    #[allow(clippy::cast_possible_truncation)]
    pub fn vars_mut(&mut self) -> &mut [u8] {
        let off = self.header().off_vars as usize;
        let size = self.header().size_vars as usize;
        &mut self.storage.as_mut_slice()[off..off + size]
    }

    /// Raw pointer and size of the variable / state block.
    ///
    /// Used by `vecu-appl` to populate `vecu_base_context_t.shm_vars`.
    #[allow(clippy::cast_possible_truncation)]
    pub fn vars_raw_parts(&mut self) -> (*mut u8, u32) {
        let off = self.header().off_vars as usize;
        let size = self.header().size_vars;
        let ptr = self.storage.as_mut_slice()[off..].as_mut_ptr();
        (ptr, size)
    }

    // -- Validation ------------------------------------------------------

    /// Validate the SHM header: magic, ABI version, offsets within bounds.
    ///
    /// # Errors
    ///
    /// Returns [`AbiError`] variants for any validation failure.
    pub fn validate(&self) -> AbiResult<()> {
        let hdr = self.header();

        if hdr.magic != SHM_MAGIC {
            return Err(AbiError::InvalidShmMagic {
                expected: SHM_MAGIC,
                got: hdr.magic,
            });
        }

        if hdr.abi_major != ABI_VERSION_MAJOR {
            return Err(AbiError::VersionMismatch {
                expected: vecu_abi::pack_version(ABI_VERSION_MAJOR, ABI_VERSION_MINOR),
                got: vecu_abi::pack_version(hdr.abi_major, hdr.abi_minor),
            });
        }

        // Check offsets are within bounds.
        let total = hdr.total_size;
        for &(offset, size) in &[
            (hdr.off_rx_frames, u64::from(hdr.size_rx_frames)),
            (hdr.off_tx_frames, u64::from(hdr.size_tx_frames)),
            (hdr.off_diag_mb, u64::from(hdr.size_diag_mb)),
            (hdr.off_vars, u64::from(hdr.size_vars)),
        ] {
            if offset + size > total {
                return Err(AbiError::ShmOffsetOutOfBounds {
                    offset,
                    size,
                    total,
                });
            }
        }

        Ok(())
    }
}

// ---------------------------------------------------------------------------
// Errors
// ---------------------------------------------------------------------------

/// Errors specific to shared‑memory operations.
#[derive(Debug, thiserror::Error)]
pub enum ShmError {
    /// I/O error during file operations.
    #[error("shared memory I/O error: {0}")]
    Io(#[from] std::io::Error),
    /// ABI‑level error.
    #[error(transparent)]
    Abi(#[from] AbiError),
    /// Frame queue is full.
    #[error("frame queue is full")]
    QueueFull,
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn anonymous_creates_valid_header() {
        let shm = SharedMemory::anonymous();
        let hdr = shm.header();
        assert_eq!(hdr.magic, SHM_MAGIC);
        assert_eq!(hdr.abi_major, ABI_VERSION_MAJOR);
        assert_eq!(hdr.abi_minor, ABI_VERSION_MINOR);
    }

    #[test]
    fn validate_passes_for_fresh_region() {
        let shm = SharedMemory::anonymous();
        assert!(shm.validate().is_ok());
    }

    #[test]
    fn total_size_matches_header() {
        let shm = SharedMemory::anonymous();
        let hdr = shm.header();
        #[allow(clippy::cast_possible_truncation)]
        let total = hdr.total_size as usize;
        assert_eq!(total, ShmLayout::default().total_size());
    }

    #[test]
    fn rx_push_pop_round_trip() {
        let mut shm = SharedMemory::anonymous();
        let frame = VecuFrame::with_data(0x100, &[1, 2, 3], 42);
        shm.rx_push(&frame).unwrap();

        let popped = shm.rx_pop().unwrap();
        assert_eq!(popped.id, 0x100);
        assert_eq!(popped.payload(), &[1, 2, 3]);
        assert_eq!(popped.timestamp, 42);
    }

    #[test]
    fn tx_push_pop_round_trip() {
        let mut shm = SharedMemory::anonymous();
        let frame = VecuFrame::with_data(0x200, &[0xAA], 7);
        shm.tx_push(&frame).unwrap();

        let popped = shm.tx_pop().unwrap();
        assert_eq!(popped.id, 0x200);
        assert_eq!(popped.payload(), &[0xAA]);
    }

    #[test]
    fn pop_empty_queue_returns_none() {
        let mut shm = SharedMemory::anonymous();
        assert!(shm.rx_pop().is_none());
        assert!(shm.tx_pop().is_none());
    }

    #[test]
    fn queue_full_returns_error() {
        let layout = ShmLayout {
            queue_capacity: 2,
            vars_size: 64,
        };
        let mut shm = SharedMemory::with_layout(layout);
        let f = VecuFrame::new(1);
        shm.rx_push(&f).unwrap();
        shm.rx_push(&f).unwrap();
        let result = shm.rx_push(&f);
        assert!(matches!(result, Err(ShmError::QueueFull)));
    }

    #[test]
    fn queue_wraps_around() {
        let layout = ShmLayout {
            queue_capacity: 2,
            vars_size: 64,
        };
        let mut shm = SharedMemory::with_layout(layout);

        // Fill and drain twice to exercise wrap‑around.
        for round in 0_u32..3 {
            #[allow(clippy::cast_possible_truncation)]
            let r = round as u8;
            let f1 = VecuFrame::with_data(round, &[r], 0);
            let f2 = VecuFrame::with_data(round + 100, &[r + 1], 0);
            shm.rx_push(&f1).unwrap();
            shm.rx_push(&f2).unwrap();
            let p1 = shm.rx_pop().unwrap();
            let p2 = shm.rx_pop().unwrap();
            assert_eq!(p1.id, round);
            assert_eq!(p2.id, round + 100);
        }
    }

    #[test]
    fn diag_mailbox_read_write() {
        let mut shm = SharedMemory::anonymous();
        {
            let mb = shm.diag_mailbox_mut();
            mb.request_pending = 1;
            mb.request_type = 0x42;
            mb.data[0] = 0xAB;
        }
        let mb = shm.diag_mailbox();
        assert_eq!(mb.request_pending, 1);
        assert_eq!(mb.request_type, 0x42);
        assert_eq!(mb.data[0], 0xAB);
    }

    #[test]
    fn vars_block_read_write() {
        let mut shm = SharedMemory::anonymous();
        shm.vars_mut()[0] = 0xFF;
        shm.vars_mut()[1] = 0x01;
        assert_eq!(shm.vars()[0], 0xFF);
        assert_eq!(shm.vars()[1], 0x01);
    }

    #[test]
    fn raw_parts_returns_non_null() {
        let mut shm = SharedMemory::anonymous();
        let (ptr, size) = shm.raw_parts();
        assert!(!ptr.is_null());
        assert!(size > 0);
        assert_eq!(size as usize, ShmLayout::default().total_size());
    }

    #[test]
    fn offsets_are_within_bounds() {
        let shm = SharedMemory::anonymous();
        let hdr = shm.header();
        let total = hdr.total_size;
        assert!(hdr.off_rx_frames + u64::from(hdr.size_rx_frames) <= total);
        assert!(hdr.off_tx_frames + u64::from(hdr.size_tx_frames) <= total);
        assert!(hdr.off_diag_mb + u64::from(hdr.size_diag_mb) <= total);
        assert!(hdr.off_vars + u64::from(hdr.size_vars) <= total);
    }

    #[test]
    fn file_backed_round_trip() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("test.shm");

        {
            let mut shm = SharedMemory::from_file(&path, ShmLayout::default()).unwrap();
            let frame = VecuFrame::with_data(0x42, &[1, 2, 3], 0);
            shm.rx_push(&frame).unwrap();
        }

        {
            let mut shm = SharedMemory::from_file(&path, ShmLayout::default()).unwrap();
            let popped = shm.rx_pop().unwrap();
            assert_eq!(popped.id, 0x42);
            assert_eq!(popped.payload(), &[1, 2, 3]);
        }
    }

    #[test]
    fn validate_detects_bad_magic() {
        let mut shm = SharedMemory::anonymous();
        // Corrupt magic.
        shm.storage.as_mut_slice()[0] = 0xFF;
        assert!(matches!(
            shm.validate(),
            Err(AbiError::InvalidShmMagic { .. })
        ));
    }
}
