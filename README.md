# wamr-plugin

Native plugin wrapping [WAMR](https://github.com/bytecodealliance/wasm-micro-runtime) (WebAssembly Micro Runtime) and [Binaryen](https://github.com/WebAssembly/binaryen) for cross-platform WASM execution and assembly. Used by both Unity (C# P/Invoke) and the Python backend (ctypes).

## What this builds

Desktop builds produce three variant-suffixed shared libraries:

| Variant | Platforms | Library name | Description |
|---------|-----------|-------------|-------------|
| **fast-interp** | All desktop | `libwamr-fast-interp.so/.dylib/.dll` | Fast interpreter, no JIT |
| **llvm-jit** | All desktop | `libwamr-llvm-jit.so/.dylib/.dll` | LLVM JIT + AOT compiler |
| **fast-jit** | Linux x86_64 | `libwamr-fast-jit.so` | WAMR lightweight JIT |

Mobile and embedded builds produce a single static library (interpreter-only, no variant suffix):

| Platform | Library |
|----------|---------|
| Android (arm64, armv7, x86, x86_64) | `libwamr-static.a` |
| iOS (arm64) | `libwamr-static.a` |
| Emscripten (wasm32) | `libwamr-static.a` (stub) |

- **macOS** produces universal binaries (arm64 + x86_64) via `lipo` for each variant.
- **Binaryen** is built on desktop only. Its `wasm-as` tool is used at build time to assemble `.wat` test modules.
- The **Emscripten/WebGL** build produces stub-only bindings (WAMR-in-WASM is not meaningful; browser-native WASM is used instead).

## WAMR Runtime API (`wamr_plugin_api.h`)

### Global lifecycle

| Function | Description |
|----------|-------------|
| `wamr_init` | Initialize the global WAMR runtime (ref-counted, idempotent) |
| `wamr_destroy` | Decrement runtime ref count (actual teardown at zero) |
| `wamr_get_version_string` | Returns e.g. `"2.4.4 (interp, fast-jit)"` |

**Important:** In the Unity Editor, the runtime should be initialized once and kept alive for the entire process lifetime. Calling `wamr_destroy` during Editor shutdown can race with JIT thread teardown on Windows and cause crashes. Let the OS reclaim resources on process exit.

### Module and instance management

| Function | Description |
|----------|-------------|
| `wamr_load_module` | Load .wasm bytes (copied internally; caller may free) |
| `wamr_unload_module` | Unload a module |
| `wamr_create_instance` | Instantiate with stack/heap size |
| `wamr_destroy_instance` | Destroy an instance |

### Function invocation

| Function | Description |
|----------|-------------|
| `wamr_find_function` | Look up an exported function by name |
| `wamr_call` | Invoke with uint32 argv (args in, result out) |
| `wamr_get_exception` | Error string after a failed call |

### Linear memory access

| Function | Description |
|----------|-------------|
| `wamr_module_malloc` / `wamr_module_free` | Allocate/free in WASM linear memory |
| `wamr_write_memory` / `wamr_read_memory` | Copy between host and WASM memory |
| `wamr_validate_memory` | Check bounds of a WASM pointer |

### Running mode control

| Function | Description |
|----------|-------------|
| `wamr_is_running_mode_supported` | Query whether a mode is available in this build |
| `wamr_set_default_running_mode` | Set the default mode for new instances (global) |
| `wamr_set_running_mode` | Change mode on an existing instance (per-instance) |
| `wamr_get_running_mode` | Get the current mode of an instance |

Modes: `1` = Interpreter, `2` = Fast JIT, `3` = LLVM JIT, `4` = Multi-Tier JIT.

The default running mode is global, but can be overridden per-instance without restarting the runtime.

## AOT Compiler API (`wamrc_plugin_api.h`)

Desktop/Editor-only API for ahead-of-time compilation of `.wasm` to `.aot` files.

| Function | Description |
|----------|-------------|
| `wamrc_init` / `wamrc_destroy` | Global lifecycle (idempotent; destroy is a no-op) |
| `wamrc_get_version_string` | Compiler version string |
| `wamrc_get_last_error` | Last error message |
| `wamrc_options_init_default` | Initialize `wamrc_options_t` with defaults |
| `wamrc_compile` | High-level: .wasm bytes → .aot bytes in one call |
| `wamrc_free_aot_file` | Free the .aot buffer returned by `wamrc_compile` |
| `wamrc_load` / `wamrc_emit` / `wamrc_unload` | Low-level: load once, compile for multiple targets |

`wamrc_options_t` fields: `target_arch`, `target_abi`, `target_cpu`, `cpu_features`, `opt_level`, `size_level`, and feature flags for bulk memory, SIMD, tail calls, ref types, and multi-threading.

Thread safety: NOT thread-safe. All calls must be serialized (Unity main thread).

## Building

Requires CMake 3.21+ and git submodules:

```bash
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release -V
```

## Submodules

| Submodule | Version | Purpose |
|-----------|---------|---------|
| `wamr/` | WAMR-2.4.4 | WebAssembly interpreter + JIT runtime |
| `binaryen/` | version_128 | WASM assembler (`wasm-as`) and shared library |

## CI

The GitHub Actions workflow (`.github/workflows/unity.yml`) builds variant-suffixed libraries for all desktop platforms (fast-interp, llvm-jit, fast-jit) plus interpreter-only static libraries for mobile/embedded targets. Artifacts are consumed by `download_plugins.ps1` in the main project. macOS arm64 and x86_64 are built separately and combined into universal binaries via `lipo` for each variant.
