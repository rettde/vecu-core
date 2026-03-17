//! CLI entry point for the vECU Loader (ADR‑001).
//!
//! Usage:
//! ```text
//! vecu-loader --appl <path> [--hsm <path>] [--config <config.yaml>] [--ticks N]
//! ```

use std::path::PathBuf;
use std::process::ExitCode;

use clap::Parser;
use vecu_loader::{ExecMode, LoaderError, SimConfig};

/// Cross‑platform vECU Loader – deterministic plugin orchestrator.
#[derive(Parser, Debug)]
#[command(name = "vecu-loader", version, about)]
struct Cli {
    /// Path to the APPL shared library.
    #[arg(long)]
    appl: Option<PathBuf>,

    /// Path to the HSM shared library (optional).
    #[arg(long)]
    hsm: Option<PathBuf>,

    /// Path to YAML configuration file.
    #[arg(long, short)]
    config: Option<PathBuf>,

    /// Number of simulation ticks (overrides config).
    #[arg(long)]
    ticks: Option<u64>,

    /// Execution mode: standalone or distributed.
    #[arg(long, default_value = "standalone")]
    mode: String,
}

fn main() -> ExitCode {
    // Initialise tracing.
    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| tracing_subscriber::EnvFilter::new("info")),
        )
        .init();

    let cli = Cli::parse();

    match run(&cli) {
        Ok(()) => ExitCode::SUCCESS,
        Err(e) => {
            tracing::error!("{e}");
            ExitCode::FAILURE
        }
    }
}

fn run(cli: &Cli) -> Result<(), LoaderError> {
    // Build config: file‑based or CLI‑based.
    let mut config = if let Some(cfg_path) = &cli.config {
        tracing::info!(path = %cfg_path.display(), "loading config");
        SimConfig::from_yaml(cfg_path)?
    } else {
        let appl = cli.appl.clone().ok_or_else(|| {
            LoaderError::Config("either --appl or --config must be specified".into())
        })?;
        SimConfig {
            appl,
            hsm: None,
            ticks: 100,
            tick_interval_us: 1000,
            queue_capacity: vecu_abi::DEFAULT_QUEUE_CAPACITY,
            vars_size: vecu_abi::DEFAULT_VARS_SIZE,
            shm_file: None,
            mode: ExecMode::Standalone,
            silkit: None,
        }
    };

    // CLI overrides.
    if let Some(appl) = &cli.appl {
        config.appl.clone_from(appl);
    }
    if let Some(hsm) = &cli.hsm {
        config.hsm = Some(hsm.clone());
    }
    if let Some(ticks) = cli.ticks {
        config.ticks = ticks;
    }
    if cli.mode == "silkit" || cli.mode == "distributed" {
        config.mode = ExecMode::Silkit;
    }

    tracing::info!(?config, "starting simulation");
    vecu_loader::run_simulation(&config)
}
