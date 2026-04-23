/**
 * ============================================================================
 * ESP32-P4 Weight Scale with LVGL Display
 * ============================================================================
 * 
 * Architecture:
 * - ESP32_Display_Panel (official vendor library for display panel)
 * - LVGL 8 via lvgl_port (official LVGL port from vendor)
 * - HX711 for weight sensing
 * 
 * Correct Flow:
 * 1. Initialize LDO power regulators (MIPI D-PHY, I2C/Touch)
 * 2. Create Board instance (handles LCD + Touch internally)
 * 3. Initialize LVGL via lvgl_port_init()
 * 4. Create LVGL UI (with proper locking)
 * 5. Loop: Read weight every ~1 second, update LVGL label with lock
 * 
 * CRITICAL:
 * - No Wire.h (I2C conflict)
 * - No LovyanGFX
 * - No custom LVGL flush callbacks
 * - No manual buffer allocation
 * - All LVGL updates inside lvgl_port_lock/unlock
 */

/* ============================================================================
   INCLUDES - CORRECT ORDER
   ============================================================================ */

// IMPORTANT: Arduino.h must come EARLY for Serial and other Arduino APIs
#include <Arduino.h>

// Standard C libraries
#include <string.h>

// ESP-IDF core
#include <esp_log.h>
#include <esp_err.h>
#include <esp_ldo_regulator.h>      // ESP32-P4 LDO management

// Board configuration (defines GPIO pins, etc.)
#include "config/app_config.h"

// ESP32_Display_Panel official headers
#include "esp_panel_drivers_conf.h"
#include "esp_panel_board_custom_conf.h"
#include "ESP_Panel_Library.h"

// LVGL and port
#include <lvgl.h>
#include <lvgl_v8_port.h>

// HX711 handled by scale_service_v2 (FreeRTOS task)
// No direct HX711 include needed here

// Application UI and services (conditional on feature flags)
#include "ui_styles.h"
#include "home_screen.h"
#include "invoice_service.h"
#include "app/app_controller.h"

// Namespaces: MUST come AFTER all includes
using namespace esp_panel::drivers;
using namespace esp_panel::board;

/* ============================================================================
   CONSTANTS & MACROS
   ============================================================================ */

#define MAIN_LOG_TAG "WeightScale"

// Logging helpers
#define MAIN_INFO(fmt, ...)  do { \
    Serial.print("[INFO] "); \
    Serial.printf(fmt "\r\n", ##__VA_ARGS__); \
} while(0)

#define MAIN_ERROR(fmt, ...) do { \
    Serial.print("[ERROR] "); \
    Serial.printf(fmt "\r\n", ##__VA_ARGS__); \
} while(0)

// LVGL colors
#define LV_COLOR_WHITE      lv_color_make(0xFF, 0xFF, 0xFF)
#define LV_COLOR_BLACK      lv_color_make(0x00, 0x00, 0x00)
#define LV_COLOR_RED        lv_color_make(0xFF, 0x00, 0x00)

/* ============================================================================
   GLOBAL STATE
   ============================================================================ */

// Board and display management
static Board *g_board = nullptr;

// LVGL UI object for weight display
static lv_obj_t *g_weight_label = nullptr;

// Weight reading state
static float g_current_weight_kg = 0.0f;

/* ============================================================================
   FUNCTION DECLARATIONS
   ============================================================================ */

static void ui_event_callback(int event_id);
static void handle_weight_update(float weight_kg);
static void handle_sync_status(const char *status);

/* ============================================================================
   SETUP FUNCTION (Arduino Entry Point)
   ============================================================================ */

