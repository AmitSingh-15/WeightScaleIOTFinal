#include "app/app_controller.h"
#include "ui_events.h"
#include "devlog.h"

/* ========================================================
   SERVICE INCLUDES - Conditionally compiled based on app_config.h
   ======================================================== */

// Always available
#include "scale_service_v2.h"
#include "invoice_session_service.h"
#include "storage_service.h"
#include "ui_styles.h"

// Conditionally available based on app_config.h
#if ENABLE_WIFI_SERVICE
  #include "wifi_service.h"
  #include "ota_service.h"
#endif

#if ENABLE_CLOUD_SYNC
  #include "sync_service.h"
#endif

/* ========================================================
   REGISTERED CALLBACKS - UI registers these
   ======================================================== */

static ui_sync_status_cb_t g_sync_status_cb = NULL;
static ui_weight_update_cb_t g_weight_update_cb = NULL;
static ui_calibration_cb_t g_calibration_cb = NULL;
static ui_invoice_cb_t g_invoice_cb = NULL;
static ui_device_name_cb_t g_device_name_cb = NULL;

/* ========================================================
   INTERNAL STATE
   ======================================================== */

static bool g_initialized = false;
static float g_last_weight = 0.0f;

#if ENABLE_WIFI_SERVICE
static bool g_wifi_connected = false;
#endif

/* ========================================================
   INITIALIZATION
   ======================================================== */

void app_controller_init(void)
{
    if (g_initialized) {
        devlog_printf("[CTRL] Already initialized");
        return;
    }

    devlog_printf("[CTRL] ✓ Initializing app controller");

    // Always init these services
    scale_service_init();
    devlog_printf("[CTRL] ✓ Scale service initialized");

    invoice_session_init();
    devlog_printf("[CTRL] ✓ Invoice service initialized");

#if ENABLE_WIFI_SERVICE
    wifi_service_init();
    // Register our internal WiFi callback
    wifi_service_register_state_callback([](wifi_state_t state) {
        g_wifi_connected = (state == WIFI_CONNECTED);
        if (g_sync_status_cb) {
            g_sync_status_cb(g_wifi_connected ? "Online" : "Offline");
        }
        devlog_printf("[CTRL] WiFi state: %s", g_wifi_connected ? "ONLINE" : "OFFLINE");
    });
    devlog_printf("[CTRL] ✓ WiFi service initialized");
#else
    devlog_printf("[CTRL] ⊘ WiFi service DISABLED (app_config.h)");
#endif

#if ENABLE_CLOUD_SYNC
    sync_service_init();
    devlog_printf("[CTRL] ✓ Sync service initialized");
#endif

    devlog_printf("[CTRL] ✓ All services initialized");
    g_initialized = true;
}

/* ========================================================
   MAIN LOOP - call from loop()
   ======================================================== */

void app_controller_loop(void)
{
    if (!g_initialized) return;

#if ENABLE_WIFI_SERVICE
    wifi_service_loop();
#endif

#if ENABLE_CLOUD_SYNC
    sync_service_loop();
#endif

    // Check for weight updates from scale
    float current_weight = scale_service_get_weight();
    if (current_weight != g_last_weight) {
        g_last_weight = current_weight;
        if (g_weight_update_cb) {
            g_weight_update_cb(current_weight);
        }
    }
}

/* ========================================================
   REGISTER UI CALLBACKS
   ======================================================== */

void app_controller_register_sync_status_callback(ui_sync_status_cb_t cb)
{
    g_sync_status_cb = cb;
    devlog_printf("[CTRL] Sync status callback registered");
}

void app_controller_register_weight_update_callback(ui_weight_update_cb_t cb)
{
    g_weight_update_cb = cb;
    devlog_printf("[CTRL] Weight update callback registered");
}

void app_controller_register_calibration_callback(ui_calibration_cb_t cb)
{
    g_calibration_cb = cb;
    devlog_printf("[CTRL] Calibration callback registered");
}

void app_controller_register_invoice_callback(ui_invoice_cb_t cb)
{
    g_invoice_cb = cb;
    devlog_printf("[CTRL] Invoice callback registered");
}

void app_controller_register_device_name_callback(ui_device_name_cb_t cb)
{
    g_device_name_cb = cb;
    devlog_printf("[CTRL] Device name callback registered");
}

