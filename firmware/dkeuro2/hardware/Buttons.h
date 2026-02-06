#pragma once
#include <cstdint>
#include "pico/time.h"

class Buttons {
    public:
        void init();
        bool update();
        int64_t lastUpdate();
        int32_t newButtonPresses = 0; // should be private soon
        
    private:
        int64_t _lastUpdateTime = 0;
        int32_t _buttonStates = 0;
};