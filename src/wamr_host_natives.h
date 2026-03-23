/*
 * wamr_host_natives.h — Registration of host-provided native functions.
 */

#ifndef WAMR_HOST_NATIVES_H
#define WAMR_HOST_NATIVES_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Register LZ4, xxHash, and BLAKE3 native functions with the WAMR runtime.
 * Must be called after wasm_runtime_init() and before loading modules.
 * Returns true on success. */
bool wamr_host_natives_init(void);

#ifdef __cplusplus
}
#endif

#endif /* WAMR_HOST_NATIVES_H */
