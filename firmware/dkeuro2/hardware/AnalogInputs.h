#pragma once
#include <cstdint>
#include "pico/time.h"

class AnalogInputs {
    public:
        void init();
        void update();
        int64_t lastUpdate();
        
    private:
        int64_t _lastUpdateTime = 0;
        uint8_t _muxChannel = 0;
        uint16_t _inputValues[16] = {0};
};