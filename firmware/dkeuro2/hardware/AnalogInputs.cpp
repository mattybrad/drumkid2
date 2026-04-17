#include "AnalogInputs.h"
#include "hardware/Pins.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include <stdio.h>

void AnalogInputs::init() {
    adc_init();
    adc_gpio_init(Pins::MUX_READ_0); // first 8 pots
    adc_gpio_init(Pins::MUX_READ_1); // 4 pots, 4 CV inputs
    gpio_init(Pins::MUX_ADDRESS_A);
    gpio_init(Pins::MUX_ADDRESS_B);
    gpio_init(Pins::MUX_ADDRESS_C);
    gpio_set_dir(Pins::MUX_ADDRESS_A, GPIO_OUT);
    gpio_set_dir(Pins::MUX_ADDRESS_B, GPIO_OUT);
    gpio_set_dir(Pins::MUX_ADDRESS_C, GPIO_OUT);

    // set SMPS mode for ADC to reduce noise from switching regulator
    gpio_init(Pins::ADC_SMPS_MODE);
    gpio_set_dir(Pins::ADC_SMPS_MODE, GPIO_OUT);
    gpio_put(Pins::ADC_SMPS_MODE, 1);
}

void AnalogInputs::update() {
    adc_select_input(0);
    _inputValues[_muxChannel] = 4095-adc_read();
    adc_select_input(1);
    _inputValues[_muxChannel + 8] = 4095-adc_read();
    _muxChannel = (_muxChannel + 1) % 8;
    gpio_put(Pins::MUX_ADDRESS_A, _muxChannel & 1);
    gpio_put(Pins::MUX_ADDRESS_B, (_muxChannel >> 1) & 1);
    gpio_put(Pins::MUX_ADDRESS_C, (_muxChannel >> 2) & 1);
    _lastUpdateTime = time_us_64();
}

int64_t AnalogInputs::lastUpdate() {
    return _lastUpdateTime;
}

uint16_t AnalogInputs::getInputValue(uint8_t inputNum) {
    if(inputNum < 16) {
        return _inputValues[inputNum];
    } else {
        return 0;
    }
}

uint8_t AnalogInputs::getLastUpdatedMuxChannel() {
    return (_muxChannel + 7) % 8;
}