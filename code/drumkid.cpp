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
#include <array>
#include <algorithm>

// Include some stuff for reading/writing SD cards
#include "f_util.h"
#include "ff.h"
#include "rtc.h"
#include "hw_config.h"

// Include Pico-specific stuff and set up audio
#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"
#include "hardware/adc.h"
#include "hardware/flash.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"
// #include "pico/multicore.h"
#include "pico/audio_i2s.h"
#include "pico/binary_info.h"
bi_decl(bi_3pins_with_names(PICO_AUDIO_I2S_DATA_PIN, "I2S DIN", PICO_AUDIO_I2S_CLOCK_PIN_BASE, "I2S BCK", PICO_AUDIO_I2S_CLOCK_PIN_BASE + 1, "I2S LRCK"));

// Drumkid classes
#include "Sample.h"
#include "Beat.h"
#include "sevensegcharacters.h"
#include "drumkid.h"
#include "sn74595.pio.h"
#include "sn74165.pio.h"

// globals, organise later
int64_t currentTime = 0; // samples
uint16_t step = 0;
int numSteps = 4 * QUARTER_NOTE_STEPS;
uint64_t lastClockIn = 0; // microseconds
bool externalClock = false;

int activeButton = BOOTUP_VISUALS;
int activeSetting = 0;
int settingsMenuLevel = 0;

int ppqn = 32;
uint64_t prevPulseTime = 0; // samples
uint64_t nextPulseTime = 0; // samples
uint64_t nextPredictedPulseTime = 0; // samples
uint quarterNote = 0;
int64_t deltaT;                 // microseconds
int64_t lastDacUpdateSamples;   // samples
int64_t lastDacUpdateMicros;    // microseconds
bool beatStarted = false;
bool firstHit = true;

repeating_timer_t outputShiftRegistersTimer;
repeating_timer_t inputShiftRegistersTimer;
repeating_timer_t multiplexersTimer;

void handleSyncPulse() {
    pulseGpio(SYNC_OUT, 10); // todo: allow different output ppqn values
    deltaT = time_us_64() - lastClockIn; // real time since last pulse
    if (lastClockIn > 0)
    {
        int64_t currentTimeSamples = lastDacUpdateSamples + (44100 * (time_us_64() - lastDacUpdateMicros)) / 1000000;
        if(firstHit) {
            nextPulseTime = currentTimeSamples + SAMPLES_PER_BUFFER;
            nextPredictedPulseTime = nextPulseTime + (44100 * deltaT) / (1000000);
            prevPulseTime = nextPulseTime - (44100 * deltaT) / (1000000); // for completeness, but maybe not used?
            firstHit = false;
            beatStarted = true;
        } else {
            nextPulseTime = currentTimeSamples + SAMPLES_PER_BUFFER;
            nextPredictedPulseTime = nextPulseTime + (44100 * deltaT) / (1000000);
            prevPulseTime = nextPulseTime - (44100 * deltaT) / (1000000);
        }

    }
    lastClockIn = time_us_64();
}

void gpio_callback(uint gpio, uint32_t events)
{
    if(externalClock) {
        handleSyncPulse();
    }
}

uint16_t shiftRegOutBase = 0b0000111100000000;
int tempDigit = 0;

void setLed(uint8_t ledNum, bool value) {
    bitWrite(shiftRegOutBase, 15-ledNum, value);
}

bool updateOutputShiftRegisters(repeating_timer_t *rt)
{
    uint16_t b = shiftRegOutBase;
    bitWrite(b, 8+tempDigit, false); // set digit's common anode low to activate
    b += sevenSegData[tempDigit];
    sn74595::shiftreg_send(b);
    tempDigit = (tempDigit + 1) % 4;

    return true;
}

