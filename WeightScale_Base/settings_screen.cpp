#include <lvgl.h>

// ✅ Ensure LVGL is detected
#ifndef LV_VERSION_MAJOR
#define LV_VERSION_MAJOR 8
#endif

#include "config/app_config.h"
#include "app/app_controller.h"
#include "settings_screen.h"
#include "ui_styles.h"
#include "wifi_service.h"
#include "ota_service.h"
#include "wifi_list_screen.h"
#include "storage_service.h"
#include "device_name_screen.h"
#include "devlog.h"
#include "sync_service.h"
#include <stdio.h>
#include <string.h>

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
static lv_obj_t *ota_btn_ref = NULL;
static lv_obj_t *device_name_label = NULL;
static lv_obj_t *device_id_label = NULL;
static lv_obj_t *offset_lbl = NULL;
static lv_obj_t *env_mode_sw = NULL;
static lv_obj_t *env_mode_lbl = NULL;
static lv_obj_t *time_label = NULL;
static lv_timer_t *time_refresh_timer = NULL;

static void settings_screen_refresh_time_label()
{
    if(!time_label || !lv_obj_is_valid(time_label)) return;
    char buf[32];
    app_controller_get_time_text(buf, sizeof(buf));
    lv_label_set_text(time_label, buf);
}
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

/* ================= OTA FULLSCREEN OVERLAY ================= */

static lv_obj_t *ota_overlay = NULL;
static lv_obj_t *ota_overlay_label = NULL;
static lv_obj_t *ota_overlay_bar = NULL;
static lv_obj_t *ota_overlay_pct = NULL;

static void ota_ui_reset_after_attempt(void)
{
    if(ota_btn_ref && lv_obj_is_valid(ota_btn_ref))
        lv_obj_clear_state(ota_btn_ref, LV_STATE_DISABLED);
    ota_requested = false;
}

