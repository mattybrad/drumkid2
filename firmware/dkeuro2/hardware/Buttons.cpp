#include "Buttons.h"
#include "sn74165.pio.h"
#include "hardware/Pins.h"
#include "hardware/gpio.h"
#include <stdio.h>

void Buttons::init() {
    // Initialize LED hardware
    gpio_init(Pins::SR_IN_DATA);
    gpio_set_dir(Pins::SR_IN_DATA, GPIO_IN);
    gpio_init(Pins::SR_IN_CLOCK);
    gpio_set_dir(Pins::SR_IN_CLOCK, GPIO_OUT);
    gpio_init(Pins::SR_IN_LOAD);
    gpio_set_dir(Pins::SR_IN_LOAD, GPIO_OUT);
    sn74165::shiftreg_init();
    add_repeating_timer_ms(2, Buttons::_updateStatic, this, &_updateTimer);
}

bool Buttons::_updateStatic(repeating_timer_t *rt) {
    auto *self = static_cast<Buttons *>(rt->user_data);
    return self ? self->_update(rt) : false;
}

bool Buttons::_update(repeating_timer_t *rt) {
    uint32_t data;
    bool tempInputChanged = false;
    data = sn74165::shiftreg_get(&tempInputChanged);
    if (tempInputChanged)
    {
        printf("Button state changed: 0x%08X\n", data);
    }
    return true;
}