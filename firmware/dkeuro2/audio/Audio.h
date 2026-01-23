#pragma once

#include "pico/audio_i2s.h"
#include "Sample.h"
#include <cstdint>

#define SAMPLES_PER_BUFFER 8

class Audio {
    public:
        void init();
        void update();
        Sample samples[8];
        struct audio_buffer_pool* getAudioBufferPool();

    private:
        struct audio_buffer_pool* init_audio();
        struct audio_buffer_pool* audioBufferPool;
};