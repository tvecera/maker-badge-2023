/*
 * Maker Badge Firmware - 2023 ESP32-S2
 *
 * This software has been tested and designed specifically for revision D of the Maker Badge.
 * Compatibility with other versions or revisions of the Maker Badge is not guaranteed, and the software
 * may not function as expected when used with other revisions.
 *
 * Particular attention should be given when working with the power management of the e-ink display.
 * E-ink displays are sensitive to power conditions, and improper management could potentially damage the display.
 * Please take special care to avoid powering the e-ink display for extended periods, as this can lead to irreparable
 * damage.
 *
 * his software is distributed under the MIT license, and the users are responsible for ensuring appropriate use.
 * No warranties or guarantees of any kind are provided. :)
 *
 * This program is inspired and based on a project by Yourigh (Juraj Repcik):
 * https://github.com/Yourigh/maker_badge_fw/blob/main/LICENSE
 *
 * To convert images to a format usable in C++, you can use online tools such as the one found at
 * https://javl.github.io/image2cpp/. This tool allows you to convert bitmap images into byte arrays that can be
 * directly used in your C++ projects.
 *
 * PlatformIO Project + CLion
 */
#include "main.h"
#include "bitmaps.h"

#define BAUD_RATE 115200
#define LOW_BATTERY_VOLTAGE 3.45
#define WAKE_UP_TOUCH_THRESHOLD 1000
#define BADGE_PAGE_WAKE_UP 360

CRGB leds[4];
GxEPD2_BW<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT> display(
        GxEPD2_213_B74(IO_DISP_CS, IO_DISP_DC, IO_DISP_RST, IO_DISP_BUSY));

// Current mode and wakes (number of wake-ups from sleep)
RTC_DATA_ATTR Mode currentMode = Badge;
RTC_DATA_ATTR int wakeCount = 0;

// Initialize Serial communication with specified BAUD_RATE
void initializeSerial() {
    Serial.begin(BAUD_RATE);
}

// Common function to set up pin mode and state
void setup_pin(int pin, int mode, int state) {
    pinMode(pin, mode);
    digitalWrite(pin, state);
}

// Setup LEDs, pins and add LEDs
void setup_leds() {
    setup_pin(IO_LED_DISABLE, OUTPUT, HIGH); // disable LEDs initially
    CFastLED::addLeds<WS2812B, IO_LED, GRB>(leds, NUM_LEDS); // add LED control
}

// Setup display, pins and initial state
void setup_display() {
    setup_pin(IO_EPD_POWER_DISABLE, OUTPUT, LOW); // enable power to EPD
    display.init(0); // initialize display
    display.setRotation(3); // set display rotation
    display.setTextColor(GxEPD_BLACK); // set text color to black
}

// Setup battery voltage reading
void setup_battery() {
    setup_pin(IO_BAT_MEAS_DISABLE, OUTPUT, HIGH); // disable battery measurement initially
}

// Handle the different wakeup causes
void handle_wake_up() {
    switch (esp_sleep_get_wakeup_cause()) {
        case ESP_SLEEP_WAKEUP_TIMER: // on timer wakeup
            wakeCount++; // increase wakeup counter
            handle_mode_change(false); // handle mode change without touchpad
            break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD: // on touchpad wakeup
            wakeCount++; // increase wakeup counter
            handle_mode_change(true); // handle mode change with touchpad
            break;
        default: // normal power-up after reset
            show_menu_page();  // display menu to select a mode
            enter_sleep(1); // sleep for 1s and get back to switch timer wakeup cause
    }
}

// Handle the mode change based on the current mode and whether the touchpad is used
void handle_mode_change(bool touchpad) {
    switch (currentMode) {
        case Badge:
            currentMode = touchpad ? QrCode : Badge; // toggle mode if touchpad was used
            show_badge_page(); // display badge
            break;
        case QrCode:
            currentMode = touchpad ? Badge : QrCode; // toggle mode if touchpad was used
            show_qr_code_page(); // display QR Code
            break;
        case WifiScan:
            currentMode = Menu; // switch to menu mode
            display_wifi_channels_page(); // display Wi-Fi scan
            break;
        default:
            show_menu_page(); // display menu for any other mode
            break;
    }
}

