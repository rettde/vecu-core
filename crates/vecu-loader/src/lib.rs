//! Cross‑platform plugin loader and orchestrator for the vECU system (ADR‑001).
//!
//! The loader is the **sole orchestrator**. It:
//! 1. Parses CLI / YAML config
//! 2. Dynamically loads APPL and HSM plugins via `vecu_get_api`
//! 3. Performs ABI negotiation
//! 4. Allocates shared memory (ADR‑003)
//! 5. Builds a [`VecuRuntimeContext`] and passes it to modules
//! 6. Drives the deterministic tick loop via [`vecu_runtime::Runtime`]

use std::path::{Path, PathBuf};

use libloading::Library;
use vecu_abi::{
    AbiError, ModuleKind, VecuGetApiFn, VecuPluginApi, VecuRuntimeContext, ABI_VERSION,
    PLUGIN_ENTRY_SYMBOL,
};
use vecu_runtime::{PluginSlot, Runtime, RuntimeAdapter, StandaloneAdapter};
use vecu_shm::{SharedMemory, ShmLayout};

// ---------------------------------------------------------------------------
// YAML configuration
// ---------------------------------------------------------------------------

/// Simulation configuration (typically loaded from `config.yaml`).
#[derive(Debug, Clone, serde::Deserialize, serde::Serialize)]
pub struct SimConfig {
    /// Path to the APPL shared library.
    pub appl: PathBuf,
    /// Path to the HSM shared library (optional).
    pub hsm: Option<PathBuf>,
    /// Number of ticks to execute (0 = unlimited, stopped externally).
    #[serde(default = "default_ticks")]
    pub ticks: u64,
    /// Tick interval in microseconds.
    #[serde(default = "default_tick_interval")]
    pub tick_interval_us: u64,
    /// Frame queue capacity per direction.
    #[serde(default = "default_queue_capacity")]
    pub queue_capacity: u32,
    /// Variable / state block size in bytes.
    #[serde(default = "default_vars_size")]
    pub vars_size: u32,
    /// Execution mode.
    #[serde(default)]
    pub mode: ExecMode,
    /// SIL Kit specific configuration (only used when `mode == silkit`).
    #[serde(default)]
    pub silkit: Option<SilKitSection>,
}

/// SIL Kit specific configuration (optional section in YAML).
#[derive(Debug, Clone, Default, serde::Deserialize, serde::Serialize)]
pub struct SilKitSection {
    /// Path to the SIL Kit shared library (auto‑detected if empty).
    #[serde(default)]
    pub library_path: Option<String>,
    /// SIL Kit registry URI.
    #[serde(default = "default_registry_uri")]
    pub registry_uri: String,
    /// Participant name.
    #[serde(default = "default_participant_name")]
    pub participant_name: String,
    /// CAN network name.
    #[serde(default = "default_can_network")]
    pub can_network: String,
    /// CAN controller name.
    #[serde(default = "default_can_controller")]
    pub can_controller_name: String,
    /// Ethernet network name (optional).
    #[serde(default)]
    pub eth_network: Option<String>,
    /// Ethernet controller name (optional).
    #[serde(default)]
    pub eth_controller_name: Option<String>,
    /// LIN network name (optional).
    #[serde(default)]
    pub lin_network: Option<String>,
    /// LIN controller name (optional).
    #[serde(default)]
    pub lin_controller_name: Option<String>,
    /// `FlexRay` network name (optional).
    #[serde(default)]
    pub flexray_network: Option<String>,
    /// `FlexRay` controller name (optional).
    #[serde(default)]
    pub flexray_controller_name: Option<String>,
    /// Simulation step size in nanoseconds.
    #[serde(default = "default_step_size_ns")]
    pub step_size_ns: u64,
    /// Use coordinated lifecycle (default: true).
    #[serde(default = "default_coordinated")]
    pub coordinated: bool,
}

fn default_registry_uri() -> String {
    "silkit://localhost:8500".into()
}
fn default_participant_name() -> String {
    "vECU".into()
}
fn default_can_network() -> String {
    "CAN1".into()
}
fn default_can_controller() -> String {
    "CAN1".into()
}
fn default_step_size_ns() -> u64 {
    1_000_000
}
fn default_coordinated() -> bool {
    true
}

/// Execution mode.
#[derive(Debug, Clone, Copy, Default, serde::Deserialize, serde::Serialize)]
#[serde(rename_all = "lowercase")]
pub enum ExecMode {
    /// Single‑process, fully deterministic.
    #[default]
    Standalone,
    /// Connected to a Vector SIL Kit co‑simulation.
    #[serde(alias = "distributed")]
    Silkit,
}

