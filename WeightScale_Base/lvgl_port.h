#pragma once

#ifdef LV_VERSION_MAJOR

#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>

bool lvgl_port_init(void *lcd_handle, void *touch_handle);
void lvgl_port_loop(void);

#else

inline bool lvgl_port_init(void *lcd_handle, void *touch_handle) { return true; }
inline void lvgl_port_loop(void) {}

#endif

