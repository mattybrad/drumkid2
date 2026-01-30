#pragma once
#include <cstdint>
#include "pico/time.h"

class Buttons {
    public:
        void init();
        void update();
        int64_t lastUpdate();
        
    private:
        int64_t _lastUpdateTime = 0;
};