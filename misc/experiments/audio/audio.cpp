#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/audio_i2s.h"

// Borrowed/adapted from pico-playground
struct audio_buffer_pool *init_audio()
{

    static audio_format_t audio_format = {
        44100,
        AUDIO_BUFFER_FORMAT_PCM_S16,
        2,
    };

    static struct audio_buffer_format producer_format = {
        .format = &audio_format,
        .sample_stride = 4};

    struct audio_buffer_pool *producer_pool = audio_new_producer_pool(&producer_format, 3, 64); // todo correct size
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

int main()
{
    stdio_init_all();

    struct audio_buffer_pool *ap = init_audio();

    printf("audio starting!\n");
    while (true) {
        struct audio_buffer *buffer = take_audio_buffer(ap, true);
        int16_t *bufferSamples = (int16_t *)buffer->buffer->bytes;
        // update audio output
        for (uint i = 0; i < buffer->max_sample_count * 2; i += 2)
        {
            // simple test tone
            // Generate a high-frequency square wave (period = 32 samples)
            bufferSamples[i] = bufferSamples[i + 1] = ((i / 2) % 64 < 32) ? 0x0FFF : -0x0FFF;
        }
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);
    }
}
