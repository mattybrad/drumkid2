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
#include "pico/stdlib.h"
#include "pico/audio_i2s.h"
#include "pico/binary_info.h"
bi_decl(bi_3pins_with_names(PICO_AUDIO_I2S_DATA_PIN, "I2S DIN", PICO_AUDIO_I2S_CLOCK_PIN_BASE, "I2S BCK", PICO_AUDIO_I2S_CLOCK_PIN_BASE + 1, "I2S LRCK"));

// Drumkid classes
#include "Sample.h"
#include "Beat.h"
#include "drumkid.h"

// move to header file at some point
uint chance = 0;
float crush = 0;
float volume = 0;
int activeButton = -1;

bool sdSafeLoadTemp = false;
const uint8_t *flashData = (const uint8_t *)(XIP_BASE + FLASH_DATA_ADDRESS);
const uint8_t *flashAudio = (const uint8_t *)(XIP_BASE + FLASH_AUDIO_ADDRESS);

// main function, obviously
int main()
{
    stdio_init_all();
    time_init();

    printf("Drumkid V2\n");

    initGpio();
    initSamplesFromFlash();
    initBeats();

    struct audio_buffer_pool *ap = init_audio();

    updateLedDisplay(1234);
    add_repeating_timer_us(100, mainTimerLogic, NULL, &mainTimer);

    // main loop, runs forever
    while (true)
    {
        // something to do with audio that i don't fully understand
        struct audio_buffer *buffer = take_audio_buffer(ap, true);
        int16_t *bufferSamples = (int16_t *)buffer->buffer->bytes;

        // update audio output
        for (uint i = 0; i < buffer->max_sample_count*2; i+=2)
        {
            // sample updates go here
            float floatValue1 = 0.0;
            float floatValue2 = 0.0;
            for (int j = 0; j < NUM_SAMPLES; j++)
            {
                samples[j].update();
                if(j==0)
                {
                    floatValue1 += (float)samples[j].value;
                }
                floatValue2 += (float)samples[j].value;
            }

            floatValue1 *= 0.25 * volume; // temp?
            floatValue2 *= 0.25 * volume; // temp?

            bufferSamples[i] = (int)floatValue1;
            bufferSamples[i + 1] = (int)floatValue2;

            // increment step if needed
            if (beatPlaying)
                stepPosition++;
            if (stepPosition >= samplesPerStep)
            {
                stepPosition = 0;
                step++;
                if (step == 32)
                {
                    step = 0;
                }
                if (step % 4 == 0)
                {
                    pulseGpio(SYNC_OUT, 20000);
                    pulseLed(0, 50000);
                }
                for (int j = 0; j < NUM_SAMPLES; j++)
                {
                    if (beats[beatNum].hits[j][step]) {
                        samples[j].position = 0.0;
                        pulseGpio(TRIGGER_OUT_PINS[j], 20000);
                    }
                    // basic initial "chance" implementation":
                    int randNum = rand() % 4096;
                    if (chance > randNum)
                    {
                        samples[j].position = 0.0;
                        pulseGpio(TRIGGER_OUT_PINS[j], 20000);
                    }
                }
            }
        }
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);

        if(sdSafeLoadTemp) {
            sdSafeLoadTemp = false;
            loadSamplesFromSD();
        }
    }
    return 0;
}

uint pulseTimerNum = 0;
const uint maxPulseTimers = 10;
repeating_timer_t pulseTimers[maxPulseTimers];

bool onGpioPulseTimeout(repeating_timer_t *rt)
{
    uint gpioNum = (uint)(rt->user_data);
    gpio_put(gpioNum, 0);
    return false;
}

void pulseGpio(uint gpioNum, uint16_t pulseLengthMicros)
{
    gpio_put(gpioNum, 1);
    add_repeating_timer_us(pulseLengthMicros, onGpioPulseTimeout, (void *)gpioNum, &pulseTimers[pulseTimerNum]);
    pulseTimerNum = (pulseTimerNum + 1) % maxPulseTimers;
}

bool onLedPulseTimeout(repeating_timer_t *rt)
{
    uint ledNum = (uint)(rt->user_data);
    bitWrite(singleLedData, ledNum, 0);
    return false;
}

void pulseLed(uint ledNum, uint16_t pulseLengthMicros)
{
    bitWrite(singleLedData, ledNum, 1);
    add_repeating_timer_us(pulseLengthMicros, onLedPulseTimeout, (void *)ledNum, &pulseTimers[pulseTimerNum]);
    pulseTimerNum = (pulseTimerNum + 1) % maxPulseTimers;
}

void initGpio() {
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
    for(int i=0; i<4; i++) {
        gpio_init(TRIGGER_OUT_PINS[i]);
        gpio_set_dir(TRIGGER_OUT_PINS[i], GPIO_OUT);
    };
}