uint32_t shiftRegInValues = 0;
bool updateInputShiftRegisters(repeating_timer_t *rt)
{
    uint32_t data;
    bool tempInputChanged = false;
    data = sn74165::shiftreg_get(&tempInputChanged);
    if (tempInputChanged)
    {
        for(int i=0; i<32; i++) {
            bool newVal = bitRead(data, i);
            if(bitRead(shiftRegInValues, i) != newVal) {
                uint8_t buttonNum = (2-(i>>3))*8 + (i%8) + 1; // to match SW num in schematic (SW1 is top left on unit)
                handleButtonChange(buttonNum, newVal);
            }
        }
        
        //loop++;

        // if(data!=0) {
        //     for(int i=0; i<16; i++) {
        //         printf("%d\t", analogReadings[i]);
        //     }
        //     printf("\n");
        // }
        // if(data == 0x00010000) {
        //     beatStarted = !beatStarted;
        //     if(beatStarted) {
        //         step = 0;
        //         prevPulseTime = lastDacUpdateSamples + (44100 * (time_us_64() - lastDacUpdateMicros)) / 1000000;
        //         nextPulseTime = prevPulseTime + (44100 * deltaT) / (1000000);
        //         nextPredictedPulseTime = nextPulseTime;
        //     }
        // }

        // printf("\n%4d 0x%08X\t\t", loop, data);
        // printf("%08b %08b %08b %08b\n",
        //         (data >> 24) & 0xFF,
        //         (data >> 16) & 0xFF,
        //         (data >> 8) & 0xFF,
        //         data & 0xFF);
    }

    return true;
}

void handleButtonChange(int buttonNum, bool buttonState)
{
    if (buttonState)
    {
        switch (buttonNum)
        {
        case BUTTON_START_STOP:
            
            break;
        case BUTTON_MENU:
            activeButton = BUTTON_MENU;
            // settingsMenuLevel = 0;
            // displaySettings();
            break;
        case BUTTON_TAP_TEMPO:
            activeButton = BUTTON_MANUAL_TEMPO;
            // updateTapTempo();
            // scheduleSaveSettings();
            break;
        case BUTTON_LIVE_EDIT:
            activeButton = BUTTON_LIVE_EDIT;
            break;
        case BUTTON_INC:
            if (activeButton == BUTTON_LIVE_EDIT)
            {
                //doLiveHit(0);
            }
            else
            {
                //nextHoldUpdateInc = currentTime + SAMPLE_RATE;
                handleIncDec(true, false);
            }
            break;
        case BUTTON_DEC:
            if (activeButton == BUTTON_LIVE_EDIT)
            {
                //doLiveHit(1);
            }
            else
            {
                //nextHoldUpdateDec = currentTime + SAMPLE_RATE;
                handleIncDec(false, false);
            }
            break;
        case BUTTON_CONFIRM:
            if (activeButton == BUTTON_LIVE_EDIT)
            {
                //doLiveHit(2);
            }
            else
            {
                handleYesNo(true);
            }
            break;
        case BUTTON_CANCEL:
            if (activeButton == BUTTON_LIVE_EDIT)
            {
                //doLiveHit(3);
            }
            else
            {
                handleYesNo(false);
            }
            break;
        case BUTTON_BACK:
            activeButton = NO_ACTIVE_BUTTON;
            //settingsMenuLevel = 0;
            break;
        case BUTTON_KIT:
            activeButton = BUTTON_KIT;
            //sdShowFolderTemp = true;
            break;
        case BUTTON_MANUAL_TEMPO:
            activeButton = BUTTON_MANUAL_TEMPO;
            displayTempo();
            break;
        case BUTTON_TIME_SIGNATURE:
            activeButton = BUTTON_TIME_SIGNATURE;
            displayTimeSignature();
            break;
        case BUTTON_TUPLET:
            activeButton = BUTTON_TUPLET;
            displayTuplet();
            break;
        case BUTTON_BEAT:
            activeButton = BUTTON_BEAT;
            displayBeat();
            break;
        case BUTTON_CLEAR:
            if (activeButton == BUTTON_STEP_EDIT || activeButton == BUTTON_LIVE_EDIT)
            {
                // for (int i = 0; i < NUM_SAMPLES; i++)
                // {
                //     beats[beatNum].beatData[i] = 0;
                // }
            }
            break;
        case BUTTON_STEP_EDIT:
            activeButton = BUTTON_STEP_EDIT;
            displayEditBeat();
            break;
        case BUTTON_SAVE:
            saveBeatLocation = beatNum;
            activeButton = BUTTON_SAVE;
            updateLedDisplayNumber(saveBeatLocation);
            break;
        default:
            printf("(button %d not assigned)\n", buttonNum);
        }
    }
}

