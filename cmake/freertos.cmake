# FreeRTOS-Kernel — fetched at configure time, cached in FETCHCONTENT_BASE_DIR.
#
# Prerequisites (must exist before this file is included):
#   freertos_config  — INTERFACE target that exposes FreeRTOSConfig.h and the
#                      platform compile flags. The platform that owns
#                      FreeRTOSConfig.h is responsible for creating it.
#
# Targets made available after inclusion:
#   freertos_kernel  — links freertos_kernel_port transitively; this is the
#                      only target application code needs to link against.

include(FetchContent)

FetchContent_Declare(
    freertos_kernel
    GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS-Kernel.git
    GIT_TAG        V11.1.0
    GIT_SHALLOW    TRUE
)

# Port and heap selection — must be set before FetchContent_MakeAvailable so
# the kernel's CMakeLists.txt picks them up as cache variables.
set(FREERTOS_PORT GCC_ARM_CM7 CACHE STRING "FreeRTOS port")
set(FREERTOS_HEAP 4           CACHE STRING "FreeRTOS heap implementation (1–5)")

FetchContent_MakeAvailable(freertos_kernel)
