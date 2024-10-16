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
uint16_t pulseStep = 0;
int numSteps = 4 * 3360;
int newNumSteps = numSteps; // newNumSteps store numSteps value to be set at start of next bar
uint64_t lastClockIn = 0; // microseconds
bool externalClock = false;
bool prevProcessTimeSlow = false;
uint8_t holdDelayInc = 0;
uint8_t holdDelayDec = 0;
int lastStep = -1;
uint16_t tupletEditStepMultipliers[NUM_TUPLET_MODES] = {420,560,672,480};
uint16_t tupletEditLiveMultipliers[NUM_TUPLET_MODES] = {840,560,672,480};
int metronomeIndex = 0;
int nextBeatCheckStep[NUM_SAMPLES] = {0};
bool forceBeatUpdate = false;

int activeButton = BOOTUP_VISUALS;
int activeSetting = 0;
int settingsMenuLevel = 0;

const int NUM_PPQN_VALUES = 8;
int ppqnValues[NUM_PPQN_VALUES] = {1, 2, 4, 8, 12, 16, 24, 32};
int syncInPpqnIndex = 0;
int syncOutPpqnIndex = 2;
int syncInPpqn = ppqnValues[syncInPpqnIndex];
int syncOutPpqn = ppqnValues[syncOutPpqnIndex];
uint64_t prevPulseTime = 0; // samples
uint64_t nextPulseTime = 0; // samples
uint64_t nextPredictedPulseTime = 0; // samples
uint quarterNote = 0;
int64_t deltaT;                 // microseconds
int64_t lastDacUpdateSamples;   // samples
int64_t lastDacUpdateMicros;    // microseconds
bool beatStarted = false;
bool firstHit = true;
int tuplet = TUPLET_STRAIGHT;

uint8_t crushVolume[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 13, 13};
int glitchChannel = GLITCH_CHANNEL_BOTH;
bool glitch1 = true;
bool glitch2 = true;

bool sdSafeLoadTemp = false;
bool sdShowFolderTemp = false;
int sampleFolderNum = 0;

repeating_timer_t outputShiftRegistersTimer;
repeating_timer_t inputShiftRegistersTimer;
repeating_timer_t multiplexersTimer;
repeating_timer_t displayTimer;
repeating_timer_t holdTimer;

