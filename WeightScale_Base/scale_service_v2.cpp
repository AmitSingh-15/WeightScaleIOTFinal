#include "scale_service_v2.h"
#include <HX711.h>
#include <math.h>

#define HX711_DOUT 43
#define HX711_SCK  44
//#define HX711_DOUT 19
//#define HX711_SCK  20

static HX711 scale;

static scale_profile_t activeProfile =
{
    "DEFAULT",
    //100.0f,
    //2280.0f,
    //0.25f,
    //0.02f,
    //1200
      1.0f,
    //61287.5,
      //58281.3,
      //2131.5,
      2174.0,
    0.35f,
    0.08f,
    500
};

static float filtered_weight = 0;
static bool hold_state = false;

static TaskHandle_t scaleTaskHandle = NULL;

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
            if (w < 0.001) w = 0.000;

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
