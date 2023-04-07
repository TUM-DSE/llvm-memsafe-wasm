add_compiler_rt_component(wasm_memsafety)

message(SEND_ERROR "here")

set(WASM_MEMSAFETY_SOURCES
wasm_memsafety.cpp
)

include_directories(..)

set(WASM_MEMSAFETY_CFLAGS
${SANITIZER_COMMON_CFLAGS}
)

# Too many existing bugs, needs cleanup.
append_list_if(COMPILER_RT_HAS_WNO_FORMAT -Wno-format WASM_MEMSAFETY_CFLAGS)

set(WASM_MEMSAFETY_DIAG_CFLAGS
-DWASM_MEMSAFETY_ENABLE_DIAG=1
)

foreach(arch ${WASM_MEMSAFETY_SUPPORTED_ARCH})
add_compiler_rt_runtime(clang_rt.wasm_memsafety
    STATIC
    ARCHS ${arch}
    SOURCES ${WASM_MEMSAFETY_SOURCES}
    OBJECT_LIBS RTInterception
                RTSanitizerCommon
                RTSanitizerCommonLibc
    CFLAGS ${WASM_MEMSAFETY_CFLAGS}
    PARENT_TARGET wasm_memsafety)
add_compiler_rt_runtime(clang_rt.wasm_memsafety_diag
    STATIC
    ARCHS ${arch}
    SOURCES ${WASM_MEMSAFETY_SOURCES}
    OBJECT_LIBS RTInterception
                RTSanitizerCommon
                RTSanitizerCommonLibc
                RTSanitizerCommonCoverage
                RTSanitizerCommonSymbolizer
                RTUbsan
    CFLAGS ${WASM_MEMSAFETY_CFLAGS} ${WASM_MEMSAFETY_DIAG_CFLAGS}
    PARENT_TARGET wasm_memsafety)
endforeach()
