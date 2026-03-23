/*
 * host_native_blake3.c — BLAKE3 host-provided native functions for WASM modules.
 *
 * Provides the standard blake3_hasher API plus two additional methods:
 *   blake3_hasher_new    — allocates a blake3_hasher on the WASM heap
 *   blake3_hasher_delete — frees a blake3_hasher from the WASM heap
 */

#include "host_native_blake3.h"

#ifdef WAMR_STUB_ONLY
bool host_native_blake3_init(const char *lib_path) { (void)lib_path; return true; }
#else

#include "wasm_export.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* blake3_hasher size: must match the struct in blake3.h.
 * 8*4 (key) + chunk_state(8*4 + 8 + 64 + 1 + 1 + 1) + 1 + (54+1)*32 = 1912 bytes */
#define BLAKE3_HASHER_SIZE 1912
#define BLAKE3_OUT_LEN     32
#define BLAKE3_KEY_LEN     32

/* --- Function pointer typedefs (match blake3.h public API) --- */

typedef void (*fn_blake3_hasher_init)(void *self);
typedef void (*fn_blake3_hasher_init_keyed)(void *self, const uint8_t key[32]);
typedef void (*fn_blake3_hasher_init_derive_key)(void *self, const char *context);
typedef void (*fn_blake3_hasher_update)(void *self, const void *input, size_t input_len);
typedef void (*fn_blake3_hasher_finalize)(const void *self, uint8_t *out, size_t out_len);
typedef void (*fn_blake3_hasher_reset)(void *self);

/* --- Symbol resolution --- */

#ifdef WAMR_HOST_NATIVES_STATIC

extern void blake3_hasher_init(void *);
extern void blake3_hasher_init_keyed(void *, const uint8_t[32]);
extern void blake3_hasher_init_derive_key(void *, const char *);
extern void blake3_hasher_update(void *, const void *, size_t);
extern void blake3_hasher_finalize(const void *, uint8_t *, size_t);
extern void blake3_hasher_reset(void *);

static fn_blake3_hasher_init            s_init            = blake3_hasher_init;
static fn_blake3_hasher_init_keyed      s_init_keyed      = blake3_hasher_init_keyed;
static fn_blake3_hasher_init_derive_key s_init_derive_key = blake3_hasher_init_derive_key;
static fn_blake3_hasher_update          s_update          = blake3_hasher_update;
static fn_blake3_hasher_finalize        s_finalize        = blake3_hasher_finalize;
static fn_blake3_hasher_reset           s_reset           = blake3_hasher_reset;

#else

#include "host_native_resolve.h"

static fn_blake3_hasher_init            s_init;
static fn_blake3_hasher_init_keyed      s_init_keyed;
static fn_blake3_hasher_init_derive_key s_init_derive_key;
static fn_blake3_hasher_update          s_update;
static fn_blake3_hasher_finalize        s_finalize;
static fn_blake3_hasher_reset           s_reset;

static bool resolve(const char *lib_path)
{
    void *lib = hn_load_lib(lib_path ? lib_path : HN_BLAKE3_DEFAULT);
    if (!lib) return false;

    s_init            = (fn_blake3_hasher_init)hn_get_sym(lib, "blake3_hasher_init");
    s_init_keyed      = (fn_blake3_hasher_init_keyed)hn_get_sym(lib, "blake3_hasher_init_keyed");
    s_init_derive_key = (fn_blake3_hasher_init_derive_key)hn_get_sym(lib, "blake3_hasher_init_derive_key");
    s_update          = (fn_blake3_hasher_update)hn_get_sym(lib, "blake3_hasher_update");
    s_finalize        = (fn_blake3_hasher_finalize)hn_get_sym(lib, "blake3_hasher_finalize");
    s_reset           = (fn_blake3_hasher_reset)hn_get_sym(lib, "blake3_hasher_reset");

    return s_init && s_update && s_finalize;
}

#endif /* WAMR_HOST_NATIVES_STATIC */

/* --- WAMR native wrappers --- */

/* blake3_hasher_new() -> wasm_ptr
 * Allocates a blake3_hasher on the WASM heap and returns the WASM address. */
static uint32_t native_blake3_hasher_new(wasm_exec_env_t exec_env)
{
    wasm_module_inst_t module_inst = get_module_inst(exec_env);
    void *native_addr = NULL;
    uint32_t wasm_addr = (uint32_t)module_malloc(BLAKE3_HASHER_SIZE, &native_addr);
    if (wasm_addr && native_addr)
        memset(native_addr, 0, BLAKE3_HASHER_SIZE);
    return wasm_addr;
}

/* blake3_hasher_delete(wasm_ptr) */
static void native_blake3_hasher_delete(wasm_exec_env_t exec_env, uint32_t hasher_offset)
{
    wasm_module_inst_t module_inst = get_module_inst(exec_env);
    module_free(hasher_offset);
}

