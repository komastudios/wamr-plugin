#include "wamr_unity_api.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* Stub-only build (Emscripten / platforms where WAMR is meaningless) */
/* ------------------------------------------------------------------ */
#ifdef WAMR_STUB_ONLY

bool wamr_init(void) { return false; }
void wamr_destroy(void) { }
const char *wamr_get_version_string(void) { return "stub (WAMR not available)"; }

wamr_module_t wamr_load_module(const uint8_t *wasm_bytes, uint32_t size,
                               char *error_buf, uint32_t error_buf_size)
{
    if (error_buf && error_buf_size > 0)
        snprintf(error_buf, error_buf_size, "WAMR not available on this platform");
    return NULL;
}

void wamr_unload_module(wamr_module_t module) { (void)module; }

wamr_instance_t wamr_create_instance(wamr_module_t module, uint32_t stack_size,
                                     uint32_t heap_size, char *error_buf,
                                     uint32_t error_buf_size)
{
    (void)module; (void)stack_size; (void)heap_size;
    if (error_buf && error_buf_size > 0)
        snprintf(error_buf, error_buf_size, "WAMR not available on this platform");
    return NULL;
}

void         wamr_destroy_instance(wamr_instance_t inst) { (void)inst; }
wamr_func_t  wamr_find_function(wamr_instance_t inst, const char *n)
    { (void)inst; (void)n; return NULL; }
bool         wamr_call(wamr_instance_t inst, wamr_func_t f, uint32_t c, uint32_t *v)
    { (void)inst; (void)f; (void)c; (void)v; return false; }
const char  *wamr_get_exception(wamr_instance_t inst)
    { (void)inst; return "WAMR not available on this platform"; }
uint32_t     wamr_module_malloc(wamr_instance_t inst, uint32_t sz)
    { (void)inst; (void)sz; return 0; }
void         wamr_module_free(wamr_instance_t inst, uint32_t p)
    { (void)inst; (void)p; }
bool         wamr_write_memory(wamr_instance_t inst, uint32_t p,
                               const void *d, uint32_t sz)
    { (void)inst; (void)p; (void)d; (void)sz; return false; }
bool         wamr_read_memory(wamr_instance_t inst, uint32_t p,
                              void *d, uint32_t sz)
    { (void)inst; (void)p; (void)d; (void)sz; return false; }
bool         wamr_validate_memory(wamr_instance_t inst, uint32_t p, uint32_t sz)
    { (void)inst; (void)p; (void)sz; return false; }
bool         wamr_is_running_mode_supported(int32_t mode) { (void)mode; return false; }
bool         wamr_set_default_running_mode(int32_t mode) { (void)mode; return false; }
bool         wamr_set_running_mode(wamr_instance_t inst, int32_t mode)
    { (void)inst; (void)mode; return false; }
int32_t      wamr_get_running_mode(wamr_instance_t inst)
    { (void)inst; return 0; }

#else /* !WAMR_STUB_ONLY — real implementation */

/* ------------------------------------------------------------------ */
/* Full WAMR implementation                                           */
/* ------------------------------------------------------------------ */
#include "wasm_export.h"

/* Internal wrapper: keeps the malloc'd WASM buffer alive alongside the module
 * (WAMR references the buffer for the lifetime of the loaded module). */
typedef struct {
    wasm_module_t module;
    uint8_t      *wasm_buf;
} module_wrapper;

/* Internal wrapper: pairs the module instance with its execution environment
 * so callers never deal with exec_env directly. */
typedef struct {
    wasm_module_inst_t module_inst;
    wasm_exec_env_t    exec_env;
} instance_wrapper;

/* --- Global lifecycle ------------------------------------------------ */

bool wamr_init(void)
{
    return wasm_runtime_init();
}

void wamr_destroy(void)
{
    wasm_runtime_destroy();
}

/* --- Version info ---------------------------------------------------- */

static char s_version_string[128] = { 0 };

