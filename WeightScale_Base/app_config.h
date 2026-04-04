#pragma once

/* ========================================================
   FEATURE CONFIGURATION - Compile-time control
   ======================================================== */

// Enable WiFi and networking services
#define ENABLE_WIFI_SERVICE        1  // Enabled for WiFi scan
#define ENABLE_OTA_UPDATES         0  // Disabled for now
#define ENABLE_CLOUD_SYNC          1  // Enabled

// Enable specific hardware features
#define ENABLE_TOUCH_INPUT         1
#define ENABLE_HX711_SCALE         1
#define ENABLE_CALIBRATION         1

// HX711 pins (scale wiring)
#define HX711_DOUT_PIN  27
#define HX711_SCK_PIN   28

// GT911 touch I2C bus
#define GT911_I2C_PORT  I2C_NUM_1
#define GT911_SDA_PIN   GPIO_NUM_45
#define GT911_SCL_PIN   GPIO_NUM_46
#define GT911_ADDR      0x14

// scale stabilize / hold configuration defaults
#define SCALE_DEFAULT_HOLD_THRESHOLD 0.12f
#define SCALE_DEFAULT_HOLD_DURATION_MS 600

// LVGL config - AGGRESSIVE buffer reduction for SRAM savings
// 10 lines × 1024 width × 2 bytes = 20KB per buffer
// 1 buffer × 20KB = 20KB total (still reasonable)
#define LVGL_BUFFER_LINES 10

// Display resolution
#define DISPLAY_WIDTH              1024
#define DISPLAY_HEIGHT             600

// Stack configuration - AGGRESSIVE reduction
// Reduced from 8192 to 4096 bytes (still sufficient for WiFi/OTA tasks)
#define SCALE_TASK_STACK_SIZE 4096
#define SCALE_TASK_PRIORITY   1
#define SCALE_TASK_CORE       1

// WiFi service depends on other modules
#if ENABLE_WIFI_SERVICE
  // WiFi pulls in: WiFi.h, WiFiClientSecure.h, HTTPClient.h
  // Only include if ENABLE_WIFI_SERVICE is 1
  #define WIFI_AVAILABLE 1
#else
  #define WIFI_AVAILABLE 0
#endif

#if ENABLE_OTA_UPDATES
  #define OTA_AVAILABLE 1
#else
  #define OTA_AVAILABLE 0
#endif

#if ENABLE_CLOUD_SYNC
  #define SYNC_AVAILABLE 1
#else
  #define SYNC_AVAILABLE 0
#endif
