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
#include "rhythm/Transport.h"
#include "rhythm/Beat.h"

#include "audio/TestKick.h"
#include "audio/TestClap.h"
#include "audio/TestHat.h"
#include "audio/TestTom.h"

Leds leds;
Buttons buttons;
Audio audio;
Channel channels[4];
Transport transport;

// Static wrapper function for GPIO interrupt
void pulseInCallback(uint gpio, uint32_t events) {
    transport.pulseIn();
}

int main()
{
    stdio_init_all();

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
    leds.setDisplay(0, Leds::asciiChars['M']);
    leds.setDisplay(1, Leds::asciiChars['L']);
    leds.setDisplay(2, Leds::asciiChars['B']);
    leds.setDisplay(3, Leds::asciiChars[' ']);

    // interrupt for clock in pulse
    gpio_set_irq_enabled_with_callback(Pins::SYNC_IN, GPIO_IRQ_EDGE_FALL, true, pulseInCallback);

    audio.init();
    transport.init();
    int64_t lastTransportPositionFP = 0;
    uint64_t now;
    while(true) {
        // generate audio in pre-buffer
        uint sampleCount = 0;
        now = time_us_64();
        while(audio.samplesRequired()) {
            uint32_t thisTransportPosition = transport.getPositionAtTimeFP(now+(sampleCount*22)) >> 32; // 22us per sample at 44.1kHz, approximate for now

            if(thisTransportPosition != lastTransportPositionFP) {
                lastTransportPositionFP = thisTransportPosition;
                
                // check for triggers
                if(thisTransportPosition % 24 == 0) {
                    channels[0].samplePosition = 0;
                }
                if(thisTransportPosition % 48 == 24) {
                    channels[1].samplePosition = 0;
                }
                if(thisTransportPosition % 6 == 0) {
                    channels[2].samplePosition = 0;
                }
            }

            int16_t leftSample = 0;
            int16_t rightSample = 0;
            for(uint ch=0; ch<4; ch++) {
                Channel &channel = channels[ch];
                if(channel.samplePosition < channel.sampleLength) {
                    leftSample += channel.sampleData[channel.samplePosition] >> 2;
                    channel.samplePosition++;
                }
            }
            rightSample = channels[3].sampleData[channels[3].sampleLength - channels[3].samplePosition - 1];

            audio.queueSample(leftSample, rightSample);
            sampleCount++;
        }
        // if(sampleCount > 0) {
        //     uint64_t elapsed = time_us_64() - now;
        //     printf("%llu\n", elapsed);
        // }

        // check whether DAC needs data
        audio.update();

        transport.update();
    }
}