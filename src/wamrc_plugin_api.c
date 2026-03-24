#include "wamrc_plugin_api.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "wasm_export.h"
#include "aot_export.h"
#include "llvm-c/Core.h" /* LLVMGetVersion */

/* Internal wrapper: keeps the malloc'd WASM buffer alive alongside the module
 * (WAMR references the buffer for the lifetime of the loaded module). */
typedef struct {
    wasm_module_t module;
    uint8_t      *wasm_buf;
} compiler_wrapper;

static bool s_initialized = false;

/* --- Global lifecycle ------------------------------------------------ */

bool wamrc_init(void)
{
    if (s_initialized)
        return true;

    RuntimeInitArgs init_args;
    memset(&init_args, 0, sizeof(init_args));
    init_args.mem_alloc_type = Alloc_With_Allocator;
    init_args.mem_alloc_option.allocator.malloc_func = malloc;
    init_args.mem_alloc_option.allocator.realloc_func = realloc;
    init_args.mem_alloc_option.allocator.free_func = free;

    if (!wasm_runtime_full_init(&init_args))
        return false;

    if (!aot_compiler_init()) {
        wasm_runtime_destroy();
        return false;
    }

    s_initialized = true;
    return true;
}

void wamrc_destroy(void)
{
    /* No-op: LLVM's LLVMShutdown is not re-entrant, so we never call
     * aot_compiler_destroy(). Let process exit handle cleanup. */
}

/* --- Version info ---------------------------------------------------- */

static char s_version_string[128] = { 0 };

const char *wamrc_get_version_string(void)
{
    if (s_version_string[0] != '\0')
        return s_version_string;

    uint32_t major, minor, patch;
    wasm_runtime_get_version(&major, &minor, &patch);

    unsigned llvm_major = 0, llvm_minor = 0, llvm_patch = 0;
    LLVMGetVersion(&llvm_major, &llvm_minor, &llvm_patch);

    snprintf(s_version_string, sizeof(s_version_string),
             "%u.%u.%u (LLVM %u.%u.%u)", major, minor, patch,
             llvm_major, llvm_minor, llvm_patch);

    return s_version_string;
}

const char *wamrc_get_last_error(void)
{
    return aot_get_last_error();
}

/* --- Options --------------------------------------------------------- */

void wamrc_options_init_default(wamrc_options_t *opts)
{
    if (!opts) return;
    memset(opts, 0, sizeof(*opts));
    opts->opt_level = 3;
    opts->size_level = 1;
    opts->enable_bulk_memory = true;
}

/* --- Internal: map wamrc_options_t → AOTCompOption ------------------- */

static void map_options(const wamrc_options_t *src, AOTCompOption *dst)
{
    memset(dst, 0, sizeof(*dst));

    dst->target_arch = (char *)src->target_arch;
    dst->target_abi = (char *)src->target_abi;
    dst->target_cpu = (char *)src->target_cpu;
    dst->cpu_features = (char *)src->cpu_features;
    dst->opt_level = src->opt_level;
    dst->size_level = src->size_level;
    dst->output_format = AOT_FORMAT_FILE;
    dst->bounds_checks = 2; /* let compiler decide */

    dst->enable_bulk_memory = src->enable_bulk_memory;
    dst->enable_simd = src->enable_simd;
    dst->enable_tail_call = src->enable_tail_call;
    dst->enable_ref_types = src->enable_ref_types;
    dst->enable_thread_mgr = src->enable_multi_thread;

    /* Fixups matching wamr-compiler/main.c behavior */
    if (src->enable_bulk_memory)
        dst->enable_bulk_memory_opt = true;
    if (src->enable_ref_types)
        dst->enable_call_indirect_overlong = true;
}

/* --- Internal: compile a loaded module with given options ------------ */

static uint8_t *compile_module(wasm_module_t module,
                               const wamrc_options_t *options,
                               uint32_t *out_aot_size,
                               char *error_buf, uint32_t error_buf_size)
{
    AOTCompOption aot_option;
    aot_comp_data_t comp_data = NULL;
    aot_comp_context_t comp_ctx = NULL;
    uint8_t *aot_buf = NULL;

    map_options(options, &aot_option);

    comp_data = aot_create_comp_data(module, aot_option.target_arch, false);
    if (!comp_data) {
        if (error_buf && error_buf_size > 0) {
            const char *err = aot_get_last_error();
            snprintf(error_buf, error_buf_size, "%s", err ? err : "Failed to create comp data");
        }
        goto fail;
    }

    comp_ctx = aot_create_comp_context(comp_data, &aot_option);
    if (!comp_ctx) {
        if (error_buf && error_buf_size > 0) {
            const char *err = aot_get_last_error();
            snprintf(error_buf, error_buf_size, "%s", err ? err : "Failed to create comp context");
        }
        goto fail;
    }

    if (!aot_compile_wasm(comp_ctx)) {
        if (error_buf && error_buf_size > 0) {
            const char *err = aot_get_last_error();
            snprintf(error_buf, error_buf_size, "%s", err ? err : "AOT compilation failed");
        }
        goto fail;
    }

    aot_buf = aot_emit_aot_file_buf(comp_ctx, comp_data, out_aot_size);
    if (!aot_buf) {
        if (error_buf && error_buf_size > 0) {
            const char *err = aot_get_last_error();
            snprintf(error_buf, error_buf_size, "%s", err ? err : "Failed to emit AOT file");
        }
        goto fail;
    }

    aot_destroy_comp_context(comp_ctx);
    aot_destroy_comp_data(comp_data);
    return aot_buf;

fail:
    if (comp_ctx)
        aot_destroy_comp_context(comp_ctx);
    if (comp_data)
        aot_destroy_comp_data(comp_data);
    return NULL;
}

