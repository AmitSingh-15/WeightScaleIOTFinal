#pragma once
#include "config/app_config.h"
#include <Arduino.h>

#ifdef LV_VERSION_MAJOR
#include <lvgl.h>
#endif

typedef enum {
    WIFI_DISCONNECTED,
    WIFI_CONNECTING,
    WIFI_CONNECTED
} wifi_state_t;

#if ENABLE_WIFI_SERVICE
void wifi_service_init(void);
void wifi_service_loop(void);
wifi_state_t wifi_service_state(void);

// Register a callback to be notified when WiFi state changes
void wifi_service_register_state_callback(void (*cb)(wifi_state_t));

void wifi_service_start_scan(void);
int  wifi_service_scan_status(void);   // -1=scanning, -2=failed, >=0=count
uint8_t wifi_service_get_ap_count(void);
String wifi_service_get_ssid(uint8_t index);

void wifi_service_connect(const char *ssid, const char *password);
void wifi_service_disconnect(void);
bool wifi_service_is_critical(void);

// ⭐ NEW: debug label for LVGL popup
#ifdef LV_VERSION_MAJOR
void wifi_service_set_debug_label(lv_obj_t *label);
#else
void wifi_service_set_debug_label(void *label);
#endif

#else  /* !ENABLE_WIFI_SERVICE */

/* Stub implementations when WiFi is disabled */
inline void wifi_service_init(void) {}
inline void wifi_service_loop(void) {}
inline wifi_state_t wifi_service_state(void) { return WIFI_DISCONNECTED; }
inline void wifi_service_register_state_callback(void (*cb)(wifi_state_t)) {}
inline void wifi_service_start_scan(void) {}
inline int  wifi_service_scan_status(void) { return -2; }
inline uint8_t wifi_service_get_ap_count(void) { return 0; }
inline String wifi_service_get_ssid(uint8_t index) { return ""; }
inline void wifi_service_connect(const char *ssid, const char *password) {}
inline void wifi_service_disconnect(void) {}
inline bool wifi_service_is_critical(void) { return false; }
#ifdef LV_VERSION_MAJOR
inline void wifi_service_set_debug_label(lv_obj_t *label) {}
#else
inline void wifi_service_set_debug_label(void *label) {}
#endif

#endif  /* ENABLE_WIFI_SERVICE */
