/*
 * test_wamr.c — Smoke test for the WAMR plugin thin wrapper.
 *
 * Loads a WASM module (assembled from add.wat by binaryen's wasm-as at
 * build time, then embedded as a C header via cmake/embed_file.cmake)
 * and exercises every public API function.
 */

#include "wamr_plugin_api.h"
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

    /* ---- init ---- */
    bool ok = wamr_init();
    ASSERT(ok, "wamr_init");

    /* ---- version string ---- */
    const char *ver = wamr_get_version_string();
    ASSERT(ver != NULL, "wamr_get_version_string");
    printf("  WAMR: %s\n", ver);

    /* ---- load module ---- */
    memset(error_buf, 0, sizeof(error_buf));
    wamr_module_t module = wamr_load_module(add_wasm_data, add_wasm_size,
                                            error_buf, sizeof(error_buf));
    if (!module)
        fprintf(stderr, "  load_module error: %s\n", error_buf);
    ASSERT(module != NULL, "wamr_load_module");

    /* ---- create instance ---- */
    memset(error_buf, 0, sizeof(error_buf));
    wamr_instance_t inst = wamr_create_instance(module, 32768, 32768,
                                                error_buf, sizeof(error_buf));
    if (!inst)
        fprintf(stderr, "  create_instance error: %s\n", error_buf);
    ASSERT(inst != NULL, "wamr_create_instance");

    /* ---- find function ---- */
    wamr_func_t add_fn = wamr_find_function(inst, "add");
    ASSERT(add_fn != NULL, "wamr_find_function(add)");

    wamr_func_t missing = wamr_find_function(inst, "nonexistent");
    ASSERT(missing == NULL, "wamr_find_function(nonexistent) returns NULL");

    /* ---- basic call: add(2, 3) == 5 ---- */
    {
        uint32_t argv[2] = { 2, 3 };
        ok = wamr_call(inst, add_fn, 2, argv);
        ASSERT(ok, "wamr_call(add, 2, 3)");
        ASSERT(argv[0] == 5, "add(2, 3) == 5");
    }

    /* ---- different values: add(100, 200) == 300 ---- */
    {
        uint32_t argv[2] = { 100, 200 };
        ok = wamr_call(inst, add_fn, 2, argv);
        ASSERT(ok, "wamr_call(add, 100, 200)");
        ASSERT(argv[0] == 300, "add(100, 200) == 300");
    }

    /* ---- zeros: add(0, 0) == 0 ---- */
    {
        uint32_t argv[2] = { 0, 0 };
        ok = wamr_call(inst, add_fn, 2, argv);
        ASSERT(ok, "wamr_call(add, 0, 0)");
        ASSERT(argv[0] == 0, "add(0, 0) == 0");
    }

    /* ---- wrap-around: add(0xFFFFFFFF, 1) == 0 ---- */
    {
        uint32_t argv[2] = { 0xFFFFFFFF, 1 };
        ok = wamr_call(inst, add_fn, 2, argv);
        ASSERT(ok, "wamr_call(add, MAX_UINT, 1)");
        ASSERT(argv[0] == 0, "add(0xFFFFFFFF, 1) wraps to 0");
    }

    /* ---- memory operations ---- */
    {
        uint32_t ptr = wamr_module_malloc(inst, 16);
        ASSERT(ptr != 0, "wamr_module_malloc(16)");

        uint8_t write_data[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
        ok = wamr_write_memory(inst, ptr, write_data, 4);
        ASSERT(ok, "wamr_write_memory");

        uint8_t read_data[4] = { 0 };
        ok = wamr_read_memory(inst, ptr, read_data, 4);
        ASSERT(ok, "wamr_read_memory");
        ASSERT(memcmp(write_data, read_data, 4) == 0, "memory round-trip matches");

        ok = wamr_validate_memory(inst, ptr, 16);
        ASSERT(ok, "wamr_validate_memory in-bounds");

        wamr_module_free(inst, ptr);
    }

    /* ---- NULL safety ---- */
    ASSERT(wamr_find_function(NULL, "add") == NULL, "find_function(NULL inst)");
    ASSERT(!wamr_call(NULL, add_fn, 0, NULL),       "call(NULL inst)");
    ASSERT(!wamr_call(inst, NULL,   0, NULL),       "call(NULL func)");

    /* ---- cleanup ---- */
    wamr_destroy_instance(inst);
    wamr_unload_module(module);
    wamr_destroy();

    printf("\n%s (%d failure(s))\n",
           g_failures ? "FAILED" : "ALL TESTS PASSED", g_failures);
    return g_failures ? 1 : 0;
}