void handleIncDec(bool isInc, bool isHold)
{
    // have to declare stuff up here because of switch statement
    int prevBeatNum;

    switch (activeButton)
    {
    case BUTTON_MANUAL_TEMPO:
        if (!externalClock)
        {
            if (isHold)
            {
                tempo += isInc ? 10 : -10;
                tempo = 10 * ((tempo + 5) / 10);
            }
            else
            {
                tempo += isInc ? 1 : -1;
            }
            if (tempo > 9999)
                tempo = 9999;
            else if (tempo < 10)
                tempo = 10;
            stepTime = 2646000 / (tempo * QUARTER_NOTE_STEPS);
            displayTempo();
        }
        break;

    case BUTTON_TIME_SIGNATURE:
        // newNumSteps += isInc ? QUARTER_NOTE_STEPS : -QUARTER_NOTE_STEPS;
        // if (newNumSteps > 7 * QUARTER_NOTE_STEPS)
        //     newNumSteps = 7 * QUARTER_NOTE_STEPS;
        // else if (newNumSteps < QUARTER_NOTE_STEPS)
        //     newNumSteps = QUARTER_NOTE_STEPS;
        // if (!beatPlaying)
        //     numSteps = newNumSteps;
        displayTimeSignature();
        break;

    case BUTTON_BEAT:
        prevBeatNum = beatNum;
        beatNum += isInc ? 1 : -1;
        if (beatNum >= NUM_BEATS)
            beatNum = NUM_BEATS - 1;
        if (beatNum < 0)
            beatNum = 0;
        if (beatNum != prevBeatNum)
        {
            //revertBeat(prevBeatNum);
            //backupBeat(beatNum);6
        }
        displayBeat();
        break;

    case BUTTON_STEP_EDIT:
        editSample += isInc ? 1 : -1;
        if (editSample < 0)
            editSample = 0;
        if (editSample >= NUM_SAMPLES)
            editSample = NUM_SAMPLES - 1;
        displayEditBeat();
        break;

    case BUTTON_MENU:
        if (settingsMenuLevel == 0)
        {
            activeSetting += isInc ? 1 : -1;
            if (activeSetting < 0)
                activeSetting = 0;
            if (activeSetting >= NUM_MENU_SETTINGS)
                activeSetting = NUM_MENU_SETTINGS - 1;
        }
        else if (settingsMenuLevel == 1)
        {
            //handleSubSettingIncDec(isInc);
        }
        displaySettings();
        break;

    case BUTTON_SAVE:
        saveBeatLocation += isInc ? 1 : -1;
        if (saveBeatLocation < 0)
            saveBeatLocation = 0;
        if (saveBeatLocation >= NUM_BEATS)
            saveBeatLocation = NUM_BEATS - 1;
        updateLedDisplayNumber(saveBeatLocation);
        break;

    case BUTTON_TUPLET:
        // tuplet += isInc ? 1 : -1;
        // if (tuplet < 0)
        //     tuplet = 0;
        // else if (tuplet >= NUM_TUPLET_MODES)
        //     tuplet = NUM_TUPLET_MODES - 1;
        // nextQuarterNoteDivision = quarterNoteDivisionRef[tuplet];
        displayTuplet();
        break;

    case BUTTON_KIT:
        // sampleFolderNum += isInc ? 1 : -1;
        // if (sampleFolderNum < 0)
        //     sampleFolderNum = 0;
        // else if (sampleFolderNum > numSampleFolders - 1)
        //     sampleFolderNum = numSampleFolders - 1;
        // sdShowFolderTemp = true;
        break;
    }
    //scheduleSaveSettings();
}

