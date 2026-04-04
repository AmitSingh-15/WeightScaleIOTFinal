#include <lvgl.h>

// ✅ Ensure LVGL is detected
#ifndef LV_VERSION_MAJOR
#define LV_VERSION_MAJOR 8
#endif

#include "config/app_config.h"
#include "settings_screen.h"
#include "ui_styles.h"
#include "wifi_service.h"
#include "ota_service.h"
#include "wifi_list_screen.h"
#include "storage_service.h"
#include "device_name_screen.h"
#include "devlog.h"

extern void wifi_password_popup_show(const char *ssid);

#ifdef LV_VERSION_MAJOR

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================= */
static bool ota_requested = false;
static lv_obj_t *wifi_status;
static void (*back_cb)(void) = NULL;
static void (*calibration_cb)(void) = NULL;

static lv_obj_t *settings_scr_ref = NULL;
static lv_obj_t *wifi_list_scr = NULL;
static lv_obj_t *ota_status_label = NULL;
static lv_obj_t *device_name_label = NULL;
/* WiFi state change callback */
static void wifi_state_cb(wifi_state_t s)
{
    if(wifi_status && lv_obj_is_valid(wifi_status))
        settings_screen_update_wifi_status();
}

/* ================= CONNECTING OVERLAY ================= */

static lv_obj_t *connecting_overlay = NULL;

