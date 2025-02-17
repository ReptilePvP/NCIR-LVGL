# M5Stack Temperature Monitor Reference Guide

## Hardware Configuration

### Core Components
- M5Stack CoreS3 (Main board, no physical buttons)
- M5Stack NCIR Unit (MLX90614 temperature sensor)
- M5Stack Dual Button Unit (External buttons)
- M5Stack Key Unit (External select button)

### Pin Assignments
| Component | Pin | Function | Hardware Unit |
|-----------|-----|----------|---------------|
| Temperature Sensor (I2C) | 2, 1 | MLX90614 Communication | NCIR Unit |
| Button1 | 17 | Up/Previous/Toggle | Dual Button Unit |
| Button2 | 18 | Down/Next/Menu | Dual Button Unit |
| Key Unit | 8 | Select/Enter | Key Unit |

### Hardware Notes
- CoreS3 has no physical buttons, all input via external units
- NCIR Unit uses I2C for communication
- Dual Button Unit provides two independent buttons
- Key Unit provides single press functionality

## Sound Settings

### Volume Control
- Range: 25% to 100%
- Step Size: 5%
- Hardware Range: 64-255 (M5Stack volume)
- Default: 100%

### Sound Frequencies
```cpp
#define MENU_BEEP_FREQ 2000      // Navigation/Selection
#define CONFIRM_BEEP_FREQ 2500   // Confirmations/OK
#define ERROR_BEEP_FREQ 1000     // Errors/Warnings
```

### Beep Durations
```cpp
#define BEEP_DURATION 50         // Standard beep
#define LONG_BEEP_DURATION 100   // Extended beep
```

## Brightness Settings

### Brightness Levels
```cpp
static uint8_t brightness_values[4] = {25, 50, 75, 100};  // Percentage values
```

### Brightness Labels
```cpp
static const char* brightness_labels[4] = {"25%", "50%", "75%", "100%"};
```

## EEPROM Storage

### Addresses
```cpp
#define SETTINGS_VALID_ADDR 0     // Settings validation
#define TEMP_UNIT_ADDR 1         // Temperature unit
#define BRIGHTNESS_ADDR 2        // Screen brightness
#define GAUGE_VISIBLE_ADDR 3     // Gauge visibility
#define SOUND_SETTINGS_ADDR 4    // Sound settings
#define EMISSIVITY_ADDR 5        // Emissivity value (2 bytes)
```

### Format
- Settings Valid: 0x55 (magic number)
- Temperature Unit: 0 = Fahrenheit, 1 = Celsius
- Brightness: 0-3 (index into brightness_values)
- Gauge Visible: 0 = Hidden, 1 = Visible
- Sound Settings: 
  - Bit 7: Sound enabled flag (0x80)
  - Bits 0-6: Volume percentage (25-100)
- Emissivity: 16-bit integer (value * 100)

## Menu System

### Menu Items
```cpp
static const char* menu_items[] = {
    "Temperature Unit",
    "Brightness",
    "Emissivity",
    "Sound Settings",
    "Gauge Visibility",
    "Exit"
};
```

### Menu States
- `menu_active`: Main menu active
- `brightness_menu_active`: Brightness settings active
- `sound_menu_active`: Sound settings active
- `bar_active`: Emissivity bar active

## Debug Logging Conventions

### Function Entry/Exit
```cpp
Serial.println("Entering [function_name]...");
Serial.println("[function_name] completed");
```

### Value Updates
```cpp
Serial.print("[value_name] changed from ");
Serial.print(old_value);
Serial.print(" to ");
Serial.println(new_value);
```

### Error Conditions
```cpp
Serial.println("Error: [description]");
Serial.println("Warning: [description]");
```

### State Changes
```cpp
Serial.print("State changed to: [state_name]");
```

## User Interface Reference

### Main Screen
- Title: "Temperature Monitor"
- Temperature Value Label: "[value]°C" or "[value]°F"
- Status Label: "Status: [message]"
- Battery Status: "[percentage]%"
- Gauge Display (Optional)

### Settings Menu
- Title: "Settings"
- Menu Items:
  1. "Temperature Unit"
  2. "Brightness"
  3. "Emissivity"
  4. "Sound Settings"
  5. "Gauge Visibility"
  6. "Exit"

