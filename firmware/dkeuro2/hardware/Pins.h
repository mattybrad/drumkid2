#pragma once

namespace Pins {
    
    // SD card pins
    constexpr uint8_t SD_CLOCK = 2;
    constexpr uint8_t SD_SERIAL_IN = 3;
    constexpr uint8_t SD_SERIAL_OUT = 4;
    constexpr uint8_t SD_CHIP_SELECT = 5;

    // shift register pins (74HC595 for output, 74HC165 for input)
    constexpr uint8_t SR_OUT_DATA = 6;
    constexpr uint8_t SR_OUT_CLOCK = 7;
    constexpr uint8_t SR_OUT_LATCH = 8;
    constexpr uint8_t SR_IN_DATA = 14;
    constexpr uint8_t SR_IN_CLOCK = 13;
    constexpr uint8_t SR_IN_LOAD = 12;

    // multiplexer (4051) pins
    constexpr uint8_t MUX_ADDRESS_A = 19;
    constexpr uint8_t MUX_ADDRESS_B = 20;
    constexpr uint8_t MUX_ADDRESS_C = 21;
    constexpr uint8_t MUX_READ_0 = 26;
    constexpr uint8_t MUX_READ_1 = 27;

    // DAC (audio output) pins
    constexpr uint8_t DAC_DOUT = 9;
    constexpr uint8_t DAC_BCLK = 10;
    constexpr uint8_t DAC_LRC = 11;

    // clock/sync pins
    constexpr uint8_t SYNC_IN = 16;
    constexpr uint8_t SYNC_OUT = 17;

    // trigger output pins
    constexpr uint8_t TRIGGER_1 = 15;
    constexpr uint8_t TRIGGER_2 = 28;
    constexpr uint8_t TRIGGER_3 = 22;
    constexpr uint8_t TRIGGER_4 = 24;
}