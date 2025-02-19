// Last updated 2/18/25 2:22 PM

#include "lv_conf.h"
#include <Adafruit_MLX90614.h>
#include <Wire.h>
#include <FastLED.h>
#include <M5Unified.h>
#include <lvgl.h>
#include <M5GFX.h>
#include <Preferences.h>
LV_FONT_DECLARE(lv_font_unscii_16);

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

// Temperature readings struct
struct TempReadings {
    float object;
    float ambient;
};

// Function prototype
TempReadings readTemperatures();

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
#define DEBUG_INTERVAL 5000       // Debug output every 5 seconds

// Settings structure
struct Settings {
    float emissivity = 0.95f;  // Default emissivity
    bool temp_in_celsius = false;
    uint8_t brightness = 5;
    bool show_gauge = false;  // Changed default to false
    bool sound_enabled = true;
    uint8_t volume = 40;
    
    void save() {
        Preferences prefs;
        prefs.begin("settings", false);  // false = RW mode
        
        // Clear preferences first to ensure clean state
        prefs.clear();
        
        // Save all settings
        prefs.putFloat("emissivity", emissivity);
        prefs.putBool("celsius", temp_in_celsius);
        prefs.putUChar("brightness", brightness);
        prefs.putBool("gauge", show_gauge);
        prefs.putBool("sound_en", sound_enabled);
        prefs.putUChar("volume", volume);
        prefs.end();
        Serial.printf("Saved settings - Emissivity: %.3f, Celsius: %d, Gauge: %d\n", 
                     emissivity, temp_in_celsius, show_gauge);
    }
    
    void load() {
        Preferences prefs;
        prefs.begin("settings", true);  // true = read-only mode
        
        // Load all settings with correct defaults matching struct
        emissivity = prefs.getFloat("emissivity", 0.95f);
        temp_in_celsius = prefs.getBool("celsius", false);  // Default to Fahrenheit
        brightness = prefs.getUChar("brightness", 5);
        show_gauge = prefs.getBool("gauge", false);  // Default to not showing gauge
        sound_enabled = prefs.getBool("sound_en", true);
        volume = prefs.getUChar("volume", 40);
        prefs.end();
        
        Serial.printf("Loaded settings - Emissivity: %.3f, Celsius: %d, Gauge: %d\n", 
                     emissivity, temp_in_celsius, show_gauge);
    }
} settings;

// Add brightness levels
#define BRIGHTNESS_LEVELS 4
static uint8_t brightness_values[BRIGHTNESS_LEVELS] = {25, 50, 75, 100};  // Percentage values
static const char* brightness_labels[BRIGHTNESS_LEVELS] = {"25%", "50%", "75%", "100%"};
static uint8_t current_brightness = 3;  // Default to 100%

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

// New UI elements
static lv_obj_t *trend_label = NULL;      // For up/down arrow
static lv_obj_t *ambient_label = NULL;    // For ambient temperature
static lv_obj_t *timestamp_label = NULL;  // For last reading time
static float last_temp = 0;               // For trend calculation

// State variables
static bool showGauge = false;      // Track gauge visibility
static bool lastKeyState = HIGH;    // Track key state for toggle
static bool lastButton1State = HIGH;
static bool lastButton2State = HIGH;
static bool showSettings = false;
static int selected_menu_item = 0;
static bool menu_active = false;
static bool temp_in_celsius = false;  // Moved here to global scope
static float current_emissivity = 0.95f;  // Default emissivity
static lv_obj_t *emissivity_slider = NULL;
static lv_obj_t *emissivity_label = NULL;
static bool bar_active = false;
static bool brightness_menu_active = false;
static uint8_t selected_brightness = 0;
static bool sound_menu_active = false;  // Added sound menu state
static lv_obj_t *sound_dialog = NULL;  // Added global sound dialog
static float temp_emissivity = 0.0f;  // Temporary emissivity value

// Sound settings
#define MENU_BEEP_FREQ 2000
#define CONFIRM_BEEP_FREQ 2500
#define ERROR_BEEP_FREQ 1000
#define BEEP_DURATION 50
#define LONG_BEEP_DURATION 100

// Volume steps for M5Stack CoreS3
#define VOLUME_MIN 64    // About 25% volume
#define VOLUME_MAX 255   // 100% volume
#define VOLUME_STEP 5    // Smaller steps for finer control with buttons

// Sound state variables
static bool sound_enabled = true;
static uint8_t volume_level = 40;  // 25-100%
static lv_obj_t *volume_slider = NULL;  // Global for button access

// Update global variables
static lv_obj_t *restart_msgbox = NULL;
static lv_obj_t *restart_label = NULL;
static bool emissivity_changed = false;
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
static void update_battery_status();
static void show_brightness_settings();
static void show_sound_settings();

// Global variables for restart dialog
static lv_obj_t *restart_cont = NULL;
static bool restart_dialog_active = false;

// Global MLX90614 object
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// Function to save settings
void saveSettings() {
    Serial.println("Saving settings...");
    settings.emissivity = current_emissivity;
    settings.temp_in_celsius = temp_in_celsius;
    settings.brightness = current_brightness;
    settings.show_gauge = showGauge;  // Save current gauge state
    settings.sound_enabled = sound_enabled;
    settings.volume = volume_level;
    settings.save();
    Serial.printf("Saved showGauge: %d\n", showGauge);  // Debug print
}

