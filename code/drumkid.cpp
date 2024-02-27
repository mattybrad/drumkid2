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
#include "pico/stdlib.h"
// #include "pico/multicore.h"
#include "pico/audio_i2s.h"
#include "pico/binary_info.h"
bi_decl(bi_3pins_with_names(PICO_AUDIO_I2S_DATA_PIN, "I2S DIN", PICO_AUDIO_I2S_CLOCK_PIN_BASE, "I2S BCK", PICO_AUDIO_I2S_CLOCK_PIN_BASE + 1, "I2S LRCK"));

// Drumkid classes
#include "Sample.h"
#include "Beat.h"
#include "drumkid.h"

// move to header file at some point
int chance = 0;
int chain = 0;
int zoom = 0;
int velRange = 0;
int velMidpoint = 0;
int drop = 0;
// NB order = NA,NA,NA,NA,tom,hat,snare,kick
uint8_t dropRef[9] = {
    0b00000000,
    0b00000001,
    0b00001001,
    0b00001101,
    0b00001111,
    0b00001110,
    0b00000110,
    0b00000100,
    0b00000000
};
int timeSignature = 4;
int newNumSteps = timeSignature * QUARTER_NOTE_STEPS;
int numSteps = timeSignature * QUARTER_NOTE_STEPS;
bool shift = false;
int tuplet = TUPLET_STRAIGHT;
float quarterNoteDivisionRef[NUM_TUPLET_MODES] = {QUARTER_NOTE_STEPS, 3*QUARTER_NOTE_STEPS/4, 5*QUARTER_NOTE_STEPS/8, 7*QUARTER_NOTE_STEPS/8}; // to do: calculate this automatically
float quarterNoteDivision = quarterNoteDivisionRef[tuplet];
int outputSample = 0;
int tempMissingCount = 0;
float nextHoldUpdateInc = 0;
float nextHoldUpdateDec = 0;

// SD card stuff
int sampleFolderNum = 0;
char sampleFolderName[255];

// tempo/sync stuff
bool syncInMode = false;
int syncInStep = 0;
int64_t syncInHistory[8] = {-1, -1, -1, -1, -1, -1, -1, -1};

int activeButton = -1;

bool sdSafeLoadTemp = false;
bool sdShowFolderTemp = false;

void generateTupletMap();

// convert zoom and step to velocity multiplier
uint8_t maxZoom = log(4*QUARTER_NOTE_STEPS)/log(2) + 1;
float zoomDivisor = 4096 / maxZoom;
uint8_t tupletMap[NUM_TUPLET_MODES][QUARTER_NOTE_STEPS * 4];
uint8_t stepVal[NUM_TUPLET_MODES][QUARTER_NOTE_STEPS * 4];
float getZoomMultiplier(int thisStep)
{
    int thisStepVal = stepVal[0][thisStep % (4 * QUARTER_NOTE_STEPS)];
    if(numSteps != 4 * QUARTER_NOTE_STEPS) {
        // bit complicated, this, but basically only the 4/4 time signature needs the "2" value (it sounds weird in other time signatures), and we also need to prevent an additional "1" value for 5/4 or 6/4, because it naturally occurs after 4 beats but again sounds wrong (sorry, bad explanation)
        if(thisStep > 0 && thisStepVal <= 2) thisStepVal = 3;
    }
    // I try not to hardcode stuff that could be done programatically, but this stuff is confusing, and should only change if I add, say, undectuplets, which isn't even a word
    if(thisStepVal == 4) {
        // tuplets don't make sense at this level and need to be disabled
        if(tuplet == TUPLET_TRIPLET) {
            thisStepVal = 5; // revert to quarter notes
        } else if(tuplet >= TUPLET_QUINTUPLET) {
            thisStepVal = 6;
        }
    } else if(thisStepVal == 5) {
        if(tuplet >= TUPLET_QUINTUPLET) {
            thisStepVal = 6;
        }
    }

    /*float vel = 1.0 + zoom - (float)thisStepVal;
    if (vel < 0.0)
        vel = 0.0;
    else if (vel > 1.0)
        vel = 1.0;*/
    int vel = 4096 + zoom - (thisStepVal<<12);
    if(vel < 0) vel = 0;
    else if(vel > 4096) vel = 4096;
    // printf("step %d, vel %f\n", thisStep, vel);
    return vel;
}

