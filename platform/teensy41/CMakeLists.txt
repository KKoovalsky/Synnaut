# Platform: Teensy 4.1 / NXP IMXRT1062 (ARM Cortex-M7 @ 600 MHz)
#
# Startup and linker script derived from PJRC Teensyduino core (MIT License).
# https://github.com/PaulStoffregen/cores
#
# Three-layer design:
#   nautsyn_teensy41_flags    — compile/definition flags only (INTERFACE)
#   nautsyn_teensy41_startup  — startup.c + clockspeed.c (OBJECT)
#   platform::teensy41        — public target: flags + startup + linker (INTERFACE)
#   platform::teensy41::uart  — opt-in LPUART6 polling driver (Serial1, pins 0/1)
#
# Application targets link platform::teensy41 and get everything automatically.
# Add platform::teensy41::uart for UART support.

cmake_minimum_required(VERSION 3.24)

set(TEENSY41_DIR            "${CMAKE_CURRENT_LIST_DIR}")
set(TEENSY41_LINKER_SCRIPT  "${TEENSY41_DIR}/imxrt1062_t41.ld")
set(TEENSY41_TARGET_TRIPLE  "thumbv7em-none-eabihf")

# CPU flags shared between compile and link steps.
set(TEENSY41_CPU_FLAGS
    -mcpu=cortex-m7
    -mfpu=fpv5-d16
    -mfloat-abi=hard
    -mthumb
)

# ── 1. Compile flags ─────────────────────────────────────────────────────────
# Kept separate so the startup OBJECT library and any future platform
# sub-libraries can consume them without pulling in linker flags.
add_library(nautsyn_teensy41_flags INTERFACE)

target_compile_options(nautsyn_teensy41_flags INTERFACE
    $<$<CXX_COMPILER_ID:Clang,AppleClang>:--target=${TEENSY41_TARGET_TRIPLE}>
    ${TEENSY41_CPU_FLAGS}
    -ffreestanding
    -fno-exceptions
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
)
target_compile_definitions(nautsyn_teensy41_flags INTERFACE
    __IMXRT1062__
    F_CPU=600000000UL
)
target_include_directories(nautsyn_teensy41_flags INTERFACE
    ${TEENSY41_DIR}       # exposes imxrt.h
)

# ── 2. Startup object library ────────────────────────────────────────────────
# OBJECT type forces the linker to include these translation units in every
# executable that links platform::teensy41, even with --gc-sections.
add_library(nautsyn_teensy41_startup OBJECT
    startup.c
    clockspeed.c
)
target_link_libraries(nautsyn_teensy41_startup PRIVATE
    nautsyn_teensy41_flags
)

# ── 3. Platform interface target ─────────────────────────────────────────────
add_library(nautsyn_platform_teensy41 INTERFACE)
add_library(platform::teensy41 ALIAS nautsyn_platform_teensy41)

target_link_libraries(nautsyn_platform_teensy41 INTERFACE
    nautsyn_teensy41_flags
    $<TARGET_OBJECTS:nautsyn_teensy41_startup>
)
target_link_options(nautsyn_platform_teensy41 INTERFACE
    $<$<CXX_COMPILER_ID:Clang,AppleClang>:--target=${TEENSY41_TARGET_TRIPLE}>
    ${TEENSY41_CPU_FLAGS}
    -nostdlib
    -T "${TEENSY41_LINKER_SCRIPT}"
)
set_property(TARGET nautsyn_platform_teensy41 PROPERTY
    INTERFACE_LINK_DEPENDS "${TEENSY41_LINKER_SCRIPT}"
)

# ── 4. UART driver (opt-in) ──────────────────────────────────────────────────
# Polling LPUART driver for all 8 Serial instances (uart_serial1..8).
# Link platform::teensy41::uart to pull in uart_serial.c and expose uart_serial.h.
add_library(nautsyn_teensy41_uart STATIC uart_serial.c)
add_library(platform::teensy41::uart ALIAS nautsyn_teensy41_uart)
target_link_libraries(nautsyn_teensy41_uart PUBLIC nautsyn_teensy41_flags)

# ── teensy41_generate_hex(<target>) ──────────────────────────────────────────
# Post-build step: produce <target>.hex alongside the ELF for teensy_loader_cli.
function(teensy41_generate_hex TARGET)
    set(_hex "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.hex")
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND "${CMAKE_OBJCOPY}" -O ihex -R .eeprom
            "$<TARGET_FILE:${TARGET}>" "${_hex}"
        BYPRODUCTS "${_hex}"
        COMMENT "Generating ${TARGET}.hex"
        VERBATIM
    )
endfunction()

# ── teensy41_add_flash_target(<target>) ──────────────────────────────────────
# Creates a '<target>_flash' custom target that invokes teensy_loader_cli.
# Requires teensy41_generate_hex(<target>) to have been called first.
# Usage: cmake --build . --target <target>_flash
function(teensy41_add_flash_target TARGET)
    set(_hex "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.hex")
    add_custom_target(${TARGET}_flash
        COMMAND teensy_loader_cli --mcu=TEENSY41 -w "${_hex}"
        DEPENDS "${_hex}"
        COMMENT "Flashing ${TARGET}.hex to Teensy 4.1"
        VERBATIM
    )
endfunction()