static void show_connecting_overlay()
{
    if(connecting_overlay) return;

    connecting_overlay = lv_obj_create(settings_scr_ref);
    lv_obj_remove_style_all(connecting_overlay);
    lv_obj_set_size(connecting_overlay, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(connecting_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(connecting_overlay, LV_OPA_60, 0);
    lv_obj_add_flag(connecting_overlay, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *lbl = lv_label_create(connecting_overlay);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_28, 0);
    lv_label_set_text(lbl, "Connecting...");
    lv_obj_center(lbl);
}

static void hide_connecting_overlay()
{
    if(connecting_overlay && lv_obj_is_valid(connecting_overlay))
        lv_obj_del(connecting_overlay);

    connecting_overlay = NULL;
}

/* ================= EVENTS ================= */

static void back_cb_wrapper(lv_event_t *e)
{
    if(back_cb) back_cb();
}



static void ota_cb(lv_event_t *e)
{
    if(ota_requested) return;  // prevent double click

    ota_requested = true;

    lv_obj_add_state(lv_event_get_target(e), LV_STATE_DISABLED);

    if(ota_status_label && lv_obj_is_valid(ota_status_label))
    {
        lv_label_set_text(ota_status_label, "OTA: Requested...");
        lv_refr_now(NULL);
    }
}
//static void ota_cb(lv_event_t *e)
//{
 //   ota_service_check_and_update();
//}

static void scan_cb(lv_event_t *e)
{
    lv_scr_load(wifi_list_scr);
    wifi_list_screen_start_scan();  // async scan with visual feedback
}

static void wifi_selected(const char *ssid)
{
    wifi_password_popup_show(ssid);
}

static void wifi_list_back(void)
{
    lv_scr_load(settings_scr_ref);
}

/* Developer mode toggle */
static lv_obj_t *dev_log_ta = NULL;
static lv_obj_t *dev_mode_sw = NULL;
static lv_timer_t *devlog_timer = NULL;

static void dev_mode_changed(lv_event_t *e)
{
    if(!dev_mode_sw) return;
    bool on = lv_obj_has_state(dev_mode_sw, LV_STATE_CHECKED);
    storage_save_dev_mode(on);
    if(on)
    {
        devlog_load_from_storage();
        if(dev_log_ta && lv_obj_is_valid(dev_log_ta))
            lv_textarea_set_text(dev_log_ta, devlog_get_text().c_str());
    }
    else
    {
        if(dev_log_ta && lv_obj_is_valid(dev_log_ta))
            lv_textarea_set_text(dev_log_ta, "Developer mode disabled");
    }
}

static void clear_logs_cb(lv_event_t *e)
{
    devlog_clear();
    if(dev_log_ta && lv_obj_is_valid(dev_log_ta))
        lv_textarea_set_text(dev_log_ta, "[Logs cleared]");
    devlog_printf("[SETTINGS] Logs cleared by user");
}

/* ================= REGISTRATION ================= */

void settings_screen_register_back_callback(void (*cb)(void))
{
    back_cb = cb;
}

void settings_screen_register_calibration_callback(void (*cb)(void))
{
    calibration_cb = cb;
}

/* ================= CREATE ================= */

void settings_screen_create(lv_obj_t *parent)
{
    ui_styles_init();
    settings_scr_ref = parent;

    /* Null stale pointers when this screen is deleted */
    lv_obj_add_event_cb(parent, [](lv_event_t *e) {
        wifi_status = NULL;
        settings_scr_ref = NULL;
        connecting_overlay = NULL;
        ota_status_label = NULL;
        device_name_label = NULL;
        dev_log_ta = NULL;
        dev_mode_sw = NULL;
        wifi_list_scr = NULL;
    }, LV_EVENT_DELETE, NULL);

    lv_obj_add_style(parent, &g_styles.screen, 0);
    lv_obj_set_size(parent, DISPLAY_WIDTH, DISPLAY_HEIGHT);

    /* ===== HEADER BAR ===== */
    lv_obj_t *header = lv_obj_create(parent);
    lv_obj_add_style(header, &g_styles.card, 0);
    lv_obj_set_size(header, DISPLAY_WIDTH, 90);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, LV_SYMBOL_SETTINGS "  SETTINGS");
    lv_obj_add_style(title, &g_styles.title, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t *back_btn = lv_btn_create(header);
    lv_obj_add_style(back_btn, &g_styles.btn_secondary, 0);
    lv_obj_set_size(back_btn, 160, 65);
    lv_obj_align(back_btn, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_add_event_cb(back_btn, back_cb_wrapper, LV_EVENT_RELEASED, NULL);
    lv_label_set_text(lv_label_create(back_btn), LV_SYMBOL_LEFT " Back");

    /* ===== MAIN CONTENT CARD ===== */
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_add_style(card, &g_styles.card, 0);
    lv_obj_set_size(card, DISPLAY_WIDTH - 40, DISPLAY_HEIGHT - 105);
    lv_obj_align(card, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* --- WiFi Status --- */
    wifi_status = lv_label_create(card);
    lv_obj_set_style_text_font(wifi_status, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(wifi_status, COLOR_MUTED, 0);
    lv_label_set_text(wifi_status, LV_SYMBOL_WIFI " Wi-Fi: Offline");
    lv_obj_align(wifi_status, LV_ALIGN_TOP_LEFT, 10, 10);

    /* --- Button row 1: Scan WiFi | OTA Update | Calibration --- */
    lv_obj_t *btn_row1 = lv_obj_create(card);
    lv_obj_remove_style_all(btn_row1);
    lv_obj_set_size(btn_row1, lv_pct(100), 85);
    lv_obj_align(btn_row1, LV_ALIGN_TOP_LEFT, 0, 55);
    lv_obj_set_flex_flow(btn_row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row1, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(btn_row1, 15, 0);
    lv_obj_set_style_pad_left(btn_row1, 10, 0);

    lv_obj_t *scan_btn = lv_btn_create(btn_row1);
    lv_obj_add_style(scan_btn, &g_styles.btn_primary, 0);
    lv_obj_set_size(scan_btn, 250, 70);
    lv_obj_add_event_cb(scan_btn, scan_cb, LV_EVENT_RELEASED, NULL);
    lv_label_set_text(lv_label_create(scan_btn), LV_SYMBOL_WIFI " Scan Wi-Fi");

    lv_obj_t *ota_btn = lv_btn_create(btn_row1);
    lv_obj_add_style(ota_btn, &g_styles.btn_action, 0);
    lv_obj_set_size(ota_btn, 250, 70);
    lv_obj_add_event_cb(ota_btn, ota_cb, LV_EVENT_RELEASED, NULL);
    lv_label_set_text(lv_label_create(ota_btn), LV_SYMBOL_DOWNLOAD " OTA Update");

    lv_obj_t *cal_btn = lv_btn_create(btn_row1);
    lv_obj_add_style(cal_btn, &g_styles.btn_warning, 0);
    lv_obj_set_size(cal_btn, 250, 70);
    lv_obj_add_event_cb(cal_btn, [](lv_event_t *e){
        if (calibration_cb) calibration_cb();
    }, LV_EVENT_RELEASED, NULL);
    lv_label_set_text(lv_label_create(cal_btn), LV_SYMBOL_SETTINGS " Calibration");

lv_timer_create([](lv_timer_t *t){

    if(!ota_requested) return;

    ota_requested = false;

    if(ota_status_label && lv_obj_is_valid(ota_status_label))
    {
        lv_label_set_text(ota_status_label, "OTA: Checking...");
        lv_refr_now(NULL);
    }

    xTaskCreate(
        [](void *param){
            ota_service_check_and_update();
            vTaskDelete(NULL);
        },
        "ota_task",
        8192,
        NULL,
        1,
        NULL
    );

}, 300, NULL);

    /* --- OTA Status --- */
    ota_status_label = lv_label_create(card);
    lv_obj_set_style_text_font(ota_status_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(ota_status_label, COLOR_MUTED, 0);
    lv_label_set_text(ota_status_label, "OTA Status: Idle");
    lv_obj_align(ota_status_label, LV_ALIGN_TOP_LEFT, 10, 150);
    lv_obj_set_width(ota_status_label, 700);

    /* ✅ THREAD SAFE CALLBACK */
    ota_service_set_display_callback([](const String &msg){
        char *msg_copy = strdup(msg.c_str());
        lv_async_call([](void *data){
            const char *text = (const char *)data;
            if(ota_status_label && lv_obj_is_valid(ota_status_label))
            {
                lv_label_set_text(ota_status_label, text);
                lv_refr_now(NULL);
            }
            free(data);
        }, msg_copy);
    });

    /* --- Device Info row --- */
    lv_obj_t *dev_row = lv_obj_create(card);
    lv_obj_remove_style_all(dev_row);
    lv_obj_set_size(dev_row, lv_pct(100), 75);
    lv_obj_align(dev_row, LV_ALIGN_TOP_LEFT, 0, 180);
    lv_obj_set_flex_flow(dev_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dev_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(dev_row, 15, 0);
    lv_obj_set_style_pad_left(dev_row, 10, 0);

    lv_obj_t *dev_icon = lv_label_create(dev_row);
    lv_obj_set_style_text_font(dev_icon, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(dev_icon, COLOR_MUTED, 0);
    lv_label_set_text(dev_icon, LV_SYMBOL_HOME "  Device:");

    device_name_label = lv_label_create(dev_row);
    lv_obj_set_style_text_font(device_name_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(device_name_label, lv_color_hex(0x38BDF8), 0);
    {
        char dname[33] = {0};
        if(storage_load_device_name(dname, sizeof(dname)))
            lv_label_set_text(device_name_label, dname);
        else
            lv_label_set_text(device_name_label, "(not set)");
    }

    lv_obj_t *edit_btn = lv_btn_create(dev_row);
    lv_obj_add_style(edit_btn, &g_styles.btn_primary, 0);
    lv_obj_set_size(edit_btn, 180, 60);
    lv_obj_add_event_cb(edit_btn, [](lv_event_t *e) {
        /* Navigate to device name screen */
        lv_obj_t *dn_scr = lv_obj_create(NULL);
        device_name_screen_create(dn_scr);
        device_name_screen_register_callback([](int evt, const char *name) {
            if(evt == DEVNAME_EVT_SAVE && name) {
                storage_save_device_name(name);
                /* Capture current screen (device name) before switching */
                lv_obj_t *old_scr = lv_scr_act();
                /* Return to settings */
                if(settings_scr_ref && lv_obj_is_valid(settings_scr_ref)) {
                    lv_scr_load(settings_scr_ref);
                    /* Update the label */
                    if(device_name_label && lv_obj_is_valid(device_name_label))
                        lv_label_set_text(device_name_label, name);
                }
                if(old_scr) lv_obj_del_async(old_scr);
            }
        });
        lv_scr_load(dn_scr);
    }, LV_EVENT_RELEASED, NULL);
    lv_obj_t *edit_lbl = lv_label_create(edit_btn);
    lv_obj_set_style_text_font(edit_lbl, &lv_font_montserrat_16, 0);
    lv_label_set_text(edit_lbl, LV_SYMBOL_EDIT " EDIT NAME");
    lv_obj_center(edit_lbl);

    /* --- Button row 2: Developer Mode switch | Clear Logs --- */
    lv_obj_t *btn_row2 = lv_obj_create(card);
    lv_obj_remove_style_all(btn_row2);
    lv_obj_set_size(btn_row2, lv_pct(100), 75);
    lv_obj_align(btn_row2, LV_ALIGN_TOP_LEFT, 0, 255);
    lv_obj_set_flex_flow(btn_row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row2, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(btn_row2, 20, 0);
    lv_obj_set_style_pad_left(btn_row2, 10, 0);

    lv_obj_t *dev_lbl = lv_label_create(btn_row2);
    lv_obj_set_style_text_font(dev_lbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(dev_lbl, COLOR_TEXT, 0);
    lv_label_set_text(dev_lbl, "Developer Mode");

    dev_mode_sw = lv_switch_create(btn_row2);
    lv_obj_set_size(dev_mode_sw, 70, 40);
    bool was = storage_load_dev_mode();
    if(was) lv_obj_add_state(dev_mode_sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(dev_mode_sw, dev_mode_changed, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *clear_logs_btn = lv_btn_create(btn_row2);
    lv_obj_add_style(clear_logs_btn, &g_styles.btn_danger, 0);
    lv_obj_set_size(clear_logs_btn, 220, 65);
    lv_obj_add_event_cb(clear_logs_btn, clear_logs_cb, LV_EVENT_RELEASED, NULL);
    lv_label_set_text(lv_label_create(clear_logs_btn), LV_SYMBOL_TRASH " Clear Logs");

    /* WiFi List Screen */
    wifi_list_scr = lv_obj_create(NULL);
    wifi_list_screen_create(wifi_list_scr);
    wifi_list_screen_register_back(wifi_list_back);
    wifi_list_screen_register_select(wifi_selected);

    /* Log textarea */
    dev_log_ta = lv_textarea_create(card);
    lv_obj_set_size(dev_log_ta, lv_pct(95), 200);
    lv_obj_align(dev_log_ta, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_color(dev_log_ta, lv_color_hex(0x0C1222), 0);
    lv_obj_set_style_text_color(dev_log_ta, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_text_font(dev_log_ta, &lv_font_montserrat_14, 0);
    lv_obj_set_style_border_color(dev_log_ta, lv_color_hex(0x334155), 0);

    /* Load logs from storage on startup */
    if(was) devlog_load_from_storage();
    lv_textarea_set_text(dev_log_ta, was ? devlog_get_text().c_str() : "Developer mode off");

    if(was)
    {
        devlog_timer = lv_timer_create(
            [](lv_timer_t *t){
                if(dev_log_ta && lv_obj_is_valid(dev_log_ta))
                    lv_textarea_set_text(dev_log_ta, devlog_get_text().c_str());
            },
            1000,
            NULL
        );
    }

    wifi_service_register_state_callback(wifi_state_cb);

    /* Set correct WiFi status immediately (may already be connected) */
    settings_screen_update_wifi_status();
}

/* ================= STATUS UPDATE ================= */

void settings_screen_update_wifi_status(void)
{
    if(!wifi_status || !lv_obj_is_valid(wifi_status)) return;

    wifi_state_t state = wifi_service_state();

    if(state == WIFI_CONNECTING)
    {
        lv_label_set_text(wifi_status, LV_SYMBOL_WIFI " Wi-Fi: Connecting...");
        show_connecting_overlay();
    }
    else if(state == WIFI_CONNECTED)
    {
        lv_label_set_text(wifi_status, LV_SYMBOL_WIFI " Wi-Fi: Connected");
        hide_connecting_overlay();
    }
    else
    {
        lv_label_set_text(wifi_status, LV_SYMBOL_WIFI " Wi-Fi: Offline");
        hide_connecting_overlay();
    }
}

void settings_screen_show(void)
{
    if(settings_scr_ref)
    {
        settings_screen_update_wifi_status();
        lv_scr_load(settings_scr_ref);
    }
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // LV_VERSION_MAJOR
