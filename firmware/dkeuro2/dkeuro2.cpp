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
    leds.setDisplay(0, Leds::asciiChars['b']);
    leds.setDisplay(1, Leds::asciiChars['e']);
    leds.setDisplay(2, Leds::asciiChars['s']);
    leds.setDisplay(3, Leds::asciiChars['t']);

    // interrupt for clock in pulse
    gpio_set_irq_enabled_with_callback(Pins::SYNC_IN, GPIO_IRQ_EDGE_FALL, true, pulseInCallback);

    audio.init();
    bool bufferReady = false;
    while(true) {
        // generate audio in pre-buffer
        if(!audio.preBufferReady) {
            for(uint i=0; i<audio.preBufferSize; i+=2) {
                // this is a really naive/dirty way of mapping the transport position to sample triggering for now
                if(i==0) {
                    uint thisTransportPosition = transport.getPositionFP() >> 32;
                    if(thisTransportPosition % 24 == 0) {
                        channels[0].samplePosition = 0; // kick on downbeat
                    }
                    if(thisTransportPosition % 48 == 24) {
                        channels[1].samplePosition = 0; // clap on "and" of 1
                    }
                    if(thisTransportPosition % 12 == 0) {
                        channels[2].samplePosition = 0; // hat on downbeat
                    }
                }

                int16_t sampleValue = 0;
                for(uint ch=0; ch<4; ch++) {
                    Channel &channel = channels[ch];
                    if(channel.samplePosition < channel.sampleLength) {
                        sampleValue += channel.sampleData[channel.samplePosition] >> 2; // reduce volume
                        channel.samplePosition++;
                    }
                }
                audio.preBuffer[i] = sampleValue; // left
                audio.preBuffer[i+1] = channels[0].sampleData[channels[0].sampleLength - channels[0].samplePosition - 1]; // right
            }
            audio.preBufferReady = true;
        }

        // check whether DAC needs data
        audio.update();

        transport.update();
    }
}