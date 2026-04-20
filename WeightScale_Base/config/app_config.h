#pragma once

// Screen and touch configuration for CrowPanel Advanced 10.1" ESP32-P4
#define DISPLAY_WIDTH   1024
#define DISPLAY_HEIGHT   600

// HX711 pins (original scale wiring, adapt on platform hardware if needed)
#define HX711_DOUT_PIN  48
#define HX711_SCK_PIN   47

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

// WiFi/OTA task configuration
#define WIFI_OTA_TASK_STACK_SIZE   8192    // 8KB — TLS + OTA needs headroom
#define WIFI_OTA_TASK_PRIORITY     3       // Above scale(1), below LVGL
#define WIFI_OTA_TASK_CORE         1

// OTA timing
#define OTA_CMD_COOLDOWN_MS        30000   // 30s between OTA check commands
#define OTA_WIFI_SETTLE_MS         10000   // WiFi must be connected this long before OTA
#define OTA_CHECK_INTERVAL_MS      30000   // 30s cooldown between version checks
#define OTA_CHUNK_SIZE             512     // SDIO-friendly chunk size
#define OTA_CHUNK_DELAY_MS         20      // SDIO breathing per chunk
#define OTA_STALL_TIMEOUT_MS       15000   // 15s zero-progress = stall
#define OTA_MAX_RETRIES            3       // retry download up to 3x
#define OTA_HTTP_TIMEOUT_MS        10000   // 10s HTTP timeout

// WiFi scan
#define WIFI_SCAN_COOLDOWN_MS      8000    // 8s between scan starts
#define WIFI_SCAN_MAX_APS          20

// Sync timing
#define SYNC_INTERVAL_MS           30000   // 30s between sync attempts
#define SYNC_HTTPS_COOLDOWN_MS     10000   // 10s after WiFi connect before HTTPS
#define SYNC_TRANSPORT_COOLDOWN_MS 60000   // 60s cooldown after transport failure

// Weight stability
#define WEIGHT_NOISE_FLOOR_KG      0.3f    // Below this = zero (was 0.5)
#define WEIGHT_STABILITY_KG        0.2f    // Jitter tolerance for stable detection (was 0.5 — too loose)
#define WEIGHT_STABILITY_MS        800     // ms weight must be stable to auto-add (was 400 — too fast)
#define WEIGHT_SNAP_STEP_KG        0.1f    // Snap resolution (was 0.5 — caused same weight to show different)

// HX711 health
#define HX711_NO_DATA_TIMEOUT_MS   3000    // 3s no data = no sensor
#define HX711_WARMUP_MS            5000    // 5s warmup after init
#define HX711_STUCK_TIMEOUT_MS     5000    // 5s same raw value = stuck

// Heap monitoring
#define HEAP_CHECK_INTERVAL_MS     60000   // 60s between heap checks
#define HEAP_LOW_THRESHOLD_BYTES   20480   // 20KB low-memory warning

// Crash loop
#define CRASH_LOOP_MAX_BOOTS       3       // >3 crashes = safe mode
#define CRASH_COUNTER_CLEAR_MS     30000   // Clear crash counter after 30s stable

// WiFi stability
#define WIFI_STABLE_AFTER_MS       15000   // 15s connected = STABLE
