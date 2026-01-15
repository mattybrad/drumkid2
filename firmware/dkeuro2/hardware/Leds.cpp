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