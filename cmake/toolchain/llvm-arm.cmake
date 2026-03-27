# Propagate LLVM_TOOLCHAIN_DIR into try_compile sub-projects that re-load
# this toolchain file (e.g. during compiler ABI detection).
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES LLVM_TOOLCHAIN_DIR)

if(NOT DEFINED LLVM_TOOLCHAIN_DIR)
    message(FATAL_ERROR "LLVM_TOOLCHAIN_DIR cache variable must be set.\n"
        "Pass it via: cmake -DLLVM_TOOLCHAIN_DIR=/path/to/llvm ...")
endif()

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Prevent CMake from trying to link a test executable during compiler checks —
# bare-metal targets have no default runtime/crt to link against.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_COMPILER   "${LLVM_TOOLCHAIN_DIR}/bin/clang"   CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER "${LLVM_TOOLCHAIN_DIR}/bin/clang++" CACHE FILEPATH "")
set(CMAKE_ASM_COMPILER "${LLVM_TOOLCHAIN_DIR}/bin/clang"   CACHE FILEPATH "")

set(CMAKE_AR      "${LLVM_TOOLCHAIN_DIR}/bin/llvm-ar"      CACHE FILEPATH "")
set(CMAKE_RANLIB  "${LLVM_TOOLCHAIN_DIR}/bin/llvm-ranlib"  CACHE FILEPATH "")
set(CMAKE_LINKER  "${LLVM_TOOLCHAIN_DIR}/bin/ld.lld"       CACHE FILEPATH "")

set(CMAKE_OBJCOPY "${LLVM_TOOLCHAIN_DIR}/bin/llvm-objcopy" CACHE FILEPATH "")
set(CMAKE_OBJDUMP "${LLVM_TOOLCHAIN_DIR}/bin/llvm-objdump" CACHE FILEPATH "")
set(CMAKE_SIZE    "${LLVM_TOOLCHAIN_DIR}/bin/llvm-size"    CACHE FILEPATH "")

# Use lld; set here so it applies before project() calls add their own flags.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld")
