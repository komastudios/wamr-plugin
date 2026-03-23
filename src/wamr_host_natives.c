/*
 * wamr_host_natives.c — Host-provided native functions for WASM modules.
 *
 * Registers LZ4, xxHash, and BLAKE3 functions as WAMR native imports so
 * WASM modules can call them via "env" module imports.
 *
 * Links statically against lz4, xxhash, and blake3 (from third_party/).
 */

#include "wamr_host_natives.h"

#ifdef WAMR_STUB_ONLY

bool wamr_host_natives_init(void) { return true; }

#else

#include "wasm_export.h"
#include <lz4.h>
#include <xxhash.h>

/* ------------------------------------------------------------------ */
/* WAMR native function implementations                               */
/* First parameter is always wasm_exec_env_t.                         */
/* Signature notation: * = pointer (auto WASM→native conversion),     */
/*                     ~ = preceding pointer's length (i32).          */
/* ------------------------------------------------------------------ */

static int32_t native_lz4_compress(wasm_exec_env_t exec_env,
                                   const char *src, int32_t src_size,
                                   char *dst, int32_t dst_capacity)
{
    return LZ4_compress_default(src, dst, src_size, dst_capacity);
}

static int32_t native_lz4_decompress(wasm_exec_env_t exec_env,
                                     const char *src, int32_t compressed_size,
                                     char *dst, int32_t dst_capacity)
{
    return LZ4_decompress_safe(src, dst, compressed_size, dst_capacity);
}

static int32_t native_lz4_compress_bound(wasm_exec_env_t exec_env,
                                         int32_t input_size)
{
    return LZ4_compressBound(input_size);
}

static uint32_t native_xxh32(wasm_exec_env_t exec_env,
                             const void *data, uint32_t len, uint32_t seed)
{
    return (uint32_t)XXH32(data, (size_t)len, (unsigned int)seed);
}

static void native_xxh3_64(wasm_exec_env_t exec_env,
                           const void *data, uint32_t len, uint64_t *out)
{
    if (!out) return;
    *out = (uint64_t)XXH3_64bits(data, (size_t)len);
}

/* ------------------------------------------------------------------ */
/* Registration table                                                 */
/* ------------------------------------------------------------------ */

static NativeSymbol s_native_symbols[] = {
    { "LZ4_compress_default", (void *)native_lz4_compress,       "(*~*~)i", NULL },
    { "LZ4_decompress_safe",  (void *)native_lz4_decompress,     "(*~*~)i", NULL },
    { "LZ4_compressBound",    (void *)native_lz4_compress_bound, "(i)i",    NULL },
    { "XXH32",                (void *)native_xxh32,              "(*~i)i",  NULL },
    { "XXH3_64bits",          (void *)native_xxh3_64,            "(*~*)",   NULL },
};

#define N_NATIVE_SYMBOLS (sizeof(s_native_symbols) / sizeof(s_native_symbols[0]))

bool wamr_host_natives_init(void)
{
    return wasm_runtime_register_natives("env",
                                         s_native_symbols,
                                         N_NATIVE_SYMBOLS);
}

#endif /* WAMR_STUB_ONLY */
