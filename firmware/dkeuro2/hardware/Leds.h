#pragma once
#include <cstdint>
#include "pico/time.h"

class Leds {
    public:
        void init();
        void setLed(uint8_t ledNum, bool state);
        void setDisplay(uint8_t digitNum, uint8_t segmentData);
        void update(repeating_timer_t *rt);
        static const uint8_t CLOCK_OUT = 0;
        static const uint8_t CLOCK_IN = 1;
        static const uint8_t PULSE = 2;
        static const uint8_t ERROR = 3;

    private:
        uint8_t singleLedStates;
        uint8_t segmentData[4];
        repeating_timer_t updateTimer;

};