const char *wamr_get_version_string(void)
{
    if (s_version_string[0] != '\0')
        return s_version_string;

    uint32_t major, minor, patch;
    wasm_runtime_get_version(&major, &minor, &patch);

    /* Build list of supported running modes */
    char modes[64] = { 0 };
    int  pos = 0;

    typedef struct { RunningMode mode; const char *name; } mode_entry;
    static const mode_entry entries[] = {
        { Mode_Interp,         "interp" },
        { Mode_Fast_JIT,       "fast-jit" },
        { Mode_LLVM_JIT,       "llvm-jit" },
        { Mode_Multi_Tier_JIT, "multi-tier-jit" },
    };

    for (unsigned i = 0; i < sizeof(entries) / sizeof(entries[0]); i++) {
        if (wasm_runtime_is_running_mode_supported(entries[i].mode)) {
            if (pos > 0)
                pos += snprintf(modes + pos, sizeof(modes) - pos, ", ");
            pos += snprintf(modes + pos, sizeof(modes) - pos, "%s",
                            entries[i].name);
        }
    }

    snprintf(s_version_string, sizeof(s_version_string),
             "%u.%u.%u (%s)", major, minor, patch,
             pos > 0 ? modes : "no runtime modes");

    return s_version_string;
}

/* --- Module loading -------------------------------------------------- */

wamr_module_t wamr_load_module(const uint8_t *wasm_bytes, uint32_t size,
                               char *error_buf, uint32_t error_buf_size)
{
    if (!wasm_bytes || size == 0) {
        if (error_buf && error_buf_size > 0)
            snprintf(error_buf, error_buf_size, "Invalid wasm bytes");
        return NULL;
    }

    module_wrapper *w = (module_wrapper *)malloc(sizeof(module_wrapper));
    if (!w) {
        if (error_buf && error_buf_size > 0)
            snprintf(error_buf, error_buf_size, "Failed to allocate module wrapper");
        return NULL;
    }

    /* Copy the bytecode — WAMR holds a reference into this buffer. */
    w->wasm_buf = (uint8_t *)malloc(size);
    if (!w->wasm_buf) {
        free(w);
        if (error_buf && error_buf_size > 0)
            snprintf(error_buf, error_buf_size, "Failed to allocate wasm buffer");
        return NULL;
    }
    memcpy(w->wasm_buf, wasm_bytes, size);

    w->module = wasm_runtime_load(w->wasm_buf, size, error_buf, error_buf_size);
    if (!w->module) {
        free(w->wasm_buf);
        free(w);
        return NULL;
    }

    return (wamr_module_t)w;
}

void wamr_unload_module(wamr_module_t module)
{
    if (!module) return;
    module_wrapper *w = (module_wrapper *)module;
    wasm_runtime_unload(w->module);
    free(w->wasm_buf);
    free(w);
}

/* --- Instance lifecycle ---------------------------------------------- */

wamr_instance_t wamr_create_instance(wamr_module_t module,
                                     uint32_t stack_size, uint32_t heap_size,
                                     char *error_buf, uint32_t error_buf_size)
{
    if (!module) {
        if (error_buf && error_buf_size > 0)
            snprintf(error_buf, error_buf_size, "Module is NULL");
        return NULL;
    }

    module_wrapper *mw = (module_wrapper *)module;

    instance_wrapper *w = (instance_wrapper *)malloc(sizeof(instance_wrapper));
    if (!w) {
        if (error_buf && error_buf_size > 0)
            snprintf(error_buf, error_buf_size, "Failed to allocate instance wrapper");
        return NULL;
    }

    w->module_inst = wasm_runtime_instantiate(mw->module, stack_size, heap_size,
                                              error_buf, error_buf_size);
    if (!w->module_inst) {
        free(w);
        return NULL;
    }

    w->exec_env = wasm_runtime_create_exec_env(w->module_inst, stack_size);
    if (!w->exec_env) {
        if (error_buf && error_buf_size > 0)
            snprintf(error_buf, error_buf_size,
                     "Failed to create execution environment");
        wasm_runtime_deinstantiate(w->module_inst);
        free(w);
        return NULL;
    }

    return (wamr_instance_t)w;
}

void wamr_destroy_instance(wamr_instance_t instance)
{
    if (!instance) return;
    instance_wrapper *w = (instance_wrapper *)instance;
    wasm_runtime_destroy_exec_env(w->exec_env);
    wasm_runtime_deinstantiate(w->module_inst);
    free(w);
}

