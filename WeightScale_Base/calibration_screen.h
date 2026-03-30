#pragma once
#ifdef LV_VERSION_MAJOR
#include <lvgl.h>

typedef enum {
    CAL_EVT_BACK = 1,
    CAL_EVT_PROFILE_1KG,
    CAL_EVT_PROFILE_100KG,
    CAL_EVT_PROFILE_500KG,
    CAL_EVT_CAPTURE_ZERO,
    CAL_EVT_CAPTURE_LOAD,
    CAL_EVT_SAVE
} cal_event_t;

void calibration_screen_create(lv_obj_t *parent);
void calibration_screen_set_live(float weight, float raw);

void calibration_screen_register_callback(void (*cb)(int evt));
void calibration_screen_set_profile(const char *name);

#else
/* Forward declarations when LVGL is unavailable */
typedef enum { CAL_EVT_BACK = 1, CAL_EVT_PROFILE_1KG, CAL_EVT_PROFILE_100KG, CAL_EVT_PROFILE_500KG, CAL_EVT_CAPTURE_ZERO, CAL_EVT_CAPTURE_LOAD, CAL_EVT_SAVE } cal_event_t;
void calibration_screen_create(void *parent);
void calibration_screen_set_live(float weight, float raw);
void calibration_screen_register_callback(void (*cb)(int evt));
void calibration_screen_set_profile(const char *name);
#endif