// temp figuring out proper hit generation
uint16_t step = 0; 
int64_t nextSyncOut = 0;
int64_t nextHitTime = 0; // in samples
int64_t currentTime = 0; // in samples
int scheduleAheadTime = SAMPLES_PER_BUFFER * 4; // in samples, was 0.02 seconds
uint16_t mainTimerInterval = 100; // us
struct hit
{
    bool waiting = false;
    int64_t time = 0;
    uint16_t velocity = 4096; // was float, 1.0
    int channel = 0;
    uint16_t step = 0;
};
const uint maxQueuedHits = 256; // queue fills up at ~300BPM when maxQueuedHits = 64 and everything else maxed out
struct hit tempHitQueue[maxQueuedHits];
uint16_t hitQueueIndex = 0;
void scheduleSyncOut() {
    if(step % QUARTER_NOTE_STEPS == 0) nextSyncOut = nextHitTime;
}
void scheduleHits()
{
    int64_t adjustedHitTime = nextHitTime;

    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        if (beats[beatNum].getHit(i, step, tuplet))
        {
            samples[i].queueHit(adjustedHitTime, step, 4096);
        }
        else
        {
            int randNum = rand() % 4070; // a bit less than 4096 in case of imperfect pots that can't quite reach max
            if (chance > randNum)
            {
                int zoomMult = getZoomMultiplier(step);
                int intVel = (rand() % velRange) + velMidpoint - velRange / 2;
                if (intVel < 0)
                    intVel = 0;
                else if (intVel > 4096)
                    intVel = 4096;
                intVel = (intVel * zoomMult) >> 12;
                if (intVel >= 8)
                {
                    samples[i].queueHit(adjustedHitTime, step, intVel); // temp vel
                }
            }
        }
    }
}
void scheduleHitsOld()
{
    // schedule sync out pulses using hit queue, special channel = -1
    if (step % QUARTER_NOTE_STEPS == 0)
    {
        tempHitQueue[hitQueueIndex].channel = -1;
        tempHitQueue[hitQueueIndex].waiting = true;
        tempHitQueue[hitQueueIndex].time = nextHitTime;
        tempHitQueue[hitQueueIndex].step = step;
        hitQueueIndex++;
        if (hitQueueIndex == maxQueuedHits)
            hitQueueIndex = 0;
    }

    // this function is all over the place and inefficient and just sort of proof of concept right now, but this is where the swing and slop (and "slide"..?) will be implemented
    float adjustedHitTime = nextHitTime;
    bool isSlide = true;
    // this next bit is causing issues! messing up nice smooth high-freq wave at high zoom values
    /*if(step % QUARTER_NOTE_STEPS == QUARTER_NOTE_STEPS / 2) {
        adjustedHitTime += ((float)analogReadings[POT_SWING] / 4095.0) * 0.25 * (60.0 / tempo);
    } else if(step % (QUARTER_NOTE_STEPS / 2) == QUARTER_NOTE_STEPS / 4) {
        adjustedHitTime += ((float)analogReadings[POT_SWING] / 4095.0) * 0.125 * (60.0 / tempo);
    }*/

    // this is also causing issues, removing for now, causing hiccups due to inconsistent pot readings? need a dead zone and maybe pot reading smoothing/average
    //if(!isSlide) adjustedHitTime += ((float)analogReadings[POT_SLOP] / 4095.0) * ((rand() / (double)(RAND_MAX)) * 2 - 1) * (60.0 / tempo);

    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        if (beats[beatNum].getHit(i, step, tuplet))
        {
            tempHitQueue[hitQueueIndex].channel = i;
            tempHitQueue[hitQueueIndex].waiting = true;
            if (i==2 && isSlide)
            {
                // this is also causing issues, removing for now, causing hiccups due to inconsistent pot readings? need a dead zone and maybe pot reading smoothing/average
                // adjustedHitTime += ((float)analogReadings[POT_SLOP] / 4095.0) * 0.25;
            }
            tempHitQueue[hitQueueIndex].time = adjustedHitTime;
            tempHitQueue[hitQueueIndex].step = step;
            tempHitQueue[hitQueueIndex].velocity = 4096; // was 1.0
            hitQueueIndex++;
            if (hitQueueIndex == maxQueuedHits)
                hitQueueIndex = 0;
        }
        else
        {
            // temp? early reimplementation of aleatoric stuff - currently only additive, doesn't remove hits
            int randNum = rand() % 4070; // a bit less than 4096 in case of imperfect pots that can't quite reach max
            if (chance > randNum)
            {
                int zoomMult = getZoomMultiplier(step);
                int intVel = (rand() % velRange) + velMidpoint - velRange / 2;
                if (intVel < 0)
                    intVel = 0;
                else if (intVel > 4096)
                    intVel = 4096;
                //float floatVel = zoomMult * (float)intVel / 4096.0;
                intVel = (intVel * zoomMult) >> 12;
                if (intVel >= 8/*floatVel >= 0.01*/)
                {
                    if (tempHitQueue[hitQueueIndex].waiting)
                    {
                        panic("hit queue full");
                    }
                    tempHitQueue[hitQueueIndex].channel = i;
                    tempHitQueue[hitQueueIndex].waiting = true;
                    // timing not yet compatible with slide
                    tempHitQueue[hitQueueIndex].time = adjustedHitTime;
                    tempHitQueue[hitQueueIndex].step = step;
                    tempHitQueue[hitQueueIndex].velocity = intVel;//floatVel;
                    hitQueueIndex++;
                    if (hitQueueIndex == maxQueuedHits)
                        hitQueueIndex = 0;
                }
            }
        }
    }
}
/*void scheduleHit(channel, time, step, vel) {

}*/
void nextHit()
{
    // bit messy right now, hard to account for all the tuplet stuff

    //nextHitTime += (60.0 / tempo) / quarterNoteDivision;
    nextHitTime += (2646000 / tempo) / quarterNoteDivision;
    step++;
    if (step % QUARTER_NOTE_STEPS >= (int)quarterNoteDivision)
        step += QUARTER_NOTE_STEPS - (step % QUARTER_NOTE_STEPS);
    if (step == numSteps)
    {
        numSteps = newNumSteps;
        if (activeButton == BUTTON_TIME_SIGNATURE)
            displayTimeSignature(); // temp? not 100% sure this is a good idea, maybe OTT, shows new time signature has come into effect
        step = 0;
    }
}
void scheduler()
{
    while (beatPlaying && nextHitTime < currentTime + scheduleAheadTime)
    {
        scheduleHits();
        scheduleSyncOut();
        nextHit();
    }
}

