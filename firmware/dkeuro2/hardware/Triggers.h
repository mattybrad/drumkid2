#pragma once
#include <cstdint>
#include "pico/time.h"
#include "hardware/Pins.h"
#include "hardware/gpio.h"

class Triggers {
    public:
        void init();
        void sendPulse(uint8_t triggerNum, absolute_time_t startTimeUs);
        
    private:
        uint16_t _pulseLengthMs = 10; // pulse length in milliseconds
        static constexpr uint8_t _triggerPins[4] = {Pins::TRIGGER_1, Pins::TRIGGER_2, Pins::TRIGGER_3, Pins::TRIGGER_4};
        static int64_t _alarmCallbackHigh(alarm_id_t id, void* userData);
        static int64_t _alarmCallbackLow(alarm_id_t id, void* userData);
};