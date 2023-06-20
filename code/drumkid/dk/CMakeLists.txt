# only build I2S example if library is available
if (TARGET pico_audio_i2s)
    add_executable(dk_i2s
            dk.cpp
            )

    target_link_libraries(dk_i2s PRIVATE
            pico_stdlib
            pico_audio_i2s
            hardware_adc
            )

    # enable usb output, disable uart output
    pico_enable_stdio_usb(dk_i2s 1)
    pico_enable_stdio_uart(dk_i2s 0)

    target_compile_definitions(dk_i2s PRIVATE
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
    pico_add_extra_outputs(dk_i2s)
endif ()