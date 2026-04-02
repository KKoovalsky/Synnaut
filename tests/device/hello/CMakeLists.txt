add_executable(hello hello.cpp)

target_link_libraries(hello PRIVATE
    platform::teensy41
    platform::teensy41::uart
    platform::teensy41::freertos
)

teensy41_generate_hex(hello)
teensy41_add_flash_target(hello)
