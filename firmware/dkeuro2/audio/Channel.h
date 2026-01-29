#pragma once
#include <cstdint>

class Channel {
    public:
        void init();
        const int16_t* sampleData;
        uint32_t sampleLength;
        uint32_t samplePosition;
        int64_t samplePositionFP; // Q32.32
        int64_t playbackSpeed; // Q32.32

    private:

};