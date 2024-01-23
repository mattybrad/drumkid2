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
float zoom = 0.0;
int velRange = 0;
int velMidpoint = 0;
int newNumSteps = 64;
int numSteps = 64; // 64=4/4, 48=3/4, etc
bool shift = false;
int tuplet = TUPLET_STRAIGHT;
float quarterNoteDivision = 16.0;
float quarterNoteDivisionRef[8] = {16, 16, 12, 12, 10, 10, 14, 14};
uint32_t tempTime = 0;
int outputSample = 0;

// tempo/sync stuff
bool syncInMode = false;
int syncInStep = 0;
float syncInHistory[8] = {-1, -1, -1, -1, -1, -1, -1, -1};

int activeButton = -1;

bool sdSafeLoadTemp = false;

// convert zoom and step to velocity multiplier
// currently set up for 4/4 - for other time sigs, there should be no "2" value, and "1" should only happen once (not %-looped as happens now)
int stepVal[MAX_BEAT_STEPS] = {
    1,
    7,
    6,
    7,
    5,
    7,
    6,
    7,
    4,
    7,
    6,
    7,
    5,
    7,
    6,
    7,
    3,
    7,
    6,
    7,
    5,
    7,
    6,
    7,
    4,
    7,
    6,
    7,
    5,
    7,
    6,
    7,
    2,
    7,
    6,
    7,
    5,
    7,
    6,
    7,
    4,
    7,
    6,
    7,
    5,
    7,
    6,
    7,
    3,
    7,
    6,
    7,
    5,
    7,
    6,
    7,
    4,
    7,
    6,
    7,
    5,
    7,
    6,
    7,
};
float getZoomMultiplier(int thisStep)
{
    float vel = 1.0 + zoom - (float)stepVal[thisStep % 64];
    if (vel < 0.0)
        vel = 0.0;
    else if (vel > 1.0)
        vel = 1.0;
    // printf("step %d, vel %f\n", thisStep, vel);
    return vel;
}

