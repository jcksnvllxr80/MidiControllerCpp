# Pico SDK build for the firmware (RP2350 / Pico 2 W). The DESKTOP build uses the
# Makefile; this CMake is only for the microcontroller target. Build it in a
# SEPARATE directory so it doesn't clash with the Makefile's build/:
#
#   export PICO_SDK_PATH=/path/to/pico-sdk
#   cmake -B build-pico -DPICO_BOARD=pico2_w
#   cmake --build build-pico         # -> build-pico/midicontroller_pico.uf2
#
# Needs Python 3 on PATH (CMake runs tools/embed_data.py to bake data/*.json).

cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2_w CACHE STRING "Target board")
# nlohmann/json and the config loader use exceptions, so enable them in the SDK.
set(PICO_CXX_ENABLE_EXCEPTIONS 1)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(midicontroller_pico C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

pico_sdk_init()

# Bake data/*.json into flash (regenerated each build).
set(EMBEDDED_SRC ${CMAKE_CURRENT_BINARY_DIR}/EmbeddedData.cpp)
add_custom_command(
    OUTPUT ${EMBEDDED_SRC}
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_data.py
            ${CMAKE_CURRENT_SOURCE_DIR}/data ${EMBEDDED_SRC}
    COMMENT "Embedding data/*.json -> EmbeddedData.cpp"
    VERBATIM)

# Domain core + config + app (the sim adapters and src/main.cpp are NOT used here).
file(GLOB CORE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/domain/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app/*.cpp)
file(GLOB MCU_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/adapters/mcu/*.cpp)
# UsbConfigTransport is opt-in (needs TinyUSB descriptors); add it below if enabled.
list(REMOVE_ITEM MCU_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/adapters/mcu/UsbConfigTransport.cpp)

add_executable(midicontroller_pico ${CORE_SRC} ${MCU_SRC} ${EMBEDDED_SRC})

target_include_directories(midicontroller_pico PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

target_link_libraries(midicontroller_pico
    pico_stdlib
    hardware_uart
    hardware_pwm
    hardware_i2c
    hardware_timer
    hardware_flash    # FlashKv persistence
    hardware_sync)

# The USB editor link (CDC). OFF by default because the descriptors are standard
# boilerplate that hasn't been validated on hardware here. When OFF, stdio uses
# USB (handy for debug); when ON, stdio moves to UART so the CDC is free.
option(MC_ENABLE_USB_EDITOR "Build the USB CDC editor transport (TinyUSB)" OFF)
if(MC_ENABLE_USB_EDITOR)
    target_sources(midicontroller_pico PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/adapters/mcu/UsbConfigTransport.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/adapters/mcu/usb/usb_descriptors.c)
    target_include_directories(midicontroller_pico PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/adapters/mcu/usb)  # tusb_config.h
    target_compile_definitions(midicontroller_pico PRIVATE MC_ENABLE_USB_EDITOR=1)
    target_link_libraries(midicontroller_pico tinyusb_device tinyusb_board)
    pico_enable_stdio_usb(midicontroller_pico 0)
    pico_enable_stdio_uart(midicontroller_pico 1)
else()
    pico_enable_stdio_usb(midicontroller_pico 1)
    pico_enable_stdio_uart(midicontroller_pico 0)
endif()

pico_add_extra_outputs(midicontroller_pico)  # .uf2 / .bin / .hex
