#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// I2C defines
// This example will use I2C0 on GPIO16 (SDA) and GPIO17 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 16
#define I2C_SCL 17
#define ADDR 0b100000 // 0x20

#define IODIR 0x00 // input / output
#define OLAT  0x0A // output high / low
#define GPIO 0x09 // input high / low

// set_pin
void setPin(unsigned char address, unsigned char reg, unsigned char value)  {
    uint8_t buf[2];
    buf[0] = reg; // OLAT or IODIR
    buf[1] = value; // set input or output / high or low
    i2c_write_blocking(I2C_PORT, address, buf, 2, false); // write chip
}

// read_pin
unsigned char readPin(unsigned char address, unsigned char reg)  {
    uint8_t value;
    i2c_write_blocking(I2C_PORT, address, &reg, 1, true);   // send register
    i2c_read_blocking(I2C_PORT, address, &value, 1, false); // read value
    return value;
}

// init_chip func
void initChip() {
    setPin(ADDR, IODIR, 0b01111111); // configure inputs / outputs
}

int main()
{

    stdio_init_all();
    // sleep_ms(20000); // wait for serial monitor

    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400*1000);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);

    sleep_ms(100);
    initChip();

    // set-up-to flash pico light
    gpio_init(15);
    gpio_set_dir(15, GPIO_OUT);

    while (true) {
        uint8_t value = readPin(ADDR, GPIO); // read to see if pins are high / low
        if ((value & 0b1) == 1) {
            printf("%d", value);
            setPin(ADDR, OLAT, 0b00000000); // MCP23008 GP7 ON
        }
        else {
            setPin(ADDR, OLAT, 0b10000000); // MCP23008 GP7 OFF
            printf("%d", value);
        }

        // // flash pico led
        // gpio_put(15, 1);   // Pico LED ON
        // sleep_ms(500);

        // gpio_put(15, 0);   // Pico LED OFF
        // sleep_ms(500);
    }
}