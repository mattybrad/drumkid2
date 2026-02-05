# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)
set(PICO_EXTRAS_FETCH_FROM_GIT ON)
include(pico_extras_import.cmake)

project(dkeuro2 C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_subdirectory(libraries/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/src build)

add_executable(dkeuro2
    dkeuro2.cpp
    hardware/CardReader.cpp
    hardware/CardReaderConfig.c
    hardware/Leds.cpp
    hardware/Buttons.cpp
    hardware/Memory.cpp
    audio/Audio.cpp
    audio/Channel.cpp
    rhythm/Transport.cpp
    rhythm/Beat.cpp
    )
pico_generate_pio_header(dkeuro2 ${CMAKE_CURRENT_LIST_DIR}/hardware/sn74595.pio)
pico_generate_pio_header(dkeuro2 ${CMAKE_CURRENT_LIST_DIR}/hardware/sn74165.pio)

pico_set_program_name(dkeuro2 "dkeuro2")
pico_set_program_version(dkeuro2 "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(dkeuro2 0)
pico_enable_stdio_usb(dkeuro2 0)

# Add the standard library to the build
target_link_libraries(dkeuro2
        pico_stdlib
        hardware_adc
        hardware_flash
        hardware_sync
        hardware_pio
        pico_multicore
        pico_audio_i2s
        no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
        )

# disable usb output, enable uart output
pico_enable_stdio_usb(dkeuro2 0)
pico_enable_stdio_uart(dkeuro2 1)

# Add the standard include files to the build
target_include_directories(dkeuro2 PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(dkeuro2)

