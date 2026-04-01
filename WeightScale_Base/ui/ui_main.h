#pragma once

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

void ui_main_init(void (*event_cb)(int evt));
void ui_main_set_weight(float kg, bool hold);
void ui_main_set_quantity(int qty);
void ui_main_set_device_name(const char *name);
void ui_main_set_sync_status(const char *status);

#ifdef __cplusplus
}
#endif
