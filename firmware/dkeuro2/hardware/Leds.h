#pragma once
#include <cstdint>

class Leds {
    public:
        void init();
        void setLed(uint8_t ledNum, bool state);
        void setDisplay(uint8_t digitNum, uint8_t segmentData);
        void update();
        static const uint8_t CLOCK_OUT = 0;
        static const uint8_t CLOCK_IN = 1;
        static const uint8_t PULSE = 2;
        static const uint8_t ERROR = 3;

    private:
        uint16_t ledStates;
};

