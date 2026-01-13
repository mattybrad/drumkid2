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
#include "dkeuro2.h"

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

typedef struct {
    int64_t pulseInCount; // total pulses received
    int64_t positionFP; // Q32.32, pulses
    int64_t rateFP;     // Q32.32, pulses per microsecond
    int64_t lastUpdateTime; // microseconds
    int64_t lastPulseTime;   // microseconds
    bool firstPulseReceived;
    bool secondPulseReceived;
} transport_t;

transport_t transport = {
    .pulseInCount = 0,
    .positionFP = 0,
    .rateFP = 0,
    .lastUpdateTime = 0,
    .lastPulseTime = 0,
    .firstPulseReceived = false,
    .secondPulseReceived = false
};

int64_t tempTriggerPulseOffCallback(alarm_id_t id, void *user_data)
{
    gpio_put(15, 0);
    return 0;
}

void tempTriggerPulse() {
    gpio_put(15, 1);
    add_alarm_in_ms(5, tempTriggerPulseOffCallback, NULL, true);
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

void updateTransport() {
    if(!transport.secondPulseReceived) {
        return; // can't update until we have a rate
    }

    uint64_t now = time_us_64();
    uint64_t deltaTime = now - transport.lastUpdateTime;
    transport.lastUpdateTime = now;
    int64_t lastPosition = transport.positionFP;
    transport.positionFP += (transport.rateFP * deltaTime); // Q32.32
    if((transport.positionFP >> 32) > (lastPosition >> 32)) {
        // Integer part of position has incremented
        printf("Position incremented to %lld\n", transport.positionFP >> 32);
        printf("Updated position: %lld.%09lld\n", 
        (transport.positionFP >> 32), 
        ((transport.positionFP & 0xFFFFFFFF) * 1000000000LL) >> 32);
        tempTriggerPulse();
    }

    // printf("Updated position: %lld.%09lld\n", 
    //     (transport.positionFP >> 32), 
    //     ((transport.positionFP & 0xFFFFFFFF) * 1000000000LL) >> 32);
}

void clockInCallback(uint gpio, uint32_t events)
{
    uint64_t now = time_us_64();
    transport.pulseInCount++;

    if(!transport.firstPulseReceived) {
        transport.lastPulseTime = now;
        transport.firstPulseReceived = true;
        printf("First pulse received!\n");
        return;
    }

    int64_t period = now - transport.lastPulseTime;

    if(!transport.secondPulseReceived) {
        transport.rateFP = ((int64_t)1ULL << 32) / period;
        transport.positionFP = ((int64_t)transport.pulseInCount) << 32;
        transport.secondPulseReceived = true;
        transport.lastUpdateTime = now;
        printf("Second pulse received! Period: %lld us, Rate: %lld (Q32.32)\n", period, transport.rateFP);
    } else {
        transport.rateFP = ((int64_t)1ULL << 32) / period;
        printf("Rate: %lld.%09lld\n", 
        (transport.rateFP >> 32), 
        ((transport.rateFP & 0xFFFFFFFF) * 1000000000LL) >> 32);
        int64_t predictedPulse = transport.positionFP >> 32;
        int64_t phaseError = (int64_t)transport.pulseInCount - predictedPulse;
        int64_t correction = (phaseError << 32) / 4; // simple proportional correction, gain = 1/8
        transport.positionFP += correction;
        //transport.positionFP += phaseError;
    }
        
    transport.lastPulseTime = now;
}