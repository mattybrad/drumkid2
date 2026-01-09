/*

Drumkid Eurorack module firmware
by Bradshaw Instruments (Matt Bradshaw)
Version: 2.0.0
Started Jan 2026

*/

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"

void secondCoreCode() {
    while (true) {
        // Code for the second core
        sleep_ms(50);
        multicore_fifo_push_blocking(1);
        sleep_ms(5);
        multicore_fifo_push_blocking(0);
    }
}

int main()
{
    stdio_init_all();
    multicore_launch_core1(secondCoreCode); // launch second core

    gpio_init(17);
    gpio_set_dir(17, GPIO_OUT);

    while (true) {
        uint32_t received = multicore_fifo_pop_blocking();
        printf("Received from core 1: %d\n", received);

        if (received == 0) {
            gpio_put(17, 0); // Set GPIO 17 high
        } else {
            gpio_put(17, 1); // Set GPIO 17 low
        }
    }
}
