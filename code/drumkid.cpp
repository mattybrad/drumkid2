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
int cluster = 0;
int magnet = 0;
int zoom = 0;
bool useZoomVelocityGradient = false;
int velRange = 0;
int velMidpoint = 0;
int drop = 0;
int dropRandom = 0;
int swing = 0;
int crush = 0;
//uint8_t crushVolume[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,10,8};
uint8_t crushVolume[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 13, 13};
int magnetCurve[6][32];

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
int newNumSteps = 4 * QUARTER_NOTE_STEPS;
int numSteps = 4 * QUARTER_NOTE_STEPS;
bool shift = false;
int tuplet = TUPLET_STRAIGHT;
int quarterNoteDivisionRef[NUM_TUPLET_MODES] = {QUARTER_NOTE_STEPS, 3*QUARTER_NOTE_STEPS/4, 5*QUARTER_NOTE_STEPS/8, 7*QUARTER_NOTE_STEPS/8}; // to do: calculate this automatically
int quarterNoteDivision = quarterNoteDivisionRef[tuplet];
int nextQuarterNoteDivision = quarterNoteDivision;
int outputSample = 0;
int tempMissingCount = 0;
float nextHoldUpdateInc = 0;
float nextHoldUpdateDec = 0;
const int NUM_PPQN_VALUES = 6;
int ppqnValues[NUM_PPQN_VALUES] = {1, 2, 4, 8, 16, 32};
int syncInPpqnIndex = 0;
int syncOutPpqnIndex = 0;
int syncInPpqn = ppqnValues[syncInPpqnIndex];
int syncOutPpqn = ppqnValues[syncOutPpqnIndex];

// SD card stuff
int sampleFolderNum = 0;

// tempo/sync stuff
bool externalClock = false;

int activeButton = BOOTUP_VISUALS;
int activeSetting = 0;
int settingsMenuLevel = 0;
int glitchChannel = GLITCH_CHANNEL_BOTH;
bool glitch1 = true;
bool glitch2 = true;

bool sdSafeLoadTemp = false;
bool sdShowFolderTemp = false;

// metronome temp
int metronomeWaveHigh = true;
int metronomeLengthTally = INT32_MAX;
int metronomeBarStart = true;
int64_t nextMetronome = INT64_MAX;

// convert zoom and step to velocity multiplier
uint8_t maxZoom = log(4*QUARTER_NOTE_STEPS)/log(2) + 1;
float zoomDivisor = 4095 / maxZoom;
uint8_t stepVal[QUARTER_NOTE_STEPS * 4];
float getZoomMultiplier(int thisStep)
{
    int thisStepVal = stepVal[thisStep % (4 * QUARTER_NOTE_STEPS)];
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

    int vel = 4095 + zoom - (thisStepVal<<12);
    if(vel < 0) vel = 0;
    else if(vel > 4095 || (!useZoomVelocityGradient && vel > 0)) vel = 4095;
    return vel;
}

// temp figuring out proper hit generation
uint16_t scheduledStep = 0;

int64_t nextSyncOut = 0;
int64_t nextHitTime = 0; // in samples
int64_t currentTime = 0; // in samples
int scheduleAheadTime = SAMPLES_PER_BUFFER * 4; // in samples, was 0.02 seconds
uint16_t mainTimerInterval = 100; // us

void scheduleSyncOut() {
    if(scheduledStep % (QUARTER_NOTE_STEPS/syncOutPpqn) == 0) nextSyncOut = nextHitTime;
}
void scheduleMetronome()
{
    if (activeButton == BUTTON_LIVE_EDIT && scheduledStep % QUARTER_NOTE_STEPS == 0) {
        nextMetronome = nextHitTime;
        metronomeBarStart = (scheduledStep == 0);
    }
}
int64_t tempOffsets[NUM_SAMPLES] = {0};
#define SWING_OFF 0
#define SWING_8TH 1
#define SWING_16TH 2
uint8_t swingMode = SWING_OFF;
bool clusterReady[NUM_SAMPLES];
void scheduleHits()
{
    int64_t adjustedHitTime = nextHitTime;
    int16_t revStep = scheduledStep;

    // swing calculations
    if(tuplet == TUPLET_STRAIGHT) {
        // currently only implementing swing in straight mode, otherwise too confusing!
        int thisSwingVal;
        if(swing < 2048) {
            // swing 8th notes
            thisSwingVal = 4095 - swing * 2;
            swingMode = SWING_8TH;
        } else {
            // swing 16th notes
            thisSwingVal = (swing - 2048) * 2;
            swingMode = SWING_16TH;
        }
        thisSwingVal = (thisSwingVal * 5 - 4096) >> 2; // increase range then offset and clamp, giving dead zone in centre
        thisSwingVal = std::max(0, std::min(4095, thisSwingVal));

        if(thisSwingVal==0) {
            swingMode = SWING_OFF;
        } else {
            if (swingMode == SWING_8TH && scheduledStep % QUARTER_NOTE_STEPS == QUARTER_NOTE_STEPS >> 1)
            {
                adjustedHitTime += ((int64_t)thisSwingVal * stepTime * QUARTER_NOTE_STEPS) >> 14;
            }
            else if (swingMode == SWING_16TH && scheduledStep % (QUARTER_NOTE_STEPS >> 1) == QUARTER_NOTE_STEPS >> 2)
            {
                adjustedHitTime += ((int64_t)thisSwingVal * stepTime * QUARTER_NOTE_STEPS) >> 15;
            } 
        }

    }

    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        if (Sample::pitch < 0)
        {
            // offset sample if reversed so it ends at the correct time
            tempOffsets[i] = (static_cast<int32_t>(samples[i].length) * QUARTER_NOTE_STEPS << Sample::LERP_BITS) / (SAMPLE_RATE * -Sample::pitch);
            revStep = (scheduledStep + tempOffsets[i]) % numSteps;
        }

        bool dropHit = !bitRead(dropRef[drop], i);
        bool dropHitRandom = !bitRead(dropRef[dropRandom], i);
        int randNum = rand() % 4095; // range of 0 to 4094, allowing a chance value of 4095 (maximum) to act as "100% probability" (always bigger)
        int thisStepVal = std::max((int)stepVal[revStep] - 3, 0); // step values from 0 (quarter note) to 5 (128th note)
        int thisChance;
        if(thisStepVal == 0 || chance == 4095) {
            thisChance = chance;
        } else if(magnet < 2048) {
            thisChance = (magnet * magnetCurve[thisStepVal][chance>>7] + (2048-magnet) * ((chance*chance)>>12)) >> 11;
        } else {
            thisChance = ((4095 - magnet) * magnetCurve[thisStepVal][chance >> 7]) >> 11;
        }
        if(clusterReady[i]) thisChance = std::max(cluster, thisChance);
        int intVel = 2*velMidpoint - 4095 + (rand() % std::max(1,velRange*2)) - velRange; // outside chance that this line is causing issues, fenceposts etc, check if having problems

        int zoomMult = getZoomMultiplier(revStep);
        if (!dropHit && beats[beatNum].getHit(i, revStep, tuplet))
        {
            // if current beat contains a hit on this step, calculate the velocity and queue the hit
            int thisVel = 4095;
            if(chance > randNum) {
                thisVel = std::min(4095, std::max(0, 4095 + intVel));
            }
            samples[i].queueHit(adjustedHitTime, revStep, thisVel);
        }
        else
        {
            if (!dropHit && !dropHitRandom && thisChance > randNum)
            {
                //int intVel = (rand() % velRange) + velMidpoint - velRange / 2;
                if (intVel < 0)
                    intVel = 0;
                else if (intVel > 4095)
                    intVel = 4095;
                intVel = (intVel * zoomMult) >> 12;
                if (intVel >= 8)
                {
                    samples[i].queueHit(adjustedHitTime, revStep, intVel);
                }
            }
        }
        if (zoomMult > 0)
        {
            if (thisChance > randNum)
            {
                clusterReady[i] = true;
            }
            else
            {
                clusterReady[i] = false;
            }
        }
    }
}

