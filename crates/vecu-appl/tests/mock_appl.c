/* mock_appl.c — Minimal mock ECU Application for bridge integration tests.
 *
 * Exports Appl_Init, Appl_MainFunction, Appl_Shutdown.
 * Tracks call counts via global counters.
 */

#include <stdint.h>

static int g_appl_init_count = 0;
static int g_appl_main_count = 0;
static int g_appl_shutdown_count = 0;

#ifdef _WIN32
  #define EXPORT __declspec(dllexport)
#else
  #define EXPORT __attribute__((visibility("default")))
#endif

EXPORT void Appl_Init(void) {
    g_appl_init_count++;
}

EXPORT void Appl_MainFunction(void) {
    g_appl_main_count++;
}

EXPORT void Appl_Shutdown(void) {
    g_appl_shutdown_count++;
}

EXPORT int mock_appl_init_count(void)     { return g_appl_init_count; }
EXPORT int mock_appl_main_count(void)     { return g_appl_main_count; }
EXPORT int mock_appl_shutdown_count(void) { return g_appl_shutdown_count; }
