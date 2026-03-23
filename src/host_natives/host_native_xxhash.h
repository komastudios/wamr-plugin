#ifndef HOST_NATIVE_XXHASH_H
#define HOST_NATIVE_XXHASH_H

/* Initialize xxHash host natives and register with WAMR.
 * lib_path: path or name of the xxHash shared library (NULL = platform default).
 * Returns 0 on success, non-zero on failure. */
int host_native_xxhash_init(const char *lib_path);

#endif