void handleSyncPulse() {
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
    if(activeButton == BUTTON_MANUAL_TEMPO) {
        displayTempo();
    }
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

int64_t ledPulseLowCallback(alarm_id_t id, void *user_data)
{
    uint ledNum = (uint)user_data;
    setLed(ledNum, false);
    return 0;
}

int64_t ledPulseHighCallback(alarm_id_t id, void *user_data)
{
    uint ledNum = (uint)user_data;
    setLed(ledNum, true);
    add_alarm_in_ms(20, ledPulseLowCallback, (void *)ledNum, true);
    return 0;
}

void pulseLed(uint ledNum, uint16_t delayMicros)
{
    add_alarm_in_us(delayMicros, ledPulseHighCallback, (void *)ledNum, true);
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

bool updateInputShiftRegisters(repeating_timer_t *rt)
{
    uint32_t data;
    bool tempInputChanged = false;
    data = sn74165::shiftreg_get(&tempInputChanged);
    if (tempInputChanged)
    {
        for(int i=0; i<32; i++) {
            uint8_t buttonNum = (2-(i>>3))*8 + (i%8) + 1; // to match SW num in schematic (SW1 is top left on unit) - could be lookup table to improve speed
            bool newVal = bitRead(data, i);
            if(bitRead(buttonStableStates, buttonNum) != newVal && cyclesSinceChange[i] >= 20) {
                handleButtonChange(buttonNum, newVal);
                cyclesSinceChange[i] = 0;
                bitWrite(buttonStableStates, buttonNum, newVal);
            } else if(cyclesSinceChange[i]<20) {
                cyclesSinceChange[i] ++;
            }
        }  
    } else {
        for (int i = 0; i < 32; i++)
        {
            if (cyclesSinceChange[i] < 20)
                cyclesSinceChange[i]++;
        }
    }

    return true;
}

void doLiveHit(int sampleNum)
{
    int quantizeSteps = tupletEditLiveMultipliers[tuplet];
    int64_t samplesSincePulse = (44100 * (time_us_64() - lastDacUpdateMicros)) / 1000000 + lastDacUpdateSamples - prevPulseTime - SAMPLES_PER_BUFFER;
    int samplesPerPulse = nextPulseTime - prevPulseTime;
    int thisStep = (pulseStep + (samplesSincePulse * 3360) / samplesPerPulse) % numSteps;
    thisStep = ((thisStep + quantizeSteps/2) % numSteps) / quantizeSteps;
    thisStep = thisStep * quantizeSteps;
    beats[beatNum].addHit(sampleNum, thisStep, analogReadings[POT_VELOCITY]>>4, 255, 0);

    // if scheduled hit is in the past, directly trigger sample now
    int compareLastStep = lastStep;
    int compareThisStep = thisStep;
    if(lastStep - SAMPLES_PER_BUFFER < 0) {
        compareLastStep += numSteps;
    }
    if(thisStep - SAMPLES_PER_BUFFER < 0) {
        compareThisStep += numSteps;
    }
    if (compareThisStep <= compareLastStep)
    {
        samples[sampleNum].queueHit(lastDacUpdateSamples, 0, analogReadings[POT_VELOCITY] >> 4);
    }

    forceBeatUpdate = true;
}

void handleButtonChange(int buttonNum, bool buttonState)
{
    int i;
    if (buttonState)
    {
        switch (buttonNum)
        {
        case BUTTON_START_STOP:
            numSteps = newNumSteps;
            if (activeButton == BUTTON_TIME_SIGNATURE)
                displayTimeSignature();
            beatPlaying = !beatPlaying;
            if (beatPlaying)
            {
                // handle beat start
                if (!externalClock)
                {
                    pulseStep = 0;
                    prevPulseTime = lastDacUpdateSamples + SAMPLES_PER_BUFFER; // ?
                    nextPulseTime = prevPulseTime + (44100 * deltaT) / (1000000);
                    nextPredictedPulseTime = nextPulseTime;
                }
            }
            else
            {
                // handle beat stop
                if (activeButton == BUTTON_TIME_SIGNATURE)
                    displayTimeSignature();
                pulseStep = 0;

                //scheduleSaveSettings();
            }
            break;
        case BUTTON_MENU:
            activeButton = BUTTON_MENU;
            settingsMenuLevel = 0;
            displaySettings();
            break;
        case BUTTON_TAP_TEMPO:
            activeButton = BUTTON_MANUAL_TEMPO;
            updateTapTempo();
            // scheduleSaveSettings();
            break;
        case BUTTON_LIVE_EDIT:
            activeButton = BUTTON_LIVE_EDIT;
            break;
        case BUTTON_INC:
            if (activeButton == BUTTON_LIVE_EDIT)
            {
                doLiveHit(0);
            }
            else
            {
                //nextHoldUpdateInc = currentTime + SAMPLE_RATE;
                holdDelayInc = 8;
                handleIncDec(true, false);
            }
            break;
        case BUTTON_DEC:
            if (activeButton == BUTTON_LIVE_EDIT)
            {
                doLiveHit(1);
            }
            else
            {
                //nextHoldUpdateDec = currentTime + SAMPLE_RATE;
                holdDelayDec = 8;
                handleIncDec(false, false);
            }
            break;
        case BUTTON_CONFIRM:
            if (activeButton == BUTTON_LIVE_EDIT)
            {
                doLiveHit(2);
            }
            else
            {
                handleYesNo(true);
            }
            break;
        case BUTTON_CANCEL:
            if (activeButton == BUTTON_LIVE_EDIT)
            {
                doLiveHit(3);
            }
            else
            {
                handleYesNo(false);
            }
            break;
        case BUTTON_BACK:
            activeButton = NO_ACTIVE_BUTTON;
            displayPulse(lastStep/3360,0);
            //settingsMenuLevel = 0;
            break;
        case BUTTON_KIT:
            activeButton = BUTTON_KIT;
            sdShowFolderTemp = true;
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
                for(i=0; i<MAX_BEAT_HITS; i++) {
                    beats[beatNum].hits[i].sample = 0;
                    beats[beatNum].hits[i].step = 0;
                    beats[beatNum].hits[i].velocity = 0;
                    beats[beatNum].hits[i].probability = 0;
                    beats[beatNum].hits[i].group = 0;
                }
                beats[beatNum].numHits = 0;
            }
            break;
        case BUTTON_STEP_EDIT:
            if(activeButton == BUTTON_STEP_EDIT) {
                editSample = (editSample + 1) % 4;
            }
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

void handleSubSettingIncDec(bool isInc)
{
    // could probably do this with pointers to make it neater but it's hot today so that's not happening
    int thisInc = isInc ? 1 : -1;
    switch (activeSetting)
    {
    case SETTING_CLOCK_MODE:
        externalClock = !externalClock;
        displayClockMode();
        break;

    case SETTING_OUTPUT_1:
    case SETTING_OUTPUT_2:
        // outputSample += isInc ? 1 : -1;
        // if (outputSample < 0)
        //     outputSample = 0;
        // else if (outputSample > NUM_SAMPLES - 1)
        //     outputSample = NUM_SAMPLES - 1;
        // displayOutput(activeSetting == SETTING_OUTPUT_1 ? 1 : 2);
        break;

    case SETTING_GLITCH_CHANNEL:
        glitchChannel += thisInc;
        glitchChannel = std::max(0, std::min(3, glitchChannel));
        glitch1 = !bitRead(glitchChannel, 1);
        glitch2 = !bitRead(glitchChannel, 0);
        break;

    case SETTING_OUTPUT_PULSE_LENGTH:
        outputPulseLength += thisInc;
        outputPulseLength = std::max(10, std::min(200, outputPulseLength));
        break;

    case SETTING_OUTPUT_PPQN:
        syncOutPpqnIndex += thisInc;
        syncOutPpqnIndex = std::max(0, std::min(NUM_PPQN_VALUES - 1, syncOutPpqnIndex));
        syncOutPpqn = ppqnValues[syncOutPpqnIndex];
        break;

    case SETTING_INPUT_PPQN:
        syncInPpqnIndex += thisInc;
        syncInPpqnIndex = std::max(0, std::min(NUM_PPQN_VALUES - 1, syncInPpqnIndex));
        syncInPpqn = ppqnValues[syncInPpqnIndex];
        pulseStep = (pulseStep / (3360 / syncInPpqn)) * (3360 / syncInPpqn); // quantize pulse step to new PPQN value to prevent annoying phase offset thing
        break;

    case SETTING_PITCH_CURVE:
        pitchCurve += thisInc;
        pitchCurve = std::max(0, std::min(PITCH_CURVE_FORWARDS, pitchCurve));
        break;

    case SETTING_INPUT_QUANTIZE:
        inputQuantizeIndex += thisInc;
        inputQuantizeIndex = std::max(0, std::min(NUM_QUANTIZE_VALUES - 1, inputQuantizeIndex));
        inputQuantize = quantizeValues[inputQuantizeIndex];
        break;
    }
}

void backupBeat(int backupBeatNum)
{
    for (int i = 0; i < MAX_BEAT_HITS; i++)
    {
        // would be neater with memcpy
        beatBackup.hits[i].sample = beats[backupBeatNum].hits[i].sample;
        beatBackup.hits[i].step = beats[backupBeatNum].hits[i].step;
        beatBackup.hits[i].velocity = beats[backupBeatNum].hits[i].velocity;
        beatBackup.hits[i].probability = beats[backupBeatNum].hits[i].probability;
        beatBackup.hits[i].group = beats[backupBeatNum].hits[i].group;
    }
    beatBackup.numHits = beats[backupBeatNum].numHits;
}

void revertBeat(int revertBeatNum)
{
    for (int i = 0; i < MAX_BEAT_HITS; i++)
    {
        // would be neater with memcpy
        beats[revertBeatNum].hits[i].sample = beatBackup.hits[i].sample;
        beats[revertBeatNum].hits[i].step = beatBackup.hits[i].step;
        beats[revertBeatNum].hits[i].velocity = beatBackup.hits[i].velocity;
        beats[revertBeatNum].hits[i].probability = beatBackup.hits[i].probability;
        beats[revertBeatNum].hits[i].group = beatBackup.hits[i].group;
    }
    beats[revertBeatNum].numHits = beatBackup.numHits;
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
            deltaT = 60000000 / tempo;
            displayTempo();
        }
        break;

    case BUTTON_TIME_SIGNATURE:
        newNumSteps += isInc ? QUARTER_NOTE_STEPS : -QUARTER_NOTE_STEPS;
        if (newNumSteps > 7 * QUARTER_NOTE_STEPS)
            newNumSteps = 7 * QUARTER_NOTE_STEPS;
        else if (newNumSteps < QUARTER_NOTE_STEPS)
            newNumSteps = QUARTER_NOTE_STEPS;
        if (!beatPlaying)
            numSteps = newNumSteps;
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
            revertBeat(prevBeatNum);
            backupBeat(beatNum);
        }
        forceBeatUpdate = true;
        displayBeat();
        break;

    case BUTTON_STEP_EDIT:
        // editSample += isInc ? 1 : -1;
        // if (editSample < 0)
        //     editSample = 0;
        // if (editSample >= NUM_SAMPLES)
        //     editSample = NUM_SAMPLES - 1;
        editStep += isInc ? 1: -1;
        if(editStep < 0) editStep = numSteps / tupletEditStepMultipliers[tuplet] - 1;
        if (editStep >= numSteps / tupletEditStepMultipliers[tuplet])
            editStep = 0;
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
            handleSubSettingIncDec(isInc);
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
        tuplet += isInc ? 1 : -1;
        if (tuplet < 0)
            tuplet = 0;
        else if (tuplet >= NUM_TUPLET_MODES)
            tuplet = NUM_TUPLET_MODES - 1;
        else {
            // tuplet changed, reset edit step
            editStep = 0;
        }
        displayTuplet();
        break;

    case BUTTON_KIT:
        sampleFolderNum += isInc ? 1 : -1;
        if (sampleFolderNum < 0)
            sampleFolderNum = 0;
        else if (sampleFolderNum > numSampleFolders - 1)
            sampleFolderNum = numSampleFolders - 1;
        sdShowFolderTemp = true;
        break;
    }
    //scheduleSaveSettings();
}

void handleYesNo(bool isYes)
{
    // have to declare stuff up here because of switch statement
    int hitNum;

    bool useDefaultNoBehaviour = true;
    switch (activeButton)
    {
    case BUTTON_KIT:
        if (isYes)
            sdSafeLoadTemp = true;
        break;

    case BUTTON_STEP_EDIT:
        useDefaultNoBehaviour = false;

        // tupletEditStep = (editStep / QUARTER_NOTE_STEPS_SEQUENCEABLE) * QUARTER_NOTE_STEPS_SEQUENCEABLE + Beat::tupletMap[tuplet][editStep % QUARTER_NOTE_STEPS_SEQUENCEABLE];
        // bitWrite(beats[beatNum].beatData[editSample], tupletEditStep, isYes);
        // editStep++;
        // if (editStep % QUARTER_NOTE_STEPS_SEQUENCEABLE >= quarterNoteDivision >> 2)
        // {
        //     editStep = (editStep / QUARTER_NOTE_STEPS_SEQUENCEABLE + 1) * QUARTER_NOTE_STEPS_SEQUENCEABLE;
        // }
        // if (editStep >= (newNumSteps / QUARTER_NOTE_STEPS) * QUARTER_NOTE_STEPS_SEQUENCEABLE)
        //     editStep = 0;
        hitNum = beats[beatNum].getHit(editSample, editStep*tupletEditStepMultipliers[tuplet]);
        if (hitNum >= 0)
        {
            // remove existing hit, whether or not we want to add a new one
            beats[beatNum].removeHit(editSample, editStep * tupletEditStepMultipliers[tuplet]);
        }
        if(isYes) {
            beats[beatNum].addHit(editSample, editStep * tupletEditStepMultipliers[tuplet], 255, 255, 0);
        }
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
        if (isYes)
            saveAllBeats();
        activeButton = NO_ACTIVE_BUTTON;
        break;
    }
    if (!isYes && useDefaultNoBehaviour)
    {
        activeButton = NO_ACTIVE_BUTTON;
        //bitWrite(singleLedData, 3, false); // clear error LED
        setLed(3, false);
    }
    //scheduleSaveSettings();
}

uint64_t lastTapTempoTime;
void updateTapTempo()
{
    if (!externalClock)
    {
        //uint64_t deltaT = time_us_64() - lastTapTempoTime;
        uint64_t newDeltaT = time_us_64() - lastTapTempoTime;
        lastTapTempoTime = time_us_64();
        if (newDeltaT < 5000000)
        {
            deltaT = newDeltaT;
            //stepTime = (44100 * deltaT) / 32000000;
            //tempo = 2646000 / (stepTime * QUARTER_NOTE_STEPS); // temp...
            tempo = 60000000 / deltaT;
            displayTempo();
        }
    }
}

int64_t displayPulseCallback(alarm_id_t id, void *user_data)
{
    uint thisPulse = (uint)user_data;
    for(int i=0; i<4; i++) {
        if (i == thisPulse % 4)
        {
            sevenSegData[i] = thisPulse < 4 ? 0b00100000 : 0b00010000;
        }
        else
        {
            sevenSegData[i] = 0;
        }
    }
    return 0;
}

void displayPulse(int thisPulse, uint16_t delayMicros)
{
    add_alarm_in_us(delayMicros, displayPulseCallback, (void *)thisPulse, true);
}

void displayClockMode()
{
    updateLedDisplayAlpha(externalClock ? "ext" : "int");
}

void displayTempo()
{
    if (externalClock)
    {
        int calculatedTempo = 60000000 / (deltaT * syncInPpqn);
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
    timeSigText[0] = newNumSteps / QUARTER_NOTE_STEPS + 48; // 48 is digit zero in ascii
    timeSigText[1] = '-';
    timeSigText[2] = '4';
    timeSigText[3] = (newNumSteps != numSteps) ? '_' : ' ';
    updateLedDisplayAlpha(timeSigText);
}

void displayTuplet()
{
    char tupletNames[NUM_TUPLET_MODES][5] = {
        "stra",
        "trip",
        "quin",
        "sept"};
    updateLedDisplayAlpha(tupletNames[tuplet]);
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
    chars[1] = humanReadableEditStep / 10 + 48;
    chars[2] = (humanReadableEditStep % 10) + 48;
    chars[3] = beats[beatNum].getHit(editSample, editStep * tupletEditStepMultipliers[tuplet]) >= 0 ? '_' : ' ';
    updateLedDisplayAlpha(chars);
    bitWrite(sevenSegData[0], 7, true);
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
            if (glitchChannel == GLITCH_CHANNEL_BOTH)
                updateLedDisplayAlpha("both");
            else if (glitchChannel == GLITCH_CHANNEL_1)
                updateLedDisplayAlpha("1");
            else if (glitchChannel == GLITCH_CHANNEL_2)
                updateLedDisplayAlpha("2");
            else if (glitchChannel == GLITCH_CHANNEL_NONE)
                updateLedDisplayAlpha("none");
            break;

        case SETTING_OUTPUT_PULSE_LENGTH:
            updateLedDisplayNumber(outputPulseLength);
            break;

        case SETTING_OUTPUT_PPQN:
            updateLedDisplayNumber(syncOutPpqn);
            break;

        case SETTING_INPUT_PPQN:
            updateLedDisplayNumber(syncInPpqn);
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

int zoomValues[NUM_TUPLET_MODES][9] = {
    {-1, 13440, 6720, 3360, 1680, 840, 420, 210, 105},
    {-1, 13440, 13440, 3360, 1120, 560, 280, 140, 70},
    {-1, 13440, 13440, 3360, 672, 336, 168, 84, 42},
    {-1, 13440, 13440, 3360, 480, 240, 120, 60, 30},
};

int getRandomHitVelocity(int step) {
    // first draft...

    int tempZoom = analogReadings[POT_ZOOM];
    applyDeadZones(tempZoom);
    tempZoom = tempZoom  / 456;
    if (zoomValues[tuplet][tempZoom] >= 0 && (step % zoomValues[tuplet][tempZoom]) == 0)
    {
        int tempChance = analogReadings[POT_CHANCE];
        applyDeadZones(tempChance);
        if(tempChance > rand()%4095) {
            int returnVel = analogReadings[POT_VELOCITY] + ((analogReadings[POT_RANGE] * (rand() % 4095)) >> 12) - (analogReadings[POT_RANGE]>>1);
            if(returnVel < 0) returnVel = 0;
            else if(returnVel > 4095) returnVel = 4095;
            return returnVel >> 4;
        }
    }
    return 0;
}

bool updateLedDisplay(repeating_timer_t *rt)
{
    sevenSegData[0] = rand() % 256;
    sevenSegData[1] = rand() % 256;
    sevenSegData[2] = rand() % 256;
    sevenSegData[3] = rand() % 256;

    if (time_us_32() > 1000000)
    {
        activeButton = NO_ACTIVE_BUTTON;
        displayPulse(0, 0);
        return false;
    }
    else
    {
        return true;
    }
}

bool updateHold(repeating_timer_t *rt) {
    if(holdDelayInc > 0) {
        holdDelayInc --;
    } else if(bitRead(buttonStableStates, BUTTON_INC)) {
        handleIncDec(true, true);
    }
    if(holdDelayDec > 0) {
        holdDelayDec --;
    } else if(bitRead(buttonStableStates, BUTTON_DEC)) {
        handleIncDec(false, true);
    }
    return true;
}

void applyDeadZones(int &param)
{
    param = (4095 * (std::max(ANALOG_DEAD_ZONE_LOWER, std::min(ANALOG_DEAD_ZONE_UPPER, param)) - ANALOG_DEAD_ZONE_LOWER)) / ANALOG_EFFECTIVE_RANGE;
}

int main()
{
    stdio_init_all();
    time_init();

    printf("Drumkid V2\n");

    // temp beat setting
    // beats[0].addHit(0, 0, 255, 255, 0);
    // beats[0].addHit(1, 2*3360, 255, 255, 0);
    // beats[0].addHit(1, 12*3360/4, 255, 127, 1);
    // beats[0].addHit(1, 13*3360/4, 255, 127, 1);
    // beats[0].addHit(1, 14*3360/4, 255, 127, 1);
    // beats[0].addHit(1, 15*3360/4, 255, 127, 1);
    // int numHatsTemp = 16;
    // for(int i=0; i<numHatsTemp; i++) {
    //     beats[0].addHit(2, (i*4*3360)/numHatsTemp, 64, 255, 0);
    // }

    findCurrentFlashSettingsSector();
    initGpio();
    initSamplesFromFlash();
    loadBeatsFromFlash();

    beats[0].addHit(3, 0, 255, 255, 0);

    // interrupt for clock in pulse
    gpio_set_irq_enabled_with_callback(SYNC_IN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    // init PIO programs
    sn74595::shiftreg_init();
    sn74165::shiftreg_init();

    // can tweak timing for any of these
    add_repeating_timer_ms(2, updateOutputShiftRegisters, NULL, &outputShiftRegistersTimer);
    add_repeating_timer_ms(1, updateInputShiftRegisters, NULL, &inputShiftRegistersTimer); // could use an interrupt
    add_repeating_timer_us(250, updateMultiplexers, NULL, &multiplexersTimer); // could use PIO maybe, potentially faster
    add_repeating_timer_ms(60, updateLedDisplay, NULL, &displayTimer);
    add_repeating_timer_ms(125, updateHold, NULL, &holdTimer);

    struct audio_buffer_pool *ap = init_audio();

    beatPlaying = false;

    if (!externalClock)
    {
        syncInPpqn = 1;
        deltaT = 500000;
        nextPulseTime = (44100 * deltaT) / (1000000);
        nextPredictedPulseTime = nextPulseTime;
        beatStarted = true;
    }

    // audio buffer loop, runs forever
    int groupStatus[8] = {GROUP_PENDING};
    while (true)
    {
        struct audio_buffer *buffer = take_audio_buffer(ap, true);
        int16_t *bufferSamples = (int16_t *)buffer->buffer->bytes;

        int64_t audioProcessStartTime = time_us_64();

        int crush = analogReadings[POT_CRUSH];
        applyDeadZones(crush);
        crush = 4095 - (((4095 - crush) * (4095 - crush)) >> 12);
        crush = crush >> 8;

        // pitch knob maths - looks horrible! the idea is for the knob to have a deadzone at 12 o'clock which corresponds to normal playback speed (1024). to keep the maths efficient (ish), i'm trying to avoid division, because apart from the deadzone section, it doesn't matter too much what the exact values are, so i'm just using bitwise operators to crank up the range until it feels right. CV adjustment happens after the horrible maths, because otherwise CV control (e.g. a sine LFO pitch sweep) would sound weird and disjointed
        int pitchInt = analogReadings[POT_PITCH];
        int pitchDeadZone = 500;
        if (pitchInt > 2048 + pitchDeadZone)
        {
            // speed above 100%
            pitchInt = ((pitchInt - 2048 - pitchDeadZone) << 1) + 1024;
        }
        else if (pitchInt < 2048 - pitchDeadZone)
        {
            // speed below 100%, and reverse
            pitchInt = pitchInt - 2048 + pitchDeadZone + 1024;
            if (pitchInt < 0)
            {
                pitchInt = pitchInt << 3;
            }
        }
        else
        {
            // speed exactly 100%, in deadzone
            pitchInt = 1024;
        }
        pitchInt += analogReadings[CV_TBC] - 2048;
        if (pitchInt == 0)
            pitchInt = 1; // seems sensible, prevents sample getting stuck

        Sample::pitch = pitchInt; // temp...

        // update audio output
        for (uint i = 0; i < buffer->max_sample_count * 2; i += 2)
        {
            // sample updates go here
            int32_t out1 = 0;
            int32_t out2 = 0;

            // update step at PPQN level
            if(beatStarted && beatPlaying && currentTime >= nextPulseTime) {
                prevPulseTime = nextPulseTime;
                nextPulseTime += (44100*deltaT)/(1000000);
                if(!externalClock) {
                    nextPredictedPulseTime = nextPulseTime;
                }
                pulseStep = (pulseStep + (3360 / syncInPpqn)) % numSteps;
            }

            // update step
            int64_t step = pulseStep + (3360 * (currentTime - prevPulseTime)) / (syncInPpqn * (nextPulseTime - prevPulseTime));
            bool newStep = false; // only check beat when step has been incremented

            // do stuff if step has been incremented
            if(beatStarted && beatPlaying && currentTime < nextPredictedPulseTime && step != lastStep) {
                newStep = true;
                if(step == 0 && numSteps != newNumSteps) {
                    numSteps = newNumSteps;
                    if(activeButton == BUTTON_TIME_SIGNATURE) {
                        displayTimeSignature();
                    }
                }
                if(step == 0) {
                    // reset groups
                    for(int j=0; j<8; j++) {
                        groupStatus[j] = GROUP_PENDING;
                    }
                }
                if((step % (3360/syncOutPpqn)) == 0) {
                    int64_t syncOutDelay = (1000000 * (SAMPLES_PER_BUFFER + i / 2)) / 44100 + lastDacUpdateMicros - time_us_64();
                    pulseLed(0, syncOutDelay+15000); // the 15000 is a bodge because something is wrong here
                    pulseGpio(SYNC_OUT, syncOutDelay); // removed 15000 bodge because we want minimum latency for devices receiving clock signal
                }
                if((step % 3360) == 0) {
                    int64_t pulseDelay = (1000000 * (SAMPLES_PER_BUFFER + i / 2)) / 44100 + lastDacUpdateMicros - time_us_64();
                    if(activeButton == NO_ACTIVE_BUTTON || activeButton == BUTTON_LIVE_EDIT) {
                        displayPulse(step / 3360, pulseDelay);
                    }
                    pulseLed(2, pulseDelay);
                }
                if (forceBeatUpdate)
                {
                    for (int j = 0; j < NUM_SAMPLES; j++)
                    {
                        nextBeatCheckStep[j] = step;
                    }
                    forceBeatUpdate = false;
                }
            }
            if(activeButton == BUTTON_LIVE_EDIT && beatPlaying && (step % 3360) < 840) {
                out1 += metronomeIndex < 50 ? 5000 : -5000;
                metronomeIndex = (metronomeIndex + (step < 3360 ? 2 : 1)) % 100;
            }
            lastStep = step; // this maybe needs to be reset to -1 or INT_MAX or something when beat stops?

            int tempChance = analogReadings[POT_CHANCE];
            applyDeadZones(tempChance);

            for (int j = 0; j < NUM_SAMPLES; j++)
            {
                if(newStep) {
                    int thisVel = 0;
                    int foundHit = -1;
                    if(step == nextBeatCheckStep[j]) {
                        foundHit = beats[beatNum].getHit(j, step);
                        nextBeatCheckStep[j] = beats[beatNum].getNextHitStep(j, step);
                    }
                    if(foundHit >= 0) {
                        int prob = beats[beatNum].hits[foundHit].probability;
                        int combinedProb = (prob * tempChance) >> 11;
                        int group = beats[beatNum].hits[foundHit].group;
                        int randNum = rand() % 255;
                        // hits from group 0 are treated independently (group 0 is not a group)
                        if(group>0) {
                            // if the group's status has not yet been determined, calculate it
                            if(groupStatus[group] == GROUP_PENDING) {
                                groupStatus[group] = (prob > randNum) ? GROUP_YES : GROUP_NO;
                            }
                            if(groupStatus[group] == GROUP_YES && (tempChance>>3)>randNum) {
                                thisVel = beats[beatNum].hits[foundHit].velocity;
                            }
                        }
                        else if (combinedProb > randNum)
                        {
                            thisVel = beats[beatNum].hits[foundHit].velocity;
                        }
                    }
                    // calculate whether random hit should occur, even if a beat hit has already been found (random hit could be higher velocity)
                    thisVel = std::max(thisVel, getRandomHitVelocity(step));
                    if(thisVel > 0) {
                        samples[j].queueHit(currentTime, 0, thisVel);
                        int64_t syncOutDelay = (1000000 * (SAMPLES_PER_BUFFER + i / 2)) / 44100 + lastDacUpdateMicros - time_us_64() + 15000; // the 15000 is a bodge because something is wrong here
                        pulseGpio(TRIGGER_OUT_PINS[j], syncOutDelay);
                    }
                }
                samples[j].update(currentTime);
                out1 += samples[j].value;
                out2 += samples[j].value;
            }
            if (glitch1)
                bufferSamples[i] = (out1 >> (2 + crush)) << crushVolume[crush];
            else
                bufferSamples[i] = out1 >> 2;
            if (glitch2)
                bufferSamples[i + 1] = (out2 >> (2 + crush)) << crushVolume[crush];
            else
                bufferSamples[i + 1] = out2 >> 2;

            currentTime++;
        }

        int64_t audioProcessTime = time_us_64() - audioProcessStartTime; // should be well below 5.8ms (5800us)
        if(audioProcessTime > 5000) {
            if(prevProcessTimeSlow) {
                showError("perf");
                beatPlaying = false;
            } else {
                prevProcessTimeSlow = true;
            }
        } else {
            prevProcessTimeSlow = false;
        }
        //printf("%lld\n", audioProcessTime);

        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);
        lastDacUpdateSamples = currentTime;
        lastDacUpdateMicros = time_us_64();

        if (sdSafeLoadTemp)
        {
            sdSafeLoadTemp = false;
            loadSamplesFromSD();
        }

        if (sdShowFolderTemp)
        {
            sdShowFolderTemp = false;
            scanSampleFolders();
            if (activeButton != ERROR_DISPLAY)
            {
                if (sampleFolderNum >= numSampleFolders)
                    sampleFolderNum = 0; // just in case card is removed, changed, and reinserted - todo: proper folder names comparison to revert to first folder if any changes
                char ledFolderName[5];
                strncpy(ledFolderName, folderNames[sampleFolderNum], 5);
                updateLedDisplayAlpha(ledFolderName);
            }
        }
    }
    return 0;
}

void showError(const char *msgx)
{
    updateLedDisplayAlpha(msgx);
    activeButton = ERROR_DISPLAY;
    setLed(3, true);
}

void loadSamplesFromSD()
{
    uint totalSize = 0;
    bool dryRun = false;
    int pageNum = 0;
    const char *sampleNames[NUM_SAMPLES] = {"/1.wav", "/2.wav", "/3.wav", "/4.wav"};
    uint32_t sampleStartPoints[NUM_SAMPLES] = {0};
    uint32_t sampleLengths[NUM_SAMPLES] = {0};

    for (int n = 0; n < NUM_SAMPLES; n++)
    {
        sd_init_driver();
        sd_card_t *pSD = sd_get_by_num(0);
        if (!pSD->sd_test_com(pSD))
        {
            showError("card");
            return;
        }
        FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);
        if (FR_OK != fr)
        {
            printf("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
            showError("err");
            return;
        }
        FIL fil;
        char filename[255];
        strcpy(filename, "samples/");
        strcpy(filename + strlen(filename), folderNames[sampleFolderNum]);
        strcpy(filename + strlen(filename), sampleNames[n]);
        fr = f_open(&fil, filename, FA_READ);
        if (FR_OK != fr)
        {
            printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
            showError("err");
            return;
        }

        // below is my first proper stab at a WAV file parser. it's messy but it works as long as you supply a mono, 16-bit, 44.1kHz file. need to make it handle more edge cases in future

        // read WAV file header before reading data
        uint br; // bytes read
        bool foundDataChunk = false;
        char descriptorBuffer[4];
        uint8_t sizeBuffer[4];
        uint8_t sampleDataBuffer[FLASH_PAGE_SIZE];

        // first 3 4-byte sections should be "RIFF", file size, "WAVE"
        for (int i = 0; i < 3; i++)
        {
            fr = f_read(&fil, descriptorBuffer, sizeof descriptorBuffer, &br);
            if (i == 0 || i == 2)
                printf("check... %s\n", descriptorBuffer);
        }

        while (!foundDataChunk)
        {
            fr = f_read(&fil, descriptorBuffer, sizeof descriptorBuffer, &br);
            printf("descriptor=%s, ", descriptorBuffer);
            fr = f_read(&fil, sizeBuffer, sizeof sizeBuffer, &br);
            uint32_t chunkSize = (uint32_t)getIntFromBuffer(sizeBuffer, 0);
            printf("size=%d bytes\n", chunkSize);
            uint brTotal = 0;
            for (;;)
            {
                uint bytesToRead = std::min(sizeof sampleDataBuffer, (uint)chunkSize);
                fr = f_read(&fil, sampleDataBuffer, bytesToRead, &br);
                if (br == 0)
                    break;

                if (strncmp(descriptorBuffer, "data", 4) == 0)
                {
                    if (!foundDataChunk)
                    {
                        sampleStartPoints[n] = pageNum * FLASH_PAGE_SIZE;
                        sampleLengths[n] = chunkSize;
                        totalSize += chunkSize;
                        printf("sample %d, start on page %d, length %d\n", n, sampleStartPoints[n], sampleLengths[n]);
                    }
                    foundDataChunk = true;

                    if (!dryRun)
                    {
                        writePageToFlash(sampleDataBuffer, FLASH_AUDIO_ADDRESS + pageNum * FLASH_PAGE_SIZE);
                    }

                    pageNum++;
                }

                brTotal += br;
                if (brTotal == chunkSize)
                {
                    break;
                }
            }
        }

        printf("\n");
        fr = f_close(&fil);
        if (FR_OK != fr)
        {
            printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
        }
        f_unmount(pSD->pcName);
    }
    printf("TOTAL SAMPLE SIZE: %d bytes\n", totalSize);

    if (!dryRun)
    {
        // write sample metadata to flash
        uint8_t metadataBuffer[FLASH_PAGE_SIZE] = {0};

        // copy data so we don't lose other data (check num, tempo, etc)
        for (int i = 0; i < FLASH_PAGE_SIZE; i++)
        {
            metadataBuffer[i] = flashData[i];
        }

        for (int n = 0; n < NUM_SAMPLES; n++)
        {
            std::memcpy(metadataBuffer + SAMPLE_START_POINTS + n * 4, &sampleStartPoints[n], 4);
            std::memcpy(metadataBuffer + SAMPLE_LENGTHS + n * 4, &sampleLengths[n], 4);
        }
        writePageToFlash(metadataBuffer, FLASH_AUDIO_METADATA_ADDRESS);

        initSamplesFromFlash();
    }
}

void resetSampleFolderList()
{
    char nullString[MAX_FOLDER_NAME_LENGTH] = "";
    for (int i = 0; i < MAX_SAMPLE_FOLDERS; i++)
    {
        strncpy(folderNames[i], nullString, MAX_FOLDER_NAME_LENGTH);
    }
}

// not the most beautiful function, but this adds a sample folder at the right point in the alphabetical list
void addSampleFolder(char *newFolderPointer)
{
    int insertPoint = 0;
    bool done = false;

    for (int i = 0; i < MAX_SAMPLE_FOLDERS && !done; i++)
    {
        int res = strncmp(newFolderPointer, folderNames[i], MAX_FOLDER_NAME_LENGTH);
        if (res < 0 || folderNames[i][0] == 0)
        {
            insertPoint = i;
            done = true;
        }
    }

    done = false;
    for (int i = MAX_SAMPLE_FOLDERS - 1; i > insertPoint && i > 0; i--)
    {
        strncpy(folderNames[i], folderNames[i - 1], MAX_FOLDER_NAME_LENGTH);
    }
    strncpy(folderNames[insertPoint], newFolderPointer, MAX_FOLDER_NAME_LENGTH);
}

// go through every sample folder and create an alphabetically ordered array of the folder names
void scanSampleFolders()
{
    printf("scan sample folders...\n");
    sd_init_driver();
    sd_card_t *pSD = sd_get_by_num(0);
    if (!pSD->sd_test_com(pSD))
    {
        showError("card");
        return;
    }
    FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);
    if (FR_OK != fr)
    {
        printf("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
        showError("card");
        return;
    }
    static FILINFO fno;
    int foundNum = -1;
    DIR dir;
    fr = f_opendir(&dir, path);
    if (FR_OK != fr)
    {
        printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
        showError("err");
        return;
    }
    resetSampleFolderList();
    for (;;)
    {
        fr = f_readdir(&dir, &fno); /* Read a directory item */
        if (fr != FR_OK || fno.fname[0] == 0)
            break; /* Break on error or end of dir */
        if (fno.fattrib & AM_DIR)
        { /* It is a directory */
            foundNum++;
            addSampleFolder(fno.fname);
        }
    }
    numSampleFolders = foundNum + 1;

    f_closedir(&dir);
}

void saveSettings()
{
    // if (checkSettingsChange())
    // {
    //     uint saveNum = getIntFromBuffer(flashData, currentSettingsSector * FLASH_SECTOR_SIZE + 4) + 1;
    //     uint saveSector = (currentSettingsSector + 1) % 64; // temp, 64 should be properly defined somewhere
    //     currentSettingsSector = saveSector;
    //     printf("save settings to sector %d\n", currentSettingsSector);

    //     uint8_t buffer[FLASH_PAGE_SIZE];
    //     int32_t refCheckNum = CHECK_NUM;
    //     int32_t refExternalClock = externalClock ? 1 : 0;
    //     int32_t refOutput1 = 0;
    //     int32_t refOutput2 = 0;
    //     for (int i = 0; i < NUM_SAMPLES; i++)
    //     {
    //         bitWrite(refOutput1, i, samples[i].output1);
    //         bitWrite(refOutput2, i, samples[i].output2);
    //     }
    //     std::memcpy(buffer, &refCheckNum, 4);
    //     std::memcpy(buffer + 4, &saveNum, 4);
    //     std::memcpy(buffer + 8 + 4 * SETTING_CLOCK_MODE, &refExternalClock, 4);
    //     std::memcpy(buffer + 8 + 4 * SETTING_OUTPUT_1, &refOutput1, 4);
    //     std::memcpy(buffer + 8 + 4 * SETTING_OUTPUT_2, &refOutput2, 4);
    //     std::memcpy(buffer + 8 + 4 * SETTING_GLITCH_CHANNEL, &glitchChannel, 4);
    //     std::memcpy(buffer + 8 + 4 * SETTING_OUTPUT_PULSE_LENGTH, &outputPulseLength, 4);
    //     std::memcpy(buffer + 8 + 4 * SETTING_OUTPUT_PPQN, &syncOutPpqnIndex, 4);
    //     std::memcpy(buffer + 8 + 4 * SETTING_INPUT_PPQN, &syncInPpqnIndex, 4);
    //     std::memcpy(buffer + 8 + 4 * SETTING_PITCH_CURVE, &refCheckNum, 4);    // temp
    //     std::memcpy(buffer + 8 + 4 * SETTING_INPUT_QUANTIZE, &refCheckNum, 4); // temp
    //     std::memcpy(buffer + 8 + 4 * SETTING_BEAT, &beatNum, 4);
    //     std::memcpy(buffer + 8 + 4 * SETTING_TEMPO, &tempo, 2);
    //     std::memcpy(buffer + 8 + 4 * SETTING_TUPLET, &tuplet, 4);
    //     std::memcpy(buffer + 8 + 4 * SETTING_TIME_SIG, &newNumSteps, 4);

    //     writePageToFlash(buffer, FLASH_DATA_ADDRESS + saveSector * FLASH_SECTOR_SIZE);
    // }
}

// Writes a page of data (256 bytes) to a flash address. If the page number is at the start of a sector, that sector is erased first. Function designed to be called several times for consecutive pages, starting at the start of a sector, otherwise data won't be written properly.
void writePageToFlash(const uint8_t *buffer, uint address)
{
    uint32_t interrupts; // The Pico requires you to save and disable interrupts when writing/erasing flash memory

    uint pageNum = address / FLASH_PAGE_SIZE;
    if (pageNum % (FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE) == 0)
    {
        interrupts = save_and_disable_interrupts();
        flash_range_erase(address, FLASH_SECTOR_SIZE);
        restore_interrupts(interrupts);
    }

    interrupts = save_and_disable_interrupts();
    flash_range_program(address, buffer, FLASH_PAGE_SIZE);
    restore_interrupts(interrupts);

    // todo: check data has been written? check buffer is right size? basically more checks
}

void saveAllBeats() {
    if (beatNum != saveBeatLocation)
    {
        for (int i = 0; i < MAX_BEAT_HITS; i++)
        {
            // would be neater with memcpy
            beats[saveBeatLocation].hits[i].sample = beats[beatNum].hits[i].sample;
            beats[saveBeatLocation].hits[i].step = beats[beatNum].hits[i].step;
            beats[saveBeatLocation].hits[i].velocity = beats[beatNum].hits[i].velocity;
            beats[saveBeatLocation].hits[i].probability = beats[beatNum].hits[i].probability;
            beats[saveBeatLocation].hits[i].group = beats[beatNum].hits[i].group;
        }
        beats[saveBeatLocation].numHits = beats[beatNum].numHits;
        revertBeat(beatNum);
        beatNum = saveBeatLocation;
    }
    backupBeat(beatNum);

    uint8_t buffer[FLASH_PAGE_SIZE] = {0};
    int bufferIndex = 0;
    int pageNum = 0;
    int structSize = sizeof(Beat::Hit);
    assert(structSize <= 8); // otherwise data won't fit
    for(int i=0; i<NUM_BEATS; i++) {
        for(int j=0; j<MAX_BEAT_HITS; j++) {
            std::memcpy(buffer + bufferIndex, &(beats[i].hits[j]), structSize);
            bufferIndex += 8;
            if(bufferIndex == FLASH_PAGE_SIZE) {
                writePageToFlash(buffer, FLASH_USER_BEATS_ADDRESS + pageNum * FLASH_PAGE_SIZE);
                bufferIndex = 0;
                pageNum ++;
                for(int k=0; k<FLASH_PAGE_SIZE; k++) {
                    buffer[k] = 0; // reset buffer
                }
            }
        }
    }
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
        //testInt = 9999; // temp, force initialisation
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
        uint8_t buffer[FLASH_PAGE_SIZE];
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

        saveSettings();
        saveAllBeats();
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
    int structSize = sizeof(Beat::Hit);
    assert(structSize <= 8);
    uint8_t tempBuffer[8] = {0};
    for (int i = 0; i < NUM_BEATS; i++)
    {
        for(int j=0; j<MAX_BEAT_HITS; j++) {
            std::memcpy(&beats[i].hits[j], &flashUserBeats[(i*MAX_BEAT_HITS + j) * 8], structSize);
            if(beats[i].hits[j].sample != 255) {
                beats[i].numHits = j + 1;
            }
        }
    }
    backupBeat(beatNum);
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

int64_t gpioPulseLowCallback(alarm_id_t id, void *user_data)
{
    uint gpioNum = (uint)user_data;
    gpio_put(gpioNum, 0);
    return 0;
}

int64_t gpioPulseHighCallback(alarm_id_t id, void *user_data)
{
    uint gpioNum = (uint)user_data;
    gpio_put(gpioNum, 1);
    add_alarm_in_ms(15, gpioPulseLowCallback, (void *)gpioNum, true);
    return 0;
}

void pulseGpio(uint gpioNum, uint16_t delayMicros)
{
    add_alarm_in_us(delayMicros, gpioPulseHighCallback, (void *)gpioNum, true);
}