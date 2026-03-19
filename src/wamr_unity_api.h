/*
 * wamr_unity_api.h — Thin C wrapper around WAMR for Unity P/Invoke.
 *
 * This is NOT a generic WAMR binding. It exposes only what the replay
 * serialization layer needs: load a .wasm module, call exported functions
 * with typed arguments, and read/write linear memory for buffer passing.
 *
 * Thread safety: instances are NOT thread-safe. All calls for a given
 * wamr_instance_t must happen on the same thread (Unity main thread).
 */

#ifndef WAMR_UNITY_API_H
#define WAMR_UNITY_API_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- DLL export/import macros --- */
#if defined(_WIN32)
    #if defined(WAMR_UNITY_EXPORT)
        #define WAMR_API __declspec(dllexport)
    #elif defined(WAMR_UNITY_IMPORT)
        #define WAMR_API __declspec(dllimport)
    #else
        #define WAMR_API
    #endif
#elif defined(WAMR_UNITY_EXPORT)
    #define WAMR_API __attribute__((visibility("default")))
#else
    #define WAMR_API
#endif

/* --- Opaque handle types --- */
typedef void *wamr_module_t;
typedef void *wamr_instance_t;
typedef void *wamr_func_t;

/* --- Global lifecycle --- */
WAMR_API bool     wamr_init(void);
WAMR_API void     wamr_destroy(void);

/* --- Version info ---
 * Returns a string like "2.4.4 (interp, fast-jit)".
 * The pointer is valid for the lifetime of the process. */
WAMR_API const char *wamr_get_version_string(void);

/* --- Module loading ---
 * wasm_bytes are copied internally; the caller may free them after this call.
 * Returns NULL on failure (error written to error_buf). */
WAMR_API wamr_module_t wamr_load_module(const uint8_t *wasm_bytes,
                                        uint32_t       size,
                                        char          *error_buf,
                                        uint32_t       error_buf_size);
WAMR_API void           wamr_unload_module(wamr_module_t module);

/* --- Instance lifecycle ---
 * stack_size: WASM operand/call stack (bytes). 32768 is a safe default.
 * heap_size:  WASM linear-memory heap managed by the allocator (bytes). */
WAMR_API wamr_instance_t wamr_create_instance(wamr_module_t module,
                                              uint32_t      stack_size,
                                              uint32_t      heap_size,
                                              char         *error_buf,
                                              uint32_t      error_buf_size);
WAMR_API void             wamr_destroy_instance(wamr_instance_t instance);

/* --- Function lookup ---
 * Returns NULL if the function is not exported by the module. */
WAMR_API wamr_func_t wamr_find_function(wamr_instance_t instance,
                                        const char     *name);

/* --- Function invocation ---
 * argv serves as both input arguments and output results.
 *   i32/f32 params: one uint32_t per arg (floats via memcpy of their bits).
 *   i64/f64 params: two consecutive uint32_t per arg (little-endian).
 * On success the return value is written to argv[0] (argv[0..1] for 64-bit).
 * Returns false on error; call wamr_get_exception for details. */
WAMR_API bool wamr_call(wamr_instance_t instance,
                        wamr_func_t     func,
                        uint32_t        argc,
                        uint32_t       *argv);

/* --- Error information after a failed wamr_call --- */
WAMR_API const char *wamr_get_exception(wamr_instance_t instance);

/* --- Linear memory access ---
 * wamr_module_malloc returns a WASM-address offset (0 on failure).
 * write/read_memory copy between host buffers and WASM linear memory.
 * validate_memory checks that [wasm_ptr, wasm_ptr+size) is in bounds. */
WAMR_API uint32_t wamr_module_malloc(wamr_instance_t instance, uint32_t size);
WAMR_API void     wamr_module_free(wamr_instance_t instance, uint32_t wasm_ptr);

WAMR_API bool wamr_write_memory(wamr_instance_t instance,
                                uint32_t        wasm_ptr,
                                const void     *data,
                                uint32_t        size);
WAMR_API bool wamr_read_memory(wamr_instance_t instance,
                               uint32_t        wasm_ptr,
                               void           *data,
                               uint32_t        size);
WAMR_API bool wamr_validate_memory(wamr_instance_t instance,
                                   uint32_t        wasm_ptr,
                                   uint32_t        size);

/* --- Running mode control ---
 * Modes: 1 = Interp, 2 = Fast JIT, 3 = LLVM JIT, 4 = Multi-Tier JIT.
 * Query support before setting. Per-instance functions need the wrapper
 * handle (not a raw wasm_module_inst_t). */
WAMR_API bool     wamr_is_running_mode_supported(int32_t mode);
WAMR_API bool     wamr_set_default_running_mode(int32_t mode);
WAMR_API bool     wamr_set_running_mode(wamr_instance_t instance, int32_t mode);
WAMR_API int32_t  wamr_get_running_mode(wamr_instance_t instance);

#ifdef __cplusplus
}
#endif

#endif /* WAMR_UNITY_API_H */