void setup() {
    // Serial initialization
    Serial.begin(115200);
    delay(500);
    
    MAIN_INFO("╔════════════════════════════════════════════════════════════╗");
    MAIN_INFO("║   🚀 ESP32-P4 Weight Scale - Boot Sequence              ║");
    MAIN_INFO("╚════════════════════════════════════════════════════════════╝\n");

    /* ──────────────────────────────────────────────────────────────────────
       STEP 1: Power Management - LDO Configuration
       ────────────────────────────────────────────────────────────────────── */
    MAIN_INFO("Step 1: Initializing Power Regulators (LDO)...");
    
    esp_err_t err = ESP_OK;

    // LDO3: 2.5V for MIPI D-PHY (display interface)
    esp_ldo_channel_handle_t ldo3_handle = nullptr;
    esp_ldo_channel_config_t ldo3_cfg = {
        .chan_id = 3,
        .voltage_mv = 2500,  // 2.5V
    };
    
    err = esp_ldo_acquire_channel(&ldo3_cfg, &ldo3_handle);
    if (err != ESP_OK) {
        MAIN_ERROR("LDO3 failed: %s", esp_err_to_name(err));
        delay(3000); ESP.restart();
    }
    MAIN_INFO("  ✓ LDO3 enabled (2.5V for MIPI D-PHY)");

    // LDO4: 3.3V for I2C pull-up (touch controller)
    esp_ldo_channel_handle_t ldo4_handle = nullptr;
    esp_ldo_channel_config_t ldo4_cfg = {
        .chan_id = 4,
        .voltage_mv = 3300,  // 3.3V
    };
    
    err = esp_ldo_acquire_channel(&ldo4_cfg, &ldo4_handle);
    if (err != ESP_OK) {
        MAIN_ERROR("LDO4 failed: %s", esp_err_to_name(err));
        delay(3000); ESP.restart();
    }
    MAIN_INFO("  ✓ LDO4 enabled (3.3V for I2C/Touch)\n");

    /* ──────────────────────────────────────────────────────────────────────
       STEP 2: Display Panel Initialization
       ────────────────────────────────────────────────────────────────────── */
    MAIN_INFO("Step 2: Initializing Display Panel (Board)...");
    
    // Create Board instance (handles LCD, Touch, Backlight internally)
    g_board = new Board();
    if (!g_board) {
        MAIN_ERROR("Failed to create Board instance");
        delay(3000); ESP.restart();
    }

    // Initialize the board hardware (bus setup, GPIO config)
    if (!g_board->init()) {
        MAIN_ERROR("Board::init() failed");
        delay(3000); ESP.restart();
    }
    MAIN_INFO("  ✓ Board hardware initialized");

    // Configure LCD for triple-buffered anti-tear (MUST be after init, before begin)
    {
        auto lcd_cfg = g_board->getLCD();
        if (lcd_cfg) {
            lcd_cfg->configFrameBufferNumber(3);
            MAIN_INFO("  ✓ LCD configured for 3 frame buffers (anti-flicker)");
        }
    }

    // Start the board (enable LCD output, touch input, backlight)
    if (!g_board->begin()) {
        MAIN_ERROR("Board::begin() failed");
        delay(3000); ESP.restart();
    }
    MAIN_INFO("  ✓ Board started (display active)\n");

    /* ──────────────────────────────────────────────────────────────────────
       STEP 3: LVGL Framework Initialization
       ────────────────────────────────────────────────────────────────────── */
    MAIN_INFO("Step 3: Initializing LVGL Graphics Framework...");
    
    auto lcd = g_board->getLCD();
    auto touch = g_board->getTouch();
    
    if (!lcd) {
        MAIN_ERROR("No LCD device from Board");
        delay(3000); ESP.restart();
    }
    MAIN_INFO("  ✓ LCD device obtained");
    
    if (touch) {
        MAIN_INFO("  ✓ Touch device obtained");
    } else {
        MAIN_INFO("  ⚠ Touch device NOT available (non-critical)");
    }

    // Initialize official LVGL port (handles rendering task, buffers, etc.)
    lvgl_port_init(lcd, touch);
    MAIN_INFO("  ✓ LVGL framework initialized\n");

    /* ──────────────────────────────────────────────────────────────────────
       STEP 4: HX711 Weight Sensor Initialization
       ────────────────────────────────────────────────────────────────────── */
    MAIN_INFO("Step 4: Initializing HX711 Weight Sensor...");
    // HX711 is initialized by scale_service_v2 in its FreeRTOS task
    // No duplicate init here — avoids GPIO 27/28 contention
    MAIN_INFO("  ✓ HX711 will init via scale_service task\n");

    /* ──────────────────────────────────────────────────────────────────────
       STEP 5: LVGL UI Creation
       ────────────────────────────────────────────────────────────────────── */
    MAIN_INFO("Step 5: Initializing UI and Application Services...");
    
    // Initialize styles (must come before screen creation)
    ui_styles_init();
    
    // Create the home screen on a fresh LVGL screen
    lv_obj_t *home_scr = lv_obj_create(NULL);
    home_screen_create(home_scr);
    home_screen_register_callback(ui_event_callback);
    lv_scr_load(home_scr);
    
    Serial.println("[SETUP] Home screen created successfully");
    Serial.flush();
    
#if ENABLE_WIFI_SERVICE
    // Initialize application controller (handles WiFi, OTA, sync, etc.)
    app_controller_init();
    
    // Register callbacks for app controller
    app_controller_register_weight_update_callback(handle_weight_update);
    app_controller_register_sync_status_callback(handle_sync_status);
    
    // Set initial invoice ID on home screen
    home_screen_set_invoice(invoice_service_current_id());
#else
    // Minimal mode without WiFi/services
    MAIN_INFO("  ⚠ WiFi and app controller disabled");
#endif
    
    MAIN_INFO("  ✓ UI initialized with all application services\n");

    /* ──────────────────────────────────────────────────────────────────────
       BOOT COMPLETE
       ────────────────────────────────────────────────────────────────────── */
    MAIN_INFO("╔════════════════════════════════════════════════════════════╗");
    MAIN_INFO("║   ✅ BOOT COMPLETE - System Ready                        ║");
    MAIN_INFO("╚════════════════════════════════════════════════════════════╝\n");
    
    MAIN_INFO("Display: ESP32_Display_Panel (EK79007 LCD + GT911 Touch)");
    MAIN_INFO("Graphics: LVGL 8 (vendor port with rendering task)");
    MAIN_INFO("Sensor: HX711 Load Cell (GPIO %d/%d)", HX711_DOUT_PIN, HX711_SCK_PIN);
    MAIN_INFO("Status: Weight reading updates every 1 second\n");
}

