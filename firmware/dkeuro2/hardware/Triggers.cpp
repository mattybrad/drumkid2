#include "Triggers.h"

void Triggers::init() {
    for(int i=0; i<NUM_TRIGGER_OUTPUTS; i++) {
        gpio_init(_triggerPins[i]);
        gpio_set_dir(_triggerPins[i], GPIO_OUT);
    }
    gpio_init(Pins::SYNC_OUT);
    gpio_set_dir(Pins::SYNC_OUT, GPIO_OUT);
}

void Triggers::sendClockPulse(absolute_time_t startTimeUs) {
    void *userData = (void*)(uintptr_t)Pins::SYNC_OUT;
    add_alarm_at(startTimeUs, _alarmCallbackHigh, userData, true);
    add_alarm_at(startTimeUs + (_pulseLengthMs * 1000), _alarmCallbackLow, userData, true);
}

void Triggers::sendPulse(uint8_t triggerNum, absolute_time_t startTimeUs) {
    if(triggerNum >= NUM_TRIGGER_OUTPUTS) return;

    void *userData = (void*)(uintptr_t)_triggerPins[triggerNum];
    add_alarm_at(startTimeUs, _alarmCallbackHigh, userData, true);
    add_alarm_at(startTimeUs + (_pulseLengthMs * 1000), _alarmCallbackLow, userData, true);
}

int64_t Triggers::_alarmCallbackHigh(alarm_id_t id, void* userData) {
    uint8_t gpioPin = (uint8_t)(uintptr_t)userData;

    gpio_put(gpioPin, 1);

    return 0; // no need to repeat
}

int64_t Triggers::_alarmCallbackLow(alarm_id_t id, void* userData) {
    uint8_t gpioPin = (uint8_t)(uintptr_t)userData;

    gpio_put(gpioPin, 0);

    return 0; // no need to repeat
}