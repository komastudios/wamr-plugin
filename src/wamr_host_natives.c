/*
 * wamr_host_natives.c — Host-provided native functions for WASM modules.
 *
 * Registers LZ4 and xxHash functions as WAMR native imports so WASM modules
 * can call them via "env" module imports. Function names and signatures match
 * the original C library APIs exactly.
 *
 * Links statically against lz4-static and xxhash-static (from third_party/).
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
/*                                                                    */
/* Signatures use 'i' for all params (including pointers). Address    */
/* conversion and bounds checking is done manually per the WAMR docs. */
/* ------------------------------------------------------------------ */

/* int LZ4_compress_default(const char *src, char *dst,
 *                          int srcSize, int dstCapacity) */
static int32_t native_LZ4_compress_default(wasm_exec_env_t exec_env,
                                           uint32_t src_offset, uint32_t dst_offset,
                                           int32_t src_size, int32_t dst_capacity)
{
    wasm_module_inst_t module_inst = get_module_inst(exec_env);

    if (src_size <= 0 || dst_capacity <= 0)
        return 0;
    if (!validate_app_addr(src_offset, src_size))
        return 0;
    if (!validate_app_addr(dst_offset, dst_capacity))
        return 0;

    const char *src = addr_app_to_native(src_offset);
    char *dst = addr_app_to_native(dst_offset);
    return LZ4_compress_default(src, dst, src_size, dst_capacity);
}

/* int LZ4_decompress_safe(const char *src, char *dst,
 *                         int compressedSize, int dstCapacity) */
static int32_t native_LZ4_decompress_safe(wasm_exec_env_t exec_env,
                                          uint32_t src_offset, uint32_t dst_offset,
                                          int32_t compressed_size, int32_t dst_capacity)
{
    wasm_module_inst_t module_inst = get_module_inst(exec_env);

    if (compressed_size <= 0 || dst_capacity <= 0)
        return -1;
    if (!validate_app_addr(src_offset, compressed_size))
        return -1;
    if (!validate_app_addr(dst_offset, dst_capacity))
        return -1;

    const char *src = addr_app_to_native(src_offset);
    char *dst = addr_app_to_native(dst_offset);
    return LZ4_decompress_safe(src, dst, compressed_size, dst_capacity);
}

/* int LZ4_compressBound(int inputSize) */
static int32_t native_LZ4_compressBound(wasm_exec_env_t exec_env,
                                        int32_t input_size)
{
    return LZ4_compressBound(input_size);
}

/* XXH32_hash_t XXH32(const void *input, size_t length, XXH32_hash_t seed) */
static uint32_t native_XXH32(wasm_exec_env_t exec_env,
                             uint32_t input_offset, uint32_t length, uint32_t seed)
{
    wasm_module_inst_t module_inst = get_module_inst(exec_env);

    if (length == 0)
        return (uint32_t)XXH32(NULL, 0, seed);
    if (!validate_app_addr(input_offset, length))
        return 0;

    const void *input = addr_app_to_native(input_offset);
    return (uint32_t)XXH32(input, (size_t)length, (unsigned int)seed);
}

/* XXH64_hash_t XXH3_64bits(const void *input, size_t length) */
static uint64_t native_XXH3_64bits(wasm_exec_env_t exec_env,
                                   uint32_t input_offset, uint32_t length)
{
    wasm_module_inst_t module_inst = get_module_inst(exec_env);

    if (length == 0)
        return (uint64_t)XXH3_64bits(NULL, 0);
    if (!validate_app_addr(input_offset, length))
        return 0;

    const void *input = addr_app_to_native(input_offset);
    return (uint64_t)XXH3_64bits(input, (size_t)length);
}

/* ------------------------------------------------------------------ */
/* Registration table                                                 */
/* ------------------------------------------------------------------ */

static NativeSymbol s_native_symbols[] = {
    { "LZ4_compress_default", (void *)native_LZ4_compress_default, "(iiii)i", NULL },
    { "LZ4_decompress_safe",  (void *)native_LZ4_decompress_safe,  "(iiii)i", NULL },
    { "LZ4_compressBound",    (void *)native_LZ4_compressBound,    "(i)i",    NULL },
    { "XXH32",                (void *)native_XXH32,                "(iii)i",  NULL },
    { "XXH3_64bits",          (void *)native_XXH3_64bits,          "(ii)I",   NULL },
};

#define N_NATIVE_SYMBOLS (sizeof(s_native_symbols) / sizeof(s_native_symbols[0]))

bool wamr_host_natives_init(void)
{
    return wasm_runtime_register_natives("env",
                                         s_native_symbols,
                                         N_NATIVE_SYMBOLS);
}

#endif /* WAMR_STUB_ONLY */
