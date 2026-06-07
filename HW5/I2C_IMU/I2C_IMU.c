#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "ssd1306.h"

// I2C (both OLED and IMU share i2c1)
#define I2C_PORT i2c1
#define I2C_SDA  14
#define I2C_SCL  15

// IMU address & registers
#define MPU6050_ADDR 0x68

#define CONFIG       0x1A
#define GYRO_CONFIG  0x1B
#define ACCEL_CONFIG 0x1C
#define PWR_MGMT_1   0x6B
#define PWR_MGMT_2   0x6C

#define ACCEL_XOUT_H 0x3B
#define ACCEL_XOUT_L 0x3C
#define ACCEL_YOUT_H 0x3D
#define ACCEL_YOUT_L 0x3E
#define ACCEL_ZOUT_H 0x3F
#define ACCEL_ZOUT_L 0x40
#define TEMP_OUT_H   0x41
#define TEMP_OUT_L   0x42
#define GYRO_XOUT_H  0x43
#define GYRO_XOUT_L  0x44
#define GYRO_YOUT_H  0x45
#define GYRO_YOUT_L  0x46
#define GYRO_ZOUT_H  0x47
#define GYRO_ZOUT_L  0x48
#define WHO_AM_I     0x75

// OLED display dimensions
#define OLED_W 128
#define OLED_H  32
#define CX (OLED_W / 2)   // center x = 64
#define CY (OLED_H / 2)   // center y = 16

// line drawing scale: max line length in pixels
// At 2g sensitivity, raw value ~16383 = 1g; scale so 1g = 15 pixels.
#define SCALE 0.000916f   // 15.0 / 16383.0

// IMU helpers

static void imu_write_reg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    i2c_write_blocking(I2C_PORT, MPU6050_ADDR, buf, 2, false);
}

static uint8_t imu_read_reg(uint8_t reg) {
    uint8_t val;
    i2c_write_blocking(I2C_PORT, MPU6050_ADDR, &reg, 1, true);  // true = keep bus
    i2c_read_blocking (I2C_PORT, MPU6050_ADDR, &val, 1, false);
    return val;
}

// Check WHO_AM_I — halt with LED blink on failure
static void imu_check_whoami() {
    uint8_t id = imu_read_reg(WHO_AM_I);

        // // show on screen
        // ssd1306_clear();
        // char buf[20];
        // sprintf(buf, "WHO_AM_I: 0x%02X", id);
        // drawMessage(0, 0, buf);
        // ssd1306_update();
        // sleep_ms(3000);

    if (id != 0x68 && id != 0x98) {
        // wrong response — blink forever
        gpio_init(16);
        gpio_set_dir(16, GPIO_OUT);
        while (true) {
            gpio_put(16, 1); sleep_ms(100);
            gpio_put(16, 0); sleep_ms(100);
        }
    }
}

// wake chip, configure accel (2g) and gyro (2000 dps)
static void imu_init() {
    imu_write_reg(PWR_MGMT_1,   0x00);  // wake up (clear sleep bit)
    imu_write_reg(ACCEL_CONFIG, 0x00);  // +/-2g  -> bits [4:3] = 00
    imu_write_reg(GYRO_CONFIG,  0x18);  // +/-2000 dps -> bits [4:3] = 11
}

// Struct to hold all sensor readings (raw and converted)
typedef struct {
    int16_t ax_raw, ay_raw, az_raw;
    int16_t gx_raw, gy_raw, gz_raw;
    int16_t temp_raw;
    float ax_g, ay_g, az_g;         // acceleration in g
    float gx_dps, gy_dps, gz_dps;   // angular velocity in deg/s
    float temp_c;                   // temperature in °C
} IMUData;

