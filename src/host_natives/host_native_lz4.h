#ifndef HOST_NATIVE_LZ4_H
#define HOST_NATIVE_LZ4_H

#include <stdint.h>

/* Initialize LZ4 host natives and register with WAMR.
 * lib_path: path or name of the LZ4 shared library (NULL = platform default).
 * Returns 0 on success, non-zero on failure. */
int host_native_lz4_init(const char *lib_path);

/* Return the NativeSymbol array for LZ4 (without registering).
 * Used by the host_native_lib entry point for iwasm --native-lib. */
struct NativeSymbol;
uint32_t host_native_lz4_get_symbols(struct NativeSymbol **p_symbols);

#endif
