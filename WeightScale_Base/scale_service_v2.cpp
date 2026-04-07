#include "scale_service_v2.h"
#include <HX711.h>
#include <math.h>
#include "devlog.h"

#define HX711_DOUT 27
#define HX711_SCK  28

static HX711 scale;

static scale_profile_t activeProfile =
{
    "DEFAULT",
    1.0f,
    2174.0,
    0.35f,
    0.08f,
    500
};

/* Calibration profile (3-point linear regression result) */
static cal_profile_t activeCal = { "NONE", 500, 1.0f, 0.0f, false };

static float filtered_weight = 0;
static bool hold_state = false;

static TaskHandle_t scaleTaskHandle = NULL;

/* Auto-zero state */
static bool auto_zero_enabled = true;
static const float AUTO_ZERO_THRESHOLD = 0.05f;   /* kg */
static const unsigned long AUTO_ZERO_STABLE_MS = 2000; /* 2 seconds stable */
static const unsigned long AUTO_ZERO_COOLDOWN_MS = 10000; /* 10s between zeros */
static unsigned long auto_zero_stable_since = 0;
static unsigned long auto_zero_last_tare = 0;
static bool auto_zero_was_loaded = false;  /* track if weight was on scale recently */

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

    const int SAMPLE_COUNT = 16;     // 🔥 industrial averaging window
    float samples[SAMPLE_COUNT];
    int index = 0;
    bool buffer_full = false;

    float last_valid = 0;

    while (true)
    {
        if (scale.is_ready())
        {
            /* ===== READ RAW WEIGHT ===== */
            float w = scale.get_units(1);
            if (w < 0.01) w = 0.00;

            /* ===== STORE IN ROLLING BUFFER ===== */
            samples[index++] = w;

            if(index >= SAMPLE_COUNT)
            {
                index = 0;
                buffer_full = true;
            }

            /* ===== ONLY PROCESS WHEN BUFFER READY ===== */
            if(buffer_full)
            {
                /* ---------- AVERAGE ---------- */
                float sum = 0;
                for(int i=0;i<SAMPLE_COUNT;i++)
                    sum += samples[i];

                float avg = sum / SAMPLE_COUNT;

                /* ---------- Apply calibration profile if valid ---------- */
                if(activeCal.valid) {
                    /* weight = slope * raw_units + offset */
                    avg = activeCal.slope * avg + activeCal.offset;
                    if(avg < 0.001f) avg = 0.0f;
                }

                /* ---------- SPIKE REJECTION ---------- */
                if(fabs(avg - last_valid) > activeProfile.hold_threshold)
                {
                    last_valid = avg;
                }

                /* ---------- EMA SMOOTH ---------- */
                filtered_weight = ema(
                    filtered_weight,
                    last_valid,
                    activeProfile.ema_alpha
                );

                /* ---------- AUTO-ZERO ---------- */
                if(auto_zero_enabled)
                {
                    float abs_wt = fabs(filtered_weight);
                    if(abs_wt > 0.5f) {
                        /* Something is on the scale */
                        auto_zero_was_loaded = true;
                        auto_zero_stable_since = 0;
                    }
                    else if(abs_wt < AUTO_ZERO_THRESHOLD) {
                        /* Near zero — start/continue stability timer */
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

        vTaskDelay(pdMS_TO_TICKS(50));   // faster loop = smoother response
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

long scale_service_get_raw()
{
    if (!scale.is_ready()) return 0;
    return scale.read();
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
    if(!scale.is_ready()) return 0;

    long total = 0;
    int count = 0;
    for(int i = 0; i < samples; i++) {
        if(scale.is_ready()) {
            total += scale.read();
            count++;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return count > 0 ? (total / count) : 0;
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

    if(fabs(w - last_weight) < 0.02f)   // stability threshold
    {
        if(stable_start == 0)
            stable_start = millis();

        if(!captured && (millis() - stable_start > 600))
        {
            if(w > 0.05f)
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