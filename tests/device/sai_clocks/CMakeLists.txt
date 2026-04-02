add_executable(test_sai_clocks test_sai_clocks.cpp)

target_link_libraries(test_sai_clocks PRIVATE
    platform::teensy41
    platform::teensy41::uart
    platform::teensy41::sai
    platform::teensy41::freertos
)

teensy41_generate_hex(test_sai_clocks)
teensy41_add_flash_target(test_sai_clocks)
