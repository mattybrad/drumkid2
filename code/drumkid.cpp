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
#include "hardware/watchdog.h"
#include "pico/stdlib.h"
// #include "pico/multicore.h"
#include "pico/audio_i2s.h"
#include "pico/binary_info.h"
bi_decl(bi_3pins_with_names(PICO_AUDIO_I2S_DATA_PIN, "I2S DIN", PICO_AUDIO_I2S_CLOCK_PIN_BASE, "I2S BCK", PICO_AUDIO_I2S_CLOCK_PIN_BASE + 1, "I2S LRCK"));

// Drumkid classes
#include "Sample.h"
#include "Beat.h"
#include "defaultbeats.h"
#include "sevensegcharacters.h"
#include "drumkid.h"
#include "sn74595.pio.h"
#include "sn74165.pio.h"

// globals, organise later
int64_t currentTime = 0; // samples
uint16_t pulseStep = 0;
int numSteps = 4 * SYSTEM_PPQN;
int newNumSteps = numSteps; // newNumSteps store numSteps value to be set at start of next bar
bool validDeltaT = false;
int64_t currentPulseTime = INT64_MAX;
bool currentPulseFound = false;
uint64_t totalPulses = 0;
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
alarm_id_t displayPulseAlarm = 0;
bool clusterReady[NUM_SAMPLES] = {false}; // if true, it means a sample was triggered on the previous possible step (according to zoom level), so the cluster value will be taken into account
int magnetCurve[6][32]; // i don't remember why these numbers are like this...
int magnetZoomValue = 0;
uint32_t gpioPulseStatuses = 0;
uint8_t ledPulseStatuses = 0;
uint32_t errorStatuses = 0;
int isSilent = false;
bool specificClearDone = false;

// NB order = NA,NA,NA,NA,tom,hat,snare,kick
uint8_t dropRef[11] = {
    0b11110000,
    0b11110001,
    0b11111001,
    0b11111101,
    0b11111111,
    0b11111111, // extra copies of full (no drop) value to provide deadzone
    0b11111111,
    0b11111110,
    0b11110110,
    0b11110100,
    0b11110000
};

alarm_id_t eventAlarm = 0; // single alarm to handle all events
struct Event
{
    uint64_t time = UINT64_MAX;
    void (*callback)(int);
    int user_data = 0;
};
Event events[MAX_EVENTS];

int activeButton = BOOTUP_VISUALS;
int activeSetting = 0;
int settingsMenuLevel = 0;
int outputSample = 0;

// variables, after adjustments such as deadzones/CV
int chance;
int zoom;
int swing;
int cluster;
int magnet;
int velRange;
int velocity;
int drop;
int dropRandom;
uint8_t swingMode = SWING_STRAIGHT;

const int NUM_PPQN_VALUES = 8;
int ppqnValues[NUM_PPQN_VALUES] = {1, 2, 4, 8, 12, 16, 24, 32};
int syncInPpqnIndex = 0;
int syncOutPpqnIndex = 2;
int syncInPpqn = ppqnValues[syncInPpqnIndex];
int syncOutPpqn = ppqnValues[syncOutPpqnIndex];
int64_t prevPulseTime = 0; // samples
int64_t nextPulseTime = 0; // samples
int64_t nextPredictedPulseTime = 0; // samples
uint quarterNote = 0;
int64_t deltaT;                 // microseconds
int64_t lastDacUpdateSamples;   // samples
int64_t lastDacUpdateMicros;    // microseconds
bool beatStarted = false;
bool firstHit = true;
int tuplet = TUPLET_STRAIGHT;

uint8_t crushVolume[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 13, 13};
int crushChannel = CRUSH_CHANNEL_BOTH;
bool crush1 = true;
bool crush2 = true;

bool factoryResetCodeFound = false;
bool doFactoryResetSafe = false;
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
    
    int64_t currentTimeSamples = lastDacUpdateSamples + (44100 * (time_us_64() - lastDacUpdateMicros)) / 1000000;
    currentPulseFound = false;
    prevPulseTime = currentPulseTime;
    currentPulseTime = currentTimeSamples + SAMPLES_PER_BUFFER;
    nextPredictedPulseTime = currentPulseTime + (44100 * deltaT) / (1000000);
    lastClockIn = time_us_64();
    totalPulses ++;
    if (activeButton == BUTTON_MANUAL_TEMPO) {
        displayTempo();
    }
}

void gpio_callback(uint gpio, uint32_t events)
{
    if(externalClock) {
        handleSyncPulse();
        pulseLed(1, 100); // delay was 0, seemed to cause issues, added 100us delay, can't pretend i know why this works
    }
}

uint16_t shiftRegOutBase = 0b0000111100000000;
int tempDigit = 0;

void setLed(uint8_t ledNum, bool value) {
    bitWrite(shiftRegOutBase, 15-ledNum, value);
}

void ledPulseLowCallback(int user_data)
{
    uint ledNum = (uint)user_data;
    setLed(ledNum, false);
    bitWrite(ledPulseStatuses, ledNum, false);
}

void ledPulseHighCallback(int user_data)
{
    uint ledNum = (uint)user_data;
    if (!bitRead(ledPulseStatuses, ledNum)) {
        setLed(ledNum, true);
        bitWrite(ledPulseStatuses, ledNum, true);
        addEvent(20000, ledPulseLowCallback, ledNum);
    } else {
        setLed(ledNum, false);
    }
}

