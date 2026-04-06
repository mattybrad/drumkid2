/*

Drumkid Eurorack module firmware
by Bradshaw Instruments (Matt Bradshaw)
Version: 2.0.0
Started Jan 2026

*/

#include <stdio.h>
#include <algorithm>
#include <string>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"
#include "dkeuro2.h"
#include "Config.h"
#include "hardware/Pins.h"
#include "hardware/Leds.h"
#include "hardware/Buttons.h"
#include "hardware/Memory.h"
#include "hardware/CardReader.h"
#include "audio/Audio.h"
#include "audio/KitManager.h"
#include "audio/Channel.h"
#include "audio/ChannelManager.h"
#include "rhythm/Transport.h"
#include "rhythm/Beat.h"
#include "interface/Menu.h"

CardReader cardReader;
Memory memory;
Leds leds;
Buttons buttons;
Audio audio;
Transport transport;
//Channel channels[MAX_CHANNELS];
ChannelManager channelManager;
KitManager kitManager;
Menu menu;

// Static wrapper function for GPIO interrupt
void pulseInCallback(uint gpio, uint32_t events) {
    transport.pulseIn();
}

int main()
{
    channelManager.init();
    // for(uint i = 0; i < MAX_CHANNELS; i++) {
    //     channels[i].init();
    // }

    stdio_init_all();
    
    gpio_init(Pins::SYNC_IN);
    gpio_set_dir(Pins::SYNC_IN, GPIO_IN);
    gpio_init(Pins::SYNC_OUT);
    gpio_set_dir(Pins::SYNC_OUT, GPIO_OUT);
    gpio_init(Pins::TRIGGER_1);
    gpio_set_dir(Pins::TRIGGER_1, GPIO_OUT);
    
    memory.init();
    kitManager.init(&memory, &channelManager);
    cardReader.init(&memory);
    buttons.init();
    leds.init();
    menu.init(&leds, &memory, &cardReader, &kitManager);

    // temporarily set channel data from kitmanager here
    // for(int i=0; i<MAX_CHANNELS; i++) {
    //     if(kitManager.kits[0].numSamples > i) {
    //         channelManager.channels[i].sampleData = (const int16_t *)(XIP_BASE + (kitManager.kits[0].samples[i].address * FLASH_PAGE_SIZE));
    //         channelManager.channels[i].sampleLength = kitManager.kits[0].samples[i].lengthSamples;
    //         channelManager.channels[i].playbackSpeed = (int64_t)(kitManager.kits[0].samples[i].sampleRate * (1LL << 32) / 44100); // convert sample rate to Q32.32 format for playback speed
    //     }
    // }
    // numChannels = kitManager.kits[0].numSamples;

    // newer way to init kit
    kitManager.initKit(0);

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
                    channelManager.channels[0].samplePosition = 0;
                    channelManager.channels[0].samplePositionFP = 0;
                }
                if(thisTransportPosition % 48 == 24) {
                    channelManager.channels[1].samplePosition = 0;
                    channelManager.channels[1].samplePositionFP = 0;
                }
                if(thisTransportPosition % 6 == 0) {
                    channelManager.channels[2].samplePosition = 0;
                    channelManager.channels[2].samplePositionFP = 0;
                }
                for(uint i = 3; i < channelManager.numChannels; i++) {
                    if(thisTransportPosition % 4 == 0 && rand() % 4 == 0) {
                        channelManager.channels[i].samplePosition = 0;
                        channelManager.channels[i].samplePositionFP = 0;
                    }
                }
            }

            int16_t leftSample = 0;
            int16_t rightSample = 0;
            for(uint ch=0; ch<channelManager.numChannels; ch++) {
                Channel &channel = channelManager.channels[ch];
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
            //rightSample = channelManager.channels[3].sampleData[channelManager.channels[3].sampleLength - channelManager.channels[3].samplePosition - 1];

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
                if(buttons.update()) {
                    int16_t buttonIndex;
                    while((buttonIndex = buttons.getButtonPress()) != -1) {
                        menu.handleButtonPress(buttonIndex);
                    }
                }
            }
            // 4051 mux update will also go here
        }


        // check whether DAC needs data
        audio.update();

        transport.update();
    }
}