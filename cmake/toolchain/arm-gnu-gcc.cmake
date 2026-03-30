# Propagate ARM_GNU_GCC_TOOLCHAIN_DIR into try_compile sub-projects that re-load
# this toolchain file (e.g. during compiler ABI detection).
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES ARM_GNU_GCC_TOOLCHAIN_DIR)

if(NOT DEFINED ARM_GNU_GCC_TOOLCHAIN_DIR)
    message(FATAL_ERROR "ARM_GNU_GCC_TOOLCHAIN_DIR cache variable must be set.")
endif()

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Prevent CMake from trying to link a test executable during compiler checks —
# bare-metal targets have no default runtime/crt to link against.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_COMPILER   "${ARM_GNU_GCC_TOOLCHAIN_DIR}/bin/arm-none-eabi-gcc"   CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER "${ARM_GNU_GCC_TOOLCHAIN_DIR}/bin/arm-none-eabi-g++" CACHE FILEPATH "")
set(CMAKE_ASM_COMPILER "${ARM_GNU_GCC_TOOLCHAIN_DIR}/bin/arm-none-eabi-gcc"   CACHE FILEPATH "")

set(CMAKE_AR      "${ARM_GNU_GCC_TOOLCHAIN_DIR}/bin/arm-none-eabi-ar"      CACHE FILEPATH "")
set(CMAKE_RANLIB  "${ARM_GNU_GCC_TOOLCHAIN_DIR}/bin/arm-none-eabi-ranlib"  CACHE FILEPATH "")
set(CMAKE_LINKER  "${ARM_GNU_GCC_TOOLCHAIN_DIR}/bin/arm-none-eabi-ld"       CACHE FILEPATH "")

set(CMAKE_OBJCOPY "${ARM_GNU_GCC_TOOLCHAIN_DIR}/bin/arm-none-eabi-objcopy" CACHE FILEPATH "")
set(CMAKE_OBJDUMP "${ARM_GNU_GCC_TOOLCHAIN_DIR}/bin/arm-none-eabi-objdump" CACHE FILEPATH "")
set(CMAKE_SIZE    "${ARM_GNU_GCC_TOOLCHAIN_DIR}/bin/arm-none-eabi-size"    CACHE FILEPATH "")