void nextHit()
{
    scheduledStep++;
    
    if(nextQuarterNoteDivision != quarterNoteDivision) {
        // if tuplet mode has changed, remap the step to the same position in the bar
        int thisQuarterNote = scheduledStep / QUARTER_NOTE_STEPS;
        int thisSubStep = scheduledStep % QUARTER_NOTE_STEPS;
        int nextSubStep = (thisSubStep * nextQuarterNoteDivision) / quarterNoteDivision + 1;
        int nextStep = thisQuarterNote * QUARTER_NOTE_STEPS + nextSubStep;
        scheduledStep = nextStep;
        // horrible looking calculation, basically instead of nextHitTime being a single step in the future, it's now the difference between the time of the previous step and the time of the next step, and the code had to be written this way because I'm dealing with ints, so fractions don't really work unless you cross-multiply(?) the stuff you're subtracting
        int64_t nextHitTimeDelta = (2646000 * (quarterNoteDivision*nextSubStep - nextQuarterNoteDivision*thisSubStep)) / (tempo * quarterNoteDivision * nextQuarterNoteDivision); // todo: change this from tempo to stepTime for better precision
        nextHitTime += nextHitTimeDelta;
        quarterNoteDivision = nextQuarterNoteDivision;
    } else {
        //nextHitTime += (2646000 / tempo) / quarterNoteDivision; // not very precise?
        nextHitTime += (stepTime * QUARTER_NOTE_STEPS) / quarterNoteDivision; // todo: handle tuplets
    }

    if (scheduledStep % QUARTER_NOTE_STEPS >= quarterNoteDivision)
        // in non-straight tuplet modes, quarterNoteDivision will be less than QUARTER_NOTE_STEPS, so need to skip to next integer multiple of QUARTER_NOTE_STEPS when we go past that point
        scheduledStep = (scheduledStep / QUARTER_NOTE_STEPS + 1) * QUARTER_NOTE_STEPS;

    if (scheduledStep >= numSteps)
    {
        numSteps = newNumSteps;
        if (activeButton == BUTTON_TIME_SIGNATURE)
            displayTimeSignature(); // temp? not 100% sure this is a good idea, maybe OTT, shows new time signature has come into effect
        scheduledStep = 0;

    }

    // temp, v/oct CV tempo (commenting out for now - see notes)
    //float cvTempo = 825.0 * pow(2.0, ((float)analogReadings[CV_TBC] - 3517.0) / 409.5);
    //tempo = cvTempo;
    //stepTime = 2646000 / (tempo * QUARTER_NOTE_STEPS);
    //printf("cv in %d %f\n", analogReadings[CV_TBC], cvTempo);
}
void scheduler()
{
    while (beatPlaying && nextHitTime < currentTime + scheduleAheadTime)
    {
        scheduleHits();
        scheduleSyncOut();
        scheduleMetronome();
        nextHit();
    }
}

int64_t codeStartTime = 0;
int64_t worstTime = 0;
void startCodeTimer() {
    codeStartTime = time_us_64();
}
void stopCodeTimer(bool showNow) {
    int64_t codeTime = time_us_64() - codeStartTime;
    if(codeTime > worstTime) {
        printf("worst t=%lld\n", codeTime);
        worstTime = codeTime;
    }
    if(showNow) {
        printf("t=%lld\n", codeTime);
    }
}

bool tempScheduler(repeating_timer_t *rt)
{
    //startCodeTimer();
    scheduler();
    //stopCodeTimer();
    return true;
}

bool bootupVisualFrame(repeating_timer_t *rt) {
    sevenSegData[0] = rand() % 256;
    sevenSegData[1] = rand() % 256;
    sevenSegData[2] = rand() % 256;
    sevenSegData[3] = rand() % 256;
    if(time_us_32() > 1000000) {
        activeButton = NO_ACTIVE_BUTTON;
        return false;
    } else {
        return true;
    }
}

