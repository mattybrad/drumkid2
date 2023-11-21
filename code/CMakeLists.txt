cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)
include(pico_extras_import_optional.cmake)

add_executable(drumkid
        drumkid.cpp
        hw_config.c
        )

project(drumkid)

pico_sdk_init()

add_subdirectory(lib/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/src build)

target_link_libraries(drumkid PRIVATE
        pico_stdlib
        pico_audio_i2s
        hardware_adc
        hardware_flash
        FatFs_SPI
        )

# enable usb output, disable uart output
pico_enable_stdio_usb(drumkid 1)
pico_enable_stdio_uart(drumkid 0)

target_compile_definitions(drumkid PRIVATE
        # compile time configuration of I2S
        PICO_AUDIO_I2S_MONO_INPUT=1
        USE_AUDIO_I2S=1
        )
# create map/bin/hex file etc.
pico_add_extra_outputs(drumkid)