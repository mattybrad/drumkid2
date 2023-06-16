/*

Bradshaw Instruments
Drumkid V2 (Eurorack version)
Aleatoric drum machine

*/

// Include a bunch of libraries (possibly not all necessary)
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <stdio.h>
#include <math.h>

// Borrowing some useful Arduino macros
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))

// Include Pico-specific stuff and set up audio
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
#define LED_PIN 25
#define DATA_595 6
#define CLOCK_595 7
#define LATCH_595 8

// Drumkid classes
#include "Sample.h"
#include "Beat.h"

// Audio data (temporary, will be loaded from SD card?)
#include "kick.h"
#include "snare.h"
#include "closedhat.h"

// Borrowed/adapted from pico-playground
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

// 
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
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_init(DATA_595);
    gpio_set_dir(DATA_595, GPIO_OUT);
    gpio_init(CLOCK_595);
    gpio_set_dir(CLOCK_595, GPIO_OUT);
    gpio_init(LATCH_595);
    gpio_set_dir(LATCH_595, GPIO_OUT);

    struct audio_buffer_pool *ap = init_audio();

    int step = 0;
    int stepPosition = 0;
    int nextStepTime = 0;

    // temp, messing around
    int ledStepPosition = 0;
    int phase595 = 0;
    int ledData = 0;
    int storedLedData = 0;

    // init samples (temporary, will be from SD card?)
    Sample samples[3];
    samples[0].sampleData = sampleKick;
    samples[0].length = sampleKickLength;
    samples[1].sampleData = sampleSnare;
    samples[1].length = sampleSnareLength;
    samples[2].sampleData = sampleClosedHat;
    samples[2].length = sampleClosedHatLength;

    Beat testBeat;
    testBeat.addHit(0,0);
    testBeat.addHit(0,16);
    testBeat.addHit(1,8);
    testBeat.addHit(1,24);
    testBeat.addHit(1,28);
    testBeat.addHit(2,0);
    testBeat.addHit(2,4);
    testBeat.addHit(2,8);
    testBeat.addHit(2,12);
    testBeat.addHit(2,16);
    testBeat.addHit(2,20);
    testBeat.addHit(2,24);
    testBeat.addHit(2,28);

    // main loop
    while (true)
    {
        struct audio_buffer *buffer = take_audio_buffer(ap, true);
        int16_t *bufferSamples = (int16_t *)buffer->buffer->bytes;

        // update audio output
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
            if(stepPosition == 1000) {
                stepPosition = 0;
                step ++;
                if(step == 32) {
                    step = 0;
                }
                for(int j=0; j<3; j++) {
                    if(testBeat.hits[j][step]) samples[j].position = 0.0;
                    bitWrite(ledData, j, testBeat.hits[j][step]);
                }
                
            }

            ledStepPosition ++;
            if(ledStepPosition == 10) {
                ledStepPosition = 0;

                // update hardware states
                if (phase595 < 24)
                {
                    if (phase595 == 0)
                    {
                        gpio_put(LATCH_595, 0);
                        storedLedData = ledData;
                    }
                    if (phase595 % 3 == 0)
                    {
                        gpio_put(DATA_595, bitRead(storedLedData, phase595/3));
                        gpio_put(CLOCK_595, 0);
                    }
                    else if (phase595 % 3 == 1)
                    {
                        gpio_put(CLOCK_595, 1);
                    }
                    else if (phase595 % 3 == 2)
                    {
                        gpio_put(CLOCK_595, 0);
                    }
                }
                else
                {
                    gpio_put(LATCH_595, 1);
                }
                phase595++;
                if (phase595 == 25)
                    phase595 = 0;
            }
            
        }
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);

        samples[2].speed = 0.25 + 4.0 * ((float)adc_read()) / 4095.0;
    }
    return 0;
}
