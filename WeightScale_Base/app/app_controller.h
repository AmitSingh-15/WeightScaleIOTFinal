#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================
   APP CONTROLLER - Central orchestrator between UI and Services
   
   Architecture:
   ┌─────────────────┐
   │   UI Layer      │ (home_screen.cpp, settings_screen.cpp, etc)
   │                 │ NO direct service includes!
   └────────┬────────┘
            │ UI events (int event_id)
            │ UI callbacks (void (*)(const char*))
            ↓
   ┌─────────────────────────────────────────────┐
   │   APP CONTROLLER (THIS MODULE)              │ ← Decoupling layer
   │                                             │
   │  Receives: UI events                        │
   │  Calls: Services (conditionally compiled)   │
   │  Updates: UI via callbacks                  │
   └─────────────────────────────────────────────┘
            ↓
   ┌──────────────────────────────────┐
   │   SERVICE LAYER                  │
   │  - wifi_service (optional)       │
   │  - scale_service_v2              │
   │  - storage_service               │
   │  - sync_service (optional)       │
   │  - invoice_service               │
   └──────────────────────────────────┘
   
   ======================================================== */

/* ========================================================
   UI CALLBACKS - UI registers these to receive updates
   ======================================================== */

typedef void (*ui_sync_status_cb_t)(const char *status);      // "Online" / "Offline"
typedef void (*ui_weight_update_cb_t)(float weight);          // Scale weight changed
typedef void (*ui_calibration_cb_t)(const char *message);     // Cal status messages
typedef void (*ui_invoice_cb_t)(const char *invoice_number);  // Invoice generated
typedef void (*ui_device_name_cb_t)(const char *name);        // Device name updated

/* ========================================================
   INITIALIZATION & LIFECYCLE
   ======================================================== */

/**
 * Initialize the app controller
 * Calls service init functions based on app_config.h flags
 * Must be called ONCE at startup
 */
void app_controller_init(void);

/**
 * Main loop - call from loop()
 * Processes service updates, wifi state changes, etc.
 */
void app_controller_loop(void);

/* ========================================================
   REGISTER UI CALLBACKS
   
   UI layer calls these to register callback functions.
   App controller calls these when state changes.
   ======================================================== */

void app_controller_register_sync_status_callback(ui_sync_status_cb_t cb);
void app_controller_register_weight_update_callback(ui_weight_update_cb_t cb);
void app_controller_register_calibration_callback(ui_calibration_cb_t cb);
void app_controller_register_invoice_callback(ui_invoice_cb_t cb);
void app_controller_register_device_name_callback(ui_device_name_cb_t cb);

/* ========================================================
   UI EVENT HANDLING
   
   UI layer calls this when user clicks a button or interacts.
   Parameter: event_id from ui_events.h
   ======================================================== */

void app_controller_handle_ui_event(int event_id);

/* ========================================================
   SCALE SERVICE API (proxies to scale_service_v2)
   ======================================================== */

float app_controller_get_weight(void);
void app_controller_tare_scale(void);
void app_controller_calibrate_scale(float known_weight);

/* ========================================================
   INVOICE SERVICE API
   ======================================================== */

void app_controller_add_invoice_item(const char *item_code);
void app_controller_remove_invoice_item(uint8_t index);
void app_controller_clear_invoice(void);

/* ========================================================
   SETTINGS API
   ======================================================== */

const char *app_controller_get_device_name(void);
void app_controller_set_device_name(const char *name);

#if ENABLE_WIFI_SERVICE
/**
 * WiFi is optional - only callable if ENABLE_WIFI_SERVICE is 1
 */
void app_controller_start_wifi_scan(void);
void app_controller_connect_wifi(const char *ssid, const char *password);
const char *app_controller_get_wifi_status(void);
#endif

/* ========================================================
   INTERNAL STATE QUERIES (for diagnostics)
   ======================================================== */

bool app_controller_is_wifi_enabled(void);
bool app_controller_is_ota_enabled(void);
bool app_controller_is_sync_enabled(void);

#ifdef __cplusplus
}
#endif