// Main setup function
void setup() {
    initializeSerial(); // setup serial communication
    analogReadResolution(RESOLUTION); // set analog read resolution
    analogSetAttenuation(ATTENUATION); // set analog attenuation
    WiFi.mode(WIFI_OFF); // switch off Wi-Fi mode

    setup_leds(); // setup LEDs
    setup_display(); // setup display
    setup_battery(); // setup battery measurement

    touch_pad_init(); // initialize touchpad
    touchAttachInterrupt(IO_TOUCH5, touch_interrupt_callback, WAKE_UP_TOUCH_THRESHOLD); // attach touch interrupt
    esp_sleep_enable_touchpad_wakeup(); // enable touchpad wakeup

    handle_wake_up(); // handle wakeup causes
}

void init_page(uint16_t textColor = GxEPD_BLACK) {
    // Enable LEDs
    digitalWrite(IO_LED_DISABLE, LOW);
    FastLED.show();

    // Display setup
    display.setFullWindow();
    display.firstPage();
    display.setTextWrap(true);
    display.setTextColor(textColor);
}

// Function to display text
void print_text(short x, short y, const GFXfont *f, const char *text) {
    display.setCursor(x, y);
    display.setFont(f);
    display.print(text);
}

// Function to display the menu
void show_menu_page() {
    leds[0] = leds[1] = CRGB(5, 10, 0);
    init_page();

    do {
        display.fillScreen(GxEPD_WHITE);
        display.drawBitmap(0, 0, bmp_menu, 250, 122, GxEPD_BLACK);

        // Menu items
        print_text(110, 42, &FreeSansBold9pt7b, "Prague 2023");
        print_text(110, 67, &FreeSans9pt7b, "1. Badge");
        print_text(110, 83, &FreeSans9pt7b, "2. QR code");
        print_text(110, 99, &FreeSans9pt7b, "3. WiFi CH rating");
        print_text(110, 115, &FreeSans9pt7b, "5. Wake up");
    } while (display.nextPage());

    // LED flip and timeout variables
    uint8_t flipLED = 1;
    uint32_t lastMillis = 0;
    uint16_t timeout = 100; //*0.6s = 600s = 10min

    // Main loop
    while (true) {
        delay(150);

        // Touch pin reading and mode selection
        switch (read_touch_pins()) {
            case 0b00001: //1
                currentMode = Badge;
                return;
            case 0b00010: //2
                currentMode = QrCode;
                return;
            case 0b00100: //3
                currentMode = WifiScan;
                return;
            default:
                currentMode = Menu;
                break;
        }

        // LED flipping and timeout condition
        if ((lastMillis + 600) < millis()) {
            leds[flipLED & 0x01] = CRGB(5, 10, 0);
            leds[!(flipLED++ & 0x01)] = CRGB(0, 0, 0);
            FastLED.show();
            lastMillis = millis();
            if (0 == timeout--)
                // Sleep forever :)
                enter_sleep(0);
        }
    }
}

// Function to display the badge
void show_badge_page() {
    leds[0] = CRGB(0, 10, 0);
    init_page(GxEPD_WHITE);

    char batteryVoltageText[32];
    char wakeUpCountText[32];
    snprintf(batteryVoltageText, sizeof(batteryVoltageText), "B: %.2f V", read_battery_voltage());
    snprintf(wakeUpCountText, sizeof(wakeUpCountText), "W: x %d", wakeCount);

    do {
        display.fillScreen(GxEPD_WHITE);
        display.drawBitmap(0, 0, bmp_badge, 250, 122, GxEPD_BLACK);
        // Display battery voltage
        print_text(165, 17, &FreeSans9pt7b, batteryVoltageText);
        // Display wake count
        print_text(160, 40, &FreeSans9pt7b, wakeUpCountText);
    } while (display.nextPage());

    // Power off the LED and display, then sleep to 6 minutes
    enter_sleep(BADGE_PAGE_WAKE_UP);
}

