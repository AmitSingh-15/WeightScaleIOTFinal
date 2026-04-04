#pragma once
#ifdef LV_VERSION_MAJOR
#include <lvgl.h>

extern "C" {

void history_screen_create(lv_obj_t *parent);
void history_screen_refresh(void);
void history_screen_register_back(void (*cb)(void));

}  // extern "C"

#else
/* Forward declarations when LVGL is unavailable */
extern "C" {

void history_screen_create(void *parent);
void history_screen_refresh(void);
void history_screen_register_back(void (*cb)(void));

}  // extern "C"

#endif
