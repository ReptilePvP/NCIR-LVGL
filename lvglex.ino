#include "lv_conf.h"
#include <Wire.h>
#include <FastLED.h>
#include <M5Unified.h>
#include <M5GFX.h>
#include <lvgl.h>
#include <EEPROM.h>

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

// EEPROM addresses for settings
#define EEPROM_SIZE 16
#define SETTINGS_VALID_ADDR 0     // Address to check if settings are valid
#define TEMP_UNIT_ADDR 1         // Address for temperature unit setting
#define GAUGE_VISIBLE_ADDR 2     // Address for gauge visibility setting
#define BRIGHTNESS_ADDR 3        // Address for brightness setting
#define EMISSIVITY_ADDR 4        // Address for emissivity setting (uses 2 bytes)
#define SOUND_SETTINGS_ADDR 6    // Address for sound settings
#define SETTINGS_VALID_VALUE 0x55 // Magic number to validate settings

// Add brightness levels
#define BRIGHTNESS_LEVELS 5
static uint8_t current_brightness = 4; // 0-4 (20%, 40%, 60%, 80%, 100%)
static const uint8_t brightness_values[] = {51, 102, 153, 204, 255}; // 20%, 40%, 60%, 80%, 100%

// LED Array
CRGB leds[NUM_LEDS];

// Display and LVGL objects
static lv_obj_t *temp_label;
static lv_obj_t *temp_value_label;
static lv_obj_t *status_label;
static lv_obj_t *meter;
static lv_obj_t *settings_panel;    // Settings panel
static lv_obj_t *settings_items[6]; // Array for settings buttons
static lv_meter_indicator_t *temp_indic;
static lv_meter_indicator_t *temp_arc_low;
static lv_meter_indicator_t *temp_arc_normal;
static lv_meter_indicator_t *temp_arc_high;
static lv_meter_scale_t *temp_scale;
static lv_style_t style_text;
static lv_style_t style_background;
static lv_style_t style_settings_panel;
static lv_style_t style_settings_btn;
static lv_style_t style_battery;
static lv_obj_t *settings_container = NULL;

// State variables
static bool showGauge = true;      // Track gauge visibility
static bool lastKeyState = HIGH;    // Track key state for toggle
static bool lastButton1State = HIGH;
static bool lastButton2State = HIGH;
static bool showSettings = false;
static int selected_menu_item = 0;
static bool menu_active = false;
static bool temp_in_celsius = true;  // Moved here to global scope
static float current_emissivity = 0.95f;  // Default emissivity
static lv_obj_t *emissivity_bar = NULL;
static lv_obj_t *emissivity_label = NULL;
static bool bar_active = false;

// Sound settings
static bool sound_enabled = true;
static uint8_t volume_level = 100;  // 25, 50, 75, or 100
static const char* volume_options[] = {"25%", "50%", "75%", "100%", ""};
static uint8_t selected_volume = 3;  // Default to 100%

// Update global variables
static lv_obj_t *restart_msgbox = NULL;
static lv_obj_t *restart_label = NULL;
static bool emissivity_changed = false;
static float pending_emissivity = 0.0f;
static uint32_t restart_countdown = 0;
static lv_timer_t *restart_timer = NULL;

// Battery status
static lv_obj_t *battery_label;
static lv_obj_t *battery_icon;
static bool last_charging_state = false;
static uint8_t last_battery_level = 0;

// Settings menu items
const char* settings_labels[] = {
    "Temperature Unit",
    "Brightness",
    "Emissivity",
    "Sound Settings",
    "Gauge Visibility",
    "Exit"
};

// Function prototypes
static void setEmissivity(float value);
static void saveSettings();
static void loadSettings();
static void handle_menu_selection(int item);
static void handle_button_press(int pin);
static void restart_msgbox_event_cb(lv_event_t *e);
static void restart_timer_cb(lv_timer_t *timer);
static void emissivity_slider_event_cb(lv_event_t *e);
static void playBeep(int frequency, int duration);
static void update_battery_status();