void handleYesNo(bool isYes)
{
    // have to declare stuff up here because of switch statement
    int tupletEditStep;

    bool useDefaultNoBehaviour = true;
    switch (activeButton)
    {
    case BUTTON_KIT:
        if (isYes)
            //sdSafeLoadTemp = true;
        break;

    case BUTTON_STEP_EDIT:
        // useDefaultNoBehaviour = false;
        // tupletEditStep = (editStep / QUARTER_NOTE_STEPS_SEQUENCEABLE) * QUARTER_NOTE_STEPS_SEQUENCEABLE + Beat::tupletMap[tuplet][editStep % QUARTER_NOTE_STEPS_SEQUENCEABLE];
        // bitWrite(beats[beatNum].beatData[editSample], tupletEditStep, isYes);
        // editStep++;
        // if (editStep % QUARTER_NOTE_STEPS_SEQUENCEABLE >= quarterNoteDivision >> 2)
        // {
        //     editStep = (editStep / QUARTER_NOTE_STEPS_SEQUENCEABLE + 1) * QUARTER_NOTE_STEPS_SEQUENCEABLE;
        // }
        // if (editStep >= (newNumSteps / QUARTER_NOTE_STEPS) * QUARTER_NOTE_STEPS_SEQUENCEABLE)
        //     editStep = 0;
        displayEditBeat();
        break;

    case BUTTON_MENU:
        if (settingsMenuLevel == 0 && isYes)
            settingsMenuLevel = 1;
        else if (settingsMenuLevel == 1)
        {
            // handle new chosen option

            if (activeSetting == SETTING_OUTPUT_1 || activeSetting == SETTING_OUTPUT_2)
            {
                useDefaultNoBehaviour = false;
                if (activeSetting == SETTING_OUTPUT_1)
                {
                    //samples[outputSample].output1 = isYes;
                }
                else
                {
                    //samples[outputSample].output2 = isYes;
                }
                displayOutput(activeSetting == SETTING_OUTPUT_1 ? 1 : 2);
            }
            else
            {
                settingsMenuLevel = 0;
            }
        }
        displaySettings();
        break;

    case BUTTON_SAVE:
        // if (isYes)
        //     writeBeatsToFlash();
        activeButton = NO_ACTIVE_BUTTON;
        break;
    }
    if (!isYes && useDefaultNoBehaviour)
    {
        activeButton = NO_ACTIVE_BUTTON;
        bitWrite(singleLedData, 3, false); // clear error LED
    }
    //scheduleSaveSettings();
}

void displayClockMode()
{
    updateLedDisplayAlpha(externalClock ? "ext" : "int");
}

void displayTempo()
{
    if (externalClock)
    {
        int calculatedTempo = 2646000 / (stepTime * QUARTER_NOTE_STEPS);
        updateLedDisplayNumber(calculatedTempo);
    }
    else
    {
        updateLedDisplayNumber((int)tempo);
    }
}

void displayTimeSignature()
{
    char timeSigText[4];
    // timeSigText[0] = newNumSteps / QUARTER_NOTE_STEPS + 48;
    // timeSigText[1] = '-';
    // timeSigText[2] = '4';
    // timeSigText[3] = (newNumSteps != numSteps) ? '_' : ' ';
    updateLedDisplayAlpha(timeSigText);
}

void displayTuplet()
{
    // char tupletNames[NUM_TUPLET_MODES][5] = {
    //     "stra",
    //     "trip",
    //     "quin",
    //     "sept"};
    // updateLedDisplayAlpha(tupletNames[tuplet]);
}

void displayBeat()
{
    updateLedDisplayNumber(beatNum);
}

