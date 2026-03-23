#ifndef HOST_NATIVE_BLAKE3_H
#define HOST_NATIVE_BLAKE3_H

/* Initialize BLAKE3 host natives and register with WAMR.
 * lib_path: path or name of the BLAKE3 shared library (NULL = platform default).
 * Returns 0 on success, non-zero on failure. */
int host_native_blake3_init(const char *lib_path);

#endif
