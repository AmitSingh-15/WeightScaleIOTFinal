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
            if (sample < 0.0f) {
                sample = 0.0f;
            }

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
                if (fabs(local_filtered - stable_value) <= activeProfile.hold_threshold)
                {
                    if (stable_start_ts == 0)
                        stable_start_ts = now;

                    if ((now - stable_start_ts) >= activeProfile.hold_time_ms)
                    {
                        hold_state = true;
                    }
                }
                else
                {
                    stable_value = local_filtered;
                    stable_start_ts = now;
                    hold_state = false;
                }
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
