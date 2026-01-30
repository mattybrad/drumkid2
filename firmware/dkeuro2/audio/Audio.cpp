#include "Audio.h"
#include "pico/time.h"

void Audio::init() {
    _audioBufferPool = _initAudio();
    _buffer = nullptr;
}

bool Audio::samplesRequired() {
    return !_preBufferReady;
}

void Audio::queueSample(int16_t sampleLeft, int16_t sampleRight) {
    _preBuffer[_preBufferWriteIndex++] = sampleLeft;
    _preBuffer[_preBufferWriteIndex++] = sampleRight;
    if(_preBufferWriteIndex >= _preBufferSize) {
        _preBufferReady = true;
    }
}

void Audio::update() {
    bool audioProcessed = false;
    uint preBufferReadIndex = 0;
    while(true) {
        _buffer = take_audio_buffer(_audioBufferPool, false);
        if (_buffer == nullptr) {
            if(audioProcessed) {
                _preBufferReady = false;
                _preBufferWriteIndex = 0;
            }
            return;
        }
        audioProcessed = true;
        int16_t *bufferSamples = (int16_t *) _buffer->buffer->bytes;
        for (uint i = 0; i < _buffer->max_sample_count * 2; i+=2) {
            bufferSamples[i] = _preBuffer[preBufferReadIndex++]; // left
            bufferSamples[i+1] = _preBuffer[preBufferReadIndex++]; // right
        }
        _buffer->sample_count = _buffer->max_sample_count;
        give_audio_buffer(_audioBufferPool, _buffer);
        _lastDacUpdateTimeUs = time_us_64();
    }
}

int64_t Audio::lastDacUpdate() {
    return _lastDacUpdateTimeUs;
}

// Borrowed/adapted from pico-playground
struct audio_buffer_pool* Audio::_initAudio()
{

    static audio_format_t audio_format = {
        44100,
        AUDIO_BUFFER_FORMAT_PCM_S16,
        2,
    };

    static struct audio_buffer_format producer_format = {
        .format = &audio_format,
        .sample_stride = 4};

    struct audio_buffer_pool *producer_pool = audio_new_producer_pool(&producer_format, NUM_AUDIO_BUFFERS, SAMPLES_PER_BUFFER); // todo correct size
    bool __unused ok;
    const struct audio_format *output_format;
    struct audio_i2s_config config = {
        .data_pin = 9,
        .clock_pin_base = 10,
        .dma_channel = 2, // was 0, trying to avoid SD conflict
        .pio_sm = 0,
    };

    output_format = audio_i2s_setup(&audio_format, &config);
    if (!output_format)
    {
        panic("PicoAudio: Unable to open audio device.\n");
    }

    ok = audio_i2s_connect(producer_pool);
    assert(ok);
    audio_i2s_set_enabled(true);
    return producer_pool;
}