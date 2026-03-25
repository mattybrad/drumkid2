#pragma once
#include "Kit.h"
#include <cstdint>

class KitManager {
    public:
        void init();
        uint8_t kitNum = 0;
        Kit kits[MAX_KITS];
        uint getFreeSectors();

    private:

};