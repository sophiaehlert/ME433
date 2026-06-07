/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "bsp/board_api.h"
#include "tusb.h"
#include "usb_descriptors.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED     = 1000,
  BLINK_SUSPENDED   = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void);
void hid_task(void);

//--------------------------------------------------------------------+
// IMU
//--------------------------------------------------------------------+

#define I2C_PORT     i2c1
#define I2C_SDA      14
#define I2C_SCL      15
#define MPU6050_ADDR 0x68

#define ACCEL_CONFIG 0x1C
#define PWR_MGMT_1   0x6B
#define GYRO_CONFIG  0x1B
#define ACCEL_XOUT_H 0x3B

static void imu_write_reg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    i2c_write_blocking(I2C_PORT, MPU6050_ADDR, buf, 2, false);
}

static void imu_init() {
    imu_write_reg(PWR_MGMT_1,   0x00);  // wake up
    imu_write_reg(ACCEL_CONFIG, 0x00);  // +/-2g
    imu_write_reg(GYRO_CONFIG,  0x18);  // +/-2000 dps
}

static void imu_read_accel(int16_t *ax, int16_t *ay) {
    uint8_t reg = ACCEL_XOUT_H;
    uint8_t buf[4];
    i2c_write_blocking(I2C_PORT, MPU6050_ADDR, &reg, 1, true);
    i2c_read_blocking(I2C_PORT, MPU6050_ADDR, buf, 4, false);
    *ax = (int16_t)((buf[0] << 8) | buf[1]);
    *ay = (int16_t)((buf[2] << 8) | buf[3]);
}

static int8_t accel_to_delta(int16_t raw) {
    int16_t a = abs(raw);
    int8_t sign = (raw < 0) ? -1 : 1;
    if      (a > 12000) return sign * 5;
    else if (a >  8000) return sign * 3;
    else if (a >  4000) return sign * 1;
    else                return 0;
}

//--------------------------------------------------------------------+
// Mode + Button
//--------------------------------------------------------------------+

#define LED_PIN    16
#define BUTTON_PIN 17

typedef enum { MODE_IMU = 0, MODE_CIRCLE } Mode;
static volatile Mode current_mode = MODE_IMU;
static uint32_t last_press_ms = 0;

static void button_callback(uint gpio, uint32_t events) {
    uint32_t now = board_millis();
    if (now - last_press_ms < 200) return;  // debounce
    last_press_ms = now;
    if (current_mode == MODE_IMU) {
        current_mode = MODE_CIRCLE;
        gpio_put(LED_PIN, 1);   // LED on = circle mode
    } else {
        current_mode = MODE_IMU;
        gpio_put(LED_PIN, 0);   // LED off = IMU mode
    }
}

//--------------------------------------------------------------------+
// Circle state
//--------------------------------------------------------------------+

static float circle_angle = 0.0f;
#define CIRCLE_RADIUS 10.0f
#define CIRCLE_SPEED  0.05f

/*------------- MAIN -------------*/
int main(void)
{
  board_init();

  // init device stack on configured roothub port
  tud_init(BOARD_TUD_RHPORT);

  if (board_init_after_tusb) {
    board_init_after_tusb();
  }

  // I2C + IMU
  i2c_init(I2C_PORT, 400 * 1000);
  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA);
  gpio_pull_up(I2C_SCL);
  sleep_ms(100);
  imu_init();

  // Mode indicator LED
  gpio_init(LED_PIN);
  gpio_set_dir(LED_PIN, GPIO_OUT);
  gpio_put(LED_PIN, 0);  // start in IMU mode, LED off

  // Button with internal pull-up (wire button between GP17 and GND)
  gpio_init(BUTTON_PIN);
  gpio_set_dir(BUTTON_PIN, GPIO_IN);
  gpio_pull_up(BUTTON_PIN);
  gpio_set_irq_enabled_with_callback(BUTTON_PIN,
      GPIO_IRQ_EDGE_FALL, true, &button_callback);

  while (1)
  {
    tud_task();         // tinyusb device task
    led_blinking_task();
    hid_task();
  }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

static void send_hid_report(uint8_t report_id, uint32_t btn)
{
  // skip if hid is not ready yet
  if ( !tud_hid_ready() ) return;

  if (report_id == REPORT_ID_MOUSE)
  {
    int8_t dx = 0, dy = 0;

    if (current_mode == MODE_IMU) {
      int16_t ax, ay;
      imu_read_accel(&ax, &ay);
      dx = -accel_to_delta(ax);
      dy =  accel_to_delta(ay);
    } else {
      // circle mode
      dx = (int8_t)(CIRCLE_RADIUS * cosf(circle_angle));
      dy = (int8_t)(CIRCLE_RADIUS * sinf(circle_angle));
      circle_angle += CIRCLE_SPEED;
      if (circle_angle > 2.0f * (float)M_PI) circle_angle -= 2.0f * (float)M_PI;
    }

    tud_hid_mouse_report(REPORT_ID_MOUSE, 0x00, dx, dy, 0, 0);
  }
}

// Every 10ms, we will send 1 report
void hid_task(void)
{
  const uint32_t interval_ms = 10;
  static uint32_t start_ms = 0;

  if ( board_millis() - start_ms < interval_ms) return;  // not enough time
  start_ms += interval_ms;

  uint32_t const btn = board_button_read();

  // Remote wakeup
  if ( tud_suspended() && btn )
  {
    // Wake up host if we are in suspend mode
    // and REMOTE_WAKEUP feature is enabled by host
    tud_remote_wakeup();
  } else {
    send_hid_report(REPORT_ID_MOUSE, btn);
  }
}

// Invoked when sent REPORT successfully to host
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len)
{
  (void) instance;
  (void) len;
  (void) report;
  // only one report type (mouse), nothing to chain
}

// Invoked when received GET_REPORT control request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;
  return 0;
}

// Invoked when received SET_REPORT control request
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) bufsize;
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // blink is disabled
  if (!blink_interval_ms) return;

  // Blink every interval ms
  if ( board_millis() - start_ms < blink_interval_ms) return;  // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state;  // toggle
}