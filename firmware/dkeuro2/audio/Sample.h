#pragma once
#include <cstdint>

class Sample {
    public:
        void init();
        uint32_t address = 0;
        uint32_t lengthSamples = 0;
        uint32_t sampleRate = 44100;

    private:

};