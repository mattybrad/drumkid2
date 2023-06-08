/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <math.h>

#if PICO_ON_DEVICE

#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"

#endif

#include "pico/stdlib.h"

#include "pico/audio_i2s.h"

#if PICO_ON_DEVICE
#include "pico/binary_info.h"
bi_decl(bi_3pins_with_names(PICO_AUDIO_I2S_DATA_PIN, "I2S DIN", PICO_AUDIO_I2S_CLOCK_PIN_BASE, "I2S BCK", PICO_AUDIO_I2S_CLOCK_PIN_BASE + 1, "I2S LRCK"));
#endif

#define SAMPLES_PER_BUFFER 256

#include "kick.h"
#include "snare.h"
#include "closedhat.h"

struct audio_buffer_pool *init_audio()
{

    static audio_format_t audio_format = {
        44100,
        AUDIO_BUFFER_FORMAT_PCM_S16,
        1,
    };

    static struct audio_buffer_format producer_format = {
        .format = &audio_format,
        .sample_stride = 2};

    struct audio_buffer_pool *producer_pool = audio_new_producer_pool(&producer_format, 3,
                                                                      SAMPLES_PER_BUFFER); // todo correct size
    bool __unused ok;
    const struct audio_format *output_format;
    struct audio_i2s_config config = {
        .data_pin = 9,
        .clock_pin_base = 10,
        .dma_channel = 0,
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

int thing = 0;
int step = 0;
int kickPos = 0;
int snarePos = 0;
int closedHatPos = 0;

int main()
{

    stdio_init_all();

    struct audio_buffer_pool *ap = init_audio();

    while (true)
    {
        struct audio_buffer *buffer = take_audio_buffer(ap, true);
        int16_t *samples = (int16_t *)buffer->buffer->bytes;
        for (uint i = 0; i < buffer->max_sample_count; i++)
        {
            thing++;
            kickPos ++;
            snarePos ++;
            closedHatPos ++;
            if (thing >= 5500) {
                thing = 0;
                step ++;
                if(step == 8) step = 0;
                if(step%4 == 0) kickPos = 0;
                if(step == 4) snarePos = 0;
                closedHatPos = 0;
            }
            samples[i] = 0;
            if(kickPos<sampleKickLength) samples[i] += 0.25 * sampleKick[kickPos];
            if(snarePos<sampleSnareLength) samples[i] += 0.25 * sampleSnare[snarePos];
            if (closedHatPos < sampleClosedHatLength) samples[i] += 0.25 * sampleClosedHat[closedHatPos];
        }
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);
    }
    return 0;
}
