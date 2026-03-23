/*
 * test_wamr_compiler.c — Smoke test for the WAMR AOT compiler plugin wrapper.
 *
 * Exercises every public wamrc_* API function: init, version, high-level
 * compile, low-level load/emit/unload, and NULL-safety checks.
 */

#include "wamrc_plugin_api.h"
#include "generated_add_wasm.h" /* generated: provides add_wasm[] and add_wasm_size */

#include <stdio.h>
#include <string.h>

static int g_failures = 0;

#define ASSERT(cond, msg) do {                                         \
    if (!(cond)) {                                                     \
        fprintf(stderr, "FAIL: %s (line %d)\n", (msg), __LINE__);     \
        g_failures++;                                                  \
    } else {                                                           \
        fprintf(stdout, "PASS: %s\n", (msg));                          \
    }                                                                  \
} while (0)

int main(void)
{
    char error_buf[256];
    bool ok;

    /* ---- init ---- */
    ok = wamrc_init();
    ASSERT(ok, "wamrc_init");

    /* ---- version string ---- */
    const char *ver = wamrc_get_version_string();
    ASSERT(ver != NULL, "wamrc_get_version_string");
    printf("  WAMRC: %s\n", ver);

    /* ---- high-level: compile add.wasm → .aot ---- */
    {
        wamrc_options_t opts;
        wamrc_options_init_default(&opts);

        uint32_t aot_size = 0;
        memset(error_buf, 0, sizeof(error_buf));
        uint8_t *aot_buf = wamrc_compile(add_wasm_data, add_wasm_size,
                                         &opts, &aot_size,
                                         error_buf, sizeof(error_buf));
        if (!aot_buf)
            fprintf(stderr, "  wamrc_compile error: %s\n", error_buf);
        ASSERT(aot_buf != NULL, "wamrc_compile");
        ASSERT(aot_size > 0, "wamrc_compile produces non-zero output");

        if (aot_buf)
            wamrc_free_aot_file(aot_buf);
    }

    /* ---- low-level: load + emit + unload ---- */
    {
        memset(error_buf, 0, sizeof(error_buf));
        wamrc_compiler_t comp = wamrc_load(add_wasm_data, add_wasm_size,
                                           error_buf, sizeof(error_buf));
        if (!comp)
            fprintf(stderr, "  wamrc_load error: %s\n", error_buf);
        ASSERT(comp != NULL, "wamrc_load");

        if (comp) {
            wamrc_options_t opts;
            wamrc_options_init_default(&opts);

            uint32_t aot_size = 0;
            memset(error_buf, 0, sizeof(error_buf));
            uint8_t *aot_buf = wamrc_emit(comp, &opts, &aot_size,
                                          error_buf, sizeof(error_buf));
            if (!aot_buf)
                fprintf(stderr, "  wamrc_emit error: %s\n", error_buf);
            ASSERT(aot_buf != NULL, "wamrc_emit");
            ASSERT(aot_size > 0, "wamrc_emit produces non-zero output");

            if (aot_buf)
                wamrc_free_aot_file(aot_buf);

            wamrc_unload(comp);
        }
    }

    /* ---- NULL safety ---- */
    {
        wamrc_options_t opts;
        wamrc_options_init_default(&opts);
        uint32_t aot_size = 0;

        ASSERT(wamrc_compile(NULL, 0, &opts, &aot_size, error_buf,
                             sizeof(error_buf)) == NULL,
               "wamrc_compile(NULL)");
        ASSERT(wamrc_load(NULL, 0, error_buf, sizeof(error_buf)) == NULL,
               "wamrc_load(NULL)");
        ASSERT(wamrc_emit(NULL, &opts, &aot_size, error_buf,
                          sizeof(error_buf)) == NULL,
               "wamrc_emit(NULL)");
    }

    /* ---- cleanup ---- */
    wamrc_destroy();

    printf("\n%s (%d failure(s))\n",
           g_failures ? "FAILED" : "ALL TESTS PASSED", g_failures);
    return g_failures ? 1 : 0;
}
