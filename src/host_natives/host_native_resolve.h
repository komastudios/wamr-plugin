/*
 * host_native_resolve.h — Platform-agnostic dynamic library loading helpers.
 */

#ifndef HOST_NATIVE_RESOLVE_H
#define HOST_NATIVE_RESOLVE_H

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static inline void *hn_load_lib(const char *name) { return (void *)LoadLibraryA(name); }
static inline void *hn_get_sym(void *lib, const char *name) { return (void *)GetProcAddress((HMODULE)lib, name); }
#else
#include <dlfcn.h>
static inline void *hn_load_lib(const char *name) { return dlopen(name, RTLD_NOW | RTLD_GLOBAL); }
static inline void *hn_get_sym(void *lib, const char *name) { return dlsym(lib, name); }
#endif

/* Default library names per platform */
#ifdef _WIN32
#define HN_LZ4_DEFAULT    "lz4.dll"
#define HN_XXHASH_DEFAULT "xxhash.dll"
#elif defined(__APPLE__)
#define HN_LZ4_DEFAULT    "liblz4.dylib"
#define HN_XXHASH_DEFAULT "libxxhash.dylib"
#else
#define HN_LZ4_DEFAULT    "liblz4.so"
#define HN_XXHASH_DEFAULT "libxxhash.so"
#endif

#endif /* HOST_NATIVE_RESOLVE_H */
