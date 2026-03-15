/* mock_appl_minimal.c — Minimal application stubs for BaseLayer integration test.
 *
 * Paired with the real BaseLayer (baselayer/src/*) to verify
 * end-to-end bridge mode.
 */

#include <stdint.h>

static int g_init_count = 0;
static int g_main_count = 0;
static int g_shutdown_count = 0;

#ifdef _WIN32
  #define EXPORT __declspec(dllexport)
#else
  #define EXPORT __attribute__((visibility("default")))
#endif

EXPORT void Appl_Init(void) {
    g_init_count++;
}

EXPORT void Appl_MainFunction(void) {
    g_main_count++;
}

EXPORT void Appl_Shutdown(void) {
    g_shutdown_count++;
}

EXPORT int mock_appl_init_count(void)     { return g_init_count; }
EXPORT int mock_appl_main_count(void)     { return g_main_count; }
EXPORT int mock_appl_shutdown_count(void) { return g_shutdown_count; }