fn default_ticks() -> u64 {
    100
}
fn default_tick_interval() -> u64 {
    1000
}
fn default_queue_capacity() -> u32 {
    vecu_abi::DEFAULT_QUEUE_CAPACITY
}
fn default_vars_size() -> u32 {
    vecu_abi::DEFAULT_VARS_SIZE
}

impl SimConfig {
    /// Load a config from a YAML file.
    ///
    /// # Errors
    ///
    /// Returns [`LoaderError::Config`] on parse failure.
    pub fn from_yaml(path: &Path) -> Result<Self, LoaderError> {
        let content = std::fs::read_to_string(path).map_err(LoaderError::Io)?;
        serde_yaml::from_str(&content).map_err(|e| LoaderError::Config(e.to_string()))
    }

    /// Build an [`ShmLayout`] from this config.
    #[must_use]
    pub fn shm_layout(&self) -> ShmLayout {
        ShmLayout {
            queue_capacity: self.queue_capacity,
            vars_size: self.vars_size,
        }
    }
}

// ---------------------------------------------------------------------------
// Plugin loading
// ---------------------------------------------------------------------------

/// A dynamically loaded vECU plugin.
///
/// Holds the [`Library`] handle to keep the shared object alive and
/// the negotiated [`VecuPluginApi`].
pub struct LoadedPlugin {
    api: VecuPluginApi,
    path: PathBuf,
    _library: Library,
}

impl std::fmt::Debug for LoadedPlugin {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("LoadedPlugin")
            .field("path", &self.path)
            .field("api", &self.api)
            .finish_non_exhaustive()
    }
}

impl LoadedPlugin {
    /// Load a plugin from a shared library at `path`.
    ///
    /// Resolves the single `vecu_get_api` symbol, calls it with the
    /// loader's ABI version, and validates the returned API table.
    ///
    /// # Errors
    ///
    /// Returns on library load failure, missing symbol, or ABI mismatch.
    pub fn load(path: &Path) -> Result<Self, LoaderError> {
        tracing::info!(path = %path.display(), "loading plugin");

        #[allow(unsafe_code)]
        let library = unsafe {
            // SAFETY: Loading a shared library is inherently unsafe.
            Library::new(path).map_err(LoaderError::Library)?
        };

        #[allow(unsafe_code)]
        let get_api: VecuGetApiFn = unsafe {
            // SAFETY: We resolve the well‑known symbol and trust the signature.
            let sym: libloading::Symbol<'_, VecuGetApiFn> = library
                .get(PLUGIN_ENTRY_SYMBOL)
                .map_err(|_| LoaderError::Abi(AbiError::MissingEntryPoint))?;
            *sym
        };

        let mut api = VecuPluginApi::zeroed();
        #[allow(unsafe_code)]
        let rc = unsafe {
            // SAFETY: api is a valid, writeable VecuPluginApi on the stack.
            get_api(ABI_VERSION, &mut api)
        };

        if rc != vecu_abi::status::OK {
            return Err(LoaderError::Abi(AbiError::ModuleError(rc)));
        }

        // Validate ABI major version.
        let (our_major, _) = vecu_abi::unpack_version(ABI_VERSION);
        let (their_major, _) = vecu_abi::unpack_version(api.abi_version);
        if our_major != their_major {
            return Err(LoaderError::Abi(AbiError::VersionMismatch {
                expected: ABI_VERSION,
                got: api.abi_version,
            }));
        }

        // Validate module kind.
        if ModuleKind::from_raw(api.module_kind).is_none() {
            return Err(LoaderError::Abi(AbiError::UnknownModuleKind(
                api.module_kind,
            )));
        }

        // Require at least step + init.
        if api.step.is_none() {
            return Err(LoaderError::Abi(AbiError::MissingFunction("step".into())));
        }
        if api.init.is_none() {
            return Err(LoaderError::Abi(AbiError::MissingFunction("init".into())));
        }

        tracing::info!(
            path = %path.display(),
            kind = ?ModuleKind::from_raw(api.module_kind),
            caps = api.capabilities,
            "plugin loaded"
        );

        Ok(Self {
            api,
            path: path.to_path_buf(),
            _library: library,
        })
    }

    /// Module kind reported by the plugin.
    #[must_use]
    pub fn module_kind(&self) -> Option<ModuleKind> {
        ModuleKind::from_raw(self.api.module_kind)
    }

    /// Consume this handle and return the API table.
    pub fn into_api(self) -> VecuPluginApi {
        self.api
    }

    /// Borrow the API table.
    #[must_use]
    pub fn api(&self) -> &VecuPluginApi {
        &self.api
    }

    /// Path the plugin was loaded from.
    #[must_use]
    pub fn path(&self) -> &Path {
        &self.path
    }
}

