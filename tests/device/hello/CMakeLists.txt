add_executable(hello main.cpp)

target_link_libraries(hello PRIVATE platform::teensy41 platform::teensy41::uart)

teensy41_generate_hex(hello)
teensy41_add_flash_target(hello)