// main function, obviously
int main()
{
    stdio_init_all();
    time_init();
    alarm_pool_init_default(); // not sure if needed
    initZoom();

    printf("Drumkid V2\n");
    printf("zoom %d\n", maxZoom);

    checkFlashData();
    initGpio();

    // temp...
    getNthSampleFolder(0);
    loadSamplesFromSD();
    // ...end temp

    initSamplesFromFlash();
    initBeats();

    char startString[4] = "abc";
    updateLedDisplayAlpha(startString);

    struct audio_buffer_pool *ap = init_audio();

    add_repeating_timer_us(mainTimerInterval, mainTimerLogic, NULL, &mainTimer);

    // temp
    //beatPlaying = true;
    int timeIncrement = SAMPLES_PER_BUFFER; // in samples
    int64_t prevTime = 0;

    // audio buffer loop, runs forever
    while (true)
    {
        // something to do with audio that i don't fully understand
        struct audio_buffer *buffer = take_audio_buffer(ap, true);
        int16_t *bufferSamples = (int16_t *)buffer->buffer->bytes;

        prevTime = currentTime;

        bool dropHit[NUM_SAMPLES];
        for(int i=0; i<NUM_SAMPLES; i++) {
            dropHit[i] = !bitRead(dropRef[drop], i);
        }

        // update audio output
        for (uint i = 0; i < buffer->max_sample_count * 2; i += 2)
        {
            // sample updates go here
            int out1 = 0;
            int out2 = 0;
            if(currentTime + (i>>1) == nextSyncOut) {
                pulseGpio(SYNC_OUT, 1000);
            }
            for (int j = 0; j < NUM_SAMPLES; j++)
            {
                bool didTrigger = samples[j].update(currentTime + (i>>1)); // could be more efficient
                if(didTrigger) {
                    pulseGpio(TRIGGER_OUT_PINS[j], 1000);
                }
                if(!dropHit[j]) {
                    if(samples[j].output1) out1 += samples[j].value;
                    if(samples[j].output2) out2 += samples[j].value;
                }
            }

            bufferSamples[i] = out1 >> 2;
            bufferSamples[i + 1] = out2 >> 2;
        }
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);

        currentTime += timeIncrement;
        
        // scheduler call moved to here 23/2/24
        scheduler();

        if (sdSafeLoadTemp)
        {
            sdSafeLoadTemp = false;
            loadSamplesFromSD();
        }

        if(sdShowFolderTemp) {
            sdShowFolderTemp = false;
            getNthSampleFolder(sampleFolderNum);
        }

        for (int i = 0; i < NUM_SAMPLES; i++)
        {
            //bitWrite(singleLedData, i, samples[i].playing);
        }
    }
    return 0;
}

int64_t onGpioPulseLowTimeout(alarm_id_t id, void *user_data)
{
    uint gpioNum = (uint)user_data;
    gpio_put(gpioNum, 0);
    return 0;
}

int64_t onGpioPulseHighTimeout(alarm_id_t id, void *user_data)
{
    uint gpioNum = (uint)user_data;
    gpio_put(gpioNum, 1);
    return 0;
}

