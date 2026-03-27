# Platform: Teensy 4.1 (NXP IMXRT1062, ARM Cortex-M7)
#
# Provides an INTERFACE library target `platform::teensy41`.
# Link against it to apply all hardware-specific compile/link flags.

set(TEENSY41_TARGET_TRIPLE "thumbv7em-none-eabihf")

add_library(nautsyn_platform_teensy41 INTERFACE)
add_library(platform::teensy41 ALIAS nautsyn_platform_teensy41)

target_compile_options(nautsyn_platform_teensy41 INTERFACE
    --target=${TEENSY41_TARGET_TRIPLE}
    -mcpu=cortex-m7
    -mfpu=fpv5-d16
    -mfloat-abi=hard
    # -mthumb
    -ffreestanding
    -fno-exceptions
    -fno-rtti
)

target_compile_definitions(nautsyn_platform_teensy41 INTERFACE
    __IMXRT1062__
    TEENSY41
    F_CPU=600000000UL
)

target_link_options(nautsyn_platform_teensy41 INTERFACE
    --target=${TEENSY41_TARGET_TRIPLE}
    -mcpu=cortex-m7
    -mfpu=fpv5-d16
    -mfloat-abi=hard
    # -mthumb
    -nostdlib
)
