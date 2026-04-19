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
void wifi_service_cancel_scan(void);
int  wifi_service_scan_status(void);   // -1=scanning, -2=failed, >=0=count
uint8_t wifi_service_get_ap_count(void);
String wifi_service_get_ssid(uint8_t index);
int8_t wifi_service_get_rssi(uint8_t index);

void wifi_service_connect(const char *ssid, const char *password);
void wifi_service_disconnect(void);
void wifi_service_request_reconnect(void);
bool wifi_service_is_critical(void);
unsigned long wifi_service_connected_since_ms(void);
String wifi_service_get_connected_ssid(void);

// Debug label (opaque pointer — no LVGL dependency in wifi_service)
void wifi_service_set_debug_label(void *label);

#else  /* !ENABLE_WIFI_SERVICE */

/* Stub implementations when WiFi is disabled */
inline void wifi_service_init(void) {}
inline void wifi_service_loop(void) {}
inline wifi_state_t wifi_service_state(void) { return WIFI_DISCONNECTED; }
inline void wifi_service_register_state_callback(void (*cb)(wifi_state_t)) {}
inline void wifi_service_start_scan(void) {}
inline void wifi_service_cancel_scan(void) {}
inline int  wifi_service_scan_status(void) { return -2; }
inline uint8_t wifi_service_get_ap_count(void) { return 0; }
inline String wifi_service_get_ssid(uint8_t index) { return ""; }
inline int8_t wifi_service_get_rssi(uint8_t index) { return -127; }
inline void wifi_service_connect(const char *ssid, const char *password) {}
inline void wifi_service_disconnect(void) {}
inline void wifi_service_request_reconnect(void) {}
inline bool wifi_service_is_critical(void) { return false; }
inline unsigned long wifi_service_connected_since_ms(void) { return 0; }
inline String wifi_service_get_connected_ssid(void) { return ""; }
inline void wifi_service_set_debug_label(void *label) {}

#endif  /* ENABLE_WIFI_SERVICE */
