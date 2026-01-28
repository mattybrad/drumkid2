#pragma once

#include "pico/audio_i2s.h"
#include "Sample.h"
#include <cstdint>

#define SAMPLES_PER_BUFFER 16
#define NUM_AUDIO_BUFFERS 2

class Audio {
    public:
        void init();
        bool preBufferReady = false;
        uint preBufferSize = SAMPLES_PER_BUFFER * NUM_AUDIO_BUFFERS * 2; // stereo
        bool bufferNeedsData();
        void giveSample(int16_t sampleLeft, int16_t sampleRight);
        void update();
        uint bufferRequestSize();
        struct audio_buffer_pool* getAudioBufferPool();
        int16_t preBuffer[SAMPLES_PER_BUFFER * NUM_AUDIO_BUFFERS * 2]; // stereo

    private:
        uint _bufferRequestSize;
        struct audio_buffer *buffer;
        //int16_t *bufferSamples;
        struct audio_buffer_pool* init_audio();
        struct audio_buffer_pool* audioBufferPool;
        uint bufferIndex = 0;
};