#pragma once
#include <Arduino.h>
#include "app_config.h"

typedef void (*ota_display_cb_t)(const String &msg);

#if ENABLE_OTA_UPDATES

/* Real implementations - OTA enabled */
void ota_service_init(void);
void ota_service_check_and_update(void);
void ota_service_set_display_callback(ota_display_cb_t cb);
String ota_service_stored_version(void);
String ota_service_current_version(void);

#else

/* Stub implementations - OTA disabled */
inline void ota_service_init(void) { }
inline void ota_service_check_and_update(void) { }
inline void ota_service_set_display_callback(ota_display_cb_t cb) { (void)cb; }
inline String ota_service_stored_version(void) { return ""; }
inline String ota_service_current_version(void) { return ""; }

#endif
