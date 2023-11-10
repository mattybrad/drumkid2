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
#if PICO_ON_DEVICE
#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"
#include "hardware/adc.h"
#endif
#include "pico/stdlib.h"
#include "pico/audio_i2s.h"
#if PICO_ON_DEVICE
#include "pico/binary_info.h"
bi_decl(bi_3pins_with_names(PICO_AUDIO_I2S_DATA_PIN, "I2S DIN", PICO_AUDIO_I2S_CLOCK_PIN_BASE, "I2S BCK", PICO_AUDIO_I2S_CLOCK_PIN_BASE + 1, "I2S LRCK"));
#endif

// Borrowing some useful Arduino macros
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))

#define SAMPLES_PER_BUFFER 32 // 256 works well

// pins (updated for PCB 2.02)
#define MUX_ADDR_A 19
#define MUX_ADDR_B 20
#define MUX_ADDR_C 21
#define MUX_READ_POTS 26
#define MUX_READ_CV 27
#define LED_PIN 25
#define DATA_595 6
#define CLOCK_595 7
#define LATCH_595 8
#define LOAD_165 12
#define CLOCK_165 13
#define DATA_165 14
#define SYNC_IN 16
#define SYNC_OUT 17

// button numbers
#define BUTTON_INC 0
#define BUTTON_DEC 1
#define BUTTON_BEAT 6
#define BUTTON_START_STOP 7

// Drumkid classes
#include "Sample.h"
#include "Beat.h"

// Audio data (temporary, will be loaded from SD card?)
#include "kick.h"
#include "snare.h"
#include "closedhat.h"

// Beat variables
int step = 0;         // e.g. 0 to 31 for a 4/4 pattern of 8th-notes
int stepPosition = 0; // position within the step, measured in samples
float tempo = 120.0;  // BPM
int samplesPerStep;   // slower tempos give higher values
float sampleRate = 44100.0;
bool beatPlaying = false;
int beatNum = 0;
Beat beats[8];
Sample samples[3];

// temporary (ish?) LED variables (first 8 bits are the segments, next 4 are the character selects, final 4 are 3mm LEDs)
uint8_t sevenSegData[4] = {0b00000000, 0b00000000, 0b00000000, 0b00000000};
uint8_t singleLedData = 0b0010; // 4 x 3mm LEDs
uint16_t storedLedData[4] = {0, 0, 0, 0};
uint8_t sevenSegCharacters[10] = {
    0b11111100,
    0b01100000,
    0b11011010,
    0b11110010,
    0b01100110,
    0b10110110,
    0b10111110,
    0b11100000,
    0b11111110,
    0b11110110
};

// Borrowed/adapted from pico-playground
struct audio_buffer_pool *init_audio()
{

    static audio_format_t audio_format = {
        (uint32_t) sampleRate,
        AUDIO_BUFFER_FORMAT_PCM_S16,
        1,
    };

    static struct audio_buffer_format producer_format = {
        .format = &audio_format,
        .sample_stride = 2};

    struct audio_buffer_pool *producer_pool = audio_new_producer_pool(&producer_format, 3,
                                                                      SAMPLES_PER_BUFFER); // todo correct size
    bool __unused ok;
    const struct audio_format *output_format;
    struct audio_i2s_config config = {
        .data_pin = 9,
        .clock_pin_base = 10,
        .dma_channel = 0,
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
                step = 0;
                stepPosition = 0;
            }
            break;
        case BUTTON_BEAT:
            printf("change beat...\n");
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
        case 2:
            break;
        default:
            printf("(button not assigned)\n");
        }
    } else if(!buttonState) {
        // temp, show button num on release
        updateLedDisplay(buttonNum);
    }
}

// new button variables
int lastButtonChange = 0; // measured in number of loops of updateButtons function, i.e. a bit janky
bool buttonStableStates[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,}; // array of bools not good use of space, change
uint shiftRegInLoopNum = 0; // 0 to 15
uint shiftRegInPhase = 0;   // 0 or 1

