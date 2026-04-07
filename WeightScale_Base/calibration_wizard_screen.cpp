#include <stdio.h>
#include <lvgl.h>

#ifndef LV_VERSION_MAJOR
#define LV_VERSION_MAJOR 8
#endif

#include "config/app_config.h"
#include "calibration_wizard_screen.h"
#include "ui_styles.h"

#ifdef LV_VERSION_MAJOR

#ifdef __cplusplus
extern "C" {
#endif

static void (*event_cb)(int evt) = NULL;

static lv_obj_t *lbl_step;
static lv_obj_t *lbl_weight;
static lv_obj_t *lbl_raw;
static lv_obj_t *lbl_status;
static lv_obj_t *lbl_result;
static lv_obj_t *btn_next;
static lv_obj_t *btn_save;

static char g_wiz_buf[64];

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
    lv_obj_add_style(parent, &g_styles.screen, 0);
    lv_obj_set_size(parent, DISPLAY_WIDTH, DISPLAY_HEIGHT);

    /* ===== HEADER ===== */
    lv_obj_t *header = lv_obj_create(parent);
    lv_obj_add_style(header, &g_styles.card, 0);
    lv_obj_set_size(header, DISPLAY_WIDTH, 80);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(header);
    lv_obj_add_style(title, &g_styles.title, 0);
    lv_label_set_text(title, LV_SYMBOL_SETTINGS "  3-POINT CALIBRATION");
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t *back = lv_btn_create(header);
    lv_obj_add_style(back, &g_styles.btn_secondary, 0);
    lv_obj_set_size(back, 140, 60);
    lv_obj_align(back, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_add_event_cb(back, btn_evt, LV_EVENT_RELEASED, (void*)WIZ_EVT_BACK);
    lv_label_set_text(lv_label_create(back), LV_SYMBOL_LEFT " EXIT");

    /* ===== STEP INSTRUCTION ===== */
    lbl_step = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_step, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_step, COLOR_TEXT, 0);
    lv_label_set_text(lbl_step, "Step 1: Select calibration profile");
    lv_obj_align(lbl_step, LV_ALIGN_TOP_MID, 0, 90);

    /* ===== LIVE DISPLAY CARD ===== */
    lv_obj_t *live = lv_obj_create(parent);
    lv_obj_add_style(live, &g_styles.card, 0);
    lv_obj_set_size(live, DISPLAY_WIDTH - 40, 130);
    lv_obj_align(live, LV_ALIGN_TOP_MID, 0, 120);
    lv_obj_clear_flag(live, LV_OBJ_FLAG_SCROLLABLE);

    lbl_weight = lv_label_create(live);
    lv_obj_add_style(lbl_weight, &g_styles.value_big, 0);
    lv_label_set_text(lbl_weight, "0.00 kg");
    lv_obj_align(lbl_weight, LV_ALIGN_LEFT_MID, 20, 0);

    lbl_raw = lv_label_create(live);
    lv_obj_set_style_text_font(lbl_raw, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_raw, COLOR_MUTED, 0);
    lv_label_set_text(lbl_raw, "RAW: 0");
    lv_obj_align(lbl_raw, LV_ALIGN_RIGHT_MID, -20, 0);

    /* ===== STATUS MESSAGE ===== */
    lbl_status = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_status, COLOR_MUTED, 0);
    lv_label_set_text(lbl_status, "");
    lv_obj_align(lbl_status, LV_ALIGN_TOP_MID, 0, 260);

    /* ===== RESULT DISPLAY ===== */
    lbl_result = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_result, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_result, lv_color_hex(0x38BDF8), 0);
    lv_label_set_text(lbl_result, "");
    lv_obj_align(lbl_result, LV_ALIGN_TOP_MID, 0, 290);

    /* ===== PROFILE SELECT ROW ===== */
    lv_obj_t *profiles = lv_obj_create(parent);
    lv_obj_add_style(profiles, &g_styles.card, 0);
    lv_obj_set_size(profiles, DISPLAY_WIDTH - 40, 90);
    lv_obj_align(profiles, LV_ALIGN_TOP_MID, 0, 320);
    lv_obj_set_flex_flow(profiles, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(profiles, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(profiles, 20, 0);

    const struct { const char* txt; int evt; } prof[] = {
        {"100 KG", WIZ_EVT_PROFILE_100KG},
        {"500 KG", WIZ_EVT_PROFILE_500KG},
        {"600 KG", WIZ_EVT_PROFILE_600KG}
    };

    for(int i = 0; i < 3; i++)
    {
        lv_obj_t *b = lv_btn_create(profiles);
        lv_obj_set_size(b, 220, 65);
        lv_obj_add_style(b, &g_styles.btn_primary, 0);
        lv_obj_add_event_cb(b, btn_evt, LV_EVENT_RELEASED, (void*)(uintptr_t)prof[i].evt);
        lv_obj_t *l = lv_label_create(b);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
        lv_label_set_text(l, prof[i].txt);
        lv_obj_center(l);
    }

    /* ===== ACTION BUTTONS ===== */
    lv_obj_t *actions = lv_obj_create(parent);
    lv_obj_remove_style_all(actions);
    lv_obj_set_size(actions, DISPLAY_WIDTH - 40, 80);
    lv_obj_align(actions, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(actions, 30, 0);

    btn_next = lv_btn_create(actions);
    lv_obj_add_style(btn_next, &g_styles.btn_action, 0);
    lv_obj_set_size(btn_next, 300, 70);
    lv_obj_add_event_cb(btn_next, btn_evt, LV_EVENT_RELEASED, (void*)WIZ_EVT_NEXT);
    lv_obj_t *nl = lv_label_create(btn_next);
    lv_obj_set_style_text_font(nl, &lv_font_montserrat_20, 0);
    lv_label_set_text(nl, "CAPTURE POINT");
    lv_obj_center(nl);

    btn_save = lv_btn_create(actions);
    lv_obj_add_style(btn_save, &g_styles.btn_success, 0);
    lv_obj_set_size(btn_save, 300, 70);
    lv_obj_add_event_cb(btn_save, btn_evt, LV_EVENT_RELEASED, (void*)WIZ_EVT_SAVE);
    lv_obj_t *sl = lv_label_create(btn_save);
    lv_obj_set_style_text_font(sl, &lv_font_montserrat_20, 0);
    lv_label_set_text(sl, LV_SYMBOL_SAVE " SAVE CALIBRATION");
    lv_obj_center(sl);
    lv_obj_add_flag(btn_save, LV_OBJ_FLAG_HIDDEN);  /* hidden until calibration complete */
}

void calibration_wizard_set_step(const char *txt)
{
    if(lbl_step && lv_obj_is_valid(lbl_step))
        lv_label_set_text(lbl_step, txt);
}

void calibration_wizard_set_live(float weight, long raw)
{
    if(lbl_weight && lv_obj_is_valid(lbl_weight)) {
        snprintf(g_wiz_buf, sizeof(g_wiz_buf), "%.2f kg", weight);
        lv_label_set_text(lbl_weight, g_wiz_buf);
    }
    if(lbl_raw && lv_obj_is_valid(lbl_raw)) {
        snprintf(g_wiz_buf, sizeof(g_wiz_buf), "RAW: %ld", raw);
        lv_label_set_text(lbl_raw, g_wiz_buf);
    }
}

void calibration_wizard_set_status(const char *txt, uint32_t color)
{
    if(lbl_status && lv_obj_is_valid(lbl_status)) {
        lv_label_set_text(lbl_status, txt);
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(color), 0);
    }
}

void calibration_wizard_show_result(float slope, float offset)
{
    if(lbl_result && lv_obj_is_valid(lbl_result)) {
        snprintf(g_wiz_buf, sizeof(g_wiz_buf), "Slope: %.6f  Offset: %.2f", slope, offset);
        lv_label_set_text(lbl_result, g_wiz_buf);
    }
}

void calibration_wizard_enable_save(bool enable)
{
    if(!btn_save || !lv_obj_is_valid(btn_save)) return;
    if(!btn_next || !lv_obj_is_valid(btn_next)) return;

    if(enable) {
        lv_obj_clear_flag(btn_save, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_next, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(btn_save, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(btn_next, LV_OBJ_FLAG_HIDDEN);
    }
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // LV_VERSION_MAJOR
