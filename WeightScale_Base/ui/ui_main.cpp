#include "ui/ui_main.h"
#include "ui/ui_calibration.h"
#include "ui_styles.h"
#include "ui_events.h"
#include "services/scale_service.h"
#include "services/calibration_service.h"

static lv_obj_t *home_screen = NULL;
static lv_obj_t *label_weight = NULL;
static lv_obj_t *label_qty = NULL;
static lv_obj_t *label_device = NULL;
static lv_obj_t *label_hold = NULL;
static lv_obj_t *label_sync = NULL;

static void (*global_event_cb)(int evt) = NULL;

static void button_handler(lv_event_t *e)
{
    if (!global_event_cb) return;
    uintptr_t id = (uintptr_t)lv_event_get_user_data(e);
    global_event_cb((int)id);
}

void ui_main_init(void (*event_cb)(int evt))
{
    global_event_cb = event_cb;

    home_screen = lv_obj_create(NULL);
    lv_obj_add_style(home_screen, &g_styles.screen, 0);

    lv_obj_t *container = lv_obj_create(home_screen);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_STRETCH);

    // header
    lv_obj_t *header = lv_obj_create(container);
    lv_obj_add_style(header, &g_styles.card, 0);
    lv_obj_set_width(header, LV_PCT(100));
    lv_obj_set_height(header, 80);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    label_device = lv_label_create(header);
    lv_label_set_text(label_device, "Device: -");
    label_sync = lv_label_create(header);
    lv_label_set_text(label_sync, "Offline");

    lv_obj_t *btn_cal = lv_btn_create(header);
    lv_obj_add_event_cb(btn_cal, button_handler, LV_EVENT_RELEASED, (void*)UI_EVT_CALIBRATE);
    lv_label_set_text(lv_label_create(btn_cal), "Calibrate");

    lv_obj_t *btn_settings = lv_btn_create(header);
    lv_obj_add_event_cb(btn_settings, button_handler, LV_EVENT_RELEASED, (void*)UI_EVT_SETTINGS);
    lv_label_set_text(lv_label_create(btn_settings), "Settings");

    // main weight view
    lv_obj_t *main = lv_obj_create(container);
    lv_obj_add_style(main, &g_styles.card, 0);
    lv_obj_set_size(main, LV_PCT(100), LV_PCT(60));
    lv_obj_set_flex_flow(main, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    label_weight = lv_label_create(main);
    lv_obj_set_style_text_font(label_weight, &lv_font_montserrat_28, 0);
    lv_label_set_text(label_weight, "0.00 kg");

    label_hold = lv_label_create(main);
    lv_obj_set_style_text_font(label_hold, &lv_font_montserrat_20, 0);
    lv_label_set_text(label_hold, "Not Stable");

    lv_obj_t *qty_row = lv_obj_create(main);
    lv_obj_set_flex_flow(qty_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(qty_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    label_qty = lv_label_create(qty_row);
    lv_label_set_text(label_qty, "Qty: 1");

    // controls
    lv_obj_t *controls = lv_obj_create(container);
    lv_obj_add_style(controls, &g_styles.card, 0);
    lv_obj_set_width(controls, LV_PCT(100));
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);


    lv_obj_t *btn_finalize = lv_btn_create(controls);
    lv_obj_add_event_cb(btn_finalize, button_handler, LV_EVENT_RELEASED, (void*)UI_EVT_RESET);
    lv_label_set_text(lv_label_create(btn_finalize), "Finalize");

    lv_obj_t *btn_clear = lv_btn_create(controls);
    lv_obj_add_event_cb(btn_clear, button_handler, LV_EVENT_RELEASED, (void*)UI_EVT_RESET_ALL);
    lv_label_set_text(lv_label_create(btn_clear), "Clear");

    lv_scr_load(home_screen);
}

void ui_main_set_weight(float kg, bool hold)
{
    if (label_weight)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f kg", kg);
        lv_label_set_text(label_weight, buf);
    }
    if (label_hold)
    {
        lv_label_set_text(label_hold, hold ? "Stable" : "Changing");
    }
}

void ui_main_set_quantity(int qty)
{
    if (label_qty)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "Qty: %d", qty);
        lv_label_set_text(label_qty, buf);
    }
}

void ui_main_set_device_name(const char *name)
{
    if (label_device)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "Device: %s", name);
        lv_label_set_text(label_device, buf);
    }
}

void ui_main_set_sync_status(const char *status)
{
    if (label_sync)
        lv_label_set_text(label_sync, status);
}
