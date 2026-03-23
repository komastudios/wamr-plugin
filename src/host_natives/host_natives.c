/*
 * host_natives.c — Aggregates and initializes all host-provided native modules.
 */

#include "host_natives.h"
#include "host_native_lz4.h"
#include "host_native_xxhash.h"
#include "host_native_blake3.h"
#include <stddef.h>

int wamr_host_natives_init(void)
{
    int err = 0;
    err |= host_native_lz4_init(NULL);
    err |= host_native_xxhash_init(NULL);
    err |= host_native_blake3_init(NULL);
    return err;
}
