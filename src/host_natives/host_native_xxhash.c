/*
 * host_native_xxhash.c — xxHash host-provided native functions for WASM modules.
 */

#include "host_native_xxhash.h"

#include "wasm_export.h"
#include <stddef.h>
#include <stdint.h>

/* --- Function pointer typedefs (match xxHash public API) --- */

typedef unsigned int       (*fn_XXH32)(const void *input, size_t length, unsigned int seed);
typedef unsigned long long (*fn_XXH3_64bits)(const void *input, size_t length);

/* --- Symbol resolution --- */

#ifdef WAMR_HOST_NATIVES_STATIC

extern unsigned int       XXH32(const void *, size_t, unsigned int);
extern unsigned long long XXH3_64bits(const void *, size_t);

static fn_XXH32       s_xxh32    = XXH32;
static fn_XXH3_64bits s_xxh3_64  = XXH3_64bits;

#else

#include "host_native_resolve.h"

static fn_XXH32       s_xxh32;
static fn_XXH3_64bits s_xxh3_64;

static bool resolve(const char *lib_path)
{
    void *lib = hn_load_lib(lib_path ? lib_path : HN_XXHASH_DEFAULT);
    if (!lib) return false;

    s_xxh32   = (fn_XXH32)hn_get_sym(lib, "XXH32");
    s_xxh3_64 = (fn_XXH3_64bits)hn_get_sym(lib, "XXH3_64bits");

    return s_xxh32 && s_xxh3_64;
}

#endif /* WAMR_HOST_NATIVES_STATIC */

/* --- WAMR native wrappers --- */

static uint32_t native_XXH32(wasm_exec_env_t exec_env,
                             uint32_t input_offset, uint32_t length, uint32_t seed)
{
    wasm_module_inst_t module_inst = get_module_inst(exec_env);
    if (!s_xxh32) return 0;
    if (length == 0)
        return (uint32_t)s_xxh32(NULL, 0, seed);
    if (!validate_app_addr(input_offset, length))
        return 0;

    return (uint32_t)s_xxh32(addr_app_to_native(input_offset),
                             (size_t)length, (unsigned int)seed);
}

static uint64_t native_XXH3_64bits(wasm_exec_env_t exec_env,
                                   uint32_t input_offset, uint32_t length)
{
    wasm_module_inst_t module_inst = get_module_inst(exec_env);
    if (!s_xxh3_64) return 0;
    if (length == 0)
        return (uint64_t)s_xxh3_64(NULL, 0);
    if (!validate_app_addr(input_offset, length))
        return 0;

    return (uint64_t)s_xxh3_64(addr_app_to_native(input_offset),
                               (size_t)length);
}

/* --- Registration table --- */

static NativeSymbol s_xxhash_symbols[] = {
    { "XXH32",       (void *)native_XXH32,       "(iii)i", NULL },
    { "XXH3_64bits", (void *)native_XXH3_64bits, "(ii)I",  NULL },
};

int host_native_xxhash_init(const char *lib_path)
{
    int err = 0;
#ifndef WAMR_HOST_NATIVES_STATIC
    if (!resolve(lib_path))
        err = 1;
#else
    (void)lib_path;
#endif
    if (!wasm_runtime_register_natives("env", s_xxhash_symbols,
            sizeof(s_xxhash_symbols) / sizeof(NativeSymbol)))
        err = 1;
    return err;
}
