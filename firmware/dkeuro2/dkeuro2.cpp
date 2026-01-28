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
#include "dkeuro2.h"
#include "hardware/Pins.h"
#include "hardware/Leds.h"
#include "hardware/Buttons.h"
#include "audio/Audio.h"
#include "audio/Channel.h"

#include "audio/TestKick.h"
#include "audio/TestClap.h"
#include "audio/TestHat.h"
#include "audio/TestTom.h"

void secondCoreCode() {
    while (true) {
        // Code for the second core
        sleep_ms(50);
        multicore_fifo_push_blocking(1);
        sleep_ms(5);
        multicore_fifo_push_blocking(0);
    }
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
Audio audio;
Channel channels[4];

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

    for(uint i = 0; i < 4; i++) {
        channels[i].init();
    }
    channels[0].sampleData = testKick;
    channels[0].sampleLength = testKickLength;
    channels[1].sampleData = testClap;
    channels[1].sampleLength = testClapLength;
    channels[2].sampleData = testHat;
    channels[2].sampleLength = testHatLength;
    channels[3].sampleData = testTom;
    channels[3].sampleLength = testTomLength;

    buttons.init();

    leds.init();
    leds.setDisplay(0, Leds::asciiChars['b']);
    leds.setDisplay(1, Leds::asciiChars['e']);
    leds.setDisplay(2, Leds::asciiChars['s']);
    leds.setDisplay(3, Leds::asciiChars['t']);

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

    audio.init();
    uint64_t lastAudioUpdateTime = time_us_64();
    int printIndex = 0;
    int16_t nextAudio[32];
    bool bufferReady = false;
    while(true) {
        int64_t startTime = time_us_64();
        bool anyUpdates = false;
        int numUpdates = 0;
        while(audio.bufferNeedsData()) {
            anyUpdates = true;
            audio.giveSample(nextAudio[numUpdates], random() % 32768 - 16384);
            numUpdates++;
        }
        if(anyUpdates) {
            bufferReady = false;
            int64_t timeTaken = time_us_64() - startTime;
            int64_t timeSinceLastAudioUpdate = time_us_64() - lastAudioUpdateTime;
            lastAudioUpdateTime = time_us_64();
            printIndex++;
            if(printIndex % 1000 == 0) {
                //printf("Audio buffer filled in %lldus, last update %lldus ago, %d updates\n", timeTaken, timeSinceLastAudioUpdate, numUpdates);
            }
        }

        // generate audio here for next buffer(s)
        if(!bufferReady) {
            for(int i=0; i<32; i++) {
                int16_t sample = 0;
                for(int ch=0; ch<4; ch++) {
                    sample += channels[ch].sampleData[channels[ch].samplePosition] >> 2; // divide by 4 to avoid clipping
                    channels[ch].samplePosition = (channels[ch].samplePosition + 1) % channels[ch].sampleLength;
                }
                nextAudio[i] = sample;
            }
            bufferReady = true;
        }

        //updateTransport();
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