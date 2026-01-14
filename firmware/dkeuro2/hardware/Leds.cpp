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
    add_repeating_timer_ms(2, update, NULL, &updateTimer);
}

void Leds::setLed(uint8_t ledNum, bool state) {
    // Set individual LED state
    if(state) {
        singleLedStates |= (1 << (3-ledNum));
    } else {
        singleLedStates &= ~(1 << (3-ledNum));
    }
}

void Leds::setDisplay(uint8_t digitNum, uint8_t segmentData) {
    // Set 7-segment display data
    // uint16_t shiftRegData = 0;
    // shiftRegData &= ~(0b11111111); // clear segment bits
    // shiftRegData |= segmentData; // write segment bits
    // shiftRegData |= 0b1111 << 8; // clear digit bits (set high)
    // shiftRegData &= ~(1 << (8+digitNum)); // set selected digit low
}

void Leds::update(repeating_timer_t *rt) {
    // Update LED states on hardware
    // first 8 bits toggle segments, next 4 bits toggle digits, last 4 bits toggle LEDs
    uint16_t shiftRegData = 0;
    shiftRegData |= (singleLedStates << 12);
    sn74595::shiftreg_send(shiftRegData);
}