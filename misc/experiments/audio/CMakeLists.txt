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
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")
set(PICO_EXTRAS_FETCH_FROM_GIT 1)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

project(audio C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(audio audio.cpp )

pico_set_program_name(audio "audio")
pico_set_program_version(audio "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(audio 1)
pico_enable_stdio_usb(audio 0)

# Add the standard library to the build
target_link_libraries(audio
        pico_stdlib
        pico_audio_i2s)

# Add the standard include files to the build
target_include_directories(audio PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_definitions(audio PRIVATE
        # compile time configuration of I2S
        PICO_AUDIO_I2S_MONO_INPUT=0
        PICO_AUDIO_I2S_MONO_OUTPUT=0
        USE_AUDIO_I2S=1
        )

pico_add_extra_outputs(audio)