// read all 14 bytes starting from ACCEL_XOUT_H
static void imu_read(IMUData *d) {
    uint8_t reg = ACCEL_XOUT_H;
    uint8_t buf[14];
    i2c_write_blocking(I2C_PORT, MPU6050_ADDR, &reg, 1, true);
    i2c_read_blocking (I2C_PORT, MPU6050_ADDR, buf, 14, false);

    // recombine high/low bytes into signed 16-bit integers
    d->ax_raw   = (int16_t)((buf[0]  << 8) | buf[1]);
    d->ay_raw   = (int16_t)((buf[2]  << 8) | buf[3]);
    d->az_raw   = (int16_t)((buf[4]  << 8) | buf[5]);
    d->temp_raw = (int16_t)((buf[6]  << 8) | buf[7]);
    d->gx_raw   = (int16_t)((buf[8]  << 8) | buf[9]);
    d->gy_raw   = (int16_t)((buf[10] << 8) | buf[11]);
    d->gz_raw   = (int16_t)((buf[12] << 8) | buf[13]);

    // convert to physical units
    d->ax_g    = d->ax_raw * 0.000061f;
    d->ay_g    = d->ay_raw * 0.000061f;
    d->az_g    = d->az_raw * 0.000061f;
    d->gx_dps  = d->gx_raw * 0.007630f;
    d->gy_dps  = d->gy_raw * 0.007630f;
    d->gz_dps  = d->gz_raw * 0.007630f;
    d->temp_c  = d->temp_raw / 340.00f + 36.53f;
}

// line drawing
static void draw_line(int x0, int y0, int x1, int y1, uint8_t color) {
    int dx =  abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        ssd1306_drawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// main
int main() {
    stdio_init_all();

    // I2C init — shared bus for OLED and IMU
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // // debug
    // sleep_ms(100);
    // ssd1306_setup();
    // ssd1306_clear();
    // drawMessage(0, 0, "OLED OK");
    // ssd1306_update();
    // sleep_ms(2000);

    // ssd1306_clear();
    // drawMessage(0, 0, "IMU init...");
    // ssd1306_update();

    // imu_check_whoami();
    // imu_init();

    // ssd1306_clear();
    // drawMessage(0, 0, "IMU OK");
    // ssd1306_update();
    // sleep_ms(2000);

    // OLED init
    sleep_ms(100);
    ssd1306_setup();
    ssd1306_clear();
    ssd1306_update();

    // IMU init
    sleep_ms(100);
    imu_check_whoami();
    imu_init();

    IMUData imu;
    unsigned int t_prev = to_us_since_boot(get_absolute_time());

    while (true) {
        // read IMU at ~100Hz
        imu_read(&imu);

        // OLED: draw gravity vector
        ssd1306_clear();

        // scale raw accel to pixel offset
        // ax: positive = tilt right  → line goes right
        // ay: positive = tilt forward → line goes down
        // Clamp to stay on screen (max 15px from center)
        int dx = (int)(-imu.ax_raw * SCALE);
        int dy = (int)(imu.ay_raw * SCALE);

        // clamp
        if (dx >  CX - 2) dx =  CX - 2;
        if (dx < -(CX-2)) dx = -(CX-2);
        if (dy >  CY - 2) dy =  CY - 2;
        if (dy < -(CY-2)) dy = -(CY-2);

        // draw line from center to tip
        draw_line(CX, CY, CX + dx, CY + dy, 1);

        // draw a small crosshair at center
        ssd1306_drawPixel(CX,   CY,   1);
        ssd1306_drawPixel(CX+1, CY,   1);
        ssd1306_drawPixel(CX-1, CY,   1);
        ssd1306_drawPixel(CX,   CY+1, 1);
        ssd1306_drawPixel(CX,   CY-1, 1);

        // // print numeric values
        // char buf[32];
        // sprintf(buf, "ax:%.2f ay:%.2f", imu.ax_g, imu.ay_g);
        // drawMessage(0, 24, buf);   // bottom row

        // ssd1306_update();

        // timing: target 100Hz = 10ms per loop
        unsigned int t_now = to_us_since_boot(get_absolute_time());
        unsigned int dt = t_now - t_prev;
        t_prev = t_now;

        // print to serial for debugging
        printf("ax=%.3f ay=%.3f az=%.3f | gx=%.1f gy=%.1f gz=%.1f | T=%.1fC | dt=%uus\n",
               imu.ax_g, imu.ay_g, imu.az_g,
               imu.gx_dps, imu.gy_dps, imu.gz_dps,
               imu.temp_c, dt);

        // sleep remainder of 10ms if we have time left
        if (dt < 10000) sleep_us(10000 - dt);
    }
}