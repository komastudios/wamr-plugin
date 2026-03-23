#ifndef HOST_NATIVE_BLAKE3_H
#define HOST_NATIVE_BLAKE3_H

#include <stdbool.h>

/* Initialize BLAKE3 host natives and register with WAMR.
 * lib_path: path or name of the BLAKE3 shared library (NULL = platform default).
 * For static builds (WAMR_HOST_NATIVES_STATIC), lib_path is ignored. */
bool host_native_blake3_init(const char *lib_path);

#endif
