// Last updated 2/18/25 2:22 PM

# M5Stack Temperature Monitor

A non-contact temperature monitoring solution using M5Stack CoreS3 and MLX90614 infrared sensor.

## Features

- Non-contact temperature measurement using MLX90614 sensor
- Configurable temperature display (Fahrenheit/Celsius)
- Adjustable emissivity for different materials (0.65-1.00)
- Interactive UI with LVGL
- Visual temperature gauge
- Adjustable display brightness
- Sound feedback with volume control
- Persistent settings storage

## Hardware Requirements

- M5Stack CoreS3
- M5Stack Dual Button Unit
- M5Stack Key Unit
- M5Stack NCIR Unit (MLX90614)

## Software Requirements

- Arduino IDE
- Required Libraries:
  - M5Unified
  - Wire.h
  - FastLED
  - LVGL 8.4.0
  - Preferences
  - Adafruit_MLX90614

## Installation

1. Install Arduino IDE
2. Install required libraries through Arduino Library Manager
3. Connect M5Stack hardware components
4. Upload firmware to M5Stack CoreS3

## Usage

- Left Button: Enter/Exit Menu
- Right Button: Toggle Gauge Display
- In Menu:
  - Left Button: Navigate
  - Right Button: Select/Adjust

## Documentation

- See [REFERENCE.md](REFERENCE.md) for detailed documentation
- Check [CHANGELOG.md](CHANGELOG.md) for version history

## License

This project is licensed under the MIT License - see the LICENSE file for details.
