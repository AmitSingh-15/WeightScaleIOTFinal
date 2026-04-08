#include "scale_service_v2.h"
#include <HX711.h>
#include <math.h>
#include "devlog.h"

#define HX711_DOUT 48
#define HX711_SCK  47

static HX711 scale;

static scale_profile_t activeProfile =
{
    "DEFAULT",
    1.0f,
    2174.0,
    0.70f,           // ema_alpha — heavy machine, grams don't matter
    0.30f,           // hold_threshold — was 0.08, 300g tolerance for heavy scale
    500
};

/* Calibration profile (3-point linear regression result) */
static cal_profile_t activeCal = { "NONE", 500, 1.0f, 0.0f, false };

static float filtered_weight = 0;
static bool hold_state = false;
static volatile long last_valid_raw = 0;  /* cached tare-adjusted raw delta */
static long g_stable_raw_candidate = 0;
static unsigned long g_stable_raw_since = 0;

static TaskHandle_t scaleTaskHandle = NULL;

/* Filter reset flag — set by scale_service_reset_filter(), consumed in scale_task() */
static volatile bool g_filter_reset_pending = false;

/* Auto-zero state */
static bool auto_zero_enabled = true;
static const float AUTO_ZERO_THRESHOLD = 0.30f;   /* kg — was 0.05, heavy machine tolerance */
static const unsigned long AUTO_ZERO_STABLE_MS = 2000; /* 2 seconds stable */
static const unsigned long AUTO_ZERO_COOLDOWN_MS = 10000; /* 10s between zeros */
static const unsigned long RAW_STABLE_MS = 250; /* was 500 — settle faster */
static const float FAST_TRACK_STEP_KG = 3.0f; /* was 5.0 — snap sooner */
static const float MEDIUM_TRACK_STEP_KG = 0.5f; /* was 1.0 — use fast EMA for smaller changes */
static const float RETURN_ZERO_THRESHOLD_KG = 0.50f; /* was 0.20 — heavy machine, under 500g = zero */
static const unsigned long RETURN_ZERO_MS = 500; /* was 2000 — snap to zero fast */
static unsigned long auto_zero_stable_since = 0;
static unsigned long auto_zero_last_tare = 0;
static bool auto_zero_was_loaded = false;  /* track if weight was on scale recently */
static unsigned long return_zero_since = 0;

static float ema(float prev, float input, float alpha)
{
    return (alpha * input) + ((1.0f - alpha) * prev);
}