void initBeats() {
    // temporarily defining preset beats here
    beats[0].addHit(0, 0);
    beats[0].addHit(0, 16);
    beats[0].addHit(1, 8);
    beats[0].addHit(1, 24);
    beats[0].addHit(1, 28);
    beats[0].addHit(2, 0);
    beats[0].addHit(2, 4);
    beats[0].addHit(2, 8);
    beats[0].addHit(2, 12);
    beats[0].addHit(2, 16);
    beats[0].addHit(2, 20);
    beats[0].addHit(2, 24);
    beats[0].addHit(2, 28);
    beats[1].addHit(0, 0);
    beats[1].addHit(0, 16);
    beats[1].addHit(1, 8);
    beats[1].addHit(1, 24);
    beats[1].addHit(1, 28);
    beats[2].addHit(0, 0);
    beats[2].addHit(0, 8);
    beats[2].addHit(0, 16);
    beats[2].addHit(0, 24);
}

uint16_t microsSinceSyncIn = 0;

bool mainTimerLogic(repeating_timer_t *rt) {
    updateLeds();
    updateShiftRegButtons();
    updateAnalog();

    // update effects etc
    samplesPerStep = (int)(sampleRate * 7.5 / tempo); // 7.5 because it's 60/8, 8 subdivisions per quarter note..?
    
    // knobs
    chance = analogReadings[0];
    crush = ((float)analogReadings[10]) / 4095.0;
    volume = ((float)analogReadings[11]) / 4095.0;


    // CV
    //samples[1].speed = 0.25 + 4.0 * ((float)analogReadings[14]) / 4095.0;

    return true;
}

// Borrowed/adapted from pico-playground
struct audio_buffer_pool *init_audio()
{

