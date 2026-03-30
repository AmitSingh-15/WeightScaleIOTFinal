#pragma once
#ifdef LV_VERSION_MAJOR
#include <lvgl.h>

typedef enum
{
    WIZ_EVT_BACK = 1,
    WIZ_EVT_PROFILE_1KG,
    WIZ_EVT_PROFILE_100KG,
    WIZ_EVT_PROFILE_500KG,
    WIZ_EVT_NEXT
} wiz_event_t;

void calibration_wizard_create(lv_obj_t *parent);
void calibration_wizard_set_step(const char *txt);
void calibration_wizard_set_live(float weight,long raw);

void calibration_wizard_register_callback(void (*cb)(int evt));

#else
/* Forward declarations when LVGL is unavailable */
typedef enum { WIZ_EVT_BACK = 1, WIZ_EVT_PROFILE_1KG, WIZ_EVT_PROFILE_100KG, WIZ_EVT_PROFILE_500KG, WIZ_EVT_NEXT } wiz_event_t;
void calibration_wizard_create(void *parent);
void calibration_wizard_set_step(const char *txt);
void calibration_wizard_set_live(float weight, long raw);
void calibration_wizard_register_callback(void (*cb)(int evt));
#endif
