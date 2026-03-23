/*
 * host_natives.c — Aggregates and initializes all host-provided native modules.
 */

#include "host_natives.h"
#include "host_native_lz4.h"
#include "host_native_xxhash.h"
#include "host_native_blake3.h"
#include <stddef.h>

bool wamr_host_natives_init(void)
{
    bool ok = true;
    ok = host_native_lz4_init(NULL) && ok;
    ok = host_native_xxhash_init(NULL) && ok;
    ok = host_native_blake3_init(NULL) && ok;
    return ok;
}
