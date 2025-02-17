# Changelog

All notable changes to the M5Stack Temperature Monitoring Device project will be documented in this file.

## [Unreleased]

## [1.1.0] - 2025-02-17

### Added
- Enhanced debug logging system throughout the application
  - Function entry/exit logging
  - State change tracking
  - Value update monitoring
  - Error condition detection

### Changed
- Improved sound settings storage format
  - New format supports fine-grained volume control (5% steps)
  - Volume range from 25% to 100%
  - Automatic conversion of old settings format
- Enhanced brightness control interface
  - Added sound feedback for brightness adjustments
  - Improved button handling
  - Better visual feedback
- Refined menu navigation
  - Consistent sound feedback across all menus
  - Better state management
  - Improved error handling

### Fixed
- Sound settings persistence across device restarts
- Brightness control sound feedback
- Menu state handling edge cases
- Debug message consistency

## [1.0.0] - 2025-02-16

### Added
- Initial release with core temperature monitoring functionality
- LVGL-based graphical user interface
- Real-time temperature display with gauge visualization
- Temperature unit switching (Celsius/Fahrenheit)
- Screen brightness control
- Gauge visibility toggle
- Settings persistence using EEPROM
- Emissivity adjustment with touch slider
- Sound feedback system with configurable settings
  - Volume control (25%, 50%, 75%, 100%)
  - Enable/Disable toggle
  - Different tones for menu navigation and confirmations

### Hardware Support
- M5Stack CoreS3 main board
- M5Stack NCIR Unit (MLX90614 temperature sensor)
- M5Stack Dual Button Unit
- M5Stack Key Unit

### Technical Details
- I2C communication for temperature sensor (pins 2, 1)
- Button1 on pin 17: Menu Navigation/Temperature Unit Toggle
- Button2 on pin 18: Menu Navigation
- Key Unit on pin 8: Menu Selection/Gauge Visibility Toggle
- EEPROM storage for persistent settings
- LVGL 8.4.0 for graphics rendering

### UI Features
- Interactive settings menu with 6 items
- Touch-based emissivity adjustment
- Sound settings dialog with button matrix for volume control
- Restart confirmation for critical changes
- Real-time sensor configuration updates
- Smooth animations and transitions

### Settings Storage
- Temperature Unit preference
- Screen Brightness level
- Gauge Visibility state
- Sound Settings (Enable/Disable and Volume)
- Emissivity Value

## Notes
- Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/)
- Project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
