/* mock_base.c — Minimal mock BaseLayer for bridge integration tests.
 *
 * Exports Base_Init, Base_Step, Base_Shutdown.
 * Tracks call counts via global counters that the test can inspect.
 */

#include <stdint.h>
#include <stddef.h>

/* Minimal vecu_base_context_t — only the fields we use in the mock. */
typedef struct {
    void* push_tx_frame;
    void* pop_rx_frame;
    void* hsm_encrypt;
    void* hsm_decrypt;
    void* hsm_generate_mac;
    void* hsm_verify_mac;
    void* hsm_seed;
    void* hsm_key;
    void* hsm_rng;
    void*    shm_vars;
    uint32_t shm_vars_size;
    uint32_t _pad0;
    void*    log_fn;
    uint64_t tick_interval_us;
} vecu_base_context_t;

static int g_base_init_count  = 0;
static int g_base_step_count  = 0;
static int g_base_shutdown_count = 0;
static uint64_t g_last_tick = 0;

#ifdef _WIN32
  #define EXPORT __declspec(dllexport)
#else
  #define EXPORT __attribute__((visibility("default")))
#endif

EXPORT void Base_Init(const vecu_base_context_t* ctx) {
    (void)ctx;
    g_base_init_count++;
}

EXPORT void Base_Step(uint64_t tick) {
    g_base_step_count++;
    g_last_tick = tick;
}

EXPORT void Base_Shutdown(void) {
    g_base_shutdown_count++;
}

EXPORT int mock_base_init_count(void)  { return g_base_init_count; }
EXPORT int mock_base_step_count(void)  { return g_base_step_count; }
EXPORT int mock_base_shutdown_count(void) { return g_base_shutdown_count; }
EXPORT uint64_t mock_base_last_tick(void) { return g_last_tick; }
