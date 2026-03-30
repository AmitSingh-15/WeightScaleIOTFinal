#ifndef SETTINGS_SCREEN_H
#define SETTINGS_SCREEN_H

#ifdef LV_VERSION_MAJOR
#include <lvgl.h>

/* Create the settings screen */
void settings_screen_create(lv_obj_t *parent);

/* Update WiFi status label on screen */
void settings_screen_update_wifi_status(void);

/* Register callbacks */
void settings_screen_register_back_callback(void (*cb)(void));
void settings_screen_register_calibration_callback(void (*cb)(void));

/* ⭐ NEW: Show settings screen (used by WiFi popup) */
void settings_screen_show(void);

#else
/* Forward declarations when LVGL is unavailable */
void settings_screen_create(void *parent);
void settings_screen_update_wifi_status(void);
void settings_screen_register_back_callback(void (*cb)(void));
void settings_screen_register_calibration_callback(void (*cb)(void));
void settings_screen_show(void);
#endif

#endif