/* ============================================================================
   LOOP FUNCTION (Arduino Main Loop)
   ============================================================================ */

void loop() {
    // Minimal loop: all services coordinated by app_controller
    // LVGL rendering runs autonomously in background task
    
#if ENABLE_WIFI_SERVICE
    // Let app controller handle all service coordination
    // (weight reading, WiFi, OTA, sync, etc.)
    app_controller_loop();
#else
    // Minimal mode: just yield to LVGL task
    // Weight reading can still happen via HX711 if needed
#endif
    
    delay(50);  // Yield to LVGL rendering task
}

/* ============================================================================
   HX711 WEIGHT SENSOR FUNCTIONS
   ============================================================================ */

/* HX711 is fully managed by scale_service_v2 (FreeRTOS task on Core 1) */

/* ============================================================================
   EVENT & CALLBACK FUNCTIONS
   ============================================================================ */

/**
 * @brief UI event callback - handles button presses, screen switches, etc.
 * @param event_id Event identifier from UI layer
 */
static void ui_event_callback(int event_id) {
    MAIN_INFO("UI Event: %d", event_id);
    // ✅ Forward all UI events to the application controller
    app_controller_handle_ui_event(event_id);
}

/**
 * @brief Weight update callback - called when weight reading changes
 * @param weight_kg Current weight in kilograms
 */
static void handle_weight_update(float weight_kg) {
    g_current_weight_kg = weight_kg;
    home_screen_set_weight(weight_kg);
}

/**
 * @brief Sync status callback - called when WiFi/sync status changes
 * @param status Status string ("Online", "Offline", "Syncing", etc.)
 */
static void handle_sync_status(const char *status) {
    MAIN_INFO("Sync Status: %s", status);
    // UI can display sync status to user
}

/* ============================================================================
   END OF FILE
   ============================================================================ */
