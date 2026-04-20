#include <lvgl.h>

// ✅ Ensure LVGL is detected
#ifndef LV_VERSION_MAJOR
#define LV_VERSION_MAJOR 8
#endif

#include "config/app_config.h"  /* For DISPLAY_WIDTH, DISPLAY_HEIGHT */
#include "home_screen.h"
#include "ui_styles.h"
#include "ui_events.h"
#include "invoice_session_service.h"
#include "time_service.h"
#include "ota_service.h"
#include "lvgl_port.h"         /* lvgl_port_lock / unlock */
#include "devlog.h"

#ifdef LV_VERSION_MAJOR
#ifdef __cplusplus
extern "C" {
#endif
static lv_obj_t *lbl_weight;
static lv_obj_t *lbl_qty;
static lv_obj_t *lbl_total_weight;
static lv_obj_t *lbl_invoice;
static lv_obj_t *lbl_device;
static lv_obj_t *lbl_sync;
static lv_obj_t *lbl_clock;
static lv_obj_t *version_label;   // ✅ ADDED
static lv_obj_t *lbl_sensor_status; // HX711 status bar

static lv_obj_t *item_labels[MAX_INVOICE_ITEMS];
static lv_obj_t *item_delete_btns[MAX_INVOICE_ITEMS];

static void (*event_cb)(int evt) = NULL;

// ⭐ OPTIMIZATION: Single reusable buffer for all text formatting (saves ~200 bytes)
static char g_format_buf[64];  // Largest size needed (for device name)

/* Numeric keypad state */
static int keypad_value = 1;
static bool keypad_started = false;

/* Keypad toggle state */
static lv_obj_t *keypad_box_obj = NULL;
static lv_obj_t *qty_row_obj = NULL;  /* ⭐ NEW: Reference to qty row for dynamic positioning */
static bool keypad_visible = false;
static lv_obj_t *weight_box_obj = NULL;
static lv_obj_t *save_btn_obj = NULL;
static int g_weight_h_hidden = 0;
static int g_weight_h_shown = 0;
static int g_qty_h = 0;      /* ⭐ NEW: Store qty height for calculations */
static int g_keypad_h = 0;   /* ⭐ NEW: Store keypad height for calculations */
static int g_gap_v = 0;      /* ⭐ NEW: Store gap for calculations */

/* Save popup state */
static lv_obj_t *save_popup = NULL;
static lv_timer_t *save_popup_timer = NULL;
static lv_timer_t *clock_timer = NULL;

/* Restart confirmation popup */
static lv_obj_t *restart_backdrop = NULL;
static lv_obj_t *restart_popup = NULL;

/* (weight font is fixed at montserrat_48 — weight_box height computed at build) */

static void home_screen_refresh_clock(void)
{
    if(!lbl_clock || !lv_obj_is_valid(lbl_clock)) return;
    time_service_format_hhmm(g_format_buf, sizeof(g_format_buf));
    lv_label_set_text(lbl_clock, g_format_buf);
}

/* Null-out every static pointer so setters become no-ops
   while the home screen does not exist.                  */
static void home_screen_cleanup(void)
{
    lbl_weight   = NULL;
    lbl_qty      = NULL;
    lbl_total_weight = NULL;
    lbl_invoice  = NULL;
    lbl_device   = NULL;
    lbl_sync     = NULL;
    lbl_clock    = NULL;
    version_label = NULL;
    lbl_sensor_status = NULL;
    for(int i = 0; i < MAX_INVOICE_ITEMS; i++) {
        item_labels[i] = NULL;
        item_delete_btns[i] = NULL;
    }
    event_cb     = NULL;
    keypad_value = 1;
    keypad_started = false;
    keypad_box_obj = NULL;
    qty_row_obj = NULL;  /* ⭐ NEW: Reset qty row reference */
    keypad_visible = false;
    weight_box_obj = NULL;
    save_btn_obj = NULL;
    g_weight_h_hidden = 0;
    g_weight_h_shown = 0;
    g_qty_h = 0;          /* ⭐ NEW: Reset qty height */
    g_keypad_h = 0;       /* ⭐ NEW: Reset keypad height */
    g_gap_v = 0;          /* ⭐ NEW: Reset gap */
    if(clock_timer) { lv_timer_del(clock_timer); clock_timer = NULL; }
    if(save_popup_timer) { lv_timer_del(save_popup_timer); save_popup_timer = NULL; }
    if(save_popup && lv_obj_is_valid(save_popup)) { lv_obj_del(save_popup); }
    save_popup = NULL;
    if(restart_popup && lv_obj_is_valid(restart_popup)) { lv_obj_del(restart_popup); }
    restart_popup = NULL;
    if(restart_backdrop && lv_obj_is_valid(restart_backdrop)) { lv_obj_del(restart_backdrop); }
    restart_backdrop = NULL;
}

/* Called automatically when the home-screen parent is deleted */
static void home_screen_delete_cb(lv_event_t *e)
{
    (void)e;
    home_screen_cleanup();
}

/* ===== RESTART CONFIRMATION POPUP ===== */

static void restart_popup_dismiss(void)
{
    if(restart_popup && lv_obj_is_valid(restart_popup)) {
        lv_obj_del(restart_popup);
    }
    restart_popup = NULL;
    if(restart_backdrop && lv_obj_is_valid(restart_backdrop)) {
        lv_obj_del(restart_backdrop);
    }
    restart_backdrop = NULL;
}

static void restart_confirm_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code != LV_EVENT_CLICKED) return;
    restart_popup_dismiss();
    if(event_cb) event_cb(UI_EVT_RESTART);
}

static void restart_cancel_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code != LV_EVENT_CLICKED) return;
    restart_popup_dismiss();
}

