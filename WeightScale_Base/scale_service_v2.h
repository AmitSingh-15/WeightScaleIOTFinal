// Returns true if an item should be added (auto-add logic)
bool scale_service_should_add_item(float *weight_out);
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

/* Calibration profile (3-point linear regression) */
typedef struct
{
    char  name[16];
    int   max_capacity;      /* e.g. 100, 500, 600 kg */
    float slope;
    float offset;
    bool  valid;             /* true if calibrated */
} cal_profile_t;

void scale_service_init();
void scale_service_set_profile(const scale_profile_t *profile);

float scale_service_get_weight();
bool  scale_service_is_hold();

void scale_service_tare();
long scale_service_get_raw();
long scale_service_get_raw_avg(int samples);
const scale_profile_t* scale_service_get_profile();

void scale_service_suspend(void);
void scale_service_resume(void);

/* Auto-zero: runs in background, tares when weight < threshold for N seconds */
void scale_service_enable_auto_zero(bool enable);
bool scale_service_auto_zero_active(void);

/* Calibration profile management */
void scale_service_set_cal_profile(const cal_profile_t *cp);
const cal_profile_t* scale_service_get_cal_profile(void);

#endif