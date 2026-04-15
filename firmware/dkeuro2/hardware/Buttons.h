#pragma once
#include <cstdint>
#include "pico/time.h"

class Buttons {
    public:
        void init();
        bool update();
        int64_t lastUpdate();
        int16_t getButtonPress();
        
    private:
        uint32_t _newButtonPresses = 0;
        int64_t _lastUpdateTime = 0;
        int32_t _buttonStates = 0;
        uint8_t _nextButtonIndex = 0;
        uint8_t _buttonMap[24] = {
            17, 18, 19, 20, 255, 255, 255, 255,
            9, 10, 11, 12, 13, 14, 15, 16,
            1, 2, 3, 4, 5, 6, 7, 8,
        };
};