    static audio_format_t audio_format = {
        (uint32_t) sampleRate,
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
    if(buttonState) {
        switch (buttonNum)
        {
        case BUTTON_START_STOP:
            beatPlaying = !beatPlaying;
            if(beatPlaying) {
                for (int j = 0; j < NUM_SAMPLES; j++)
                {
                    samples[j].position = 0.0; // temp
                }
                step = 0;
                stepPosition = 0;
            }
            break;
        case BUTTON_INC:
            handleIncDec(true);
            printf("+\n");
            break;
        case BUTTON_DEC:
            handleIncDec(false);
            printf("-\n");
            break;
        case BUTTON_SD_TEMP:
            sdSafeLoadTemp = true;
            break;
        case BUTTON_MANUAL_TEMPO:
            activeButton = BUTTON_MANUAL_TEMPO;
            displayTempo();
        default:
            printf("(button not assigned)\n");
        }
    } else if(!buttonState) {
        
    }
}

void handleIncDec(bool isInc) {
    switch(activeButton) {
        case BUTTON_MANUAL_TEMPO:
        tempo += isInc ? 1 : -1;
        if(tempo>9999) tempo = 9999;
        else if(tempo<10) tempo = 10;
        // todo: press and hold function
        displayTempo();
        break;
    }
}

void displayTempo() {
    updateLedDisplay((int)tempo);
}

void updateShiftRegButtons()
{
    // update shift register
    if(shiftRegInPhase == 0) {
        gpio_put(LOAD_165, 0);
    } else if(shiftRegInPhase == 1) {
        gpio_put(LOAD_165, 1);
    } else if(shiftRegInPhase == 2) {
        if(microsSinceChange[shiftRegInLoopNum] > 20000) {
            bool buttonState = gpio_get(DATA_165);
            if (buttonState != buttonStableStates[shiftRegInLoopNum])
            {
                buttonStableStates[shiftRegInLoopNum] = buttonState;
                microsSinceChange[shiftRegInLoopNum] = 0;
                printf("button %d: %d\n", shiftRegInLoopNum, buttonState?1:0);
                handleButtonChange(shiftRegInLoopNum, buttonState);
            }
        } else {
            microsSinceChange[shiftRegInLoopNum] += 2000; // total guess at us value for a full shift reg cycle, temporary
        }
        gpio_put(CLOCK_165, 0);
    } else if(shiftRegInPhase == 3) {
        gpio_put(CLOCK_165, 1);
    }
    shiftRegInPhase ++;
    if (shiftRegInPhase == 4) {
        shiftRegInLoopNum ++;
        if(shiftRegInLoopNum < 16) {
            shiftRegInPhase = 2;
        } else {
            shiftRegInPhase = 0;
            shiftRegInLoopNum = 0;
        }
    }
}

void updateAnalog()
{
    if(analogPhase == 0) {
        gpio_put(MUX_ADDR_A, bitRead(analogLoopNum, 0));
        gpio_put(MUX_ADDR_B, bitRead(analogLoopNum, 1));
        gpio_put(MUX_ADDR_C, bitRead(analogLoopNum, 2));
    } else if(analogPhase == 1) {
        adc_select_input(0);
        analogReadings[analogLoopNum] = 4095 - adc_read();
        adc_select_input(1);
        analogReadings[analogLoopNum + 8] = 4095 - adc_read();
    }

    analogPhase ++;
    if(analogPhase == 2) {
        analogPhase = 0;
        analogLoopNum ++;
        if(analogLoopNum == 8) {
            analogLoopNum = 0;

            // temp
            for(int i=0; i<16; i++) {
                if(buttonStableStates[i]) {
                    //updateLedDisplay(analogReadings[i]);
                }
            }
        }
    }
}

void loadSamplesFromSD() {
    int pageNum = 0;
    const char* filenames[NUM_SAMPLES] = {"samples/kick.wav","samples/snare.wav","samples/closedhat.wav","samples/tom.wav"};
    uint32_t sampleStartPoints[NUM_SAMPLES] = {0};
    uint32_t sampleLengths[NUM_SAMPLES] = {0};

    for(int n=0; n<NUM_SAMPLES; n++) {
        sd_card_t *pSD = sd_get_by_num(0);
        FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);
        if (FR_OK != fr)
            panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
        FIL fil;
        //const char *const filename = "samples/kick.wav";
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
        for(int i=0; i<3; i++) {
            fr = f_read(&fil, descriptorBuffer, sizeof descriptorBuffer, &br);
            if(i==0||i==2) printf("check... %s\n", descriptorBuffer);
        }

        while(!foundDataChunk) {
            fr = f_read(&fil, descriptorBuffer, sizeof descriptorBuffer, &br);
            printf("descriptor=%s, ", descriptorBuffer);
            fr = f_read(&fil, sizeBuffer, sizeof sizeBuffer, &br);
            uint32_t chunkSize = sizeBuffer[0] | sizeBuffer[1] << 8 | sizeBuffer[2] << 16 | sizeBuffer[3] << 24;
            printf("size=%d bytes\n", chunkSize);
            uint brTotal = 0;
            for (;;)
            {
                uint bytesToRead = std::min(sizeof sampleDataBuffer, (uint) chunkSize);
                fr = f_read(&fil, sampleDataBuffer, bytesToRead, &br);
                if (br == 0)
                    break;

                if (strncmp(descriptorBuffer, "data", 4) == 0) {
                    if (!foundDataChunk)
                    {
                        sampleStartPoints[n] = pageNum*FLASH_PAGE_SIZE;
                        sampleLengths[n] = chunkSize;
                        printf("sample %d, start on page %d, length %d\n", n, sampleStartPoints[n], sampleLengths[n]);
                    }
                    foundDataChunk = true;

                    writePageToFlash(sampleDataBuffer, FLASH_AUDIO_ADDRESS + pageNum * FLASH_PAGE_SIZE);

                    pageNum++;
                }

                brTotal += br;
                if(brTotal == chunkSize) {
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

    // write metadata at start
    uint8_t metadataBuffer[FLASH_PAGE_SIZE] = {0};
    print_buf(metadataBuffer, FLASH_PAGE_SIZE);
    printf("\n\n");
    for(int n=0;n<NUM_SAMPLES;n++) {
        std::memcpy(metadataBuffer + n*4, &sampleStartPoints[n], 4);
        std::memcpy(metadataBuffer + n*4 + 16, &sampleLengths[n], 4);
    }
    print_buf(metadataBuffer, FLASH_PAGE_SIZE);
    printf("\n\n");
    writePageToFlash(metadataBuffer, FLASH_DATA_ADDRESS);
    print_buf(flashData, FLASH_PAGE_SIZE);
    printf("\n\n");

    initSamplesFromFlash();
}

void initSamplesFromFlash() {
    for(int n=0; n<NUM_SAMPLES; n++) {
        uint sampleStart = flashData[n * 4] | flashData[n * 4 + 1] << 8 | flashData[n * 4 + 2] << 16 | flashData[n * 4 + 3] << 24;
        uint sampleLength = flashData[n * 4 + 16] | flashData[n * 4 + 17] << 8 | flashData[n * 4 + 18] << 16 | flashData[n * 4 + 19] << 24;
        printf("start %d, length %d\n",sampleStart,sampleLength);
        
        for (int i = 0; i < samples[n].length && i < sampleLength/2; i++)
        {
            int16_t thisSample = flashAudio[i * 2 + 1 + sampleStart] << 8 | flashAudio[i * 2 + sampleStart];
            samples[n].sampleData[i] = thisSample;
        }
        samples[n].length = sampleLength/2;
        samples[n].position = (float)samples[n].length;
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
            bitWrite(storedLedData[i], 4+i, 0);
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
        if(shiftRegOutPhase == 3) {
            shiftRegOutPhase = 0;
            shiftRegOutLoopNum ++;
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
    
    uint pageNum = address/FLASH_PAGE_SIZE;
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