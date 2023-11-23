# Demo

## Building

```shell
for i in ../demo/demo*.c; do
  ./build/bin/clang --target=wasm64-unknown-wasi -mmem-safety --sysroot ../wasi-sdk/build/wasi-sdk-20.32gb3d5dd44687d/share/wasi-sysroot -g -fsanitize=wasm-memsafety -Os -g "$i" -o "${i%.c}.wasm"
done
```

Wasmtime can be cross-compiled for aarch64 with the provided `Dockerfile` in this directory.

## Running

### `demo.c`

```shell
./wasmtime run -W memory64=y -W mem-safet=y -C mte=y -- demo.wasm [index] [value]
```

### `demo-heap.c`

```shell
./wasmtime run -W memory64=y -W mem-safet=y -C mte=y -- demo-heap.wasm [heap_size] [index] [value]
```

### `demo-use-after-free.c`

```shell
./wasmtime run -W memory64=y -W mem-safet=y -C mte=y -- demo-use-after-free.wasm [heap_size] [index] [value] [early_free]
```

### `demo-scanf.c`

```shell
./wasmtime run -W memory64=y -W mem-safet=y -C mte=y -- demo-scanf.cwasm
```
