;; Smoke test for libhost_native (iwasm --native-lib).
;;
;; Imports host-provided native functions from LZ4, xxHash, and BLAKE3,
;; calls each one, and returns 0 on success or a non-zero error code
;; identifying which check failed.
;;
;; Run:  iwasm --native-lib=libhost_native.dylib native_lib_test.wasm
;; Exit: 0 = all checks passed

(module
  (import "env" "LZ4_compressBound" (func $lz4_bound (param i32) (result i32)))
  (import "env" "XXH32" (func $xxh32 (param i32 i32 i32) (result i32)))
  (import "env" "blake3_hasher_new" (func $blake3_new (result i32)))
  (import "env" "blake3_hasher_delete" (func $blake3_delete (param i32)))

  (memory (export "memory") 1)

  (func (export "main") (result i32)
    (local $hasher i32)

    ;; --- Check 1: LZ4_compressBound(100) must be > 0 ---
    (if (i32.le_s (call $lz4_bound (i32.const 100)) (i32.const 0))
      (then (return (i32.const 1)))
    )

    ;; --- Check 2: XXH32(NULL, 0, 0) must equal 0x02CC5D05 ---
    (if (i32.ne
          (call $xxh32 (i32.const 0) (i32.const 0) (i32.const 0))
          (i32.const 0x02CC5D05))
      (then (return (i32.const 2)))
    )

    ;; --- Check 3: blake3_hasher_new must return non-zero (heap addr) ---
    (local.set $hasher (call $blake3_new))
    (if (i32.eqz (local.get $hasher))
      (then (return (i32.const 3)))
    )
    (call $blake3_delete (local.get $hasher))

    ;; All checks passed
    (i32.const 0)
  )
)
