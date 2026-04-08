#include <stdio.h>
#include <stdlib.h>
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
static lv_obj_t *weight_ta;
static lv_obj_t *weight_kb;

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

static void kb_evt(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_READY) {
        /* User pressed Enter/OK on keyboard — treat as CAPTURE */
        if(event_cb) event_cb(WIZ_EVT_NEXT);
    }
}

void calibration_wizard_create(lv_obj_t *parent)
{
    lv_obj_add_style(parent, &g_styles.screen, 0);
    lv_obj_set_size(parent, DISPLAY_WIDTH, DISPLAY_HEIGHT);

    /* ===== HEADER ===== */
    lv_obj_t *header = lv_obj_create(parent);
    lv_obj_add_style(header, &g_styles.card, 0);
    lv_obj_set_size(header, DISPLAY_WIDTH, 70);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(header);
    lv_obj_add_style(title, &g_styles.title, 0);
    lv_label_set_text(title, LV_SYMBOL_SETTINGS "  3-POINT CALIBRATION");
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t *back = lv_btn_create(header);
    lv_obj_add_style(back, &g_styles.btn_secondary, 0);
    lv_obj_set_size(back, 130, 55);
    lv_obj_align(back, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_add_event_cb(back, btn_evt, LV_EVENT_RELEASED, (void*)WIZ_EVT_BACK);
    lv_label_set_text(lv_label_create(back), LV_SYMBOL_LEFT " EXIT");

    /* ===== LEFT COLUMN: Live display + weight input ===== */

    /* Step instruction */
    lbl_step = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_step, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_step, COLOR_TEXT, 0);
    lv_label_set_text(lbl_step, "Step 1: Enter known weight (kg) and CAPTURE");
    lv_obj_align(lbl_step, LV_ALIGN_TOP_LEFT, 20, 78);

    /* Live display card */
    lv_obj_t *live = lv_obj_create(parent);
    lv_obj_add_style(live, &g_styles.card, 0);
    lv_obj_set_size(live, 480, 110);
    lv_obj_align(live, LV_ALIGN_TOP_LEFT, 20, 105);
    lv_obj_clear_flag(live, LV_OBJ_FLAG_SCROLLABLE);

    lbl_weight = lv_label_create(live);
    lv_obj_add_style(lbl_weight, &g_styles.value_big, 0);
    lv_label_set_text(lbl_weight, "0.00 kg");
    lv_obj_align(lbl_weight, LV_ALIGN_LEFT_MID, 15, 0);

    lbl_raw = lv_label_create(live);
    lv_obj_set_style_text_font(lbl_raw, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_raw, COLOR_MUTED, 0);
    lv_label_set_text(lbl_raw, "RAW: 0");
    lv_obj_align(lbl_raw, LV_ALIGN_RIGHT_MID, -15, 0);

    /* Weight input label */
    lv_obj_t *inp_lbl = lv_label_create(parent);
    lv_obj_set_style_text_font(inp_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(inp_lbl, COLOR_MUTED, 0);
    lv_label_set_text(inp_lbl, "Known Weight (kg):");
    lv_obj_align(inp_lbl, LV_ALIGN_TOP_LEFT, 25, 225);

    /* Textarea for weight entry */
    weight_ta = lv_textarea_create(parent);
    lv_obj_set_size(weight_ta, 300, 55);
    lv_obj_align(weight_ta, LV_ALIGN_TOP_LEFT, 210, 218);
    lv_textarea_set_max_length(weight_ta, 8);
    lv_textarea_set_one_line(weight_ta, true);
    lv_textarea_set_placeholder_text(weight_ta, "e.g. 50.0");
    lv_obj_set_style_text_font(weight_ta, &lv_font_montserrat_28, 0);
    lv_obj_set_style_bg_color(weight_ta, ui_theme_surface(), 0);
    lv_obj_set_style_bg_opa(weight_ta, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(weight_ta, ui_theme_text(), 0);
    lv_obj_set_style_border_color(weight_ta, ui_theme_accent(), 0);
    lv_obj_set_style_pad_all(weight_ta, 8, 0);

    /* Status message */
    lbl_status = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_status, COLOR_MUTED, 0);
    lv_label_set_text(lbl_status, "");
    lv_obj_align(lbl_status, LV_ALIGN_TOP_LEFT, 25, 280);

    /* Result display */
    lbl_result = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_result, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_result, ui_theme_accent(), 0);
    lv_label_set_text(lbl_result, "");
    lv_obj_align(lbl_result, LV_ALIGN_TOP_LEFT, 25, 310);

    /* ===== RIGHT COLUMN: Numeric keypad ===== */

    weight_kb = lv_keyboard_create(parent);
    lv_keyboard_set_mode(weight_kb, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(weight_kb, weight_ta);
    lv_obj_set_size(weight_kb, 490, 310);
    lv_obj_align(weight_kb, LV_ALIGN_TOP_RIGHT, -15, 100);
    lv_obj_add_style(weight_kb, &g_styles.kb_bg, LV_PART_MAIN);
    lv_obj_add_style(weight_kb, &g_styles.kb_btn, LV_PART_ITEMS);
    lv_obj_add_event_cb(weight_kb, kb_evt, LV_EVENT_READY, NULL);

    /* ===== ACTION BUTTONS (bottom) ===== */
    lv_obj_t *actions = lv_obj_create(parent);
    lv_obj_remove_style_all(actions);
    lv_obj_set_size(actions, DISPLAY_WIDTH - 40, 80);
    lv_obj_align(actions, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(actions, 30, 0);

    btn_next = lv_btn_create(actions);
    lv_obj_add_style(btn_next, &g_styles.btn_action, 0);
    lv_obj_set_size(btn_next, 350, 70);
    lv_obj_add_event_cb(btn_next, btn_evt, LV_EVENT_RELEASED, (void*)WIZ_EVT_NEXT);
    lv_obj_t *nl = lv_label_create(btn_next);
    lv_obj_set_style_text_font(nl, &lv_font_montserrat_20, 0);
    lv_label_set_text(nl, LV_SYMBOL_OK " CAPTURE POINT");
    lv_obj_center(nl);

    btn_save = lv_btn_create(actions);
    lv_obj_add_style(btn_save, &g_styles.btn_success, 0);
    lv_obj_set_size(btn_save, 350, 70);
    lv_obj_add_event_cb(btn_save, btn_evt, LV_EVENT_RELEASED, (void*)WIZ_EVT_SAVE);
    lv_obj_t *sl = lv_label_create(btn_save);
    lv_obj_set_style_text_font(sl, &lv_font_montserrat_20, 0);
    lv_label_set_text(sl, LV_SYMBOL_SAVE " SAVE CALIBRATION");
    lv_obj_center(sl);
    lv_obj_add_flag(btn_save, LV_OBJ_FLAG_HIDDEN);
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
        /* Hide keyboard when done */
        if(weight_kb && lv_obj_is_valid(weight_kb))
            lv_obj_add_flag(weight_kb, LV_OBJ_FLAG_HIDDEN);
        if(weight_ta && lv_obj_is_valid(weight_ta))
            lv_obj_add_flag(weight_ta, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(btn_save, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(btn_next, LV_OBJ_FLAG_HIDDEN);
    }
}

float calibration_wizard_get_entered_weight(void)
{
    if(!weight_ta || !lv_obj_is_valid(weight_ta)) return -1.0f;
    const char *txt = lv_textarea_get_text(weight_ta);
    if(!txt || txt[0] == '\0') return -1.0f;
    float val = (float)atof(txt);
    return val;
}

void calibration_wizard_clear_input(void)
{
    if(weight_ta && lv_obj_is_valid(weight_ta))
        lv_textarea_set_text(weight_ta, "");
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // LV_VERSION_MAJOR