// Function to save settings to EEPROM
void saveSettings() {
    EEPROM.write(SETTINGS_VALID_ADDR, SETTINGS_VALID_VALUE);
    EEPROM.write(TEMP_UNIT_ADDR, temp_in_celsius ? 1 : 0);
    EEPROM.write(BRIGHTNESS_ADDR, current_brightness);
    EEPROM.write(GAUGE_VISIBLE_ADDR, showGauge ? 1 : 0);
    EEPROM.write(SOUND_SETTINGS_ADDR, (sound_enabled ? 0x80 : 0) | (volume_level / 25));
    
    // Save emissivity (as integer, multiply by 100 to preserve 2 decimal places)
    uint16_t emissivity_int = current_emissivity * 100;
    EEPROM.write(EMISSIVITY_ADDR, emissivity_int & 0xFF);
    EEPROM.write(EMISSIVITY_ADDR + 1, emissivity_int >> 8);
    
    EEPROM.commit();
}

// Function to load settings from EEPROM
void loadSettings() {
    if (EEPROM.read(SETTINGS_VALID_ADDR) == SETTINGS_VALID_VALUE) {
        // Load existing settings
        temp_in_celsius = EEPROM.read(TEMP_UNIT_ADDR) == 1;
        current_brightness = EEPROM.read(BRIGHTNESS_ADDR);
        showGauge = EEPROM.read(GAUGE_VISIBLE_ADDR) == 1;
        
        // Load sound settings
        uint8_t sound_byte = EEPROM.read(SOUND_SETTINGS_ADDR);
        sound_enabled = (sound_byte & 0x80) != 0;
        volume_level = (sound_byte & 0x03) * 25 + 25;
        selected_volume = (volume_level / 25) - 1;
        
        // Load emissivity
        uint16_t emissivity_int = EEPROM.read(EMISSIVITY_ADDR) | (EEPROM.read(EMISSIVITY_ADDR + 1) << 8);
        current_emissivity = emissivity_int / 100.0f;
        
        // Ensure emissivity is within valid range
        if (current_emissivity < 0.65f) current_emissivity = 0.65f;
        if (current_emissivity > 1.0f) current_emissivity = 1.0f;
        
        // Apply emissivity
        setEmissivity(current_emissivity);
        
        // Ensure brightness is within valid range
        if (current_brightness >= BRIGHTNESS_LEVELS) {
            current_brightness = BRIGHTNESS_LEVELS - 1;
        }
        
        // Apply loaded settings
        M5.Lcd.setBrightness(brightness_values[current_brightness]);
        
        // Update temperature display scale based on loaded unit
        if (temp_in_celsius) {
            lv_meter_set_scale_range(meter, temp_scale, TEMP_MIN_C, TEMP_MAX_C, 270, 135);
        } else {
            lv_meter_set_scale_range(meter, temp_scale, TEMP_MIN_F, TEMP_MAX_F, 270, 135);
        }
        
        // Update gauge visibility
        if (showGauge) {
            lv_obj_clear_flag(meter, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(meter, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void create_settings_panel() {
    // Initialize settings panel style
    lv_style_init(&style_settings_panel);
    lv_style_set_bg_color(&style_settings_panel, lv_color_black());
    lv_style_set_border_width(&style_settings_panel, 0);
    lv_style_set_pad_all(&style_settings_panel, 0);
    
    // Initialize settings button style
    lv_style_init(&style_settings_btn);
    lv_style_set_bg_color(&style_settings_btn, lv_palette_darken(LV_PALETTE_GREY, 3));
    lv_style_set_border_width(&style_settings_btn, 0);
    lv_style_set_radius(&style_settings_btn, 0);
    lv_style_set_pad_all(&style_settings_btn, 0);
    lv_style_set_text_color(&style_settings_btn, lv_color_white());
    
    // Create settings panel
    settings_panel = lv_obj_create(lv_scr_act());
    lv_obj_add_style(settings_panel, &style_settings_panel, 0);
    lv_obj_set_size(settings_panel, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_align(settings_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);
    
    // Create title label
    lv_obj_t *title = lv_label_create(settings_panel);
    lv_label_set_text(title, "SETTINGS");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_color(title, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    
    // Create scrollable container
    settings_container = lv_obj_create(settings_panel);
    lv_obj_set_size(settings_container, SCREEN_WIDTH, 160); // Fixed height for menu items
    lv_obj_align(settings_container, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_style_bg_color(settings_container, lv_color_black(), 0);
    lv_obj_set_style_border_width(settings_container, 0, 0);
    lv_obj_set_style_pad_all(settings_container, 0, 0);
    lv_obj_set_style_pad_top(settings_container, 0, 0);
    lv_obj_set_style_pad_bottom(settings_container, 0, 0);
    lv_obj_set_scrollbar_mode(settings_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(settings_container, LV_DIR_VER); // Enable vertical scrolling
    
    // Calculate button dimensions
    int btn_height = 45;
    int btn_spacing = 5;
    int total_content_height = 6 * (btn_height + btn_spacing); // Height for all buttons
    
    // Create a content container for the buttons
    lv_obj_t *content = lv_obj_create(settings_container);
    lv_obj_set_size(content, SCREEN_WIDTH, total_content_height);
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(content, lv_color_black(), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    
    // Create settings buttons inside content container
    for(int i = 0; i < 6; i++) {
        settings_items[i] = lv_btn_create(content);
        lv_obj_add_style(settings_items[i], &style_settings_btn, 0);
        lv_obj_set_size(settings_items[i], SCREEN_WIDTH, btn_height);
        lv_obj_align(settings_items[i], LV_ALIGN_TOP_LEFT, 0, i * (btn_height + btn_spacing));
        
        // Create button label
        lv_obj_t *label = lv_label_create(settings_items[i]);
        lv_label_set_text(label, settings_labels[i]);
        lv_obj_center(label);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    }
    
    // Add navigation instructions
    lv_obj_t *nav_label = lv_label_create(settings_panel);
    lv_label_set_text(nav_label, "BTN1: Up | BTN2: Down | KEY: Select");
    lv_obj_align(nav_label, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_text_color(nav_label, lv_color_white(), 0);
}

static void update_menu_selection() {
    for (int i = 0; i < 6; i++) {
        if (i == selected_menu_item) {
            lv_obj_set_style_bg_color(settings_items[i], lv_palette_main(LV_PALETTE_BLUE), 0);
            lv_obj_t* label = lv_obj_get_child(settings_items[i], 0);
            char buf[32];
            snprintf(buf, sizeof(buf), "> %s <", settings_labels[i]);
            lv_label_set_text(label, buf);
            
            // Calculate scroll position to make selected item visible
            lv_coord_t btn_y = i * 50; // button height + spacing
            lv_obj_scroll_to_y(settings_container, btn_y, LV_ANIM_ON);
        } else {
            lv_obj_set_style_bg_color(settings_items[i], lv_palette_darken(LV_PALETTE_GREY, 3), 0);
            lv_obj_t* label = lv_obj_get_child(settings_items[i], 0);
            lv_label_set_text(label, settings_labels[i]);
        }
    }
}

static void handle_menu_selection(int item) {
    switch (item) {
        case 0:  // Temperature Unit
            temp_in_celsius = !temp_in_celsius;
            if (temp_in_celsius) {
                lv_meter_set_scale_range(meter, temp_scale, TEMP_MIN_C, TEMP_MAX_C, 270, 135);
            } else {
                lv_meter_set_scale_range(meter, temp_scale, TEMP_MIN_F, TEMP_MAX_F, 270, 135);
            }
            saveSettings();  // Save the new temperature unit setting
            menu_active = false;
            lv_obj_add_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);
            break;
            
        case 1:  // Brightness
            current_brightness = (current_brightness + 1) % BRIGHTNESS_LEVELS;
            M5.Lcd.setBrightness(brightness_values[current_brightness]);
            saveSettings();  // Save the new brightness setting
            menu_active = false;
            lv_obj_add_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);
            break;
            
        case 2:  // Emissivity
            if (!bar_active) {
                // Create container for emissivity control
                lv_obj_t *emissivity_cont = lv_obj_create(lv_scr_act());
                lv_obj_set_size(emissivity_cont, 200, 100);
                lv_obj_align(emissivity_cont, LV_ALIGN_CENTER, 0, 0);
                
                // Create emissivity bar
                emissivity_bar = lv_bar_create(emissivity_cont);
                lv_obj_set_size(emissivity_bar, 180, 20);
                lv_obj_align(emissivity_bar, LV_ALIGN_TOP_MID, 0, 10);
                lv_bar_set_range(emissivity_bar, 65, 100);  // Range 0.65 to 1.00
                lv_bar_set_value(emissivity_bar, current_emissivity * 100, LV_ANIM_OFF);
                
                // Make the bar draggable
                lv_obj_add_flag(emissivity_bar, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_add_event_cb(emissivity_bar, emissivity_slider_event_cb, LV_EVENT_PRESSED, NULL);
                lv_obj_add_event_cb(emissivity_bar, emissivity_slider_event_cb, LV_EVENT_PRESSING, NULL);
                lv_obj_add_event_cb(emissivity_bar, emissivity_slider_event_cb, LV_EVENT_RELEASED, NULL);
                
                // Create label for current value
                emissivity_label = lv_label_create(emissivity_cont);
                char buf[32];
                snprintf(buf, sizeof(buf), "Emissivity: %.2f", current_emissivity);
                lv_label_set_text(emissivity_label, buf);
                lv_obj_align(emissivity_label, LV_ALIGN_BOTTOM_MID, 0, -10);
                
                bar_active = true;
            }
            break;
            
        case 3:  // Sound Settings
            show_sound_settings();
            break;
            
        case 4:  // Gauge Visibility
            showGauge = !showGauge;
            if (showGauge) {
                lv_obj_clear_flag(meter, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(meter, LV_OBJ_FLAG_HIDDEN);
            }
            saveSettings();
            menu_active = false;
            lv_obj_add_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);
            break;
            
        case 5:  // Exit
            if (emissivity_changed) {
                show_restart_confirmation();
            } else {
                menu_active = false;
                lv_obj_add_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);
            }
            break;
    }
    
    // Show main display elements when exiting menu
    if (!menu_active) {
        if (showGauge) lv_obj_clear_flag(meter, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(temp_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(temp_value_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(status_label, LV_OBJ_FLAG_HIDDEN);
    }
}

static void handle_button_press(int pin) {
    if (menu_active) {
        if (pin == BUTTON1_PIN) {  // Move selection up
            selected_menu_item = (selected_menu_item > 0) ? selected_menu_item - 1 : 5;
            playBeep(2000, 50);
            update_menu_selection();
        }
        else if (pin == BUTTON2_PIN) {  // Move selection down
            selected_menu_item = (selected_menu_item < 5) ? selected_menu_item + 1 : 0;
            playBeep(2000, 50);
            update_menu_selection();
        }
        else if (pin == KEY_PIN) {  // Select current item
            playBeep(2500, 50);
            handle_menu_selection(selected_menu_item);
        }
    } else {
        if (pin == BUTTON2_PIN) {  // Open menu
            menu_active = true;
            playBeep(2500, 50);
            lv_obj_clear_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);
            update_menu_selection();
        } else if (pin == BUTTON1_PIN) {  // Temperature unit toggle
            temp_in_celsius = !temp_in_celsius;
            playBeep(3000, 50);
            saveSettings();
        }
    }
}

// Display buffer
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[SCREEN_WIDTH * 10];

// Display driver
static lv_disp_drv_t disp_drv;

// Temperature unit

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

    // Initialize Power Manager
    M5.Power.begin();
    M5.Power.setChargeCurrent(1000);

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
    
    // Initialize EEPROM
    EEPROM.begin(EEPROM_SIZE);
    
    // Create styles
    lv_style_init(&style_text);
    lv_style_set_text_font(&style_text, &lv_font_montserrat_24);
    lv_style_set_text_color(&style_text, lv_palette_main(LV_PALETTE_LIGHT_BLUE));
    
    lv_style_init(&style_background);
    lv_style_set_bg_color(&style_background, lv_color_black());
    
    lv_style_init(&style_battery);
    lv_style_set_text_color(&style_battery, lv_color_white());
    lv_style_set_text_font(&style_battery, &lv_font_montserrat_16);
    
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
    if (temp_in_celsius) {
        lv_meter_set_scale_range(meter, temp_scale, TEMP_MIN_C, TEMP_MAX_C, 270, 135);
    } else {
        lv_meter_set_scale_range(meter, temp_scale, TEMP_MIN_F, TEMP_MAX_F, 270, 135);
    }
    
    // Add the temperature indicator (needle)
    temp_indic = lv_meter_add_needle_line(meter, temp_scale, 5, lv_palette_main(LV_PALETTE_RED), -10);
    
    // Add colored arcs for temperature ranges
    // Cold arc (blue)
    temp_arc_low = lv_meter_add_arc(meter, temp_scale, 15, lv_palette_main(LV_PALETTE_BLUE), 0);
    
    // Normal arc (green)
    temp_arc_normal = lv_meter_add_arc(meter, temp_scale, 15, lv_palette_main(LV_PALETTE_GREEN), 0);
    
    // Hot arc (red)
    temp_arc_high = lv_meter_add_arc(meter, temp_scale, 15, lv_palette_main(LV_PALETTE_RED), 0);
    
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
    
    // Create battery indicator
    battery_icon = lv_label_create(lv_scr_act());
    lv_obj_add_style(battery_icon, &style_battery, 0);
    lv_obj_align(battery_icon, LV_ALIGN_TOP_RIGHT, -10, 5);
    
    battery_label = lv_label_create(lv_scr_act());
    lv_obj_add_style(battery_label, &style_battery, 0);
    lv_obj_align(battery_label, LV_ALIGN_TOP_RIGHT, -35, 5);
    
    // Initialize LED
    FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(50);
    
    // Initialize buttons
    pinMode(BUTTON1_PIN, INPUT_PULLUP);
    pinMode(BUTTON2_PIN, INPUT_PULLUP);
    pinMode(KEY_PIN, INPUT_PULLUP);
    
    // Create settings panel
    create_settings_panel();
    
    Serial.println("Setup completed!");
    lastLvglTick = millis();
    
    // Load settings after display initialization
    loadSettings();
    
    // Initial LED color
    leds[0] = CRGB::Green;
    FastLED.show();
}

void loop() {
    M5.update();
    uint32_t currentMillis = millis();
    
    // Handle LVGL timing
    if (currentMillis - lastLvglTick > screenTickPeriod) {
        lv_timer_handler();
        lastLvglTick = currentMillis;
    }
    
    // Check Button2 press (Down/Next)
    bool button2State = digitalRead(BUTTON2_PIN);
    if (button2State == LOW && lastButton2State == HIGH) {
        handle_button_press(BUTTON2_PIN);
        delay(50); // Debounce
    }
    lastButton2State = button2State;
    
    // Check Button1 press (Up/Previous)
    bool button1State = digitalRead(BUTTON1_PIN);
    if (button1State == LOW && lastButton1State == HIGH) {
        handle_button_press(BUTTON1_PIN);
        delay(50); // Debounce
    }
    lastButton1State = button1State;
    
    // Check KEY press (Select/Enter)
    bool keyState = digitalRead(KEY_PIN);
    if (keyState == LOW && lastKeyState == HIGH) {
        if (bar_active) {
            // When emissivity bar is active, set the value and return to menu
            if (emissivity_bar) {
                lv_obj_del(lv_obj_get_parent(emissivity_bar));
                emissivity_bar = NULL;
                emissivity_label = NULL;
            }
            bar_active = false;
            
            // If emissivity was changed, show restart dialog
            if (emissivity_changed) {
                playBeep(2500, 50);
                show_restart_confirmation();
            } else {
                playBeep(3000, 50);
                menu_active = false;
                lv_obj_add_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);
            }
        } else if (menu_active) {
            playBeep(2500, 50);
            handle_menu_selection(selected_menu_item);
        } else {
            showGauge = !showGauge;
            playBeep(3000, 50);
            if (showGauge) {
                lv_obj_clear_flag(meter, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(meter, LV_OBJ_FLAG_HIDDEN);
            }
            saveSettings();
        }
        delay(50); // Debounce
    }
    lastKeyState = keyState;
    
    // Update temperature more frequently
    static uint32_t lastTempUpdate = 0;
    if (currentMillis - lastTempUpdate > TEMP_UPDATE_INTERVAL) {
        float objectTemp = readObjectTemperature();
        
        if (objectTemp > -273.15) {
            if (!temp_in_celsius) {
                objectTemp = objectTemp * 9.0 / 5.0 + 32.0; // Convert to Fahrenheit
            }
            
            char tempStr[32];
            snprintf(tempStr, sizeof(tempStr), "%d°%c", (int)round(objectTemp), temp_in_celsius ? 'C' : 'F');
            lv_label_set_text(temp_value_label, tempStr);
            
            // Update meter value and scale based on temperature unit
            if (temp_in_celsius) {
                lv_meter_set_scale_range(meter, temp_scale, TEMP_MIN_C, TEMP_MAX_C, 270, 135);
                lv_meter_set_indicator_value(meter, temp_indic, objectTemp);
                
                // Update arcs with smooth animation
                lv_meter_set_indicator_start_value(meter, temp_arc_low, TEMP_MIN_C);
                lv_meter_set_indicator_end_value(meter, temp_arc_low, MIN(objectTemp, TEMP_LOW_C));
                
                lv_meter_set_indicator_start_value(meter, temp_arc_normal, TEMP_LOW_C);
                lv_meter_set_indicator_end_value(meter, temp_arc_normal, MIN(objectTemp, TEMP_HIGH_C));
                
                lv_meter_set_indicator_start_value(meter, temp_arc_high, TEMP_HIGH_C);
                lv_meter_set_indicator_end_value(meter, temp_arc_high, MIN(objectTemp, TEMP_MAX_C));
                
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
                
                // Update arcs with smooth animation
                lv_meter_set_indicator_start_value(meter, temp_arc_low, TEMP_MIN_F);
                lv_meter_set_indicator_end_value(meter, temp_arc_low, MIN(objectTemp, TEMP_LOW_F));
                
                lv_meter_set_indicator_start_value(meter, temp_arc_normal, TEMP_LOW_F);
                lv_meter_set_indicator_end_value(meter, temp_arc_normal, MIN(objectTemp, TEMP_HIGH_F));
                
                lv_meter_set_indicator_start_value(meter, temp_arc_high, TEMP_HIGH_F);
                lv_meter_set_indicator_end_value(meter, temp_arc_high, MIN(objectTemp, TEMP_MAX_F));
                
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
    
    update_battery_status();
    
    delay(5);
}

static void update_battery_status() {
    int bat_level = M5.Power.getBatteryLevel();
    bool is_charging = M5.Power.isCharging();
    
    // Only update if values have changed
    if (bat_level != last_battery_level || is_charging != last_charging_state) {
        // Update battery percentage
        char bat_text[8];
        snprintf(bat_text, sizeof(bat_text), "%d%%", bat_level);
        lv_label_set_text(battery_label, bat_text);
        
        // Update battery icon with charging status
        const char* icon = is_charging ? LV_SYMBOL_CHARGE : LV_SYMBOL_BATTERY_FULL;
        lv_label_set_text(battery_icon, icon);
        
        // Update color based on battery level
        lv_color_t color;
        if (bat_level <= 20) {
            color = lv_color_make(255, 0, 0); // Red for low battery
        } else if (bat_level <= 50) {
            color = lv_color_make(255, 165, 0); // Orange for medium
        } else {
            color = lv_color_make(0, 255, 0); // Green for good
        }
        
        lv_obj_set_style_text_color(battery_label, color, 0);
        lv_obj_set_style_text_color(battery_icon, color, 0);
        
        last_battery_level = bat_level;
        last_charging_state = is_charging;
    }
}

static void update_restart_countdown() {
    if (restart_countdown > 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Restarting in %d seconds...", restart_countdown);
        if (restart_label != NULL) {
            lv_label_set_text(restart_label, buf);
        }
        playBeep(2000, 50);  // Beep for each second of countdown
        restart_countdown--;
        
        if (restart_countdown == 0) {
            playBeep(3000, 100);  // Longer beep before restart
            delay(100);  // Wait for beep to finish
            ESP.restart();
        }
    }
}

static void show_restart_confirmation() {
    static const char* btns[] = {"Yes, restart", "No, cancel", ""};
    
    // Create message box
    restart_msgbox = lv_msgbox_create(NULL, "Restart Required", NULL, btns, false);
    
    // Create and add the message label
    restart_label = lv_label_create(restart_msgbox);
    lv_label_set_text(restart_label, "Emissivity change requires a restart.\nWould you like to restart now?");
    
    // Position the label
    lv_obj_set_style_text_align(restart_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(restart_label, LV_ALIGN_TOP_MID, 0, 30);
    
    lv_obj_add_event_cb(restart_msgbox, restart_msgbox_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_center(restart_msgbox);
}

// Handle restart dialog events
static void restart_msgbox_event_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_current_target(e);
    const char *txt = lv_msgbox_get_active_btn_text(obj);
    
    if (txt && strcmp(txt, "Yes, restart") == 0) {
        playBeep(3000, 50);
        // Start countdown from 5 seconds
        restart_countdown = 5;
        lv_label_set_text(restart_label, "Restarting in 5 seconds...");
        
        // Remove buttons
        lv_obj_t *btnm = lv_msgbox_get_btns(restart_msgbox);
        if (btnm) {
            lv_obj_add_flag(btnm, LV_OBJ_FLAG_HIDDEN);
        }
        
        // Create timer for countdown
        restart_timer = lv_timer_create(restart_timer_cb, 1000, NULL);
        
        // Save settings
        saveSettings();
    } else {
        playBeep(1000, 50);
        // Cancel the change
        current_emissivity = pending_emissivity; // Restore the original value
        if (emissivity_bar != NULL) {
            lv_bar_set_value(emissivity_bar, current_emissivity * 100, LV_ANIM_OFF);
            char buf[32];
            snprintf(buf, sizeof(buf), "Emissivity: %.2f", current_emissivity);
            lv_label_set_text(emissivity_label, buf);
        }
        lv_obj_del(restart_msgbox);
        restart_msgbox = NULL;
        restart_label = NULL;
        emissivity_changed = false;
    }
}

// Timer callback for restart countdown
static void restart_timer_cb(lv_timer_t *timer) {
    update_restart_countdown();
}

static void setEmissivity(float value) {
    if (value != current_emissivity) {
        pending_emissivity = current_emissivity; // Store the original value
        current_emissivity = value;
        emissivity_changed = true;
        
        Wire.beginTransmission(0x5A);
        Wire.write(0x24);                   // Emissivity register address
        uint16_t emissivityData = value * 65535;
        Wire.write((uint8_t)(emissivityData >> 8));    // High byte
        Wire.write((uint8_t)(emissivityData & 0xFF));  // Low byte
        Wire.endTransmission();
    }
}

static void emissivity_slider_event_cb(lv_event_t *e) {
    lv_obj_t *bar = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING) {
        lv_point_t point;
        lv_indev_get_point(lv_indev_get_act(), &point);
        
        // Get bar coordinates and size
        lv_area_t bar_coords;
        lv_obj_get_coords(bar, &bar_coords);
        
        // Calculate relative position (0-1)
        float pos = (float)(point.x - bar_coords.x1) / (bar_coords.x2 - bar_coords.x1);
        if (pos < 0) pos = 0;
        if (pos > 1) pos = 1;
        
        // Calculate new emissivity value (0.65-1.00)
        float new_emissivity = 0.65f + (pos * 0.35f);
        
        // Update bar value
        lv_bar_set_value(bar, new_emissivity * 100, LV_ANIM_OFF);
        
        // Update label
        char buf[32];
        snprintf(buf, sizeof(buf), "Emissivity: %.2f", new_emissivity);
        lv_label_set_text(emissivity_label, buf);
        
        // Update emissivity
        setEmissivity(new_emissivity);
    }
}

static void show_sound_settings() {
    // Create dialog
    lv_obj_t *dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(dialog, 220, 200);
    lv_obj_center(dialog);
    
    // Title
    lv_obj_t *title = lv_label_create(dialog);
    lv_label_set_text(title, "Sound Settings");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Sound Enable Switch
    lv_obj_t *switch_label = lv_label_create(dialog);
    lv_label_set_text(switch_label, "Sound Enable:");
    lv_obj_align(switch_label, LV_ALIGN_TOP_LEFT, 20, 40);
    
    lv_obj_t *sound_switch = lv_switch_create(dialog);
    lv_obj_align(sound_switch, LV_ALIGN_TOP_RIGHT, -20, 40);
    if (sound_enabled) {
        lv_obj_add_state(sound_switch, LV_STATE_CHECKED);
    }
    
    // Volume Label
    lv_obj_t *vol_label = lv_label_create(dialog);
    lv_label_set_text(vol_label, "Volume:");
    lv_obj_align(vol_label, LV_ALIGN_TOP_LEFT, 20, 80);
    
    // Volume value label
    lv_obj_t *vol_value = lv_label_create(dialog);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", volume_level);
    lv_label_set_text(vol_value, buf);
    lv_obj_align(vol_value, LV_ALIGN_TOP_MID, 0, 80);
    
    // Volume buttons
    static const char * vol_btn_map[] = {"25%", "50%", "75%", "100%", ""};
    lv_obj_t *vol_btnm = lv_btnmatrix_create(dialog);
    lv_btnmatrix_set_map(vol_btnm, vol_btn_map);
    lv_obj_set_size(vol_btnm, 180, 40);
    lv_obj_align(vol_btnm, LV_ALIGN_TOP_MID, 0, 110);
    lv_btnmatrix_set_btn_ctrl_all(vol_btnm, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_one_checked(vol_btnm, true);
    lv_btnmatrix_set_btn_ctrl(vol_btnm, selected_volume, LV_BTNMATRIX_CTRL_CHECKED);
    
    // Event handlers
    lv_obj_add_event_cb(sound_switch, [](lv_event_t *e) {
        lv_obj_t *sw = lv_event_get_target(e);
        sound_enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
        if (sound_enabled) {
            playBeep(2000, 50);  // Beep for each second of countdown
        }
    }, LV_EVENT_VALUE_CHANGED, NULL);
    
    lv_obj_add_event_cb(vol_btnm, [](lv_event_t *e) {
        lv_obj_t *obj = lv_event_get_target(e);
        uint32_t id = lv_btnmatrix_get_selected_btn(obj);
        selected_volume = id;
        volume_level = (id + 1) * 25;
        
        // Update volume label
        lv_obj_t *label = lv_obj_get_child(lv_obj_get_parent(obj), 4); // Volume value label
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", volume_level);
        lv_label_set_text(label, buf);
        
        playBeep(2000, 50);  // Beep for each second of countdown
    }, LV_EVENT_VALUE_CHANGED, NULL);
    
    // OK Button
    lv_obj_t *btn = lv_btn_create(dialog);
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "OK");
    lv_obj_center(btn_label);
    lv_obj_set_size(btn, 100, 40);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    lv_obj_add_event_cb(btn, [](lv_event_t *e) {
        lv_obj_t *btn = lv_event_get_target(e);
        lv_obj_t *dialog = lv_obj_get_parent(btn);
        saveSettings();
        playBeep(2000, 50);  // Beep for each second of countdown
        lv_obj_del(dialog);
        menu_active = false;
        lv_obj_add_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);
    }, LV_EVENT_CLICKED, NULL);
}

static void playBeep(int frequency, int duration) {
    if (sound_enabled) {
        M5.Speaker.setVolume(volume_level);
        M5.Speaker.tone(frequency, duration);
    }
}