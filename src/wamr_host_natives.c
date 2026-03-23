/*
 * wamr_host_natives.c — Host-provided native functions for WASM modules.
 *
 * Registers LZ4, xxHash, and BLAKE3 functions as WAMR native imports so
 * WASM modules can call them via "env" module imports.
 *
 * Dynamic builds (shared lib): resolves symbols at runtime via dlsym/GetProcAddress
 * from libraries already loaded by the host process.
 * Static builds (mobile): links directly against extern C symbols.
 */

#include "wamr_host_natives.h"

#ifdef WAMR_STUB_ONLY

bool wamr_host_natives_init(void) { return true; }

#else

#include "wasm_export.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* Expected function signatures from host libraries                   */
/* ------------------------------------------------------------------ */

/* LZ4 */
typedef int (*lz4_compress_default_fn)(const char *src, char *dst,
                                       int srcSize, int dstCapacity);
typedef int (*lz4_decompress_safe_fn)(const char *src, char *dst,
                                      int compressedSize, int dstCapacity);
typedef int (*lz4_compress_bound_fn)(int inputSize);

/* xxHash */
typedef unsigned int (*xxh32_fn)(const void *input, size_t length,
                                 unsigned int seed);
typedef unsigned long long (*xxh3_64bits_fn)(const void *input, size_t length);

/* BLAKE3 */
typedef void (*blake3_hasher_init_fn)(void *hasher);
typedef void (*blake3_hasher_update_fn)(void *hasher, const void *input,
                                        size_t input_len);
typedef void (*blake3_hasher_finalize_fn)(const void *hasher, void *out,
                                          size_t out_len);

/* ------------------------------------------------------------------ */
/* Symbol resolution                                                  */
/* ------------------------------------------------------------------ */

#ifdef WAMR_HOST_NATIVES_STATIC
/* Static builds: link directly against global symbols */
extern int LZ4_compress_default(const char *, char *, int, int);
extern int LZ4_decompress_safe(const char *, char *, int, int);
extern int LZ4_compressBound(int);
extern unsigned int XXH32(const void *, size_t, unsigned int);
extern unsigned long long XXH3_64bits(const void *, size_t);
extern void blake3_hasher_init(void *);
extern void blake3_hasher_update(void *, const void *, size_t);
extern void blake3_hasher_finalize(const void *, void *, size_t);

static lz4_compress_default_fn s_lz4_compress   = LZ4_compress_default;
static lz4_decompress_safe_fn  s_lz4_decompress = LZ4_decompress_safe;
static lz4_compress_bound_fn   s_lz4_bound      = LZ4_compressBound;
static xxh32_fn                s_xxh32           = XXH32;
static xxh3_64bits_fn          s_xxh3_64         = XXH3_64bits;
static blake3_hasher_init_fn     s_blake3_init     = blake3_hasher_init;
static blake3_hasher_update_fn   s_blake3_update   = blake3_hasher_update;
static blake3_hasher_finalize_fn s_blake3_finalize = blake3_hasher_finalize;

#else
/* Dynamic builds: resolve at runtime */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static void *load_lib(const char *name)
{
    return (void *)LoadLibraryA(name);
}

static void *lib_symbol(void *lib, const char *name)
{
    return (void *)GetProcAddress((HMODULE)lib, name);
}
#else
#include <dlfcn.h>

static void *load_lib(const char *name)
{
    return dlopen(name, RTLD_NOW | RTLD_GLOBAL);
}

static void *lib_symbol(void *lib, const char *name)
{
    return dlsym(lib, name);
}
#endif

static lz4_compress_default_fn s_lz4_compress;
static lz4_decompress_safe_fn  s_lz4_decompress;
static lz4_compress_bound_fn   s_lz4_bound;
static xxh32_fn                s_xxh32;
static xxh3_64bits_fn          s_xxh3_64;
static blake3_hasher_init_fn     s_blake3_init;
static blake3_hasher_update_fn   s_blake3_update;
static blake3_hasher_finalize_fn s_blake3_finalize;

static void resolve_from(void *lib, const char *sym_name, void **out)
{
    if (lib) *out = lib_symbol(lib, sym_name);
}

