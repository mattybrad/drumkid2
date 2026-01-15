#pragma once
#include <cstdint>
#include "pico/time.h"

class Buttons {
    public:
        void init();
        
        private:
        static bool _updateStatic(repeating_timer_t *rt);
        bool _update(repeating_timer_t *rt);
        repeating_timer_t _updateTimer;
};