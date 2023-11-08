add_executable(sd
        sd.cpp
        hw_config.c
)

add_subdirectory(lib/no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI build)

target_link_libraries(sd PRIVATE
        pico_stdlib
        pico_audio_i2s
        FatFs_SPI
)

# create map/bin/hex file etc.
pico_add_extra_outputs(sd)

# Enable usb output, disable uart output
pico_enable_stdio_usb(sd 1)
pico_enable_stdio_uart(sd 0)