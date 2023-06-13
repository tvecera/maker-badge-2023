#ifndef MAIN_H
#define MAIN_H

#include "FastLED.h"
#include "GxEPD2_BW.h"
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include "driver/touch_pad.h"
#include <WiFi.h>

#define NUM_LEDS 4

// Pins
#define IO_LED_DISABLE 21
#define IO_LED 18 //4x WS2812B, GRB order
#define IO_DISP_CS 41
#define IO_DISP_DC 40
#define IO_DISP_RST 39
#define IO_DISP_BUSY 42
#define AIN_BATT 6
#define IO_TOUCH5 1
#define IO_EPD_POWER_DISABLE 16 //put to low to power E-paper display
#define IO_BAT_MEAS_DISABLE 14 //put to low to measure battery voltage. Battery has to be measured right after pulling low.

#define TOUCH_THRESHOLD 20000 //ca. 15000 when empty, 30k for touch
#define RESOLUTION 12
#define ATTENUATION ADC_11db
#define BATT_V_CAL_SCALE 1.05f

void show_menu_page();

void show_badge_page();

void show_qr_code_page();

float read_battery_voltage();

void display_wifi_channels_page();

void enter_sleep(uint16_t TimedWakeUpSec);

int read_touch_pins();

void touch_interrupt_callback() {}

void handle_mode_change(bool touchpad);

void setup_pin(int pin, int mode, int state);

void print_text(short x, short y, const GFXfont *f, const char *text);

void init_page(uint16_t textColor);

// Enumeration for modes
enum Mode {
    Badge,
    QrCode,
    WifiScan,
    Menu
};

#endif // MAIN_H


