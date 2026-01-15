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
    singleLedStates = 0;
    segmentData[0] = 0b11000000;
    segmentData[1] = 0b00110000;
    segmentData[2] = 0b00001100;
    segmentData[3] = 0b00000011;
    add_repeating_timer_ms(2, Leds::updateStatic, this, &updateTimer);
}

bool Leds::updateStatic(repeating_timer_t *rt) {
    auto *self = static_cast<Leds *>(rt->user_data);
    return self ? self->update(rt) : false;
}

void Leds::setLed(uint8_t ledNum, bool state) {
    // Set individual LED state
    if(state) {
        singleLedStates |= (1 << (3-ledNum));
    } else {
        singleLedStates &= ~(1 << (3-ledNum));
    }
}

void Leds::setDisplay(uint8_t digitNum, uint8_t digitSegmentData) {
    // Set 7-segment display data
    if(digitNum < 4) {
        segmentData[digitNum] = digitSegmentData;
    }
}

bool Leds::update(repeating_timer_t *rt) {
    // Update LED states on hardware
    // first 8 bits toggle segments, next 4 bits toggle digits, last 4 bits toggle LEDs
    uint16_t shiftRegData = 0b0000111100000000;
    shiftRegData |= (singleLedStates << 12);
    shiftRegData |= segmentData[currentDigit];
    shiftRegData &= ~(1 << (8+currentDigit)); // set selected digit low
    sn74595::shiftreg_send(shiftRegData);
    currentDigit = (currentDigit + 1) % 4;
    return true;
}