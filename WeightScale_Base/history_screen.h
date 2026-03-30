#pragma once
#ifdef LV_VERSION_MAJOR
#include <lvgl.h>

void history_screen_create(lv_obj_t *parent);
void history_screen_refresh(void);
void history_screen_register_back(void (*cb)(void));

#else
/* Forward declarations when LVGL is unavailable */
void history_screen_create(void *parent);
void history_screen_refresh(void);
void history_screen_register_back(void (*cb)(void));
#endif
