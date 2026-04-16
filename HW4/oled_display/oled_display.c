#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "ssd1306.h"

// I2C defines
// I2C0 on GPIO14 (SDA) and GPIO15 (SCL) running at 400KHz
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define LEDPIN 16

int main()
{
    stdio_init_all();

    // I2C initialization (100Khz)
    i2c_init(I2C_PORT, 400*1000);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);

    // initialize ssd1306
    sleep_ms(100);
    ssd1306_setup();
    ssd1306_clear(); // make sure screen is clear
    ssd1306_update();

    // set up pico pin to flash LED
    gpio_init(16);
    gpio_set_dir(16, GPIO_OUT);

    while (true) {
        // // hearbeat code
        // ssd1306_drawPixel(0, 0, 1); // turn on top left pixel
        // ssd1306_update(); // update screen
        // gpio_put(LEDPIN, 1); // turn on led
        // sleep_ms(1000); // blink at 1Hz

        // ssd1306_drawPixel(0, 0, 0); // turn off top left pixel
        // ssd1306_update(); // update screen
        // gpio_put(LEDPIN, 0); // turn off led
        // sleep_ms(1000);

        // // draw a letter code
        // drawLetter(0, 0, 'a');
        // ssd1306_update(); // update screen
        // sleep_ms(1000); // blink at 1Hz

        ssd1306_clear();

        // int i = 15;
        char l1[50]; 
        sprintf(l1, "lorem ipsum dolor"); 
        drawMessage(0,0,l1); // draw starting at x=20,y=10
        
        char l2[50]; 
        sprintf(l2, "sit amet, consectetur"); 
        drawMessage(0,8,l2); // draw starting at x=20,y=10
        
        char l3[50]; 
        sprintf(l3, "adipiscing elit, sed"); 
        drawMessage(0,16,l3); // draw starting at x=20,y=10
        
        ssd1306_update();

    }
}
