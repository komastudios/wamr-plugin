/*
 * wamrc_plugin_api.h — Thin C wrapper around WAMR's AOT compiler.
 *
 * Enables AOT compilation of .wasm → .aot from the Unity Editor via P/Invoke.
 * The compiler statically links LLVM and is Editor-only (desktop platforms).
 *
 * Thread safety: NOT thread-safe. All calls must be serialized (Unity main thread).
 * LLVM lifecycle: wamrc_init is idempotent, wamrc_destroy is a no-op.
 * LLVM cannot re-initialize after shutdown, so we never call LLVMShutdown.
 */

#ifndef WAMRC_PLUGIN_API_H
#define WAMRC_PLUGIN_API_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- DLL export/import macros --- */
#if defined(_WIN32)
    #if defined(WAMR_PLUGIN_EXPORT)
        #define WAMR_API __declspec(dllexport)
    #elif defined(WAMR_PLUGIN_IMPORT)
        #define WAMR_API __declspec(dllimport)
    #else
        #define WAMR_API
    #endif
#elif defined(WAMR_PLUGIN_EXPORT)
    #define WAMR_API __attribute__((visibility("default")))
#else
    #define WAMR_API
#endif

/* --- Opaque handle --- */
typedef void *wamrc_compiler_t;

/* --- Compilation options --- */
typedef struct {
    const char *target_arch;       /* "x86_64", "aarch64", "arm", NULL=host */
    const char *target_abi;        /* "gnu", "msvc", NULL=default */
    const char *target_cpu;        /* NULL=default */
    const char *cpu_features;      /* NULL=none */
    uint32_t    opt_level;         /* 0..3, default 3 */
    uint32_t    size_level;        /* 0..3, default 3 */
    bool        enable_bulk_memory;
    bool        enable_simd;
    bool        enable_tail_call;
    bool        enable_ref_types;
    bool        enable_multi_thread;
} wamrc_options_t;

/* --- Global lifecycle (once per process) --- */
WAMR_API bool        wamrc_init(void);
WAMR_API void        wamrc_destroy(void);
WAMR_API const char *wamrc_get_version_string(void);
WAMR_API const char *wamrc_get_last_error(void);

/* --- Options --- */
WAMR_API void        wamrc_options_init_default(wamrc_options_t *opts);

/* --- High-level: .wasm bytes → .aot bytes in one call ---
 * Returns malloc'd buffer on success (caller frees via wamrc_free_aot_file).
 * Returns NULL on failure (error written to error_buf). */
WAMR_API uint8_t    *wamrc_compile(const uint8_t *wasm_bytes, uint32_t wasm_size,
                                   const wamrc_options_t *options,
                                   uint32_t *out_aot_size,
                                   char *error_buf, uint32_t error_buf_size);
WAMR_API void        wamrc_free_aot_file(uint8_t *aot_bytes);

/* --- Low-level: load once, compile for multiple targets --- */
WAMR_API wamrc_compiler_t wamrc_load(const uint8_t *wasm_bytes, uint32_t wasm_size,
                                     char *error_buf, uint32_t error_buf_size);
WAMR_API uint8_t         *wamrc_emit(wamrc_compiler_t compiler,
                                     const wamrc_options_t *options,
                                     uint32_t *out_aot_size,
                                     char *error_buf, uint32_t error_buf_size);
WAMR_API void             wamrc_unload(wamrc_compiler_t compiler);

#ifdef __cplusplus
}
#endif

#endif /* WAMRC_PLUGIN_API_H */
