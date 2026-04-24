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
#include "hardware/AnalogInputs.h"
#include "hardware/Memory.h"
#include "hardware/CardReader.h"
#include "audio/Audio.h"
#include "audio/KitManager.h"
#include "audio/Channel.h"
#include "audio/ChannelManager.h"
#include "rhythm/Transport.h"
#include "rhythm/Beat.h"
#include "rhythm/Aleatory.h"
#include "interface/Menu.h"

CardReader cardReader;
Memory memory;
Leds leds;
Buttons buttons;
AnalogInputs analogInputs;
Audio audio;
Transport transport;
ChannelManager channelManager;
KitManager kitManager;
Menu menu;
Beat tempBeat;
Aleatory aleatory;

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
    
    buttons.init();
    leds.init();
    memory.init();
    analogInputs.init();
    channelManager.init();
    kitManager.init(&memory, &channelManager);
    cardReader.init(&memory);
    menu.init(&leds, &memory, &cardReader, &kitManager);
    audio.init();
    transport.init();
    tempBeat.init();
    aleatory.init(&analogInputs);

    // read settings from flash
    //transport.clockMode = memory.readSetting(SETTINGS_CLOCK_MODE);
    //transport.tempo = memory.readSetting(SETTINGS_TEMPO);
    //transport.timeSignature = memory.readSetting(SETTINGS_TIME_SIGNATURE);
    //transport.tuplet = memory.readSetting(SETTINGS_TUPLET);
    kitManager.initKit(memory.readSetting(SETTINGS_KIT_NUM));

    // interrupt for clock in pulse
    gpio_set_irq_enabled_with_callback(Pins::SYNC_IN, GPIO_IRQ_EDGE_FALL, true, pulseInCallback);

    int32_t lastTransportPositionFP = 0;
    uint64_t now;
    int longestTime = 0;
    int dacIntervalUs = audio.dacIntervalUs();
    while(true) {
        // generate audio in pre-buffer
        uint sampleCount = 0;
        now = time_us_64();
        while(audio.samplesRequired()) {
            uint32_t thisTransportPositionFP = transport.getPositionAtTimeFP(now+(sampleCount*22)); // Keep full Q16.16 precision
            uint32_t beatPositionFP = thisTransportPositionFP % (4 << 16); // 4 quarter notes per beat cycle

            // handle sample triggering from current beat and aleatoric algorithm
            if(thisTransportPositionFP != lastTransportPositionFP) {
                //float thisBeatPositionQ16 = beatPositionFP / 65536.0f;
                //printf("Position: %.4f\n", thisBeatPositionQ16);
                lastTransportPositionFP = thisTransportPositionFP;

                Beat::Hit hits[MAX_CHANNELS] = {0};

                while(tempBeat.hitAvailable(beatPositionFP)) {
                    Beat::Hit beatHit = tempBeat.getNextHit();
                    hits[beatHit.channel] = beatHit;
                }

                for(uint8_t ch=0; ch<kitManager.getNumChannels(); ch++) {
                    Beat::Hit aleatoryHit = aleatory.generateHit(ch, beatPositionFP);
                    if(aleatoryHit.velocity > hits[aleatoryHit.channel].velocity) {
                        hits[aleatoryHit.channel] = aleatoryHit;
                    }
                    if(hits[ch].velocity > 0) {
                        channelManager.triggerChannel(ch, hits[ch].velocity);
                    }
                }
                aleatory.finishGeneratingHits(beatPositionFP);
                    
            }

            // generate audio
            int16_t leftSample = 0;
            int16_t rightSample = 0;
            for(uint ch=0; ch<channelManager.numChannels; ch++) {
                Channel &channel = channelManager.channels[ch];

                if(channel.playbackSpeedFP > 0) {
                    // forward playback
                    if(channel.samplePosition < channel.sampleLength) {
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
                        leftSample += (int16_t)((int32_t)lerpedSample * channel.velocity >> 12);
                        channel.samplePositionFP += channel.playbackSpeedFP;
                        channel.samplePosition = channel.samplePositionFP >> 32;
                    }

                } else {
                    // reverse playback (doesn't sound 100% perfect, need to thoroughly check code)
                    if(channel.samplePosition > 0) {
                        int16_t lerpedSample = 0;
                        // simple linear interpolation
                        uint32_t indexInt = channel.samplePositionFP >> 32;
                        uint32_t indexFrac = channel.samplePositionFP & 0xFFFFFFFF;
                        if(indexInt > 0) {
                            int16_t sample1 = channel.sampleData[indexInt];
                            int16_t sample2 = channel.sampleData[indexInt - 1];
                            lerpedSample = sample1 + ((int64_t)(sample2 - sample1) * indexFrac >> 32);
                        } else {
                            lerpedSample = channel.sampleData[indexInt];
                        }
                        leftSample += (int16_t)((int32_t)lerpedSample * channel.velocity >> 12);
                        channel.samplePositionFP += channel.playbackSpeedFP;
                        channel.samplePosition = channel.samplePositionFP >> 32;
                    }
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
                        if(buttonIndex == BUTTON_POWER_OFF && time_us_64() > 1000000) {
                            // save settings to flash
                            printf("Power off, save settings...\n");
                            uint8_t settingsData[FLASH_PAGE_SIZE] = {0};
                            // *(uint32_t*)(&settingsData[SETTINGS_CLOCK_MODE]) = transport.clockMode;
                            // *(uint32_t*)(&settingsData[SETTINGS_TEMPO]) = transport.tempo;
                            // *(uint32_t*)(&settingsData[SETTINGS_TIME_SIGNATURE]) = transport.timeSignature;
                            // *(uint32_t*)(&settingsData[SETTINGS_TUPLET]) = transport.tuplet;
                            // *(uint32_t*)(&settingsData[SETTINGS_BEAT_NUM]) = transport.beatNum;
                            settingsData[SETTINGS_KIT_NUM] = kitManager.kitNum;
                            *(uint32_t*)(&settingsData[SETTINGS_CHECK_NUM]) = VALUE_SETTINGS_CHECK_NUM;
                            memory.writeToFlashPage(SECTOR_SETTINGS * (FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE), settingsData);
                            printf("Settings saved!\n");
                            uint64_t timeAfterSave = time_us_64();
                            while(1) {
                                printf("Time since power off: %llu ms\n", (time_us_64() - timeAfterSave) / 1000);
                                sleep_ms(1);
                            }
                        } else {
                            menu.handleButtonPress(buttonIndex);
                        }
                    }
                }
            }
            // 4051 mux update will also go here
            if(time_us_64() - analogInputs.lastUpdate() > 1000) {
                analogInputs.update();

                // update specific things if that input has changed
                uint8_t lastMuxChannel = analogInputs.getLastUpdatedMuxChannel();
                if(lastMuxChannel+8 == POT_PITCH) {
                    for(int ch=0; ch<kitManager.getNumChannels(); ch++) {
                        channelManager.channels[ch].playbackSpeedFP = (int64_t)(kitManager.kits[kitManager.kitNum].samples[ch].sampleRate * (1LL << 32) / 44100) * (analogInputs.getInputValue(POT_PITCH) * 8.0f / 4095.0f - 4.0f);
                    }
                }
            }
        }


        // check whether DAC needs data
        audio.update();

        transport.update();
    }
}