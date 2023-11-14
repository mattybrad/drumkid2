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
#include "pico/stdlib.h"
#include "pico/audio_i2s.h"
#include "pico/binary_info.h"
bi_decl(bi_3pins_with_names(PICO_AUDIO_I2S_DATA_PIN, "I2S DIN", PICO_AUDIO_I2S_CLOCK_PIN_BASE, "I2S BCK", PICO_AUDIO_I2S_CLOCK_PIN_BASE + 1, "I2S LRCK"));

// Drumkid classes
#include "Sample.h"
#include "Beat.h"
#include "dk.h"

// Audio data (temporary, will be loaded from flash)
#include "kick.h"
#include "snare.h"
#include "closedhat.h"

// main function, obviously
int main()
{
    stdio_init_all();
    time_init();

    sleep_ms(1000); // hacky, allows serial monitor to connect, remove later
    puts("Drumkid V2");

    struct audio_buffer_pool *ap = init_audio();

    initGpio();
    initSamples();
    initBeats();
    updateLedDisplay(9999);

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
        for (uint i = 0; i < buffer->max_sample_count; i++)
        {
            // sample updates go here
            float floatValue = 0.0;
            for (int j = 0; j < 3; j++)
            {
                samples[j].update();
                floatValue += (float)samples[j].value;
            }
            floatValue *= 0.25;
            bufferSamples[i] = (int)floatValue;

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
                    // TODO: shorter, constant pulse length
                    gpio_put(SYNC_OUT, 1);
                    // ledData = 0;
                    // bitWrite(ledData, step / 4, 1);
                }
                else
                {
                    // TODO: shorter, constant pulse length
                    gpio_put(SYNC_OUT, 0);
                }
                for (int j = 0; j < 3; j++)
                {
                    if (beats[beatNum].hits[j][step])
                        samples[j].position = 0.0;
                    // basic initial "chance" implementation":
                    int randNum = rand() % 4096;
                    if (analogReadings[0] > randNum)
                    {
                        samples[j].position = 0.0;
                    }
                }
            }
        }
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);

        // move this to a timer
        samples[2].speed = 0.25 + 4.0 * ((float)analogReadings[12]) / 4095.0;
    }
    return 0;
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
    gpio_init(BUTTON_PIN_START_STOP);
    gpio_set_dir(BUTTON_PIN_START_STOP, GPIO_IN);
    gpio_pull_up(BUTTON_PIN_START_STOP);
    gpio_init(BUTTON_PIN_TAP_TEMPO);
    gpio_set_dir(BUTTON_PIN_TAP_TEMPO, GPIO_IN);
    gpio_pull_up(BUTTON_PIN_TAP_TEMPO);
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

bool mainTimerLogic(repeating_timer_t *rt) {
    updateLeds();
    updateShiftRegButtons();
    updateStandardButtons();
    updateAnalog();
    puts("test");
    return true;
}

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

void updateShiftRegButtons()
{
    // update shift register
    if(shiftRegInPhase == 0) {
        gpio_put(LOAD_165, 0);
    } else if(shiftRegInPhase == 1) {
        gpio_put(LOAD_165, 1);
    } else if(shiftRegInPhase == 2) {
        if(millisSinceChange[shiftRegInLoopNum] > 50) {
            bool buttonState = gpio_get(DATA_165);
            if (buttonState != buttonStableStates[shiftRegInLoopNum])
            {
                buttonStableStates[shiftRegInLoopNum] = buttonState;
                millisSinceChange[shiftRegInLoopNum] = 0;
                printf("button %d: %d\n", shiftRegInLoopNum, buttonState?1:0);
                handleButtonChange(shiftRegInLoopNum, buttonState);
            }
        } else {
            millisSinceChange[shiftRegInLoopNum] += 2; // total guess at ms value for a full shift reg cycle, temporary
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

void updateStandardButtons()
{
    for(int i=16; i<18; i++) {
        if (millisSinceChange[i] > 50)
        {
            bool buttonState = !gpio_get(i-16); // standard buttons on pins 0 and 1
            if (buttonState != buttonStableStates[i])
            {
                buttonStableStates[i] = buttonState;
                millisSinceChange[i] = 0;
                printf("button %d: %d\n", i, buttonState ? 1 : 0);
                handleButtonChange(i, buttonState);
            }
        }
        else
        {
            millisSinceChange[i] += 1; // total guess at ms value for a full shift reg cycle, temporary
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

bool syncInLedOff(repeating_timer_t *rt) {
    bitWrite(singleLedData, 0, 0);
    return false;
}

void flashSyncInLed() {
    bitWrite(singleLedData, 0, 1);
    add_repeating_timer_ms(500, syncInLedOff, NULL, &syncLedTimer);
}