void displayEditBeat()
{
    int humanReadableEditStep = editStep + 1;
    char chars[4];
    chars[0] = editSample + 49;
    chars[1] = ' ';
    chars[2] = (humanReadableEditStep < 10) ? ' ' : humanReadableEditStep / 10 + 48;
    chars[3] = (humanReadableEditStep % 10) + 48;
    updateLedDisplayAlpha(chars);
}

void displayOutput(int outputNum)
{
    // for (int i = 0; i < 4 && i < NUM_SAMPLES; i++)
    // {
    //     if (outputNum == 1)
    //     {
    //         sevenSegData[i] = samples[i].output1 ? 0b10000000 : 0b00000000;
    //     }
    //     else if (outputNum == 2)
    //     {
    //         sevenSegData[i] = samples[i].output2 ? 0b10000000 : 0b00000000;
    //     }
    // }
    // bitWrite(sevenSegData[outputSample], 4, true);
}

void displaySettings()
{
    if (settingsMenuLevel == 0)
    {
        switch (activeSetting)
        {
        case SETTING_CLOCK_MODE:
            updateLedDisplayAlpha("cloc");
            break;

        case SETTING_OUTPUT_1:
            updateLedDisplayAlpha("out1");
            break;

        case SETTING_OUTPUT_2:
            updateLedDisplayAlpha("out2");
            break;

        case SETTING_GLITCH_CHANNEL:
            updateLedDisplayAlpha("glit");
            break;

        case SETTING_OUTPUT_PULSE_LENGTH:
            updateLedDisplayAlpha("plso");
            break;

        case SETTING_OUTPUT_PPQN:
            updateLedDisplayAlpha("pqno");
            break;

        case SETTING_INPUT_PPQN:
            updateLedDisplayAlpha("pqni");
            break;

        case SETTING_PITCH_CURVE:
            updateLedDisplayAlpha("ptch");
            break;

        case SETTING_INPUT_QUANTIZE:
            updateLedDisplayAlpha("qntz");
            break;
        }
    }
    else if (settingsMenuLevel == 1)
    {
        switch (activeSetting)
        {
        case SETTING_CLOCK_MODE:
            displayClockMode();
            break;

        case SETTING_OUTPUT_1:
            displayOutput(1);
            break;

        case SETTING_OUTPUT_2:
            displayOutput(2);
            break;

        case SETTING_GLITCH_CHANNEL:
            // if (glitchChannel == GLITCH_CHANNEL_BOTH)
            //     updateLedDisplayAlpha("both");
            // else if (glitchChannel == GLITCH_CHANNEL_1)
            //     updateLedDisplayAlpha("1");
            // else if (glitchChannel == GLITCH_CHANNEL_2)
            //     updateLedDisplayAlpha("2");
            // else if (glitchChannel == GLITCH_CHANNEL_NONE)
            //     updateLedDisplayAlpha("none");
            break;

        case SETTING_OUTPUT_PULSE_LENGTH:
            updateLedDisplayNumber(outputPulseLength);
            break;

        case SETTING_OUTPUT_PPQN:
            //updateLedDisplayNumber(syncOutPpqn);
            break;

        case SETTING_INPUT_PPQN:
            //updateLedDisplayNumber(syncInPpqn);
            break;

        case SETTING_PITCH_CURVE:
            if (pitchCurve == PITCH_CURVE_DEFAULT)
                updateLedDisplayAlpha("dflt");
            else if (pitchCurve == PITCH_CURVE_LINEAR)
                updateLedDisplayAlpha("linr");
            else if (pitchCurve == PITCH_CURVE_FORWARDS)
                updateLedDisplayAlpha("fwds");
            break;

        case SETTING_INPUT_QUANTIZE:
            updateLedDisplayNumber(inputQuantize);
            break;
        }
    }
}

// Borrowed from StackOverflow
char getNthDigit(int x, int n)
{
    while (n--)
    {
        x /= 10;
    }
    return x % 10;
}

