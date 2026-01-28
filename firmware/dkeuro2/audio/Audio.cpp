#include "Audio.h"

void Audio::init() {
    audioBufferPool = init_audio();
    buffer = nullptr;
}

bool Audio::giveSample(int16_t sample) {
    if(buffer == nullptr) {
        buffer = take_audio_buffer(audioBufferPool, false);
        if(buffer == nullptr) {
            return false;
        } else {
            bufferSamples = (int16_t *) buffer->buffer->bytes;
            bufferIndex = 0;
        }
    }


    bufferSamples[bufferIndex] = sample;
    bufferIndex++;
    if(bufferIndex == buffer->max_sample_count * 2) {
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(audioBufferPool, buffer);
        buffer = nullptr;
        bufferIndex = 0;
        return false;
    }

    return true;
}

// void Audio::update() {
//     buffer = take_audio_buffer(audioBufferPool, true);
//     int16_t *bufferSamples = (int16_t *) buffer->buffer->bytes;
//     for (uint i = 0; i < buffer->max_sample_count * 2; i+=2) {
//         int16_t thisSample = rand() % 32768 - 16384; // random noise
//         bufferSamples[i] = thisSample;
//         bufferSamples[i+1] = thisSample; // stereo
//     }
//     buffer->sample_count = buffer->max_sample_count;
//     give_audio_buffer(audioBufferPool, buffer);
// }

// Borrowed/adapted from pico-playground
struct audio_buffer_pool* Audio::init_audio()
{

    static audio_format_t audio_format = {
        44100,
        AUDIO_BUFFER_FORMAT_PCM_S16,
        2,
    };

    static struct audio_buffer_format producer_format = {
        .format = &audio_format,
        .sample_stride = 4};

    struct audio_buffer_pool *producer_pool = audio_new_producer_pool(&producer_format, 3, SAMPLES_PER_BUFFER); // todo correct size
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