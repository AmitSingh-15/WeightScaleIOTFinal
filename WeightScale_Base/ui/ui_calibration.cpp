#include "ui/ui_calibration.h"
#include "ui_styles.h"
#include "services/scale_service.h"
#include "services/calibration_service.h"

static lv_obj_t *cal_screen = NULL;
static lv_obj_t *lbl_profile = NULL;
static lv_obj_t *lbl_weight = NULL;
static lv_obj_t *lbl_raw = NULL;
static void (*global_event_cb)(int evt) = NULL;

static void button_handler(lv_event_t *e)
{
    if (!global_event_cb) return;
    uintptr_t id = (uintptr_t)lv_event_get_user_data(e);
    global_event_cb((int)id);
}

void ui_calibration_init(void (*event_cb)(int evt))
{
    global_event_cb = event_cb;
    cal_screen = lv_obj_create(NULL);
    lv_obj_add_style(cal_screen, &g_styles.screen, 0);

    lv_obj_t *content = lv_obj_create(cal_screen);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_STRETCH);

    lv_obj_t *header = lv_obj_create(content);
    lv_obj_add_style(header, &g_styles.card, 0);
    lv_obj_set_width(header, LV_PCT(100));
    lv_obj_set_height(header, 60);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_label_set_text(lv_label_create(header), "Calibration Mode");
    lv_obj_t *btn_back = lv_btn_create(header);
    lv_obj_add_event_cb(btn_back, button_handler, LV_EVENT_CLICKED, (void*)UI_EVT_HOME);
    lv_label_set_text(lv_label_create(btn_back), "Back");

    lbl_profile = lv_label_create(content);
    lv_label_set_text(lbl_profile, "Profile: DEFAULT");
    lv_obj_set_style_text_font(lbl_profile, &lv_font_montserrat_28, 0);

    lbl_weight = lv_label_create(content);
    lv_label_set_text(lbl_weight, "Weight: 0.000 kg");
    lv_obj_set_style_text_font(lbl_weight, &lv_font_montserrat_36, 0);

    lbl_raw = lv_label_create(content);
    lv_label_set_text(lbl_raw, "Raw: 0");

    lv_obj_t *buttons = lv_obj_create(content);
    lv_obj_set_flex_flow(buttons, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(buttons, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(buttons, LV_PCT(100));

    lv_obj_t *btn_zero = lv_btn_create(buttons);
    lv_obj_add_event_cb(btn_zero, button_handler, LV_EVENT_CLICKED, (void*)CAL_EVT_CAPTURE_ZERO);
    lv_label_set_text(lv_label_create(btn_zero), "Tare");

    lv_obj_t *btn_load = lv_btn_create(buttons);
    lv_obj_add_event_cb(btn_load, button_handler, LV_EVENT_CLICKED, (void*)CAL_EVT_CAPTURE_LOAD);
    lv_label_set_text(lv_label_create(btn_load), "Capture Load");

    lv_obj_t *btn_save = lv_btn_create(buttons);
    lv_obj_add_event_cb(btn_save, button_handler, LV_EVENT_CLICKED, (void*)CAL_EVT_SAVE);
    lv_label_set_text(lv_label_create(btn_save), "Save");

    lv_scr_load(cal_screen);
}

void ui_calibration_set_live(float weight, long raw)
{
    if (lbl_weight) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Weight: %.3f kg", weight);
        lv_label_set_text(lbl_weight, buf);
    }

    if (lbl_raw) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Raw: %ld", raw);
        lv_label_set_text(lbl_raw, buf);
    }

    const scale_profile_t *p = scale_service_get_profile();
    if (p && lbl_profile) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Profile: %s (scale=%.1f, threshold=%.3f)", p->name, p->scale, p->hold_threshold);
        lv_label_set_text(lbl_profile, buf);
    }
}
