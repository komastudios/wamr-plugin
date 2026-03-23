#ifndef HOST_NATIVE_XXHASH_H
#define HOST_NATIVE_XXHASH_H

#include <stdbool.h>

/* Initialize xxHash host natives and register with WAMR.
 * lib_path: path or name of the xxHash shared library (NULL = platform default).
 * For static builds (WAMR_HOST_NATIVES_STATIC), lib_path is ignored. */
bool host_native_xxhash_init(const char *lib_path);

#endif
