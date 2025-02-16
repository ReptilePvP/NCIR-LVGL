#include "lv_conf.h"
#include <Wire.h>
#include <FastLED.h>
#include <M5Unified.h>
#include <M5GFX.h>
#include <lvgl.h>

// Pin Definitions
#define KEY_PIN 8
#define LED_PIN 9
#define BUTTON1_PIN 17
#define BUTTON2_PIN 18
#define NUM_LEDS 1

// Screen dimensions
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// Temperature ranges
#define TEMP_MIN_C 149
#define TEMP_MAX_C 396
#define TEMP_MIN_F 300
#define TEMP_MAX_F 745

// Temperature ranges for dabs (in Fahrenheit)
#define TEMP_TOO_COLD_F 400    // Below this is too cold
#define TEMP_LOW_F      450    // Low end of good range
#define TEMP_HIGH_F     600    // High end of good range
#define TEMP_TOO_HOT_F  650    // Above this is too hot

// Temperature ranges for dabs (in Celsius)
#define TEMP_TOO_COLD_C 204    // Below this is too cold
#define TEMP_LOW_C      232    // Low end of good range
#define TEMP_HIGH_C     316    // High end of good range
#define TEMP_TOO_HOT_C  343    // Above this is too hot

// LVGL Refresh time
static const uint32_t screenTickPeriod = 10;  // Increased to 10ms for better stability
static uint32_t lastLvglTick = 0;

// Update intervals
#define TEMP_UPDATE_INTERVAL 100  // Update temperature every 100ms for more responsive readings

// LED Array
CRGB leds[NUM_LEDS];

// Display and LVGL objects
static lv_obj_t *temp_label;
static lv_obj_t *temp_value_label;
static lv_obj_t *status_label;
static lv_obj_t *meter;
static lv_meter_indicator_t *temp_indic;
static lv_meter_scale_t *temp_scale;  // Scale needs to be stored
static lv_style_t style_text;
static lv_style_t style_background;

// State variables
static bool showGauge = true;      // Track gauge visibility
static bool lastKeyState = HIGH;    // Track key state for toggle

// Display buffer
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[SCREEN_WIDTH * 10];

// Display driver
static lv_disp_drv_t disp_drv;

// Temperature unit
bool isCelsius = true;

float readObjectTemperature() {
    Wire.beginTransmission(0x5A);
    Wire.write(0x07);
    if(Wire.endTransmission(false) != 0) {
        return -999.0;
    }
    
    if(Wire.requestFrom(0x5A, 2) != 2) {
        return -999.0;
    }
    
    if (Wire.available() == 2) {
        uint16_t object_raw = (Wire.read() | (Wire.read() << 8));
        return object_raw * 0.02 - 273.15;
    }
    return -999.0;
}

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    M5.Lcd.startWrite();
    M5.Lcd.setAddrWindow(area->x1, area->y1, w, h);
    M5.Lcd.pushColors((uint16_t *)&color_p->full, w * h, true);
    M5.Lcd.endWrite();

    lv_disp_flush_ready(disp);
}

void setup() {
    Serial.begin(115200);
    delay(100); // Short delay for serial stability
    Serial.println("Starting setup...");

    // Initialize M5Stack with specific config
    auto cfg = M5.config();
    cfg.clear_display = true;
    cfg.output_power = true;
    cfg.internal_imu = false;
    cfg.internal_rtc = false;
    M5.begin(cfg);
    
    // Initialize I2C
    Wire.begin(2, 1);
    Wire.setClock(100000);
    
    // Initialize display
    M5.Lcd.begin();
    M5.Lcd.setRotation(1);
    M5.Lcd.setBrightness(255);
    M5.Lcd.fillScreen(TFT_BLACK);
    
    Serial.println("Display initialized");
    
    // Initialize LVGL
    lv_init();
    
    // Initialize display buffer (single buffer for 8.4.0)
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, SCREEN_WIDTH * 10);
    
    // Initialize display driver
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    
    // Create styles
    lv_style_init(&style_text);
    lv_style_set_text_font(&style_text, &lv_font_montserrat_24);
    lv_style_set_text_color(&style_text, lv_palette_main(LV_PALETTE_LIGHT_BLUE));
    
    lv_style_init(&style_background);
    lv_style_set_bg_color(&style_background, lv_color_black());
    
    // Create main container
    lv_obj_t *main_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(main_container, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_add_style(main_container, &style_background, 0);
    
    // Create meter
    meter = lv_meter_create(main_container);
    lv_obj_center(meter);
    lv_obj_set_size(meter, 200, 200);
    
    // Create and configure scale
    temp_scale = lv_meter_add_scale(meter);
    lv_meter_set_scale_ticks(meter, temp_scale, 41, 2, 10, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_major_ticks(meter, temp_scale, 8, 4, 15, lv_color_white(), 10);
    
    // Set initial scale range
    if (isCelsius) {
        lv_meter_set_scale_range(meter, temp_scale, TEMP_MIN_C, TEMP_MAX_C, 270, 135);
    } else {
        lv_meter_set_scale_range(meter, temp_scale, TEMP_MIN_F, TEMP_MAX_F, 270, 135);
    }
    
    // Add the temperature indicator (needle)
    temp_indic = lv_meter_add_needle_line(meter, temp_scale, 5, lv_palette_main(LV_PALETTE_RED), -10);
    
    // Create temperature label
    temp_label = lv_label_create(main_container);
    lv_label_set_text(temp_label, "Temperature:");
    lv_obj_add_style(temp_label, &style_text, 0);
    lv_obj_align(temp_label, LV_ALIGN_TOP_MID, 0, 10);
    
    // Create temperature value label with larger font
    temp_value_label = lv_label_create(main_container);
    lv_label_set_text(temp_value_label, "Reading...");
    lv_obj_add_style(temp_value_label, &style_text, 0);
    lv_obj_align(temp_value_label, LV_ALIGN_TOP_MID, 0, 40);
    
    // Create status label
    status_label = lv_label_create(main_container);
    lv_label_set_text(status_label, "");
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_MID, 0, -20);
    
    // Initialize LED
    FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(50);
    
    // Initialize buttons
    pinMode(BUTTON1_PIN, INPUT_PULLUP);
    pinMode(BUTTON2_PIN, INPUT_PULLUP);
    pinMode(KEY_PIN, INPUT_PULLUP);
    
    Serial.println("Setup completed!");
    lastLvglTick = millis();
    
    // Initial LED color
    leds[0] = CRGB::Green;
    FastLED.show();
}

