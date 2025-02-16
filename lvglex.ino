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

// LVGL Refresh time
static const uint32_t screenTickPeriod = 10;  // Increased to 10ms for better stability
static uint32_t lastLvglTick = 0;

// LED Array
CRGB leds[NUM_LEDS];

// Display and LVGL objects
static lv_obj_t *temp_label;
static lv_obj_t *temp_value_label;
static lv_style_t style_text;
static lv_style_t style_background;

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
    
    // Create temperature label
    temp_label = lv_label_create(main_container);
    lv_label_set_text(temp_label, "Temperature:");
    lv_obj_add_style(temp_label, &style_text, 0);
    lv_obj_align(temp_label, LV_ALIGN_TOP_MID, 0, 40);
    
    // Create temperature value label
    temp_value_label = lv_label_create(main_container);
    lv_label_set_text(temp_value_label, "Reading...");
    lv_obj_add_style(temp_value_label, &style_text, 0);
    lv_obj_align(temp_value_label, LV_ALIGN_CENTER, 0, 20);
    
    // Initialize FastLED
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
    
    // Check button press to toggle temperature unit
    static bool lastButtonState = HIGH;
    bool buttonState = digitalRead(BUTTON1_PIN);
    if (buttonState == LOW && lastButtonState == HIGH) {
        isCelsius = !isCelsius;
    }
    lastButtonState = buttonState;
    
    // Update temperature every second
    static uint32_t lastTempUpdate = 0;
    if (currentMillis - lastTempUpdate > 1000) {
        float objectTemp = readObjectTemperature();
        
        if (objectTemp > -273.15) {
            if (!isCelsius) {
                objectTemp = objectTemp * 9.0 / 5.0 + 32.0; // Convert to Fahrenheit
            }
            
            char tempStr[32];
            snprintf(tempStr, sizeof(tempStr), "%.1f°%c", objectTemp, isCelsius ? 'C' : 'F');
            lv_label_set_text(temp_value_label, tempStr);
            
            // Update LED color based on temperature
            if (objectTemp > (isCelsius ? 37.5 : 99.5)) {
                leds[0] = CRGB::Red;
            } else if (objectTemp > (isCelsius ? 35.0 : 95.0)) {
                leds[0] = CRGB::Green;
            } else {
                leds[0] = CRGB::Blue;
            }
            FastLED.show();
        }
        lastTempUpdate = currentMillis;
    }
    
    delay(5);
}