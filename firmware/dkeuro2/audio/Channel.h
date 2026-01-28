#pragma once
#include <cstdint>

class Channel {
    public:
        void init();
        const int16_t* sampleData;
        uint32_t sampleLength;
        uint32_t samplePosition;

    private:

};