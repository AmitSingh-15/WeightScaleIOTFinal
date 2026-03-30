#pragma once
#include <Arduino.h>

typedef void (*ota_display_cb_t)(const String &msg);

void ota_service_init(void);
void ota_service_check_and_update(void);
void ota_service_set_display_callback(ota_display_cb_t cb);
// ota_service.h
String ota_service_stored_version(void);

String ota_service_current_version(void);
