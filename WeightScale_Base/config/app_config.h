#pragma once

// Screen and touch configuration for CrowPanel Advanced 10.1" ESP32-P4
#define DISPLAY_WIDTH   1024
#define DISPLAY_HEIGHT   600

// HX711 pins (original scale wiring, adapt on platform hardware if needed)
#define HX711_DOUT_PIN  27
#define HX711_SCK_PIN   28

// GT911 touch I2C bus (CrowPanel standard, adjust if platform uses different pins)
#define GT911_I2C_PORT  I2C_NUM_1
#define GT911_SDA_PIN   GPIO_NUM_45
#define GT911_SCL_PIN   GPIO_NUM_46
#define GT911_ADDR      0x14

// scale stabilize / hold configuration defaults
#define SCALE_DEFAULT_HOLD_THRESHOLD 0.12f
#define SCALE_DEFAULT_HOLD_DURATION_MS 600

// LVGL buffer config - AGGRESSIVE: 10 lines (~20KB for 1 buffer)
#define LVGL_BUFFER_LINES 10

// Feature flags - all enabled for full functionality
#define ENABLE_WIFI_SERVICE 1
#define ENABLE_OTA_UPDATES 1
#define ENABLE_CLOUD_SYNC 1
#define ENABLE_TOUCH_INPUT 1
#define ENABLE_HX711_SCALE 1
#define ENABLE_CALIBRATION 1
#define SCALE_TASK_STACK_SIZE 12288
#define SCALE_TASK_PRIORITY   1
#define SCALE_TASK_CORE       1
