#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/pwm.h"

#define SERVO_PIN 16

void set_servo_angle(uint pin, float angle) {
    // Map 0–180° -> 650 (3.25% dc) –3000 (15% dc) µs
    float pulse = 650.0f + (angle / 180.0f) * 2350.0f;

    pwm_set_gpio_level(pin, (uint16_t)pulse);
}

int main() {
    stdio_init_all();

    gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);

    uint slice = pwm_gpio_to_slice_num(SERVO_PIN);

    // set PWM frequency to 50 Hz
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 125.0f); // div slow clock
    pwm_config_set_wrap(&config, 20000);    // wrap 20ms period

    pwm_init(slice, &config, true);

    while (1) {
        // sweep 0 to 180
        for (float angle = 0; angle <= 180; angle += 2) {
            set_servo_angle(SERVO_PIN, angle);
            sleep_ms(20);
        }

        // sweep 180 to 0
        for (float angle = 180; angle >= 0; angle -= 2) {
            set_servo_angle(SERVO_PIN, angle);
            sleep_ms(20);
        }
    }
}