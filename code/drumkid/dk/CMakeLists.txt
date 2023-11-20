add_executable(dk
        dk.cpp
        hw_config.c
        )

add_subdirectory(lib/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/src build)

target_link_libraries(dk PRIVATE
        pico_stdlib
        pico_audio_i2s
        hardware_adc
        hardware_flash
        FatFs_SPI
        )

# enable usb output, disable uart output
pico_enable_stdio_usb(dk 1)
pico_enable_stdio_uart(dk 0)

target_compile_definitions(dk PRIVATE
        # compile time configuration of I2S
        PICO_AUDIO_I2S_MONO_INPUT=1
        #define for our example code
        USE_AUDIO_I2S=1
#            PICO_AUDIO_I2S_DATA_PIN=22
#            PICO_AUDIO_I2S_CLOCK_PIN_BASE=23
#            PICO_DEFAULT_UART=0
#            PICO_DEFAULT_UART_TX_PIN=28
#            PICO_DEFAULT_UART_RX_PIN=29
        )
# create map/bin/hex file etc.
pico_add_extra_outputs(dk)