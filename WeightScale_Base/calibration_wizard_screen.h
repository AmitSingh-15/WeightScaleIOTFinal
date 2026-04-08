#pragma once
#ifdef LV_VERSION_MAJOR
#include <lvgl.h>

typedef enum
{
    WIZ_EVT_BACK = 1,
    WIZ_EVT_NEXT,
    WIZ_EVT_SAVE
} wiz_event_t;

extern "C" {

void calibration_wizard_create(lv_obj_t *parent);
void calibration_wizard_set_step(const char *txt);
void calibration_wizard_set_live(float weight, long raw);
void calibration_wizard_set_status(const char *txt, uint32_t color);
void calibration_wizard_show_result(float slope, float offset);
void calibration_wizard_enable_save(bool enable);
float calibration_wizard_get_entered_weight(void);
void calibration_wizard_clear_input(void);

void calibration_wizard_register_callback(void (*cb)(int evt));

}  // extern "C"

#else
typedef enum { WIZ_EVT_BACK = 1, WIZ_EVT_NEXT, WIZ_EVT_SAVE } wiz_event_t;

extern "C" {

void calibration_wizard_create(void *parent);
void calibration_wizard_set_step(const char *txt);
void calibration_wizard_set_live(float weight, long raw);
void calibration_wizard_set_status(const char *txt, unsigned int color);
void calibration_wizard_show_result(float slope, float offset);
void calibration_wizard_enable_save(bool enable);
float calibration_wizard_get_entered_weight(void);
void calibration_wizard_clear_input(void);
void calibration_wizard_register_callback(void (*cb)(int evt));

}  // extern "C"

#endif
