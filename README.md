# wamr-plugin

Unity native plugin wrapping [WAMR](https://github.com/bytecodealliance/wasm-micro-runtime) (WebAssembly Micro Runtime) and [Binaryen](https://github.com/WebAssembly/binaryen) for cross-platform WASM execution and assembly.

## What this builds

| Component | Desktop (Win/macOS/Linux) | Mobile (Android/iOS) | WebGL |
|-----------|--------------------------|----------------------|-------|
| **libwamr** (shared) | .dll / .dylib / .so | -- | -- |
| **libwamr** (static) | .a | .a | .a (stub) |
| **libbinaryen** (shared) | .dll / .dylib / .so | -- | -- |

- **WAMR** is configured as a fast interpreter with builtin libc, no WASI, no JIT/AOT, no threading.
- **Binaryen** is built on desktop only. Its `wasm-as` tool is used at build time to assemble `.wat` test modules, and the shared library is distributed for runtime use (e.g. assembling WASM modules from text).
- The **Emscripten/WebGL** build produces stub-only WAMR bindings (WAMR-in-WASM is not meaningful; browser-native WASM is used instead).

## WAMR C API

The thin C wrapper (`src/wamr_unity_api.h`) exposes:

- `wamr_init` / `wamr_destroy` -- global runtime lifecycle
- `wamr_load_module` / `wamr_unload_module` -- load .wasm bytes (copied internally)
- `wamr_create_instance` / `wamr_destroy_instance` -- instantiate with stack/heap size
- `wamr_find_function` -- look up an exported function by name
- `wamr_call` -- invoke a function with uint32 argv (serves as both args and return value)
- `wamr_module_malloc` / `wamr_module_free` -- allocate/free in WASM linear memory
- `wamr_write_memory` / `wamr_read_memory` / `wamr_validate_memory` -- buffer passing

Thread safety: WAMR instances are **not** thread-safe. All calls for a given instance must happen on the same thread.

## Building

Requires CMake 3.21+ and git submodules:

```bash
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Submodules

| Submodule | Version | Purpose |
|-----------|---------|---------|
| `wamr/` | WAMR-2.4.4 | WebAssembly interpreter runtime |
| `binaryen/` | version_128 | WASM assembler (`wasm-as`) and shared library |

## CI

The GitHub Actions workflow (`.github/workflows/unity.yml`) builds for all 7 Unity target platforms and produces artifacts consumed by `download_plugins.ps1` in the main project.