// temp figuring out proper hit generation
uint32_t step = 0;                // measured in 64th-notes, i.e. 16 steps per quarter note
float nextHitTime = 0;            // s
float currentTime = 0;            // s
float scheduleAheadTime = 0.01;    // s
uint16_t schedulerInterval = 5; // ms
uint16_t mainTimerInterval = 100; // us
struct hit
{
    bool waiting = false;
    float time = 0.0;
    float velocity = 1.0;
    int channel = 0;
    uint16_t step = 0;
};
const uint maxQueuedHits = 256; // queue fills up at ~300BPM when maxQueuedHits = 64 and everything else maxed out
struct hit tempHitQueue[maxQueuedHits];
uint16_t hitQueueIndex = 0;
void scheduleHits()
{
    // schedule sync out pulses using hit queue, special channel = -1
    if (step % 16 == 0)
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
    if(step % 16 == 8) {
        adjustedHitTime += ((float)analogReadings[POT_SWING] / 4095.0) * 0.25 * (60.0 / tempo);
    } else if(step % 8 == 4) {
        adjustedHitTime += ((float)analogReadings[POT_SWING] / 4095.0) * 0.125 * (60.0 / tempo);
    }
    if(!isSlide) adjustedHitTime += ((float)analogReadings[POT_SLOP] / 4095.0) * ((rand() / (double)(RAND_MAX)) * 2 - 1) * (60.0 / tempo);

    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        if (beats[beatNum].hits[i][step % 64])
        {
            tempHitQueue[hitQueueIndex].channel = i;
            tempHitQueue[hitQueueIndex].waiting = true;
            if (i==2 && isSlide)
            {
                adjustedHitTime += ((float)analogReadings[POT_SLOP] / 4095.0) * 0.25;
            }
            tempHitQueue[hitQueueIndex].time = adjustedHitTime;
            tempHitQueue[hitQueueIndex].step = step;
            tempHitQueue[hitQueueIndex].velocity = 1.0;
            hitQueueIndex++;
            if (hitQueueIndex == maxQueuedHits)
                hitQueueIndex = 0;
        }
        else
        {
            // temp? early reimplementation of aleatoric stuff - currently only additive, doesn't remove hits
            int randNum = rand() % 4080; // a bit less than 4096 in case of imperfect pots that can't quite reach max
            if (chance > randNum)
            {
                float zoomMult = getZoomMultiplier(step);
                // float zoomMult = step % 4 == 0 ? 1.0 : 0.0;
                int intVel = (rand() % velRange) + velMidpoint - velRange / 2;
                if (intVel < 0)
                    intVel = 0;
                else if (intVel > 4096)
                    intVel = 4096;
                float floatVel = zoomMult * (float)intVel / 4096.0;
                if (floatVel >= 0.01)
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
                    tempHitQueue[hitQueueIndex].velocity = floatVel;
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
    // nextHitTime += (60.0 / tempo) / 16.0; // for 16 steps per quarter note
    nextHitTime += (60.0 / tempo) / quarterNoteDivision;
    step++;
    if (step % 16 >= (int)quarterNoteDivision)
        step += 16 - (step % 16);
    if (step == numSteps)
    {
        numSteps = newNumSteps;
        if (activeButton == BUTTON_TIME_SIGNATURE)
            displayTimeSignature(); // temp? not 100% sure this is a good idea, maybe OTT, shows new time signature has come into effect
        step = 0;
    }
}
bool scheduler(repeating_timer_t *rt)
{
    // printf("current time = %f\n", currentTime);
    while (beatPlaying && nextHitTime < currentTime + scheduleAheadTime)
    {
        scheduleHits();
        nextHit();
    }
    return true;
}

// main function, obviously
int main()
{
    stdio_init_all();
    time_init();
    alarm_pool_init_default();

    printf("Drumkid V2\n");

    checkFlashData();
    initGpio();
    initSamplesFromFlash();
    initBeats();

    tempo = 320;

    struct audio_buffer_pool *ap = init_audio();

    updateLedDisplay(1234);
    add_repeating_timer_us(mainTimerInterval, mainTimerLogic, NULL, &mainTimer);
    add_repeating_timer_ms(schedulerInterval, scheduler, NULL, &schedulerTimer);

    // temp
    float timeIncrement = (float)SAMPLES_PER_BUFFER / sampleRate;
    float prevTime = 0;

    // audio buffer loop, runs forever
    while (true)
    {
        // something to do with audio that i don't fully understand
        struct audio_buffer *buffer = take_audio_buffer(ap, true);
        int16_t *bufferSamples = (int16_t *)buffer->buffer->bytes;

        // before populating buffer, check queue for impending hits and pass timing data to sample objects
        for (int i = 0; i < maxQueuedHits; i++)
        {
            float delaySeconds;
            int delaySamples;
            if(tempHitQueue[i].waiting && tempHitQueue[i].time <= currentTime + timeIncrement * 2)
            {
                // calculate time (in samples) before next hit on same channel
                delaySeconds = tempHitQueue[i].time - currentTime;
                if(delaySeconds < 0.0) delaySeconds = 0.0;
                delaySamples = delaySeconds * sampleRate;
                
                if (tempHitQueue[i].channel == -1)
                {
                    pulseGpio(SYNC_OUT, delaySeconds * 1000000);
                    pulseLed(3, delaySeconds * 1000000);
                }
                else
                {
                    pulseGpio(TRIGGER_OUT_PINS[tempHitQueue[i].channel], delaySeconds * 1000000);

                    // don't pass data to sample if already waiting, should prioritise next hit rather than more distant hits in queue(?)
                    if (samples[tempHitQueue[i].channel].delaySamples == 0)
                    {
                        samples[tempHitQueue[i].channel].delaySamples = delaySamples + 1; // +1 because delaySamples is decremented at start of update (bit hacky, fenceposts, y'know)
                        samples[tempHitQueue[i].channel].waiting = true;
                        samples[tempHitQueue[i].channel].nextVelocity = tempHitQueue[i].velocity;
                    }
                }
                tempHitQueue[i].waiting = false;
            }
        }

        prevTime = currentTime;

        // update audio output
        for (uint i = 0; i < buffer->max_sample_count * 2; i += 2)
        {
            // sample updates go here
            int out1 = 0;
            int out2 = 0;
            for (int j = 0; j < NUM_SAMPLES; j++)
            {
                samples[j].update();
                if(samples[j].output1) out1 += samples[j].value;
                if(samples[j].output2) out2 += samples[j].value;
            }

            bufferSamples[i] = out1 >> 2;
            bufferSamples[i + 1] = out2 >> 2;
        }
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);

        currentTime += timeIncrement;
        // printf("current time %f\n", currentTime);

        if (sdSafeLoadTemp)
        {
            sdSafeLoadTemp = false;
            loadSamplesFromSD();
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

void pulseGpio(uint gpioNum, uint16_t delayMicros)
{
    add_alarm_in_us(delayMicros, onGpioPulseHighTimeout, (void *) gpioNum, true);
    add_alarm_in_us(delayMicros + 20000, onGpioPulseLowTimeout, (void *) gpioNum, true);
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
    // temporarily defining preset beats here
    beats[0].addHit(0, 0);
    beats[0].addHit(0, 32);
    beats[0].addHit(1, 16);
    beats[0].addHit(1, 48);
    beats[0].addHit(1, 56);
    beats[0].addHit(2, 0);
    beats[0].addHit(2, 8);
    beats[0].addHit(2, 16);
    beats[0].addHit(2, 24);
    beats[0].addHit(2, 32);
    beats[0].addHit(2, 40);
    beats[0].addHit(2, 48);
    beats[0].addHit(2, 56);

    beats[1].addHit(0, 0);
    beats[1].addHit(0, 32);
    beats[1].addHit(1, 16);
    beats[1].addHit(1, 48);
    beats[1].addHit(1, 56);

    beats[2].addHit(0, 0);
    beats[2].addHit(0, 16);
    beats[2].addHit(0, 32);
    beats[2].addHit(0, 48);
}

bool mainTimerLogic(repeating_timer_t *rt)
{

    tempTime += mainTimerInterval;

    updateLeds();
    updateShiftRegButtons();
    updateAnalog();
    updateSyncIn();

    // update effects etc
    samplesPerStep = (int)(sampleRate * 7.5 / tempo); // 7.5 because it's 60/8, 8 subdivisions per quarter note..?

    // knobs
    chance = analogReadings[POT_CHANCE] + analogReadings[CV_CHANCE] - 2048;
    if (chance < 0)
        chance = 0;
    else if (chance > 4096)
        chance = 4096;
    int zoomInt = analogReadings[POT_ZOOM] + analogReadings[CV_ZOOM] - 2048;
    if (zoomInt < 0)
        zoomInt = 0;
    else if (zoomInt > 4096)
        zoomInt = 4096;
    zoom = zoomInt / 585.15; // gives range of 0.0 to 7.0 for complicated reasons
    velRange = analogReadings[POT_RANGE];
    velMidpoint = analogReadings[POT_MIDPOINT] + analogReadings[CV_MIDPOINT] - 2048;
    if (velMidpoint < 0)
        velMidpoint = 0;
    else if (velMidpoint > 4096)
        velMidpoint = 4096;

    // CV
    // samples[1].speed = 0.25 + 4.0 * ((float)analogReadings[14]) / 4095.0;

    return true;
}

// Borrowed/adapted from pico-playground
struct audio_buffer_pool *init_audio()
{

    static audio_format_t audio_format = {
        (uint32_t)sampleRate,
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
        sevenSegData[i] = sevenSegCharacters[getNthDigit(num, i)];
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
                scheduler(&schedulerTimer);
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
        case BUTTON_INC:
            handleIncDec(true);
            break;
        case BUTTON_DEC:
            handleIncDec(false);
            break;
        case BUTTON_CONFIRM:
            handleYesNo(true);
            break;
        case BUTTON_CANCEL:
            handleYesNo(false);
            break;
        case BUTTON_SD_TEMP:
            sdSafeLoadTemp = true;
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
        newNumSteps += isInc ? 16 : -16;
        if (newNumSteps > 112)
            newNumSteps = 112;
        else if (newNumSteps < 16)
            newNumSteps = 16;
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
        tuplet += isInc ? 2 : -2;
        if (tuplet < 1)
            tuplet = 1;
        else if (tuplet > 7)
            tuplet = 7;
        quarterNoteDivision = quarterNoteDivisionRef[tuplet]; // ugly, temp
        displayTuplet();
        break;

    case BUTTON_OUTPUT:
        outputSample += isInc ? 1 : -1;
        if(outputSample < 0) outputSample = 0;
        else if(outputSample > NUM_SAMPLES-1) outputSample = NUM_SAMPLES-1;
        updateLedDisplay(outputSample);
        break;

    }
}

void handleYesNo(bool isYes) {
    switch(activeButton)
    {
        case BUTTON_OUTPUT:
            samples[outputSample].output1 = isYes;
            break;
    }
}

void displayTempo()
{
    updateLedDisplay((int)tempo);
}

void displayTimeSignature()
{
    int tempTimeSig = 4 + 10 * newNumSteps / 16;
    if (newNumSteps != numSteps)
        tempTimeSig += 8000; // temp, shows that the new signature is not yet active
    updateLedDisplay(tempTimeSig);
}

void displayTuplet()
{
    updateLedDisplay(tuplet);
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

void loadSamplesFromSD()
{
    int pageNum = 0;
    const char *filenames[NUM_SAMPLES] = {"samples/kick.wav", "samples/snare.wav", "samples/closedhat.wav", "samples/tom.wav"};
    uint32_t sampleStartPoints[NUM_SAMPLES] = {0};
    uint32_t sampleLengths[NUM_SAMPLES] = {0};

    for (int n = 0; n < NUM_SAMPLES; n++)
    {
        sd_card_t *pSD = sd_get_by_num(0);
        FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);
        if (FR_OK != fr)
            panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
        FIL fil;
        // const char *const filename = "samples/kick.wav";
        fr = f_open(&fil, filenames[n], FA_READ);
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
            // uint32_t chunkSize = sizeBuffer[0] | sizeBuffer[1] << 8 | sizeBuffer[2] << 16 | sizeBuffer[3] << 24;
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
                        printf("sample %d, start on page %d, length %d\n", n, sampleStartPoints[n], sampleLengths[n]);
                    }
                    foundDataChunk = true;

                    writePageToFlash(sampleDataBuffer, FLASH_AUDIO_ADDRESS + pageNum * FLASH_PAGE_SIZE);

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

void initSamplesFromFlash()
{
    for (int n = 0; n < NUM_SAMPLES; n++)
    {
        uint sampleStart = getIntFromBuffer(flashData, SAMPLE_START_POINTS + n * 4);
        uint sampleLength = getIntFromBuffer(flashData, SAMPLE_LENGTHS + n * 4);
        printf("start %d, length %d\n", sampleStart, sampleLength);

        samples[n].length = sampleLength / 2;
        samples[n].position = (float)samples[n].length;

        for (int i = 0; i < samples[n].length && i < sampleLength / 2; i++)
        {
            int16_t thisSample = flashAudio[i * 2 + 1 + sampleStart] << 8 | flashAudio[i * 2 + sampleStart];
            samples[n].sampleData[i] = thisSample;
        }

        // temp
        samples[n].output1 = n%2 == 1;
        samples[n].output2 = n%2 == 0;
    }
}

void updateLeds()
{
    if (shiftRegOutLoopNum == 0 && shiftRegOutPhase == 0)
    {
        // copy LED data so it doesn't change during serial transfer
        for (int i = 0; i < 4; i++)
        {
            storedLedData[i] = (sevenSegData[i] << 8) + 0b11110000 + singleLedData;
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