#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#if PICO_ON_DEVICE

#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"

#endif

#include "pico/stdlib.h"

#include "pico/audio_i2s.h"

#if PICO_ON_DEVICE
#include "pico/binary_info.h"
bi_decl(bi_3pins_with_names(PICO_AUDIO_I2S_DATA_PIN, "I2S DIN", PICO_AUDIO_I2S_CLOCK_PIN_BASE, "I2S BCK", PICO_AUDIO_I2S_CLOCK_PIN_BASE + 1, "I2S LRCK"));
#endif

#define SAMPLES_PER_BUFFER 256

int thing = 0;
int step = 0;
int kickPos = 0;
int snarePos = 0;
int closedHatPos = 0;

#define LED_PIN 25
#define DATA_595 6
#define CLOCK_595 7
#define LATCH_595 8

int main()
{

    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_init(DATA_595);
    gpio_set_dir(DATA_595, GPIO_OUT);
    gpio_init(CLOCK_595);
    gpio_set_dir(CLOCK_595, GPIO_OUT);
    gpio_init(LATCH_595);
    gpio_set_dir(LATCH_595, GPIO_OUT);

    int count = 0;

    while (true)
    {
        /*int delay = 500;
        gpio_put(LED_PIN, 1);
        sleep_ms(delay);
        gpio_put(LED_PIN, 0);
        sleep_ms(delay);*/

        gpio_put(LATCH_595, 0);
        for(int i=0; i<8; i++) {
            gpio_put(DATA_595, i==count);
            gpio_put(CLOCK_595, 0);
            sleep_ms(1);
            gpio_put(CLOCK_595, 1);
            sleep_ms(1);
            gpio_put(CLOCK_595, 0);
            sleep_ms(1);
        }
        gpio_put(LATCH_595, 1);
        sleep_ms(1000);
        count ++;
        if(count == 8) count = 0;
    }
    return 0;
}
