#ifndef HOST_NATIVES_H
#define HOST_NATIVES_H

#ifdef __cplusplus
extern "C" {
#endif

/* Register all host-provided native functions with the WAMR runtime.
 * Must be called after wasm_runtime_init() and before loading modules.
 * Returns 0 on success, non-zero on failure. */
int wamr_host_natives_init(void);

#ifdef __cplusplus
}
#endif

#endif
