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
    ledStates = 0;
}

void Leds::setLed(uint8_t ledNum, bool state) {
    // Set individual LED state
    if(state) {
        ledStates |= (1 << (15-ledNum));
    } else {
        ledStates &= ~(1 << (15-ledNum));
    }
}

void Leds::setDisplay(uint8_t digitNum, uint8_t segmentData) {
    // Set 7-segment display data
    ledStates &= ~(0b11111111); // clear segment bits
    ledStates |= segmentData; // write segment bits
    ledStates |= 0b1111 << 8; // clear digit bits (set high)
    ledStates &= ~(1 << (8+digitNum)); // set selected digit low
}

void Leds::update() {
    // Update LED states on hardware
    // first 8 bits toggle segments, next 4 bits toggle digits, last 4 bits toggle LEDs
    sn74595::shiftreg_send(ledStates);
}