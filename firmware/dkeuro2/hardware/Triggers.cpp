#include "Triggers.h"

void Triggers::init() {
    for(int i=0; i<4; i++) {
        gpio_init(_triggerPins[i]);
        gpio_set_dir(_triggerPins[i], GPIO_OUT);
    }
}

void Triggers::sendPulse(uint8_t triggerNum, absolute_time_t startTimeUs) {
    if(triggerNum >= 4) return;

    void *userData = (void*)(uintptr_t)triggerNum;
    add_alarm_at(startTimeUs, _alarmCallbackHigh, userData, true);
    add_alarm_at(startTimeUs + (_pulseLengthMs * 1000), _alarmCallbackLow, userData, true);
}

int64_t Triggers::_alarmCallbackHigh(alarm_id_t id, void* userData) {
    uint8_t triggerNum = (uint8_t)(uintptr_t)userData;
    if(triggerNum >= 4) return 0;

    gpio_put(_triggerPins[triggerNum], 1);

    return 0; // no need to repeat
}

int64_t Triggers::_alarmCallbackLow(alarm_id_t id, void* userData) {
    uint8_t triggerNum = (uint8_t)(uintptr_t)userData;
    if(triggerNum >= 4) return 0;

    gpio_put(_triggerPins[triggerNum], 0);

    return 0; // no need to repeat
}