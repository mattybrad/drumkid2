#include "Buttons.h"
#include "sn74165.pio.h"
#include "hardware/Pins.h"
#include "hardware/gpio.h"
#include <stdio.h>

void Buttons::init() {
    // Initialize button hardware
    gpio_init(Pins::SR_IN_DATA);
    gpio_set_dir(Pins::SR_IN_DATA, GPIO_IN);
    gpio_init(Pins::SR_IN_CLOCK);
    gpio_set_dir(Pins::SR_IN_CLOCK, GPIO_OUT);
    gpio_init(Pins::SR_IN_LOAD);
    gpio_set_dir(Pins::SR_IN_LOAD, GPIO_OUT);
    sn74165::shiftreg_init();
}

bool Buttons::update() {
    uint32_t data;
    bool tempInputChanged = false;
    data = sn74165::shiftreg_get(&tempInputChanged);
    if (tempInputChanged)
    {
        newButtonPresses = data & ~_buttonStates;
        _buttonStates = data;
    } else {
        newButtonPresses = 0;
    }
    _lastUpdateTime = time_us_64();
    return newButtonPresses != 0;
}

int64_t Buttons::lastUpdate() {
    return _lastUpdateTime;
}