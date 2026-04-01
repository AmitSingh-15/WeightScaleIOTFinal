#include <stdio.h>
#include <lvgl.h>
#include "calibration_wizard_screen.h"
#include "ui_styles.h"

#ifdef LV_VERSION_MAJOR

static void (*event_cb)(int evt) = NULL;

static lv_obj_t *lbl_step;
static lv_obj_t *lbl_weight;
static lv_obj_t *lbl_raw;

// ⭐ OPTIMIZATION: Single reusable buffer for text (saves ~64 bytes)
static char g_wiz_buf[32];

static void btn_evt(lv_event_t *e)
{
    if(!event_cb) return;
    uintptr_t id = (uintptr_t)lv_event_get_user_data(e);
    event_cb((int)id);
}

void calibration_wizard_register_callback(void (*cb)(int evt))
{
    event_cb = cb;
}

void calibration_wizard_create(lv_obj_t *parent)
{

    lv_obj_add_style(parent,&g_styles.screen,0);
    lv_obj_set_size(parent,800,480);

    /* HEADER */
    lv_obj_t *header = lv_obj_create(parent);
    lv_obj_add_style(header,&g_styles.card,0);
    lv_obj_set_size(header,800,80);
    lv_obj_align(header,LV_ALIGN_TOP_MID,0,0);

    lv_label_set_text(lv_label_create(header),"CALIBRATION WIZARD");

    lv_obj_t *back = lv_btn_create(header);
    lv_obj_add_style(back,&g_styles.btn_secondary,0);
    lv_obj_align(back,LV_ALIGN_RIGHT_MID,-10,0);
    lv_obj_add_event_cb(back,btn_evt,LV_EVENT_CLICKED,(void*)WIZ_EVT_BACK);
    lv_label_set_text(lv_label_create(back),"EXIT");

    /* STEP MESSAGE BIG */
    lbl_step = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_step,&lv_font_montserrat_20,0);
    lv_label_set_text(lbl_step,"Select Load Cell Profile");
    lv_obj_align(lbl_step,LV_ALIGN_TOP_MID,0,110);

    /* LIVE DISPLAY */
    lbl_weight = lv_label_create(parent);
    lv_obj_add_style(lbl_weight,&g_styles.value_big,0);
    lv_label_set_text(lbl_weight,"0.00 kg");
    lv_obj_align(lbl_weight,LV_ALIGN_CENTER,0,-20);

    lbl_raw = lv_label_create(parent);
    lv_label_set_text(lbl_raw,"RAW: 0");
    lv_obj_align(lbl_raw,LV_ALIGN_CENTER,0,40);

    /* PROFILE SELECT ROW */
    lv_obj_t *profiles = lv_obj_create(parent);
    lv_obj_add_style(profiles,&g_styles.card,0);
    lv_obj_set_size(profiles,780,80);
    lv_obj_align(profiles,LV_ALIGN_BOTTOM_MID,0,-100);
    lv_obj_set_flex_flow(profiles,LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(profiles,10,0);

    const struct {
        const char* txt;
        int evt;
    } btns[] = {
        {"1KG",WIZ_EVT_PROFILE_1KG},
        {"100KG",WIZ_EVT_PROFILE_100KG},
        {"500KG",WIZ_EVT_PROFILE_500KG}
    };

    for(int i=0;i<3;i++)
    {
        lv_obj_t *b = lv_btn_create(profiles);
        lv_obj_set_size(b,140,60);
        lv_obj_add_style(b,&g_styles.btn_primary,0);
        lv_obj_add_event_cb(b,btn_evt,LV_EVENT_CLICKED,(void*)btns[i].evt);
        lv_label_set_text(lv_label_create(b),btns[i].txt);
    }

    /* NEXT BUTTON */
    lv_obj_t *next = lv_btn_create(parent);
    lv_obj_add_style(next,&g_styles.btn_secondary,0);
    lv_obj_set_size(next,240,70);
    lv_obj_align(next,LV_ALIGN_BOTTOM_MID,0,-10);
    lv_obj_add_event_cb(next,btn_evt,LV_EVENT_CLICKED,(void*)WIZ_EVT_NEXT);
    lv_label_set_text(lv_label_create(next),"NEXT STEP");
}

void calibration_wizard_set_step(const char *txt)
{
    lv_label_set_text(lbl_step,txt);
}

void calibration_wizard_set_live(float weight,long raw)
{
    snprintf(g_wiz_buf,sizeof(g_wiz_buf),"%.3f kg",weight);
    lv_label_set_text(lbl_weight,g_wiz_buf);

    snprintf(g_wiz_buf,sizeof(g_wiz_buf),"RAW: %ld",raw);
    lv_label_set_text(lbl_raw,g_wiz_buf);
}

#else
/* Stub implementations when LVGL is unavailable */
void calibration_wizard_create(void *parent) { }
void calibration_wizard_set_step(const char *txt) { }
void calibration_wizard_set_live(float weight, long raw) { }
void calibration_wizard_register_callback(void (*cb)(int evt)) { }
#endif  // LV_VERSION_MAJOR
