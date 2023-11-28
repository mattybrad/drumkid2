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

// Audio data (temporary, will be loaded from flash)
#include "kick.h"
#include "snare.h"
#include "closedhat.h"

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

bool sdSafeLoadTemp = false;

// main function, obviously
int main()
{
    stdio_init_all();
    time_init();

    printf("Drumkid V2\n");

    initGpio();
    initSamples();
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

        // update tempo (could put this somewhere else if using too much CPU)
        tempo = 50.0 + (float)analogReadings[2] * 0.05;
        samplesPerStep = (int)(sampleRate * 7.5 / tempo); // 7.5 because it's 60/8, 8 subdivisions per quarter note..?

        // update audio output
        for (uint i = 0; i < buffer->max_sample_count*2; i+=2)
        {
            // sample updates go here
            float floatValue1 = 0.0;
            float floatValue2 = 0.0;
            for (int j = 0; j < 3; j++)
            {
                samples[j].update();
                if(j==0)
                {
                    floatValue1 += (float)samples[j].value;
                }
                else
                {
                    floatValue2 += (float)samples[j].value;
                }
            }
            floatValue1 *= 0.25; // temp?
            floatValue2 *= 0.25; // temp?
            bufferSamples[i] = (int)floatValue1;
            bufferSamples[i+1] = (int)floatValue2;

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
                for (int j = 0; j < 3; j++)
                {
                    if (beats[beatNum].hits[j][step]) {
                        samples[j].position = 0.0;
                        pulseGpio(TRIGGER_OUT_PINS[j], 20000);
                    }
                    // basic initial "chance" implementation":
                    int randNum = rand() % 4096;
                    if (analogReadings[0] > randNum)
                    {
                        samples[j].position = 0.0;
                        pulseGpio(TRIGGER_OUT_PINS[j], 20000);
                    }
                }
            }
        }
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);

        // move this to a timer
        samples[2].speed = 0.25 + 4.0 * ((float)analogReadings[12]) / 4095.0;

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

void initSamples() {
    // init samples (temporary, will be from flash)
    samples[0].sampleData = sampleKick;
    samples[0].length = sampleKickLength;
    samples[1].sampleData = sampleSnare;
    samples[1].length = sampleSnareLength;
    samples[2].sampleData = sampleClosedHat;
    samples[2].length = sampleClosedHatLength;
    for (int i = 0; i < 3; i++)
    {
        samples[i].position = (float)samples[i].length;
    }
}

uint16_t microsSinceSyncIn = 0;

bool mainTimerLogic(repeating_timer_t *rt) {
    updateLeds();
    updateShiftRegButtons();
    updateAnalog();

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
                for (int j = 0; j < 3; j++)
                {
                    samples[j].position = 0.0; // temp
                }
                step = 0;
                stepPosition = 0;
            }
            break;
        case BUTTON_INC:
            beatNum++;
            if (beatNum == 8)
            {
                beatNum = 0;
            }
            printf("+\n");
            break;
        case BUTTON_DEC:
            beatNum--;
            if (beatNum == -1)
            {
                beatNum = 7;
            }
            printf("-\n");
            break;
        case BUTTON_SD_TEMP:
            sdSafeLoadTemp = true;
            break;
        default:
            printf("(button not assigned)\n");
        }
    } else if(!buttonState) {
        // temp, show button num on release
        updateLedDisplay(buttonNum);
    }
}

void updateShiftRegButtons()
{
    // update shift register
    if(shiftRegInPhase == 0) {
        gpio_put(LOAD_165, 0);
    } else if(shiftRegInPhase == 1) {
        gpio_put(LOAD_165, 1);
    } else if(shiftRegInPhase == 2) {
        if(microsSinceChange[shiftRegInLoopNum] > 50000) {
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
        analogReadings[analogLoopNum] = adc_read();
        adc_select_input(1);
        analogReadings[analogLoopNum + 8] = adc_read();
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
                    updateLedDisplay(analogReadings[i]);
                }
            }
        }
    }
}

void loadSamplesFromSD() {
    puts("load from SD...");
    // See FatFs - Generic FAT Filesystem Module, "Application Interface",
    // http://elm-chan.org/fsw/ff/00index_e.html
    sd_card_t *pSD = sd_get_by_num(0);
    FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);
    if (FR_OK != fr)
        panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    FIL fil;
    const char *const filename = "filename.txt";
    fr = f_open(&fil, filename, FA_OPEN_APPEND | FA_WRITE);
    if (FR_OK != fr && FR_EXIST != fr)
        panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
    if (f_printf(&fil, "Hello, world!\n") < 0)
    {
        printf("f_printf failed\n");
    }
    fr = f_close(&fil);
    if (FR_OK != fr)
    {
        printf("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
    }
    f_unmount(pSD->pcName);

    puts("Goodbye, world!");
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