int64_t prevTime = 0;
// main function, obviously
int main()
{
    stdio_init_all();
    time_init();
    alarm_pool_init_default(); // not sure if needed
    initZoom();

    printf("Drumkid V2\n");
    printf("zoom %d\n", maxZoom);

    //checkFlashData();
    findCurrentFlashSettingsSector();
    loadSettings();
    initGpio();

    initSamplesFromFlash();
    loadBeatsFromFlash();

    struct audio_buffer_pool *ap = init_audio();

    add_repeating_timer_us(mainTimerInterval, mainTimerLogic, NULL, &mainTimer);
    int64_t tempPeriod = 500;
    repeating_timer_t tempSchedulerTimer;
    add_repeating_timer_us(tempPeriod, tempScheduler, NULL, &tempSchedulerTimer);
    //add_repeating_timer_ms(1000, performanceCheck, NULL, &performanceCheckTimer);
    repeating_timer_t bootupVisualTimer;
    add_repeating_timer_ms(60, bootupVisualFrame, NULL, &bootupVisualTimer);

    // temp, populating magnet curve
    printf("magnet curve:\n");
    for(int i=0; i<6; i++) {
        for(int j=0; j<32; j++) {
            float accurateValue = pow(((float)j * 1.0) / 31.0, 4.0 * (float)i + 1.0);
            magnetCurve[i][j] = 4095.0 * accurateValue;
            printf("%d ", magnetCurve[i][j]);
        }
        printf("\n");
    }

    // temp
    //beatPlaying = true;
    int timeIncrement = SAMPLES_PER_BUFFER; // in samples

    // audio buffer loop, runs forever
    while (true)
    {
        //startCodeTimer();

        // something to do with audio that i don't fully understand
        struct audio_buffer *buffer = take_audio_buffer(ap, true);
        int16_t *bufferSamples = (int16_t *)buffer->buffer->bytes;

        displayPulse(); // temp, uses scheduledStep, could be more accurate

        // update audio output
        for (uint i = 0; i < buffer->max_sample_count * 2; i += 2)
        {
            // sample updates go here
            int32_t out1 = 0;
            int32_t out2 = 0;
            if(currentTime + (i>>1) >= nextSyncOut) {
                pulseGpio(SYNC_OUT, outputPulseLength); // todo: adjust pulse length based on PPQN, or advanced setting
                pulseLed(0, LED_PULSE_LENGTH);
                nextSyncOut = INT64_MAX;
            }
            if (currentTime + (i >> 1) >= nextMetronome)
            {
                metronomeWaveHigh = true;
                metronomeLengthTally = 0;
                nextMetronome = INT64_MAX;
            }
            for (int j = 0; j < NUM_SAMPLES; j++)
            {
                bool didTrigger = samples[j].update(currentTime + (i>>1)); // could be more efficient
                if(didTrigger && j<4) {
                    pulseGpio(TRIGGER_OUT_PINS[j], outputPulseLength);
                }
                if(samples[j].output1) out1 += samples[j].value;
                if(samples[j].output2) out2 += samples[j].value;
            }
            if(metronomeLengthTally < 2000) {
                metronomeLengthTally ++;
                if(metronomeLengthTally % (metronomeBarStart ? 50 : 100) == 0) {
                    metronomeWaveHigh = !metronomeWaveHigh;
                }
                if(metronomeWaveHigh) out1 += 5000;
            }
            if(glitch1)
                bufferSamples[i] = (out1 >> (2 + crush)) << crushVolume[crush];
            else
                bufferSamples[i] = out1 >> 2;
            if (glitch2)
                bufferSamples[i+1] = (out2 >> (2 + crush)) << crushVolume[crush];
            else
                bufferSamples[i+1] = out2 >> 2;
        }
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);

        currentTime += timeIncrement;

        if (sdSafeLoadTemp)
        {
            sdSafeLoadTemp = false;
            loadSamplesFromSD();
        }

        if(sdShowFolderTemp) {
            sdShowFolderTemp = false;
            scanSampleFolders();
            if(activeButton != ERROR_DISPLAY) {
                if (sampleFolderNum >= numSampleFolders)
                    sampleFolderNum = 0; // just in case card is removed, changed, and reinserted - todo: proper folder names comparison to revert to first folder if any changes
                char ledFolderName[5];
                strncpy(ledFolderName, folderNames[sampleFolderNum], 5);
                updateLedDisplayAlpha(ledFolderName);
            }
        }

        for (int i = 0; i < NUM_SAMPLES; i++)
        {
            //bitWrite(singleLedData, i, samples[i].playing);
        }

        //stopCodeTimer(true);
    }
    return 0;
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

int64_t onLedPulseTimeout(alarm_id_t id, void *user_data)
{
    uint ledNum = (uint)user_data;
    bitWrite(singleLedData, ledNum, false);
    return 0;
}

