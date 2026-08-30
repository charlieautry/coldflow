cmake_minimum_required(VERSION 3.13)

# Two builds live in this tree:
#
#   Off-target unit tests (desktop, no SDK needed):
#     cmake -B build-host -DCOLDFLOW_HOST_TESTS=ON
#     cmake --build build-host
#     ctest --test-dir build-host
#
#   Pico firmware (requires PICO_SDK_PATH in the environment and
#   pico_sdk_import.cmake copied into this folder from the SDK's external/ dir):
#     cmake -B build
#     cmake --build build
#
option(COLDFLOW_HOST_TESTS "Build the off-target unit tests instead of the Pico firmware" OFF)

if(COLDFLOW_HOST_TESTS)
    project(coldflow_host_tests CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    enable_testing()
    add_subdirectory(tests)
else()
    set(PICO_BOARD pico2)
    include(pico_sdk_import.cmake)
    project(coldflow C CXX ASM)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    pico_sdk_init()

    file(GLOB COLDFLOW_SOURCES CONFIGURE_DEPENDS src/*.cpp core/*.cpp)
    add_executable(coldflow ${COLDFLOW_SOURCES})
    target_include_directories(coldflow PRIVATE core src)
    target_link_libraries(coldflow pico_stdlib hardware_adc hardware_pwm hardware_spi hardware_uart)
    pico_enable_stdio_usb(coldflow 1)
    pico_enable_stdio_uart(coldflow 0)
    pico_add_extra_outputs(coldflow)

    # the second pico: resolver simulator (flash this one to the sim board)
    add_executable(resolver_sim resolver_sim/main.cpp)
    target_link_libraries(resolver_sim pico_stdlib hardware_pwm)
    pico_enable_stdio_usb(resolver_sim 1)
    pico_enable_stdio_uart(resolver_sim 0)
    pico_add_extra_outputs(resolver_sim)
endif()
