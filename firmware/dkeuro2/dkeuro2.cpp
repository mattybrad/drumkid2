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
#include "hardware/Memory.h"
#include "hardware/CardReader.h"
#include "audio/Audio.h"
#include "audio/Channel.h"
#include "rhythm/Transport.h"
#include "rhythm/Beat.h"

#include "audio/TestKick.h"
#include "audio/TestClap.h"
#include "audio/TestHat.h"
#include "audio/TestTom.h"
#include "audio/TestLong.h"

#define NUM_CHANNELS 8

CardReader cardReader;
Memory memory;
Leds leds;
Buttons buttons;
Audio audio;
Channel channels[NUM_CHANNELS];
Transport transport;

// Static wrapper function for GPIO interrupt
void pulseInCallback(uint gpio, uint32_t events) {
    transport.pulseIn();
}

int main()
{
    stdio_init_all();

    //cardReader.init();
    //cardReader.transferWavToFlash("samples/8bit/1.wav");
    //cardReader.transferWavToFlash("samples/8bit/2.wav");

    memory.init();
    uint8_t testData[256] = {0};
    for(uint i = 0; i < sizeof(testData); i++) {
        testData[i] = i % 100;
    }
    memory.writeToFlashPage(256*16, testData);
    memory.writeToFlashPage(256*16+1, testData);

    const uint8_t *testRead = (const uint8_t *)(XIP_BASE+256*16*FLASH_PAGE_SIZE+98);
    printf("Data read from flash: %d, %d, %d, %d\n", testRead[0], testRead[1], testRead[2], testRead[3]);

    gpio_init(Pins::SYNC_IN);
    gpio_set_dir(Pins::SYNC_IN, GPIO_IN);
    gpio_init(Pins::SYNC_OUT);
    gpio_set_dir(Pins::SYNC_OUT, GPIO_OUT);
    gpio_init(Pins::TRIGGER_1);
    gpio_set_dir(Pins::TRIGGER_1, GPIO_OUT);

    for(uint i = 0; i < NUM_CHANNELS; i++) {
        channels[i].init();
    }
    channels[0].sampleData = testKick;
    channels[0].sampleLength = testKickLength;
    channels[1].sampleData = testClap;
    channels[1].sampleLength = testClapLength;
    channels[2].sampleData = testHat;
    channels[2].sampleLength = testHatLength;
    for(uint i = 3; i < NUM_CHANNELS; i++) {
        channels[i].sampleData = testTom;
        channels[i].sampleLength = testTomLength;
        channels[i].playbackSpeed = (int64_t)((0.1+0.25*i) * (1LL << 32));
    }

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
    int longestTime = 0;
    int dacIntervalUs = audio.dacIntervalUs();
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
                    channels[0].samplePositionFP = 0;
                }
                if(thisTransportPosition % 48 == 24) {
                    channels[1].samplePosition = 0;
                    channels[1].samplePositionFP = 0;
                }
                if(thisTransportPosition % 6 == 0) {
                    channels[2].samplePosition = 0;
                    channels[2].samplePositionFP = 0;
                }
                for(uint i = 3; i < NUM_CHANNELS; i++) {
                    if(thisTransportPosition % 4 == 0 && rand() % 4 == 0) {
                        channels[i].samplePosition = 0;
                        channels[i].samplePositionFP = 0;
                    }
                }
            }

            int16_t leftSample = 0;
            int16_t rightSample = 0;
            for(uint ch=0; ch<NUM_CHANNELS; ch++) {
                Channel &channel = channels[ch];
                if(channel.samplePosition < channel.sampleLength) {
                    //leftSample += channel.sampleData[channel.samplePosition] >> 2;
                    int16_t lerpedSample = 0;
                    // simple linear interpolation
                    uint32_t indexInt = channel.samplePositionFP >> 32;
                    uint32_t indexFrac = channel.samplePositionFP & 0xFFFFFFFF;
                    if(indexInt + 1 < channel.sampleLength) {
                        int16_t sample1 = channel.sampleData[indexInt];
                        int16_t sample2 = channel.sampleData[indexInt + 1];
                        lerpedSample = sample1 + ((int64_t)(sample2 - sample1) * indexFrac >> 32);
                    } else {
                        lerpedSample = channel.sampleData[indexInt];
                    }
                    leftSample += lerpedSample >> 4;
                    channel.samplePositionFP += channel.playbackSpeed;
                    channel.samplePosition = channel.samplePositionFP >> 32;
                }
            }
            rightSample = channels[3].sampleData[channels[3].sampleLength - channels[3].samplePosition - 1];

            audio.queueSample(leftSample, rightSample);
            sampleCount++;
        }
        if(sampleCount > 0) {
            uint64_t elapsed = time_us_64() - now;
            if(elapsed > longestTime && time_us_64() > 2000000) {
                longestTime = elapsed;
                //printf("%d\n", longestTime);
            }
        }

        // do regular hardware updates if enough time remains before next DAC update
        if(time_us_64() - audio.lastDacUpdate() < dacIntervalUs - 250) {
            if(time_us_64() - leds.lastUpdate() > 2000) {
                leds.update();
            }
            if(time_us_64() - buttons.lastUpdate() > 1000) {
                buttons.update();
            }
            // 4051 mux update will also go here
        }


        // check whether DAC needs data
        audio.update();

        transport.update();
    }
}