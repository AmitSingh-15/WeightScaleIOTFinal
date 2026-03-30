#pragma once
#ifndef SCALE_SERVICE_V2_H_DEFINED
#define SCALE_SERVICE_V2_H_DEFINED

#include <Arduino.h>

typedef struct
{
    char  name[16];
    float capacity;
    float scale;
    float ema_alpha;
    float hold_threshold;
    uint32_t hold_time_ms;
} scale_profile_t;

void scale_service_init();
void scale_service_set_profile(const scale_profile_t *profile);

float scale_service_get_weight();
bool  scale_service_is_hold();

void scale_service_tare();
long scale_service_get_raw();
const scale_profile_t* scale_service_get_profile();

void scale_service_suspend(void);
void scale_service_resume(void);

#endif