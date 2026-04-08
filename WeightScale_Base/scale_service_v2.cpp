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
    0.35f,
    0.08f,
    500
};

/* Calibration profile (3-point linear regression result) */
static cal_profile_t activeCal = { "NONE", 500, 1.0f, 0.0f, false };

static float filtered_weight = 0;
static bool hold_state = false;
static long last_valid_raw = 0;  /* cached raw for glitch-free display */

static TaskHandle_t scaleTaskHandle = NULL;

/* Filter reset flag — set by scale_service_reset_filter(), consumed in scale_task() */
static volatile bool g_filter_reset_pending = false;

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

    int consecutive_fails = 0;

    while (true)
    {
        /* Handle filter reset request from auto-zero calibration */
        if(g_filter_reset_pending) {
            g_filter_reset_pending = false;
            filtered_weight = 0.0f;
            last_valid = 0.0f;
            last_valid_raw = 0;
            index = 0;
            buffer_full = false;
            memset(samples, 0, sizeof(samples));
            consecutive_fails = 0;
        }

        if (scale.is_ready())
        {
            /* ===== READ RAW WEIGHT (3-sample average for noise reduction) ===== */
            float w = scale.get_units(3);

            /* ===== REJECT ZERO-SPIKES =====
               If we have a valid last reading and this one drops to ~0,
               it's almost certainly a HX711 glitch — skip it. */
            if(last_valid > 0.5f && fabs(w) < 0.05f) {
                consecutive_fails++;
                if(consecutive_fails < 5) {
                    /* Ignore this glitch read, keep previous value */
                    vTaskDelay(pdMS_TO_TICKS(30));
                    continue;
                }
                /* If 5+ consecutive "zeros", accept it as real (weight removed) */
            }
            consecutive_fails = 0;

            /* Clamp small negatives to zero */
            if(w < 0.0f) w = 0.0f;

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

                /* Trim 2 from each end */
                float sum = 0;
                const int trim = 2;
                for(int i = trim; i < SAMPLE_COUNT - trim; i++)
                    sum += sorted[i];
                float avg = sum / (SAMPLE_COUNT - 2 * trim);

                /* ---------- Apply calibration profile if valid ---------- */
                if(activeCal.valid) {
                    avg = activeCal.slope * avg + activeCal.offset;
                    if(avg < 0.01f) avg = 0.0f;
                }

                /* ---------- SPIKE REJECTION ---------- */
                float diff = fabs(avg - last_valid);
                if(diff < activeProfile.hold_threshold || diff > 0.3f) {
                    /* Accept: either within normal jitter, or a real change */
                    last_valid = avg;
                }
                /* else: moderate spike — ignore, keep last_valid */

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

        vTaskDelay(pdMS_TO_TICKS(40));
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
    if (!scale.is_ready()) return last_valid_raw;
    long r = scale.read();
    /* Reject zero-glitch: if we had a valid reading and this is ~0, keep cached */
    if(last_valid_raw != 0 && r == 0) return last_valid_raw;
    last_valid_raw = r;
    return r;
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
    if(!scale.is_ready()) return last_valid_raw;

    long total = 0;
    int count = 0;
    for(int i = 0; i < samples; i++) {
        if(scale.is_ready()) {
            long r = scale.read();
            /* Skip zero-glitch reads */
            if(r == 0 && last_valid_raw != 0) continue;
            total += r;
            count++;
            if(r != 0) last_valid_raw = r;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return count > 0 ? (total / count) : last_valid_raw;
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