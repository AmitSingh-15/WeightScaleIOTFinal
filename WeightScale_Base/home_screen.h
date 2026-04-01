#pragma once
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LV_VERSION_MAJOR
#include <lvgl.h>
#include "invoice_session_service.h"

void home_screen_create(lv_obj_t *parent);
void home_screen_register_callback(void (*cb)(int evt));

void home_screen_set_weight(float w);
void home_screen_set_quantity(uint16_t qty);
void home_screen_set_invoice(uint32_t id);
void home_screen_set_device(const char *name);
void home_screen_set_sync_status(const char *txt);

void home_screen_refresh_invoice_details(void);
void home_screen_set_version(const char *ver);

#else
/* Forward declarations when LVGL is unavailable */
void home_screen_create(void *parent);
void home_screen_register_callback(void (*cb)(int evt));
void home_screen_set_weight(float w);
void home_screen_set_quantity(uint16_t qty);
void home_screen_set_invoice(uint32_t id);
void home_screen_set_device(const char *name);
void home_screen_set_sync_status(const char *txt);
void home_screen_refresh_invoice_details(void);
void home_screen_set_version(const char *ver);
#endif

#ifdef __cplusplus
}
#endif
