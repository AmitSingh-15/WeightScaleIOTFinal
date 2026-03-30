#pragma once
#include <Arduino.h>

#ifdef LV_VERSION_MAJOR
#include <lvgl.h>
#endif

typedef enum {
    WIFI_DISCONNECTED,
    WIFI_CONNECTING,
    WIFI_CONNECTED
} wifi_state_t;

void wifi_service_init(void);
void wifi_service_loop(void);
wifi_state_t wifi_service_state(void);

// Register a callback to be notified when WiFi state changes
void wifi_service_register_state_callback(void (*cb)(wifi_state_t));

void wifi_service_start_scan(void);
uint8_t wifi_service_get_ap_count(void);
String wifi_service_get_ssid(uint8_t index);

void wifi_service_connect(const char *ssid, const char *password);
bool wifi_service_is_critical(void);

// ⭐ NEW: debug label for LVGL popup
#ifdef LV_VERSION_MAJOR
void wifi_service_set_debug_label(lv_obj_t *label);
#else
void wifi_service_set_debug_label(void *label);
#endif
