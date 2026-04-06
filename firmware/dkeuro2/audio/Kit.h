#pragma once
#include "audio/Sample.h"
#include "Config.h"
#include <cstdint>

class Kit {
    public:
        void init();
        char name[32]; // could prob just be 4 characters
        uint16_t startSector = 0;
        uint16_t sizeSectors = 0;
        uint8_t numSamples = 0;
        Sample samples[MAX_CHANNELS];

    private:

};