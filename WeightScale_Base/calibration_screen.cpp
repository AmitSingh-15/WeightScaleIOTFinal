#include <lvgl.h>
#include "calibration_screen.h"
#include "app_config.h"
#if ENABLE_HX711_SCALE
#include "scale_service_v2.h"
#endif
#include "ui_styles.h"
#include <stdio.h>

#ifdef LV_VERSION_MAJOR

static void (*event_cb)(int evt) = NULL;

static lv_obj_t *lbl_weight;
static lv_obj_t *lbl_raw;



static lv_obj_t *lbl_profile;

// ⭐ OPTIMIZATION: Single reusable buffer for display text (saves ~128 bytes)
static char g_cal_buf[64];

static void btn_evt(lv_event_t *e)
{
    if(!event_cb) return;
    uintptr_t id = (uintptr_t)lv_event_get_user_data(e);
    event_cb((int)id);
}

void calibration_screen_register_callback(void (*cb)(int evt))
{
    event_cb = cb;
}

/* =====================================================
   CREATE INDUSTRIAL CALIBRATION SCREEN
=====================================================*/

void calibration_screen_create(lv_obj_t *parent)
{
    lv_obj_add_style(parent,&g_styles.screen,0);
    lv_obj_set_size(parent, screenWidth, screenHeight);

    /* ===== HEADER ===== */

    lv_obj_t *header = lv_obj_create(parent);
    lv_obj_add_style(header,&g_styles.card,0);
    lv_obj_set_size(header, screenWidth, 80);
    lv_obj_align(header,LV_ALIGN_TOP_MID,0,0);

    lv_label_set_text(lv_label_create(header),"CALIBRATION MODE");

    lv_obj_t *back = lv_btn_create(header);
    lv_obj_add_style(back,&g_styles.btn_secondary,0);
    lv_obj_align(back,LV_ALIGN_RIGHT_MID,-10,0);
    lv_obj_add_event_cb(back,btn_evt,LV_EVENT_CLICKED,(void*)CAL_EVT_BACK);
    lv_label_set_text(lv_label_create(back),"BACK");

    /* ===== LIVE DISPLAY ===== */

    lv_obj_t *live = lv_obj_create(parent);
    lv_obj_add_style(live,&g_styles.card,0);
    lv_obj_set_size(live, screenWidth - 40, 150);
    lv_obj_align(live,LV_ALIGN_TOP_MID,0,90);

    lbl_profile = lv_label_create(live);
    lv_label_set_text(lbl_profile,"Profile: NONE");
    lv_obj_align(lbl_profile,LV_ALIGN_TOP_LEFT,10,10);

    lbl_weight = lv_label_create(live);
    lv_obj_add_style(lbl_weight,&g_styles.value_big,0);
    lv_label_set_text(lbl_weight,"0.000 kg");
    lv_obj_align(lbl_weight,LV_ALIGN_CENTER,0,-10);

    lbl_raw = lv_label_create(live);
    lv_label_set_text(lbl_raw,"RAW: 0.000 kg");
    lv_obj_align(lbl_raw,LV_ALIGN_BOTTOM_MID,0,-10);

    /* ===== PROFILE SELECT ===== */

    lv_obj_t *profile = lv_obj_create(parent);
    lv_obj_add_style(profile,&g_styles.card,0);
    lv_obj_set_size(profile, screenWidth - 40, 80);
    lv_obj_align(profile,LV_ALIGN_TOP_MID,0,250);
    lv_obj_set_flex_flow(profile,LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(profile,10,0);

    const struct {
        const char* txt;
        int evt;
    } profiles[] = {
        {"RAW", CAL_EVT_PROFILE_1KG},
        {"1KG", CAL_EVT_PROFILE_100KG},
        {"500KG", CAL_EVT_PROFILE_500KG}
    };

    for(int i=0;i<3;i++)
    {
        lv_obj_t *b = lv_btn_create(profile);
        lv_obj_set_size(b,120,60);
        lv_obj_add_style(b,&g_styles.btn_primary,0);
        lv_obj_add_event_cb(b,btn_evt,LV_EVENT_CLICKED,(void*)profiles[i].evt);
        lv_label_set_text(lv_label_create(b),profiles[i].txt);
    }

    /* ===== CALIBRATION ACTIONS ===== */

    lv_obj_t *actions = lv_obj_create(parent);
    lv_obj_add_style(actions,&g_styles.card,0);
    lv_obj_set_size(actions, screenWidth - 40, 100);
    lv_obj_align(actions,LV_ALIGN_BOTTOM_MID,0,-10);
    lv_obj_set_flex_flow(actions,LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(actions,20,0);

    const struct {
        const char* txt;
        int evt;
    } act[] = {
        {"CAPTURE ZERO", CAL_EVT_CAPTURE_ZERO},
        {"CAPTURE LOAD", CAL_EVT_CAPTURE_LOAD},
        {"SAVE", CAL_EVT_SAVE}
    };

    for(int i=0;i<3;i++)
    {
        lv_obj_t *b = lv_btn_create(actions);
        lv_obj_set_size(b,200,70);
        lv_obj_add_style(b,&g_styles.btn_secondary,0);
        lv_obj_add_event_cb(b,btn_evt,LV_EVENT_CLICKED,(void*)act[i].evt);
        lv_label_set_text(lv_label_create(b),act[i].txt);
    }
}

/* =====================================================
   LIVE UPDATE
=====================================================*/


void calibration_screen_set_live(float weight,float raw)
{
    /* 🔥 HARD SAFETY CHECKS */
    if(!lbl_profile || !lv_obj_is_valid(lbl_profile)) return;
    if(!lbl_weight  || !lv_obj_is_valid(lbl_weight))  return;
    if(!lbl_raw     || !lv_obj_is_valid(lbl_raw))     return;

#if ENABLE_HX711_SCALE
    const scale_profile_t *p = scale_service_get_profile();
    if(!p) return;

    snprintf(g_cal_buf, sizeof(g_cal_buf),
             "Profile: %s | Scale: %.1f",
             p->name,
             p->scale);
    lv_label_set_text(lbl_profile, g_cal_buf);
#endif

    int value = (int)(weight * 100);
    lv_snprintf(g_cal_buf, sizeof(g_cal_buf), "%d.%03d kg", value / 100, abs(value % 100));
    
    lv_label_set_text(lbl_weight, g_cal_buf);

    int value1 = (int)(raw * 100);
    lv_snprintf(g_cal_buf, sizeof(g_cal_buf), "%d.%03d", value1 / 100, abs(value1 % 100));
    lv_label_set_text(lbl_raw, g_cal_buf);
}

#else
/* Stub implementations when LVGL is unavailable */
void calibration_screen_create(void *parent) { }
void calibration_screen_set_live(float weight, float raw) { }
void calibration_screen_register_callback(void (*cb)(int evt)) { }
void calibration_screen_set_profile(const char *name) { }
#endif  // LV_VERSION_MAJOR
