# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

set(PICO_BOARD pico2 CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)
set(PICO_EXTRAS_FETCH_FROM_GIT ON)
include(pico_extras_import.cmake)

project(drumkid)

pico_sdk_init()

add_executable(drumkid
        drumkid.cpp
        hw_config.c
        )

pico_generate_pio_header(drumkid ${CMAKE_CURRENT_LIST_DIR}/sn74595.pio)
pico_generate_pio_header(drumkid ${CMAKE_CURRENT_LIST_DIR}/sn74165.pio)

add_subdirectory(lib/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/src build)

target_link_libraries(drumkid PRIVATE
        pico_stdlib
        #pico_multicore
        pico_audio_i2s
        hardware_adc
        hardware_flash
        hardware_pio
        no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
        )

# disable usb output, enable uart output
pico_enable_stdio_usb(drumkid 0)
pico_enable_stdio_uart(drumkid 1)

target_compile_definitions(drumkid PRIVATE
        # compile time configuration of I2S
        PICO_AUDIO_I2S_MONO_INPUT=0
        PICO_AUDIO_I2S_MONO_OUTPUT=0
        USE_AUDIO_I2S=1
        )
# create map/bin/hex file etc.
pico_add_extra_outputs(drumkid)