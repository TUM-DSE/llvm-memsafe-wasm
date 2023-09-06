# CC=/scratch/fritz/src/safe-wasm/llvm-project/build/bin/clang
# WASM_FLAGS=--target=wasm64-unknown-wasi --sysroot /scratch/martin/src/wasm/wasi-libc/sysroot -g -D_WASI_EMULATED_PROCESS_CLOCKS -lwasi-emulated-process-clocks -Wl,--stack-first -Wl,--initial-memory=104857600 -Wl,--max-memory=104857600 -Wl,-z,stack-size=83886080

# SAN_FLAGS=-march=wasm64-wasi+mem-safety -fsanitize=wasm-memsafety
# #CFLAGS=-O0 ${WASM_FLAGS}
# CFLAGS=-O2 ${WASM_FLAGS}
# BUILD_DIR=build

# all: ${BUILD_DIR}/pac.wasm

# ${BUILD_DIR}/pac.wasm: pac.c
# 	${CC} -o $@ $< ${CFLAGS} ${EXTRA_FLAGS} ${SAN_FLAGS}

# clean:
# 	@ rm -f ${BUILD_DIR}/pac.wasm

# ${BUILD_DIR}:
# 	mkdir -p $@

# # Add the directory as a dependency to ensure it's created before compilation
# ${BUILD_DIR}/%.wasm: | ${BUILD_DIR}

CC=/scratch/fritz/src/safe-wasm/llvm-project/build/bin/clang
WASM_FLAGS=--target=wasm64-unknown-wasi --sysroot /scratch/martin/src/wasm/wasi-libc/sysroot -g -D_WASI_EMULATED_PROCESS_CLOCKS -lwasi-emulated-process-clocks -Wl,--stack-first -Wl,--initial-memory=104857600 -Wl,--max-memory=104857600 -Wl,-z,stack-size=83886080

SAN_FLAGS=-march=wasm64-wasi+mem-safety -fsanitize=wasm-memsafety
#CFLAGS=-O0 ${WASM_FLAGS}
CFLAGS=-O2 ${WASM_FLAGS}
BUILD_DIR=build

PAC_SOURCES=$(wildcard pac-*.c)
PAC_WASMS=$(PAC_SOURCES:%.c=${BUILD_DIR}/%.wasm)

all: ${PAC_WASMS}

${BUILD_DIR}/%.wasm: %.c
	${CC} -o $@ $< ${CFLAGS} ${EXTRA_FLAGS} ${SAN_FLAGS}

clean:
	@ rm -f ${BUILD_DIR}/*.wasm

${BUILD_DIR}:
	mkdir -p $@

# Add the directory as a dependency to ensure it's created before compilation
${BUILD_DIR}/%.wasm: | ${BUILD_DIR}
