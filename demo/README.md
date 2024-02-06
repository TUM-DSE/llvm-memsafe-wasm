# Demo

## PAC Testing

To test the PAC features, which consist of a module pass that inserts custom wasm instructions that sign (`pointer_sign`) and authenticate (`pointer_auth`) a pointer before it is stored to a memory address:
- Disable MTE (in wasmtime or simply don't generate our MTE-based custom instructions in llvm/clang), since PAC can currently not effectively work when MTE is used (no PAC instructions would be inserted due to limits in our LLVM analysis).
- Compile demo C file `test-prevent-real-attack.c` without optimizations (`-O0`), since the code that tests PAC would otherwise be optimized away.

## Building

```shell
for i in ../demo/demo*.c; do
  ./build/bin/clang --target=wasm64-unknown-wasi --sysroot /scratch/martin/src/wasm/wasi-libc/sysroot -g -fsanitize=wasm-memsafety -Os -rtlib=compiler-rt wasm_memsafety_rtlib.c "$i" -o "${i%.c}.wasm"
done
```

Wasmtime can be cross-compiled for aarch64 with the provided `Dockerfile` in this directory.

## Running

### `demo.c`

```shell
./wasmtime compile demo.wasm --cranelift-enable use_mte --wasm-features=memory64,mem-safety
./wasmtime run --allow-precompiled --wasm-features=memory64,mem-safety -- demo.cwasm [index] [value]
```

### `demo-heap.c`

```shell
./wasmtime compile demo-heap.wasm --cranelift-enable use_mte --wasm-features=memory64,mem-safety
./wasmtime run --allow-precompiled --wasm-features=memory64,mem-safety -- demo-heap.cwasm [heap_size] [index] [value]
```

### `demo-use-after-free.c`

```shell
./wasmtime compile demo-use-after-free.wasm --cranelift-enable use_mte --wasm-features=memory64,mem-safety
./wasmtime run --allow-precompiled --wasm-features=memory64,mem-safety -- demo-use-after-free.cwasm [heap_size] [index] [value] [early_free]
```

### `demo-scanf.c`

```shell
./wasmtime compile demo-scanf.wasm --cranelift-enable use_mte --wasm-features=memory64,mem-safety
./wasmtime run --allow-precompiled --wasm-features=memory64,mem-safety -- demo-scanf.cwasm
```

## Checking generated code

You can test what code is generated with the following commands, even if you are on a machine that is not aarch64 or doesn't support mte.

```shell
./wasmtime compile --target aarch64-unknown-linux-gnu --cranelift-enable use_mte --wasm-features=memory64,mem-safety demo-<name>.c
llvm-objdump -D demo-<name>.cwasm > demo-<name>.s
```

