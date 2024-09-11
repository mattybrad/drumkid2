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
#include "drumkid.h"
#include "sn74595.pio.h"

// globals, organise later
int64_t currentTime = 0; // samples
uint16_t step = 0;
int numSteps = 4 * QUARTER_NOTE_STEPS;
uint64_t lastClockIn = 0; // microseconds
bool tempExtSync = false;

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
    if(tempExtSync) {
        handleSyncPulse();
    }
}

int tempDigit = 0;
bool updateOutputShiftRegisters(repeating_timer_t *rt)
{
    uint b = 0b1111111100000000;
    bitWrite(b, 8+tempDigit, false);
    bitWrite(b, tempDigit, true);
    sn74595::shiftreg_send(b);
    tempDigit = (tempDigit + 1) % 4;

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
    beats[0].addHit(0, 0, 255);
    beats[0].addHit(1, 2*3360, 255);
    int numHatsTemp = 16;
    for(int i=0; i<numHatsTemp; i++) {
        beats[0].addHit(2, (i*4*3360)/numHatsTemp, 64);
    }

    // interrupt for clock in pulse
    gpio_set_irq_enabled_with_callback(SYNC_IN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);



    struct audio_buffer_pool *ap = init_audio();

    // testing PIO stuff
    sn74595::shiftreg_init();
    // end testing PIO

    add_repeating_timer_ms(2, updateOutputShiftRegisters, NULL, &outputShiftRegistersTimer);

    beatPlaying = true;

    if (!tempExtSync)
    {
        ppqn = 1;
        deltaT = 500000;
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
            currentTime ++;

            // sample updates go here
            int32_t out1 = 0;
            int32_t out2 = 0;

            if(beatStarted && currentTime >= nextPulseTime) {
                prevPulseTime = nextPulseTime;
                nextPulseTime += (44100*deltaT)/(1000000);
                if(!tempExtSync) {
                    nextPredictedPulseTime = nextPulseTime;
                }
            }

            int64_t microstep = (3360 * (currentTime - prevPulseTime)) / (ppqn * (nextPulseTime - prevPulseTime));
            bool checkBeat = false;

            if(beatStarted && currentTime < nextPredictedPulseTime && microstep != lastStep) {
                checkBeat = true;
            }
            lastStep = microstep;

            for (int j = 0; j < NUM_SAMPLES; j++)
            {
                if(checkBeat) {
                    if(beats[0].getHit(j, microstep + step)) {
                        samples[j].queueHit(currentTime, 0, 4095);
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
                    step = (step + (3360/ppqn)) % (4*3360);
                }
            }
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