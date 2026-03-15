//! FFI bridge: dynamic loading of `BaseLayer` and Application shared libraries.
//!
//! Loads `libbase.so` / `base.dll` and `libappl_ecu.so` / `appl_ecu.dll`
//! at runtime via `libloading`, resolves mandatory symbols, and provides
//! a safe Rust wrapper for calling them.

use std::path::Path;

/// Errors that can occur when loading or calling bridge libraries.
#[derive(Debug, thiserror::Error)]
pub(crate) enum BridgeError {
    /// A shared library could not be loaded.
    #[error("failed to load library {path}: {source}")]
    Load {
        /// Path that was attempted.
        path: String,
        /// Underlying OS error.
        source: libloading::Error,
    },
    /// A required symbol was not found.
    #[error("symbol not found in {lib}: {symbol}: {source}")]
    Symbol {
        /// Library name.
        lib: &'static str,
        /// Symbol name.
        symbol: &'static str,
        /// Underlying OS error.
        source: libloading::Error,
    },
}

// Function pointer types matching vecu_base_context.h
type BaseInitFn = unsafe extern "C" fn(*const super::context::VecuBaseContext);
type BaseStepFn = unsafe extern "C" fn(u64);
type BaseShutdownFn = unsafe extern "C" fn();
type ApplInitFn = unsafe extern "C" fn();
type ApplMainFn = unsafe extern "C" fn();
type ApplShutdownFn = unsafe extern "C" fn();

/// Holds loaded shared libraries and resolved function pointers.
pub(crate) struct BridgeLoader {
    // Libraries must be kept alive for the function pointers to remain valid.
    _base_lib: libloading::Library,
    _appl_lib: libloading::Library,
    base_init: BaseInitFn,
    base_step: BaseStepFn,
    base_shutdown: BaseShutdownFn,
    appl_init: ApplInitFn,
    appl_main: ApplMainFn,
    appl_shutdown: ApplShutdownFn,
}

#[allow(unsafe_code)]
impl BridgeLoader {
    /// Load the `BaseLayer` and Application libraries, resolving all mandatory symbols.
    ///
    /// # Safety
    ///
    /// The loaded libraries must export the correct function signatures as
    /// defined in `vecu_base_context.h`.
    pub(crate) unsafe fn load(
        base_path: &Path,
        appl_path: &Path,
    ) -> Result<Self, BridgeError> {
        let base_lib =
            unsafe { libloading::Library::new(base_path) }.map_err(|e| BridgeError::Load {
                path: base_path.display().to_string(),
                source: e,
            })?;

        let appl_lib =
            unsafe { libloading::Library::new(appl_path) }.map_err(|e| BridgeError::Load {
                path: appl_path.display().to_string(),
                source: e,
            })?;

        let base_init: BaseInitFn = unsafe {
            *base_lib.get(b"Base_Init\0").map_err(|e| BridgeError::Symbol {
                lib: "base",
                symbol: "Base_Init",
                source: e,
            })?
        };
        let base_step: BaseStepFn = unsafe {
            *base_lib.get(b"Base_Step\0").map_err(|e| BridgeError::Symbol {
                lib: "base",
                symbol: "Base_Step",
                source: e,
            })?
        };
        let base_shutdown: BaseShutdownFn = unsafe {
            *base_lib
                .get(b"Base_Shutdown\0")
                .map_err(|e| BridgeError::Symbol {
                    lib: "base",
                    symbol: "Base_Shutdown",
                    source: e,
                })?
        };

        let appl_init: ApplInitFn = unsafe {
            *appl_lib
                .get(b"Appl_Init\0")
                .map_err(|e| BridgeError::Symbol {
                    lib: "appl",
                    symbol: "Appl_Init",
                    source: e,
                })?
        };
        let appl_main: ApplMainFn = unsafe {
            *appl_lib
                .get(b"Appl_MainFunction\0")
                .map_err(|e| BridgeError::Symbol {
                    lib: "appl",
                    symbol: "Appl_MainFunction",
                    source: e,
                })?
        };
        let appl_shutdown: ApplShutdownFn = unsafe {
            *appl_lib
                .get(b"Appl_Shutdown\0")
                .map_err(|e| BridgeError::Symbol {
                    lib: "appl",
                    symbol: "Appl_Shutdown",
                    source: e,
                })?
        };

        Ok(Self {
            _base_lib: base_lib,
            _appl_lib: appl_lib,
            base_init,
            base_step,
            base_shutdown,
            appl_init,
            appl_main,
            appl_shutdown,
        })
    }

    /// Call `Base_Init(ctx)`.
    ///
    /// # Safety
    ///
    /// `ctx` must point to a valid [`VecuBaseContext`](super::context::VecuBaseContext) that remains valid
    /// until `call_base_shutdown` is called.
    pub(crate) unsafe fn call_base_init(&self, ctx: *const super::context::VecuBaseContext) {
        unsafe { (self.base_init)(ctx) };
    }

    /// Call `Base_Step(tick)`.
    pub(crate) fn call_base_step(&self, tick: u64) {
        #[allow(unsafe_code)]
        unsafe {
            (self.base_step)(tick);
        }
    }

    /// Call `Base_Shutdown()`.
    pub(crate) fn call_base_shutdown(&self) {
        #[allow(unsafe_code)]
        unsafe {
            (self.base_shutdown)();
        }
    }

    /// Call `Appl_Init()`.
    pub(crate) fn call_appl_init(&self) {
        #[allow(unsafe_code)]
        unsafe {
            (self.appl_init)();
        }
    }

    /// Call `Appl_MainFunction()`.
    pub(crate) fn call_appl_main(&self) {
        #[allow(unsafe_code)]
        unsafe {
            (self.appl_main)();
        }
    }

    /// Call `Appl_Shutdown()`.
    pub(crate) fn call_appl_shutdown(&self) {
        #[allow(unsafe_code)]
        unsafe {
            (self.appl_shutdown)();
        }
    }
}
