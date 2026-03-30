add_executable(hello main.cpp)

target_link_libraries(hello PRIVATE platform::teensy41)

teensy41_generate_hex(hello)
teensy41_add_flash_target(hello)
