# M5Stack Temperature Monitoring Device

A sophisticated temperature monitoring system built on the M5Stack CoreS3 platform, featuring an interactive LVGL interface, customizable sensor settings, and real-time temperature display.

## Features

- Real-time temperature monitoring using MLX90614 infrared sensor
- Beautiful LVGL-based graphical interface
- Interactive touch controls
- Configurable temperature units (Celsius/Fahrenheit)
- Adjustable screen brightness
- Customizable gauge display
- Sound feedback system with adjustable volume
- Persistent settings storage

## Hardware Requirements

### Core Components
- M5Stack CoreS3 (main board)
- M5Stack NCIR Unit (MLX90614 temperature sensor)
- M5Stack Dual Button Unit
- M5Stack Key Unit

### Pin Configuration
| Component | Pin(s) | Function |
|-----------|--------|-----------|
| Temperature Sensor | 2, 1 | I2C Communication |
| Button1 | 17 | Menu Navigation/Temperature Toggle |
| Button2 | 18 | Menu Navigation |
| Key Unit | 8 | Menu Selection/Gauge Toggle |

## Software Requirements

### Dependencies
- Arduino IDE
- M5Unified library
- M5GFX library
- LVGL 8.4.0
- Wire.h (for I2C)
- FastLED (for RGB LED control)
- EEPROM (for settings storage)

### Installation

1. Install Arduino IDE
2. Install required libraries through Arduino Library Manager:
   - M5Unified
   - M5GFX
   - LVGL
   - FastLED
3. Clone this repository
4. Open `lvglex.ino` in Arduino IDE
5. Select "M5Stack CoreS3" as your board
6. Upload the code

## Usage

### Main Interface
- Temperature is displayed both numerically and on a gauge
- Current sensor settings and status are shown on screen
- Touch interface for menu navigation and settings adjustment

### Controls
- **Button1**: Navigate up in menus / Toggle temperature unit
- **Button2**: Navigate down in menus
- **Key Unit**: Select menu items / Toggle gauge visibility
- **Touch Screen**: Interactive settings adjustment

### Settings Menu
1. **Temperature Unit**: Switch between Celsius and Fahrenheit
2. **Brightness**: Adjust screen brightness
3. **Gauge Visibility**: Toggle temperature gauge display
4. **Sound Settings**: 
   - Enable/disable sound feedback
   - Adjust volume (25%, 50%, 75%, 100%)
5. **Emissivity**: Adjust sensor emissivity value
6. **Reset**: Reset to default settings

### Sound Feedback
- Different tones for menu navigation
- Confirmation sounds for selections
- Adjustable volume levels
- Can be disabled completely

## Project Structure

- `lvglex.ino`: Main application code
- `lv_conf.h`: LVGL configuration
- `CHANGELOG.md`: Project change history
- `README.md`: This documentation

## Settings Storage

The following settings are automatically saved to EEPROM:
- Temperature unit preference
- Screen brightness level
- Gauge visibility state
- Sound settings (enabled/disabled and volume)
- Emissivity value

## Contributing

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push to the branch
5. Create a Pull Request

## Development Notes

### Adding New Features
- Use LVGL 8.4.0 widgets for UI elements
- Follow existing code structure for consistency
- Update EEPROM handling if adding new settings
- Add appropriate sound feedback for new interactions

### Debugging Tips
- Enable Serial debugging for development
- Use M5.Display for debug information
- Check EEPROM addresses for settings conflicts
- Verify I2C communication if sensor issues occur

## License

This project is open source and available under the MIT License.

## Acknowledgments

- M5Stack for the excellent hardware platform
- LVGL team for the graphics library
- Contributors and testers

## Version History

See [CHANGELOG.md](CHANGELOG.md) for a detailed history of changes.

## Support

For issues, questions, or contributions, please:
1. Check existing issues in the repository
2. Create a new issue with a detailed description
3. Include your hardware configuration and Arduino IDE version
