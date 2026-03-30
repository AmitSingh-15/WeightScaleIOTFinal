#pragma once

#include "services/scale_service.h"

void calibration_service_init();
void calibration_service_set_profile(const scale_profile_t *profile);
void calibration_service_apply_zero();
void calibration_service_apply_load(float weight);
