add_executable(sai_clocks main.cpp)

target_link_libraries(sai_clocks PRIVATE
    platform::teensy41
    platform::teensy41::uart
    platform::teensy41::sai
    platform::teensy41::freertos
)

teensy41_generate_hex(sai_clocks)
teensy41_add_flash_target(sai_clocks)