// Function to load settings
void loadSettings() {
    Serial.println("Loading settings...");
    settings.load();
    
    // Apply loaded settings
    current_emissivity = settings.emissivity;
    temp_in_celsius = settings.temp_in_celsius;
    current_brightness = settings.brightness;
    showGauge = settings.show_gauge;  // Load gauge state
    sound_enabled = settings.sound_enabled;
    volume_level = settings.volume;
    
    // Apply emissivity to sensor
    setEmissivity(current_emissivity);
    
    // Apply other settings
    M5.Lcd.setBrightness(map(brightness_values[current_brightness], 0, 100, 0, 255));
    
    // Update temperature display scale
    if (temp_in_celsius) {
        lv_meter_set_scale_range(meter, temp_scale, TEMP_MIN_C, TEMP_MAX_C, 270, 135);
    } else {
        lv_meter_set_scale_range(meter, temp_scale, TEMP_MIN_F, TEMP_MAX_F, 270, 135);
    }
    
    // Update gauge visibility based on loaded setting
    if (showGauge) {
        lv_obj_clear_flag(meter, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(meter, LV_OBJ_FLAG_HIDDEN);
    }
    
    Serial.printf("Loaded showGauge: %d\n", showGauge);  // Debug print
    Serial.println("Settings applied successfully");
}

static lv_obj_t *brightness_dialog = NULL;
static lv_obj_t *brightness_btnm = NULL;
static lv_obj_t *brightness_value_label = NULL;

static void update_brightness_display() {
    Serial.println("Updating brightness display...");  // Debug print - function entry
    
    if (brightness_dialog != NULL) {
        // Update the brightness value label
        if (brightness_value_label != NULL) {
            char buf[8];
            snprintf(buf, sizeof(buf), "%d%%", brightness_values[current_brightness]);
            lv_label_set_text(brightness_value_label, buf);
            Serial.print("Updated brightness label to: ");  // Debug print - value update
            Serial.println(buf);
        } else {
            Serial.println("Warning: brightness_value_label is NULL");  // Debug print - error condition
        }

        // Update button matrix selection
        if (brightness_btnm != NULL) {
            Serial.print("Setting button matrix selection to: ");  // Debug print - value update
            Serial.println(selected_brightness);
            
            for (uint8_t i = 0; i < BRIGHTNESS_LEVELS; i++) {
                if (i == selected_brightness) {
                    lv_btnmatrix_set_btn_ctrl(brightness_btnm, i, LV_BTNMATRIX_CTRL_CHECKED);
                } else {
                    lv_btnmatrix_clear_btn_ctrl(brightness_btnm, i, LV_BTNMATRIX_CTRL_CHECKED);
                }
            }
        } else {
            Serial.println("Warning: brightness_btnm is NULL");  // Debug print - error condition
        }

        // Update actual brightness
        uint8_t mapped_brightness = map(brightness_values[current_brightness], 0, 100, 0, 255);
        Serial.print("Setting LCD brightness to: ");  // Debug print - hardware update
        Serial.println(mapped_brightness);
        M5.Lcd.setBrightness(mapped_brightness);
        
        Serial.println("Brightness display update completed");  // Debug print - function exit
    } else {
        Serial.println("Error: brightness_dialog is NULL");  // Debug print - error condition
    }
}

static void close_brightness_menu() {
    Serial.println("Closing brightness menu");  // Debug print
    
    saveSettings();
    if (sound_enabled) {
        playBeep(CONFIRM_BEEP_FREQ, BEEP_DURATION);
    }
    
    // Ensure proper cleanup of all UI elements
    if (brightness_dialog != NULL) {
        lv_obj_del(brightness_dialog);
        brightness_dialog = NULL;
    }
    brightness_value_label = NULL;
    brightness_btnm = NULL;
    
    // Reset all menu states
    menu_active = false;
    brightness_menu_active = false;
    
    // Show main display elements
    if (showGauge) {
        lv_obj_clear_flag(meter, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_clear_flag(temp_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(temp_value_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(status_label, LV_OBJ_FLAG_HIDDEN);
    
    // Hide settings panel
    lv_obj_add_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);
    
    Serial.println("Brightness menu closed");  // Debug print
}

static void show_brightness_settings() {
    brightness_menu_active = true;
    menu_active = true;
    selected_brightness = current_brightness;  // Initialize selection to current brightness
    
    brightness_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(brightness_dialog, 220, 200);
    lv_obj_center(brightness_dialog);
    
    // Title
    lv_obj_t *title = lv_label_create(brightness_dialog);
    lv_label_set_text(title, "Brightness");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Current brightness label
    brightness_value_label = lv_label_create(brightness_dialog);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", brightness_values[current_brightness]);
    lv_label_set_text(brightness_value_label, buf);
    lv_obj_align(brightness_value_label, LV_ALIGN_TOP_MID, 0, 40);
    
    // Brightness buttons
    static const char * bright_btn_map[] = {"25%", "50%", "75%", "100%", ""};
    brightness_btnm = lv_btnmatrix_create(brightness_dialog);
    lv_btnmatrix_set_map(brightness_btnm, bright_btn_map);
    lv_obj_set_size(brightness_btnm, 180, 40);
    lv_obj_align(brightness_btnm, LV_ALIGN_TOP_MID, 0, 80);
    lv_btnmatrix_set_btn_ctrl_all(brightness_btnm, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_one_checked(brightness_btnm, true);
    lv_btnmatrix_set_btn_ctrl(brightness_btnm, current_brightness, LV_BTNMATRIX_CTRL_CHECKED);
    
    // Event handler for brightness buttons
    lv_obj_add_event_cb(brightness_btnm, [](lv_event_t *e) {
        lv_obj_t *obj = lv_event_get_target(e);
        uint32_t id = lv_btnmatrix_get_selected_btn(obj);
        selected_brightness = id;
        current_brightness = selected_brightness;
        update_brightness_display();
        
        if (sound_enabled) {
            playBeep(MENU_BEEP_FREQ, BEEP_DURATION);
        }
    }, LV_EVENT_VALUE_CHANGED, NULL);
    
    // OK Button
    lv_obj_t *ok_btn = lv_btn_create(brightness_dialog);
    lv_obj_t *btn_label = lv_label_create(ok_btn);
    lv_label_set_text(btn_label, "OK");
    lv_obj_center(btn_label);
    lv_obj_set_size(ok_btn, 100, 40);
    lv_obj_align(ok_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    
    // Event handler for OK button
    lv_obj_add_event_cb(ok_btn, [](lv_event_t *e) {
        saveSettings();
        if (sound_enabled) {
            playBeep(CONFIRM_BEEP_FREQ, BEEP_DURATION);
        }
        close_brightness_menu();
    }, LV_EVENT_CLICKED, NULL);
    
    // Initial brightness update
    update_brightness_display();
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
    lv_label_set_text(nav_label, "Red: Up | Blue: Down | KEY: Select");
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
            show_brightness_settings();
            break;
            
        case 2:  // Emissivity
            if (!bar_active) {
                createEmissivityDialog();
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
    if (restart_dialog_active) {
        handle_restart_button(pin);
        return;
    }
    
    if (bar_active) {
        // Handle emissivity adjustment
        if (pin == BUTTON1_PIN) {
            // Decrease emissivity (minimum 0.65)
            float new_emissivity = max(0.65f, current_emissivity - 0.01f);
            lv_slider_set_value(emissivity_slider, new_emissivity * 100, LV_ANIM_OFF);
            char buf[32];
            snprintf(buf, sizeof(buf), "%.2f", new_emissivity);
            lv_label_set_text(emissivity_label, buf);
            current_emissivity = new_emissivity;
        } else if (pin == BUTTON2_PIN) {
            // Increase emissivity (maximum 1.00)
            float new_emissivity = min(1.00f, current_emissivity + 0.01f);
            lv_slider_set_value(emissivity_slider, new_emissivity * 100, LV_ANIM_OFF);
            char buf[32];
            snprintf(buf, sizeof(buf), "%.2f", new_emissivity);
            lv_label_set_text(emissivity_label, buf);
            current_emissivity = new_emissivity;
        }
        return;
    }
    if (sound_menu_active) {  // Sound menu handling
        if (pin == BUTTON1_PIN) {
            volume_level = (volume_level > 25) ? volume_level - VOLUME_STEP : 25;
            update_volume_selection();
        } else if (pin == BUTTON2_PIN) {
            volume_level = (volume_level < 100) ? volume_level + VOLUME_STEP : 100;
            update_volume_selection();
        }
        return;  // Exit early to prevent menu navigation
    } else if (brightness_menu_active) {  // Brightness menu handling
        Serial.print("Handling brightness button press: PIN ");  // Debug print - event context
        Serial.println(pin);
        
        if (pin == BUTTON1_PIN) {
            uint8_t old_brightness = selected_brightness;  // Store old value for debug
            selected_brightness = (selected_brightness > 0) ? selected_brightness - 1 : 3;
            Serial.print("Decreased brightness from ");  // Debug print - value change
            Serial.print(old_brightness);
            Serial.print(" to ");
            Serial.println(selected_brightness);
            
            current_brightness = selected_brightness;
            update_brightness_display();
            if (sound_enabled) {
                playBeep(MENU_BEEP_FREQ, BEEP_DURATION);
            }
        } else if (pin == BUTTON2_PIN) {
            uint8_t old_brightness = selected_brightness;  // Store old value for debug
            selected_brightness = (selected_brightness < 3) ? selected_brightness + 1 : 0;
            Serial.print("Increased brightness from ");  // Debug print - value change
            Serial.print(old_brightness);
            Serial.print(" to ");
            Serial.println(selected_brightness);
            
            current_brightness = selected_brightness;
            update_brightness_display();
            if (sound_enabled) {
                playBeep(MENU_BEEP_FREQ, BEEP_DURATION);
            }
        }
        return;  // Exit early to prevent menu navigation
    } else if (menu_active) {  // Settings menu handling
        if (pin == BUTTON1_PIN) {  // Move selection up
            selected_menu_item = (selected_menu_item > 0) ? selected_menu_item - 1 : 5;
            if (sound_enabled) {
                playBeep(MENU_BEEP_FREQ, BEEP_DURATION);
            }
            update_menu_selection();
        } else if (pin == BUTTON2_PIN) {  // Move selection down
            selected_menu_item = (selected_menu_item < 5) ? selected_menu_item + 1 : 0;
            if (sound_enabled) {
                playBeep(MENU_BEEP_FREQ, BEEP_DURATION);
            }
            update_menu_selection();
        } else if (pin == KEY_PIN) {  // Select menu item
            if (sound_enabled) {
                playBeep(CONFIRM_BEEP_FREQ, BEEP_DURATION);
            }
            handle_menu_selection(selected_menu_item);
        }
        return;  // Exit early after handling menu
    }
    
    // Main screen button handling
    if (pin == BUTTON2_PIN) {  // Open menu
        menu_active = true;
        if (sound_enabled) {
            playBeep(CONFIRM_BEEP_FREQ, BEEP_DURATION);
        }
        lv_obj_clear_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);
        update_menu_selection();
    } else if (pin == BUTTON1_PIN) {  // Temperature unit toggle
        temp_in_celsius = !temp_in_celsius;
        if (sound_enabled) {
            playBeep(MENU_BEEP_FREQ, BEEP_DURATION);
        }
        saveSettings();
    }
}

// Display buffer
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[SCREEN_WIDTH * 10];

// Display driver
static lv_disp_drv_t disp_drv;

// Function to read current emissivity from sensor
float readEmissivity() {
    float emissivity = mlx.readEmissivity();
    Serial.printf("Current sensor emissivity: %.3f\n", emissivity);
    return emissivity;
}

// Function to set emissivity
void setEmissivity(float value) {
    if (value >= 0.65f && value <= 1.0f) {
        Serial.printf("Setting emissivity to: %.3f\n", value);
        
        // Get current value for comparison
        float oldEmissivity = mlx.readEmissivity();
        
        // Write new emissivity value
        mlx.writeEmissivity(value);
        
        // Add delay for EEPROM write
        delay(100);
        
        // Verify the change
        float newEmissivity = mlx.readEmissivity();
        
        if (abs(newEmissivity - value) <= 0.01) {
            Serial.println("Emissivity update successful");
            current_emissivity = value;
            // Only save to settings if value is valid
            if (value >= 0.65f && value <= 1.0f) {
                settings.emissivity = value;
                settings.save();
                Serial.printf("Saved emissivity: %.3f to settings\n", value);
            }
        } else {
            Serial.printf("Emissivity update may have failed. Wanted: %.3f, Got: %.3f\n", 
                        value, newEmissivity);
            // Only update if the new value is valid
            if (newEmissivity >= 0.65f && newEmissivity <= 1.0f) {
                current_emissivity = newEmissivity;
                settings.emissivity = newEmissivity;
                settings.save();
                Serial.printf("Saved new emissivity: %.3f to settings\n", newEmissivity);
            }
        }
    } else {
        Serial.printf("Invalid emissivity value: %.3f (must be between 0.65 and 1.0)\n", value);
    }
}

TempReadings readTemperatures() {
    TempReadings readings;
    
    // Always read in Celsius for internal consistency
    readings.ambient = mlx.readAmbientTempC();
    readings.object = mlx.readObjectTempC();
    
    return readings;
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

    // Initialize MLX90614
    if (!mlx.begin()) {
        Serial.println("Error connecting to MLX sensor. Check wiring.");
        while (1); // Don't proceed if sensor init fails
    }
    Serial.println("MLX90614 found!");
    
    // Read initial emissivity from sensor
    float sensorEmissivity = mlx.readEmissivity();
    Serial.printf("Initial sensor emissivity: %.3f\n", sensorEmissivity);
    
    // Load settings
    settings.load();
    
    // Initialize emissivity
    if (settings.emissivity >= 0.65f && settings.emissivity <= 1.0f) {
        current_emissivity = settings.emissivity;
        if (abs(current_emissivity - sensorEmissivity) > 0.01) {
            mlx.writeEmissivity(current_emissivity);
            delay(100); // Allow time for write
        }
    } else {
        current_emissivity = 0.95f; // Use default if saved value is invalid
        settings.emissivity = current_emissivity;
        settings.save();
        mlx.writeEmissivity(current_emissivity);
        delay(100); // Allow time for write
    }
    
    Serial.printf("Using emissivity: %.3f\n", current_emissivity);
    
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
    // No need to initialize EEPROM as we are using Preferences library
    
    // Create styles
    lv_style_init(&style_text);
    lv_style_set_text_font(&style_text, &lv_font_montserrat_24);
    lv_style_set_text_color(&style_text, lv_palette_main(LV_PALETTE_LIGHT_BLUE));
    
    lv_style_init(&style_background);
    lv_style_set_bg_color(&style_background, lv_color_black());
    
    lv_style_init(&style_battery);
    lv_style_set_text_color(&style_battery, lv_color_white());
    lv_style_set_text_font(&style_battery, &lv_font_montserrat_16);
    
    // Create main screen container
    lv_obj_t *main_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(main_cont, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_pad_all(main_cont, 10, 0);
    lv_obj_set_style_bg_color(main_cont, lv_color_black(), 0);
    lv_obj_set_style_border_width(main_cont, 0, 0);
    
    // Create temperature display area
    lv_obj_t *temp_cont = lv_obj_create(main_cont);
    lv_obj_set_size(temp_cont, 280, 120);
    lv_obj_align(temp_cont, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_bg_color(temp_cont, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_radius(temp_cont, 10, 0);
    lv_obj_set_style_border_width(temp_cont, 2, 0);
    lv_obj_set_style_border_color(temp_cont, lv_color_hex(0x333333), 0);
    
    // Create and style temperature label
    temp_label = lv_label_create(temp_cont);
    lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(temp_label, lv_color_hex(0xcccccc), 0);
    lv_label_set_text(temp_label, "Temperature");
    lv_obj_align(temp_label, LV_ALIGN_TOP_MID, 0, 10);
    
    // Create and style temperature value
    temp_value_label = lv_label_create(temp_cont);
    lv_obj_set_style_text_font(temp_value_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(temp_value_label, lv_color_white(), 0);
    lv_label_set_text(temp_value_label, "");
    lv_obj_align(temp_value_label, LV_ALIGN_CENTER, 0, 10);
    
    // Create trend indicator
    trend_label = lv_label_create(temp_cont);
    lv_obj_set_style_text_font(trend_label, &lv_font_montserrat_16, 0);
    lv_obj_align(trend_label, LV_ALIGN_RIGHT_MID, -20, 0);
    
    // Create status label
    status_label = lv_label_create(main_cont);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);
    lv_label_set_text(status_label, "");
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 140);  // Adjusted position below temperature container
    
    // Create ambient temperature display
    ambient_label = lv_label_create(main_cont);
    lv_obj_set_style_text_font(ambient_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ambient_label, lv_color_hex(0x888888), 0);
    lv_obj_align(ambient_label, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    
    // Create timestamp display
    timestamp_label = lv_label_create(main_cont);
    lv_obj_set_style_text_font(timestamp_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(timestamp_label, lv_color_hex(0x888888), 0);
    lv_obj_align(timestamp_label, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    
    // Create meter with adjusted size
    meter = lv_meter_create(main_cont);
    lv_obj_set_size(meter, 160, 160);  // Reduced from 200x200
    lv_obj_align(meter, LV_ALIGN_BOTTOM_MID, 0, -20);  // Position at bottom
    
    // Create scale
    temp_scale = lv_meter_add_scale(meter);
    lv_meter_set_scale_ticks(meter, temp_scale, 41, 2, 10, lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_major_ticks(meter, temp_scale, 8, 4, 15, lv_color_white(), 10);
    
    // Adjust scale range based on temperature unit
    if (temp_in_celsius) {
        lv_meter_set_scale_range(meter, temp_scale, TEMP_MIN_C, TEMP_MAX_C, 270, 135);
    } else {
        lv_meter_set_scale_range(meter, temp_scale, TEMP_MIN_F, TEMP_MAX_F, 270, 135);
    }
    
    // Create indicator with adjusted size
    temp_indic = lv_meter_add_needle_line(meter, temp_scale, 4, lv_palette_main(LV_PALETTE_RED), -10);
    
    // Create arcs with adjusted width
    // Cold arc (blue)
    temp_arc_low = lv_meter_add_arc(meter, temp_scale, 10, lv_palette_main(LV_PALETTE_BLUE), 0);
    
    // Good range arc (green)
    temp_arc_normal = lv_meter_add_arc(meter, temp_scale, 10, lv_palette_main(LV_PALETTE_GREEN), 0);
    
    // Hot arc (red)
    temp_arc_high = lv_meter_add_arc(meter, temp_scale, 10, lv_palette_main(LV_PALETTE_RED), 0);

    // Create battery indicator
    battery_icon = lv_label_create(lv_scr_act());
    lv_obj_add_style(battery_icon, &style_battery, 0);
    lv_obj_align(battery_icon, LV_ALIGN_TOP_RIGHT, -10, 5);
    
    battery_label = lv_label_create(lv_scr_act());
    lv_obj_add_style(battery_label, &style_battery, 0);
    lv_obj_align(battery_label, LV_ALIGN_TOP_RIGHT, -35, 5);
    
    // Initialize LED
    FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(255);
    
    // Initialize buttons
    pinMode(BUTTON1_PIN, INPUT_PULLUP);
    pinMode(BUTTON2_PIN, INPUT_PULLUP);
    pinMode(KEY_PIN, INPUT_PULLUP);
    
    // Create settings panel
    create_settings_panel();
    
    Serial.println("Setup completed!");
    lastLvglTick = millis();
    
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
        Serial.println("Button 2 pressed");  // Debug print
        handle_button_press(BUTTON2_PIN);
        delay(50); // Debounce
    }
    lastButton2State = button2State;
    
    // Check Button1 press (Up/Previous)
    bool button1State = digitalRead(BUTTON1_PIN);
    if (button1State == LOW && lastButton1State == HIGH) {
        Serial.println("Button 1 pressed");  // Debug print
        handle_button_press(BUTTON1_PIN);
        delay(50); // Debounce
    }
    lastButton1State = button1State;
    
    // Key handling
    bool keyState = digitalRead(KEY_PIN);
    if (keyState != lastKeyState) {
        if (keyState == LOW) {  // Key pressed
            Serial.println("Key unit pressed");  // Debug print
            
            if (bar_active) {
                Serial.println("Key press detected in emissivity menu");  // Debug print
                // Show restart confirmation
                show_restart_confirmation();
            }
            else if (brightness_menu_active) {
                Serial.println("Key press detected in brightness menu");  // Debug print
                close_brightness_menu();
            } else if (sound_menu_active) {
                Serial.println("Key press detected in sound menu");  // Debug print
                close_sound_menu();
            } else if (menu_active) {
                Serial.println("Key press detected in main menu");  // Debug print
                handle_button_press(KEY_PIN);
            } else {
                Serial.println("Key press detected in main screen");  // Debug print
                showSettings = !showSettings;
                if (showSettings) {
                    create_settings_panel();
                } else {
                    lv_obj_add_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);
                }
            }
            delay(50); // Debounce
        }
    }
    lastKeyState = keyState;
    
    // Update temperature readings and display
    static uint32_t lastDebugOutput = 0;
    static uint32_t lastTempUpdate = 0;
    
    if (currentMillis - lastTempUpdate >= TEMP_UPDATE_INTERVAL) {
        TempReadings temps = readTemperatures();
        updateTemperatureDisplay(temps);
        lastTempUpdate = currentMillis;
        
        // Debug output every DEBUG_INTERVAL
        if (currentMillis - lastDebugOutput >= DEBUG_INTERVAL) {
            Serial.println("----------------------------------------");
            Serial.printf("Emissivity: %.3f\n", current_emissivity);
            Serial.printf("Ambient: %.2f°C\n", temps.ambient);
            Serial.printf("Object: %.2f°C\n", temps.object);
            Serial.println("----------------------------------------");
            lastDebugOutput = currentMillis;
        }
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
    restart_dialog_active = true;
    
    // Create a styled container
    restart_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(restart_cont, 280, 160);
    lv_obj_center(restart_cont);
    lv_obj_set_style_bg_color(restart_cont, lv_color_hex(0x303030), 0);
    lv_obj_set_style_border_color(restart_cont, lv_color_hex(0x404040), 0);
    lv_obj_set_style_border_width(restart_cont, 2, 0);
    lv_obj_set_style_radius(restart_cont, 10, 0);  // Rounded corners
    lv_obj_set_style_pad_all(restart_cont, 15, 0); // Inner padding
    
    // Title
    lv_obj_t *title = lv_label_create(restart_cont);
    lv_label_set_text(title, "Restart Required");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    // Message
    lv_obj_t *msg = lv_label_create(restart_cont);
    lv_label_set_text(msg, "For the new emissivity to take effect,\nthe device must be restarted.\n\nDo you wish to continue?");
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_14, 0);
    lv_obj_align(msg, LV_ALIGN_TOP_MID, 0, 30);

    // Button labels
    lv_obj_t *btn1_label = lv_label_create(restart_cont);
    lv_label_set_text(btn1_label, "BTN1: No, cancel");
    lv_obj_set_style_text_font(btn1_label, &lv_font_montserrat_14, 0);
    lv_obj_align(btn1_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *btn2_label = lv_label_create(restart_cont);
    lv_label_set_text(btn2_label, "BTN2: Yes, restart");
    lv_obj_set_style_text_font(btn2_label, &lv_font_montserrat_14, 0);
    lv_obj_align(btn2_label, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

static void handle_restart_button(int pin) {
    if (!restart_dialog_active) return;
    
    if (pin == BUTTON1_PIN) {
        // No, cancel
        // Revert emissivity change
        if (bar_active) {
            // Revert the bar and label to previous value
            lv_slider_set_value(emissivity_slider, current_emissivity * 100, LV_ANIM_OFF);
            char buf[32];
            snprintf(buf, sizeof(buf), "%.2f", current_emissivity);
            lv_label_set_text(emissivity_label, buf);
        }
        // Close dialog
        lv_obj_del(restart_cont);
        restart_dialog_active = false;
    } 
    else if (pin == BUTTON2_PIN) {
        // Yes, restart
        // Save the new emissivity value
        current_emissivity = temp_emissivity;
        saveSettings();
        
        // Close emissivity menu if open
        if (bar_active) {
            lv_obj_t* parent = lv_obj_get_parent(emissivity_slider);
            lv_obj_del(parent);
            emissivity_slider = NULL;
            emissivity_label = NULL;
            bar_active = false;
            menu_active = false;
        }
        
        // Close restart dialog
        lv_obj_del(restart_cont);
        restart_dialog_active = false;
        
        // Start restart countdown
        restart_countdown = 3;
        lv_timer_t *timer = lv_timer_create(restart_timer_cb, 1000, NULL);
        lv_timer_set_repeat_count(timer, 3);
        
        // Show countdown message
        char buf[64];
        snprintf(buf, sizeof(buf), "Restarting in %d...", restart_countdown);
        restart_label = lv_label_create(lv_scr_act());
        lv_obj_set_style_text_font(restart_label, &lv_font_montserrat_24, 0);
        lv_label_set_text(restart_label, buf);
        lv_obj_center(restart_label);
    }
}

static void update_volume_selection() {
    if (sound_dialog != NULL && volume_slider != NULL) {
        // Update slider value
        lv_slider_set_value(volume_slider, volume_level, LV_ANIM_ON);
        
        // Update volume label
        lv_obj_t *volume_label = lv_obj_get_child(sound_dialog, 3);  // Volume value label
        if (volume_label != NULL) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d%%", volume_level);
            lv_label_set_text(volume_label, buf);
        }
        
        if (sound_enabled) {
            playBeep(MENU_BEEP_FREQ, BEEP_DURATION);
        }
    }
}

static void close_sound_menu() {
    if (sound_dialog != NULL) {
        saveSettings();
        if (sound_enabled) {
            playBeep(CONFIRM_BEEP_FREQ, BEEP_DURATION);
        }
        lv_obj_del(sound_dialog);
        sound_dialog = NULL;
        sound_menu_active = false;
        menu_active = false;
        lv_obj_add_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

static void show_sound_settings() {
    // Set menu state
    sound_menu_active = true;
    menu_active = true;
    
    // Create dialog with increased height
    sound_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(sound_dialog, 220, 280);
    lv_obj_center(sound_dialog);
    
    // Title
    lv_obj_t *title = lv_label_create(sound_dialog);
    lv_label_set_text(title, "Sound Settings");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Sound toggle switch
    lv_obj_t *sound_switch = lv_switch_create(sound_dialog);
    lv_obj_align(sound_switch, LV_ALIGN_TOP_MID, 0, 50);
    if (sound_enabled) {
        lv_obj_add_state(sound_switch, LV_STATE_CHECKED);
    }
    
    // Switch label
    lv_obj_t *switch_label = lv_label_create(sound_dialog);
    lv_label_set_text(switch_label, sound_enabled ? "Sound: ON" : "Sound: OFF");
    lv_obj_align(switch_label, LV_ALIGN_TOP_MID, 0, 90);
    
    // Volume label with value
    lv_obj_t *volume_value = lv_label_create(sound_dialog);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", volume_level);
    lv_label_set_text(volume_value, buf);
    lv_obj_align(volume_value, LV_ALIGN_TOP_MID, 0, 120);
    
    // Create volume slider
    volume_slider = lv_slider_create(sound_dialog);
    lv_obj_set_size(volume_slider, 180, 15);
    lv_obj_align(volume_slider, LV_ALIGN_TOP_MID, 0, 150);
    lv_slider_set_range(volume_slider, 25, 100);  // 25% to 100%
    lv_slider_set_value(volume_slider, volume_level, LV_ANIM_OFF);
    
    // Event handler for switch
    lv_obj_add_event_cb(sound_switch, [](lv_event_t *e) {
        lv_obj_t *sw = lv_event_get_target(e);
        bool is_checked = lv_obj_has_state(sw, LV_STATE_CHECKED);
        sound_enabled = is_checked;
        
        // Update switch label
        lv_obj_t *label = lv_obj_get_child(lv_obj_get_parent(sw), 2);
        if (label != NULL) {
            lv_label_set_text(label, sound_enabled ? "Sound: ON" : "Sound: OFF");
        }
        
        if (sound_enabled) {
            playBeep(MENU_BEEP_FREQ, BEEP_DURATION);
        }
    }, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Event handler for slider
    lv_obj_add_event_cb(volume_slider, [](lv_event_t *e) {
        lv_obj_t *slider = lv_event_get_target(e);
        volume_level = lv_slider_get_value(slider);
        
        // Update volume label
        lv_obj_t *label = lv_obj_get_child(lv_obj_get_parent(slider), 3);  // Volume value label
        if (label != NULL) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d%%", volume_level);
            lv_label_set_text(label, buf);
        }
        
        if (sound_enabled) {
            playBeep(MENU_BEEP_FREQ, BEEP_DURATION);
        }
    }, LV_EVENT_VALUE_CHANGED, NULL);
    
    // OK Button
    lv_obj_t *ok_btn = lv_btn_create(sound_dialog);
    lv_obj_t *btn_label = lv_label_create(ok_btn);
    lv_label_set_text(btn_label, "OK");
    lv_obj_center(btn_label);
    lv_obj_set_size(ok_btn, 100, 40);
    lv_obj_align(ok_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    
    // Event handler for OK button
    lv_obj_add_event_cb(ok_btn, [](lv_event_t *e) {
        close_sound_menu();
    }, LV_EVENT_CLICKED, NULL);
}

static void playBeep(int frequency, int duration) {
    if (sound_enabled) {
        // Convert percentage to M5Stack volume range (64-255)
        uint8_t m5_volume = map(volume_level, 25, 100, VOLUME_MIN, VOLUME_MAX);
        M5.Speaker.setVolume(m5_volume);
        M5.Speaker.tone(frequency, duration);
    }
}

static void restart_timer_cb(lv_timer_t *timer) {
    restart_countdown--;
    if (restart_countdown > 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Restarting in %d...", restart_countdown);
        lv_label_set_text(restart_label, buf);
    } else {
        ESP.restart();  // Restart the device
    }
}

static void updateTemperatureDisplay(TempReadings temps) {
    float objTemp = temps.object;
    float ambTemp = temps.ambient;
    
    // Update temperature display
    char tempStr[32];
    if (objTemp != 0) {  // Only update if we got a valid reading
        // Convert to Fahrenheit for display if needed
        float displayTemp = temp_in_celsius ? objTemp : (objTemp * 9.0/5.0 + 32.0);
        float displayAmbient = temp_in_celsius ? ambTemp : (ambTemp * 9.0/5.0 + 32.0);
        
        snprintf(tempStr, sizeof(tempStr), "%d°%c", (int)round(displayTemp), temp_in_celsius ? 'C' : 'F');
        lv_label_set_text(temp_value_label, tempStr);
        
        // Update gauge - convert value and ranges if in Fahrenheit
        float gauge_value = temp_in_celsius ? objTemp : (objTemp * 9.0/5.0 + 32.0);
        lv_meter_set_indicator_value(meter, temp_indic, gauge_value);
        
        // Update arcs based on current temperature unit
        float min_temp = temp_in_celsius ? TEMP_MIN_C : TEMP_MIN_F;
        float low_temp = temp_in_celsius ? TEMP_LOW_C : TEMP_LOW_F;
        float high_temp = temp_in_celsius ? TEMP_HIGH_C : TEMP_HIGH_F;
        float max_temp = temp_in_celsius ? TEMP_MAX_C : TEMP_MAX_F;
        
        // Update arcs
        lv_meter_set_indicator_start_value(meter, temp_arc_low, min_temp);
        lv_meter_set_indicator_end_value(meter, temp_arc_low, MIN(gauge_value, low_temp));
        
        lv_meter_set_indicator_start_value(meter, temp_arc_normal, low_temp);
        lv_meter_set_indicator_end_value(meter, temp_arc_normal, MIN(gauge_value, high_temp));
        
        lv_meter_set_indicator_start_value(meter, temp_arc_high, high_temp);
        lv_meter_set_indicator_end_value(meter, temp_arc_high, MIN(gauge_value, max_temp));

        // Calculate trend using Celsius values for consistency
        const char* trend_arrow = (objTemp > last_temp + 0.5) ? LV_SYMBOL_UP : 
                                (objTemp < last_temp - 0.5) ? LV_SYMBOL_DOWN : LV_SYMBOL_RIGHT;
        lv_label_set_text(trend_label, trend_arrow);
        
        // Update status - convert to F for status checks
        float temp_f = objTemp * 9.0/5.0 + 32.0;  // Always convert to F for status since thresholds are in F
        const char* status_text;
        lv_color_t status_color;
        
        if (temp_f < TEMP_TOO_COLD_F) {
            status_text = "TOO COLD";
            status_color = lv_color_hex(0x00ffff);
        } else if (temp_f < TEMP_LOW_F) {
            status_text = "COLD";
            status_color = lv_color_hex(0x0088ff);
        } else if (temp_f <= TEMP_HIGH_F) {
            status_text = "GOOD";
            status_color = lv_color_hex(0x00ff00);
        } else if (temp_f <= TEMP_TOO_HOT_F) {
            status_text = "HOT";
            status_color = lv_color_hex(0xff8800);
        } else {
            status_text = "TOO HOT";
            status_color = lv_color_hex(0xff0000);
        }
        
        lv_label_set_text(status_label, status_text);
        lv_obj_set_style_text_color(status_label, status_color, 0);
        
        // Update ambient temperature
        char ambient_buf[32];
        snprintf(ambient_buf, sizeof(ambient_buf), "Ambient: %d°%c", 
                (int)round(displayAmbient),
                temp_in_celsius ? 'C' : 'F');
        lv_label_set_text(ambient_label, ambient_buf);
        
        last_temp = objTemp;  // Store in Celsius for consistent trend calculation
    } else {
        Serial.println("Invalid temperature reading");
        lv_label_set_text(temp_value_label, "Error");
        lv_label_set_text(status_label, "ERROR");
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xff0000), 0);
    }
}

static void createEmissivityDialog() {
    // Create a styled container for emissivity control
    lv_obj_t *emissivity_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(emissivity_cont, 280, 160);  // Larger container
    lv_obj_align(emissivity_cont, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(emissivity_cont, lv_color_hex(0x303030), 0);
    lv_obj_set_style_border_color(emissivity_cont, lv_color_hex(0x404040), 0);
    lv_obj_set_style_border_width(emissivity_cont, 2, 0);
    lv_obj_set_style_radius(emissivity_cont, 10, 0);  // Rounded corners
    lv_obj_set_style_pad_all(emissivity_cont, 15, 0); // Inner padding
    
    // Title label
    lv_obj_t *title_label = lv_label_create(emissivity_cont);
    lv_label_set_text(title_label, "Emissivity Setting");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, 0);  // Larger font
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 0);
    
    // Create slider with styling
    emissivity_slider = lv_slider_create(emissivity_cont);
    lv_obj_set_size(emissivity_slider, 240, 20);
    lv_obj_align(emissivity_slider, LV_ALIGN_CENTER, 0, 0);
    lv_slider_set_range(emissivity_slider, 65, 100);  // Range 0.65 to 1.00 (multiplied by 100)
    lv_slider_set_value(emissivity_slider, current_emissivity * 100, LV_ANIM_OFF);
    
    // Style the slider
    lv_obj_set_style_bg_color(emissivity_slider, lv_color_hex(0x202020), LV_PART_MAIN);
    lv_obj_set_style_bg_color(emissivity_slider, lv_color_hex(0x00E0FF), LV_PART_INDICATOR);
    
    // Range labels
    lv_obj_t *min_label = lv_label_create(emissivity_cont);
    lv_label_set_text(min_label, "0.65");
    lv_obj_align_to(min_label, emissivity_slider, LV_ALIGN_OUT_LEFT_MID, -10, 0);
    
    lv_obj_t *max_label = lv_label_create(emissivity_cont);
    lv_label_set_text(max_label, "1.00");
    lv_obj_align_to(max_label, emissivity_slider, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
    
    // Current value label with styling
    emissivity_label = lv_label_create(emissivity_cont);
    lv_label_set_text_fmt(emissivity_label, "%.2f", current_emissivity);
    lv_obj_set_style_text_font(emissivity_label, &lv_font_montserrat_16, 0);
    lv_obj_align(emissivity_label, LV_ALIGN_CENTER, 0, 30);
    
    // Add event handler for slider
    lv_obj_add_event_cb(emissivity_slider, emissivity_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    bar_active = true;
}

static void emissivity_slider_event_cb(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);
    float new_emissivity = value / 100.0f;  // Convert slider value to emissivity
    
    // Update label and emissivity
    lv_label_set_text_fmt(emissivity_label, "%.2f", new_emissivity);
    current_emissivity = new_emissivity;
    mlx.writeEmissivity(current_emissivity);
    settings.emissivity = current_emissivity;
    settings.save();
}

static void handleEmissivityButtons(uint8_t pin) {
    if (bar_active) {
        if (pin == BUTTON1_PIN) {
            // Decrease emissivity
            int32_t current_value = lv_slider_get_value(emissivity_slider);
            if (current_value > 65) {  // 0.65 minimum
                lv_slider_set_value(emissivity_slider, current_value - 1, LV_ANIM_OFF);
                float new_emissivity = (current_value - 1) / 100.0f;
                lv_label_set_text_fmt(emissivity_label, "%.2f", new_emissivity);
                current_emissivity = new_emissivity;
                mlx.writeEmissivity(current_emissivity);
                settings.emissivity = current_emissivity;
                settings.save();
            }
        } else if (pin == BUTTON2_PIN) {
            // Increase emissivity
            int32_t current_value = lv_slider_get_value(emissivity_slider);
            if (current_value < 100) {  // 1.00 maximum
                lv_slider_set_value(emissivity_slider, current_value + 1, LV_ANIM_OFF);
                float new_emissivity = (current_value + 1) / 100.0f;
                lv_label_set_text_fmt(emissivity_label, "%.2f", new_emissivity);
                current_emissivity = new_emissivity;
                mlx.writeEmissivity(current_emissivity);
                settings.emissivity = current_emissivity;
                settings.save();
            }
        }
    }
}