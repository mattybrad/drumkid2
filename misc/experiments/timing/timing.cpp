#include <stdio.h>
#include "pico/stdlib.h"

void gpio_callback(uint gpio, uint32_t events)
{
    gpio_put(17, 1);
    busy_wait_us(100);
    gpio_put(17, 0);
}

int main()
{
    stdio_init_all();

    gpio_init(17);
    gpio_set_dir(17, GPIO_OUT);
    gpio_init(16);
    gpio_set_dir(16, GPIO_IN);

    gpio_set_irq_enabled_with_callback(16, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    while (true) {
        printf("Hello, world! TIMING\n");
        sleep_ms(1000);
    }
}