static void show_ota_overlay(const char *msg)
{
    if (ota_overlay) return;
    lv_obj_t *scr = lv_scr_act();
    ota_overlay = lv_obj_create(scr);
    lv_obj_remove_style_all(ota_overlay);
    lv_obj_set_size(ota_overlay, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(ota_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ota_overlay, LV_OPA_90, 0);
    lv_obj_add_flag(ota_overlay, LV_OBJ_FLAG_CLICKABLE); /* block touches */
    lv_obj_move_foreground(ota_overlay);

    /* Title */
    ota_overlay_label = lv_label_create(ota_overlay);
    lv_obj_set_style_text_color(ota_overlay_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(ota_overlay_label, &lv_font_montserrat_28, 0);
    lv_label_set_text(ota_overlay_label, msg);
    lv_obj_align(ota_overlay_label, LV_ALIGN_CENTER, 0, -60);

    /* Progress bar */
    ota_overlay_bar = lv_bar_create(ota_overlay);
    lv_obj_set_size(ota_overlay_bar, 500, 30);
    lv_bar_set_range(ota_overlay_bar, 0, 100);
    lv_bar_set_value(ota_overlay_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ota_overlay_bar, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_color(ota_overlay_bar, lv_color_hex(0x22D3EE), LV_PART_INDICATOR);
    lv_obj_set_style_radius(ota_overlay_bar, 8, 0);
    lv_obj_set_style_radius(ota_overlay_bar, 8, LV_PART_INDICATOR);
    lv_obj_align(ota_overlay_bar, LV_ALIGN_CENTER, 0, 0);

    /* Percent label */
    ota_overlay_pct = lv_label_create(ota_overlay);
    lv_obj_set_style_text_color(ota_overlay_pct, lv_color_white(), 0);
    lv_obj_set_style_text_font(ota_overlay_pct, &lv_font_montserrat_20, 0);
    lv_label_set_text(ota_overlay_pct, "0%");
    lv_obj_align(ota_overlay_pct, LV_ALIGN_CENTER, 0, 35);
}

static void update_ota_overlay_msg(const char *msg)
{
    if (ota_overlay_label && lv_obj_is_valid(ota_overlay_label))
        lv_label_set_text(ota_overlay_label, msg);
}

static void update_ota_overlay_progress(int pct)
{
    if (ota_overlay_bar && lv_obj_is_valid(ota_overlay_bar))
        lv_bar_set_value(ota_overlay_bar, pct, LV_ANIM_OFF);
    if (ota_overlay_pct && lv_obj_is_valid(ota_overlay_pct)) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", pct);
        lv_label_set_text(ota_overlay_pct, buf);
    }
}

static void hide_ota_overlay(void)
{
    if (ota_overlay && lv_obj_is_valid(ota_overlay))
        lv_obj_del(ota_overlay);
    ota_overlay = NULL;
    ota_overlay_label = NULL;
    ota_overlay_bar = NULL;
    ota_overlay_pct = NULL;
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
    ota_btn_ref = lv_event_get_target(e);
    lv_obj_add_state(ota_btn_ref, LV_STATE_DISABLED);

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
static lv_obj_t *theme_sw = NULL;
static lv_timer_t *devlog_timer = NULL;

static void dev_mode_changed(lv_event_t *e)
{
    if(!dev_mode_sw) return;
    bool on = lv_obj_has_state(dev_mode_sw, LV_STATE_CHECKED);
    storage_save_dev_mode(on);
    if(on)
    {
        if(dev_log_ta && lv_obj_is_valid(dev_log_ta)) {
            lv_obj_clear_flag(dev_log_ta, LV_OBJ_FLAG_HIDDEN);
            lv_textarea_set_text(dev_log_ta, devlog_get_text().c_str());
        }
        if(!devlog_timer) {
            devlog_timer = lv_timer_create(
                [](lv_timer_t *t){
                    if(dev_log_ta && lv_obj_is_valid(dev_log_ta))
                        lv_textarea_set_text(dev_log_ta, devlog_get_text().c_str());
                },
                1000, NULL
            );
        }
    }
    else
    {
        if(dev_log_ta && lv_obj_is_valid(dev_log_ta))
            lv_obj_add_flag(dev_log_ta, LV_OBJ_FLAG_HIDDEN);
        if(devlog_timer) {
            lv_timer_del(devlog_timer);
            devlog_timer = NULL;
        }
    }
}

static void clear_logs_cb(lv_event_t *e)
{
    devlog_clear();
    if(dev_log_ta && lv_obj_is_valid(dev_log_ta))
        lv_textarea_set_text(dev_log_ta, "[Logs cleared]");
    devlog_printf("[SETTINGS] Logs cleared by user");
}

static void clear_all_confirm_cb(lv_event_t *e)
{
    lv_obj_t *overlay = (lv_obj_t *)lv_event_get_user_data(e);
    app_controller_clear_all_data();
    if(overlay && lv_obj_is_valid(overlay))
        lv_obj_del(overlay);
}

static void show_clear_all_confirm_popup(void)
{
    lv_obj_t *overlay = lv_obj_create(settings_scr_ref ? settings_scr_ref : lv_scr_act());
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_60, 0);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *card = lv_obj_create(overlay);
    lv_obj_set_size(card, 460, 220);
    lv_obj_center(card);
    lv_obj_add_style(card, &g_styles.card, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(card);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xF97316), 0);
    lv_label_set_text(title, LV_SYMBOL_WARNING " Clear all data?");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *msg = lv_label_create(card);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(msg, ui_theme_text(), 0);
    lv_label_set_text(msg, "This will reset invoice number and erase all saved records.");
    lv_obj_set_width(msg, 400);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t *cancel_btn = lv_btn_create(card);
    lv_obj_add_style(cancel_btn, &g_styles.btn_secondary, 0);
    lv_obj_set_size(cancel_btn, 150, 60);
    lv_obj_align(cancel_btn, LV_ALIGN_BOTTOM_LEFT, 35, -20);
    lv_obj_add_event_cb(cancel_btn, [](lv_event_t *e){
        lv_obj_t *ov = (lv_obj_t *)lv_event_get_user_data(e);
        if(ov && lv_obj_is_valid(ov)) lv_obj_del(ov);
    }, LV_EVENT_RELEASED, overlay);
    lv_label_set_text(lv_label_create(cancel_btn), "Cancel");

    lv_obj_t *confirm_btn = lv_btn_create(card);
    lv_obj_add_style(confirm_btn, &g_styles.btn_danger, 0);
    lv_obj_set_size(confirm_btn, 150, 60);
    lv_obj_align(confirm_btn, LV_ALIGN_BOTTOM_RIGHT, -35, -20);
    lv_obj_add_event_cb(confirm_btn, clear_all_confirm_cb, LV_EVENT_RELEASED, overlay);
    lv_label_set_text(lv_label_create(confirm_btn), LV_SYMBOL_TRASH " Clear");
}

static void show_set_time_popup(void)
{
    lv_obj_t *overlay = lv_obj_create(settings_scr_ref ? settings_scr_ref : lv_scr_act());
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_70, 0);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *card = lv_obj_create(overlay);
    lv_obj_set_size(card, 560, 470);
    lv_obj_center(card);
    lv_obj_add_style(card, &g_styles.card, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(card);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, ui_theme_accent(), 0);
    lv_label_set_text(title, LV_SYMBOL_EDIT " Set Date & Time");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 14);

    lv_obj_t *hint = lv_label_create(card);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(hint, ui_theme_muted(), 0);
    lv_label_set_text(hint, "Enter 12 digits: YYYYMMDDHHMM");
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 60);

    lv_obj_t *ta = lv_textarea_create(card);
    lv_obj_set_size(ta, 470, 65);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 95);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, 12);
    lv_textarea_set_placeholder_text(ta, "202604141200");
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_28, 0);
    {
        char current_time[32];
        app_controller_get_time_text(current_time, sizeof(current_time));
        if(strcmp(current_time, "Time not set") != 0) {
            int year = 0, month = 0, day = 0, hour = 0, minute = 0;
            if(sscanf(current_time, "%d-%d-%d %d:%d",
                      &year, &month, &day, &hour, &minute) == 5) {
                char compact[16];
                snprintf(compact, sizeof(compact), "%04d%02d%02d%02d%02d",
                         year, month, day, hour, minute);
                lv_textarea_set_text(ta, compact);
            }
        }
    }

    lv_obj_t *err_lbl = lv_label_create(card);
    lv_obj_set_style_text_font(err_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(err_lbl, lv_color_hex(0xEF4444), 0);
    lv_label_set_text(err_lbl, "");
    lv_obj_align(err_lbl, LV_ALIGN_TOP_MID, 0, 170);

    struct time_popup_ctx {
        lv_obj_t *overlay;
        lv_obj_t *ta;
        lv_obj_t *err_lbl;
    };
    time_popup_ctx *ctx = new time_popup_ctx{overlay, ta, err_lbl};

    lv_obj_t *kb = lv_keyboard_create(card);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_set_size(kb, 520, 220);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_style(kb, &g_styles.kb_bg, LV_PART_MAIN);
    lv_obj_add_style(kb, &g_styles.kb_btn, LV_PART_ITEMS);

    lv_obj_add_event_cb(kb, [](lv_event_t *e){
        time_popup_ctx *c = (time_popup_ctx *)lv_event_get_user_data(e);
        if(!c) return;
        const char *text = lv_textarea_get_text(c->ta);
        int year = 0, month = 0, day = 0, hour = 0, minute = 0;
        if(strlen(text) == 12 &&
           sscanf(text, "%4d%2d%2d%2d%2d", &year, &month, &day, &hour, &minute) == 5 &&
           app_controller_set_datetime(year, month, day, hour, minute)) {
            settings_screen_refresh_time_label();
            lv_obj_t *ov = c->overlay;
            delete c;
            lv_obj_del(ov);
            return;
        }
        lv_label_set_text(c->err_lbl, "Invalid date/time. Use 12 digits: YYYYMMDDHHMM");
    }, LV_EVENT_READY, ctx);

    lv_obj_add_event_cb(kb, [](lv_event_t *e){
        time_popup_ctx *c = (time_popup_ctx *)lv_event_get_user_data(e);
        if(!c) return;
        lv_obj_t *ov = c->overlay;
        delete c;
        lv_obj_del(ov);
    }, LV_EVENT_CANCEL, ctx);
}