/* ========================================================
   UI EVENT HANDLING - Entry point from UI layer
   ======================================================== */

void app_controller_handle_ui_event(int event_id)
{
    devlog_printf("[CTRL] UI Event: %d", event_id);

    switch (event_id) {
        case UI_EVT_SETTINGS:
            devlog_printf("[CTRL] → Open settings");
            break;

        case UI_EVT_HISTORY:
            devlog_printf("[CTRL] → Open history");
            break;

        case UI_EVT_RESET:
            devlog_printf("[CTRL] → Reset current invoice");
            invoice_session_clear();
            if (g_invoice_cb) {
                g_invoice_cb("");
            }
            break;

        case UI_EVT_RESET_ALL:
            devlog_printf("[CTRL] → Reset ALL invoices");
            invoice_session_clear();
            storage_clear_all_records();
            if (g_invoice_cb) {
                g_invoice_cb("");
            }
            break;

        case UI_EVT_CALIBRATE:
            devlog_printf("[CTRL] → Start calibration");
            if (g_calibration_cb) {
                g_calibration_cb("Calibration started...");
            }
            break;

        case UI_EVT_SAVE:
            devlog_printf("[CTRL] → Save invoice");
            // Implement persistence
            break;

        default:
            // Check if it's a "remove item" event
            if (event_id >= UI_EVT_REMOVE_ITEM_BASE) {
                uint8_t item_idx = event_id - UI_EVT_REMOVE_ITEM_BASE;
                devlog_printf("[CTRL] → Remove invoice item %d", item_idx);
                invoice_session_remove(item_idx);
            }
            break;
    }
}

/* ========================================================
   SCALE SERVICE PROXIES
   ======================================================== */

float app_controller_get_weight(void)
{
    return scale_service_get_weight();
}

void app_controller_tare_scale(void)
{
    devlog_printf("[CTRL] Tare scale");
    scale_service_tare();
}

void app_controller_calibrate_scale(float known_weight)
{
    devlog_printf("[CTRL] Calibrate scale with %.2f", known_weight);
    // TODO: Implement calibration flow
}

/* ========================================================
   INVOICE SERVICE PROXIES
   ======================================================== */

void app_controller_add_invoice_item(const char *item_code)
{
    devlog_printf("[CTRL] Add invoice item: %s", item_code);
    // TODO: Implement
}

void app_controller_remove_invoice_item(uint8_t index)
{
    devlog_printf("[CTRL] Remove invoice item: %d", index);
    invoice_session_remove(index);
}

void app_controller_clear_invoice(void)
{
    devlog_printf("[CTRL] Clear invoice");
    invoice_session_clear();
}

/* ========================================================
   SETTINGS PROXIES
   ======================================================== */

const char *app_controller_get_device_name(void)
{
    static char name_buffer[64] = {0};
    storage_load_device_name(name_buffer, sizeof(name_buffer));
    return name_buffer;
}

void app_controller_set_device_name(const char *name)
{
    devlog_printf("[CTRL] Set device name: %s", name);
    storage_save_device_name(name);
    if (g_device_name_cb) {
        g_device_name_cb(name);
    }
}

/* ========================================================
   WiFi SERVICE PROXIES (Optional)
   ======================================================== */

#if ENABLE_WIFI_SERVICE

void app_controller_start_wifi_scan(void)
{
    devlog_printf("[CTRL] Start WiFi scan");
    wifi_service_start_scan();
}

void app_controller_connect_wifi(const char *ssid, const char *password)
{
    devlog_printf("[CTRL] Connect WiFi: %s", ssid);
    wifi_service_connect(ssid, password);
}

const char *app_controller_get_wifi_status(void)
{
    return g_wifi_connected ? "Online" : "Offline";
}

#endif // ENABLE_WIFI_SERVICE

/* ========================================================
   DIAGNOSTIC QUERIES
   ======================================================== */

bool app_controller_is_wifi_enabled(void)
{
#if ENABLE_WIFI_SERVICE
    return true;
#else
    return false;
#endif
}

bool app_controller_is_ota_enabled(void)
{
#if ENABLE_OTA_UPDATES
    return true;
#else
    return false;
#endif
}

bool app_controller_is_sync_enabled(void)
{
#if ENABLE_CLOUD_SYNC
    return true;
#else
    return false;
#endif
}