/* --- Function lookup ------------------------------------------------- */

wamr_func_t wamr_find_function(wamr_instance_t instance, const char *name)
{
    if (!instance || !name) return NULL;
    instance_wrapper *w = (instance_wrapper *)instance;
    return (wamr_func_t)wasm_runtime_lookup_function(w->module_inst, name);
}

/* --- Function invocation --------------------------------------------- */

bool wamr_call(wamr_instance_t instance, wamr_func_t func,
               uint32_t argc, uint32_t *argv)
{
    if (!instance || !func) return false;
    instance_wrapper *w = (instance_wrapper *)instance;
    return wasm_runtime_call_wasm(w->exec_env,
                                 (wasm_function_inst_t)func,
                                 argc, argv);
}

const char *wamr_get_exception(wamr_instance_t instance)
{
    if (!instance) return "Instance is NULL";
    instance_wrapper *w = (instance_wrapper *)instance;
    return wasm_runtime_get_exception(w->module_inst);
}

/* --- Linear memory access -------------------------------------------- */

uint32_t wamr_module_malloc(wamr_instance_t instance, uint32_t size)
{
    if (!instance || size == 0) return 0;
    instance_wrapper *w = (instance_wrapper *)instance;
    void *native_ptr = NULL;
    return (uint32_t)wasm_runtime_module_malloc(w->module_inst, size, &native_ptr);
}

void wamr_module_free(wamr_instance_t instance, uint32_t wasm_ptr)
{
    if (!instance || wasm_ptr == 0) return;
    instance_wrapper *w = (instance_wrapper *)instance;
    wasm_runtime_module_free(w->module_inst, (uint64_t)wasm_ptr);
}

bool wamr_write_memory(wamr_instance_t instance, uint32_t wasm_ptr,
                       const void *data, uint32_t size)
{
    if (!instance || !data || size == 0) return false;
    instance_wrapper *w = (instance_wrapper *)instance;

    if (!wasm_runtime_validate_app_addr(w->module_inst,
                                        (uint64_t)wasm_ptr, (uint64_t)size))
        return false;

    void *native = wasm_runtime_addr_app_to_native(w->module_inst,
                                                    (uint64_t)wasm_ptr);
    if (!native) return false;
    memcpy(native, data, size);
    return true;
}

bool wamr_read_memory(wamr_instance_t instance, uint32_t wasm_ptr,
                      void *data, uint32_t size)
{
    if (!instance || !data || size == 0) return false;
    instance_wrapper *w = (instance_wrapper *)instance;

    if (!wasm_runtime_validate_app_addr(w->module_inst,
                                        (uint64_t)wasm_ptr, (uint64_t)size))
        return false;

    void *native = wasm_runtime_addr_app_to_native(w->module_inst,
                                                    (uint64_t)wasm_ptr);
    if (!native) return false;
    memcpy(data, native, size);
    return true;
}

bool wamr_validate_memory(wamr_instance_t instance,
                          uint32_t wasm_ptr, uint32_t size)
{
    if (!instance) return false;
    instance_wrapper *w = (instance_wrapper *)instance;
    return wasm_runtime_validate_app_addr(w->module_inst,
                                          (uint64_t)wasm_ptr, (uint64_t)size);
}

/* --- Running mode control -------------------------------------------- */

bool wamr_is_running_mode_supported(int32_t mode)
{
    return wasm_runtime_is_running_mode_supported((RunningMode)mode);
}

bool wamr_set_default_running_mode(int32_t mode)
{
    return wasm_runtime_set_default_running_mode((RunningMode)mode);
}

bool wamr_set_running_mode(wamr_instance_t instance, int32_t mode)
{
    if (!instance) return false;
    instance_wrapper *w = (instance_wrapper *)instance;
    return wasm_runtime_set_running_mode(w->module_inst, (RunningMode)mode);
}

int32_t wamr_get_running_mode(wamr_instance_t instance)
{
    if (!instance) return 0;
    instance_wrapper *w = (instance_wrapper *)instance;
    return (int32_t)wasm_runtime_get_running_mode(w->module_inst);
}

#endif /* WAMR_STUB_ONLY */
