// Returns true if an item should be added (auto-add logic)
bool scale_service_should_add_item(float *weight_out);
#pragma once

#include "../scale_service_v2.h"

void scale_service_init();
void scale_service_tare();
void scale_service_suspend(void);
void scale_service_resume(void);