/* --- High-level API -------------------------------------------------- */

uint8_t *wamrc_compile(const uint8_t *wasm_bytes, uint32_t wasm_size,
                       const wamrc_options_t *options,
                       uint32_t *out_aot_size,
                       char *error_buf, uint32_t error_buf_size)
{
    uint8_t *wasm_buf = NULL;
    wasm_module_t module = NULL;
    uint8_t *result = NULL;
    wamrc_options_t default_opts;

    if (!wasm_bytes || wasm_size == 0) {
        if (error_buf && error_buf_size > 0)
            snprintf(error_buf, error_buf_size, "Invalid wasm bytes");
        return NULL;
    }

    if (!options) {
        wamrc_options_init_default(&default_opts);
        options = &default_opts;
    }

    /* Copy wasm bytes — WAMR holds a reference to the buffer */
    wasm_buf = (uint8_t *)malloc(wasm_size);
    if (!wasm_buf) {
        if (error_buf && error_buf_size > 0)
            snprintf(error_buf, error_buf_size, "Failed to allocate wasm buffer");
        return NULL;
    }
    memcpy(wasm_buf, wasm_bytes, wasm_size);

    module = wasm_runtime_load(wasm_buf, wasm_size, error_buf, error_buf_size);
    if (!module)
        goto cleanup;

    result = compile_module(module, options, out_aot_size,
                            error_buf, error_buf_size);

cleanup:
    if (module)
        wasm_runtime_unload(module);
    free(wasm_buf);
    return result;
}

void wamrc_free_aot_file(uint8_t *aot_bytes)
{
    if (aot_bytes)
        wasm_runtime_free(aot_bytes);
}

/* --- Low-level API --------------------------------------------------- */

wamrc_compiler_t wamrc_load(const uint8_t *wasm_bytes, uint32_t wasm_size,
                            char *error_buf, uint32_t error_buf_size)
{
    if (!wasm_bytes || wasm_size == 0) {
        if (error_buf && error_buf_size > 0)
            snprintf(error_buf, error_buf_size, "Invalid wasm bytes");
        return NULL;
    }

    compiler_wrapper *w = (compiler_wrapper *)malloc(sizeof(compiler_wrapper));
    if (!w) {
        if (error_buf && error_buf_size > 0)
            snprintf(error_buf, error_buf_size, "Failed to allocate compiler wrapper");
        return NULL;
    }

    w->wasm_buf = (uint8_t *)malloc(wasm_size);
    if (!w->wasm_buf) {
        free(w);
        if (error_buf && error_buf_size > 0)
            snprintf(error_buf, error_buf_size, "Failed to allocate wasm buffer");
        return NULL;
    }
    memcpy(w->wasm_buf, wasm_bytes, wasm_size);

    w->module = wasm_runtime_load(w->wasm_buf, wasm_size,
                                  error_buf, error_buf_size);
    if (!w->module) {
        free(w->wasm_buf);
        free(w);
        return NULL;
    }

    return (wamrc_compiler_t)w;
}

uint8_t *wamrc_emit(wamrc_compiler_t compiler,
                    const wamrc_options_t *options,
                    uint32_t *out_aot_size,
                    char *error_buf, uint32_t error_buf_size)
{
    wamrc_options_t default_opts;

    if (!compiler) {
        if (error_buf && error_buf_size > 0)
            snprintf(error_buf, error_buf_size, "Compiler handle is NULL");
        return NULL;
    }

    if (!options) {
        wamrc_options_init_default(&default_opts);
        options = &default_opts;
    }

    compiler_wrapper *w = (compiler_wrapper *)compiler;
    return compile_module(w->module, options, out_aot_size,
                          error_buf, error_buf_size);
}

void wamrc_unload(wamrc_compiler_t compiler)
{
    if (!compiler) return;
    compiler_wrapper *w = (compiler_wrapper *)compiler;
    wasm_runtime_unload(w->module);
    free(w->wasm_buf);
    free(w);
}
