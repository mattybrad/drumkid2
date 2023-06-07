# only build I2S example if library is available
if (TARGET pico_audio_i2s)
    add_executable(sample_i2s
            sample.c
            )

    target_link_libraries(sample_i2s PRIVATE
            pico_stdlib
            pico_audio_i2s
            )

    target_compile_definitions(sample_i2s PRIVATE
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
    pico_add_extra_outputs(sample_i2s)
endif ()