// ---------------------------------------------------------------------------
// Orchestrator
// ---------------------------------------------------------------------------

/// Run a full simulation according to `config`.
///
/// # Errors
///
/// Returns on the first loading, initialisation, or runtime error.
pub fn run_simulation(config: &SimConfig) -> Result<(), LoaderError> {
    // 1. Load plugins.
    let appl_plugin = LoadedPlugin::load(&config.appl)?;
    let hsm_plugin = config
        .hsm
        .as_ref()
        .map(|p| LoadedPlugin::load(p))
        .transpose()?;

    // 2. Allocate shared memory.
    let mut shm = SharedMemory::with_layout(config.shm_layout());
    shm.validate().map_err(LoaderError::Abi)?;

    // 3. Build runtime context.
    let (shm_base, shm_size) = shm.raw_parts();
    let ctx = VecuRuntimeContext {
        shm_base,
        shm_size,
        pad0: 0,
        tick_interval_us: config.tick_interval_us,
        log_fn: None,
    };

    // 4. Build runtime and assign slots.
    let mut runtime = Runtime::new(shm);
    runtime.set_appl(PluginSlot::new(appl_plugin.into_api()));
    if let Some(hsm) = hsm_plugin {
        runtime.set_hsm(PluginSlot::new(hsm.into_api()));
    }

    // 5. Initialise modules.
    runtime.init_all(&ctx)?;

    // 6. Build and run the appropriate adapter.
    match config.mode {
        ExecMode::Standalone => {
            let mut adapter = StandaloneAdapter::new(config.ticks);
            adapter.run(&mut runtime)?;
        }
        ExecMode::Silkit => {
            let sk_section = config.silkit.clone().unwrap_or_default();
            let sk_cfg = vecu_silkit::SilKitConfig {
                library_path: sk_section
                    .library_path
                    .unwrap_or_else(|| vecu_silkit::SilKitConfig::default().library_path),
                registry_uri: sk_section.registry_uri,
                participant_name: sk_section.participant_name,
                can_network: sk_section.can_network,
                can_controller_name: sk_section.can_controller_name,
                eth_network: sk_section.eth_network,
                eth_controller_name: sk_section.eth_controller_name,
                lin_network: sk_section.lin_network,
                lin_controller_name: sk_section.lin_controller_name,
                flexray_network: sk_section.flexray_network,
                flexray_controller_name: sk_section.flexray_controller_name,
                step_size_ns: sk_section.step_size_ns,
                participant_config_json: String::new(),
                coordinated: sk_section.coordinated,
            };
            let mut adapter = vecu_silkit::SilKitAdapter::new(sk_cfg);
            adapter.run(&mut runtime)?;
        }
    }

    // 7. Shutdown.
    runtime.shutdown_all();
    tracing::info!(ticks = runtime.current_tick(), "simulation complete");
    Ok(())
}

// ---------------------------------------------------------------------------
// Error types
// ---------------------------------------------------------------------------

