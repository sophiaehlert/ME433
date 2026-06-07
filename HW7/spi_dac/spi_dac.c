#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

// pin definitions
#define SPI_PORT    spi0
#define PIN_MISO    16          // GP16
#define PIN_SCK     18          // GP18
#define PIN_MOSI    19          // GP19
#define PIN_CS_DAC  17

// CS helpers
static inline void cs_select(uint cs_pin) {
    asm volatile("nop \n nop \n nop");
    gpio_put(cs_pin, 0);
    asm volatile("nop \n nop \n nop");
}

static inline void cs_deselect(uint cs_pin) {
    asm volatile("nop \n nop \n nop");
    gpio_put(cs_pin, 1);
    asm volatile("nop \n nop \n nop");
}

// DAC write
// channel: 0 = channel A, 1 = channel B
// v: voltage 0.0 to 3.3
void writeDAC(int channel, float v) {
    uint8_t data[2];

    uint16_t myV = (uint16_t)(v / 3.3f * 1023);

    // build the full 16-bit word first, then split
    uint16_t word = 0;
    word |= ((channel & 0x1) << 15);  // bit 15: channel select
    word |= (0 << 14);                 // bit 14: unbuffered
    word |= (1 << 13);                 // bit 13: gain 1x
    word |= (1 << 12);                 // bit 12: not shutdown
    word |= ((myV & 0x3FF) << 2);     // bits 11-2: 10-bit data

    // split into two bytes, MSB first
    data[0] = (word >> 8) & 0xFF;
    data[1] = word & 0xFF;

    cs_select(PIN_CS_DAC);
    spi_write_blocking(SPI_PORT, data, 2);
    cs_deselect(PIN_CS_DAC);
}

int main()
{
    stdio_init_all();

    // SPI initialization
    spi_init(SPI_PORT, 20 * 1000 * 1000);  // 20 MHz max for MCP4912
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    // CS is manual (GPIO), not SPI peripheral
    gpio_init(PIN_CS_DAC);
    gpio_set_dir(PIN_CS_DAC, GPIO_OUT);
    gpio_put(PIN_CS_DAC, 1);  // start high (deselected)

    // signal parameters
    // 2Hz sine on channel A, 1Hz triangle on channel B
    // update rate = 100Hz (10ms steps) — 50x faster than 2Hz signal
    float t = 0.0f;
    const float dt = 0.01f;  // 10ms per step

    while (true) {
        // channel A: 2Hz sine wave, 0V to 3.3V
        float sine_v = (sinf(2.0f * M_PI * 2.0f * t) + 1.0f) / 2.0f * 3.3f;
        writeDAC(0, sine_v);

        // channel B: 1Hz triangle wave, 0V to 3.3V
        // triangle: use fmod to get position within period, then fold
        float pos = fmodf(t, 1.0f);  // position within 1Hz period (0 to 1)
        float tri_v;
        if (pos < 0.5f) {
            tri_v = pos * 2.0f * 3.3f;         // rising: 0 to 3.3V
        } else {
            tri_v = (1.0f - pos) * 2.0f * 3.3f; // falling: 3.3V to 0
        }
        writeDAC(1, tri_v);

        t += dt;
        sleep_ms(10);
        // test: channel A full voltage, channel B half voltage
        // writeDAC(0, 3.3f);   // 3.3V flat line
        // writeDAC(1, 1.65f);  // 1.65V flat line
    }
}