/* blake3_hasher_init(hasher*) */
static void native_blake3_hasher_init(wasm_exec_env_t exec_env, uint32_t hasher_offset)
{
    wasm_module_inst_t module_inst = get_module_inst(exec_env);
    if (!s_init) return;
    if (!validate_app_addr(hasher_offset, BLAKE3_HASHER_SIZE)) return;
    s_init(addr_app_to_native(hasher_offset));
}

/* blake3_hasher_init_keyed(hasher*, key*) */
static void native_blake3_hasher_init_keyed(wasm_exec_env_t exec_env,
                                            uint32_t hasher_offset, uint32_t key_offset)
{
    wasm_module_inst_t module_inst = get_module_inst(exec_env);
    if (!s_init_keyed) return;
    if (!validate_app_addr(hasher_offset, BLAKE3_HASHER_SIZE)) return;
    if (!validate_app_addr(key_offset, BLAKE3_KEY_LEN)) return;
    s_init_keyed(addr_app_to_native(hasher_offset),
                 addr_app_to_native(key_offset));
}

/* blake3_hasher_init_derive_key(hasher*, context$) */
static void native_blake3_hasher_init_derive_key(wasm_exec_env_t exec_env,
                                                 uint32_t hasher_offset, uint32_t context_offset)
{
    wasm_module_inst_t module_inst = get_module_inst(exec_env);
    if (!s_init_derive_key) return;
    if (!validate_app_addr(hasher_offset, BLAKE3_HASHER_SIZE)) return;
    if (!wasm_runtime_validate_app_str_addr(module_inst, (uint64_t)context_offset)) return;
    s_init_derive_key(addr_app_to_native(hasher_offset),
                      addr_app_to_native(context_offset));
}

/* blake3_hasher_update(hasher*, input*, input_len) */
static void native_blake3_hasher_update(wasm_exec_env_t exec_env,
                                        uint32_t hasher_offset,
                                        uint32_t input_offset, uint32_t input_len)
{
    wasm_module_inst_t module_inst = get_module_inst(exec_env);
    if (!s_update) return;
    if (!validate_app_addr(hasher_offset, BLAKE3_HASHER_SIZE)) return;
    if (input_len > 0 && !validate_app_addr(input_offset, input_len)) return;
    s_update(addr_app_to_native(hasher_offset),
             input_len > 0 ? addr_app_to_native(input_offset) : NULL,
             (size_t)input_len);
}

/* blake3_hasher_finalize(hasher*, out*, out_len) */
static void native_blake3_hasher_finalize(wasm_exec_env_t exec_env,
                                          uint32_t hasher_offset,
                                          uint32_t out_offset, uint32_t out_len)
{
    wasm_module_inst_t module_inst = get_module_inst(exec_env);
    if (!s_finalize) return;
    if (!validate_app_addr(hasher_offset, BLAKE3_HASHER_SIZE)) return;
    if (!validate_app_addr(out_offset, out_len)) return;
    s_finalize(addr_app_to_native(hasher_offset),
               addr_app_to_native(out_offset),
               (size_t)out_len);
}

/* blake3_hasher_reset(hasher*) */
static void native_blake3_hasher_reset(wasm_exec_env_t exec_env, uint32_t hasher_offset)
{
    wasm_module_inst_t module_inst = get_module_inst(exec_env);
    if (!s_reset) return;
    if (!validate_app_addr(hasher_offset, BLAKE3_HASHER_SIZE)) return;
    s_reset(addr_app_to_native(hasher_offset));
}

/* --- Registration table --- */

static NativeSymbol s_blake3_symbols[] = {
    { "blake3_hasher_new",            (void *)native_blake3_hasher_new,            "()i",    NULL },
    { "blake3_hasher_delete",         (void *)native_blake3_hasher_delete,         "(i)",    NULL },
    { "blake3_hasher_init",           (void *)native_blake3_hasher_init,           "(i)",    NULL },
    { "blake3_hasher_init_keyed",     (void *)native_blake3_hasher_init_keyed,     "(ii)",   NULL },
    { "blake3_hasher_init_derive_key",(void *)native_blake3_hasher_init_derive_key,"(ii)",   NULL },
    { "blake3_hasher_update",         (void *)native_blake3_hasher_update,         "(iii)",  NULL },
    { "blake3_hasher_finalize",       (void *)native_blake3_hasher_finalize,       "(iii)",  NULL },
    { "blake3_hasher_reset",          (void *)native_blake3_hasher_reset,          "(i)",    NULL },
};

bool host_native_blake3_init(const char *lib_path)
{
#ifndef WAMR_HOST_NATIVES_STATIC
    if (!resolve(lib_path))
        return false;
#else
    (void)lib_path;
#endif
    return wasm_runtime_register_natives(
        "env", s_blake3_symbols,
        sizeof(s_blake3_symbols) / sizeof(NativeSymbol));
}

#endif /* WAMR_STUB_ONLY */
