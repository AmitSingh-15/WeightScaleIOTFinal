#pragma once
#include <stdint.h>

/* ================= EVENTS ================= */

typedef enum
{
    DEVNAME_EVT_SAVE = 1
} devname_event_t;

/* Callback receives event, device name, and device ID */
typedef void (*devname_cb_t)(int evt, const char *name, uint32_t dev_id);

#ifdef LV_VERSION_MAJOR
#include <lvgl.h>

/* ================= API ================= */

extern "C" {

void device_name_screen_create(lv_obj_t *parent);

/* callback gives entered text and device ID */
void device_name_screen_register_callback(devname_cb_t cb);

/* helpers */
void device_name_screen_set_title(const char *txt);
void device_name_screen_set_values(const char *name, uint32_t dev_id);
void device_name_screen_focus(void);

}  // extern "C"

#else
/* Forward declarations when LVGL is unavailable */

extern "C" {

void device_name_screen_create(void *parent);
void device_name_screen_register_callback(devname_cb_t cb);
void device_name_screen_set_title(const char *txt);
void device_name_screen_set_values(const char *name, uint32_t dev_id);
void device_name_screen_focus(void);

}  // extern "C"

#endif
