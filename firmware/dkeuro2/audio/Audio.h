#pragma once

#include "pico/audio_i2s.h"
#include "Sample.h"
#include <cstdint>

#define SAMPLES_PER_BUFFER 32
#define NUM_AUDIO_BUFFERS 2

class Audio {
    public:
        void init();
        bool samplesRequired();
        void queueSample(int16_t sampleLeft, int16_t sampleRight);
        void update();
        
        private:
        bool _preBufferReady = false;
        uint _preBufferWriteIndex = 0;
        int16_t _preBuffer[SAMPLES_PER_BUFFER * NUM_AUDIO_BUFFERS * 2]; // stereo
        uint _preBufferSize = SAMPLES_PER_BUFFER * NUM_AUDIO_BUFFERS * 2; // stereo
        //struct audio_buffer_pool* _getAudioBufferPool();
        struct audio_buffer *_buffer;
        struct audio_buffer_pool* _initAudio();
        struct audio_buffer_pool* _audioBufferPool;
};