/// Errors produced by the loader.
#[derive(Debug, thiserror::Error)]
pub enum LoaderError {
    /// Shared library could not be opened.
    #[error("failed to load library: {0}")]
    Library(libloading::Error),
    /// ABI‑level error.
    #[error(transparent)]
    Abi(#[from] AbiError),
    /// Runtime error.
    #[error(transparent)]
    Runtime(#[from] vecu_runtime::RuntimeError),
    /// I/O error.
    #[error("I/O error: {0}")]
    Io(#[from] std::io::Error),
    /// Configuration error.
    #[error("config error: {0}")]
    Config(String),
    /// Shared‑memory error.
    #[error(transparent)]
    Shm(#[from] vecu_shm::ShmError),
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sim_config_round_trips_yaml() {
        let yaml = "
appl: target/debug/libvecu_appl.dylib
hsm: target/debug/libvecu_hsm.dylib
ticks: 100
tick_interval_us: 500
";
        let config: SimConfig = serde_yaml::from_str(yaml).unwrap();
        assert_eq!(config.ticks, 100);
        assert_eq!(config.tick_interval_us, 500);
        assert!(config.hsm.is_some());
    }

    #[test]
    fn sim_config_defaults() {
        let yaml = "appl: target/debug/libvecu_appl.dylib\n";
        let config: SimConfig = serde_yaml::from_str(yaml).unwrap();
        assert_eq!(config.ticks, 100);
        assert_eq!(config.tick_interval_us, 1000);
        assert_eq!(config.queue_capacity, vecu_abi::DEFAULT_QUEUE_CAPACITY);
        assert_eq!(config.vars_size, vecu_abi::DEFAULT_VARS_SIZE);
        assert!(config.hsm.is_none());
    }

    #[test]
    fn sim_config_from_yaml_file() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("config.yaml");
        std::fs::write(&path, "appl: /tmp/appl.so\nticks: 50\n").unwrap();
        let config = SimConfig::from_yaml(&path).unwrap();
        assert_eq!(config.ticks, 50);
    }

    #[test]
    fn load_nonexistent_library_returns_error() {
        let result = LoadedPlugin::load(Path::new("/tmp/nonexistent.so"));
        assert!(result.is_err());
        assert!(matches!(result.unwrap_err(), LoaderError::Library(_)));
    }

    #[test]
    fn shm_layout_from_config() {
        let yaml = "appl: x\nqueue_capacity: 16\nvars_size: 512\n";
        let config: SimConfig = serde_yaml::from_str(yaml).unwrap();
        let layout = config.shm_layout();
        assert_eq!(layout.queue_capacity, 16);
        assert_eq!(layout.vars_size, 512);
    }

    #[test]
    fn exec_mode_default_is_standalone() {
        let yaml = "appl: x\n";
        let config: SimConfig = serde_yaml::from_str(yaml).unwrap();
        assert!(matches!(config.mode, ExecMode::Standalone));
        assert!(config.silkit.is_none());
    }

    #[test]
    fn exec_mode_silkit_from_yaml() {
        let yaml = "appl: x\nmode: silkit\n";
        let config: SimConfig = serde_yaml::from_str(yaml).unwrap();
        assert!(matches!(config.mode, ExecMode::Silkit));
    }

    #[test]
    fn exec_mode_distributed_alias() {
        let yaml = "appl: x\nmode: distributed\n";
        let config: SimConfig = serde_yaml::from_str(yaml).unwrap();
        assert!(matches!(config.mode, ExecMode::Silkit));
    }

    #[test]
    fn silkit_section_parses() {
        let yaml = "
appl: x
mode: silkit
silkit:
  registry_uri: silkit://10.0.0.1:9000
  participant_name: MyECU
  can_network: CAN2
  step_size_ns: 500000
  coordinated: false
";
        let config: SimConfig = serde_yaml::from_str(yaml).unwrap();
        let sk = config.silkit.unwrap();
        assert_eq!(sk.registry_uri, "silkit://10.0.0.1:9000");
        assert_eq!(sk.participant_name, "MyECU");
        assert_eq!(sk.can_network, "CAN2");
        assert_eq!(sk.step_size_ns, 500_000);
        assert!(!sk.coordinated);
        assert!(sk.library_path.is_none());
    }

    #[test]
    fn silkit_section_defaults() {
        let yaml = "appl: x\nmode: silkit\nsilkit: {}\n";
        let config: SimConfig = serde_yaml::from_str(yaml).unwrap();
        let sk = config.silkit.unwrap();
        assert_eq!(sk.registry_uri, "silkit://localhost:8500");
        assert_eq!(sk.participant_name, "vECU");
        assert_eq!(sk.can_network, "CAN1");
        assert_eq!(sk.step_size_ns, 1_000_000);
        assert!(sk.coordinated);
    }

    #[test]
    fn silkit_section_optional_buses_default_none() {
        let yaml = "appl: x\nmode: silkit\nsilkit: {}\n";
        let config: SimConfig = serde_yaml::from_str(yaml).unwrap();
        let sk = config.silkit.unwrap();
        assert!(sk.eth_network.is_none());
        assert!(sk.eth_controller_name.is_none());
        assert!(sk.lin_network.is_none());
        assert!(sk.lin_controller_name.is_none());
        assert!(sk.flexray_network.is_none());
        assert!(sk.flexray_controller_name.is_none());
    }

    #[test]
    fn silkit_section_parses_multi_bus() {
        let yaml = "
appl: x
mode: silkit
silkit:
  eth_network: ETH1
  eth_controller_name: ETH1
  lin_network: LIN1
  lin_controller_name: LIN1
  flexray_network: FR1
  flexray_controller_name: FR1
";
        let config: SimConfig = serde_yaml::from_str(yaml).unwrap();
        let sk = config.silkit.unwrap();
        assert_eq!(sk.eth_network.as_deref(), Some("ETH1"));
        assert_eq!(sk.eth_controller_name.as_deref(), Some("ETH1"));
        assert_eq!(sk.lin_network.as_deref(), Some("LIN1"));
        assert_eq!(sk.lin_controller_name.as_deref(), Some("LIN1"));
        assert_eq!(sk.flexray_network.as_deref(), Some("FR1"));
        assert_eq!(sk.flexray_controller_name.as_deref(), Some("FR1"));
    }
}
