#pragma once

#include <lvgl.h>

void ui_calibration_init(void (*event_cb)(int evt));
void ui_calibration_set_live(float weight, long raw);