### Brightness Settings Screen
- Title: "Brightness"
- Current Value Label: "[value]%"
- Button Matrix Labels:
  - "25%"
  - "50%"
  - "75%"
  - "100%"
- OK Button Label: "OK"

### Sound Settings Screen
- Title: "Sound Settings"
- Volume Label: "Volume: [value]%"
- Enable Switch Label: "Sound: ON" / "Sound: OFF"
- Volume Slider Range: "25% - 100%"
- OK Button Label: "OK"

### Emissivity Settings Screen
- Title: "Emissivity"
- Current Value Label: "Emissivity: [value]"
- Slider Range: "0.65 - 1.00"
- OK Button Label: "OK"

### Restart Confirmation Dialog
- Title: "Restart Required"
- Message: "Settings changed. Device needs to restart."
- Countdown Text: "Restarting in [seconds]..."
- Button Labels:
  - "Cancel"
  - "Restart"

### Status Messages
- Initialization: "Initializing..."
- Sensor Error: "Sensor Error!"
- Normal Operation: "Ready"
- Low Battery: "Low Battery!"
- Settings Saved: "Settings Saved"
- Temperature Out of Range: "Out of Range!"

### Button Labels (Physical)
- Button1: "UP/PREV"
- Button2: "DOWN/NEXT"
- Key Unit: "SELECT"

### UI Element IDs
```cpp
// Main Screen
static lv_obj_t *temp_label = NULL;
static lv_obj_t *temp_value_label = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *meter = NULL;

// Settings Menu
static lv_obj_t *settings_panel = NULL;
static lv_obj_t *menu_buttons[6] = {NULL};

// Brightness Menu
static lv_obj_t *brightness_dialog = NULL;
static lv_obj_t *brightness_value_label = NULL;
static lv_obj_t *brightness_btnm = NULL;

// Sound Menu
static lv_obj_t *sound_dialog = NULL;
static lv_obj_t *volume_slider = NULL;
static lv_obj_t *sound_switch = NULL;

// Emissivity Menu
static lv_obj_t *emissivity_bar = NULL;
static lv_obj_t *emissivity_label = NULL;

// Restart Dialog
static lv_obj_t *restart_msgbox = NULL;
```

### Color Scheme
- Background: Dark theme
- Text: White
- Active Elements: Blue accent
- Warning Messages: Yellow
- Error Messages: Red
- Success Messages: Green

### Font Sizes
- Title Text: 20px
- Menu Items: 16px
- Value Labels: 18px
- Status Text: 14px
- Button Text: 16px

### Animation Durations
- Menu Transitions: 300ms
- Value Updates: 200ms
- Button Feedback: 100ms
- Slider Movement: 200ms

## UI Constants

### Dialog Sizes
- Brightness Dialog: 220x200
- Sound Dialog: 220x280
- Emissivity Dialog: 200x100

### Button Sizes
- OK Button: 100x40
- Menu Buttons: 180x40

### Screen Layout
- Title Position: Top, 10px from top
- Value Labels: 40px from top
- Controls: 80px from top
- OK Button: 20px from bottom

## Arduino Configuration

### Board Settings
- Board: "M5Stack CoreS3"
- Upload Speed: 921600
- USB CDC On Boot: Enabled
- USB Firmware MSC On Boot: Disabled
- USB DFU On Boot: Disabled
- Upload Mode: UART0 / Hardware CDC
- CPU Frequency: 240MHz (WiFi)
- Flash Mode: QIO 80MHz
- Partition Scheme: Default (16MB Flash)

### Libraries
- M5Unified (Latest version)
- M5GFX (Latest version)
- LVGL 8.4.0
- Wire.h (Built-in)
- FastLED (Latest version)
- EEPROM (Built-in)

### Serial Configuration
- Baud Rate: 115200
- Debug Output: Enabled
- Serial Monitor Settings:
  - Both NL & CR
  - 115200 baud
  - No line ending

### Memory Usage
- EEPROM: 7 bytes total
  - Settings validation: 1 byte
  - Temperature unit: 1 byte
  - Brightness: 1 byte
  - Gauge visibility: 1 byte
  - Sound settings: 1 byte
  - Emissivity: 2 bytes

## Temperature Settings

### Emissivity
- Range: 0.65 to 1.00
- Default: 0.98
- Storage: Multiplied by 100 for EEPROM

### Display
- Celsius: °C
- Fahrenheit: °F
- Update Interval: Every loop
