/*

Drumkid Eurorack module firmware
by Bradshaw Instruments (Matt Bradshaw)
Version: 2.0.0
Started Jan 2026

*/

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"
#include "pico/audio_i2s.h"

#define SAMPLES_PER_BUFFER 8

void secondCoreCode() {
    while (true) {
        // Code for the second core
        sleep_ms(50);
        multicore_fifo_push_blocking(1);
        sleep_ms(5);
        multicore_fifo_push_blocking(0);
    }
}

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

int64_t pulseCount = 0; // total pulses received since start
int64_t lastPulseTime = 0; // time of last pulse in microseconds
int64_t derivedPositionFP = 0; // Q32.32
int64_t rateFP = 0; // Q32.32
int64_t lastUpdateTime = 0; // time of last transport update in microseconds

void updateTransport() {
    int64_t now = time_us_64();
    int64_t deltaTime = now - lastUpdateTime;
    lastUpdateTime = now;
    derivedPositionFP += (rateFP * deltaTime); // Q32.32
    printf("Updated position: %lld.%09lld\n", 
        (derivedPositionFP >> 32), 
        ((derivedPositionFP & 0xFFFFFFFF) * 1000000000LL) >> 32);
}

void clockInCallback(uint gpio, uint32_t events)
{
    uint64_t now = time_us_64();

    pulseCount++;
    printf("Pulse count: %lld\n", pulseCount);

    if(pulseCount == 1) {
        printf("First pulse received!\n");
        lastPulseTime = now;
    } else if(pulseCount >= 2) {
        int64_t period = now - lastPulseTime;
        lastPulseTime = now;
        printf("Period since last pulse: %lldus\n", period);

        derivedPositionFP = ((int64_t)pulseCount) << 32;
        rateFP = ((int64_t)(1ULL << 32)) / period;  // pulses per microsecond
        printf("Derived position: %lld.%09lld\n", 
            (derivedPositionFP >> 32), 
            ((derivedPositionFP & 0xFFFFFFFF) * 1000000000LL) >> 32);
        printf("Derived rate: %lld (Q32.32)\n", rateFP);
    }
}

int main()
{
    stdio_init_all();
    multicore_launch_core1(secondCoreCode); // launch second core

    gpio_init(16);
    gpio_set_dir(16, GPIO_IN);
    gpio_init(17);
    gpio_set_dir(17, GPIO_OUT);
    gpio_init(15);
    gpio_set_dir(15, GPIO_OUT);

    // interrupt for clock in pulse
    gpio_set_irq_enabled_with_callback(16, GPIO_IRQ_EDGE_FALL, true, &clockInCallback);

    // while (true) {
    //     uint32_t received = multicore_fifo_pop_blocking();
    //     printf("Received from core 1: %d\n", received);

    //     if (received == 0) {
    //         gpio_put(17, 0); // Set GPIO 17 high
    //     } else {
    //         gpio_put(17, 1); // Set GPIO 17 low
    //     }
    // }

    struct audio_buffer_pool *ap = init_audio();
    while(true) {

        struct audio_buffer *buffer = take_audio_buffer(ap, true);
        int16_t *samples = (int16_t *) buffer->buffer->bytes;
        for (uint i = 0; i < buffer->max_sample_count * 2; i++) {
            samples[i] = rand() % 32768 - 16384; // random noise
        }
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);

        updateTransport();
    }
}