// Function to display the QR code
void show_qr_code_page() {
    leds[0] = CRGB(0, 10, 0);
    init_page();

    do {
        display.fillScreen(GxEPD_WHITE);
        display.drawBitmap(0, 0, bmp_qrcode, 250, 122, GxEPD_BLACK);
    } while (display.nextPage());

    // Power off the LED and display, then sleep indefinitely
    enter_sleep(0);
}

// Function to display Wi-Fi Scan
void display_wifi_channels_page() {
    digitalWrite(IO_LED_DISABLE, LOW);
    leds[1] = CRGB(50, 0, 0);
    FastLED.show();

    // Wi-Fi setup and scanning
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    int n = WiFi.scanNetworks();

    // Channel setup
    int ch[14] = {0};  // Initialise all elements to 0
    if (n > 0) {
        for (int i = 0; i < n; ++i) {
            int channel = WiFi.channel(i);
            ch[channel] += 1;
        }
    }
    char apCountText[32];
    snprintf(apCountText, sizeof(apCountText), "%*d", 3, n);

    leds[0] = CRGB(0, 10, 0);
    init_page();

    do {
        display.fillScreen(GxEPD_WHITE);
        display.drawBitmap(210, 5, bmp_wifi, 38, 64, GxEPD_BLACK);
        print_text(215, 88, &FreeSansBold9pt7b, "All");
        print_text(210, 112, &FreeMono9pt7b, apCountText);

        short y = 17;
        short x = 0;
        for (int i = 0; i < 14; i++) {
            if (i == 7) {
                x = 95;
                y = 17;
            }
            display.setCursor(x, y);
            if (i < 7 || i >= 9)
                display.printf("CH %d: %d", i + 1, ch[i]);
            else
                display.printf("CH  %d: %d", i + 1, ch[i]);
            y = y + 17;
        }
    } while (display.nextPage());

    // Power off the LED and display, then sleep
    enter_sleep(30);
}


void enter_sleep(uint16_t TimedWakeUpSec) {
    display.powerOff();
    digitalWrite(IO_LED_DISABLE, HIGH);
    digitalWrite(IO_BAT_MEAS_DISABLE, HIGH);
    digitalWrite(IO_EPD_POWER_DISABLE, HIGH);
    WiFi.mode(WIFI_OFF);
    WiFi.disconnect(true, false);
    if (TimedWakeUpSec != 0) {
        esp_sleep_enable_timer_wakeup(TimedWakeUpSec * 1000000);
    }
    esp_deep_sleep_start();
}

// Function to read touch pins and return result as mask
int read_touch_pins() {
    int touch_result_mask = 0x00;  // Initialize the result mask

    // Iterate over the touch pins
    for (int i = 0; i < 5; i++) {
        // (5 - i) because pins order is reversed
        // If the touchRead value for a pin is more than the threshold, set that bit in the mask
        if (touchRead(5 - i) > TOUCH_THRESHOLD) {
            touch_result_mask |= 1 << i;
        }
    }

    // Return the result mask
    return touch_result_mask;
}

// Function to read and return battery voltage
float read_battery_voltage() {
    // Enable battery measurement
    digitalWrite(IO_BAT_MEAS_DISABLE, LOW);
    // Delay to let the measurement stabilize
    delayMicroseconds(150);

    // Read battery voltage through ADC
    auto batt_adc = static_cast<float>(analogRead(AIN_BATT));
    // Disable battery measurement
    digitalWrite(IO_BAT_MEAS_DISABLE, HIGH);

    // Calculate the battery voltage
    float battv = (BATT_V_CAL_SCALE * 2.0f * (2.50f * batt_adc / 4096));

    // Print the battery voltage, calibrated voltage and raw ADC value for debugging
    Serial.printf("Battv: %fV, Bat w/ calibration %fV, raw ADC %f\n", battv / BATT_V_CAL_SCALE, battv, batt_adc);

    // If the battery voltage is less than 3.45V, put the device into sleep mode
    if (battv < LOW_BATTERY_VOLTAGE)
        enter_sleep(0);

    // Return the battery voltage
    return battv;
}

void loop() {
    // intentionally empty
}