static void scale_task(void *p)
{
    scale.begin(HX711_DOUT, HX711_SCK);
    delay(3000);

    scale.set_scale(activeProfile.scale);
    scale.tare();

    const int SAMPLE_COUNT = 4;      // was 8 — fill buffer in 100ms instead of 320ms
    float samples[SAMPLE_COUNT];
    long raw_samples[SAMPLE_COUNT];
    int index = 0;
    bool buffer_full = false;

    float last_valid = 0;

    int consecutive_fails = 0;

    while (true)
    {
        /* Handle filter reset request from auto-zero calibration */
        if(g_filter_reset_pending) {
            g_filter_reset_pending = false;
            filtered_weight = 0.0f;
            last_valid = 0.0f;
            last_valid_raw = 0;
            g_stable_raw_candidate = 0;
            g_stable_raw_since = 0;
            return_zero_since = 0;
            index = 0;
            buffer_full = false;
            memset(samples, 0, sizeof(samples));
            memset(raw_samples, 0, sizeof(raw_samples));
            consecutive_fails = 0;
        }

        if (scale.is_ready())
        {
            /* Read tare-adjusted raw counts once here. This task is the only
               code path allowed to talk to HX711 directly. */
            long raw = scale.get_value(1);  /* was 3 — trimmed avg handles noise, no need to block here */

            float w;
            if(activeCal.valid) {
                w = activeCal.slope * (float)raw + activeCal.offset;
            } else {
                w = (float)raw / activeProfile.scale;
            }

            /* ===== REJECT ZERO-SPIKES =====
               If we have a valid last reading and this one drops to ~0,
               it's almost certainly a HX711 glitch — skip it. */
            if(last_valid > 1.0f && fabs(w) < 0.10f) {  /* heavy machine thresholds */
                consecutive_fails++;
                if(consecutive_fails < 3) {  /* was 5 — accept real removal faster */
                    vTaskDelay(pdMS_TO_TICKS(20));
                    continue;
                }
                /* If 3+ consecutive "zeros", accept it as real (weight removed) */
            }
            consecutive_fails = 0;

            /* Clamp small values to zero — heavy machine, sub-100g is noise */
            if(w < 0.0f) w = 0.0f;
            if(w < 0.10f) {
                w = 0.0f;
                raw = 0;
            }

            /* ===== STORE IN ROLLING BUFFER ===== */
            int slot = index;
            samples[slot] = w;
            raw_samples[slot] = raw;
            index++;

            if(index >= SAMPLE_COUNT)
            {
                index = 0;
                buffer_full = true;
            }

            /* ===== ONLY PROCESS WHEN BUFFER READY ===== */
            if(buffer_full)
            {
                /* ---------- MEDIAN-OF-3 TRIMMED AVERAGE ----------
                   Sort, discard lowest and highest 2 readings,
                   average the middle values to reject outliers.    */
                float sorted[SAMPLE_COUNT];
                memcpy(sorted, samples, sizeof(samples));

                /* Simple insertion sort (16 elements) */
                for(int i = 1; i < SAMPLE_COUNT; i++) {
                    float key = sorted[i];
                    int j = i - 1;
                    while(j >= 0 && sorted[j] > key) {
                        sorted[j+1] = sorted[j];
                        j--;
                    }
                    sorted[j+1] = key;
                }

                /* Trim 1 from each end (was 2, adjusted for smaller buffer) */
                float sum = 0;
                const int trim = 1;
                for(int i = trim; i < SAMPLE_COUNT - trim; i++)
                    sum += sorted[i];
                float avg = sum / (SAMPLE_COUNT - 2 * trim);

                long raw_sorted[SAMPLE_COUNT];
                memcpy(raw_sorted, raw_samples, sizeof(raw_samples));

                for(int i = 1; i < SAMPLE_COUNT; i++) {
                    long key = raw_sorted[i];
                    int j = i - 1;
                    while(j >= 0 && raw_sorted[j] > key) {
                        raw_sorted[j + 1] = raw_sorted[j];
                        j--;
                    }
                    raw_sorted[j + 1] = key;
                }

                long raw_sum = 0;
                for(int i = trim; i < SAMPLE_COUNT - trim; i++)
                    raw_sum += raw_sorted[i];
                long avg_raw = raw_sum / (SAMPLE_COUNT - 2 * trim);

                long raw_threshold = (long)(activeProfile.hold_threshold * activeProfile.scale);
                if(raw_threshold < 50) raw_threshold = 50;

                unsigned long now_ms = millis();
                if(labs(avg_raw - g_stable_raw_candidate) > raw_threshold) {
                    g_stable_raw_candidate = avg_raw;
                    g_stable_raw_since = now_ms;
                } else if(g_stable_raw_since != 0 &&
                          (now_ms - g_stable_raw_since) >= RAW_STABLE_MS) {
                    if(last_valid_raw != avg_raw) {
                        last_valid_raw = avg_raw;
                        devlog_printf("[SCALE] Raw stable: %ld (weight %.3f kg)",
                                      last_valid_raw, avg);
                    }
                }

                /* ---------- SPIKE REJECTION ---------- */
                /* Heavy machine: accept if within jitter OR a real change (>hold_threshold).
                   No dead zone — hold_threshold is 0.30 so anything that passes trimmed
                   average is either noise (<0.30) or real (>0.30). */
                last_valid = avg;

                /* ---------- ADAPTIVE SMOOTH ---------- */
                float filter_diff = fabsf(last_valid - filtered_weight);
                if(filter_diff >= FAST_TRACK_STEP_KG) {
                    filtered_weight = last_valid;
                } else if(filter_diff >= MEDIUM_TRACK_STEP_KG) {
                    filtered_weight = ema(filtered_weight, last_valid, 0.85f); /* was 0.70 */
                } else {
                    filtered_weight = ema(
                        filtered_weight,
                        last_valid,
                        activeProfile.ema_alpha
                    );
                }

                /* ---------- AUTO-ZERO ---------- */
                if(filtered_weight <= RETURN_ZERO_THRESHOLD_KG) {
                    if(return_zero_since == 0) {
                        return_zero_since = now_ms;
                    } else if((now_ms - return_zero_since) >= RETURN_ZERO_MS) {
                        filtered_weight = 0.0f;
                        last_valid = 0.0f;
                        last_valid_raw = 0;
                    }
                } else {
                    return_zero_since = 0;
                }

                if(auto_zero_enabled)
                {
                    float abs_wt = fabs(filtered_weight);
                    if(abs_wt > 0.5f) {
                        auto_zero_was_loaded = true;
                        auto_zero_stable_since = 0;
                    }
                    else if(abs_wt < AUTO_ZERO_THRESHOLD) {
                        unsigned long now = millis();
                        if(auto_zero_stable_since == 0)
                            auto_zero_stable_since = now;

                        if(auto_zero_was_loaded &&
                           (now - auto_zero_stable_since) >= AUTO_ZERO_STABLE_MS &&
                           (now - auto_zero_last_tare) >= AUTO_ZERO_COOLDOWN_MS)
                        {
                            scale.tare(10);
                            filtered_weight = 0.0f;
                            last_valid = 0.0f;
                            last_valid_raw = 0;
                            g_stable_raw_candidate = 0;
                            g_stable_raw_since = 0;
                            return_zero_since = 0;
                            auto_zero_last_tare = now;
                            auto_zero_was_loaded = false;
                            auto_zero_stable_since = 0;
                            devlog_printf("[SCALE] Auto-zero applied");
                        }
                    } else {
                        auto_zero_stable_since = 0;
                    }
                }
            }
        }
        else
        {
            /* HX711 not ready — don't spam, just wait */
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(25));  /* was 40 — faster sampling loop */
    }
}

