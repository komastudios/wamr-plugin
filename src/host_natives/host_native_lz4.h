#ifndef HOST_NATIVE_LZ4_H
#define HOST_NATIVE_LZ4_H

/* Initialize LZ4 host natives and register with WAMR.
 * lib_path: path or name of the LZ4 shared library (NULL = platform default).
 * Returns 0 on success, non-zero on failure. */
int host_native_lz4_init(const char *lib_path);

#endif