void updateLedDisplayNumber(int num)
{
    int compare = 1;
    for (int i = 0; i < 4; i++)
    {
        if (i == 0 || num >= compare)
        {
            sevenSegData[3 - i] = sevenSegAsciiCharacters[getNthDigit(num, i) + 48];
        }
        else
        {
            sevenSegData[3 - i] = sevenSegAsciiCharacters[' '];
        }
        compare *= 10;
    }
}

void updateLedDisplayAlpha(const char *word)
{
    bool foundWordEnd = false;
    for (int i = 0; i < 4; i++)
    {
        bool skipChar = false;
        if (word[i] == '\0')
        {
            foundWordEnd = true;
        }
        if (foundWordEnd)
        {
            sevenSegData[i] = 0b00000000;
        }
        else
        {
            sevenSegData[i] = sevenSegAsciiCharacters[word[i]];
        }
    }
}

int tempMuxAddr = 0;
bool updateMultiplexers(repeating_timer_t *rt)
{
    adc_select_input(0);
    analogReadings[tempMuxAddr] = 4095 - adc_read();
    adc_select_input(1);
    analogReadings[tempMuxAddr + 8] = 4095 - adc_read();
    tempMuxAddr = (tempMuxAddr + 1) % 8;
    gpio_put(MUX_ADDR_A, bitRead(tempMuxAddr, 0));
    gpio_put(MUX_ADDR_B, bitRead(tempMuxAddr, 1));
    gpio_put(MUX_ADDR_C, bitRead(tempMuxAddr, 2));

    return true;
}

int main()
{
    stdio_init_all();
    time_init();

    printf("Drumkid V2\n");

    findCurrentFlashSettingsSector();
    initGpio();
    initSamplesFromFlash();
    //loadBeatsFromFlash();

    // temp beat setting
    beats[0].addHit(0, 0, 255, 255, 0);
    beats[0].addHit(1, 2*3360, 255, 255, 0);
    beats[0].addHit(1, 12*3360/4, 255, 127, 1);
    beats[0].addHit(1, 13*3360/4, 255, 127, 1);
    beats[0].addHit(1, 14*3360/4, 255, 127, 1);
    beats[0].addHit(1, 15*3360/4, 255, 127, 1);
    int numHatsTemp = 16;
    for(int i=0; i<numHatsTemp; i++) {
        beats[0].addHit(2, (i*4*3360)/numHatsTemp, 64, 255, 0);
    }

    // interrupt for clock in pulse
    gpio_set_irq_enabled_with_callback(SYNC_IN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    // init PIO programs
    sn74595::shiftreg_init();
    sn74165::shiftreg_init();

    // can tweak timing for any of these
    add_repeating_timer_ms(2, updateOutputShiftRegisters, NULL, &outputShiftRegistersTimer);
    add_repeating_timer_ms(1, updateInputShiftRegisters, NULL, &inputShiftRegistersTimer); // could use an interrupt
    add_repeating_timer_us(250, updateMultiplexers, NULL, &multiplexersTimer); // could use PIO maybe, potentially faster

    struct audio_buffer_pool *ap = init_audio();

    beatPlaying = true;

    if (!externalClock)
    {
        ppqn = 1;
        deltaT = 500000;
        nextPulseTime = (44100 * deltaT) / (1000000);
        nextPredictedPulseTime = nextPulseTime;
        beatStarted = true;
    }

    // audio buffer loop, runs forever
    int lastStep = -1;
    while (true)
    {
        struct audio_buffer *buffer = take_audio_buffer(ap, true);
        int16_t *bufferSamples = (int16_t *)buffer->buffer->bytes;

        // update audio output
        for (uint i = 0; i < buffer->max_sample_count * 2; i += 2)
        {
            // sample updates go here
            int32_t out1 = 0;
            int32_t out2 = 0;

            if(beatStarted && currentTime >= nextPulseTime) {
                prevPulseTime = nextPulseTime;
                nextPulseTime += (44100*deltaT)/(1000000);
                if(!externalClock) {
                    nextPredictedPulseTime = nextPulseTime;
                }
                step = (step + (3360 / ppqn)) % (4 * 3360);
            }

            int64_t microstep = (3360 * (currentTime - prevPulseTime)) / (ppqn * (nextPulseTime - prevPulseTime));
            bool checkBeat = false; // only check beat when microstep has been incremented

            if(beatStarted && currentTime < nextPredictedPulseTime && microstep != lastStep) {
                checkBeat = true;
            }
            lastStep = microstep; // this maybe needs to be reset to -1 or something when beat stops?

            int tempChance = analogReadings[POT_CHANCE];

            for (int j = 0; j < NUM_SAMPLES; j++)
            {
                if(checkBeat) {
                    int foundHit = beats[0].getHit(j, microstep + step);
                    if(foundHit >= 0) {
                        int prob = beats[0].hits[foundHit].probability;
                        int randNum = rand() % 255;
                        if(prob>randNum) {
                            samples[j].queueHit(currentTime, 0, 4095);
                        }
                    } else {
                        int randNum = rand() % 4095;
                        if (((microstep + step) % 1680) == 0 && tempChance > randNum)
                        {
                            samples[j].queueHit(currentTime, 0, 4095);
                        }
                    }
                    // if((microstep + step) % 105 == 0) {
                    //     samples[j].queueHit(currentTime, 0, 4095);
                    // }
                }
                samples[j].update(currentTime);
                out1 += samples[j].value;
            }
            bufferSamples[i] = out1 >> 2;
            bufferSamples[i + 1] = 0;

            if(checkBeat) {
                if(microstep == 0) {
                    //step = (step + (3360/ppqn)) % (4*3360);
                }
            }

            currentTime++;
        }
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);
        lastDacUpdateSamples = currentTime;
        lastDacUpdateMicros = time_us_64();
    }
    return 0;
}