void scale_service_init()
{
    // Delay scale task startup so LVGL + RGB panel fully stabilize
    xTaskCreatePinnedToCore(
        scale_task,
        "scaleTask",
        12288  ,
        NULL,
        1,
        &scaleTaskHandle,
        1   // 🔥 MOVE TO CORE 1
    );
}


void scale_service_set_profile(const scale_profile_t *profile)
{
    if (!profile) return;

    activeProfile = *profile;
    scale.set_scale(activeProfile.scale);

    filtered_weight = 0;
    hold_state = false;
}

float scale_service_get_weight()
{
    return filtered_weight;
}

bool scale_service_is_hold()
{
    return hold_state;
}

void scale_service_tare()
{
    scale.tare();
}

void scale_service_reset_filter(void)
{
    g_filter_reset_pending = true;
}

long scale_service_get_raw()
{
    return last_valid_raw;
}

const scale_profile_t* scale_service_get_profile()
{
    return &activeProfile;
}

void scale_service_suspend(void)
{
    if(scaleTaskHandle)
        vTaskSuspend(scaleTaskHandle);
}

void scale_service_resume(void)
{
    if(scaleTaskHandle)
        vTaskResume(scaleTaskHandle);
}

void scale_service_enable_auto_zero(bool enable)
{
    auto_zero_enabled = enable;
    if(enable) {
        auto_zero_stable_since = 0;
        auto_zero_was_loaded = false;
    }
}

bool scale_service_auto_zero_active(void)
{
    return auto_zero_enabled;
}

long scale_service_get_raw_avg(int samples)
{
    if(samples < 1) samples = 1;
    if(samples > 50) samples = 50;

    long total = 0;
    for(int i = 0; i < samples; i++) {
        total += last_valid_raw;
        vTaskDelay(pdMS_TO_TICKS(25));
    }
    return total / samples;
}

void scale_service_set_cal_profile(const cal_profile_t *cp)
{
    if(!cp) return;
    activeCal = *cp;
    devlog_printf("[SCALE] Cal profile set: %s slope=%.6f offset=%.2f valid=%d",
                  cp->name, cp->slope, cp->offset, cp->valid);
}

const cal_profile_t* scale_service_get_cal_profile(void)
{
    return &activeCal;
}

bool scale_service_should_add_item(float *stable_weight)
{
    static float last_weight = 0.0f;
    static unsigned long stable_start = 0;
    static bool captured = false;

    float w = scale_service_get_weight();

    if(fabs(w - last_weight) < 0.50f)   // stability threshold — heavy machine, 500g tolerance
    {
        if(stable_start == 0)
            stable_start = millis();

        if(!captured && (millis() - stable_start > 400))  /* was 600ms */
        {
            if(w > 0.50f)  /* heavy machine — meaningful weight starts at 500g */
            {
                *stable_weight = w;
                captured = true;
                return true;
            }
        }
    }
    else
    {
        stable_start = 0;
        captured = false;
    }

    last_weight = w;
    return false;
}
