/*
 * host_native_lib.c — WAMR native-lib entry point for iwasm --native-lib.
 *
 * Aggregates LZ4, xxHash, and BLAKE3 host native symbols into a single
 * NativeSymbol array that WAMR registers automatically after loading.
 */

#ifdef HOST_NATIVE_EXPORT_SYMBOLS
  #ifdef _WIN32
    #define HOST_NATIVE_EXPORT __declspec(dllexport)
  #else
    #define HOST_NATIVE_EXPORT __attribute__((visibility("default")))
  #endif
#else
  #define HOST_NATIVE_EXPORT
#endif

#include "wasm_export.h"
#include "host_native_lz4.h"
#include "host_native_xxhash.h"
#include "host_native_blake3.h"

#include <string.h>

static NativeSymbol s_all_symbols[32];
static uint32_t s_n_symbols = 0;

static void
collect_symbols(void)
{
    NativeSymbol *syms;
    uint32_t count;

    s_n_symbols = 0;

    count = host_native_lz4_get_symbols(&syms);
    memcpy(&s_all_symbols[s_n_symbols], syms, count * sizeof(NativeSymbol));
    s_n_symbols += count;

    count = host_native_xxhash_get_symbols(&syms);
    memcpy(&s_all_symbols[s_n_symbols], syms, count * sizeof(NativeSymbol));
    s_n_symbols += count;

    count = host_native_blake3_get_symbols(&syms);
    memcpy(&s_all_symbols[s_n_symbols], syms, count * sizeof(NativeSymbol));
    s_n_symbols += count;
}

HOST_NATIVE_EXPORT uint32_t
get_native_lib(char **p_module_name, NativeSymbol **p_native_symbols)
{
    if (s_n_symbols == 0)
        collect_symbols();

    *p_module_name = "env";
    *p_native_symbols = s_all_symbols;
    return s_n_symbols;
}
