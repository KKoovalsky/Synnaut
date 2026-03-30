# Platform: Teensy 4.1 (NXP IMXRT1062, ARM Cortex-M7)
#
# Provides an INTERFACE library target `platform::teensy41`.
# Link against it to apply all hardware-specific compile/link flags.

set(TEENSY41_TARGET_TRIPLE "thumbv7em-none-eabihf")
set(TEENSY41_LINKER_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/teensy41.ld")
set(TEENSY41_STARTUP       "${CMAKE_CURRENT_LIST_DIR}/../../platform/teensy41/startup.c")

add_library(nautsyn_platform_teensy41 INTERFACE)
add_library(platform::teensy41 ALIAS nautsyn_platform_teensy41)

target_sources(nautsyn_platform_teensy41 INTERFACE "${TEENSY41_STARTUP}")

target_compile_options(nautsyn_platform_teensy41 INTERFACE
    --target=${TEENSY41_TARGET_TRIPLE}
    -mcpu=cortex-m7
    -mfpu=fpv5-d16
    -mfloat-abi=hard
    -mthumb
    -ffreestanding
    -fno-exceptions
    -fno-rtti
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
)

target_compile_definitions(nautsyn_platform_teensy41 INTERFACE
    __IMXRT1062__
    TEENSY41
    F_CPU=600000000UL
)

set_property(TARGET nautsyn_platform_teensy41 PROPERTY
    INTERFACE_LINK_DEPENDS "${TEENSY41_LINKER_SCRIPT}"
)

# teensy41_generate_hex(<target>)
# Adds a POST_BUILD step that produces <target>.hex next to the ELF.
# The .hex is what teensy_loader_cli expects.
function(teensy41_generate_hex TARGET)
    set(_hex "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.hex")
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND "${CMAKE_OBJCOPY}" -O ihex
            -R .eeprom
            "$<TARGET_FILE:${TARGET}>"
            "${_hex}"
        BYPRODUCTS "${_hex}"
        COMMENT "Generating ${TARGET}.hex"
        VERBATIM
    )
endfunction()

target_link_options(nautsyn_platform_teensy41 INTERFACE
    --target=${TEENSY41_TARGET_TRIPLE}
    -mcpu=cortex-m7
    -mfpu=fpv5-d16
    -mfloat-abi=hard
    -mthumb
    -nostdlib
    -T "${TEENSY41_LINKER_SCRIPT}"
)

