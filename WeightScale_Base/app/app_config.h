#pragma once

/* ========================================================
   FEATURE CONFIGURATION - Compile-time control
   ======================================================== */

// Enable WiFi and networking services
// Disable to reduce binary size and avoid linker issues
#define ENABLE_WIFI_SERVICE        1
#define ENABLE_OTA_UPDATES         1
#define ENABLE_CLOUD_SYNC          1

// Enable specific hardware features
#define ENABLE_TOUCH_INPUT         1
#define ENABLE_HX711_SCALE         1
#define ENABLE_CALIBRATION         1

// UI Configuration
#define DISPLAY_WIDTH              1024
#define DISPLAY_HEIGHT             600

// Memory optimization
#define LVGL_BUFFER_SIZE_LINES     96   // ~196KB per buffer (LV_COLOR_16BIT)
#define STACK_PROTECTION_ENABLED   1

/* ========================================================
   DERIVED CONFIGURATION
   ======================================================== */

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
