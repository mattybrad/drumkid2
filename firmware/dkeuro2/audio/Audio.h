#pragma once

#include "pico/audio_i2s.h"
#include "Sample.h"
#include <cstdint>

#define SAMPLES_PER_BUFFER 16

class Audio {
    public:
        void init();
        bool bufferNeedsData();
        void giveSample(int16_t sampleLeft, int16_t sampleRight);
        struct audio_buffer_pool* getAudioBufferPool();

    private:
        struct audio_buffer *buffer;
        int16_t *bufferSamples;
        struct audio_buffer_pool* init_audio();
        struct audio_buffer_pool* audioBufferPool;
        uint bufferIndex = 0;
};