void pulseLed(uint ledNum, uint16_t pulseLengthMicros)
{
    bitWrite(singleLedData, ledNum, true);
    add_alarm_in_us(pulseLengthMicros, onLedPulseTimeout, (void *)ledNum, true);
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

int64_t prevTimeCheck = 0;
int performanceThreshold = SAMPLE_RATE - 2 * SAMPLES_PER_BUFFER;
int performanceErrorTally = 0;
bool performanceCheck(repeating_timer_t *rt) {
    int64_t delta = currentTime - prevTimeCheck;
    prevTimeCheck = currentTime;
    if(delta < performanceThreshold && currentTime > SAMPLE_RATE) {
        printf("performance error, samples processed = %lld/%d\n", delta, SAMPLE_RATE);
        showError("perf");
        performanceErrorTally ++;
        if(performanceErrorTally >= 3) {
            beatPlaying = false;
            performanceErrorTally = 0;
        }
    }
    return true;
}

void applyDeadZones(int &param) {
    param = (4095 * (std::max(ANALOG_DEAD_ZONE_LOWER, std::min(ANALOG_DEAD_ZONE_UPPER, param)) - ANALOG_DEAD_ZONE_LOWER)) / ANALOG_EFFECTIVE_RANGE;
}

bool mainTimerLogic(repeating_timer_t *rt)
{
    //startCodeTimer();

    updateLeds();
    updateShiftRegButtons();
    updateAnalog();
    updateSyncIn();

    // knobs / CV
    chance = analogReadings[POT_CHANCE] + analogReadings[CV_CHANCE] - 2048;
    applyDeadZones(chance);

    cluster = analogReadings[POT_CLUSTER];
    applyDeadZones(cluster);

    zoom = analogReadings[POT_ZOOM] + analogReadings[CV_ZOOM] - 2048;
    applyDeadZones(zoom);

    magnet = analogReadings[POT_MAGNET];
    applyDeadZones(magnet);

    zoom = zoom * maxZoom;
    velRange = analogReadings[POT_RANGE];
    velMidpoint = analogReadings[POT_MIDPOINT] + analogReadings[CV_MIDPOINT] - 2048;
    applyDeadZones(velRange);
    applyDeadZones(velMidpoint);

    // pitch knob maths - looks horrible! the idea is for the knob to have a deadzone at 12 o'clock which corresponds to normal playback speed (1024). to keep the maths efficient (ish), i'm trying to avoid division, because apart from the deadzone section, it doesn't matter too much what the exact values are, so i'm just using bitwise operators to crank up the range until it feels right. CV adjustment happens after the horrible maths, because otherwise CV control (e.g. a sine LFO pitch sweep) would sound weird and disjointed
    int pitchInt = analogReadings[POT_PITCH];
    int pitchDeadZone = 500;
    if(pitchInt > 2048 + pitchDeadZone) {
        // speed above 100%
        pitchInt = ((pitchInt - 2048 - pitchDeadZone)<<1) + 1024;
    } else if(pitchInt < 2048 - pitchDeadZone) {
        // speed below 100%, and reverse
        pitchInt = pitchInt - 2048 + pitchDeadZone + 1024;
        if(pitchInt < 0) {
            pitchInt = pitchInt << 3;
        }
    } else {
        // speed exactly 100%, in deadzone
        pitchInt = 1024;
    }
    pitchInt += analogReadings[CV_TBC] - 2048;
    if(pitchInt == 0) pitchInt = 1; // seems sensible, prevents sample getting stuck

    Sample::pitch = pitchInt; // temp...

    int crop = analogReadings[POT_CROP];
    applyDeadZones(crop);
    if(crop == 4095) crop = MAX_SAMPLE_STORAGE;
    else crop = (crop * crop) >> 11; // should be 11
    Sample::crop = crop;

    drop = analogReadings[POT_DROP] / 373; // gives range of 0 to 10
    dropRandom = analogReadings[POT_DROP_RANDOM] / 373; // gives range of 0 to 10

    swing = analogReadings[POT_SWING];
    applyDeadZones(swing);

    crush = analogReadings[POT_CRUSH];
    applyDeadZones(crush);
    crush = 4095 - (((4095 - crush) * (4095 - crush))>>12);
    crush = crush >> 8;

    //stopCodeTimer();

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
    int compare = 1;
    for(int i=0; i<4; i++) {
        if(i==0||num>=compare) {
            sevenSegData[3-i] = sevenSegAsciiCharacters[getNthDigit(num, i) + 48];
        } else {
            sevenSegData[3-i] = sevenSegAsciiCharacters[' '];
        }
        compare *= 10;
    }
}

void updateLedDisplayAlpha(const char* word)
{
    bool foundWordEnd = false;
    for(int i=0; i<4; i++) {
        bool skipChar = false;
        if(word[i] == '\0') {
            foundWordEnd = true;
        }
        if(foundWordEnd) {
            sevenSegData[i] = 0b00000000;
        } else {
            sevenSegData[i] = sevenSegAsciiCharacters[word[i]];
        }
        
    }
}

void doLiveHit(int sampleNum) {
    samples[sampleNum].queueHit(currentTime, scheduledStep, 4095);
    int adjustedStep = (scheduledStep+1) % numSteps; // offset by 2 to allow some leeway either side , not sure if the maths is right there... then reduce granularity to match beat data
    bitWrite(beats[beatNum].beatData[sampleNum], (adjustedStep>>3)<<1, true);
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
                if(!externalClock) {
                    scheduledStep = 0;
                    nextHitTime = currentTime;
                }
                scheduler();
            }
            else
            {
                // handle beat stop
                numSteps = newNumSteps;
                if (activeButton == BUTTON_TIME_SIGNATURE)
                    displayTimeSignature();
                scheduledStep = 0;

                // temp! testing settings write
                saveSettings();
            }
            break;
        case BUTTON_SETTINGS:
            activeButton = BUTTON_SETTINGS;
            displaySettings();
            break;
        case BUTTON_TAP_TEMPO:
            activeButton = BUTTON_TAP_TEMPO;
            updateTapTempo();
            break;
        case BUTTON_LIVE_EDIT:
            activeButton = BUTTON_LIVE_EDIT;
            break;
        case BUTTON_CLOCK_MODE:
            activeButton = BUTTON_CLOCK_MODE;
            displayClockMode();
            break;
        case BUTTON_INC:
            if(activeButton == BUTTON_LIVE_EDIT) {
                doLiveHit(0);
            } else {
                nextHoldUpdateInc = currentTime + SAMPLE_RATE;
                handleIncDec(true, false);
            }
            break;
        case BUTTON_DEC:
            if(activeButton == BUTTON_LIVE_EDIT) {
                doLiveHit(1);
            } else {
                nextHoldUpdateDec = currentTime + SAMPLE_RATE;
                handleIncDec(false, false);
            }
            break;
        case BUTTON_CONFIRM:
            if(activeButton == BUTTON_LIVE_EDIT) {
                doLiveHit(2);
            } else {
                handleYesNo(true);
            }
            break;
        case BUTTON_CANCEL:
            if(activeButton == BUTTON_LIVE_EDIT) {
                doLiveHit(3);
            } else {
                handleYesNo(false);
            }
            break;
        case BUTTON_SHIFT_CANCEL:
            activeButton = NO_ACTIVE_BUTTON;
            break;
        case BUTTON_PPQN_IN:
            activeButton = BUTTON_PPQN_IN;
            updateLedDisplay(syncInPpqn);
            break;
        case BUTTON_PPQN_OUT:
            activeButton = BUTTON_PPQN_OUT;
            updateLedDisplay(syncOutPpqn);
            break;
        case BUTTON_LOAD_SAMPLES:
            activeButton = BUTTON_LOAD_SAMPLES;
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
        case BUTTON_OUTPUT_1:
            activeButton = BUTTON_OUTPUT_1;
            displayOutput(1);
            break;
        case BUTTON_OUTPUT_2:
            activeButton = BUTTON_OUTPUT_2;
            displayOutput(2);
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
            if(activeButton == BUTTON_EDIT_BEAT || activeButton == BUTTON_LIVE_EDIT) {
                for(int i=0; i<NUM_SAMPLES; i++) {
                    beats[beatNum].beatData[i] = 0;
                }
            }
            break;
        case BUTTON_EDIT_BEAT:
            activeButton = BUTTON_EDIT_BEAT;
            displayEditBeat();
            break;
        case BUTTON_SAVE:
            saveBeatLocation = beatNum;
            activeButton = BUTTON_SAVE;
            updateLedDisplay(saveBeatLocation);
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

void handleSubSettingIncDec(bool isInc) {
    // could probably do this with pointers to make it neater but it's hot today so that's not happening
    int thisInc = isInc ? 1 : -1;
    switch(activeSetting) {
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

void handleIncDec(bool isInc, bool isHold)
{
    // have to declare stuff up here because of switch statement
    int prevBeatNum;

    switch (activeButton)
    {
    case BUTTON_PPQN_IN:
        syncInPpqnIndex += isInc ? 1 : -1;
        syncInPpqnIndex = std::max(0, std::min(NUM_PPQN_VALUES-1, syncInPpqnIndex));
        syncInPpqn = ppqnValues[syncInPpqnIndex];
        updateLedDisplay(syncInPpqn);
        break;
    case BUTTON_PPQN_OUT:
        syncOutPpqnIndex += isInc ? 1 : -1;
        syncOutPpqnIndex = std::max(0, std::min(NUM_PPQN_VALUES - 1, syncOutPpqnIndex));
        syncOutPpqn = ppqnValues[syncOutPpqnIndex];
        updateLedDisplay(syncOutPpqn);
        break;
    case BUTTON_CLOCK_MODE:
        externalClock = !externalClock;
        if(externalClock) {

        } else {
            stepTime = 2646000 / (tempo * QUARTER_NOTE_STEPS);
        }
        displayClockMode();
        break;
    case BUTTON_MANUAL_TEMPO:
        if(!externalClock) {
            if(isHold) {
                tempo += isInc ? 10 : -10;
                tempo = 10 * ((tempo + 5) / 10);
            } else {
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
        if(beatNum != prevBeatNum) {
            revertBeat(prevBeatNum);
            backupBeat(beatNum);
        }
        displayBeat();
        break;

    case BUTTON_EDIT_BEAT:
        editSample += isInc ? 1 : -1;
        if(editSample < 0)
            editSample = 0;
        if(editSample >= NUM_SAMPLES)
            editSample = NUM_SAMPLES - 1;
        displayEditBeat();
        break;

    case BUTTON_SETTINGS:
        if(settingsMenuLevel == 0) {
            activeSetting += isInc ? 1 : -1;
            if(activeSetting < 0)
                activeSetting = 0;
            if(activeSetting >= NUM_MENU_SETTINGS)
                activeSetting = NUM_MENU_SETTINGS - 1;
        } else if(settingsMenuLevel == 1) {
            handleSubSettingIncDec(isInc);
        }
        displaySettings();
        break;

    case BUTTON_SAVE:
        saveBeatLocation += isInc ? 1 : -1;
        if(saveBeatLocation < 0)
            saveBeatLocation = 0;
        if(saveBeatLocation >= NUM_BEATS)
            saveBeatLocation = NUM_BEATS - 1;
        updateLedDisplay(saveBeatLocation);
        break;

    case BUTTON_TUPLET:
        tuplet += isInc ? 1 : -1;
        if (tuplet < 0)
            tuplet = 0;
        else if (tuplet >= NUM_TUPLET_MODES)
            tuplet = NUM_TUPLET_MODES - 1;
        nextQuarterNoteDivision = quarterNoteDivisionRef[tuplet];
        displayTuplet();
        break;

    case BUTTON_OUTPUT_1:
    case BUTTON_OUTPUT_2:
        outputSample += isInc ? 1 : -1;
        if(outputSample < 0) outputSample = 0;
        else if(outputSample > NUM_SAMPLES-1) outputSample = NUM_SAMPLES-1;
        displayOutput(activeButton == BUTTON_OUTPUT_1 ? 1 : 2);
        break;

    case BUTTON_LOAD_SAMPLES:
        sampleFolderNum += isInc ? 1 : -1;
        if (sampleFolderNum < 0)
            sampleFolderNum = 0;
        else if (sampleFolderNum > numSampleFolders - 1)
            sampleFolderNum = numSampleFolders - 1;
        sdShowFolderTemp = true;
        break;
    }
}

void handleYesNo(bool isYes) {
    // have to declare stuff up here because of switch statement
    int tupletEditStep;

    bool useDefaultNoBehaviour = true;
    switch(activeButton)
    {
        case BUTTON_OUTPUT_1:
        case BUTTON_OUTPUT_2:
            useDefaultNoBehaviour = false;
            if(activeButton == BUTTON_OUTPUT_1) {
                samples[outputSample].output1 = isYes;
            } else {
                samples[outputSample].output2 = isYes;
            }
            displayOutput(activeButton == BUTTON_OUTPUT_1 ? 1 : 2);
            break;
        
        case BUTTON_LOAD_SAMPLES:
            if(isYes) sdSafeLoadTemp = true;
            break;

        case BUTTON_EDIT_BEAT:
            useDefaultNoBehaviour = false;
            tupletEditStep = (editStep / QUARTER_NOTE_STEPS_SEQUENCEABLE) * QUARTER_NOTE_STEPS_SEQUENCEABLE + Beat::tupletMap[tuplet][editStep % QUARTER_NOTE_STEPS_SEQUENCEABLE];
            bitWrite(beats[beatNum].beatData[editSample], tupletEditStep, isYes);
            editStep ++;
            if(editStep % QUARTER_NOTE_STEPS_SEQUENCEABLE >= quarterNoteDivision>>2) {
                editStep = (editStep / QUARTER_NOTE_STEPS_SEQUENCEABLE + 1) * QUARTER_NOTE_STEPS_SEQUENCEABLE;
            }
            if(editStep >= (newNumSteps / QUARTER_NOTE_STEPS) * QUARTER_NOTE_STEPS_SEQUENCEABLE)
                editStep = 0;
            displayEditBeat();
            break;

        case BUTTON_SETTINGS:
            if(settingsMenuLevel == 0 && isYes) settingsMenuLevel = 1;
            else if(settingsMenuLevel == 1) {
                // handle new chosen option
                settingsMenuLevel = 0;
            }
            displaySettings();
            break;

        case BUTTON_SAVE:
            if(isYes) writeBeatsToFlash();
            activeButton = NO_ACTIVE_BUTTON;
            break;
    }
    if(!isYes && useDefaultNoBehaviour) {
        activeButton = NO_ACTIVE_BUTTON;
        bitWrite(singleLedData, 3, false); // clear error LED
    }
}

void displayClockMode() {
    updateLedDisplayAlpha(externalClock ? "ext" : "int");
}

void displayTempo()
{
    if(externalClock) {
        int calculatedTempo = 2646000 / (stepTime * QUARTER_NOTE_STEPS);
        updateLedDisplay(calculatedTempo);
    } else {
        updateLedDisplay((int)tempo);
    }
}

void displayTimeSignature()
{
    char timeSigText[4];
    timeSigText[0] = newNumSteps / QUARTER_NOTE_STEPS + 48;
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
        "sept"
    };
    updateLedDisplayAlpha(tupletNames[tuplet]);
}

void displayBeat()
{
    updateLedDisplay(beatNum);
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

void displayOutput(int outputNum) {
    for(int i=0; i<4 && i<NUM_SAMPLES; i++) {
        if(outputNum == 1) {
            sevenSegData[i] = samples[i].output1 ? 0b10000000 : 0b00000000;
        } else if(outputNum == 2) {
            sevenSegData[i] = samples[i].output2 ? 0b10000000 : 0b00000000;
        }
    }
    bitWrite(sevenSegData[outputSample], 4, true);
}

void displaySettings() {
    if(settingsMenuLevel == 0) {
        switch(activeSetting) {
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
    } else if(settingsMenuLevel == 1) {
        switch(activeSetting) {
            case SETTING_GLITCH_CHANNEL:
                if(glitchChannel == GLITCH_CHANNEL_BOTH)
                    updateLedDisplayAlpha("both");
                else if (glitchChannel == GLITCH_CHANNEL_1)
                    updateLedDisplayAlpha("1");
                else if (glitchChannel == GLITCH_CHANNEL_2)
                    updateLedDisplayAlpha("2");
                else if (glitchChannel == GLITCH_CHANNEL_NONE)
                    updateLedDisplayAlpha("none");
                break;

            case SETTING_OUTPUT_PULSE_LENGTH:
                updateLedDisplay(outputPulseLength);
                break;

            case SETTING_OUTPUT_PPQN:
                updateLedDisplay(syncOutPpqn);
                break;

            case SETTING_INPUT_PPQN:
                updateLedDisplay(syncInPpqn);
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
                updateLedDisplay(inputQuantize);
                break;
        }
    }
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
            handleIncDec(true, true);
        }
    }
    else if (buttonStableStates[BUTTON_DEC])
    {
        if (currentTime > nextHoldUpdateDec)
        {
            nextHoldUpdateDec = currentTime + (SAMPLE_RATE >> 3);
            handleIncDec(false, true);
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

        //analogReadings[analogLoopNum] = (4095 * (std::max(ANALOG_DEAD_ZONE_LOWER, std::min(ANALOG_DEAD_ZONE_UPPER, 4095 - adc_read())) - ANALOG_DEAD_ZONE_LOWER)) / ANALOG_EFFECTIVE_RANGE;

        adc_select_input(1);
        analogReadings[analogLoopNum + 8] = 4095 - adc_read();
        //analogReadings[analogLoopNum + 8] = (4095 * (std::max(ANALOG_DEAD_ZONE_LOWER, std::min(ANALOG_DEAD_ZONE_UPPER, 4095 - adc_read())) - ANALOG_DEAD_ZONE_LOWER)) / ANALOG_EFFECTIVE_RANGE;
    }

    analogPhase++;
    if (analogPhase == 2)
    {
        analogPhase = 0;
        analogLoopNum++;
        if (analogLoopNum == 8)
        {
            analogLoopNum = 0;
        }
    }
}

void resetSampleFolderList() {
    char nullString[MAX_FOLDER_NAME_LENGTH] = "";
    for(int i=0; i<MAX_SAMPLE_FOLDERS; i++) {
        strncpy(folderNames[i], nullString, MAX_FOLDER_NAME_LENGTH);
    }
}

// not the most beautiful function, but this adds a sample folder at the right point in the alphabetical list
void addSampleFolder(char* newFolderPointer) {
    int insertPoint = 0;
    bool done = false;

    for(int i=0; i<MAX_SAMPLE_FOLDERS && !done; i++) {
        int res = strncmp(newFolderPointer, folderNames[i], MAX_FOLDER_NAME_LENGTH);
        if(res < 0 || folderNames[i][0] == 0) {
            insertPoint = i;
            done = true;
        }
    }

    done = false;
    for(int i=MAX_SAMPLE_FOLDERS-1; i>insertPoint && i>0; i--) {
        strncpy(folderNames[i], folderNames[i - 1], MAX_FOLDER_NAME_LENGTH);
    }
    strncpy(folderNames[insertPoint], newFolderPointer, MAX_FOLDER_NAME_LENGTH);
}

// go through every sample folder and create an alphabetically ordered array of the folder names
void scanSampleFolders() {
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
    numSampleFolders = foundNum+1;

    f_closedir(&dir);
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
        if (FR_OK != fr) {
            printf("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
            showError("err");
            return;
        }
        FIL fil;
        char filename[255];
        strcpy(filename, "samples/");
        strcpy(filename + strlen(filename), folderNames[sampleFolderNum]);
        strcpy(filename+strlen(filename), sampleNames[n]);
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
        writePageToFlash(metadataBuffer, FLASH_AUDIO_METADATA_ADDRESS);

        initSamplesFromFlash();
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
        if(samples[n].startPosition + samples[n].length >= MAX_SAMPLE_STORAGE) {
            samples[n].startPosition = 0;
            samples[n].length = 1000;
            storageOverflow = true;
        }
        printf("sample %d, start %d, length %d\n",n,samples[n].startPosition,samples[n].length);
        samples[n].position = samples[n].length;
        samples[n].positionAccurate = samples[n].length << Sample::LERP_BITS;

        if(!storageOverflow) {
            for (int i = 0; i < samples[n].length && i < sampleLength / 2; i++)
            {
                int pos = i * 2 + sampleStart;
                int16_t thisSample = flashAudio[pos + 1] << 8 | flashAudio[pos];
                Sample::sampleData[sampleStart/2+i] = thisSample;
            }
        }

        // temp
        samples[n].output1 = true;
        samples[n].output2 = true;
    }

    if(storageOverflow) {
        showError("size");
    }
}

void loadBeatsFromFlash()
{
    int startPos = 0;
    for (int i = 0; i < NUM_BEATS; i++)
    {
        for (int j = 0; j < NUM_SAMPLES; j++)
        {
            std::memcpy(&beats[i].beatData[j], &flashUserBeats[8 * (i * NUM_SAMPLES + j)], 8);
        }
    }
    backupBeat(beatNum);
}

void backupBeat(int backupBeatNum) {
    for(int i=0; i<NUM_SAMPLES; i++) {
        beatBackup.beatData[i] = beats[backupBeatNum].beatData[i];
    }
}

void revertBeat(int revertBeatNum) {
    for(int i=0; i<NUM_SAMPLES; i++) {
        beats[revertBeatNum].beatData[i] = beatBackup.beatData[i];
    }
}

#define BEAT_SIZE 32


void writeBeatsToFlash() {
    if(beatNum != saveBeatLocation) {
        for (int i = 0; i < NUM_SAMPLES; i++)
        {
            beats[saveBeatLocation].beatData[i] = beats[beatNum].beatData[i];
        }
        revertBeat(beatNum);
        beatNum = saveBeatLocation;
    }
    backupBeat(beatNum);

    // flash page is 256 bytes
    // beat is 32 bytes
    // 8 beats per page
    uint8_t pageData[FLASH_PAGE_SIZE];
    int beatsPerPage = FLASH_PAGE_SIZE / BEAT_SIZE;
    for(int i=0; i<NUM_BEATS/beatsPerPage; i++) { // each page
        for(int j=0; j<beatsPerPage; j++) { // each beat
            for(int k=0; k<NUM_SAMPLES; k++) { // each channel
                std:memcpy(pageData + BEAT_SIZE * j + 8 * k, &beats[i*beatsPerPage+j].beatData[k], 8);
            }
        }
        writePageToFlash(pageData, FLASH_USER_BEATS_ADDRESS + i * FLASH_PAGE_SIZE);
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
    // potentially a silly way of doing this, written before I knew about std::memcpy? but it works
    int32_t thisInt = buffer[position] | buffer[position + 1] << 8 | buffer[position + 2] << 16 | buffer[position + 3] << 24;
    return thisInt;
}

// possibly unnecessary now, not really using floats
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

#define FLASH_SETTINGS_START 0
#define FLASH_SETTINGS_END 63
int currentSettingsSector = 0;
void findCurrentFlashSettingsSector() {
    bool foundValidSector = false;
    int mostRecentValidSector = 0;
    int highestWriteNum = 0;
    for(int i=FLASH_SETTINGS_START; i<=FLASH_SETTINGS_END; i++) {
        int testInt = getIntFromBuffer(flashData + i*FLASH_SECTOR_SIZE, DATA_CHECK);
        if(testInt == CHECK_NUM) {
            foundValidSector = true;
            int writeNum = getIntFromBuffer(flashData + i * FLASH_SECTOR_SIZE, 4);
            if(writeNum >= highestWriteNum) {
                highestWriteNum = writeNum;
                mostRecentValidSector = i;
            }
        }
    }
    if(!foundValidSector) {
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
    } else {
        currentSettingsSector = mostRecentValidSector;
    }
}

uint32_t microsSinceSyncInChange = mainTimerInterval;
bool syncInStableState = false;

int16_t clockStep = 0; // 8x bigger (<<3) than scheduledStep
bool syncInReceived = false;
uint64_t lastSyncInTime;
void updateSyncIn() {
    if (microsSinceSyncInChange > 1000)
    {
        bool syncInState = !gpio_get(SYNC_IN);
        if (syncInState != syncInStableState)
        {
            syncInStableState = syncInState;
            microsSinceSyncInChange = 0;
            if (syncInState)
            {
                if(externalClock) {
                    if(!syncInReceived) {
                        beatPlaying = true;
                        clockStep = 0;
                        scheduledStep = 0;
                        nextHitTime = currentTime;
                        scheduler();
                    } else {
                        int clockStepsPerPulse = (quarterNoteDivision<<3) / syncInPpqn;
                        clockStep = clockStep + clockStepsPerPulse;

                        if (clockStep % (QUARTER_NOTE_STEPS<<3) >= (quarterNoteDivision<<3)) {
                            // in non-straight tuplet modes, quarterNoteDivision will be less than QUARTER_NOTE_STEPS, so need to skip to next integer multiple of QUARTER_NOTE_STEPS when we go past that point
                            clockStep = (((clockStep>>3) / QUARTER_NOTE_STEPS + 1) * QUARTER_NOTE_STEPS)<<3;
                        }

                        if(clockStep >= numSteps << 3) {
                            clockStep = 0;
                        }

                        if(nextQuarterNoteDivision == quarterNoteDivision) {
                            if(clockStep % 8 == 0) {
                                if(clockStep>>3 == scheduledStep) {
                                    nextHitTime = currentTime;
                                } else {
                                    nextHitTime = currentTime + stepTime;
                                    scheduledStep = ((clockStep>>3) + 1) % numSteps; // might need to do numSteps = newNumSteps here if new scheduledStep is 0?
                                }
                            }
                        } else {
                            // don't do anything clever with timing on this pulse, it'll already be complicated enough switching between tuplet modes!
                        }
                        uint64_t deltaT = time_us_64() - lastSyncInTime;
                        stepTime = (44100 * deltaT * syncInPpqn) / 32000000;
                    }
                }
                lastSyncInTime = time_us_64();
                syncInReceived = true;
                
                if(activeButton == BUTTON_MANUAL_TEMPO) {
                    displayTempo();
                }
                pulseLed(1, LED_PULSE_LENGTH); // temp?
            }
        }
    }
    else
    {
        microsSinceSyncInChange += mainTimerInterval;
    }
}

uint64_t lastTapTempoTime;
void updateTapTempo()
{
    if (!externalClock)
    {
        uint64_t deltaT = time_us_64() - lastTapTempoTime;
        if(deltaT < 5000000) {
            stepTime = (44100 * deltaT) / 32000000;
            tempo = 2646000 / (stepTime * QUARTER_NOTE_STEPS);
            displayTempo();
        }
    }
    lastTapTempoTime = time_us_64();

}

void initZoom() {
    for (int i = 0; i < QUARTER_NOTE_STEPS * 4; i++)
    {
        bool foundLevel = false;
        for (int j = 0; j < maxZoom && !foundLevel; j++)
        {
            if ((i % ((QUARTER_NOTE_STEPS * 4) >> j)) == 0)
            {
                foundLevel = true;
                stepVal[i] = j + 1;
            }
        }
        printf("%d ", stepVal[i]);
    }
}

bool prevPulseLed = false;
void displayPulse() {
    int quarterNote = scheduledStep / QUARTER_NOTE_STEPS;
    bool showPulse = scheduledStep % QUARTER_NOTE_STEPS < (QUARTER_NOTE_STEPS >> 2);
    if(!beatPlaying) quarterNote = 0;
    if(activeButton == NO_ACTIVE_BUTTON) {
        for (int i = 0; i < 4; i++)
        {
            if (i == (quarterNote % 4) && showPulse)
            {
                sevenSegData[i] = quarterNote < 4 ? 0b10000000 : 0b00000010;
            }
            else
            {
                sevenSegData[i] = 0b00000000;
            }
        }
    } else if(activeButton == BUTTON_LIVE_EDIT) {
        for (int i = 0; i < 4; i++)
        {
            if (i == (quarterNote % 4) && showPulse)
            {
                sevenSegData[i] = quarterNote < 4 ? 0b10010000 : 0b00010010;
            }
            else
            {
                sevenSegData[i] = 0b00010000;
            }
        }
    }
    bool currentPulseLed = showPulse && beatPlaying;
    if(!prevPulseLed && currentPulseLed) {
        pulseLed(2, LED_PULSE_LENGTH);
    }
    prevPulseLed = currentPulseLed;
}

void showError(const char* msgx) {
    updateLedDisplayAlpha("err");
    activeButton = ERROR_DISPLAY;
    bitWrite(singleLedData, 3, true);
}

void saveSettings() {
    uint saveNum = currentSettingsSector + 1;
    uint saveSector = FLASH_SETTINGS_START  + (saveNum % (FLASH_SETTINGS_END - FLASH_SETTINGS_START + 1));
    currentSettingsSector = saveSector;

    uint8_t buffer[FLASH_PAGE_SIZE];
    int32_t refCheckNum = CHECK_NUM;
    std::memcpy(buffer, &refCheckNum, 4);
    std::memcpy(buffer + 4, &saveNum, 4);
    std::memcpy(buffer + 8 + 4 * SETTING_GLITCH_CHANNEL, &glitchChannel, 4);
    std::memcpy(buffer + 8 + 4 * SETTING_OUTPUT_PULSE_LENGTH, &outputPulseLength, 4);
    std::memcpy(buffer + 8 + 4 * SETTING_OUTPUT_PPQN, &syncOutPpqnIndex, 4);
    std::memcpy(buffer + 8 + 4 * SETTING_INPUT_PPQN, &syncInPpqnIndex, 4);
    std::memcpy(buffer + 8 + 4 * SETTING_PITCH_CURVE, &refCheckNum, 4); // temp
    std::memcpy(buffer + 8 + 4 * SETTING_INPUT_QUANTIZE, &refCheckNum, 4); // temp
    std::memcpy(buffer + 8 + 4 * SETTING_BEAT, &beatNum, 4);
    std::memcpy(buffer + 8 + 4 * SETTING_TEMPO, &tempo, 2);
    std::memcpy(buffer + 8 + 4 * SETTING_TUPLET, &tuplet, 4);
    std::memcpy(buffer + 8 + 4 * SETTING_TIME_SIG, &newNumSteps, 4);
    std::memcpy(buffer + 8 + 4 * SETTING_OUTPUT, &refCheckNum, 4); // temp

    writePageToFlash(buffer, FLASH_DATA_ADDRESS + saveSector * FLASH_SECTOR_SIZE);
}

// should be called after current sector has been found
void loadSettings() {
    int startPoint = FLASH_SECTOR_SIZE * currentSettingsSector + 8;
    glitchChannel = getIntFromBuffer(flashData, startPoint + 4 * SETTING_GLITCH_CHANNEL);
    outputPulseLength = getIntFromBuffer(flashData, startPoint + 4 * SETTING_OUTPUT_PULSE_LENGTH);
    syncOutPpqnIndex = getIntFromBuffer(flashData, startPoint + 4 * SETTING_OUTPUT_PPQN);
    syncInPpqnIndex = getIntFromBuffer(flashData, startPoint + 4 * SETTING_INPUT_PPQN);
    // = getIntFromBuffer(flashData, startPoint + 4 * SETTING_PITCH_CURVE);
    // = getIntFromBuffer(flashData, startPoint + 4 * SETTING_INPUT_QUANTIZE);
    beatNum = getIntFromBuffer(flashData, startPoint + 4 * SETTING_BEAT);
    tempo = getIntFromBuffer(flashData, startPoint + 4 * SETTING_TEMPO);
    stepTime = 2646000 / (tempo * QUARTER_NOTE_STEPS);
    tuplet = getIntFromBuffer(flashData, startPoint + 4 * SETTING_TUPLET);
    quarterNoteDivision = quarterNoteDivisionRef[tuplet];
    nextQuarterNoteDivision = quarterNoteDivision;
    newNumSteps = getIntFromBuffer(flashData, startPoint + 4 * SETTING_TIME_SIG);
    numSteps = newNumSteps;
    // = getIntFromBuffer(flashData, startPoint + 4 * SETTING_OUTPUT);
}