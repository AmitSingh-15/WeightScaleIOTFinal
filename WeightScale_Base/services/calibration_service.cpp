#include "services/calibration_service.h"
#include "services/scale_service.h"
#include "config/app_config.h"

void calibration_service_init()
{
    // Currently no persistent calibration state beyond scale_service profile.
}

void calibration_service_set_profile(const scale_profile_t *profile)
{
    if (!profile) return;
    scale_service_set_profile(profile);
}

void calibration_service_apply_zero()
{
    scale_service_tare();
}

void calibration_service_apply_load(float weight)
{
    (void)weight;
    // TODO: For full auto-calibration, compute new scale factor based on known load.
}
