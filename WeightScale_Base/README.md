# WeightScale IOTA

A weight scale application with IOTA cryptocurrency integration for invoice payments.

## Hardware Requirements
- ESP32-based board (e.g., ESP32-DevKitC)
- CrowPanel 7.0" TFT with ILI9486 display and XPT2046 touch controller
- HX711 load cell amplifier
- Load cell (weight sensor)

## Software Dependencies
- Arduino IDE with ESP32 board support
- Required libraries:
  - LovyanGFX
  - LVGL
  - WiFi
  - HTTPClient
  - Preferences
  - IOTA SDK (to be added)

## Pin Configuration
- TFT CS: 15
- TFT DC: 2
- TFT RST: -1
- TFT BL: 21
- Touch INT: 36
- Touch SDA: 33
- Touch SCL: 32
- SPI: SCK=18, MOSI=23, MISO=19

## Building
1. Install Arduino IDE and ESP32 board support
2. Install required libraries via Library Manager
3. Open WeightScale_Base.ino
4. Select ESP32 Dev Module board
5. Upload to device

## Features
- Weight measurement with load cell (800x480 touchscreen UI)
- Invoice generation and management
- IOTA payment integration
- Touch-based UI
- WiFi connectivity
- OTA updates
- Offline operation with sync

## Development Status
- [x] Basic hardware integration
- [x] LVGL UI framework
- [x] Invoice service
- [x] Storage service
- [ ] Weight sensor integration
- [ ] IOTA transaction logic
- [ ] Complete UI screens
- [ ] Network sync