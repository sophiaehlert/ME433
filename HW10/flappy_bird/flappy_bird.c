#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#define BUTTON_PIN 12

int main() {
    stdio_init_all();

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);

    while (true) {
        // send 1 if button pressed, 0 if not, at 100Hz
        int pressed = gpio_get(BUTTON_PIN) == 0 ? 1 : 0;
        printf("%d\n", pressed);
        sleep_ms(10);
    }
}