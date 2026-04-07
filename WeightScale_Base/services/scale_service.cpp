#include <string.h>

typedef enum {
    SCALE_STATE_IDLE = 0,
    SCALE_STATE_MEASURING,
    SCALE_STATE_STABLE,
    SCALE_STATE_WAIT_FOR_REMOVE
} scale_state_t;

static scale_state_t g_scale_state = SCALE_STATE_IDLE;
static float g_last_stable_weight = 0.0f;
static uint32_t g_stable_time = 0;
static bool g_duplicate_guard = false;

// Thread-safe API for controller
bool scale_service_should_add_item(float *weight_out) {
    bool should_add = false;
    if (scale_mutex && xSemaphoreTake(scale_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (g_scale_state == SCALE_STATE_WAIT_FOR_REMOVE && !g_duplicate_guard) {
            should_add = true;
            if (weight_out) *weight_out = g_last_stable_weight;
            g_duplicate_guard = true;
        }
        xSemaphoreGive(scale_mutex);
    }
    return should_add;
}
#include "services/scale_service.h"
#include "config/app_config.h"
#include <HX711.h>

static HX711 scale;
static SemaphoreHandle_t scale_mutex = NULL;
static TaskHandle_t scaleTaskHandle = NULL;

static scale_profile_t activeProfile = {
    "DEFAULT",
    1.0f,
    2174.0f,
    0.35f,
    0.08f,
    500
};

static float filtered_weight = 0.0f;
static bool hold_state = false;

static float ema(float prev, float input, float alpha)
{
    return (alpha * input) + ((1.0f - alpha) * prev);
}

static void scale_task(void *p)
{
    scale.begin(HX711_DOUT_PIN, HX711_SCK_PIN);
    vTaskDelay(pdMS_TO_TICKS(500));

    scale.set_scale(activeProfile.scale);
    scale.tare();

    const int SAMPLE_COUNT = 16;
    float samples[SAMPLE_COUNT];
    int index = 0;
    bool buffer_full = false;
    float last_valid = 0.0f;
    float stable_value = 0.0f;
    uint32_t stable_start_ts = 0;

    while (true)
    {
        if (scale.is_ready())
        {
            float sample = scale.get_units(1);
            if (sample < 0.0f) sample = 0.0f;

            samples[index++] = sample;
            if (index >= SAMPLE_COUNT) {
                index = 0;
                buffer_full = true;
            }

            if (buffer_full)
            {
                float sum = 0;
                for (int i = 0; i < SAMPLE_COUNT; i++) sum += samples[i];
                float avg = sum / SAMPLE_COUNT;

                // Noise filtering: ignore <5g
                if (avg < 0.005f) avg = 0.0f;

                if (fabs(avg - last_valid) <= activeProfile.hold_threshold || last_valid == 0.0f)
                {
                    last_valid = avg;
                }

                float local_filtered;
                if (xSemaphoreTake(scale_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
                {
                    filtered_weight = ema(filtered_weight, last_valid, activeProfile.ema_alpha);
                    local_filtered = filtered_weight;
                    xSemaphoreGive(scale_mutex);
                } else {
                    local_filtered = filtered_weight;
                }

                uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

                // --- STATE MACHINE ---
                switch (g_scale_state) {
                case SCALE_STATE_IDLE:
                    if (local_filtered > 0.01f) {
                        g_scale_state = SCALE_STATE_MEASURING;
                        g_stable_time = now;
                    }
                    break;
                case SCALE_STATE_MEASURING:
                    if (fabs(local_filtered - stable_value) <= activeProfile.hold_threshold) {
                        if (stable_start_ts == 0) stable_start_ts = now;
                        if ((now - stable_start_ts) >= activeProfile.hold_time_ms) {
                            g_scale_state = SCALE_STATE_STABLE;
                            g_last_stable_weight = local_filtered;
                            g_stable_time = now;
                        }
                    } else {
                        stable_value = local_filtered;
                        stable_start_ts = now;
                    }
                    break;
                case SCALE_STATE_STABLE:
                    // Immediately transition to WAIT_FOR_REMOVE
                    g_scale_state = SCALE_STATE_WAIT_FOR_REMOVE;
                    g_duplicate_guard = false;
                    break;
                case SCALE_STATE_WAIT_FOR_REMOVE:
                    if (local_filtered < 0.005f) {
                        g_scale_state = SCALE_STATE_IDLE;
                        stable_value = 0.0f;
                        stable_start_ts = 0;
                    }
                    break;
                }

                // Update hold_state for UI
                hold_state = (g_scale_state == SCALE_STATE_STABLE || g_scale_state == SCALE_STATE_WAIT_FOR_REMOVE);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void scale_service_init()
{
    if (!scale_mutex)
        scale_mutex = xSemaphoreCreateMutex();

    xTaskCreatePinnedToCore(
        scale_task,
        "scaleTask",
        SCALE_TASK_STACK_SIZE,
        NULL,
        SCALE_TASK_PRIORITY,
        &scaleTaskHandle,
        SCALE_TASK_CORE
    );
}

void scale_service_set_profile(const scale_profile_t *profile)
{
    if (!profile || !scale_mutex) return;

    if (xSemaphoreTake(scale_mutex, pdMS_TO_TICKS(20)) == pdTRUE)
    {
        activeProfile = *profile;
        filtered_weight = 0.0f;
        hold_state = false;
        xSemaphoreGive(scale_mutex);
    }

    scale.set_scale(activeProfile.scale);
}

void scale_service_tare()
{
    if (scale.is_ready()) {
        scale.tare();
    }
}

float scale_service_get_weight()
{
    float value = 0.0f;

    if (scale_mutex && xSemaphoreTake(scale_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        value = filtered_weight;
        xSemaphoreGive(scale_mutex);
    }

    return value;
}

bool scale_service_is_hold()
{
    bool value = false;
    if (scale_mutex && xSemaphoreTake(scale_mutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        value = hold_state;
        xSemaphoreGive(scale_mutex);
    }
    return value;
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
    if (scaleTaskHandle)
        vTaskSuspend(scaleTaskHandle);
}

void scale_service_resume(void)
{
    if (scaleTaskHandle)
        vTaskResume(scaleTaskHandle);
}
