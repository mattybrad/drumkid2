#pragma once
#include <cstdint>
#include "pico/time.h"

class Leds {
    public:
        void init();
        void setLed(uint8_t ledNum, bool state);
        void setDisplay(uint8_t digitNum, uint8_t segmentData);
        static const uint8_t CLOCK_OUT = 0;
        static const uint8_t CLOCK_IN = 1;
        static const uint8_t PULSE = 2;
        static const uint8_t ERROR = 3;
        static uint8_t asciiChars[256];
        void update();
        int64_t lastUpdate();
        
    private:
        uint8_t _singleLedStates;
        uint8_t _segmentData[4];
        uint8_t _currentDigit = 0;
        int64_t _lastUpdateTime = 0;
};