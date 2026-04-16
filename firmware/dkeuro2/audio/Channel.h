#pragma once
#include <cstdint>
#include "audio/Sample.h"

class Channel {
    public:
        void init();
        Sample* sample;
        const int16_t* sampleData;
        uint32_t sampleLength;
        uint32_t samplePosition;
        int64_t samplePositionFP; // Q32.32
        int64_t playbackSpeedFP; // Q32.32

    private:

};