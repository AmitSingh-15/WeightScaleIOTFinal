#pragma once
#include <cstdint>
#include <lvgl.h>

// ✅ Ensure LVGL is available
#ifndef LV_VERSION_MAJOR
#define LV_VERSION_MAJOR 8
#endif

#ifdef LV_VERSION_MAJOR
#include "invoice_session_service.h"

extern "C" {

void home_screen_create(lv_obj_t *parent);
void home_screen_register_callback(void (*cb)(int evt));

void home_screen_set_weight(float w);
void home_screen_set_quantity(int qty);
void home_screen_set_invoice(uint32_t id);
void home_screen_set_device(const char *name);
void home_screen_set_sync_status(const char *txt);
void home_screen_set_clock_text(const char *txt);

void home_screen_refresh_invoice_details(void);
void home_screen_set_version(const char *ver);

void home_screen_show_save_popup(uint32_t serial_num);
void home_screen_dismiss_save_popup(void);

}  // extern "C"

#else
/* Forward declarations when LVGL is unavailable */
extern "C" {

void home_screen_create(void *parent);
void home_screen_register_callback(void (*cb)(int evt));
void home_screen_set_weight(float w);
void home_screen_set_quantity(int qty);
void home_screen_set_invoice(uint32_t id);
void home_screen_set_device(const char *name);
void home_screen_set_sync_status(const char *txt);
void home_screen_set_clock_text(const char *txt);
void home_screen_refresh_invoice_details(void);
void home_screen_set_version(const char *ver);
void home_screen_show_save_popup(uint32_t serial_num);
void home_screen_dismiss_save_popup(void);

}  // extern "C"

#endif
