# Demo

## Building

```shell
for i in ../demo/demo*.c; do
  ./build/bin/clang --target=wasm64-unknown-wasi -mmem-safety --sysroot /scratch/martin/src/wasm/wasi-libc/sysroot -g -fsanitize=wasm-memsafety -Os -rtlib=compiler-rt wasm_memsafety_rtlib.c "$i" -o "${i%.c}.wasm"
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