#define FLASH_SETTINGS_START 0
#define FLASH_SETTINGS_END 63
int currentSettingsSector = 0;
void findCurrentFlashSettingsSector()
{
    bool foundValidSector = false;
    int mostRecentValidSector = 0;
    int highestWriteNum = 0;
    for (int i = FLASH_SETTINGS_START; i <= FLASH_SETTINGS_END; i++)
    {
        int testInt = getIntFromBuffer(flashData + i * FLASH_SECTOR_SIZE, DATA_CHECK);
        if (testInt == CHECK_NUM)
        {
            foundValidSector = true;
            int writeNum = getIntFromBuffer(flashData + i * FLASH_SECTOR_SIZE, 4);
            if (writeNum >= highestWriteNum)
            {
                highestWriteNum = writeNum;
                mostRecentValidSector = i;
            }
        }
    }
    if (!foundValidSector)
    {
        // no valid sectors, which means the flash needs to be initialised:
        /*uint8_t buffer[FLASH_PAGE_SIZE];
        int32_t refCheckNum = CHECK_NUM;
        std::memcpy(buffer, &refCheckNum, 4); // copy check number
        int32_t refZero = 0;
        std::memcpy(buffer + 4, &refZero, 4); // copy number zero, the incremental write number for wear levelling
        uint32_t dummySampleLength = 1024;
        for (int i = 0; i < NUM_SAMPLES; i++)
        {
            uint32_t dummySampleStart = i * dummySampleLength * 2;
            std::memcpy(buffer + SAMPLE_START_POINTS + 4 * i, &dummySampleStart, 4);
            std::memcpy(buffer + SAMPLE_LENGTHS + 4 * i, &dummySampleLength, 4);
        }
        writePageToFlash(buffer, FLASH_DATA_ADDRESS + FLASH_SETTINGS_START * FLASH_SECTOR_SIZE);

        // fill audio buffer with random noise
        for (int i = 0; i < dummySampleLength * 2 * NUM_SAMPLES; i += FLASH_PAGE_SIZE)
        {
            for (int j = 0; j < FLASH_PAGE_SIZE; j++)
            {
                buffer[j] = rand() % 256; // random byte
            }
            writePageToFlash(buffer, FLASH_AUDIO_ADDRESS + i);
        }

        saveSettings();*/
    }
    else
    {
        currentSettingsSector = mostRecentValidSector;
    }
}

