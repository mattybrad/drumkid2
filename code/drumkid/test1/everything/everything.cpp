#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#include <stdio.h>
#include <math.h>

#if PICO_ON_DEVICE

#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"
#include "hardware/adc.h"

#endif

#include "pico/stdlib.h"

#include "pico/audio_i2s.h"

#if PICO_ON_DEVICE
#include "pico/binary_info.h"
bi_decl(bi_3pins_with_names(PICO_AUDIO_I2S_DATA_PIN, "I2S DIN", PICO_AUDIO_I2S_CLOCK_PIN_BASE, "I2S BCK", PICO_AUDIO_I2S_CLOCK_PIN_BASE + 1, "I2S LRCK"));
#endif

#define SAMPLES_PER_BUFFER 256

// pins
#define MUX_ADDR_A 19
#define MUX_ADDR_B 20
#define MUX_ADDR_C 21
#define MUX_READ_POTS 26
#define MUX_READ_CV 27

#include "Sample.h"

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

int main()
{

    stdio_init_all();

    // init GPIO
    adc_init();
    adc_gpio_init(MUX_READ_POTS);
    adc_gpio_init(MUX_READ_CV);
    adc_select_input(0);
    gpio_init(MUX_ADDR_A);
    gpio_set_dir(MUX_ADDR_A, GPIO_OUT);
    gpio_init(MUX_ADDR_B);
    gpio_set_dir(MUX_ADDR_B, GPIO_OUT);
    gpio_init(MUX_ADDR_C);
    gpio_set_dir(MUX_ADDR_C, GPIO_OUT);

    struct audio_buffer_pool *ap = init_audio();

    int step = 0;
    int stepPosition = 0;
    int nextStepTime = 0;

    // init samples
    Sample samples[3];
    samples[0].sampleData = sampleKick;
    samples[0].length = sampleKickLength;
    samples[1].sampleData = sampleSnare;
    samples[1].length = sampleSnareLength;
    samples[2].sampleData = sampleClosedHat;
    samples[2].length = sampleClosedHatLength;

    while (true)
    {
        struct audio_buffer *buffer = take_audio_buffer(ap, true);
        int16_t *bufferSamples = (int16_t *)buffer->buffer->bytes;
        for (uint i = 0; i < buffer->max_sample_count; i++)
        {
            // sample updates go here
            samples[0].update();
            samples[1].update();
            samples[2].update();
            float floatValue = 0.25 * ((float)samples[0].value+(float)samples[1].value+(float)samples[2].value);
            bufferSamples[i] = (int)floatValue;

            // increment step if needed
            stepPosition ++;
            if(stepPosition == 5000) {
                stepPosition = 0;
                step ++;
                if(step == 8) {
                    step = 0;
                }
                if(step == 0) samples[0].position = 0.0;
                if(step == 4) samples[1].position = 0.0;
                samples[2].position = 0.0;
            }
        }
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);

        samples[2].speed = 0.25 + 4.0 * ((float)adc_read()) / 4095.0;
    }
    return 0;
}