bool updateButtons(repeating_timer_t *rt)
{
    lastButtonChange ++;

    // update shift register
    if(shiftRegInPhase == 0) {
        gpio_put(LOAD_165, 0);
    } else if(shiftRegInPhase == 1) {
        gpio_put(LOAD_165, 1);
    } else if(shiftRegInPhase == 2) {
        bool buttonState = gpio_get(DATA_165);
        if (buttonState != buttonStableStates[shiftRegInLoopNum] && lastButtonChange > 500) {
            buttonStableStates[shiftRegInLoopNum] = buttonState;
            lastButtonChange = 0;
            printf("button %d: %d\n", shiftRegInLoopNum, buttonState?1:0);
            handleButtonChange(shiftRegInLoopNum, buttonState);
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
    return true;
}

uint analogLoopNum = 0; // 0 to 7
uint analogPhase = 0; // 0 or 1
uint analogReadings[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

bool updateAnalog(repeating_timer_t *rt)
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

    return true;
}

void loadSamples() {
    sd_card_t *pSD = sd_get_by_num(0);
    FRESULT fr = f_mount(&pSD->fatfs, pSD->pcName, 1);
    if (FR_OK != fr)
        panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);

    FIL fil;
    const char *const filename = "clap.wav";
    fr = f_open(&fil, filename, FA_READ);
    printf("%s\n", (FR_OK != fr ? "Fail :(" : "OK"));
    if (FR_OK != fr)
        printf("f_open error: %s (%d)\n", FRESULT_str(fr), fr);
    fflush(stdout);

    int sampleLength = 10000;
    int sampleData[sampleLength];

    printf("reading audio file...\n");
    int pos = 0;
    while (!f_eof(&fil) && pos<44)
    {
        char c;
        UINT br;
        fr = f_read(&fil, &c, sizeof c, &br);
        if (FR_OK != fr)
            printf("f_read error: %s (%d)\n", FRESULT_str(fr), fr);
        else {
            printf("%c %d\n", c, c);
        }
        pos ++;
    }

    pos = 0;

    puts("audio data: ");

    while (!f_eof(&fil) && pos<sampleLength)
    {
        // int c = f_getc(f);
        int16_t d;
        UINT br;
        fr = f_read(&fil, &d, sizeof d, &br);
        printf("%d ", d);
        if (FR_OK != fr)
            printf("f_read error: %s (%d)\n", FRESULT_str(fr), fr);
        else
        {
            sampleData[pos] = d;
        }
        pos++;
    }

    sampleLength = pos;

    samples[1].sampleData = sampleData;
    samples[1].length = sampleLength;

    printf("audio length: %d\n", pos);

    f_unmount(pSD->pcName);
}

// new LED variables
uint shiftRegOutLoopNum = 0; // 0 to 15
uint shiftRegOutPhase = 0;   // 0, 1, or 2
uint sevenSegCharNum = 0;

bool updateLeds(repeating_timer_t *rt)
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
    return true;
}

// main function, obviously
int main()
{
    stdio_init_all();
    time_init();

    sleep_ms(1000); // hacky, allows serial monitor to connect, remove later
    puts("Drumkid V2");

    // init GPIO
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

    struct audio_buffer_pool *ap = init_audio();

    // init samples (temporary, will be from SD card?)
    samples[0].sampleData = sampleKick;
    samples[0].length = sampleKickLength;
    samples[1].sampleData = sampleSnare;
    samples[1].length = sampleSnareLength;
    samples[2].sampleData = sampleClosedHat;
    samples[2].length = sampleClosedHatLength;
    for(int i=0; i<3; i++) {
        samples[i].position = (float) samples[i].length;
    }

    // skipping this for now because the SD card often timed out, which was annoying while trying to code other stuff. investigate later
    //loadSamples();

    // temporarily defining preset beats here
    beats[0].addHit(0,0);
    beats[0].addHit(0,16);
    beats[0].addHit(1,8);
    beats[0].addHit(1,24);
    beats[0].addHit(1,28);
    beats[0].addHit(2,0);
    beats[0].addHit(2,4);
    beats[0].addHit(2,8);
    beats[0].addHit(2,12);
    beats[0].addHit(2,16);
    beats[0].addHit(2,20);
    beats[0].addHit(2,24);
    beats[0].addHit(2,28);
    beats[1].addHit(0, 0);
    beats[1].addHit(0, 16);
    beats[1].addHit(1, 8);
    beats[1].addHit(1, 24);
    beats[1].addHit(1, 28);
    beats[2].addHit(0, 0);
    beats[2].addHit(0, 8);
    beats[2].addHit(0, 16);
    beats[2].addHit(0, 24);

    updateLedDisplay(9999);
    repeating_timer_t ledTimer;
    add_repeating_timer_us(100, updateLeds, NULL, &ledTimer);
    repeating_timer_t buttonTimer;
    add_repeating_timer_us(100, updateButtons, NULL, &buttonTimer);
    repeating_timer_t analogTimer;
    add_repeating_timer_us(100, updateAnalog, NULL, &analogTimer);

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
        for (uint i = 0; i < buffer->max_sample_count; i++)
        {
            // sample updates go here
            float floatValue = 0.0;
            for(int j=0; j<3; j++) {
                samples[j].update();
                floatValue += (float)samples[j].value;
            }
            floatValue *= 0.25;
            bufferSamples[i] = (int)floatValue;

            // increment step if needed
            if(beatPlaying) stepPosition ++;
            if(stepPosition >= samplesPerStep) {
                stepPosition = 0;
                step ++;
                if(step == 32) {
                    step = 0;
                }
                if(step % 4 == 0) {
                    // TODO: shorter, constant pulse length
                    gpio_put(SYNC_OUT, 1);
                    //ledData = 0;
                    //bitWrite(ledData, step / 4, 1);
                } else {
                    // TODO: shorter, constant pulse length
                    gpio_put(SYNC_OUT, 0);
                }
                for(int j=0; j<3; j++) {
                    if(beats[beatNum].hits[j][step]) samples[j].position = 0.0;
                    // basic initial "chance" implementation":
                    int randNum = rand() % 4096;
                    if(analogReadings[0] > randNum) {
                        samples[j].position = 0.0;
                    }
                }
            }
        }
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);

        // move this to a timer
        samples[2].speed = 0.25 + 4.0 * ((float)analogReadings[1]) / 4095.0;
    }
    return 0;
}
