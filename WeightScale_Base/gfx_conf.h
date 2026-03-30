#pragma once

#include <Arduino.h>
#include <lvgl.h>

/* Screen resolution for ESP32P4 CrowPanel Advanced display */
#define screenWidth   1024
#define screenHeight  600

/* Display uses MIPI-DSI interface handled by ESP32_Display_Panel library
 * Touch uses GT911 controller handled by ESP32_Display_Panel library
 * Graphics rendered by LVGL 8.3.11
 * See lvgl_v8_port.h for LVGL porting layer configuration
 */
