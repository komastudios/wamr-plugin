#ifndef HOST_NATIVE_XXHASH_H
#define HOST_NATIVE_XXHASH_H

#include <stdint.h>

/* Initialize xxHash host natives and register with WAMR.
 * lib_path: path or name of the xxHash shared library (NULL = platform default).
 * Returns 0 on success, non-zero on failure. */
int host_native_xxhash_init(const char *lib_path);

/* Return the NativeSymbol array for xxHash (without registering).
 * Used by the host_native_lib entry point for iwasm --native-lib. */
struct NativeSymbol;
uint32_t host_native_xxhash_get_symbols(struct NativeSymbol **p_symbols);

#endif