void pulseLed(uint ledNum, uint16_t delayMicros)
{
    addEvent(delayMicros, ledPulseHighCallback, ledNum);
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

uint32_t lastValidData = 0;
bool updateInputShiftRegisters(repeating_timer_t *rt)
{
    uint32_t data;
    bool tempInputChanged = false;
    data = sn74165::shiftreg_get(&tempInputChanged);
    if (tempInputChanged)
    {
        lastValidData = data;
        for(int i=0; i<32; i++) {
            uint8_t buttonNum = (2-(i>>3))*8 + (i%8) + 1; // to match SW num in schematic (SW1 is top left on unit) - could be lookup table to improve speed
            bool newVal = bitRead(data, i);
            if(bitRead(buttonStableStates, buttonNum) != newVal) {
                if(cyclesSinceChange[i] >= DEBOUNCING_CYCLES) {
                    bitWrite(buttonStableStates, buttonNum, newVal);
                    handleButtonChange(buttonNum, newVal);
                    cyclesSinceChange[i] = 0;
                }
            }
        }  
    } else {
        for (int i = 0; i < 32; i++)
        {
            if (cyclesSinceChange[i] < DEBOUNCING_CYCLES) {
                cyclesSinceChange[i]++;
                if(cyclesSinceChange[i] == DEBOUNCING_CYCLES) {
                    uint8_t buttonNum = (2 - (i >> 3)) * 8 + (i % 8) + 1; // to match SW num in schematic (SW1 is top left on unit) - could be lookup table to improve speed
                    bool newVal = bitRead(lastValidData, i);
                    if (bitRead(buttonStableStates, buttonNum) != newVal)
                    {
                        bitWrite(buttonStableStates, buttonNum, newVal);
                        handleButtonChange(buttonNum, newVal);
                        cyclesSinceChange[i] = 0;
                    }
                }
            }
        }
    }

    return true;
}

void clearHits(int sampleNum) {
    specificClearDone = true;
    for (int i = 0; i < beats[beatNum].numHits; i++)
    {
        if ((beats[beatNum].hits[i].sample < NUM_SAMPLES && beats[beatNum].hits[i].sample == sampleNum) || sampleNum == -1)
        {
            beats[beatNum].removeHitAtIndex(i);
            i--;
        }
    }
}

void doLiveHit(int sampleNum)
{
    int quantizeSteps = tupletEditLiveMultipliers[tuplet];
    int64_t samplesSincePulse = (44100 * (time_us_64() - lastDacUpdateMicros)) / 1000000 + lastDacUpdateSamples - currentPulseTime - SAMPLES_PER_BUFFER;
    int samplesPerPulse = nextPredictedPulseTime - currentPulseTime;
    int thisStep = (pulseStep + (samplesSincePulse * SYSTEM_PPQN) / samplesPerPulse) % numSteps;
    thisStep = ((thisStep + quantizeSteps/2) % numSteps) / quantizeSteps;
    thisStep = thisStep * quantizeSteps;
    beats[beatNum].addHit(sampleNum, thisStep, velocity>>4, 255, 0);

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
        samples[sampleNum].queueHit(lastDacUpdateSamples, 0, velocity >> 4);
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
            if(externalClock) {
                // reset to start of bar on next hit
                pulseStep = numSteps - (SYSTEM_PPQN/syncInPpqn);
                for(int i=0; i<NUM_SAMPLES; i++) {
                    nextBeatCheckStep[i] = 0;
                }
                if(activeButton == NO_ACTIVE_BUTTON) {
                    displayPulse(0, 0);
                }
            } else {
                numSteps = newNumSteps;
                if (activeButton == BUTTON_TIME_SIGNATURE)
                    displayTimeSignature();
                else {
                    activeButton = NO_ACTIVE_BUTTON;
                }
                beatPlaying = !beatPlaying;
                if (beatPlaying)
                {
                    // handle beat start
                    clearError(ERROR_PERFORMANCE);
                    if (!externalClock)
                    {
                        pulseStep = 0;
                        currentPulseTime = lastDacUpdateSamples + SAMPLES_PER_BUFFER;
                        nextPredictedPulseTime = currentPulseTime + (44100 * deltaT) / (1000000);
                        for(i=0; i<NUM_SAMPLES; i++) {
                            nextBeatCheckStep[i] = 0;
                        }
                    }
                }
                else
                {
                    // handle beat stop
                    pulseStep = 0;
                    if (activeButton == BUTTON_TIME_SIGNATURE)
                        displayTimeSignature();
                    else {
                        activeButton = NO_ACTIVE_BUTTON;
                        displayPulse(0, 0);
                    }
                }
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
            scheduleSaveSettings();
            break;
        case BUTTON_LIVE_EDIT:
            activeButton = BUTTON_LIVE_EDIT;
            displayPulse(pulseStep / SYSTEM_PPQN, 0);
            break;
        case BUTTON_INC:
            if (activeButton == BUTTON_LIVE_EDIT)
            {
                if(bitRead(buttonStableStates, BUTTON_CLEAR)) {
                    clearHits(0);
                } else {
                    doLiveHit(0);
                }
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
                if (bitRead(buttonStableStates, BUTTON_CLEAR))
                {
                    clearHits(1);
                }
                else
                {
                    doLiveHit(1);
                }
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
                if (bitRead(buttonStableStates, BUTTON_CLEAR))
                {
                    clearHits(2);
                }
                else
                {
                    doLiveHit(2);
                }
            }
            else
            {
                handleYesNo(true);
            }
            break;
        case BUTTON_CANCEL:
            if (activeButton == BUTTON_LIVE_EDIT)
            {
                if (bitRead(buttonStableStates, BUTTON_CLEAR))
                {
                    clearHits(3);
                }
                else
                {
                    doLiveHit(3);
                }
            }
            else
            {
                handleYesNo(false);
            }
            break;
        case BUTTON_BACK:
            if (activeButton == BUTTON_MENU && (activeSetting == SETTING_OUTPUT_1 || activeSetting == SETTING_OUTPUT_2 || activeSetting == SETTING_CRUSH_CHANNEL))
            {
                settingsMenuLevel = 0;
                displaySettings();
            } else {
                // clear almost all errors on pressing back button (but don't clear sample size error because ideally that should be fixed by loading different samples)
                clearError(ERROR_PERFORMANCE);
                clearError(ERROR_SD_MISSING);
                clearError(ERROR_SD_OPEN);
                clearError(ERROR_SD_CLOSE);
                clearError(ERROR_SD_MOUNT);
                activeButton = NO_ACTIVE_BUTTON;
                displayPulse(lastStep/SYSTEM_PPQN,0);
            }
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
            // do nothing until button release
            specificClearDone = false;
            break;
        case BUTTON_STEP_EDIT:
            if(activeButton == BUTTON_STEP_EDIT) {
                editSample = (editSample + 1) % 4;
            }
            activeButton = BUTTON_STEP_EDIT;
            displayEditBeat();
            break;
        case BUTTON_SAVE:
            if(activeButton == BUTTON_STEP_EDIT || activeButton == BUTTON_LIVE_EDIT) {
                saveBeatLocation = beatNum;
                activeButton = BUTTON_SAVE;
                updateLedDisplayInt(saveBeatLocation);
            } else {
                saveSettingsToFlash();
            }
            break;
        default:
            printf("(button %d not assigned)\n", buttonNum);
        }
    } else {
        switch(buttonNum) {
            case BUTTON_CLEAR:
                // when clear button released, if no specific clears have already happened, clear whole beat
                if ((activeButton == BUTTON_STEP_EDIT || activeButton == BUTTON_LIVE_EDIT) && !specificClearDone)
                {
                    clearHits(-1);
                }
                break;
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
        // suspect there may be more things to adjust when switching between modes, but here's what i can think of:
        tempo = 600000000 / deltaT;
        displayClockMode();
        break;

    case SETTING_OUTPUT_1:
    case SETTING_OUTPUT_2:
        outputSample += isInc ? 1 : -1;
        if (outputSample < 0)
            outputSample = 0;
        else if (outputSample > NUM_SAMPLES - 1)
            outputSample = NUM_SAMPLES - 1;
        displayOutput(activeSetting == SETTING_OUTPUT_1 ? 1 : 2);
        break;

    case SETTING_CRUSH_CHANNEL:
        crushChannel += thisInc;
        crushChannel = std::max(0, std::min(3, crushChannel));
        crush1 = !bitRead(crushChannel, 1);
        crush2 = !bitRead(crushChannel, 0);
        break;

    case SETTING_OUTPUT_PULSE_LENGTH:
        outputPulseLength += thisInc;
        outputPulseLength = std::max(1, std::min(200, outputPulseLength));
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
        if(externalClock) pulseStep = (pulseStep / (SYSTEM_PPQN / syncInPpqn)) * (SYSTEM_PPQN / syncInPpqn); // quantize pulse step to new PPQN value to prevent annoying phase offset thing
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
            if (tempo > 99999)
                tempo = 99999;
            else if (tempo < 100)
                tempo = 100;
            deltaT = 600000000 / tempo;
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
        updateLedDisplayInt(saveBeatLocation);
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
    scheduleSaveSettings();
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
                    samples[outputSample].output1 = isYes;
                }
                else
                {
                    samples[outputSample].output2 = isYes;
                }
                displayOutput(activeSetting == SETTING_OUTPUT_1 ? 1 : 2);
            }
            else if(activeSetting == SETTING_FACTORY_RESET)
            {
                activeButton = NO_ACTIVE_BUTTON;
                settingsMenuLevel = 0;
                doFactoryResetSafe = true;
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
    scheduleSaveSettings();
}

alarm_id_t saveSettingsAlarm = 0;

int64_t onSaveSettingsTimeout(alarm_id_t id, void *user_data)
{
    saveSettingsAlarm = 0;
    return 0;
}

void scheduleSaveSettings()
{
    if (saveSettingsAlarm > 0)
    {
        cancel_alarm(saveSettingsAlarm);
    }
    saveSettingsAlarm = add_alarm_in_ms(1000, onSaveSettingsTimeout, NULL, true);
}

uint64_t lastTapTempoTime;
uint64_t tapTempoTaps[MAX_TAPS] = {0};
int8_t tapIndex = 0;
int8_t numTaps = 0;
void updateTapTempo()
{
    if (!externalClock)
    {
        bool newTapSession = true;
        if(numTaps == 1) {
            newTapSession = false;
        } else if(numTaps >= 2) {
            int64_t thisTap = time_us_64() - tapTempoTaps[tapIndex];
            int64_t lastTap = tapTempoTaps[tapIndex] - tapTempoTaps[(tapIndex + MAX_TAPS - 1) % MAX_TAPS];
            if(std::abs(thisTap-lastTap) < std::max(thisTap, lastTap)/5) {
                newTapSession = false;
            }
        }

        if (newTapSession) {
            numTaps = 1;
        } else {
            numTaps ++;
        }
        tapIndex = (tapIndex + 1) % MAX_TAPS;
        tapTempoTaps[tapIndex] = time_us_64();
        uint64_t meanDeltaT = 0;
        if(numTaps < 2) {
            updateLedDisplayAlpha("....");
        } else {
            int thisNumTaps = std::min(numTaps, (int8_t)MAX_TAPS);
            for (int i = 0; i < thisNumTaps - 1; i++)
            {
                meanDeltaT += tapTempoTaps[(tapIndex + MAX_TAPS - i) % MAX_TAPS] - tapTempoTaps[(tapIndex + MAX_TAPS - i - 1) % MAX_TAPS];
            }
            meanDeltaT = meanDeltaT / (thisNumTaps - 1);
            deltaT = meanDeltaT;
            tempo = 600000000 / deltaT;
            displayTempo();
        }
        pulseStep = ((numTaps - 1) * SYSTEM_PPQN) % numSteps;
        int64_t currentTimeSamples = lastDacUpdateSamples + (44100 * (time_us_64() - lastDacUpdateMicros)) / 1000000;
        currentPulseFound = false;
        prevPulseTime = currentPulseTime;
        currentPulseTime = currentTimeSamples + SAMPLES_PER_BUFFER;
        nextPredictedPulseTime = currentPulseTime + (44100 * deltaT) / (1000000);
        for(int i=0; i<NUM_SAMPLES; i++) {
            nextBeatCheckStep[i] = pulseStep;
        }
    } else {
        updateLedDisplayAlpha("ext");
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
        if(activeButton == BUTTON_LIVE_EDIT) {
            bitWrite(sevenSegData[i],3,true);
        }
    }
    return 0;
}

void displayPulse(int thisPulse, uint16_t delayMicros)
{
    displayPulseAlarm = add_alarm_in_us(delayMicros, displayPulseCallback, (void *)thisPulse, true);
}

void displayClockMode()
{
    updateLedDisplayAlpha(externalClock ? "ext" : "int");
}

void displayTempo()
{
    if (externalClock)
    {
        int calculatedTempo = 600000000 / (deltaT * syncInPpqn);
        updateLedDisplayInt(calculatedTempo);
        if (calculatedTempo <= 9999)
        {
            // 999.9 BPM or less
            bitWrite(sevenSegData[2], 7, true);
        }
    }
    else
    {
        updateLedDisplayInt(tempo);
        if(tempo<=9999) {
            // 999.9 BPM or less
            bitWrite(sevenSegData[2], 7, true);
        }
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
    updateLedDisplayInt(beatNum);
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
    for (int i = 0; i < 4 && i < NUM_SAMPLES; i++)
    {
        if (outputNum == 1)
        {
            sevenSegData[i] = samples[i].output1 ? 0b00001000 : 0b00000000;
        }
        else if (outputNum == 2)
        {
            sevenSegData[i] = samples[i].output2 ? 0b00001000 : 0b00000000;
        }
    }
    bitWrite(sevenSegData[outputSample], 6, true);
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

        case SETTING_CRUSH_CHANNEL:
            updateLedDisplayAlpha("crsh");
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

        case SETTING_FACTORY_RESET:
            updateLedDisplayAlpha("rset");
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

        case SETTING_CRUSH_CHANNEL:
            if (crushChannel == CRUSH_CHANNEL_BOTH)
                updateLedDisplayAlpha("both");
            else if (crushChannel == CRUSH_CHANNEL_1)
                updateLedDisplayAlpha("1");
            else if (crushChannel == CRUSH_CHANNEL_2)
                updateLedDisplayAlpha("2");
            else if (crushChannel == CRUSH_CHANNEL_NONE)
                updateLedDisplayAlpha("none");
            break;

        case SETTING_OUTPUT_PULSE_LENGTH:
            updateLedDisplayInt(outputPulseLength);
            break;

        case SETTING_OUTPUT_PPQN:
            updateLedDisplayInt(ppqnValues[syncOutPpqnIndex]);
            break;

        case SETTING_INPUT_PPQN:
            updateLedDisplayInt(ppqnValues[syncInPpqnIndex]);
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
            updateLedDisplayInt(inputQuantize);
            break;

        case SETTING_FACTORY_RESET:
            updateLedDisplayAlpha("yes?");
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

void updateLedDisplayInt(int num)
{
    if(displayPulseAlarm > 0) {
        cancel_alarm(displayPulseAlarm);
    }
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
    if(displayPulseAlarm > 0) {
        cancel_alarm(displayPulseAlarm);
    }
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

void updateLedDisplayFloat(float num) {
    if(num >= 999.5) {
        updateLedDisplayInt((int)(num+0.5));
    } else {
        num = num * 10;

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

// could express in terms of SYSTEM_PPQN (3360) but would look ugly... Just change this if SYSTEM_PPQN ever changes
int zoomValues[NUM_TUPLET_MODES][9] = {
    {-1, 13440, 6720, 3360, 1680, 840, 420, 210, 105},
    {-1, 13440, 13440, 3360, 1120, 560, 280, 140, 70},
    {-1, 13440, 13440, 3360, 672, 336, 168, 84, 42},
    {-1, 13440, 13440, 3360, 480, 240, 120, 60, 30},
};

int getMagnetZoomValue(int step) {
    bool foundLowest = false;
    int i;
    for(i=1;i<9&&!foundLowest;i++) {
        if(step%zoomValues[tuplet][i]==0) {
            foundLowest = true;
        }
    }
    return foundLowest ? std::max(0, i-4) : -1; // -4 because quarter note should be 0 (shift by -3), and i has been incremented at end of loop, which needs to be undone (extra -1)
}

int getRandomHitVelocity(int step, int sample) {
    // first draft...

    int thisZoom = zoom  / 456; // gives range from 0 to 8 inclusive
    if(swingMode == SWING_EIGHTH) {
        thisZoom = std::min(thisZoom, 4);
    }
    else if (swingMode == SWING_SIXTEENTH)
    {
        thisZoom = std::min(thisZoom, 5);
    }
    if (zoomValues[tuplet][thisZoom] >= 0 && (step % zoomValues[tuplet][thisZoom]) == 0)
    {
        int thisChance = std::max(chance - 2048, 0) << 1; // check that this can actually reach 4095...
        if (magnetZoomValue == 0 || chance == 4095)
        {
            // leave chance as is
        }
        else if (magnet < 2048)
        {
            thisChance = (magnet * magnetCurve[magnetZoomValue][thisChance >> 7] + (2048 - magnet) * ((thisChance * thisChance) >> 12)) >> 11;
        }
        else
        {
            thisChance = ((4095 - magnet) * magnetCurve[magnetZoomValue][thisChance >> 7]) >> 11;
        }
        if(clusterReady[sample]) {
            thisChance = std::max(cluster, thisChance);
        }
        clusterReady[sample] = false;
        if (thisChance > rand() % 4095)
        {
            int returnVel = velocity + ((velRange * (rand() % 4095)) >> 12) - (velRange>>1);
            if(returnVel < 0) returnVel = 0;
            else if(returnVel > 4095) returnVel = 4095;
            if(returnVel > 0) clusterReady[sample] = true;
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
    //printf("inc button: %d\n", bitRead(buttonStableStates, BUTTON_INC) ? 1 : 0);
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

// stolen from the arduino codebase
int map(int x, int in_min, int in_max, int out_min, int out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void applyDeadZones(int &param, bool centreDeadZone)
{
    param = (4095 * (std::max(ANALOG_DEAD_ZONE_LOWER, std::min(ANALOG_DEAD_ZONE_UPPER, param)) - ANALOG_DEAD_ZONE_LOWER)) / ANALOG_EFFECTIVE_RANGE;
    int deadZoneStart = 2048-250;
    int deadZoneEnd = 2048+250;
    if(centreDeadZone) {
        if(param < deadZoneStart) {
            param = map(param, 0, deadZoneStart, 0, 2048);
        } else if(param > deadZoneEnd) {
            param = map(param, deadZoneEnd, 4095, 2048, 4095);
        } else {
            param = 2048;
        }
    }
}

int main()
{
    stdio_init_all();
    time_init();

    printf("Drumkid V2\n");

    // temp, populating magnet curve
    printf("magnet curve:\n");
    for (int i = 0; i < 6; i++)
    {
        for (int j = 0; j < 32; j++)
        {
            float accurateValue = pow(((float)j * 1.0) / 31.0, 4.0 * (float)i + 1.0);
            magnetCurve[i][j] = 4095.0 * accurateValue;
            printf("%d ", magnetCurve[i][j]);
        }
        printf("\n");
    }

    initGpio();
    checkFlashStatus();
    if(currentSettingsSector == -1) {
        // wipe flash data
        wipeFlash(false);
        loadDefaultBeats();
        saveAllBeats();
        saveSettingsToFlash();
        scanSampleFolders();
        loadSamplesFromSD();
    } else if(factoryResetCodeFound) {
        loadDefaultBeats();
        saveAllBeats();
        saveSettingsToFlash();
        scanSampleFolders();
        loadSamplesFromSD();
    } else {
        loadSettingsFromFlash();
        loadSamplesFromFlash();
        loadBeatsFromFlash();
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
    add_repeating_timer_ms(60, updateLedDisplay, NULL, &displayTimer);
    add_repeating_timer_ms(125, updateHold, NULL, &holdTimer);

    struct audio_buffer_pool *ap = init_audio();

    beatPlaying = false;

    // audio buffer loop, runs forever
    int groupStatus[8] = {GROUP_PENDING};
    while (true)
    {
        if (!externalClock)
        {
            syncInPpqn = 1;
        }

        struct audio_buffer *buffer = take_audio_buffer(ap, true);
        int16_t *bufferSamples = (int16_t *)buffer->buffer->bytes;

        int64_t audioProcessStartTime = time_us_64();

        int crush = analogReadings[POT_CRUSH];
        applyDeadZones(crush, false);
        crush = 4095 - (((4095 - crush) * (4095 - crush)) >> 12);
        crush = crush >> 8;

        int crop = analogReadings[POT_CROP];
        applyDeadZones(crop, false);
        crop = 4095 - crop; // feels better with no effect at 0
        if (crop == 4095)
            crop = MAX_SAMPLE_STORAGE;
        else
            crop = (crop * crop) >> 11; // should be 11
        Sample::crop = crop;

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
        pitchInt += analogReadings[CV_PITCH] - 2048;
        if (pitchInt == 0)
            pitchInt = 1; // seems sensible, prevents sample getting stuck

        Sample::pitch = pitchInt; // temp...

        chance = analogReadings[POT_CHANCE] + analogReadings[CV_CHANCE] - 2048;
        applyDeadZones(chance, true);
        chance = std::min(4095, chance);
        chance = std::max(0, chance);
        zoom = analogReadings[POT_ZOOM] + analogReadings[CV_ZOOM] - 2048;
        applyDeadZones(zoom, false);
        zoom = std::min(4095, zoom);
        zoom = std::max(0, zoom);
        cluster = analogReadings[POT_CLUSTER];
        applyDeadZones(cluster, false);
        magnet = analogReadings[POT_MAGNET];
        applyDeadZones(magnet, true);
        velocity = analogReadings[POT_VELOCITY] + analogReadings[CV_VELOCITY] - 2048;
        applyDeadZones(velocity, false);
        velocity = std::min(4095, velocity);
        velocity = std::max(0, velocity);
        velRange = analogReadings[POT_RANGE];
        applyDeadZones(velRange, false);
        drop = analogReadings[POT_DROP] / 373; // gives range of 0 to 10
        dropRandom = analogReadings[POT_DROP_RANDOM] / 373; // gives range of 0 to 10

        // update audio output
        for (uint i = 0; i < buffer->max_sample_count * 2; i += 2)
        {
            // sample updates go here
            int32_t out1 = 0;
            int32_t out2 = 0;

            // update step at PPQN level
            if(externalClock) {
                if(currentTime >= currentPulseTime && !currentPulseFound) {
                    currentPulseFound = true;
                    pulseStep = (pulseStep + (SYSTEM_PPQN / syncInPpqn)) % numSteps;
                }
            } else {
                if (beatPlaying && currentTime >= nextPredictedPulseTime)
                {
                    prevPulseTime = currentPulseTime;
                    currentPulseTime = currentTime;
                    nextPredictedPulseTime = currentTime + (44100*deltaT)/(1000000);
                    pulseStep = (pulseStep + SYSTEM_PPQN) % numSteps;
                }
            }

            // if(beatPlaying && currentTime >= nextPulseTime) {
            //     prevPulseTime = nextPulseTime;
            //     nextPulseTime += (44100*deltaT)/(1000000);
            //     if(!externalClock) {
            //         nextPredictedPulseTime = nextPulseTime;
            //     }
            //     pulseStep = (pulseStep + (SYSTEM_PPQN / syncInPpqn)) % numSteps;
            // }

            // update step
            int64_t step = pulseStep;
            bool interpolateSteps = true && (!externalClock || totalPulses >= 2);
            // bit messy, could be streamlined
            if (interpolateSteps)
            {
                if (externalClock)
                {
                    int64_t t0 = currentPulseFound ? currentPulseTime : prevPulseTime;
                    int64_t t1 = currentPulseFound ? nextPredictedPulseTime : currentPulseTime;
                    step += (SYSTEM_PPQN * (currentTime - t0)) / (syncInPpqn * (t1 - t0));
                }
                else
                {
                    int64_t t0 = currentPulseTime;
                    int64_t t1 = nextPredictedPulseTime;
                    step += (SYSTEM_PPQN * (currentTime - t0)) / (t1 - t0);
                }
            }
            bool newStep = false; // only check beat when step has been incremented (i.e. loop will probably run several times on, say, step 327, but don't need to do stuff repeatedly for that step)

            // do stuff if step has been incremented
            if ((externalClock || beatPlaying) && currentTime < nextPredictedPulseTime && step != lastStep)
            {
                newStep = true;
                // reset nextBeatCheckStep values if any steps have been skipped
                for(int j=0; j<NUM_SAMPLES;j++) {
                    if(step - lastStep != 1) {
                        nextBeatCheckStep[j] = step;
                    }
                }
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
                if((step % (SYSTEM_PPQN/syncOutPpqn)) == 0) {
                    int64_t syncOutDelay = (1000000 * (SAMPLES_PER_BUFFER + i / 2)) / 44100 + lastDacUpdateMicros - time_us_64();
                    pulseLed(0, syncOutDelay+15000); // the 15000 is a bodge because something is wrong here
                    pulseGpio(SYNC_OUT, syncOutDelay); // removed 15000 bodge because we want minimum latency for devices receiving clock signal
                }
                if((step % SYSTEM_PPQN) == 0) {
                    int64_t pulseDelay = (1000000 * (SAMPLES_PER_BUFFER + i / 2)) / 44100 + lastDacUpdateMicros - time_us_64();
                    if(activeButton == NO_ACTIVE_BUTTON || activeButton == BUTTON_LIVE_EDIT) {
                        displayPulse(step / SYSTEM_PPQN, pulseDelay);
                    }
                    pulseLed(2, pulseDelay);

                    // swing gets set here because it prevents issue where fluctuating pot reading causes skipped hits
                    swing = analogReadings[POT_SWING];
                    applyDeadZones(swing, true);
                    if (swing == 2048)
                    {
                        swing = 0;
                        swingMode = SWING_STRAIGHT;
                    }
                    else if (swing < 2048)
                    {
                        swingMode = SWING_EIGHTH;
                        swing = map(swing, 2047, 0, SYSTEM_PPQN>>1, (SYSTEM_PPQN-1));
                    }
                    else
                    {
                        swingMode = SWING_SIXTEENTH;
                        swing = map(swing, 2049, 4095, (SYSTEM_PPQN>>2), (SYSTEM_PPQN>>1)-1);
                    }
                }
                if (forceBeatUpdate)
                {
                    for (int j = 0; j < NUM_SAMPLES; j++)
                    {
                        nextBeatCheckStep[j] = step;
                    }
                    forceBeatUpdate = false;
                }
                magnetZoomValue = getMagnetZoomValue(step);
            }
            if(activeButton == BUTTON_LIVE_EDIT && beatPlaying && (step % SYSTEM_PPQN) < (SYSTEM_PPQN>>2)) {
                int metronomeSound = metronomeIndex < 50 ? 5000 : -5000;
                metronomeIndex = (metronomeIndex + (step < SYSTEM_PPQN ? 2 : 1)) % 100;
                out1 += metronomeSound;
                out2 += metronomeSound;
            }
            lastStep = step; // this maybe needs to be reset to -1 or INT_MAX or something when beat stops?

            for (int j = 0; j < NUM_SAMPLES; j++)
            {
                if(newStep) {
                    int thisVel = 0;
                    int foundHit = -1;

                    // swing logic
                    int swingStep = step;
                    bool skipStep = false;
                    if(swingMode == SWING_EIGHTH) {
                        if(step % SYSTEM_PPQN == (SYSTEM_PPQN>>1)) {
                            // eighth note, skip it
                            skipStep = true;
                        } else if(step % SYSTEM_PPQN == swing) {
                            swingStep = (SYSTEM_PPQN>>1) + (step / SYSTEM_PPQN) * SYSTEM_PPQN;
                        }
                    } else if(swingMode == SWING_SIXTEENTH) {
                        if (step % (SYSTEM_PPQN>>1) == (SYSTEM_PPQN>>2))
                        {
                            // eighth note, skip it
                            skipStep = true;
                        }
                        else if (step % (SYSTEM_PPQN>>1) == swing)
                        {
                            swingStep = (SYSTEM_PPQN>>2) + (step / (SYSTEM_PPQN>>1)) * (SYSTEM_PPQN>>1);
                        }
                    }

                    if(!skipStep) {
                        if(swingStep == nextBeatCheckStep[j]) {
                            foundHit = beats[beatNum].getHit(j, swingStep);
                            nextBeatCheckStep[j] = beats[beatNum].getNextHitStep(j, step, numSteps);
                        } else if(step == nextBeatCheckStep[j]) {
                            // special (rare) case for if swingStep has no hit but the step it mapped onto does have a hit
                            foundHit = beats[beatNum].getHit(j, step);
                            nextBeatCheckStep[j] = beats[beatNum].getNextHitStep(j, step, numSteps);
                        }
                    }
                    if(foundHit >= 0) {
                        int prob = beats[beatNum].hits[foundHit].probability;
                        int combinedProb = (prob * chance) >> 11;
                        int group = beats[beatNum].hits[foundHit].group;
                        int randNum = rand() % 255;
                        // hits from group 0 are treated independently (group 0 is not a group)
                        if(group>0) {
                            // if the group's status has not yet been determined, calculate it
                            if(groupStatus[group] == GROUP_PENDING) {
                                groupStatus[group] = (prob > randNum) ? GROUP_YES : GROUP_NO;
                            }
                            if(groupStatus[group] == GROUP_YES && (chance>>3)>randNum) {
                                thisVel = beats[beatNum].hits[foundHit].velocity;
                            }
                        }
                        else if (combinedProb > randNum)
                        {
                            thisVel = beats[beatNum].hits[foundHit].velocity;
                        }
                    }
                    // calculate whether random hit should occur, even if a beat hit has already been found (random hit could be higher velocity)
                    bool dropHit = !bitRead(dropRef[drop], j);
                    bool dropHitRandom = !bitRead(dropRef[dropRandom], j);
                    if(!skipStep && activeButton != BUTTON_LIVE_EDIT && !dropHitRandom) {
                        int randomVel = getRandomHitVelocity(swingStep, j);
                        thisVel = std::max(thisVel, randomVel);
                    }
                    if(thisVel > 0 && !dropHit) {
                        samples[j].queueHit(currentTime, 0, thisVel);
                        int64_t syncOutDelay = (1000000 * (SAMPLES_PER_BUFFER + i / 2)) / 44100 + lastDacUpdateMicros - time_us_64() + 15000; // the 15000 is a bodge because something is wrong here
                        pulseGpio(TRIGGER_OUT_PINS[j], syncOutDelay);
                    }
                }
                samples[j].update(currentTime);
                if(samples[j].output1) out1 += samples[j].value;
                if(samples[j].output2) out2 += samples[j].value;
            }
            if (crush1)
                bufferSamples[i] = (out1 >> (2 + crush)) << crushVolume[crush];
            else
                bufferSamples[i] = out1 >> 2;
            if (crush2)
                bufferSamples[i + 1] = (out2 >> (2 + crush)) << crushVolume[crush];
            else
                bufferSamples[i + 1] = out2 >> 2;

            currentTime++;
        }

        if (bufferSamples[0] == 0 && bufferSamples[1] == 0)
        {
            isSilent = true;
            if (isSilent && (currentTime - nextPredictedPulseTime > 44100) && saveSettingsAlarm == 0)
            {
                saveSettingsToFlash();
            }
        } else {
            isSilent = false;
        }

        int64_t audioProcessTime = time_us_64() - audioProcessStartTime; // should be well below 5.8ms (5800us)
        if(audioProcessTime > 5000) {
            if(prevProcessTimeSlow) {
                //flagError(ERROR_PERFORMANCE);
                //beatPlaying = false;
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

        if(doFactoryResetSafe) {
            wipeFlash(true);
            watchdog_enable(0, 1); // causes reboot so we don't need to bother trying to reset all the variables
            while(true) {
                // wait forever (i.e. until reboot!)
            }
        }

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

void flagError(uint8_t errorNum) {
    bitWrite(errorStatuses, errorNum, true);
    setLed(3, true);
    activeButton = ERROR_DISPLAY;
    char errorMessage[4];
    errorMessage[0] = 'e';
    errorMessage[1] = 'r';
    errorMessage[2] = ' ';
    errorMessage[3] = errorNum + 48; // convert error num to ascii code
    updateLedDisplayAlpha(errorMessage); // temp
}

void clearError(uint8_t errorNum) {
    bitWrite(errorStatuses, errorNum, false);
    if(errorStatuses == 0) {
        setLed(3, false);
    }
}

void loadSamplesFromSD()
{
    uint totalSize = 0;
    bool dryRun = false;
    int pageNum = 0;
    const char *sampleNames[NUM_SAMPLES] = {"/1.wav", "/2.wav", "/3.wav", "/4.wav"};
    uint32_t sampleStartPoints[NUM_SAMPLES] = {0};
    uint32_t sampleLengths[NUM_SAMPLES] = {0};
    uint32_t sampleRates[NUM_SAMPLES] = {0};

    for (int n = 0; n < NUM_SAMPLES; n++)
    {
        sd_init_driver();
        sd_card_t *pSD = sd_get_by_num(0);
        if (!pSD->sd_test_com(pSD))
        {
            flagError(ERROR_SD_MISSING);
            return;
        }
        clearError(ERROR_SD_MISSING);
        FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);
        if (FR_OK != fr)
        {
            printf("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
            flagError(ERROR_SD_MOUNT);
            return;
        }
        clearError(ERROR_SD_MOUNT);
        FIL fil;
        char filename[255];
        strcpy(filename, "samples/");
        strcpy(filename + strlen(filename), folderNames[sampleFolderNum]);
        strcpy(filename + strlen(filename), sampleNames[n]);
        fr = f_open(&fil, filename, FA_READ);
        if (FR_OK != fr)
        {
            printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
            flagError(ERROR_SD_OPEN);
            return;
        }
        clearError(ERROR_SD_OPEN);

        // below is my first proper stab at a WAV file parser. it's messy but it works as long as you supply a mono, 16-bit, 44.1kHz file. need to make it handle more edge cases in future

        // read WAV file header before reading data
        uint br; // bytes read
        bool foundDataChunk = false;
        char descriptorBuffer[4];
        uint8_t sizeBuffer[4];
        uint8_t sampleDataBuffer[FLASH_PAGE_SIZE];
        uint8_t sampleDataBuffer2[FLASH_PAGE_SIZE]; // required for stereo samples
        bool useSecondBuffer = false;
        bool isStereo = false;

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
                fr = f_read(&fil, useSecondBuffer ? sampleDataBuffer2 : sampleDataBuffer, bytesToRead, &br);

                if(strncmp(descriptorBuffer, "fmt ", 4) == 0) {
                    printf("FORMAT SECTION\n");
                    uint16_t fmtType;
                    uint16_t fmtChannels;
                    uint32_t fmtRate;
                    std::memcpy(&fmtType, &sampleDataBuffer[0], 2);
                    std::memcpy(&fmtChannels, &sampleDataBuffer[2], 2);
                    std::memcpy(&fmtRate, &sampleDataBuffer[4], 4);
                    printf("type=%d, channels=%d, rate=%d\n", fmtType, fmtChannels, fmtRate);
                    sampleRates[n] = fmtRate;
                    if(fmtChannels == 2) isStereo = true;
                }

                if (br == 0)
                    break;

                if (strncmp(descriptorBuffer, "data", 4) == 0)
                {
                    if (!foundDataChunk)
                    {
                        sampleStartPoints[n] = pageNum * FLASH_PAGE_SIZE;
                        sampleLengths[n] = isStereo ? chunkSize>>1 : chunkSize;
                        totalSize += sampleLengths[n];
                        printf("sample %d, start on page %d, length %d\n", n, sampleStartPoints[n], sampleLengths[n]);
                    }
                    foundDataChunk = true;

                    if (!dryRun)
                    {
                        if(!isStereo) {
                            // mono
                            writePageToFlash(sampleDataBuffer, FLASH_AUDIO_ADDRESS + pageNum * FLASH_PAGE_SIZE);
                            pageNum++;
                        } else {
                            // stereo
                            if(useSecondBuffer) {
                                int16_t thisSampleLeft;
                                int16_t thisSampleRight;
                                for(int i=0; i<FLASH_PAGE_SIZE; i+=2) {
                                    if(i < FLASH_PAGE_SIZE>>1) {
                                        std::memcpy(&thisSampleLeft, &sampleDataBuffer[i*2], 2);
                                        std::memcpy(&thisSampleRight, &sampleDataBuffer[i * 2 + 2], 2);
                                        //sampleDataBuffer[i] = sampleDataBuffer[i*2];
                                        //sampleDataBuffer[i+1] = sampleDataBuffer[i * 2 + 1];
                                    } else {
                                        std::memcpy(&thisSampleLeft, &sampleDataBuffer2[i * 2 - FLASH_PAGE_SIZE], 2);
                                        std::memcpy(&thisSampleRight, &sampleDataBuffer2[i * 2 + 2 - FLASH_PAGE_SIZE], 2);
                                        //sampleDataBuffer[i] = sampleDataBuffer2[i*2-FLASH_PAGE_SIZE];
                                        //sampleDataBuffer[i+1] = sampleDataBuffer2[i * 2 + 1 - FLASH_PAGE_SIZE];
                                    }
                                    int16_t thisSampleMono = (thisSampleLeft>>1) + (thisSampleRight>>1);
                                    std::memcpy(&sampleDataBuffer[i], &thisSampleMono, 2);
                                }
                                writePageToFlash(sampleDataBuffer, FLASH_AUDIO_ADDRESS + pageNum * FLASH_PAGE_SIZE);
                                pageNum ++;
                            }
                            useSecondBuffer = !useSecondBuffer;
                        }
                    }

                    //pageNum++;
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
            std::memcpy(metadataBuffer + SAMPLE_RATES + n * 4, &sampleRates[n], 4);
        }
        writePageToFlash(metadataBuffer, FLASH_AUDIO_METADATA_ADDRESS);

        loadSamplesFromFlash();
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
        flagError(ERROR_SD_MISSING);
        return;
    }
    clearError(ERROR_SD_MISSING);
    FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);
    if (FR_OK != fr)
    {
        printf("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
        flagError(ERROR_SD_MOUNT);
        return;
    }
    clearError(ERROR_SD_MOUNT);
    static FILINFO fno;
    int foundNum = -1;
    DIR dir;
    fr = f_opendir(&dir, path);
    if (FR_OK != fr)
    {
        printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
        flagError(ERROR_SD_OPEN);
        return;
    }
    clearError(ERROR_SD_OPEN);
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

// should be called after current sector has been found
void loadSettingsFromFlash()
{
    printf("load settings from sector %d\n", currentSettingsSector);
    int startPoint = FLASH_SECTOR_SIZE * currentSettingsSector + 12;
    externalClock = getBoolFromBuffer(flashData, startPoint + 4 * SETTING_CLOCK_MODE);
    crushChannel = getIntFromBuffer(flashData, startPoint + 4 * SETTING_CRUSH_CHANNEL);
    crush1 = !bitRead(crushChannel, 1);
    crush2 = !bitRead(crushChannel, 0);
    outputPulseLength = getIntFromBuffer(flashData, startPoint + 4 * SETTING_OUTPUT_PULSE_LENGTH);
    syncOutPpqnIndex = getIntFromBuffer(flashData, startPoint + 4 * SETTING_OUTPUT_PPQN);
    syncOutPpqn = ppqnValues[syncOutPpqnIndex];
    syncInPpqnIndex = getIntFromBuffer(flashData, startPoint + 4 * SETTING_INPUT_PPQN);
    syncInPpqn = ppqnValues[syncInPpqnIndex];
    // = getIntFromBuffer(flashData, startPoint + 4 * SETTING_PITCH_CURVE);
    // = getIntFromBuffer(flashData, startPoint + 4 * SETTING_INPUT_QUANTIZE);
    beatNum = getIntFromBuffer(flashData, startPoint + 4 * SETTING_BEAT);
    tempo = getIntFromBuffer(flashData, startPoint + 4 * SETTING_TEMPO);
    deltaT = 600000000 / tempo;
    tuplet = getIntFromBuffer(flashData, startPoint + 4 * SETTING_TUPLET);
    newNumSteps = getIntFromBuffer(flashData, startPoint + 4 * SETTING_TIME_SIG);
    numSteps = newNumSteps;
    // = getIntFromBuffer(flashData, startPoint + 4 * SETTING_OUTPUT);
    uint32_t output1Loaded = getIntFromBuffer(flashData, startPoint + 4 * SETTING_OUTPUT_1);
    uint32_t output2Loaded = getIntFromBuffer(flashData, startPoint + 4 * SETTING_OUTPUT_2);
    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        samples[i].output1 = bitRead(output1Loaded, i);
        samples[i].output2 = bitRead(output2Loaded, i);
    }
}

bool checkSettingsChange()
{
    //printf("checking settings changes...\n");
    bool anyChanged = false;
    int startPoint = FLASH_SECTOR_SIZE * currentSettingsSector + 12;

    if (externalClock != getBoolFromBuffer(flashData, startPoint + 4 * SETTING_CLOCK_MODE))
        anyChanged = true;
    if (crushChannel != getIntFromBuffer(flashData, startPoint + 4 * SETTING_CRUSH_CHANNEL))
        anyChanged = true;
    if (outputPulseLength != getIntFromBuffer(flashData, startPoint + 4 * SETTING_OUTPUT_PULSE_LENGTH))
        anyChanged = true;
    if (syncOutPpqnIndex != getIntFromBuffer(flashData, startPoint + 4 * SETTING_OUTPUT_PPQN))
        anyChanged = true;
    if (syncInPpqnIndex != getIntFromBuffer(flashData, startPoint + 4 * SETTING_INPUT_PPQN))
        anyChanged = true;
    // = getIntFromBuffer(flashData, startPoint + 4 * SETTING_PITCH_CURVE);
    // = getIntFromBuffer(flashData, startPoint + 4 * SETTING_INPUT_QUANTIZE);
    if (beatNum != getIntFromBuffer(flashData, startPoint + 4 * SETTING_BEAT))
        anyChanged = true;
    if (tempo != getIntFromBuffer(flashData, startPoint + 4 * SETTING_TEMPO))
        anyChanged = true;
    if (tuplet != getIntFromBuffer(flashData, startPoint + 4 * SETTING_TUPLET))
        anyChanged = true;
    if (newNumSteps != getIntFromBuffer(flashData, startPoint + 4 * SETTING_TIME_SIG))
        anyChanged = true;

    int32_t refOutput1 = 0;
    int32_t refOutput2 = 0;
    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        bitWrite(refOutput1, i, samples[i].output1);
        bitWrite(refOutput2, i, samples[i].output2);
    }
    if (refOutput1 != getIntFromBuffer(flashData, startPoint + 4 * SETTING_OUTPUT_1))
        anyChanged = true;
    if (refOutput2 != getIntFromBuffer(flashData, startPoint + 4 * SETTING_OUTPUT_2))
        anyChanged = true;

    return anyChanged;
}

void saveSettingsToFlash()
{
    if (checkSettingsChange())
    {
        uint saveNum = getIntFromBuffer(flashData, currentSettingsSector * FLASH_SECTOR_SIZE + 4) + 1;
        uint saveSector = (currentSettingsSector + 1) % 64; // temp, 64 should be properly defined somewhere
        currentSettingsSector = saveSector;
        printf("save settings to sector %d\n", currentSettingsSector);

        uint8_t buffer[FLASH_PAGE_SIZE];
        int32_t refCheckNum = CHECK_NUM;
        int32_t refExternalClock = externalClock ? 1 : 0;
        int32_t refOutput1 = 0;
        int32_t refOutput2 = 0;
        for (int i = 0; i < NUM_SAMPLES; i++)
        {
            bitWrite(refOutput1, i, samples[i].output1);
            bitWrite(refOutput2, i, samples[i].output2);
        }
        std::memcpy(buffer, &refCheckNum, 4);
        std::memcpy(buffer + 4, &saveNum, 4);
        std::memcpy(buffer + 12 + 4 * SETTING_CLOCK_MODE, &refExternalClock, 4);
        std::memcpy(buffer + 12 + 4 * SETTING_OUTPUT_1, &refOutput1, 4);
        std::memcpy(buffer + 12 + 4 * SETTING_OUTPUT_2, &refOutput2, 4);
        std::memcpy(buffer + 12 + 4 * SETTING_CRUSH_CHANNEL, &crushChannel, 4);
        std::memcpy(buffer + 12 + 4 * SETTING_OUTPUT_PULSE_LENGTH, &outputPulseLength, 4);
        std::memcpy(buffer + 12 + 4 * SETTING_OUTPUT_PPQN, &syncOutPpqnIndex, 4);
        std::memcpy(buffer + 12 + 4 * SETTING_INPUT_PPQN, &syncInPpqnIndex, 4);
        std::memcpy(buffer + 12 + 4 * SETTING_PITCH_CURVE, &refCheckNum, 4);    // temp
        std::memcpy(buffer + 12 + 4 * SETTING_INPUT_QUANTIZE, &refCheckNum, 4); // temp
        std::memcpy(buffer + 12 + 4 * SETTING_BEAT, &beatNum, 4);
        std::memcpy(buffer + 12 + 4 * SETTING_TEMPO, &tempo, 4);
        std::memcpy(buffer + 12 + 4 * SETTING_TUPLET, &tuplet, 4);
        std::memcpy(buffer + 12 + 4 * SETTING_TIME_SIG, &newNumSteps, 4);

        writePageToFlash(buffer, FLASH_DATA_ADDRESS + saveSector * FLASH_SECTOR_SIZE);
    }
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

void wipeFlash(bool flagReset) {
    uint8_t dataBuffer[FLASH_PAGE_SIZE] = {0};

    // overwrite previous data with zeros
    for(int i=FLASH_SETTINGS_START; i<=FLASH_SETTINGS_END; i++) {
        for(int j=0; j<FLASH_SECTOR_SIZE/FLASH_PAGE_SIZE; j++) {
            writePageToFlash(dataBuffer, FLASH_DATA_ADDRESS + FLASH_SETTINGS_START * FLASH_SECTOR_SIZE + i * FLASH_SECTOR_SIZE + j * FLASH_PAGE_SIZE);
        }
    }

    int32_t refCheckNum = CHECK_NUM;
    int32_t refZero = 0;
    int32_t refFactoryReset = flagReset ? RESET_NUM : 0;
    std::memcpy(dataBuffer, &refCheckNum, 4); // copy check number
    std::memcpy(dataBuffer + 4, &refZero, 4); // copy number zero, the incremental write number for wear levelling
    std::memcpy(dataBuffer + 8, &refFactoryReset, 4); // copy factory reset flag
    writePageToFlash(dataBuffer, FLASH_DATA_ADDRESS + FLASH_SETTINGS_START * FLASH_SECTOR_SIZE);
    currentSettingsSector = 0;
}

void checkFlashStatus()
{
    // settings are saved in a different sector each time to prevent wearing out the flash memory - if no valid sector is found, assume first boot and initialise everything
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
    if(foundValidSector) {
        currentSettingsSector = mostRecentValidSector;
        int thisResetCode = getIntFromBuffer(flashData + currentSettingsSector * FLASH_SECTOR_SIZE, 8);
        if(thisResetCode == RESET_NUM) {
            factoryResetCodeFound = true;
        }
    }
    // if (!foundValidSector || doFactoryReset)
    // {
    //     // no valid sectors, which means the flash needs to be initialised:
    //     uint8_t dataBuffer[FLASH_PAGE_SIZE] = {0};

    //     // overwrite previous data with zeros
    //     for(int i=FLASH_SETTINGS_START; i<=FLASH_SETTINGS_END; i++) {
    //         for(int j=0; j<FLASH_SECTOR_SIZE/FLASH_PAGE_SIZE; j++) {
    //             writePageToFlash(dataBuffer, FLASH_DATA_ADDRESS + FLASH_SETTINGS_START * FLASH_SECTOR_SIZE + i * FLASH_SECTOR_SIZE + j * FLASH_PAGE_SIZE);
    //         }
    //     }

    //     int32_t refCheckNum = CHECK_NUM;
    //     std::memcpy(dataBuffer, &refCheckNum, 4); // copy check number
    //     int32_t refZero = 0;
    //     std::memcpy(dataBuffer + 4, &refZero, 4); // copy number zero, the incremental write number for wear levelling
    //     writePageToFlash(dataBuffer, FLASH_DATA_ADDRESS + FLASH_SETTINGS_START * FLASH_SECTOR_SIZE);
    //     currentSettingsSector = 0;

    //     // create dummy noise samples (as emergency fallback in case SD card not present on reset)
    //     uint8_t audioBuffer[FLASH_PAGE_SIZE];
    //     uint32_t dummySampleLength = 1024;
    //     uint32_t dummySampleRate = 44100;
    //     for (int i = 0; i < NUM_SAMPLES; i++)
    //     {
    //         uint32_t dummySampleStart = i * dummySampleLength * 2; // *2 for 16-bit
    //         std::memcpy(audioBuffer + SAMPLE_START_POINTS + 4 * i, &dummySampleStart, 4);
    //         std::memcpy(audioBuffer + SAMPLE_LENGTHS + 4 * i, &dummySampleLength, 4);
    //         std::memcpy(audioBuffer + SAMPLE_RATES + 4 * i, &dummySampleRate, 4);
    //     }
    //     writePageToFlash(audioBuffer, FLASH_AUDIO_METADATA_ADDRESS);

    //     // fill audio buffer with random noise (for emergency fallback samples)
    //     for (int i = 0; i < dummySampleLength * 2 * NUM_SAMPLES; i += FLASH_PAGE_SIZE)
    //     {
    //         for (int j = 0; j < FLASH_PAGE_SIZE; j++)
    //         {
    //             audioBuffer[j] = rand() % 256; // random byte
    //         }
    //         writePageToFlash(audioBuffer, FLASH_AUDIO_ADDRESS + i);
    //     }

    //     // load first sample folder if available
    //     scanSampleFolders();
    //     loadSamplesFromSD();

    //     // TO DO: reset all (RAM) settings to default before saving to flash, otherwise factory reset is meaningless
    //     saveSettingsToFlash(); // save current (default) settings to flash
    //     loadDefaultBeats(); // load default beats into RAM
    //     saveBeatLocation = beatNum; // prevents weird behaviour
    //     saveAllBeats(); // save current (default) beats to flash
    // }
    // else
    // {
    //     currentSettingsSector = mostRecentValidSector;
    // }
}

void loadSamplesFromFlash()
{
    int startPos = 0;
    int storageOverflow = false;
    for (int n = 0; n < NUM_SAMPLES; n++)
    {
        uint sampleStart = getIntFromBuffer(flashAudioMetadata, SAMPLE_START_POINTS + n * 4);
        uint sampleLength = getIntFromBuffer(flashAudioMetadata, SAMPLE_LENGTHS + n * 4);
        uint sampleRate = getIntFromBuffer(flashAudioMetadata, SAMPLE_RATES + n * 4);
        printf("start %d, length %d\n", sampleStart, sampleLength);

        samples[n].length = sampleLength / 2; // divide by 2 to go from 8-bit to 16-bit
        samples[n].startPosition = sampleStart / 2;
        samples[n].sampleRate = sampleRate; // unnecessary?
        samples[n].sampleRateAdjustment = 0;
        if(sampleRate == 22050) samples[n].sampleRateAdjustment = 1;
        else if(sampleRate == 11025) samples[n].sampleRateAdjustment = 2; // could add more if wanted, could even maybe do non-integer multiples..?
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
        flagError(ERROR_SAMPLE_SIZE);
    } else {
        clearError(ERROR_SAMPLE_SIZE);
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

bool getBoolFromBuffer(const uint8_t *buffer, uint position)
{
    return getIntFromBuffer(buffer, position) != 0;
}

void gpioPulseLowCallback(int user_data)
{
    uint gpioNum = user_data;
    gpio_put(gpioNum, 0);
    bitWrite(gpioPulseStatuses, gpioNum, false);
}

void gpioPulseHighCallback(int user_data)
{
    uint gpioNum = user_data;
    if(!bitRead(gpioPulseStatuses,gpioNum)) {
        gpio_put(gpioNum, 1);
        bitWrite(gpioPulseStatuses, gpioNum, true);
        addEvent(outputPulseLength * 1000, gpioPulseLowCallback, gpioNum);
    }
}

void pulseGpio(uint gpioNum, uint16_t delayMicros)
{
    addEvent(delayMicros, gpioPulseHighCallback, gpioNum);
}

void loadDefaultBeats()
{
    for(int i=0; i<NUM_DEFAULT_BEATS; i++) {
        beats[i].numHits = 0;
        for(int j=0; j<NUM_SAMPLES; j++) {
            for(int k=0; k<32; k++) {
                if(bitRead(defaultBeats[i][j], 31-k)) {
                    beats[i].addHit(j, k*QUARTER_NOTE_STEPS/4, 255, 255, 0);
                }
            }
        }
    }
}

void testEventCallback(int user_data)
{
    printf("event! %d\n", user_data);
}
int highestQueueNum = 0;
int64_t eventManager(alarm_id_t id, void *user_data)
{
    //printf("event manager triggered, t=%llu\n", time_us_64());
    bool foundLastCurrentEvent = false;
    int numEventsFired = 0;
    for(int i=0; i<MAX_EVENTS && !foundLastCurrentEvent; i++) {
        if(events[i].time <= time_us_64()) {
            events[i].callback(events[i].user_data);
            numEventsFired ++;
        } else {
            foundLastCurrentEvent = true;
        }
    }
    for(int i=0; i<MAX_EVENTS; i++) {
        if(i+numEventsFired < MAX_EVENTS) {
            events[i] = events[i+numEventsFired];
            // if(events[i].time < UINT64_MAX) {
            //     if (i > highestQueueNum)
            //     {
            //         printf("highest queue num %d\n", i);
            //         highestQueueNum = i;
            //     }
            // }
        } else {
            events[i].time = UINT64_MAX;
        }
    }
    if(events[0].time < UINT64_MAX) {
        eventAlarm = add_alarm_in_us(events[0].time - time_us_64(), eventManager, NULL, true);
    }

    return 0;
}
void addEvent(uint64_t delay, void (*callback)(int), int user_data)
{
    bool foundQueuePlace = false;
    uint64_t time = time_us_64() + delay;
    for(int i=0; i<MAX_EVENTS && !foundQueuePlace; i++) {
        if(time < events[i].time) {
            // shift all other events one place back in queue and insert this event
            for(int j=MAX_EVENTS-1; j>i; j--) {
                events[j] = events[j-1];
            }
            events[i].time = time;
            events[i].callback = callback;
            events[i].user_data = user_data;
            foundQueuePlace = true;
            if(i==0) {
                // first in queue, need to cancel current alarm and reschedule it
                if(eventAlarm > 0) {
                    cancel_alarm(eventAlarm);
                    eventAlarm = 0; // probably a good thing to do?
                }
                eventAlarm = add_alarm_in_us(delay, eventManager, NULL, true);
                //assert(eventAlarm > 0);
            }
        }
    }
}
