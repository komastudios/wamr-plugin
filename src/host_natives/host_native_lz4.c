/*
 * host_native_lz4.c — LZ4 host-provided native functions for WASM modules.
 */

#include "host_native_lz4.h"

#include "wasm_export.h"
#include <stddef.h>

/* --- Function pointer typedefs (match LZ4 public API) --- */

typedef int (*fn_LZ4_compress_default)(const char *src, char *dst,
                                       int srcSize, int dstCapacity);
typedef int (*fn_LZ4_decompress_safe)(const char *src, char *dst,
                                      int compressedSize, int dstCapacity);
typedef int (*fn_LZ4_compressBound)(int inputSize);

/* --- Symbol resolution --- */

#ifdef WAMR_HOST_NATIVES_STATIC

extern int LZ4_compress_default(const char *, char *, int, int);
extern int LZ4_decompress_safe(const char *, char *, int, int);
extern int LZ4_compressBound(int);

static fn_LZ4_compress_default s_compress   = LZ4_compress_default;
static fn_LZ4_decompress_safe s_decompress  = LZ4_decompress_safe;
static fn_LZ4_compressBound   s_bound       = LZ4_compressBound;

#else

#include "host_native_resolve.h"

static fn_LZ4_compress_default s_compress;
static fn_LZ4_decompress_safe s_decompress;
static fn_LZ4_compressBound   s_bound;

static bool resolve(const char *lib_path)
{
    void *lib = hn_load_lib(lib_path ? lib_path : HN_LZ4_DEFAULT);
    if (!lib) return false;

    s_compress   = (fn_LZ4_compress_default)hn_get_sym(lib, "LZ4_compress_default");
    s_decompress = (fn_LZ4_decompress_safe)hn_get_sym(lib, "LZ4_decompress_safe");
    s_bound      = (fn_LZ4_compressBound)hn_get_sym(lib, "LZ4_compressBound");

    return s_compress && s_decompress && s_bound;
}

#endif /* WAMR_HOST_NATIVES_STATIC */

/* --- WAMR native wrappers --- */

static int32_t native_LZ4_compress_default(wasm_exec_env_t exec_env,
                                           uint32_t src_offset, uint32_t dst_offset,
                                           int32_t src_size, int32_t dst_capacity)
{
    wasm_module_inst_t module_inst = get_module_inst(exec_env);
    if (!s_compress || src_size <= 0 || dst_capacity <= 0) return 0;
    if (!validate_app_addr(src_offset, src_size)) return 0;
    if (!validate_app_addr(dst_offset, dst_capacity)) return 0;

    return s_compress(addr_app_to_native(src_offset),
                      addr_app_to_native(dst_offset),
                      src_size, dst_capacity);
}

static int32_t native_LZ4_decompress_safe(wasm_exec_env_t exec_env,
                                          uint32_t src_offset, uint32_t dst_offset,
                                          int32_t compressed_size, int32_t dst_capacity)
{
    wasm_module_inst_t module_inst = get_module_inst(exec_env);
    if (!s_decompress || compressed_size <= 0 || dst_capacity <= 0) return -1;
    if (!validate_app_addr(src_offset, compressed_size)) return -1;
    if (!validate_app_addr(dst_offset, dst_capacity)) return -1;

    return s_decompress(addr_app_to_native(src_offset),
                        addr_app_to_native(dst_offset),
                        compressed_size, dst_capacity);
}

static int32_t native_LZ4_compressBound(wasm_exec_env_t exec_env,
                                        int32_t input_size)
{
    if (!s_bound) return -1;
    return s_bound(input_size);
}

/* --- Registration table --- */

static NativeSymbol s_lz4_symbols[] = {
    { "LZ4_compress_default", (void *)native_LZ4_compress_default, "(iiii)i", NULL },
    { "LZ4_decompress_safe",  (void *)native_LZ4_decompress_safe,  "(iiii)i", NULL },
    { "LZ4_compressBound",    (void *)native_LZ4_compressBound,    "(i)i",    NULL },
};

int host_native_lz4_init(const char *lib_path)
{
    int err = 0;
#ifndef WAMR_HOST_NATIVES_STATIC
    if (!resolve(lib_path))
        err = 1;
#else
    (void)lib_path;
#endif
    if (!wasm_runtime_register_natives("env", s_lz4_symbols,
            sizeof(s_lz4_symbols) / sizeof(NativeSymbol)))
        err = 1;
    return err;
}
