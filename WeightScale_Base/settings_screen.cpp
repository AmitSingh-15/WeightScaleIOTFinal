#include <lvgl.h>
#include "settings_screen.h"
#include "ui_styles.h"
#include "wifi_service.h"
#include "ota_service.h"
#include "wifi_list_screen.h"
#include "storage_service.h"
#include "devlog.h"

extern void wifi_password_popup_show(const char *ssid);

#ifdef LV_VERSION_MAJOR

/* ========================================================= */
static bool ota_requested = false;
static lv_obj_t *wifi_status;
static void (*back_cb)(void) = NULL;
static void (*calibration_cb)(void) = NULL;

static lv_obj_t *settings_scr_ref = NULL;
static lv_obj_t *wifi_list_scr = NULL;
static lv_obj_t *ota_status_label = NULL;
/* WiFi state change callback */
static void wifi_state_cb(wifi_state_t s)
{
    if(lv_scr_act() == settings_scr_ref)
        settings_screen_update_wifi_status();
}

/* ================= CONNECTING OVERLAY ================= */

static lv_obj_t *connecting_overlay = NULL;

static void show_connecting_overlay()
{
    if(connecting_overlay) return;

    connecting_overlay = lv_obj_create(settings_scr_ref);
    lv_obj_remove_style_all(connecting_overlay);
    lv_obj_set_size(connecting_overlay, 800, 480);
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
    wifi_service_start_scan();
    wifi_list_screen_refresh();
    lv_scr_load(wifi_list_scr);
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

    lv_obj_add_style(parent, &g_styles.screen, 0);
    lv_obj_set_size(parent, 800, 480);

    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_add_style(card, &g_styles.card, 0);
    lv_obj_set_size(card, 720, 400);
    lv_obj_center(card);

    lv_obj_t *back_btn = lv_btn_create(card);
    lv_obj_add_style(back_btn, &g_styles.btn_secondary, 0);
    lv_obj_align(back_btn, LV_ALIGN_TOP_RIGHT, -20, 10);
    lv_obj_add_event_cb(back_btn, back_cb_wrapper, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lv_label_create(back_btn), "Back");

    lv_obj_t *title = lv_label_create(card);
    lv_label_set_text(title, "SETTINGS");
    lv_obj_add_style(title, &g_styles.title, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    wifi_status = lv_label_create(card);
    lv_label_set_text(wifi_status, "Wi-Fi: Offline");
    lv_obj_align(wifi_status, LV_ALIGN_TOP_LEFT, 20, 60);

    lv_obj_t *scan_btn = lv_btn_create(card);
    lv_obj_add_style(scan_btn, &g_styles.btn_primary, 0);
    lv_obj_align(scan_btn, LV_ALIGN_TOP_LEFT, 20, 100);
    lv_obj_add_event_cb(scan_btn, scan_cb, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lv_label_create(scan_btn), "Scan Wi-Fi");

    lv_obj_t *ota_btn = lv_btn_create(card);
    lv_obj_add_style(ota_btn, &g_styles.btn_primary, 0);
    lv_obj_align(ota_btn, LV_ALIGN_TOP_LEFT, 20, 160);
    lv_obj_add_event_cb(ota_btn, ota_cb, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lv_label_create(ota_btn), "OTA Update");

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



    lv_obj_t *cal_btn = lv_btn_create(card);
    lv_obj_add_style(cal_btn, &g_styles.btn_primary, 0);
    lv_obj_align(cal_btn, LV_ALIGN_TOP_LEFT, 20, 220);
    lv_label_set_text(lv_label_create(cal_btn), "Calibration");

    lv_obj_add_event_cb(cal_btn, [](lv_event_t *e){
        if (calibration_cb) calibration_cb();
    }, LV_EVENT_CLICKED, NULL);

    /* WiFi List Screen */
    wifi_list_scr = lv_obj_create(NULL);
    wifi_list_screen_create(wifi_list_scr);
    wifi_list_screen_register_back(wifi_list_back);
    wifi_list_screen_register_select(wifi_selected);

    /* Developer mode switch */
    lv_obj_t *dev_lbl = lv_label_create(card);
    lv_label_set_text(dev_lbl, "Developer Mode");
    lv_obj_align(dev_lbl, LV_ALIGN_TOP_LEFT, 220, 100);

    dev_mode_sw = lv_switch_create(card);
    lv_obj_align(dev_mode_sw, LV_ALIGN_TOP_LEFT, 360, 95);
    bool was = storage_load_dev_mode();
    if(was) lv_obj_add_state(dev_mode_sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(dev_mode_sw, dev_mode_changed, LV_EVENT_VALUE_CHANGED, NULL);

    // Create OTA status label below Wi-Fi status
//static lv_obj_t *ota_status_label = nullptr;

// inside settings_screen_create()
ota_status_label = lv_label_create(card);
lv_label_set_text(ota_status_label, "OTA Status: Idle");
lv_obj_align(ota_status_label, LV_ALIGN_TOP_LEFT, 220, 210); // adjust position
lv_obj_set_width(ota_status_label, 680);

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


    /* Clear logs button */
    lv_obj_t *clear_logs_btn = lv_btn_create(card);
    lv_obj_add_style(clear_logs_btn, &g_styles.btn_secondary, 0);
    lv_obj_align(clear_logs_btn, LV_ALIGN_TOP_LEFT, 480, 95);
    lv_obj_add_event_cb(clear_logs_btn, clear_logs_cb, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lv_label_create(clear_logs_btn), "Clear Logs");

    /* Log textarea (hidden unless dev mode on) */
    dev_log_ta = lv_textarea_create(card);
    lv_obj_set_size(dev_log_ta, 640, 120);
    lv_obj_align(dev_log_ta, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    /* Load logs from storage on startup */
    if(was)
    {
        devlog_load_from_storage();
    }
    lv_textarea_set_text(dev_log_ta, was ? devlog_get_text().c_str() : "Developer mode off");
    lv_obj_set_style_text_font(dev_log_ta, &lv_font_montserrat_14, 0);

    if(was)
    {
        /* start periodic refresh */
        devlog_timer = lv_timer_create(
            [](lv_timer_t *t){
                if(dev_log_ta && lv_obj_is_valid(dev_log_ta))
                    lv_textarea_set_text(dev_log_ta, devlog_get_text().c_str());
            },
            1000,
            NULL
        );
    }

    // Register for wifi state updates so the label updates immediately
    wifi_service_register_state_callback(wifi_state_cb);
}

/* ================= STATUS UPDATE ================= */

void settings_screen_update_wifi_status(void)
{
    static wifi_state_t last_state = WIFI_DISCONNECTED;

    wifi_state_t state = wifi_service_state();
    if(state == last_state) return;

    last_state = state;

    if(state == WIFI_CONNECTING)
    {
        lv_label_set_text(wifi_status, "Wi-Fi: Connecting...");
        show_connecting_overlay();
    }
    else if(state == WIFI_CONNECTED)
    {
        lv_label_set_text(wifi_status, "Wi-Fi: Connected");
        hide_connecting_overlay();
    }
    else
    {
        lv_label_set_text(wifi_status, "Wi-Fi: Offline");
        hide_connecting_overlay();
    }
}

// ⭐ NEW: Show settings screen
void settings_screen_show(void)
{
    if(settings_scr_ref)
    {
        settings_screen_update_wifi_status();
        lv_scr_load(settings_scr_ref);
    }
}

#else
/* Stub implementations when LVGL is unavailable */
void settings_screen_create(void *parent) { }
void settings_screen_update_wifi_status(void) { }
void settings_screen_register_back_callback(void (*cb)(void)) { }
void settings_screen_register_calibration_callback(void (*cb)(void)) { }
void settings_screen_show(void) { }
#endif  // LV_VERSION_MAJOR