void pulseGpio(uint gpioNum, uint16_t pulseLengthMicros)
{
    gpio_put(gpioNum, 1);
    add_alarm_in_us(pulseLengthMicros, onGpioPulseLowTimeout, (void *)gpioNum, true);
}

int64_t onLedPulseLowTimeout(alarm_id_t id, void *user_data)
{
    uint ledNum = (uint)user_data;
    bitWrite(singleLedData, ledNum, false);
    return 0;
}

int64_t onLedPulseHighTimeout(alarm_id_t id, void *user_data)
{
    uint ledNum = (uint)user_data;
    bitWrite(singleLedData, ledNum, true);
    return 0;
}

void pulseLed(uint gpioNum, uint16_t delayMicros)
{
    add_alarm_in_us(delayMicros, onLedPulseHighTimeout, (void *)gpioNum, true);
    add_alarm_in_us(delayMicros + 50000, onLedPulseLowTimeout, (void *)gpioNum, true);
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
}

void initBeats()
{
    // new, simpler beat definition (but also prob temp?)
    beats[0].beatData[0] = 0b0000000000000000000000000000000100000000000000000000000000000001;
    beats[0].beatData[1] = 0b0000000000000001000000000000000000000000000000010000000000000000;
    beats[0].beatData[2] = 0b0000000100000001000000010000000100000001000000010000000100000001;

    beats[1].beatData[0] = 0b0000000000000000000000000000000100000000000000000000000000000001;
    beats[1].beatData[1] = 0b0000000000000001000000000000000000000000000000010000000000000000;
    beats[1].beatData[2] = 0b0111010101110101011101010111010101110101011101010111010101110101;
}

