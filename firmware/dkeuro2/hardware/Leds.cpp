#include "Leds.h"
#include "sn74595.pio.h"
#include "hardware/Pins.h"
#include "hardware/gpio.h"

void Leds::init() {
    // Initialize LED hardware
    gpio_init(Pins::SR_OUT_DATA);
    gpio_set_dir(Pins::SR_OUT_DATA, GPIO_OUT);
    gpio_init(Pins::SR_OUT_CLOCK);
    gpio_set_dir(Pins::SR_OUT_CLOCK, GPIO_OUT);
    gpio_init(Pins::SR_OUT_LATCH);
    gpio_set_dir(Pins::SR_OUT_LATCH, GPIO_OUT);
    sn74595::shiftreg_init();
    _singleLedStates = 0;
    _segmentData[0] = 0b11000000;
    _segmentData[1] = 0b00110000;
    _segmentData[2] = 0b00001100;
    _segmentData[3] = 0b00000011;
    add_repeating_timer_ms(2, Leds::_updateStatic, this, &_updateTimer);
}

bool Leds::_updateStatic(repeating_timer_t *rt) {
    auto *self = static_cast<Leds *>(rt->user_data);
    return self ? self->_update(rt) : false;
}

void Leds::setLed(uint8_t ledNum, bool state) {
    // Set individual LED state
    if(state) {
        _singleLedStates |= (1 << (3-ledNum));
    } else {
        _singleLedStates &= ~(1 << (3-ledNum));
    }
}

void Leds::setDisplay(uint8_t digitNum, uint8_t digitSegmentData) {
    // Set 7-segment display data
    if(digitNum < 4) {
        _segmentData[digitNum] = digitSegmentData;
    }
}

bool Leds::_update(repeating_timer_t *rt) {
    // Update LED states on hardware
    // first 8 bits toggle segments, next 4 bits toggle digits, last 4 bits toggle LEDs
    uint16_t shiftRegData = 0b0000111100000000;
    shiftRegData |= (_singleLedStates << 12);
    shiftRegData |= _segmentData[_currentDigit];
    shiftRegData &= ~(1 << (8+_currentDigit)); // set selected digit low
    sn74595::shiftreg_send(shiftRegData);
    _currentDigit = (_currentDigit + 1) % 4;
    return true;
}

uint8_t Leds::asciiChars[] = {
    0b00000000, // 0 NULL
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000, // 32 SPACE
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b01000000, // -
    0b10000000, // .
    0b00000000,
    0b00111111, // digit 0
    0b00000110,
    0b01011011,
    0b01001111,
    0b01100110,
    0b01101101,
    0b01111101,
    0b00000111,
    0b01111111,
    0b01101111, // digit 9
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b01010011,
    0b00000000, // 64
    0b01110111, // A (a)
    0b01111100,
    0b00111001,
    0b01011110,
    0b01111001,
    0b01110001,
    0b01101111,
    0b01110110,
    0b00000110,
    0b00001111,
    0b01110110,
    0b00111000,
    0b00110111,
    0b01010100,
    0b01011100,
    0b01110011,
    0b01100111,
    0b01010000,
    0b01101101,
    0b01111000,
    0b00111110,
    0b00011100,
    0b00111110,
    0b01110110,
    0b01101110,
    0b01011011, // Z (z)
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00001000, // _
    0b00000000, // 96
    0b01110111, // a
    0b01111100,
    0b00111001,
    0b01011110,
    0b01111001,
    0b01110001,
    0b01101111,
    0b01110110,
    0b00000110,
    0b00001111,
    0b01110110,
    0b00111000,
    0b00110111,
    0b01010100,
    0b01011100,
    0b01110011,
    0b01100111,
    0b01010000,
    0b01101101,
    0b01111000,
    0b00111110,
    0b00011100,
    0b00111110,
    0b01110110,
    0b01101110,
    0b01011011, // z
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
};

