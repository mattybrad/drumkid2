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
    data &= ~(0xF << 4); // set bits 4-7 to 0 (unused buttons)
    data ^= (1 << 3); // invert bit 3 to detect power off
    if (tempInputChanged)
    {
        _newButtonPresses = data & ~_buttonStates;
        _buttonStates = data;
        //printf("INPUT CHANGED\n"); // power off is detected but not as a button press, maybe need to invert logic? buttons go high when pressed, power voltage divider goes low when power off
    } else {
        _newButtonPresses = 0;
    }
    _lastUpdateTime = time_us_64();
    _nextButtonIndex = 0;
    return _newButtonPresses != 0;
}

int64_t Buttons::lastUpdate() {
    return _lastUpdateTime;
}

int16_t Buttons::getButtonPress() {
    for(uint i=_nextButtonIndex; i<24; i++) {
        if(_newButtonPresses & (1 << i)) {
            _newButtonPresses &= ~(1 << i); // clear this button press so it won't be returned again
            _nextButtonIndex = i + 1;
            return _buttonMap[i];
        }
    }
    return -1;
}