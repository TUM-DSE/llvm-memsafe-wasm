CC=/scratch/fritz/src/safe-wasm/llvm-project/build/bin/clang
# WASM_FLAGS=--target=wasm64-unknown-wasi --sysroot /scratch/martin/src/wasm/wasi-libc/sysroot -g -D_WASI_EMULATED_PROCESS_CLOCKS -lwasi-emulated-process-clocks /scratch/martin/src/wasm/llvm-project/wasm_memsafety_rtlib.c
WASM_FLAGS=--target=wasm64-unknown-wasi --sysroot /scratch/martin/src/wasm/wasi-libc/sysroot -g -D_WASI_EMULATED_PROCESS_CLOCKS -lwasi-emulated-process-clocks -Wl,--stack-first -Wl,--initial-memory=104857600 -Wl,--max-memory=104857600 -Wl,-z,stack-size=83886080
SAN_FLAGS=-march=wasm64-wasi+mem-safety -fsanitize=wasm-memsafety
#CFLAGS=-O0 ${WASM_FLAGS}
CFLAGS=-O2 ${WASM_FLAGS}
BUILD_DIR=build

all: ${BUILD_DIR}/bubble_sort.wasm ${BUILD_DIR}/merge_sort.wasm ${BUILD_DIR}/modified_merge_sort.wasm

${BUILD_DIR}/bubble_sort.wasm: bubble_sort.c
	${CC} -o $@ $< ${CFLAGS} ${EXTRA_FLAGS} ${SAN_FLAGS}

${BUILD_DIR}/merge_sort.wasm: merge_sort.c
	${CC} -o $@ $< ${CFLAGS} ${EXTRA_FLAGS} ${SAN_FLAGS}

${BUILD_DIR}/modified_merge_sort.wasm: modified_merge_sort.c
	${CC} -o $@ $< ${CFLAGS} ${EXTRA_FLAGS} ${SAN_FLAGS}

clean:
	@ rm -f ${BUILD_DIR}/bubble_sort.wasm ${BUILD_DIR}/merge_sort.wasm ${BUILD_DIR}/modified_merge_sort.wasm

${BUILD_DIR}:
	mkdir -p $@

# Add the directory as a dependency to ensure it's created before compilation
${BUILD_DIR}/%.wasm: | ${BUILD_DIR}
