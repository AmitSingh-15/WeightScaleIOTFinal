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
// ✅ REFACTORED: Removed #include "wifi_service.h" - WiFi is now decoupled
// ✅ Use app_controller instead for WiFi state updates

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
static lv_obj_t *version_label;   // ✅ ADDED

static lv_obj_t *item_labels[MAX_INVOICE_ITEMS];
static lv_obj_t *item_delete_btns[MAX_INVOICE_ITEMS];

static void (*event_cb)(int evt) = NULL;

// ⭐ OPTIMIZATION: Single reusable buffer for all text formatting (saves ~200 bytes)
static char g_format_buf[64];  // Largest size needed (for device name)

/* Numeric keypad state */
static int keypad_value = 1;
static bool keypad_started = false;

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
    version_label = NULL;
    for(int i = 0; i < MAX_INVOICE_ITEMS; i++) {
        item_labels[i] = NULL;
        item_delete_btns[i] = NULL;
    }
    event_cb     = NULL;
    keypad_value = 1;
    keypad_started = false;
}

/* Called automatically when the home-screen parent is deleted */
static void home_screen_delete_cb(lv_event_t *e)
{
    (void)e;
    home_screen_cleanup();
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

    Serial.println("[BTN] Button released!");
    if(!event_cb) {
        Serial.println("[BTN] ERROR: event_cb is NULL!");
        return;
    }
    uintptr_t id = (uintptr_t)lv_event_get_user_data(e);
    Serial.printf("[BTN] Calling event_cb with id=%u\n", (unsigned int)id);
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
            if(nv <= 9999) keypad_value = nv;
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

    lbl_invoice = lv_label_create(status_bar);
    lv_obj_set_style_text_font(lbl_invoice, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_invoice, ui_theme_accent(), 0);
    lv_label_set_text(lbl_invoice, "Invoice #1");
    lv_obj_align(lbl_invoice, LV_ALIGN_LEFT_MID, 0, 0);

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

    /* Buttons row — full width with even spacing */
    lv_obj_t *hdr_btns = lv_obj_create(header);
    lv_obj_remove_style_all(hdr_btns);
    lv_obj_set_size(hdr_btns, DISPLAY_WIDTH - 20, 54);
    lv_obj_align(hdr_btns, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_flex_flow(hdr_btns, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hdr_btns, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(hdr_btns, 8, 0);

    /* HISTORY */
    lv_obj_t *history_btn = lv_btn_create(hdr_btns);
    lv_obj_set_size(history_btn, 185, 48);
    lv_obj_add_style(history_btn, &g_styles.btn_secondary, 0);
    lv_obj_add_event_cb(history_btn, btn_event_cb, LV_EVENT_PRESSED, (void*)UI_EVT_HISTORY);
    lv_obj_add_event_cb(history_btn, btn_event_cb, LV_EVENT_RELEASED, (void*)UI_EVT_HISTORY);
    lv_obj_t *his_lbl = lv_label_create(history_btn);
    lv_obj_set_style_text_font(his_lbl, &lv_font_montserrat_16, 0);
    lv_label_set_text(his_lbl, LV_SYMBOL_LIST " HISTORY");
    lv_obj_center(his_lbl);

    /* CALIBRATE */
    lv_obj_t *cal_btn = lv_btn_create(hdr_btns);
    lv_obj_set_size(cal_btn, 185, 48);
    lv_obj_add_style(cal_btn, &g_styles.btn_warning, 0);
    lv_obj_add_event_cb(cal_btn, btn_event_cb, LV_EVENT_PRESSED, (void*)UI_EVT_CALIBRATE);
    lv_obj_add_event_cb(cal_btn, btn_event_cb, LV_EVENT_RELEASED, (void*)UI_EVT_CALIBRATE);
    lv_obj_t *cal_lbl = lv_label_create(cal_btn);
    lv_obj_set_style_text_font(cal_lbl, &lv_font_montserrat_16, 0);
    lv_label_set_text(cal_lbl, LV_SYMBOL_REFRESH " CALIBRATE");
    lv_obj_center(cal_lbl);

    /* CLEAR */
    lv_obj_t *clear_btn = lv_btn_create(hdr_btns);
    lv_obj_set_size(clear_btn, 185, 48);
    lv_obj_add_style(clear_btn, &g_styles.btn_danger, 0);
    lv_obj_add_event_cb(clear_btn, btn_event_cb, LV_EVENT_PRESSED, (void*)UI_EVT_RESET_ALL);
    lv_obj_add_event_cb(clear_btn, btn_event_cb, LV_EVENT_RELEASED, (void*)UI_EVT_RESET_ALL);
    lv_obj_t *clr_lbl = lv_label_create(clear_btn);
    lv_obj_set_style_text_font(clr_lbl, &lv_font_montserrat_16, 0);
    lv_label_set_text(clr_lbl, LV_SYMBOL_TRASH " CLEAR");
    lv_obj_center(clr_lbl);

    /* SETTINGS */
    lv_obj_t *settings_btn = lv_btn_create(hdr_btns);
    lv_obj_set_size(settings_btn, 185, 48);
    lv_obj_add_style(settings_btn, &g_styles.btn_primary, 0);
    lv_obj_add_event_cb(settings_btn, btn_event_cb, LV_EVENT_PRESSED, (void*)UI_EVT_SETTINGS);
    lv_obj_add_event_cb(settings_btn, btn_event_cb, LV_EVENT_RELEASED, (void*)UI_EVT_SETTINGS);
    lv_obj_t *set_lbl = lv_label_create(settings_btn);
    lv_obj_set_style_text_font(set_lbl, &lv_font_montserrat_16, 0);
    lv_label_set_text(set_lbl, LV_SYMBOL_SETTINGS " SETTINGS");
    lv_obj_center(set_lbl);

    /* WIFI */
    lv_obj_t *wifi_btn = lv_btn_create(hdr_btns);
    lv_obj_set_size(wifi_btn, 185, 48);
    lv_obj_add_style(wifi_btn, &g_styles.btn_action, 0);
    lv_obj_add_event_cb(wifi_btn, btn_event_cb, LV_EVENT_PRESSED, (void*)UI_EVT_WIFI_DIRECT);
    lv_obj_add_event_cb(wifi_btn, btn_event_cb, LV_EVENT_RELEASED, (void*)UI_EVT_WIFI_DIRECT);
    lv_obj_t *wifi_lbl = lv_label_create(wifi_btn);
    lv_obj_set_style_text_font(wifi_lbl, &lv_font_montserrat_16, 0);
    lv_label_set_text(wifi_lbl, LV_SYMBOL_WIFI " WIFI");
    lv_obj_center(wifi_lbl);


    /* ================= BODY (below header) ================= */

    lv_obj_t *main = lv_obj_create(parent);
    lv_obj_remove_style_all(main);
    lv_obj_set_size(main, DISPLAY_WIDTH, DISPLAY_HEIGHT - 94);
    lv_obj_align(main, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* ===== LEFT PANEL - Weight & Controls (640px) ===== */

    lv_obj_t *left = lv_obj_create(main);
    lv_obj_add_style(left,&g_styles.card,0);
    lv_obj_set_size(left, 640, DISPLAY_HEIGHT - 100);
    lv_obj_align(left,LV_ALIGN_LEFT_MID,5,0);
    lv_obj_clear_flag(left, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(left, LV_SCROLLBAR_MODE_OFF);

    /* --- WEIGHT DISPLAY --- */
    lv_obj_t *weight_box = lv_obj_create(left);
    lv_obj_set_size(weight_box, 610, 150);
    lv_obj_align(weight_box, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(weight_box, ui_theme_surface(), 0);
    lv_obj_set_style_bg_opa(weight_box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(weight_box, 12, 0);
    lv_obj_set_style_border_width(weight_box, 2, 0);
    lv_obj_set_style_border_color(weight_box, ui_theme_border(), 0);
    lv_obj_set_style_pad_all(weight_box, 0, 0);
    lv_obj_clear_flag(weight_box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *kg_label = lv_label_create(weight_box);
    lv_obj_set_style_text_font(kg_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(kg_label, ui_theme_muted(), 0);
    lv_label_set_text(kg_label, "WEIGHT (kg)");
    lv_obj_align(kg_label, LV_ALIGN_TOP_MID, 0, 6);

    lbl_weight = lv_label_create(weight_box);
    lv_obj_set_style_text_font(lbl_weight, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_weight,
        ui_styles_is_light_mode() ? lv_color_hex(0x0E7490) : lv_color_hex(0x22D3EE), 0);
    lv_label_set_text(lbl_weight,"0.00");
    lv_obj_align(lbl_weight, LV_ALIGN_CENTER, 0, 12);

    /* ===== QUANTITY KEYPAD ===== */

    lv_obj_t *keypad_box = lv_obj_create(left);
    lv_obj_remove_style_all(keypad_box);
    lv_obj_set_size(keypad_box, 610, 200);
    lv_obj_align(keypad_box, LV_ALIGN_TOP_MID, 0, 160);
    lv_obj_clear_flag(keypad_box, LV_OBJ_FLAG_SCROLLABLE);

    /* Qty display label */
    lbl_qty = lv_label_create(keypad_box);
    lv_obj_add_style(lbl_qty, &g_styles.value_big, 0);
    lv_label_set_text(lbl_qty, "Qty: 1");
    lv_obj_set_style_text_align(lbl_qty, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_width(lbl_qty, 200);
    lv_obj_align(lbl_qty, LV_ALIGN_TOP_LEFT, 0, 0);

    lbl_total_weight = lv_label_create(keypad_box);
    lv_obj_add_style(lbl_total_weight, &g_styles.value_big, 0);
    lv_obj_set_style_text_align(lbl_total_weight, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_width(lbl_total_weight, 410);
    lv_obj_align(lbl_total_weight, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_label_set_text(lbl_total_weight, "Total: 0.00 kg");

    /* Helper to create a keypad button */
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

    /* 2-row keypad: 6 buttons per row */
    const int kw = 90, kh = 50, gap = 8;
    const int row1_y = 55, row2_y = 55 + kh + 24;

    /* Row 1: [1] [2] [3] [4] [5] [CLR] */
    for(int d = 1; d <= 5; d++) {
        char dl[2] = { (char)('0'+d), 0 };
        int xp = (d - 1) * (kw + gap);
        KPAD_BTN(keypad_box, xp, row1_y, kw, kh, dl, &lv_font_montserrat_20, &g_styles.btn_secondary, d);
    }
    KPAD_BTN(keypad_box, 5 * (kw + gap), row1_y, kw, kh, "CLR", &lv_font_montserrat_16, &g_styles.btn_danger, -1);

    /* Row 2: [6] [7] [8] [9] [0] [⌫] */
    for(int d = 6; d <= 9; d++) {
        char dl[2] = { (char)('0'+d), 0 };
        int xp = (d - 6) * (kw + gap);
        KPAD_BTN(keypad_box, xp, row2_y, kw, kh, dl, &lv_font_montserrat_20, &g_styles.btn_secondary, d);
    }
    KPAD_BTN(keypad_box, 4 * (kw + gap), row2_y, kw, kh, "0", &lv_font_montserrat_20, &g_styles.btn_secondary, 0);
    KPAD_BTN(keypad_box, 5 * (kw + gap), row2_y, kw, kh, LV_SYMBOL_BACKSPACE, &lv_font_montserrat_16, &g_styles.btn_warning, -2);

    #undef KPAD_BTN

    /* ===== SAVE ===== */

    lv_obj_t *button_row = lv_obj_create(left);
    lv_obj_remove_style_all(button_row);
    lv_obj_set_size(button_row, 610, 85);
    lv_obj_align(button_row, LV_ALIGN_TOP_MID, 0, 390);
    lv_obj_set_flex_flow(button_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(button_row, 20, 0);

    /* SAVE button — Vivid Green */
    lv_obj_t *final_btn = lv_btn_create(button_row);
    lv_obj_set_size(final_btn, 610, 80);
    lv_obj_add_style(final_btn, &g_styles.btn_primary, 0);
    lv_obj_add_event_cb(final_btn, btn_event_cb, LV_EVENT_PRESSED, (void*)UI_EVT_RESET);
    lv_obj_add_event_cb(final_btn, btn_event_cb, LV_EVENT_RELEASED, (void*)UI_EVT_RESET);
    lv_obj_set_style_bg_color(final_btn, lv_color_hex(0x15803D), 0);
    lv_obj_set_style_bg_grad_color(final_btn, lv_color_hex(0x166534), 0);
    lv_obj_set_style_bg_grad_dir(final_btn, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_color(final_btn, lv_color_hex(0x4ADE80), 0);
    lv_obj_set_style_shadow_width(final_btn, 14, 0);
    lv_obj_set_style_shadow_color(final_btn, lv_color_hex(0x22C55E), 0);
    lv_obj_set_style_shadow_opa(final_btn, LV_OPA_40, 0);
    lv_obj_set_style_text_color(final_btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_t *lbl_final = lv_label_create(final_btn);
    lv_obj_set_style_text_font(lbl_final, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_final, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(lbl_final, LV_SYMBOL_SAVE " SAVE");
    lv_obj_center(lbl_final);

    /* ===== RIGHT PANEL - Invoice Items (remaining width) ===== */

    lv_obj_t *right = lv_obj_create(main);
    lv_obj_add_style(right,&g_styles.card,0);
    lv_obj_set_size(right, DISPLAY_WIDTH - 660, DISPLAY_HEIGHT - 100);
    lv_obj_align(right,LV_ALIGN_RIGHT_MID,-5,0);

    lv_obj_t *items_title = lv_label_create(right);
    lv_obj_set_style_text_font(items_title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(items_title, ui_theme_accent(), 0);
    lv_label_set_text(items_title, "ITEMS");
    lv_obj_align(items_title, LV_ALIGN_TOP_MID, 0, 0);

    for(int i=0;i<MAX_INVOICE_ITEMS;i++)
    {
        lv_obj_t *row = lv_obj_create(right);
        lv_obj_set_size(row, lv_pct(95), 48);
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 40 + i * 52);
        lv_obj_set_style_bg_color(row, (i % 2 == 0) ? ui_theme_row_even() : ui_theme_row_odd(), 0);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 4, 0);

        item_labels[i] = lv_label_create(row);
        lv_obj_set_style_text_color(item_labels[i], ui_theme_text(), 0);
        lv_obj_set_style_text_font(item_labels[i], &lv_font_montserrat_20, 0);
        lv_obj_align(item_labels[i], LV_ALIGN_LEFT_MID, 8, 0);
        lv_obj_set_width(item_labels[i], 260);

        item_delete_btns[i] = lv_btn_create(row);
        lv_obj_set_size(item_delete_btns[i], 44, 34);
        lv_obj_align(item_delete_btns[i], LV_ALIGN_RIGHT_MID, -4, 0);
        lv_obj_add_style(item_delete_btns[i], &g_styles.btn_danger, 0);
        lv_obj_add_event_cb(item_delete_btns[i], btn_event_cb, LV_EVENT_PRESSED,
                            (void*)(uintptr_t)(UI_EVT_REMOVE_ITEM_BASE + i));
        lv_obj_add_event_cb(item_delete_btns[i], btn_event_cb, LV_EVENT_RELEASED,
                            (void*)(uintptr_t)(UI_EVT_REMOVE_ITEM_BASE + i));
        lv_obj_t *del_lbl = lv_label_create(item_delete_btns[i]);
        lv_obj_set_style_text_font(del_lbl, &lv_font_montserrat_16, 0);
        lv_label_set_text(del_lbl, LV_SYMBOL_TRASH);
        lv_obj_center(del_lbl);
        lv_obj_add_flag(item_delete_btns[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* Version label */
    version_label = lv_label_create(parent);
    lv_obj_align(version_label, LV_ALIGN_BOTTOM_RIGHT, -10, -5);
    lv_obj_set_style_text_color(version_label, ui_theme_muted(), 0);
    lv_obj_set_style_text_font(version_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(version_label, "v0.0.0");

}

void home_screen_set_device(const char *name)
{
    if(lbl_device && lv_obj_is_valid(lbl_device))
    {
        snprintf(g_format_buf, sizeof(g_format_buf), "Device: %s", name);
        lv_label_set_text(lbl_device, g_format_buf);
    }
}

void home_screen_set_sync_status(const char *txt)
{
    if(lbl_sync && lv_obj_is_valid(lbl_sync))
        lv_label_set_text(lbl_sync,txt);
}

void home_screen_refresh_invoice_details(void)
{
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
}

void home_screen_set_weight(float w)
{
    if(!lbl_weight || !lv_obj_is_valid(lbl_weight)) return;

    snprintf(g_format_buf, sizeof(g_format_buf), "%.1f", w);
    lv_label_set_text(lbl_weight, g_format_buf);
}

void home_screen_set_quantity(int qty)
{
    /* Only reset keypad_started when value actually changes
       (avoids resetting during round-trip from controller) */
    if(qty != keypad_value) {
        keypad_value = qty;
        keypad_started = (qty > 1);
    }
    if(!lbl_qty || !lv_obj_is_valid(lbl_qty)) return;

    snprintf(g_format_buf, sizeof(g_format_buf), "Qty: %d", qty);
    lv_label_set_text(lbl_qty, g_format_buf);
}

void home_screen_set_invoice(uint32_t id)
{
    if(!lbl_invoice || !lv_obj_is_valid(lbl_invoice)) return;

    snprintf(g_format_buf, sizeof(g_format_buf), "Invoice #%lu", id);
    lv_label_set_text(lbl_invoice, g_format_buf);
}

void home_screen_set_version(const char *ver)
{
    if(version_label && lv_obj_is_valid(version_label))
    {
        snprintf(g_format_buf, sizeof(g_format_buf), "v%s", ver);
        lv_label_set_text(version_label, g_format_buf);
    }
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // LV_VERSION_MAJOR
