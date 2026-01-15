/*

Drumkid Eurorack module firmware
by Bradshaw Instruments (Matt Bradshaw)
Version: 2.0.0
Started Jan 2026

*/

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"
#include "pico/audio_i2s.h"
#include "dkeuro2.h"
#include "hardware/Pins.h"
#include "hardware/Leds.h"
#include "hardware/Buttons.h"

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
    int64_t lastPositionFP; // Q32.32, pulses
    int64_t rateFP;     // Q32.32, pulses per microsecond
    int64_t lastUpdateTime; // microseconds
    int64_t lastPulseTime;   // microseconds
    int64_t nextPulseTimeEstimate; // microseconds
    bool firstPulseReceived;
    bool secondPulseReceived;
} transport_t;

transport_t transport = {
    .pulseInCount = 0,
    .positionFP = 0,
    .lastPositionFP = 0,
    .rateFP = 0,
    .lastUpdateTime = 0,
    .lastPulseTime = 0,
    .nextPulseTimeEstimate = 0,
    .firstPulseReceived = false,
    .secondPulseReceived = false
};

Leds leds;
Buttons buttons;

int64_t tempTriggerPulseOffCallback(alarm_id_t id, void *user_data)
{
    gpio_put(Pins::TRIGGER_1, 0);
    return 0;
}

bool gateState = false;
int64_t tempTriggerGateOffCallback(alarm_id_t id, void *user_data)
{
    gateState = false;
    return 0;
}

void tempTriggerPulse() {
    gpio_put(Pins::TRIGGER_1, 1);
    add_alarm_in_us(500, tempTriggerPulseOffCallback, NULL, true);
}

void tempTriggerGate() {
    gateState = true;
    add_alarm_in_ms(50, tempTriggerGateOffCallback, NULL, true);
}

int main()
{
    stdio_init_all();
    multicore_launch_core1(secondCoreCode); // launch second core

    gpio_init(Pins::SYNC_IN);
    gpio_set_dir(Pins::SYNC_IN, GPIO_IN);
    gpio_init(Pins::SYNC_OUT);
    gpio_set_dir(Pins::SYNC_OUT, GPIO_OUT);
    gpio_init(Pins::TRIGGER_1);
    gpio_set_dir(Pins::TRIGGER_1, GPIO_OUT);

    buttons.init();

    leds.init();
    leds.setDisplay(0, Leds::asciiChars['c']);
    leds.setDisplay(1, Leds::asciiChars['a']);
    leds.setDisplay(2, Leds::asciiChars['t']);
    while(true) {
        leds.setLed(Leds::CLOCK_OUT, true);
        leds.setLed(Leds::PULSE, true);
        leds.setLed(Leds::ERROR, false);
        leds.setDisplay(3, Leds::asciiChars['t']);
        sleep_ms(500);
        leds.setLed(Leds::CLOCK_OUT, false);
        leds.setLed(Leds::PULSE, false);
        leds.setLed(Leds::ERROR, true);
        leds.setDisplay(3, Leds::asciiChars['b']);
        sleep_ms(500);
    }

    // interrupt for clock in pulse
    gpio_set_irq_enabled_with_callback(Pins::SYNC_IN, GPIO_IRQ_EDGE_FALL, true, &clockInCallback);

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
            samples[i] = gateState ? rand() % 32768 - 16384 : 0; // random noise
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
    int64_t period = transport.nextPulseTimeEstimate - transport.lastPulseTime;
    transport.lastPositionFP = transport.positionFP;
    transport.positionFP = (transport.pulseInCount << 32) + ((int64_t)(now - transport.nextPulseTimeEstimate) << 32) / period;
    uint64_t wholePulsePosition = transport.positionFP >> 32;
    if(wholePulsePosition > (transport.lastPositionFP >> 32)) {
        if(wholePulsePosition % 24 == 0) {
            tempTriggerPulse();
        }
        if(wholePulsePosition % 24 == 0 || wholePulsePosition % 24 == 6) {
            tempTriggerGate();
        }
    }
}

void clockInCallback(uint gpio, uint32_t events)
{
    uint64_t now = time_us_64();
    int64_t period = now - transport.lastPulseTime;
    transport.pulseInCount++;

    if(!transport.firstPulseReceived) {
        transport.lastPulseTime = now;
        transport.firstPulseReceived = true;
        return;
    }

    transport.nextPulseTimeEstimate = now + period;
    transport.lastPulseTime = now;

    if(!transport.secondPulseReceived) {
        transport.secondPulseReceived = true;
        return;
    }
}

uint8_t Leds::asciiChars[] = {
    0b00000000, // 0 NULL
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000, // 32 SPACE
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b01000000, // -
    0b10000000, // .
    0b00000000,
    0b00111111, // digit 0
    0b00000110,
    0b01011011,
    0b01001111,
    0b01100110,
    0b01101101,
    0b01111101,
    0b00000111,
    0b01111111,
    0b01101111, // digit 9
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b01010011,
    0b00000000, // 64
    0b01110111, // A (a)
    0b01111100,
    0b00111001,
    0b01011110,
    0b01111001,
    0b01110001,
    0b01101111,
    0b01110110,
    0b00000110,
    0b00001111,
    0b01110110,
    0b00111000,
    0b00110111,
    0b01010100,
    0b01011100,
    0b01110011,
    0b01100111,
    0b01010000,
    0b01101101,
    0b01111000,
    0b00111110,
    0b00011100,
    0b00111110,
    0b01110110,
    0b01101110,
    0b01011011, // Z (z)
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00001000, // _
    0b00000000, // 96
    0b01110111, // a
    0b01111100,
    0b00111001,
    0b01011110,
    0b01111001,
    0b01110001,
    0b01101111,
    0b01110110,
    0b00000110,
    0b00001111,
    0b01110110,
    0b00111000,
    0b00110111,
    0b01010100,
    0b01011100,
    0b01110011,
    0b01100111,
    0b01010000,
    0b01101101,
    0b01111000,
    0b00111110,
    0b00011100,
    0b00111110,
    0b01110110,
    0b01101110,
    0b01011011, // z
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
};