void loop() {
    M5.update();
    
    // Handle LVGL timing
    uint32_t currentMillis = millis();
    if (currentMillis - lastLvglTick > screenTickPeriod) {
        lv_timer_handler();
        lastLvglTick = currentMillis;
    }
    
    // Check key press to toggle gauge visibility
    bool keyState = digitalRead(KEY_PIN);
    if (keyState == LOW && lastKeyState == HIGH) {
        showGauge = !showGauge;
        if (showGauge) {
            lv_obj_clear_flag(meter, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(meter, LV_OBJ_FLAG_HIDDEN);
        }
    }
    lastKeyState = keyState;
    
    // Check button press to toggle temperature unit
    static bool lastButtonState = HIGH;
    bool buttonState = digitalRead(BUTTON1_PIN);
    if (buttonState == LOW && lastButtonState == HIGH) {
        isCelsius = !isCelsius;
    }
    lastButtonState = buttonState;
    
    // Update temperature more frequently
    static uint32_t lastTempUpdate = 0;
    if (currentMillis - lastTempUpdate > TEMP_UPDATE_INTERVAL) {
        float objectTemp = readObjectTemperature();
        
        if (objectTemp > -273.15) {
            if (!isCelsius) {
                objectTemp = objectTemp * 9.0 / 5.0 + 32.0; // Convert to Fahrenheit
            }
            
            char tempStr[32];
            snprintf(tempStr, sizeof(tempStr), "%d°%c", (int)round(objectTemp), isCelsius ? 'C' : 'F');
            lv_label_set_text(temp_value_label, tempStr);
            
            // Update meter value and scale based on temperature unit
            if (isCelsius) {
                lv_meter_set_scale_range(meter, temp_scale, TEMP_MIN_C, TEMP_MAX_C, 270, 135);
                lv_meter_set_indicator_value(meter, temp_indic, objectTemp);
                
                // Update status and LED color based on temperature ranges
                if (objectTemp < TEMP_TOO_COLD_C) {
                    lv_label_set_text(status_label, "TOO COLD");
                    leds[0] = CRGB::Blue;
                } else if (objectTemp < TEMP_LOW_C) {
                    lv_label_set_text(status_label, "HEATING UP");
                    leds[0] = CRGB::Yellow;
                } else if (objectTemp <= TEMP_HIGH_C) {
                    lv_label_set_text(status_label, "PERFECT!");
                    leds[0] = CRGB::Green;
                } else if (objectTemp <= TEMP_TOO_HOT_C) {
                    lv_label_set_text(status_label, "COOLING DOWN");
                    leds[0] = CRGB::Orange;
                } else {
                    lv_label_set_text(status_label, "TOO HOT!");
                    leds[0] = CRGB::Red;
                }
            } else {
                lv_meter_set_scale_range(meter, temp_scale, TEMP_MIN_F, TEMP_MAX_F, 270, 135);
                lv_meter_set_indicator_value(meter, temp_indic, objectTemp);
                
                // Update status and LED color based on temperature ranges
                if (objectTemp < TEMP_TOO_COLD_F) {
                    lv_label_set_text(status_label, "TOO COLD");
                    leds[0] = CRGB::Blue;
                } else if (objectTemp < TEMP_LOW_F) {
                    lv_label_set_text(status_label, "HEATING UP");
                    leds[0] = CRGB::Yellow;
                } else if (objectTemp <= TEMP_HIGH_F) {
                    lv_label_set_text(status_label, "PERFECT!");
                    leds[0] = CRGB::Green;
                } else if (objectTemp <= TEMP_TOO_HOT_F) {
                    lv_label_set_text(status_label, "COOLING DOWN");
                    leds[0] = CRGB::Orange;
                } else {
                    lv_label_set_text(status_label, "TOO HOT!");
                    leds[0] = CRGB::Red;
                }
            }
            FastLED.show();
        }
        lastTempUpdate = currentMillis;
    }
    
    delay(5);
}