void initSamplesFromFlash()
{
    int startPos = 0;
    int storageOverflow = false;
    for (int n = 0; n < NUM_SAMPLES; n++)
    {
        uint sampleStart = getIntFromBuffer(flashAudioMetadata, SAMPLE_START_POINTS + n * 4);
        uint sampleLength = getIntFromBuffer(flashAudioMetadata, SAMPLE_LENGTHS + n * 4);
        printf("start %d, length %d\n", sampleStart, sampleLength);

        samples[n].length = sampleLength / 2; // divide by 2 to go from 8-bit to 16-bit
        samples[n].startPosition = sampleStart / 2;
        if (samples[n].startPosition + samples[n].length >= MAX_SAMPLE_STORAGE)
        {
            samples[n].startPosition = 0;
            samples[n].length = 1000;
            storageOverflow = true;
        }
        printf("sample %d, start %d, length %d\n", n, samples[n].startPosition, samples[n].length);
        samples[n].position = samples[n].length;
        //samples[n].positionAccurate = samples[n].length << Sample::LERP_BITS;

        if (!storageOverflow)
        {
            for (int i = 0; i < samples[n].length && i < sampleLength / 2; i++)
            {
                int pos = i * 2 + sampleStart;
                int16_t thisSample = flashAudio[pos + 1] << 8 | flashAudio[pos];
                Sample::sampleData[sampleStart / 2 + i] = thisSample;
            }
        }
    }

    if (storageOverflow)
    {
        //showError("size");
    }
}

void loadBeatsFromFlash()
{
    int startPos = 0;
    for (int i = 0; i < NUM_BEATS; i++)
    {
        for (int j = 0; j < NUM_SAMPLES; j++)
        {
            //std::memcpy(&beats[i].beatData[j], &flashUserBeats[8 * (i * NUM_SAMPLES + j)], 8);
        }
    }
    backupBeat(beatNum);
}

void backupBeat(int backupBeatNum)
{
    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        //beatBackup.beatData[i] = beats[backupBeatNum].beatData[i];
    }
}

void initGpio()
{
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
    gpio_init(SYNC_OUT);
    gpio_set_dir(SYNC_OUT, GPIO_OUT);
    gpio_init(SYNC_IN);
    gpio_set_dir(SYNC_IN, GPIO_IN);
    for (int i = 0; i < 4; i++)
    {
        gpio_init(TRIGGER_OUT_PINS[i]);
        gpio_set_dir(TRIGGER_OUT_PINS[i], GPIO_OUT);
    };

    // set GPIO23 high, apparently improves ADC readings
    gpio_init(23);
    gpio_set_dir(23, GPIO_OUT);
    gpio_put(23, 1);
}

// Borrowed/adapted from pico-playground
struct audio_buffer_pool *init_audio()
{

    static audio_format_t audio_format = {
        SAMPLE_RATE,
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

int32_t getIntFromBuffer(const uint8_t *buffer, uint position)
{
    // potentially a silly way of doing this, written before I knew about std::memcpy? but it works
    int32_t thisInt = buffer[position] | buffer[position + 1] << 8 | buffer[position + 2] << 16 | buffer[position + 3] << 24;
    return thisInt;
}

int64_t onGpioPulseTimeout(alarm_id_t id, void *user_data)
{
    uint gpioNum = (uint)user_data;
    gpio_put(gpioNum, 0);
    return 0;
}

void pulseGpio(uint gpioNum, uint16_t pulseLengthMillis)
{
    gpio_put(gpioNum, 1);
    add_alarm_in_ms(pulseLengthMillis, onGpioPulseTimeout, (void *)gpioNum, true);
}