static void resolve_all(void)
{
    /* Load libraries by name — the OS resolves the platform-specific
       filename (liblz4.so / liblz4.dylib / lz4.dll etc.) */
#ifdef _WIN32
    void *lz4    = load_lib("lz4.dll");
    void *xxhash = load_lib("xxhash.dll");
    void *blake3 = load_lib("blake3.dll");
#elif defined(__APPLE__)
    void *lz4    = load_lib("liblz4.dylib");
    void *xxhash = load_lib("libxxhash.dylib");
    void *blake3 = load_lib("libblake3.dylib");
#else
    void *lz4    = load_lib("liblz4.so");
    void *xxhash = load_lib("libxxhash.so");
    void *blake3 = load_lib("libblake3.so");
#endif

    resolve_from(lz4, "LZ4_compress_default", (void **)&s_lz4_compress);
    resolve_from(lz4, "LZ4_decompress_safe",  (void **)&s_lz4_decompress);
    resolve_from(lz4, "LZ4_compressBound",    (void **)&s_lz4_bound);

    resolve_from(xxhash, "XXH32",      (void **)&s_xxh32);
    resolve_from(xxhash, "XXH3_64bits", (void **)&s_xxh3_64);

    resolve_from(blake3, "blake3_hasher_init",     (void **)&s_blake3_init);
    resolve_from(blake3, "blake3_hasher_update",   (void **)&s_blake3_update);
    resolve_from(blake3, "blake3_hasher_finalize", (void **)&s_blake3_finalize);
}
#endif /* WAMR_HOST_NATIVES_STATIC */

/* ------------------------------------------------------------------ */
/* WAMR native function implementations                               */
/* First parameter is always wasm_exec_env_t.                         */
/* Signature notation: * = pointer (auto WASM→native conversion),     */
/*                     ~ = preceding pointer's length (i32).          */
/* ------------------------------------------------------------------ */

static int32_t native_lz4_compress(wasm_exec_env_t exec_env,
                                   const char *src, int32_t src_size,
                                   char *dst, int32_t dst_capacity)
{
    if (!s_lz4_compress) return -1;
    return s_lz4_compress(src, dst, src_size, dst_capacity);
}

static int32_t native_lz4_decompress(wasm_exec_env_t exec_env,
                                     const char *src, int32_t compressed_size,
                                     char *dst, int32_t dst_capacity)
{
    if (!s_lz4_decompress) return -1;
    return s_lz4_decompress(src, dst, compressed_size, dst_capacity);
}

static int32_t native_lz4_compress_bound(wasm_exec_env_t exec_env,
                                         int32_t input_size)
{
    if (!s_lz4_bound) return -1;
    return s_lz4_bound(input_size);
}

static uint32_t native_xxh32(wasm_exec_env_t exec_env,
                             const void *data, uint32_t len, uint32_t seed)
{
    if (!s_xxh32) return 0;
    return (uint32_t)s_xxh32(data, (size_t)len, (unsigned int)seed);
}

static void native_xxh3_64(wasm_exec_env_t exec_env,
                           const void *data, uint32_t len, uint64_t *out)
{
    if (!s_xxh3_64 || !out) return;
    *out = (uint64_t)s_xxh3_64(data, (size_t)len);
}

/* BLAKE3 hasher state is 1912 bytes. We use a stack-local one. */
#define BLAKE3_HASHER_SIZE 1912

static void native_blake3(wasm_exec_env_t exec_env,
                          const void *data, uint32_t len, void *out_hash)
{
    if (!s_blake3_init || !s_blake3_update || !s_blake3_finalize || !out_hash)
        return;
    char hasher[BLAKE3_HASHER_SIZE];
    memset(hasher, 0, sizeof(hasher));
    s_blake3_init(hasher);
    s_blake3_update(hasher, data, (size_t)len);
    s_blake3_finalize(hasher, out_hash, 32);
}

/* ------------------------------------------------------------------ */
/* Registration table                                                 */
/* ------------------------------------------------------------------ */

static NativeSymbol s_native_symbols[] = {
    { "rs_host_lz4_compress",       (void *)native_lz4_compress,       "(*~*~)i", NULL },
    { "rs_host_lz4_decompress",     (void *)native_lz4_decompress,     "(*~*~)i", NULL },
    { "rs_host_lz4_compress_bound", (void *)native_lz4_compress_bound, "(i)i",    NULL },
    { "rs_host_xxh32",              (void *)native_xxh32,              "(*~i)i",  NULL },
    { "rs_host_xxh3_64",            (void *)native_xxh3_64,            "(*~*)",   NULL },
    { "rs_host_blake3",             (void *)native_blake3,             "(*~*)",   NULL },
};

#define N_NATIVE_SYMBOLS (sizeof(s_native_symbols) / sizeof(s_native_symbols[0]))

bool wamr_host_natives_init(void)
{
#ifndef WAMR_HOST_NATIVES_STATIC
    resolve_all();
#endif
    return wasm_runtime_register_natives("env",
                                         s_native_symbols,
                                         N_NATIVE_SYMBOLS);
}

#endif /* WAMR_STUB_ONLY */
