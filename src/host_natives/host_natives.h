#ifndef HOST_NATIVES_H
#define HOST_NATIVES_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Register all host-provided native functions with the WAMR runtime.
 * Must be called after wasm_runtime_init() and before loading modules.
 * Returns true if all libraries were resolved and registered. */
bool wamr_host_natives_init(void);

#ifdef __cplusplus
}
#endif

#endif
