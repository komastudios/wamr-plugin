;; Trivial test module: exports an "add" function that adds two i32 values,
;; plus a linear memory for buffer-passing tests.
;; Compile with: wat2wasm add.wat -o add.wasm
;; The binary form is embedded directly in test_wamr.c as ADD_WASM[].

(module
  (func (export "add") (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.add)
  (memory (export "memory") 1))