#if ENABLE_CLOUD_SYNC
static void sync_env_changed(lv_event_t *e)
{
    bool is_prod = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    sync_service_set_env(is_prod);

    if(env_mode_lbl && lv_obj_is_valid(env_mode_lbl))
        lv_label_set_text(env_mode_lbl, is_prod ? "Prod API" : "Dev API");
}
#endif

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
        ota_btn_ref = NULL;
        device_name_label = NULL;
        device_id_label = NULL;
        dev_log_ta = NULL;
        dev_mode_sw = NULL;
        theme_sw = NULL;
        env_mode_sw = NULL;
        env_mode_lbl = NULL;
        time_label = NULL;
        wifi_list_scr = NULL;
        offset_lbl = NULL;
        ota_overlay = NULL;
        ota_overlay_label = NULL;
        ota_overlay_bar = NULL;
        ota_overlay_pct = NULL;
        if(devlog_timer) { lv_timer_del(devlog_timer); devlog_timer = NULL; }
        if(time_refresh_timer) { lv_timer_del(time_refresh_timer); time_refresh_timer = NULL; }
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
    lv_obj_set_scroll_dir(card, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_AUTO);

    /* --- WiFi Status --- */
    wifi_status = lv_label_create(card);
    lv_obj_set_style_text_font(wifi_status, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(wifi_status, ui_theme_muted(), 0);
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
    ota_btn_ref = ota_btn;
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

    if(ota_status_label && lv_obj_is_valid(ota_status_label))
    {
        lv_label_set_text(ota_status_label, "OTA: Checking...");
        lv_refr_now(NULL);
    }

    show_ota_overlay("Checking for update...");

    devlog_printf("[OTA] Starting OTA check task...");

    /* Suspend scale service during OTA to reduce SDIO bus contention */
    scale_service_suspend();

    xTaskCreatePinnedToCore(
        [](void *param){
            ota_service_check_and_update();
            /* Resume scale service after OTA completes (unless rebooting) */
            scale_service_resume();
            /* Hide overlay on task end (failure/no-update path) */
            lv_async_call([](void *d){ hide_ota_overlay(); }, NULL);
            lv_async_call([](void *d){ ota_ui_reset_after_attempt(); }, NULL);
            vTaskDelete(NULL);
        },
        "ota_task",
        16384,
        NULL,
        3,       // prio 3 for OTA safe mode
        NULL,
        0        // pin to Core 0 — Core 1 used by scale/LVGL
    );

}, 300, NULL);

    /* --- OTA Status --- */
    ota_status_label = lv_label_create(card);
    lv_obj_set_style_text_font(ota_status_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(ota_status_label, ui_theme_muted(), 0);
    {
        String ver = ota_service_stored_version();
        if(ver.length() == 0) ver = ota_service_current_version();
        String status_text = "Firmware: v" + ver;
        lv_label_set_text(ota_status_label, status_text.c_str());
    }
    lv_obj_align(ota_status_label, LV_ALIGN_TOP_LEFT, 10, 150);
    lv_obj_set_width(ota_status_label, 700);

    /* ✅ THREAD SAFE CALLBACK */
    ota_service_set_display_callback([](const String &msg){
        devlog_printf("[OTA] %s", msg.c_str());
        char *msg_copy = strdup(msg.c_str());
        lv_async_call([](void *data){
            const char *text = (const char *)data;
            if(ota_status_label && lv_obj_is_valid(ota_status_label))
            {
                lv_label_set_text(ota_status_label, text);
            }
            update_ota_overlay_msg(text);
            /* Hide overlay on final states (failure) */
            if(strstr(text, "failed") || strstr(text, "Already")
               || strstr(text, "Latest") || strstr(text, "offline")
               || strstr(text, "busy") || strstr(text, "Wait for Wi-Fi")
               || strstr(text, "No space") || strstr(text, "Bad file size")
               || strstr(text, "Download incomplete")
               || strstr(text, "Finalize failed")) {
                hide_ota_overlay();
            }
            free(data);
        }, msg_copy);
    });

    /* ✅ THREAD SAFE PROGRESS CALLBACK */
    ota_service_set_progress_callback([](int pct){
        int *p = (int *)malloc(sizeof(int));
        if(p) {
            *p = pct;
            lv_async_call([](void *data){
                int percent = *(int *)data;
                update_ota_overlay_progress(percent);
                free(data);
            }, p);
        }
    });

    /* --- Device Info row --- */
    lv_obj_t *dev_row = lv_obj_create(card);
    lv_obj_remove_style_all(dev_row);
    lv_obj_set_size(dev_row, lv_pct(100), 75);
    lv_obj_align(dev_row, LV_ALIGN_TOP_LEFT, 0, 180);
    lv_obj_set_flex_flow(dev_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dev_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(dev_row, 12, 0);
    lv_obj_set_style_pad_left(dev_row, 10, 0);

    lv_obj_t *dev_icon = lv_label_create(dev_row);
    lv_obj_set_style_text_font(dev_icon, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(dev_icon, ui_theme_muted(), 0);
    lv_label_set_text(dev_icon, LV_SYMBOL_HOME "  ID:");

    device_id_label = lv_label_create(dev_row);
    lv_obj_set_style_text_font(device_id_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(device_id_label, ui_theme_accent(), 0);
    {
        uint32_t did = storage_load_device_id();
        char id_buf[16];
        snprintf(id_buf, sizeof(id_buf), "%lu", did);
        lv_label_set_text(device_id_label, id_buf);
    }

    lv_obj_t *sep_lbl = lv_label_create(dev_row);
    lv_obj_set_style_text_font(sep_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(sep_lbl, ui_theme_muted(), 0);
    lv_label_set_text(sep_lbl, "|  Name:");

    device_name_label = lv_label_create(dev_row);
    lv_obj_set_style_text_font(device_name_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(device_name_label, ui_theme_accent(), 0);
    {
        char dname[33] = {0};
        if(storage_load_device_name(dname, sizeof(dname)))
            lv_label_set_text(device_name_label, dname);
        else
            lv_label_set_text(device_name_label, "(not set)");
    }

    lv_obj_t *edit_btn = lv_btn_create(dev_row);
    lv_obj_add_style(edit_btn, &g_styles.btn_primary, 0);
    lv_obj_set_size(edit_btn, 160, 60);
    lv_obj_add_event_cb(edit_btn, [](lv_event_t *e) {
        /* Navigate to device config screen */
        lv_obj_t *dn_scr = lv_obj_create(NULL);
        device_name_screen_create(dn_scr);

        /* Pre-fill current values */
        {
            char cur_name[33] = {0};
            storage_load_device_name(cur_name, sizeof(cur_name));
            uint32_t cur_id = storage_load_device_id();
            device_name_screen_set_values(cur_name, cur_id);
        }

        device_name_screen_register_callback([](int evt, const char *name, uint32_t dev_id) {
            if(evt == DEVNAME_EVT_SAVE && name) {
                storage_save_device_name(name);
                storage_save_device_id(dev_id);
                /* Capture current screen before switching */
                lv_obj_t *old_scr = lv_scr_act();
                /* Return to settings */
                if(settings_scr_ref && lv_obj_is_valid(settings_scr_ref)) {
                    lv_scr_load(settings_scr_ref);
                    /* Update labels */
                    if(device_name_label && lv_obj_is_valid(device_name_label))
                        lv_label_set_text(device_name_label, name);
                    if(device_id_label && lv_obj_is_valid(device_id_label)) {
                        char id_buf[16];
                        snprintf(id_buf, sizeof(id_buf), "%lu", dev_id);
                        lv_label_set_text(device_id_label, id_buf);
                    }
                }
                if(old_scr) lv_obj_del_async(old_scr);

                /* Update home screen device label */
                extern void home_screen_set_device(const char *name);
                home_screen_set_device(name);
            }
        });
        lv_scr_load(dn_scr);
    }, LV_EVENT_RELEASED, NULL);
    lv_obj_t *edit_lbl = lv_label_create(edit_btn);
    lv_obj_set_style_text_font(edit_lbl, &lv_font_montserrat_16, 0);
    lv_label_set_text(edit_lbl, LV_SYMBOL_EDIT " EDIT");
    lv_obj_center(edit_lbl);

    /* --- Appearance row: Light Mode toggle --- */
    lv_obj_t *appearance_row = lv_obj_create(card);
    lv_obj_remove_style_all(appearance_row);
    lv_obj_set_size(appearance_row, lv_pct(100), 75);
    lv_obj_align(appearance_row, LV_ALIGN_TOP_LEFT, 0, 255);
    lv_obj_set_flex_flow(appearance_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(appearance_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(appearance_row, 20, 0);
    lv_obj_set_style_pad_left(appearance_row, 10, 0);

    lv_obj_t *theme_lbl = lv_label_create(appearance_row);
    lv_obj_set_style_text_font(theme_lbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(theme_lbl, ui_theme_text(), 0);
    lv_label_set_text(theme_lbl, LV_SYMBOL_IMAGE "  Light Mode");

    theme_sw = lv_switch_create(appearance_row);
    lv_obj_set_size(theme_sw, 70, 40);
    lv_obj_set_style_bg_color(theme_sw, lv_color_hex(0x334155), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(theme_sw, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(theme_sw, lv_color_hex(0xF59E0B), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(theme_sw, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(theme_sw, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    if(storage_load_light_mode()) lv_obj_add_state(theme_sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(theme_sw, [](lv_event_t *e){
        bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
        storage_save_light_mode(on);
        ui_styles_set_theme(on);
        /* Navigate back to home immediately so both home AND settings
           screens are recreated with the new theme colors on next open. */
        if(back_cb) back_cb();
    }, LV_EVENT_VALUE_CHANGED, NULL);

    /* --- Time row: current time | set time | clear all --- */
    lv_obj_t *time_row = lv_obj_create(card);
    lv_obj_remove_style_all(time_row);
    lv_obj_set_size(time_row, lv_pct(100), 75);
    lv_obj_align(time_row, LV_ALIGN_TOP_LEFT, 0, 330);
    lv_obj_set_flex_flow(time_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(time_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(time_row, 14, 0);
    lv_obj_set_style_pad_left(time_row, 10, 0);

    lv_obj_t *time_title = lv_label_create(time_row);
    lv_obj_set_style_text_font(time_title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(time_title, ui_theme_text(), 0);
    lv_label_set_text(time_title, LV_SYMBOL_BELL " Time");

    time_label = lv_label_create(time_row);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(time_label, ui_theme_accent(), 0);
    settings_screen_refresh_time_label();

    lv_obj_t *set_time_btn = lv_btn_create(time_row);
    lv_obj_add_style(set_time_btn, &g_styles.btn_primary, 0);
    lv_obj_set_size(set_time_btn, 190, 60);
    lv_obj_add_event_cb(set_time_btn, [](lv_event_t *e){
        (void)e;
        show_set_time_popup();
    }, LV_EVENT_RELEASED, NULL);
    lv_label_set_text(lv_label_create(set_time_btn), LV_SYMBOL_EDIT " Set Time");

    lv_obj_t *clear_all_btn = lv_btn_create(time_row);
    lv_obj_add_style(clear_all_btn, &g_styles.btn_danger, 0);
    lv_obj_set_size(clear_all_btn, 210, 60);
    lv_obj_add_event_cb(clear_all_btn, [](lv_event_t *e){
        (void)e;
        show_clear_all_confirm_popup();
    }, LV_EVENT_RELEASED, NULL);
    lv_label_set_text(lv_label_create(clear_all_btn), LV_SYMBOL_TRASH " Clear All");

    /* --- Sync environment row: Dev/Prod API toggle --- */
    lv_obj_t *sync_env_row = lv_obj_create(card);
    lv_obj_remove_style_all(sync_env_row);
    lv_obj_set_size(sync_env_row, lv_pct(100), 75);
    lv_obj_align(sync_env_row, LV_ALIGN_TOP_LEFT, 0, 405);
    lv_obj_set_flex_flow(sync_env_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sync_env_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(sync_env_row, 20, 0);
    lv_obj_set_style_pad_left(sync_env_row, 10, 0);

    lv_obj_t *sync_env_title = lv_label_create(sync_env_row);
    lv_obj_set_style_text_font(sync_env_title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(sync_env_title, ui_theme_text(), 0);
    lv_label_set_text(sync_env_title, "Sync Endpoint");

#if ENABLE_CLOUD_SYNC
    env_mode_sw = lv_switch_create(sync_env_row);
    lv_obj_set_size(env_mode_sw, 70, 40);
    lv_obj_set_style_bg_color(env_mode_sw, lv_color_hex(0x334155), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(env_mode_sw, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(env_mode_sw, lv_color_hex(0x16A34A), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(env_mode_sw, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(env_mode_sw, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    if(sync_service_is_prod())
        lv_obj_add_state(env_mode_sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(env_mode_sw, sync_env_changed, LV_EVENT_VALUE_CHANGED, NULL);

    env_mode_lbl = lv_label_create(sync_env_row);
    lv_obj_set_style_text_font(env_mode_lbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(env_mode_lbl, ui_theme_muted(), 0);
    lv_label_set_text(env_mode_lbl, sync_service_is_prod() ? "Prod API" : "Dev API");
#else
    env_mode_lbl = lv_label_create(sync_env_row);
    lv_obj_set_style_text_font(env_mode_lbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(env_mode_lbl, ui_theme_muted(), 0);
    lv_label_set_text(env_mode_lbl, "Sync disabled");
#endif

    /* --- Button row 2: Developer Mode switch | Clear Logs --- */
    lv_obj_t *btn_row2 = lv_obj_create(card);
    lv_obj_remove_style_all(btn_row2);
    lv_obj_set_size(btn_row2, lv_pct(100), 75);
    lv_obj_align(btn_row2, LV_ALIGN_TOP_LEFT, 0, 480);
    lv_obj_set_flex_flow(btn_row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row2, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(btn_row2, 20, 0);
    lv_obj_set_style_pad_left(btn_row2, 10, 0);

    lv_obj_t *dev_lbl = lv_label_create(btn_row2);
    lv_obj_set_style_text_font(dev_lbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(dev_lbl, ui_theme_text(), 0);
    lv_label_set_text(dev_lbl, "Developer Mode");

    dev_mode_sw = lv_switch_create(btn_row2);
    lv_obj_set_size(dev_mode_sw, 70, 40);
    /* Dark switch styling */
    lv_obj_set_style_bg_color(dev_mode_sw, lv_color_hex(0x334155), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dev_mode_sw, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(dev_mode_sw, lv_color_hex(0x2563EB), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(dev_mode_sw, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(dev_mode_sw, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    bool was = storage_load_dev_mode();
    if(was) lv_obj_add_state(dev_mode_sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(dev_mode_sw, dev_mode_changed, LV_EVENT_VALUE_CHANGED, NULL);

    /* --- Separator --- */
    lv_obj_t *sep2 = lv_label_create(btn_row2);
    lv_obj_set_style_text_font(sep2, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(sep2, ui_theme_muted(), 0);
    lv_label_set_text(sep2, "|");

    /* --- Enable Test toggle --- */
    lv_obj_t *test_lbl = lv_label_create(btn_row2);
    lv_obj_set_style_text_font(test_lbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(test_lbl, ui_theme_text(), 0);
    lv_label_set_text(test_lbl, "Enable Test");

    lv_obj_t *test_sw = lv_switch_create(btn_row2);
    lv_obj_set_size(test_sw, 70, 40);
    lv_obj_set_style_bg_color(test_sw, lv_color_hex(0x334155), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(test_sw, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(test_sw, lv_color_hex(0xD97706), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(test_sw, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(test_sw, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    {
        extern bool app_controller_is_test_mode(void);
        if(app_controller_is_test_mode())
            lv_obj_add_state(test_sw, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(test_sw, [](lv_event_t *e){
        lv_obj_t *sw = lv_event_get_target(e);
        bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
        extern void app_controller_set_test_mode(bool on);
        app_controller_set_test_mode(on);
    }, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *clear_logs_btn = lv_btn_create(btn_row2);
    lv_obj_add_style(clear_logs_btn, &g_styles.btn_danger, 0);
    lv_obj_set_size(clear_logs_btn, 220, 65);
    lv_obj_add_event_cb(clear_logs_btn, clear_logs_cb, LV_EVENT_RELEASED, NULL);
    lv_label_set_text(lv_label_create(clear_logs_btn), LV_SYMBOL_TRASH " Clear Logs");

    /* --- Manual Weight Offset row --- */

    lv_obj_t *offset_row = lv_obj_create(card);
    lv_obj_remove_style_all(offset_row);
    lv_obj_set_size(offset_row, lv_pct(100), 75);
    lv_obj_align(offset_row, LV_ALIGN_TOP_LEFT, 0, 555);
    lv_obj_set_flex_flow(offset_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(offset_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(offset_row, 12, 0);
    lv_obj_set_style_pad_left(offset_row, 10, 0);

    lv_obj_t *off_title = lv_label_create(offset_row);
    lv_obj_set_style_text_font(off_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(off_title, ui_theme_text(), 0);
    lv_label_set_text(off_title, "Manual Offset (kg):");

    /* [-] button */
    lv_obj_t *off_minus = lv_btn_create(offset_row);
    lv_obj_add_style(off_minus, &g_styles.btn_danger, 0);
    lv_obj_set_size(off_minus, 65, 55);
    lv_obj_t *ml = lv_label_create(off_minus);
    lv_obj_set_style_text_font(ml, &lv_font_montserrat_28, 0);
    lv_label_set_text(ml, "-");
    lv_obj_center(ml);

    /* Offset value label */
    offset_lbl = lv_label_create(offset_row);
    lv_obj_set_style_text_font(offset_lbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(offset_lbl, ui_theme_accent(), 0);
    lv_obj_set_width(offset_lbl, 120);
    lv_obj_set_style_text_align(offset_lbl, LV_TEXT_ALIGN_CENTER, 0);
    {
        extern float app_controller_get_manual_offset(void);
        float cur = app_controller_get_manual_offset();
        char obuf[16];
        snprintf(obuf, sizeof(obuf), "%+.2f", cur);
        lv_label_set_text(offset_lbl, obuf);
    }

    /* [+] button */
    lv_obj_t *off_plus = lv_btn_create(offset_row);
    lv_obj_add_style(off_plus, &g_styles.btn_primary, 0);
    lv_obj_set_size(off_plus, 65, 55);
    lv_obj_t *pl = lv_label_create(off_plus);
    lv_obj_set_style_text_font(pl, &lv_font_montserrat_28, 0);
    lv_label_set_text(pl, "+");
    lv_obj_center(pl);

    /* Reset button */
    lv_obj_t *off_reset = lv_btn_create(offset_row);
    lv_obj_add_style(off_reset, &g_styles.btn_secondary, 0);
    lv_obj_set_size(off_reset, 100, 55);
    lv_obj_t *rl = lv_label_create(off_reset);
    lv_obj_set_style_text_font(rl, &lv_font_montserrat_16, 0);
    lv_label_set_text(rl, "RESET");
    lv_obj_center(rl);

    /* [-] callback: decrease by 0.05 */
    lv_obj_add_event_cb(off_minus, [](lv_event_t *e){
        extern float app_controller_get_manual_offset(void);
        extern void app_controller_set_manual_offset(float);
        float cur = app_controller_get_manual_offset() - 0.05f;
        app_controller_set_manual_offset(cur);
        char obuf[16];
        snprintf(obuf, sizeof(obuf), "%+.2f", cur);
        if(offset_lbl && lv_obj_is_valid(offset_lbl))
            lv_label_set_text(offset_lbl, obuf);
    }, LV_EVENT_RELEASED, NULL);

    /* [+] callback: increase by 0.05 */
    lv_obj_add_event_cb(off_plus, [](lv_event_t *e){
        extern float app_controller_get_manual_offset(void);
        extern void app_controller_set_manual_offset(float);
        float cur = app_controller_get_manual_offset() + 0.05f;
        app_controller_set_manual_offset(cur);
        char obuf[16];
        snprintf(obuf, sizeof(obuf), "%+.2f", cur);
        if(offset_lbl && lv_obj_is_valid(offset_lbl))
            lv_label_set_text(offset_lbl, obuf);
    }, LV_EVENT_RELEASED, NULL);

    /* Reset callback: set to 0 */
    lv_obj_add_event_cb(off_reset, [](lv_event_t *e){
        extern void app_controller_set_manual_offset(float);
        app_controller_set_manual_offset(0.0f);
        if(offset_lbl && lv_obj_is_valid(offset_lbl))
            lv_label_set_text(offset_lbl, "+0.00");
    }, LV_EVENT_RELEASED, NULL);

    /* WiFi List Screen */
    wifi_list_scr = lv_obj_create(NULL);
    wifi_list_screen_create(wifi_list_scr);
    wifi_list_screen_register_back(wifi_list_back);
    wifi_list_screen_register_select(wifi_selected);

    /* Log textarea */
    dev_log_ta = lv_textarea_create(card);
    lv_obj_set_size(dev_log_ta, lv_pct(95), 130);
    lv_obj_align(dev_log_ta, LV_ALIGN_TOP_MID, 0, 640);
    lv_obj_set_style_bg_color(dev_log_ta, ui_theme_surface(), 0);
    lv_obj_set_style_bg_opa(dev_log_ta, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(dev_log_ta, ui_theme_muted(), 0);
    lv_obj_set_style_text_font(dev_log_ta, &lv_font_montserrat_14, 0);
    lv_obj_set_style_border_color(dev_log_ta, ui_theme_border(), 0);

    /* Show/hide based on dev mode state */
    if(was) {
        lv_textarea_set_text(dev_log_ta, devlog_get_text().c_str());
        devlog_timer = lv_timer_create(
            [](lv_timer_t *t){
                if(dev_log_ta && lv_obj_is_valid(dev_log_ta))
                    lv_textarea_set_text(dev_log_ta, devlog_get_text().c_str());
            },
            1000, NULL
        );
    } else {
        lv_obj_add_flag(dev_log_ta, LV_OBJ_FLAG_HIDDEN);
    }

    wifi_service_register_state_callback(wifi_state_cb);

    /* Set correct WiFi status immediately (may already be connected) */
    settings_screen_update_wifi_status();

    settings_screen_refresh_time_label();
    time_refresh_timer = lv_timer_create([](lv_timer_t *t){
        (void)t;
        settings_screen_refresh_time_label();
    }, 1000, NULL);
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
