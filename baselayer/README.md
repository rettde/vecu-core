# BaseLayer — Reference AUTOSAR BSW Stub Library

Minimal but functional BaseLayer implementing the three mandatory exports
defined in `vecu_base_context.h` (ADR-005):

| Export | Purpose |
|--------|---------|
| `Base_Init(ctx)` | Store callback context, initialise EcuM → SchM → Os → Det |
| `Base_Step(tick)` | Advance Os tick, drive EcuM_MainFunction → SchM schedule |
| `Base_Shutdown()` | EcuM_GoSleep, release resources |

## BSW Modules (P3 Minimal Set)

| Module | Level | Description |
|--------|-------|-------------|
| **EcuM** | Functional | STARTUP → RUN → SHUTDOWN state machine |
| **SchM** | Functional | Deterministic MainFunction scheduler |
| **Os** | Stub | Tick counter, no real task activation |
| **Det** | Functional | Error reporting via `ctx.log_fn` callback |
| **Rte** | Skeleton | Empty `Rte_Start` / `Rte_Stop` stubs |

## Building

```bash
# From workspace root
cmake -S baselayer -B baselayer/build
cmake --build baselayer/build

# Result: baselayer/build/libbase.so (Linux)
#         baselayer/build/libbase.dylib (macOS)
#         baselayer/build/base.dll (Windows)
```

## Using with vecu-appl

Set environment variables before running the vECU:

```bash
export VECU_BASE_LIB=./baselayer/build/libbase.dylib
export VECU_APPL_LIB=./path/to/libappl_ecu.dylib
```

The BaseLayer will be loaded by `vecu-appl` during `appl_init` and the
callback context will be injected automatically.

## Extending

To add new BSW modules (e.g. Com, PduR in P4):

1. Add `include/NewModule.h` and `src/NewModule.c`
2. Register `NewModule_MainFunction` in `SchM.c` → `g_main_functions[]`
3. Add init/deinit calls in `EcuM.c` → `EcuM_Init()` / `EcuM_GoSleep()`
4. Add the source file to `CMakeLists.txt` → `BASE_SOURCES`