static void show_restart_confirmation(void)
{
    if(restart_popup) return;  /* already showing */

    lv_obj_t *scr = lv_scr_act();

    /* Dark semi-transparent backdrop — blocks interaction with background */
    restart_backdrop = lv_obj_create(scr);
    lv_obj_remove_style_all(restart_backdrop);
    lv_obj_set_size(restart_backdrop, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(restart_backdrop, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(restart_backdrop, LV_OPA_70, 0);
    lv_obj_center(restart_backdrop);
    lv_obj_clear_flag(restart_backdrop, LV_OBJ_FLAG_SCROLLABLE);

    /* Popup dialog on top of backdrop */
    restart_popup = lv_obj_create(scr);
    lv_obj_set_size(restart_popup, 400, 200);
    lv_obj_center(restart_popup);
    lv_obj_set_style_bg_color(restart_popup, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_bg_opa(restart_popup, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(restart_popup, lv_color_hex(0xEF4444), 0);
    lv_obj_set_style_border_width(restart_popup, 2, 0);
    lv_obj_set_style_radius(restart_popup, 12, 0);
    lv_obj_set_style_pad_all(restart_popup, 20, 0);
    lv_obj_clear_flag(restart_popup, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(restart_popup);
    lv_obj_set_style_text_color(title, lv_color_hex(0xEF4444), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_label_set_text(title, LV_SYMBOL_WARNING " Restart Device?");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *msg = lv_label_create(restart_popup);
    lv_obj_set_style_text_color(msg, lv_color_hex(0xCBD5E1), 0);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_16, 0);
    lv_label_set_text(msg, "Device will restart. Unsaved data\nmay be lost.");
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, -5);

    /* Cancel button */
    lv_obj_t *cancel_btn = lv_btn_create(restart_popup);
    lv_obj_set_size(cancel_btn, 140, 44);
    lv_obj_align(cancel_btn, LV_ALIGN_BOTTOM_LEFT, 10, 0);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0x334155), 0);
    lv_obj_add_event_cb(cancel_btn, restart_cancel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *cancel_lbl = lv_label_create(cancel_btn);
    lv_obj_set_style_text_font(cancel_lbl, &lv_font_montserrat_16, 0);
    lv_label_set_text(cancel_lbl, "Cancel");
    lv_obj_center(cancel_lbl);

    /* Confirm button */
    lv_obj_t *confirm_btn = lv_btn_create(restart_popup);
    lv_obj_set_size(confirm_btn, 140, 44);
    lv_obj_align(confirm_btn, LV_ALIGN_BOTTOM_RIGHT, -10, 0);
    lv_obj_set_style_bg_color(confirm_btn, lv_color_hex(0xEF4444), 0);
    lv_obj_add_event_cb(confirm_btn, restart_confirm_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *confirm_lbl = lv_label_create(confirm_btn);
    lv_obj_set_style_text_font(confirm_lbl, &lv_font_montserrat_16, 0);
    lv_label_set_text(confirm_lbl, LV_SYMBOL_POWER " Restart");
    lv_obj_center(confirm_lbl);
}

static void btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_target(e);

    /* ===== PRESS FEEDBACK: Visual state on button interaction ===== */
    if (code == LV_EVENT_PRESSED) {
        lv_obj_add_style(btn, &g_styles.btn_pressed, 0);
        return;  /* Don't trigger action on press, only on release */
    } else if (code == LV_EVENT_RELEASED) {
        lv_obj_remove_style(btn, &g_styles.btn_pressed, 0);
    }

    /* ===== EXECUTE ACTION on release (not press) ===== */
    if (code != LV_EVENT_RELEASED) {
        return;  /* Only respond to release events */
    }

    uintptr_t id = (uintptr_t)lv_event_get_user_data(e);

    /* Intercept restart — show confirmation popup instead of firing immediately */
    if(id == UI_EVT_RESTART) {
        show_restart_confirmation();
        return;
    }

    if(!event_cb) {
        return;
    }
    event_cb((int)id);
}

static void keypad_update_qty(void)
{
    if(!lbl_qty || !lv_obj_is_valid(lbl_qty)) return;
    snprintf(g_format_buf, sizeof(g_format_buf), "Qty: %d", keypad_value);
    lv_label_set_text(lbl_qty, g_format_buf);
    invoice_session_set_selected_qty(keypad_value);
    if(event_cb) event_cb(UI_EVT_QTY_CHANGED);
}

static void keypad_btn_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_target(e);

    if (code == LV_EVENT_PRESSED) {
        lv_obj_add_style(btn, &g_styles.btn_pressed, 0);
        return;
    } else if (code == LV_EVENT_RELEASED) {
        lv_obj_remove_style(btn, &g_styles.btn_pressed, 0);
    }
    if (code != LV_EVENT_RELEASED) return;

    intptr_t key = (intptr_t)lv_event_get_user_data(e);

    if(key >= 0 && key <= 9) {
        /* Digit press */
        if(!keypad_started) {
            /* First digit: replace the default "1" entirely */
            keypad_value = (int)key;
            keypad_started = true;
        } else {
            int nv = keypad_value * 10 + (int)key;
            if(nv <= 999) keypad_value = nv;
        }
        if(keypad_value < 1) keypad_value = 1;
    } else if(key == -1) {
        /* Clear */
        keypad_value = 1;
        keypad_started = false;
    } else if(key == -2) {
        /* Backspace */
        keypad_value = keypad_value / 10;
        if(keypad_value < 1) { keypad_value = 1; keypad_started = false; }
    }
    keypad_update_qty();
}

static void multiply_btn_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_target(e);

    if (code == LV_EVENT_PRESSED) {
        lv_obj_add_style(btn, &g_styles.btn_pressed, 0);
        return;
    } else if (code == LV_EVENT_RELEASED) {
        lv_obj_remove_style(btn, &g_styles.btn_pressed, 0);
    }
    if (code != LV_EVENT_RELEASED) return;

    if(!keypad_box_obj || !lv_obj_is_valid(keypad_box_obj)) return;
    keypad_visible = !keypad_visible;
    
    if(keypad_visible) {
        /* Show keypad: shrink weight box and reposition qty_row and keypad below it */
        if(weight_box_obj && lv_obj_is_valid(weight_box_obj))
            lv_obj_set_height(weight_box_obj, g_weight_h_shown);
        
        /* Position qty_row below the weight box (stuck to bottom of weight box) */
        if(qty_row_obj && lv_obj_is_valid(qty_row_obj)) {
            lv_obj_align(qty_row_obj, LV_ALIGN_TOP_LEFT, 0, g_weight_h_shown + g_gap_v);
        }
        
        /* Show keypad positioned below qty_row */
        lv_obj_clear_flag(keypad_box_obj, LV_OBJ_FLAG_HIDDEN);
        int keypad_y = g_weight_h_shown + g_gap_v + g_qty_h + g_gap_v;
        lv_obj_align(keypad_box_obj, LV_ALIGN_TOP_MID, 0, keypad_y);
    } else {
        /* Hide keypad: expand weight box and reposition qty_row */
        if(weight_box_obj && lv_obj_is_valid(weight_box_obj))
            lv_obj_set_height(weight_box_obj, g_weight_h_hidden);
        
        /* Position qty_row below the weight box (stuck to bottom of weight box) */
        if(qty_row_obj && lv_obj_is_valid(qty_row_obj)) {
            lv_obj_align(qty_row_obj, LV_ALIGN_TOP_LEFT, 0, g_weight_h_hidden + g_gap_v);
        }
        
        /* Hide keypad */
        lv_obj_add_flag(keypad_box_obj, LV_OBJ_FLAG_HIDDEN);
    }
    
    if(keypad_box_obj && lv_obj_is_valid(keypad_box_obj))
        lv_obj_move_foreground(keypad_box_obj);
}

void home_screen_register_callback(void (*cb)(int evt))
{
    event_cb = cb;
}

void home_screen_create(lv_obj_t *parent)
{
    /* Ensure stale pointers from a previous home screen are cleared */
    home_screen_cleanup();

    /* Auto-cleanup when this screen is deleted (e.g. navigating to settings) */
    lv_obj_add_event_cb(parent, home_screen_delete_cb, LV_EVENT_DELETE, NULL);

    lv_obj_add_style(parent,&g_styles.screen,0);
    lv_obj_set_size(parent, DISPLAY_WIDTH, DISPLAY_HEIGHT);  // 1024×600 from app_config.h

    /* ================= STATUS BAR (30px) ================= */

    lv_obj_t *status_bar = lv_obj_create(parent);
    lv_obj_remove_style_all(status_bar);
    lv_obj_set_size(status_bar, DISPLAY_WIDTH, 30);
    lv_obj_align(status_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(status_bar, ui_theme_surface(), 0);
    lv_obj_set_style_bg_opa(status_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(status_bar, 15, 0);
    lv_obj_set_style_pad_right(status_bar, 15, 0);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);

    lbl_device = lv_label_create(status_bar);
    lv_obj_set_style_text_color(lbl_device, ui_theme_muted(), 0);
    lv_obj_set_style_text_font(lbl_device, &lv_font_montserrat_16, 0);
    lv_label_set_text(lbl_device, "Device: -");
    lv_obj_align(lbl_device, LV_ALIGN_CENTER, 0, 0);

    lbl_sync = lv_label_create(status_bar);
    lv_obj_set_style_text_font(lbl_sync, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_sync, ui_theme_muted(), 0);
    lv_label_set_text(lbl_sync, LV_SYMBOL_WIFI " Offline");
    lv_obj_align(lbl_sync, LV_ALIGN_RIGHT_MID, 0, 0);

    /* ================= NAVIGATION BAR (60px) ================= */

    lv_obj_t *header = lv_obj_create(parent);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, DISPLAY_WIDTH, 60);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_color(header, ui_theme_card(), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_border_color(header, ui_theme_border(), 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lbl_clock = lv_label_create(header);
    lv_obj_set_style_text_color(lbl_clock, ui_theme_accent(), 0);
    lv_obj_set_style_text_font(lbl_clock, &lv_font_montserrat_20, 0);
    lv_label_set_text(lbl_clock, "--:--");
    lv_obj_align(lbl_clock, LV_ALIGN_LEFT_MID, 18, 0);

    /* RESTART button — between clock and nav buttons */
    lv_obj_t *restart_btn = lv_btn_create(header);
    lv_obj_set_size(restart_btn, 48, 48);
    lv_obj_align(restart_btn, LV_ALIGN_LEFT_MID, 100, 0);
    lv_obj_add_style(restart_btn, &g_styles.btn_danger, 0);
    lv_obj_add_event_cb(restart_btn, btn_event_cb, LV_EVENT_PRESSED, (void*)UI_EVT_RESTART);
    lv_obj_add_event_cb(restart_btn, btn_event_cb, LV_EVENT_RELEASED, (void*)UI_EVT_RESTART);
    lv_obj_t *restart_lbl = lv_label_create(restart_btn);
    lv_obj_set_style_text_font(restart_lbl, &lv_font_montserrat_20, 0);
    lv_label_set_text(restart_lbl, LV_SYMBOL_POWER);
    lv_obj_center(restart_lbl);

    /* Buttons row — full width with even spacing */
    lv_obj_t *hdr_btns = lv_obj_create(header);
    lv_obj_remove_style_all(hdr_btns);
    lv_obj_set_size(hdr_btns, DISPLAY_WIDTH - 180, 54);
    lv_obj_align(hdr_btns, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_flex_flow(hdr_btns, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hdr_btns, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(hdr_btns, 6, 0);

    /* SERIAL NUMBER (display button) */
    lv_obj_t *sn_btn = lv_btn_create(hdr_btns);
    lv_obj_set_size(sn_btn, 150, 48);
    lv_obj_add_style(sn_btn, &g_styles.btn_action, 0);
    lbl_invoice = lv_label_create(sn_btn);
    lv_obj_set_style_text_font(lbl_invoice, &lv_font_montserrat_16, 0);
    lv_label_set_text(lbl_invoice, "S/N 1");
    lv_obj_center(lbl_invoice);

    /* HISTORY */
    lv_obj_t *history_btn = lv_btn_create(hdr_btns);
    lv_obj_set_size(history_btn, 150, 48);
    lv_obj_add_style(history_btn, &g_styles.btn_secondary, 0);
    lv_obj_add_event_cb(history_btn, btn_event_cb, LV_EVENT_PRESSED, (void*)UI_EVT_HISTORY);
    lv_obj_add_event_cb(history_btn, btn_event_cb, LV_EVENT_RELEASED, (void*)UI_EVT_HISTORY);
    lv_obj_t *his_lbl = lv_label_create(history_btn);
    lv_obj_set_style_text_font(his_lbl, &lv_font_montserrat_16, 0);
    lv_label_set_text(his_lbl, LV_SYMBOL_LIST " HISTORY");
    lv_obj_center(his_lbl);

    /* SET TO ZERO */
    lv_obj_t *cal_btn = lv_btn_create(hdr_btns);
    lv_obj_set_size(cal_btn, 155, 48);
    lv_obj_add_style(cal_btn, &g_styles.btn_warning, 0);
    lv_obj_add_event_cb(cal_btn, btn_event_cb, LV_EVENT_PRESSED, (void*)UI_EVT_CALIBRATE);
    lv_obj_add_event_cb(cal_btn, btn_event_cb, LV_EVENT_RELEASED, (void*)UI_EVT_CALIBRATE);
    lv_obj_t *cal_lbl = lv_label_create(cal_btn);
    lv_obj_set_style_text_font(cal_lbl, &lv_font_montserrat_16, 0);
    lv_label_set_text(cal_lbl, LV_SYMBOL_REFRESH " Set to zero");
    lv_obj_center(cal_lbl);

    /* SETTINGS */
    lv_obj_t *settings_btn = lv_btn_create(hdr_btns);
    lv_obj_set_size(settings_btn, 155, 48);
    lv_obj_add_style(settings_btn, &g_styles.btn_primary, 0);
    lv_obj_add_event_cb(settings_btn, btn_event_cb, LV_EVENT_PRESSED, (void*)UI_EVT_SETTINGS);
    lv_obj_add_event_cb(settings_btn, btn_event_cb, LV_EVENT_RELEASED, (void*)UI_EVT_SETTINGS);
    lv_obj_t *set_lbl = lv_label_create(settings_btn);
    lv_obj_set_style_text_font(set_lbl, &lv_font_montserrat_16, 0);
    lv_label_set_text(set_lbl, LV_SYMBOL_SETTINGS " SETTINGS");
    lv_obj_center(set_lbl);

    /* Wi-Fi Setting */
    lv_obj_t *wifi_btn = lv_btn_create(hdr_btns);
    lv_obj_set_size(wifi_btn, 160, 48);
    lv_obj_add_style(wifi_btn, &g_styles.btn_action, 0);
    lv_obj_add_event_cb(wifi_btn, btn_event_cb, LV_EVENT_PRESSED, (void*)UI_EVT_WIFI_DIRECT);
    lv_obj_add_event_cb(wifi_btn, btn_event_cb, LV_EVENT_RELEASED, (void*)UI_EVT_WIFI_DIRECT);
    lv_obj_t *wifi_lbl = lv_label_create(wifi_btn);
    lv_obj_set_style_text_font(wifi_lbl, &lv_font_montserrat_16, 0);
    lv_label_set_text(wifi_lbl, LV_SYMBOL_WIFI " Wi-Fi Setting");
    lv_obj_center(wifi_lbl);


    /* ================= BODY (below header) ================= */

    lv_obj_t *main = lv_obj_create(parent);
    lv_obj_remove_style_all(main);
    lv_obj_set_size(main, DISPLAY_WIDTH, DISPLAY_HEIGHT - 94);
    lv_obj_align(main, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* ===== LEFT PANEL - Weight & Controls ===== */
    /* Use absolute positioning — reliable on LVGL 8 embedded targets.
       Left panel inner area (after card padding ~10px each side):
       usable width  ≈ (DISPLAY_WIDTH - 200) - 20 = ~804
       usable height ≈ (DISPLAY_HEIGHT - 100) - 20 = ~480              */

    const int left_w = DISPLAY_WIDTH - 200;
    const int left_h = DISPLAY_HEIGHT - 100;
    const int pad = 10;                     /* card padding approx      */
    const int inner_w = left_w - 2 * pad;   /* ~804                     */
    const int inner_h = left_h - 2 * pad;   /* ~480                     */

    /* Fixed element heights */
    const int qty_h    = 46;
    const int keypad_h = 130;
    const int save_h   = 75;
    const int gap_v    = 6;

    /* Weight box height in both states */
    const int weight_h_hidden = inner_h - qty_h - save_h - 3 * gap_v;             /* no keypad */
    const int weight_h_shown  = inner_h - qty_h - keypad_h - save_h - 4 * gap_v;  /* keypad visible */

    lv_obj_t *left = lv_obj_create(main);
    lv_obj_add_style(left, &g_styles.card, 0);
    lv_obj_set_size(left, left_w, left_h);
    lv_obj_align(left, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_clear_flag(left, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(left, LV_SCROLLBAR_MODE_OFF);

    /* --- Y cursor for stacking elements --- */
    int cur_y = 0;

    /* --- WEIGHT DISPLAY --- */
    weight_box_obj = lv_obj_create(left);
    lv_obj_set_size(weight_box_obj, inner_w, weight_h_hidden);
    lv_obj_align(weight_box_obj, LV_ALIGN_TOP_MID, 0, cur_y);
    lv_obj_set_style_bg_color(weight_box_obj, ui_theme_surface(), 0);
    lv_obj_set_style_bg_opa(weight_box_obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(weight_box_obj, 12, 0);
    lv_obj_set_style_border_width(weight_box_obj, 2, 0);
    lv_obj_set_style_border_color(weight_box_obj, ui_theme_border(), 0);
    lv_obj_set_style_pad_all(weight_box_obj, 0, 0);
    lv_obj_clear_flag(weight_box_obj, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *kg_label = lv_label_create(weight_box_obj);
    lv_obj_set_style_text_font(kg_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(kg_label, ui_theme_muted(), 0);
    lv_label_set_text(kg_label, "WEIGHT (kg)");
    lv_obj_align(kg_label, LV_ALIGN_TOP_MID, 0, 10);

    lbl_weight = lv_label_create(weight_box_obj);
    lv_obj_set_style_text_font(lbl_weight, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_weight,
        ui_styles_is_light_mode() ? lv_color_hex(0x0E7490) : lv_color_hex(0x22D3EE), 0);
    lv_label_set_text(lbl_weight, "0.00");
    lv_obj_align(lbl_weight, LV_ALIGN_CENTER, 0, 10);

    /* --- QTY / MULTIPLY / TOTAL (stick to BOTTOM of weight box) --- */
    lv_obj_t *qty_row = lv_obj_create(left);
    lv_obj_remove_style_all(qty_row);
    lv_obj_set_size(qty_row, inner_w, qty_h);
    lv_obj_align(qty_row, LV_ALIGN_TOP_MID, 0, weight_h_hidden + gap_v);  /* Position below weight box */
    lv_obj_clear_flag(qty_row, LV_OBJ_FLAG_SCROLLABLE);
    
    /* ⭐ STORE: Qty row object for dynamic positioning */
    qty_row_obj = qty_row;
    g_qty_h = qty_h;
    g_gap_v = gap_v;

    /* Use flex row: Qty | Multiply | Total — packed tightly left to right */
    lv_obj_set_flex_flow(qty_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(qty_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(qty_row, 10, 0);

    lbl_qty = lv_label_create(qty_row);
    lv_obj_add_style(lbl_qty, &g_styles.value_big, 0);
    lv_label_set_text(lbl_qty, "Qty: 1");
    lv_label_set_long_mode(lbl_qty, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(lbl_qty, LV_SIZE_CONTENT);

    /* Multiply button — toggles keypad visibility */
    lv_obj_t *mult_btn = lv_btn_create(qty_row);
    lv_obj_set_size(mult_btn, 150, 42);
    lv_obj_add_style(mult_btn, &g_styles.btn_warning, 0);
    lv_obj_add_event_cb(mult_btn, multiply_btn_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(mult_btn, multiply_btn_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_t *mult_lbl = lv_label_create(mult_btn);
    lv_obj_set_style_text_font(mult_lbl, &lv_font_montserrat_16, 0);
    lv_label_set_text(mult_lbl, LV_SYMBOL_EDIT " Multiply");
    lv_obj_center(mult_lbl);

    lbl_total_weight = lv_label_create(qty_row);
    lv_obj_add_style(lbl_total_weight, &g_styles.value_big, 0);
    lv_label_set_long_mode(lbl_total_weight, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(lbl_total_weight, LV_SIZE_CONTENT);
    lv_label_set_text(lbl_total_weight, "Total: 0.00 kg");
    lv_obj_set_flex_grow(lbl_total_weight, 1);
    lv_obj_set_style_text_align(lbl_total_weight, LV_TEXT_ALIGN_RIGHT, 0);

    cur_y += qty_h + gap_v;

    /* --- NUMERIC KEYPAD (hidden by default) --- */
    keypad_box_obj = lv_obj_create(left);
    lv_obj_remove_style_all(keypad_box_obj);
    lv_obj_set_size(keypad_box_obj, inner_w, keypad_h);
    lv_obj_align(keypad_box_obj, LV_ALIGN_TOP_MID, 0, weight_h_hidden + qty_h + 2*gap_v);  /* Position below qty row */
    lv_obj_clear_flag(keypad_box_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(keypad_box_obj, LV_OBJ_FLAG_HIDDEN);
    keypad_visible = false;
    
    /* ⭐ STORE: Keypad object for dynamic positioning */
    g_keypad_h = keypad_h;

    #define KPAD_BTN(parent, x, y, w, h, label_text, font, style, key_id) \
    do { \
        lv_obj_t *kb = lv_btn_create(parent); \
        lv_obj_set_size(kb, w, h); \
        lv_obj_set_pos(kb, x, y); \
        lv_obj_add_style(kb, style, 0); \
        lv_obj_add_event_cb(kb, keypad_btn_cb, LV_EVENT_PRESSED, (void*)(intptr_t)(key_id)); \
        lv_obj_add_event_cb(kb, keypad_btn_cb, LV_EVENT_RELEASED, (void*)(intptr_t)(key_id)); \
        lv_obj_t *kl = lv_label_create(kb); \
        lv_obj_set_style_text_font(kl, font, 0); \
        lv_label_set_text(kl, label_text); \
        lv_obj_center(kl); \
    } while(0)

    const int kw = 120, kh = 50, kgap = 10;
    const int row1_y = 5, row2_y = 5 + kh + 10;

    for(int d = 1; d <= 5; d++) {
        char dl[2] = { (char)('0'+d), 0 };
        int xp = (d - 1) * (kw + kgap);
        KPAD_BTN(keypad_box_obj, xp, row1_y, kw, kh, dl, &lv_font_montserrat_20, &g_styles.btn_secondary, d);
    }
    KPAD_BTN(keypad_box_obj, 5 * (kw + kgap), row1_y, kw, kh, "CLR", &lv_font_montserrat_16, &g_styles.btn_danger, -1);

    for(int d = 6; d <= 9; d++) {
        char dl[2] = { (char)('0'+d), 0 };
        int xp = (d - 6) * (kw + kgap);
        KPAD_BTN(keypad_box_obj, xp, row2_y, kw, kh, dl, &lv_font_montserrat_20, &g_styles.btn_secondary, d);
    }
    KPAD_BTN(keypad_box_obj, 4 * (kw + kgap), row2_y, kw, kh, "0", &lv_font_montserrat_20, &g_styles.btn_secondary, 0);
    KPAD_BTN(keypad_box_obj, 5 * (kw + kgap), row2_y, kw, kh, LV_SYMBOL_BACKSPACE, &lv_font_montserrat_16, &g_styles.btn_warning, -2);

    #undef KPAD_BTN

    /* Store height values for multiply callback */
    g_weight_h_hidden = weight_h_hidden;
    g_weight_h_shown  = weight_h_shown;

    /* ===== SAVE BUTTON (ALWAYS AT BOTTOM) ===== */
    save_btn_obj = lv_btn_create(left);
    lv_obj_set_size(save_btn_obj, inner_w, save_h);
    lv_obj_align(save_btn_obj, LV_ALIGN_BOTTOM_MID, 0, 0);  /* ⭐ FIXED: Always at bottom using BOTTOM_MID */
    lv_obj_add_style(save_btn_obj, &g_styles.btn_primary, 0);
    lv_obj_add_event_cb(save_btn_obj, btn_event_cb, LV_EVENT_PRESSED, (void*)UI_EVT_RESET);
    lv_obj_add_event_cb(save_btn_obj, btn_event_cb, LV_EVENT_RELEASED, (void*)UI_EVT_RESET);
    lv_obj_set_style_bg_color(save_btn_obj, lv_color_hex(0x15803D), 0);
    lv_obj_set_style_bg_grad_color(save_btn_obj, lv_color_hex(0x166534), 0);
    lv_obj_set_style_bg_grad_dir(save_btn_obj, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_color(save_btn_obj, lv_color_hex(0x4ADE80), 0);
    lv_obj_set_style_shadow_width(save_btn_obj, 14, 0);
    lv_obj_set_style_shadow_color(save_btn_obj, lv_color_hex(0x22C55E), 0);
    lv_obj_set_style_shadow_opa(save_btn_obj, LV_OPA_40, 0);
    lv_obj_set_style_text_color(save_btn_obj, lv_color_hex(0xFFFFFF), 0);
    lv_obj_t *lbl_final = lv_label_create(save_btn_obj);
    lv_obj_set_style_text_font(lbl_final, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_final, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(lbl_final, LV_SYMBOL_SAVE " SAVE");
    lv_obj_center(lbl_final);

    /* ===== RIGHT PANEL - Items (50% of original width, full height) ===== */

    lv_obj_t *right = lv_obj_create(main);
    lv_obj_add_style(right,&g_styles.card,0);
    lv_obj_set_size(right, 182, DISPLAY_HEIGHT - 100);
    lv_obj_align(right,LV_ALIGN_RIGHT_MID,-5,0);

    lv_obj_t *items_title = lv_label_create(right);
    lv_obj_set_style_text_font(items_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(items_title, ui_theme_accent(), 0);
    lv_label_set_text(items_title, "ITEMS");
    lv_obj_align(items_title, LV_ALIGN_TOP_MID, 0, 0);

    for(int i=0;i<MAX_INVOICE_ITEMS;i++)
    {
        lv_obj_t *row = lv_obj_create(right);
        lv_obj_set_size(row, lv_pct(95), 42);
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 30 + i * 44);
        lv_obj_set_style_bg_color(row, (i % 2 == 0) ? ui_theme_row_even() : ui_theme_row_odd(), 0);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 2, 0);

        item_labels[i] = lv_label_create(row);
        lv_obj_set_style_text_color(item_labels[i], ui_theme_text(), 0);
        lv_obj_set_style_text_font(item_labels[i], &lv_font_montserrat_14, 0);
        lv_label_set_long_mode(item_labels[i], LV_LABEL_LONG_DOT);
        lv_obj_align(item_labels[i], LV_ALIGN_LEFT_MID, 4, 0);
        lv_obj_set_width(item_labels[i], 110);

        item_delete_btns[i] = lv_btn_create(row);
        lv_obj_set_size(item_delete_btns[i], 34, 28);
        lv_obj_align(item_delete_btns[i], LV_ALIGN_RIGHT_MID, -2, 0);
        lv_obj_add_style(item_delete_btns[i], &g_styles.btn_danger, 0);
        lv_obj_add_event_cb(item_delete_btns[i], btn_event_cb, LV_EVENT_PRESSED,
                            (void*)(uintptr_t)(UI_EVT_REMOVE_ITEM_BASE + i));
        lv_obj_add_event_cb(item_delete_btns[i], btn_event_cb, LV_EVENT_RELEASED,
                            (void*)(uintptr_t)(UI_EVT_REMOVE_ITEM_BASE + i));
        lv_obj_t *del_lbl = lv_label_create(item_delete_btns[i]);
        lv_obj_set_style_text_font(del_lbl, &lv_font_montserrat_14, 0);
        lv_label_set_text(del_lbl, LV_SYMBOL_TRASH);
        lv_obj_center(del_lbl);
        lv_obj_add_flag(item_delete_btns[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* Sensor status bar (bottom-left) */
    lbl_sensor_status = lv_label_create(parent);
    lv_obj_align(lbl_sensor_status, LV_ALIGN_BOTTOM_LEFT, 10, -5);
    lv_obj_set_style_text_color(lbl_sensor_status, lv_color_hex(0x22C55E), 0);  // Green = OK
    lv_obj_set_style_text_font(lbl_sensor_status, &lv_font_montserrat_14, 0);
    lv_label_set_text(lbl_sensor_status, LV_SYMBOL_OK " Sensor: Initializing...");

    /* Version label */
    version_label = lv_label_create(parent);
    lv_obj_align(version_label, LV_ALIGN_BOTTOM_RIGHT, -10, -5);
    lv_obj_set_style_text_color(version_label, ui_theme_muted(), 0);
    lv_obj_set_style_text_font(version_label, &lv_font_montserrat_14, 0);
    String ota_ver = ota_service_current_version();
    snprintf(g_format_buf, sizeof(g_format_buf), "v%s", ota_ver.length() ? ota_ver.c_str() : "0.0.0");
    lv_label_set_text(version_label, g_format_buf);

    home_screen_refresh_clock();
    clock_timer = lv_timer_create([](lv_timer_t *t){
        (void)t;
        home_screen_refresh_clock();
    }, 1000, NULL);

}

void home_screen_set_device(const char *name)
{
    if(!lvgl_port_lock(100)) return;
    if(lbl_device && lv_obj_is_valid(lbl_device))
    {
        snprintf(g_format_buf, sizeof(g_format_buf), "Device: %s", name);
        lv_label_set_text(lbl_device, g_format_buf);
    }
    lvgl_port_unlock();
}

void home_screen_set_sync_status(const char *txt)
{
    if(!lvgl_port_lock(100)) return;
    if(lbl_sync && lv_obj_is_valid(lbl_sync))
        lv_label_set_text(lbl_sync,txt);
    lvgl_port_unlock();
}

void home_screen_set_clock_text(const char *txt)
{
    if(!lvgl_port_lock(100)) return;
    if(lbl_clock && lv_obj_is_valid(lbl_clock))
        lv_label_set_text(lbl_clock, txt ? txt : "--:--");
    lvgl_port_unlock();
}

void home_screen_refresh_invoice_details(void)
{
    if(!lvgl_port_lock(100)) return;

    uint8_t count = invoice_session_count();
    float total_weight = 0.0f;

    for(int i = 0; i < MAX_INVOICE_ITEMS; i++)
    {
        if(!item_labels[i] || !lv_obj_is_valid(item_labels[i])) continue;

        if(i < count)
        {
            const invoice_item_t *it = invoice_session_get(i);
            if(it)
            {
                total_weight += (it->weight * (float)it->qty);
                snprintf(g_format_buf, sizeof(g_format_buf),
                         "%d. %.1f kg x %d = %.1f kg",
                         i + 1,
                         it->weight,
                         it->qty,
                         it->weight * (float)it->qty);
                lv_label_set_text(item_labels[i], g_format_buf);
                if(item_delete_btns[i] && lv_obj_is_valid(item_delete_btns[i]))
                    lv_obj_clear_flag(item_delete_btns[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
        else
        {
            lv_label_set_text(item_labels[i], "");
            if(item_delete_btns[i] && lv_obj_is_valid(item_delete_btns[i]))
                lv_obj_add_flag(item_delete_btns[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    if(lbl_total_weight && lv_obj_is_valid(lbl_total_weight)) {
        snprintf(g_format_buf, sizeof(g_format_buf), "Total: %.1f kg", total_weight);
        lv_label_set_text(lbl_total_weight, g_format_buf);
    }

    lvgl_port_unlock();
}

void home_screen_set_weight(float w)
{
    if(!lvgl_port_lock(100)) return;
    if(!lbl_weight || !lv_obj_is_valid(lbl_weight)) { lvgl_port_unlock(); return; }

    snprintf(g_format_buf, sizeof(g_format_buf), "%.1f", w);
    lv_label_set_text(lbl_weight, g_format_buf);

    /* Dismiss save popup AFTER weight label update to avoid flash from large overlay deletion */
    if(w > 0.1f && save_popup) {
        home_screen_dismiss_save_popup();
    }
    lvgl_port_unlock();
}

void home_screen_set_quantity(int qty)
{
    /* Only reset keypad_started when value actually changes
       (avoids resetting during round-trip from controller) */
    if(qty != keypad_value) {
        keypad_value = qty;
        keypad_started = (qty > 1);
    }
    if(!lvgl_port_lock(100)) return;
    if(!lbl_qty || !lv_obj_is_valid(lbl_qty)) { lvgl_port_unlock(); return; }

    snprintf(g_format_buf, sizeof(g_format_buf), "Qty: %d", qty);
    lv_label_set_text(lbl_qty, g_format_buf);
    lvgl_port_unlock();
}

void home_screen_set_invoice(uint32_t id)
{
    if(!lvgl_port_lock(100)) return;
    if(!lbl_invoice || !lv_obj_is_valid(lbl_invoice)) { lvgl_port_unlock(); return; }

    snprintf(g_format_buf, sizeof(g_format_buf), "S/N %lu", id);
    lv_label_set_text(lbl_invoice, g_format_buf);
    lvgl_port_unlock();
}

void home_screen_set_version(const char *ver)
{
    if(!lvgl_port_lock(100)) return;
    if(version_label && lv_obj_is_valid(version_label))
    {
        snprintf(g_format_buf, sizeof(g_format_buf), "v%s", ver);
        lv_label_set_text(version_label, g_format_buf);
    }
    lvgl_port_unlock();
}

void home_screen_dismiss_save_popup(void)
{
    if(!save_popup && !save_popup_timer) return;  /* nothing to dismiss */
    devlog_printf("[HOME] Dismissing save popup");
    if(save_popup_timer) {
        lv_timer_del(save_popup_timer);
        save_popup_timer = NULL;
    }
    if(save_popup && lv_obj_is_valid(save_popup)) {
        lv_obj_del(save_popup);
    }
    save_popup = NULL;
}

/* Standalone version with its own lock — for callers outside LVGL context */
void home_screen_dismiss_save_popup_safe(void)
{
    if(!lvgl_port_lock(100)) return;
    home_screen_dismiss_save_popup();
    lvgl_port_unlock();
}

void home_screen_set_sensor_status(const char *status_text, bool is_error)
{
    if(!lvgl_port_lock(100)) return;
    if(!lbl_sensor_status || !lv_obj_is_valid(lbl_sensor_status)) { lvgl_port_unlock(); return; }
    lv_label_set_text(lbl_sensor_status, status_text);
    lv_obj_set_style_text_color(lbl_sensor_status,
        is_error ? lv_color_hex(0xEF4444) : lv_color_hex(0x22C55E), 0);
    lvgl_port_unlock();
}

static void save_popup_timer_cb(lv_timer_t *t)
{
    (void)t;
    devlog_printf("[HOME] Save popup auto-dismiss timer fired");
    /* Timer is single-shot (repeat_count=1), LVGL auto-deletes it after this callback.
       Clear our pointer so dismiss_save_popup() doesn't double-delete it. */
    save_popup_timer = NULL;
    home_screen_dismiss_save_popup();
}

static void save_popup_close_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code != LV_EVENT_CLICKED) return;
    home_screen_dismiss_save_popup();
}

void home_screen_show_save_popup(uint32_t serial_num, uint8_t item_count, uint32_t total_qty, float total_weight)
{
    home_screen_dismiss_save_popup();

    devlog_printf("[HOME] Showing save popup S/N=%lu items=%u qty=%lu wt=%.1f",
                  (unsigned long)serial_num, item_count, (unsigned long)total_qty, total_weight);

    char buf[128];
    lv_obj_t *scr = lv_scr_act();

    /* Full-screen overlay — tap anywhere to dismiss */
    save_popup = lv_obj_create(scr);
    lv_obj_remove_style_all(save_popup);
    lv_obj_set_size(save_popup, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(save_popup, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(save_popup, LV_OPA_70, 0);
    lv_obj_center(save_popup);
    lv_obj_clear_flag(save_popup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(save_popup, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(save_popup, save_popup_close_cb, LV_EVENT_CLICKED, NULL);

    /* Popup card */
    lv_obj_t *card = lv_obj_create(save_popup);
    lv_obj_set_size(card, 560, 340);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x4ADE80), 0);
    lv_obj_set_style_shadow_width(card, 20, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0x22C55E), 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_40, 0);
    lv_obj_set_style_pad_all(card, 15, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* Close button (top-right, inside card) */
    lv_obj_t *close_btn = lv_btn_create(card);
    lv_obj_set_size(close_btn, 40, 40);
    lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0x475569), 0);
    lv_obj_set_style_radius(close_btn, 20, 0);
    lv_obj_set_style_shadow_width(close_btn, 0, 0);
    lv_obj_add_event_cb(close_btn, save_popup_close_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *close_lbl = lv_label_create(close_btn);
    lv_obj_set_style_text_font(close_lbl, &lv_font_montserrat_20, 0);
    lv_label_set_text(close_lbl, LV_SYMBOL_CLOSE);
    lv_obj_center(close_lbl);

    /* Check mark / Saved label */
    lv_obj_t *check_lbl = lv_label_create(card);
    lv_obj_set_style_text_font(check_lbl, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(check_lbl, lv_color_hex(0x4ADE80), 0);
    lv_label_set_text(check_lbl, LV_SYMBOL_OK " Saved!");
    lv_obj_align(check_lbl, LV_ALIGN_TOP_MID, 0, 15);

    /* Serial Number */
    lv_obj_t *sn_lbl = lv_label_create(card);
    lv_obj_set_style_text_font(sn_lbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(sn_lbl, lv_color_hex(0xFFFFFF), 0);
    snprintf(buf, sizeof(buf), "S/N: %lu", (unsigned long)serial_num);
    lv_label_set_text(sn_lbl, buf);
    lv_obj_align(sn_lbl, LV_ALIGN_TOP_MID, 0, 70);

    /* Invoice summary: items, qty, total weight */
    lv_obj_t *info_lbl = lv_label_create(card);
    lv_obj_set_style_text_font(info_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(info_lbl, lv_color_hex(0xCBD5E1), 0);
    snprintf(buf, sizeof(buf), "Items: %u   |   Qty: %lu   |   Total: %.1f kg",
             item_count, (unsigned long)total_qty, total_weight);
    lv_label_set_text(info_lbl, buf);
    lv_obj_set_style_text_align(info_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(info_lbl, LV_ALIGN_CENTER, 0, 20);

    /* Hint text */
    lv_obj_t *hint_lbl = lv_label_create(card);
    lv_obj_set_style_text_font(hint_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(hint_lbl, lv_color_hex(0x64748B), 0);
    lv_label_set_text(hint_lbl, "Tap anywhere or place weight to dismiss");
    lv_obj_align(hint_lbl, LV_ALIGN_BOTTOM_MID, 0, -15);

    /* Auto-dismiss after 60 seconds */
    save_popup_timer = lv_timer_create(save_popup_timer_cb, 60000, NULL);
    lv_timer_set_repeat_count(save_popup_timer, 1);
    devlog_printf("[HOME] Save popup timer created (60s)");
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // LV_VERSION_MAJOR
