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
#define LOAD_165 12
#define CLOCK_165 13
#define DATA_165 14

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

// temporary (ish?) LED variables
int ledStepPosition = 0;
int phase595 = 0; // 25 "phases" of the 595 shift register
int ledData = 0;
int storedLedData = 0;

void updateLeds() {
    ledStepPosition++;
    if (ledStepPosition == 10)
    {
        ledStepPosition = 0;

        // update shift register
        if (phase595 < 24)
        {
            if (phase595 == 0)
            {
                gpio_put(LATCH_595, 0);
                storedLedData = ledData;
            }
            if (phase595 % 3 == 0)
            {
                gpio_put(DATA_595, bitRead(storedLedData, phase595 / 3));
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

int buttonStepPosition = 0;
int phase165 = 0;
int buttonStates = 0;

void updateButtons() {
    buttonStepPosition ++;
    if(buttonStepPosition == 10) {
        buttonStepPosition = 0;

        // update shift register
        if(phase165 == 0) {
            gpio_put(LOAD_165, 0);
        } else if(phase165 == 1) {
            gpio_put(LOAD_165, 1);
        } else if(phase165 % 2 == 0) {
            bitWrite(buttonStates, (phase165 - 2)/2, !gpio_get(DATA_165));
            gpio_put(CLOCK_165, 0);
        } else if(phase165 % 2 == 1) {
            gpio_put(CLOCK_165, 1);
        }
        phase165 ++;
        if(phase165 == 18) {
            phase165 = 0;
            ledData = buttonStates;
        }
    }
}

int analogStepPosition = 0;
int phase4051 = 0;
int analogReadings[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

void updateAnalog() {
    analogStepPosition ++;
    if(analogStepPosition == 10) {
        analogStepPosition = 0;

        if(phase4051 % 2 == 0) {
            gpio_put(MUX_ADDR_A, bitRead(phase4051/2, 0));
            gpio_put(MUX_ADDR_B, bitRead(phase4051/2, 1));
            gpio_put(MUX_ADDR_C, bitRead(phase4051/2, 2));
        } else {
            adc_select_input(0);
            analogReadings[(phase4051-1)/2] = adc_read();
            adc_select_input(1);
            analogReadings[(phase4051 - 1) / 2 + 8] = adc_read();
        }

        phase4051 ++;
        if(phase4051 == 16) phase4051 = 0;

    }
}

// main function, obviously
int main()
{

    stdio_init_all();

    // init GPIO
    adc_init();
    adc_gpio_init(MUX_READ_POTS);
    adc_gpio_init(MUX_READ_CV);
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
    gpio_init(LOAD_165);
    gpio_set_dir(LOAD_165, GPIO_OUT);
    gpio_init(CLOCK_165);
    gpio_set_dir(CLOCK_165, GPIO_OUT);
    gpio_init(DATA_165);
    gpio_set_dir(DATA_165, GPIO_IN);

    struct audio_buffer_pool *ap = init_audio();

    int step = 0;
    int stepPosition = 0;
    int nextStepTime = 0;

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

    // main loop, runs forever
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
            if(stepPosition == 2000) {
                stepPosition = 0;
                step ++;
                if(step == 32) {
                    step = 0;
                }
                printf("analog: ");
                for(int j=0; j<16; j++) {
                    printf("%d ", analogReadings[j]);
                }
                printf("\n");
                for(int j=0; j<3; j++) {
                    if(testBeat.hits[j][step]) samples[j].position = 0.0;
                    //bitWrite(ledData, j, testBeat.hits[j][step]);
                }
                
            }

            updateButtons();
            updateLeds();
            updateAnalog();
            
        }
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);

        //samples[2].speed = 0.25 + 4.0 * ((float)adc_read()) / 4095.0;
    }
    return 0;
}