bool mainTimerLogic(repeating_timer_t *rt)
{

    updateLeds();
    updateShiftRegButtons();
    updateAnalog();
    updateSyncIn();

    // update effects etc
    //samplesPerStep = (int)(sampleRate * 7.5 / tempo); // 7.5 because it's 60/8, 8 subdivisions per quarter note..?

    // knobs / CV
    chance = analogReadings[POT_CHANCE] + analogReadings[CV_CHANCE] - 2048;
    if (chance < 0)
        chance = 0;
    else if (chance > 4096)
        chance = 4096;
    //chance = 4096; // temp!
    zoom = analogReadings[POT_ZOOM] + analogReadings[CV_ZOOM] - 2048;
    if (zoom < 0)
        zoom = 0;
    else if (zoom > 4096)
        zoom = 4096;
    //zoomInt = 4096; // temp!
    zoom = zoom * maxZoom;
    velRange = analogReadings[POT_RANGE];
    velMidpoint = analogReadings[POT_MIDPOINT] + analogReadings[CV_MIDPOINT] - 2048;
    if (velMidpoint < 0)
        velMidpoint = 0;
    else if (velMidpoint > 4096)
        velMidpoint = 4096;
    //velRange = 0; // temp
    //velMidpoint = 4096; // temp
    int pitchInt = analogReadings[POT_PITCH] + analogReadings[CV_TBC] - 4096; // temp, haven't figured out final range
    if (pitchInt < -2048)
        pitchInt = -2048;
    else if (pitchInt > 2048)
        pitchInt = 2048;
    else if(pitchInt == 0)
        pitchInt = 1; // need to do this to prevent division by zero...
    Sample::pitch = pitchInt; // temp...
    drop = analogReadings[POT_DROP] / 456; // gives range of 0 to 8

    return true;
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

    struct audio_buffer_pool *producer_pool = audio_new_producer_pool(&producer_format, 3,
                                                                      SAMPLES_PER_BUFFER); // todo correct size
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

// Borrowerd from StackOverflow
char getNthDigit(int x, int n)
{
    while (n--)
    {
        x /= 10;
    }
    return x % 10;
}

void updateLedDisplay(int num)
{
    for (int i = 0; i < 4; i++)
    {
        sevenSegData[3-i] = sevenSegCharacters[getNthDigit(num, i)];
    }
}

void updateLedDisplayAlpha(char* word)
{
    bool foundWordEnd = false;
    for(int i=0; i<4; i++) {
        int alphaNum = 0;
        bool skipChar = false;
        if(word[i] == '\0') {
            foundWordEnd = true;
        } else {
            alphaNum = word[i] - 97; // temp, only lower case alphabet for now
            if(alphaNum < 0 || alphaNum > 122) {
                skipChar = true;
            }
        }
        if(foundWordEnd || skipChar) {
            sevenSegData[i] = 0b00000000;
        } else {
            sevenSegData[i] = sevenSegAlphaCharacters[alphaNum];
        }
        
    }
}

void handleButtonChange(int buttonNum, bool buttonState)
{
    if (shift && buttonNum != BUTTON_SHIFT)
        buttonNum += 16;
    if (buttonState)
    {
        switch (buttonNum)
        {
        case BUTTON_SHIFT:
            shift = true;
            break;
        case BUTTON_START_STOP:
            numSteps = newNumSteps;
            if (activeButton == BUTTON_TIME_SIGNATURE)
                displayTimeSignature();
            beatPlaying = !beatPlaying;
            if (beatPlaying)
            {
                // handle beat start
                for (int i = 0; i < maxQueuedHits; i++)
                {
                    tempHitQueue[i].waiting = false;
                }
                hitQueueIndex = 0;
                step = 0;
                nextHitTime = currentTime;
                scheduler();
            }
            else
            {
                // handle beat stop
                for (int i = 0; i < maxQueuedHits; i++)
                {
                    tempHitQueue[i].waiting = false;
                }
                hitQueueIndex = 0;

                numSteps = newNumSteps;
                if (activeButton == BUTTON_TIME_SIGNATURE)
                    displayTimeSignature();
            }
            break;
        case BUTTON_TAP_TEMPO:
            break;
        case BUTTON_INC:
            nextHoldUpdateInc = currentTime + SAMPLE_RATE;
            handleIncDec(true);
            break;
        case BUTTON_DEC:
            nextHoldUpdateDec = currentTime + SAMPLE_RATE;
            handleIncDec(false);
            break;
        case BUTTON_CONFIRM:
            handleYesNo(true);
            break;
        case BUTTON_CANCEL:
            handleYesNo(false);
            break;
        case BUTTON_LOAD_SAMPLES:
            activeButton = BUTTON_LOAD_SAMPLES;
            sdShowFolderTemp = true;
            //displaySamplePack();
            //sdSafeLoadTemp = true;
            break;
        case BUTTON_MANUAL_TEMPO:
            activeButton = BUTTON_MANUAL_TEMPO;
            displayTempo();
            break;
        case BUTTON_TIME_SIGNATURE:
            activeButton = BUTTON_TIME_SIGNATURE;
            displayTimeSignature();
            break;
        case BUTTON_OUTPUT:
            activeButton = BUTTON_OUTPUT;
            break;
        case BUTTON_TUPLET:
            activeButton = BUTTON_TUPLET;
            displayTuplet();
            break;
        case BUTTON_BEAT:
            activeButton = BUTTON_BEAT;
            displayBeat();
            break;
        default:
            printf("(button not assigned)\n");
        }
    }
    else if (!buttonState)
    {
        switch (buttonNum)
        {
        case BUTTON_SHIFT:
            shift = false;
            break;
        }
    }
}

void handleIncDec(bool isInc)
{
    switch (activeButton)
    {
    case BUTTON_MANUAL_TEMPO:
        tempo += isInc ? 1 : -1;
        if (tempo > 9999)
            tempo = 9999;
        else if (tempo < 10)
            tempo = 10;
        // todo: press and hold function
        displayTempo();
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
        beatNum += isInc ? 1 : -1;
        if (beatNum > 3)
            beatNum = 3;
        if (beatNum < 0)
            beatNum = 0;
        displayBeat();
        break;

    case BUTTON_TUPLET:
        tuplet += isInc ? 1 : -1;
        if (tuplet < 0)
            tuplet = 0;
        else if (tuplet >= NUM_TUPLET_MODES)
            tuplet = NUM_TUPLET_MODES - 1;
        quarterNoteDivision = quarterNoteDivisionRef[tuplet];
        displayTuplet();
        break;

    case BUTTON_OUTPUT:
        outputSample += isInc ? 1 : -1;
        if(outputSample < 0) outputSample = 0;
        else if(outputSample > NUM_SAMPLES-1) outputSample = NUM_SAMPLES-1;
        updateLedDisplay(outputSample);
        break;

    case BUTTON_LOAD_SAMPLES:
        sampleFolderNum += isInc ? 1 : -1;
        if (sampleFolderNum < 0)
            sampleFolderNum = 0;
        else if (sampleFolderNum > 3)
            sampleFolderNum = 3;
        sdShowFolderTemp = true;
        break;
    }
}

void handleYesNo(bool isYes) {
    switch(activeButton)
    {
        case BUTTON_OUTPUT:
            samples[outputSample].output1 = isYes;
            break;
        
        case BUTTON_LOAD_SAMPLES:
            sdSafeLoadTemp = true;
            break;
    }
}

void displayTempo()
{
    updateLedDisplay((int)tempo);
}

void displayTimeSignature()
{
    int tempTimeSig = 4 + 10 * newNumSteps / QUARTER_NOTE_STEPS;
    if (newNumSteps != numSteps)
        tempTimeSig += 8000; // temp, shows that the new signature is not yet active
    updateLedDisplay(tempTimeSig);
}

void displayTuplet()
{
    updateLedDisplay(tuplet*2+1);
}

void displayBeat()
{
    updateLedDisplay(beatNum);
}

void updateShiftRegButtons()
{
    // update shift register
    if (shiftRegInPhase == 0)
    {
        gpio_put(LOAD_165, 0);
    }
    else if (shiftRegInPhase == 1)
    {
        gpio_put(LOAD_165, 1);
    }
    else if (shiftRegInPhase == 2)
    {
        if (microsSinceChange[shiftRegInLoopNum] > 20000)
        {
            bool buttonState = gpio_get(DATA_165);
            if (buttonState != buttonStableStates[shiftRegInLoopNum])
            {
                buttonStableStates[shiftRegInLoopNum] = buttonState;
                microsSinceChange[shiftRegInLoopNum] = 0;
                printf("button %d: %d\n", shiftRegInLoopNum, buttonState ? 1 : 0);
                handleButtonChange(shiftRegInLoopNum, buttonState);
            }
        }
        else
        {
            microsSinceChange[shiftRegInLoopNum] += 20 * mainTimerInterval; // total guess at us value for a full shift reg cycle, temporary
        }
        gpio_put(CLOCK_165, 0);
    }
    else if (shiftRegInPhase == 3)
    {
        gpio_put(CLOCK_165, 1);
    }
    shiftRegInPhase++;
    if (shiftRegInPhase == 4)
    {
        shiftRegInLoopNum++;
        if (shiftRegInLoopNum < 16)
        {
            shiftRegInPhase = 2;
        }
        else
        {
            shiftRegInPhase = 0;
            shiftRegInLoopNum = 0;
        }
    }

    // handle inc/dec hold - maybe should be in another function..?
    if (buttonStableStates[BUTTON_INC])
    {
        if (currentTime > nextHoldUpdateInc)
        {
            nextHoldUpdateInc = currentTime + (SAMPLE_RATE >> 3);
            handleIncDec(true);
        }
    }
    else if (buttonStableStates[BUTTON_DEC])
    {
        if (currentTime > nextHoldUpdateDec)
        {
            nextHoldUpdateDec = currentTime + (SAMPLE_RATE >> 3);
            handleIncDec(false);
        }
    }
}

void updateAnalog()
{
    if (analogPhase == 0)
    {
        gpio_put(MUX_ADDR_A, bitRead(analogLoopNum, 0));
        gpio_put(MUX_ADDR_B, bitRead(analogLoopNum, 1));
        gpio_put(MUX_ADDR_C, bitRead(analogLoopNum, 2));
    }
    else if (analogPhase == 1)
    {
        adc_select_input(0);
        analogReadings[analogLoopNum] = 4095 - adc_read();
        adc_select_input(1);
        analogReadings[analogLoopNum + 8] = 4095 - adc_read();
    }

    analogPhase++;
    if (analogPhase == 2)
    {
        analogPhase = 0;
        analogLoopNum++;
        if (analogLoopNum == 8)
        {
            analogLoopNum = 0;

            // temp
            for (int i = 0; i < 16; i++)
            {
                if (buttonStableStates[i])
                {
                    // updateLedDisplay(analogReadings[i]);
                }
            }
        }
    }
}

void getNthSampleFolder(int n) {
    printf("get nth sample folder...\n");
    sd_card_t *pSD = sd_get_by_num(0);
    FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);
    if (FR_OK != fr)
        panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    static FILINFO fno;
    UINT i;
    int foundNum = -1;
    char path[256];
    strcpy(path, "samples/");
    DIR dir;
    fr = f_opendir(&dir, path);
    if (FR_OK != fr)
    {
        printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
        return;
    }
    for (;;)
    {
        fr = f_readdir(&dir, &fno); /* Read a directory item */
        if (fr != FR_OK || fno.fname[0] == 0)
            break; /* Break on error or end of dir */
        if (fno.fattrib & AM_DIR)
        { /* It is a directory */
            foundNum ++;
            i = strlen(path);
            sprintf(&path[i], "/%s", fno.fname); // don't totally understand this
            path[i] = 0;
            if(foundNum == n) {
                printf("sample folder: %s\n", fno.fname);
                strcpy(sampleFolderName, fno.fname);
                updateLedDisplayAlpha(fno.fname);
                break;
            }
        }
        else
        { /* It is a file. */
            printf("%s/%s\n", path, fno.fname);
        }
    }
    f_closedir(&dir);
}

