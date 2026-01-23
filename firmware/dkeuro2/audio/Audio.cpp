#include "Audio.h"
#include "testsample.h"

void Audio::init() {
    audioBufferPool = init_audio();
    for(uint s = 0; s < 8; s++) {
        //samples[s].init();
        samples[s].pos = s*1000; // start each sample at different position
    }
}

void Audio::update() {
    struct audio_buffer *buffer = take_audio_buffer(audioBufferPool, true);
    int16_t *bufferSamples = (int16_t *) buffer->buffer->bytes;
    uint testSampleLen = sizeof(testSample) / sizeof(testSample[0]);
    for (uint i = 0; i < buffer->max_sample_count * 2; i+=2) {
        //bufferSamples[i] = rand() % 32768 - 16384; // random noise
        int16_t thisSample = 0;
        for(uint s = 0; s < 8; s++) {
            thisSample += testSample[samples[s].pos] >> 4; // mix 8 samples, simple average
            samples[s].pos = (samples[s].pos + 1) % testSampleLen;
        }
        bufferSamples[i] = thisSample;
        bufferSamples[i+1] = thisSample; // stereo
    }
    buffer->sample_count = buffer->max_sample_count;
    give_audio_buffer(audioBufferPool, buffer);
}

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