cmake_minimum_required(VERSION 3.24)

set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type")

project(NautSyn C CXX ASM)

set(CMAKE_C_STANDARD   11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

add_subdirectory(platform/teensy41)  # must precede freertos.cmake (defines freertos_config)
include(cmake/freertos.cmake)

add_subdirectory(tests/device/hello)