void loadSamplesFromSD()
{
    uint totalSize = 0;
    bool dryRun = false;
    int pageNum = 0;
    const char *sampleNames[NUM_SAMPLES] = {"/kick.wav", "/snare.wav", "/closedhat.wav", "/tom.wav"};
    uint32_t sampleStartPoints[NUM_SAMPLES] = {0};
    uint32_t sampleLengths[NUM_SAMPLES] = {0};

    for (int n = 0; n < NUM_SAMPLES; n++)
    {
        sd_card_t *pSD = sd_get_by_num(0);
        FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);
        if (FR_OK != fr)
            panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
        FIL fil;
        char filename[255];
        strcpy(filename, "samples/");
        strcpy(filename+strlen(filename), sampleFolderName);
        strcpy(filename+strlen(filename), sampleNames[n]);
        fr = f_open(&fil, filename, FA_READ);
        if (FR_OK != fr)
        {
            printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
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

                    if(!dryRun) {
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

    if(!dryRun) {
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
        writePageToFlash(metadataBuffer, FLASH_DATA_ADDRESS);

        initSamplesFromFlash();
    }
}

void initSamplesFromFlash()
{
    int startPos = 0;
    for (int n = 0; n < NUM_SAMPLES; n++)
    {
        uint sampleStart = getIntFromBuffer(flashData, SAMPLE_START_POINTS + n * 4);
        uint sampleLength = getIntFromBuffer(flashData, SAMPLE_LENGTHS + n * 4);
        printf("start %d, length %d\n", sampleStart, sampleLength);

        samples[n].length = sampleLength / 2; // divide by 2 to go from 8-bit to 16-bit
        samples[n].startPosition = sampleStart / 2;
        printf("sample %d, start %d, length %d\n",n,samples[n].startPosition,samples[n].length);
        samples[n].position = samples[n].length;
        samples[n].positionAccurate = samples[n].length << Sample::LERP_BITS;

        for (int i = 0; i < samples[n].length && i < sampleLength / 2; i++)
        {
            int16_t thisSample = flashAudio[i * 2 + 1 + sampleStart] << 8 | flashAudio[i * 2 + sampleStart];
            Sample::sampleData[sampleStart/2+i] = thisSample;
        }

        // temp
        samples[n].output1 = true;
        samples[n].output2 = n == 2;
    }
}

void updateLeds()
{
    if (shiftRegOutLoopNum == 0 && shiftRegOutPhase == 0)
    {
        // copy LED data so it doesn't change during serial transfer
        for (int i = 0; i < 4; i++)
        {
            storedLedData[i] = (sevenSegData[3-i] << 8) + 0b11110000 + singleLedData;
            bitWrite(storedLedData[i], 4 + i, 0);
        }
        gpio_put(LATCH_595, 0);
    }
    if (shiftRegOutLoopNum < 16)
    {
        switch (shiftRegOutPhase)
        {
        case 0:
            gpio_put(
                DATA_595,
                bitRead(storedLedData[sevenSegCharNum], shiftRegOutLoopNum));
            gpio_put(CLOCK_595, 0);
            break;

        case 1:
            gpio_put(CLOCK_595, 1);
            break;

        case 2:
            gpio_put(CLOCK_595, 0);
            break;
        }
        shiftRegOutPhase++;
        if (shiftRegOutPhase == 3)
        {
            shiftRegOutPhase = 0;
            shiftRegOutLoopNum++;
        }
    }
    else
    {
        gpio_put(LATCH_595, 1);
        shiftRegOutLoopNum = 0;
        shiftRegOutPhase = 0;
        sevenSegCharNum = (sevenSegCharNum + 1) % 4;
    }
}

void print_buf(const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; ++i)
    {
        printf("%02x", buf[i]);
        if (i % 16 == 15)
            printf("\n");
        else
            printf(" ");
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

int32_t getIntFromBuffer(const uint8_t *buffer, uint position)
{
    int32_t thisInt = buffer[position] | buffer[position + 1] << 8 | buffer[position + 2] << 16 | buffer[position + 3] << 24;
    return thisInt;
}

float getFloatFromBuffer(const uint8_t *buffer, uint position)
{
    union FloatConverter
    {
        uint8_t bytes[4];
        float floatVal;
    };
    FloatConverter converter;
    for (int i = 0; i < 4; i++)
    {
        converter.bytes[i] = buffer[position + i];
    }
    printf("float bytes converted: %d %d %d %d to %f\n", buffer[position], buffer[position + 1], buffer[position + 2], buffer[position + 3], converter.floatVal);
    return converter.floatVal;
}

void checkFlashData()
{
    int checkNum = getIntFromBuffer(flashData, DATA_CHECK);
    if (checkNum == CHECK_NUM)
    {
        printf("Flash check successful\n");
    }
    else
    {
        printf("Flash check failed: %d\nInitialising flash\n", checkNum);
        // flash data not valid, likely first time booting the module - initialise flash data (eventually required SD at this point?)
        uint8_t buffer[FLASH_PAGE_SIZE];
        int32_t refCheckNum = CHECK_NUM;
        std::memcpy(buffer, &refCheckNum, 4);       // copy check number
        std::memcpy(buffer + VAR_TEMPO, &tempo, 4); // copy tempo float
        printf("float bytes written to buffer: %d %d %d %d\n", buffer[VAR_TEMPO], buffer[VAR_TEMPO + 1], buffer[VAR_TEMPO + 2], buffer[VAR_TEMPO + 3]);
        uint32_t dummySampleLength = 1024;
        for (int i = 0; i < NUM_SAMPLES; i++)
        {
            uint32_t dummySampleStart = i * dummySampleLength * 2;
            std::memcpy(buffer + SAMPLE_START_POINTS + 4 * i, &dummySampleStart, 4);
            std::memcpy(buffer + SAMPLE_LENGTHS + 4 * i, &dummySampleLength, 4);
        }
        writePageToFlash(buffer, FLASH_DATA_ADDRESS);

        // fill audio buffer with random noise
        for (int i = 0; i < dummySampleLength * 2 * NUM_SAMPLES; i += FLASH_PAGE_SIZE)
        {
            for (int j = 0; j < FLASH_PAGE_SIZE; j++)
            {
                buffer[j] = rand() % 256; // random byte
            }
            writePageToFlash(buffer, FLASH_AUDIO_ADDRESS + i);
        }
    }
}

uint32_t microsSinceSyncInChange = mainTimerInterval;
bool syncInStableState = false;
void updateSyncIn()
{
    if (microsSinceSyncInChange > 1000)
    {
        bool syncInState = !gpio_get(SYNC_IN);
        if (syncInState != syncInStableState)
        {
            syncInStableState = syncInState;
            microsSinceSyncInChange = 0;
            if (syncInState)
            {
                syncInMode = true; // temp, once a sync signal is received, module stays in "sync in" mode until reboot
                // doStep();
                float historyTally = 0.0;
                int validHistoryCount = 0;
                for (int i = 7; i >= 1; i--)
                {
                    syncInHistory[i] = syncInHistory[i - 1];
                }
                syncInHistory[0] = currentTime;
                for (int i = 0; i < 7; i++)
                {
                    if (syncInHistory[i] >= 0 && syncInHistory[i + 1] >= 0)
                    {
                        float timeDiff = syncInHistory[i] - syncInHistory[i + 1];
                        if (timeDiff >= 0 && timeDiff < 10)
                        {
                            // arbitrary 10-second limit for now
                            // printf("%d %f\t", i, timeDiff);
                            validHistoryCount++;
                            historyTally += timeDiff;
                        }
                    }
                }
                if (validHistoryCount > 0)
                {
                    float testSyncTempo = 60.0 * validHistoryCount / historyTally;
                    // printf("test sync tempo %f\n", testSyncTempo);
                    tempo = testSyncTempo;
                }
                pulseLed(3, 50000);
            }
        }
    }
    else
    {
        microsSinceSyncInChange += 100;
    }
}

void initZoom() {
    generateTupletMap();
    for (int i = 0; i < QUARTER_NOTE_STEPS * 4; i++)
    {
        bool foundLevel = false;
        for (int j = 0; j < maxZoom && !foundLevel; j++)
        {
            if ((i % ((QUARTER_NOTE_STEPS * 4) >> j)) == 0)
            {
                foundLevel = true;
                stepVal[0][i] = j + 1;
            }
        }
        printf("%d ", stepVal[0][i]);
    }
}

void generateTupletMap() {
    printf("\n");
    for(int q=0; q<4; q++) {
        for(int i=0; i<NUM_TUPLET_MODES; i++) {
            float multiplier = QUARTER_NOTE_STEPS / quarterNoteDivisionRef[i];
            for(int j=0; j<QUARTER_NOTE_STEPS; j++) {
                int thisStep = static_cast<int>(round(j * multiplier));
                if(thisStep >= QUARTER_NOTE_STEPS) thisStep = QUARTER_NOTE_STEPS - 1;
                tupletMap[i][j + q * QUARTER_NOTE_STEPS] = thisStep + q * QUARTER_NOTE_STEPS;
            }
            printf("\n");
        }
    }
}