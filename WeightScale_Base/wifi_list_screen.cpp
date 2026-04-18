#include <lvgl.h>

// ✅ Ensure LVGL is detected
#ifndef LV_VERSION_MAJOR
#define LV_VERSION_MAJOR 8
#endif

#ifdef LV_VERSION_MAJOR

#include "config/app_config.h"
#include "wifi_list_screen.h"
#include "wifi_service.h"
#include "wifi_ota_task.h"  /* ⭐ NEW: Use Core 1 task API */
#include "storage_service.h"
#include "ui_styles.h"

#ifdef __cplusplus
extern "C" {
#endif

static lv_obj_t *scr;
static lv_obj_t *list;
static lv_timer_t *scan_poll_timer = NULL;
static void (*back_cb)(void) = NULL;
static void (*select_cb)(const char*) = NULL;
static void scan_poll_cb(lv_timer_t *t);

/* Static SSID storage — avoids strdup/free heap fragmentation */
#define MAX_SCAN_APS 20
static char ssid_store[MAX_SCAN_APS][33];

/* Saved network storage for per-row callbacks */
#define MAX_SAVED_ROWS WIFI_MAX_SAVED
static char saved_ssid_rows[MAX_SAVED_ROWS][33];
static char saved_pwd_rows[MAX_SAVED_ROWS][65];
static char forget_target_ssid[33];  /* SSID pending forget confirmation */

static void ssid_clicked(lv_event_t *e)
{
    uintptr_t idx = (uintptr_t)lv_event_get_user_data(e);
    if(idx >= MAX_SCAN_APS) return;

    /* If we have a saved password for this SSID, connect directly */
    char saved_pwd[65] = {0};
    if(storage_find_wifi_password(ssid_store[idx], saved_pwd, sizeof(saved_pwd))) {
        /* ⭐ NEW: Use Core 1 task API to enqueue connect command */
        uint8_t ssid_len = strlen(ssid_store[idx]);
        uint8_t pwd_len = strlen(saved_pwd);
        uint8_t payload[2 + 33 + 65];  /* len_ssid + len_pwd + ssid + pwd */
        payload[0] = ssid_len;
        payload[1] = pwd_len;
        memcpy(&payload[2], ssid_store[idx], ssid_len);
        memcpy(&payload[2 + ssid_len], saved_pwd, pwd_len);
        
        wifi_ota_task_enqueue(WIFI_TASK_CMD_CONNECT, payload, 2 + ssid_len + pwd_len);
        
        wifi_list_screen_refresh();
        if(scan_poll_timer) lv_timer_del(scan_poll_timer);
        scan_poll_timer = lv_timer_create(scan_poll_cb, 500, NULL);
        return;
    }

    /* No saved password — show password popup */
    if(select_cb) select_cb(ssid_store[idx]);
}

void wifi_list_screen_register_back(void (*cb)(void)) { back_cb = cb; }
void wifi_list_screen_register_select(void (*cb)(const char*)) { select_cb = cb; }

static void back_evt(lv_event_t *e)
{
    if(scan_poll_timer) {
        lv_timer_del(scan_poll_timer);
        scan_poll_timer = NULL;
    }
    wifi_service_cancel_scan();
    if(back_cb) back_cb();
}

static void saved_connect_clicked(lv_event_t *e)
{
    uintptr_t idx = (uintptr_t)lv_event_get_user_data(e);
    if(idx >= MAX_SAVED_ROWS || saved_ssid_rows[idx][0] == 0) return;
    
    /* ⭐ NEW: Use Core 1 task API to enqueue connect command */
    uint8_t ssid_len = strlen(saved_ssid_rows[idx]);
    uint8_t pwd_len = strlen(saved_pwd_rows[idx]);
    uint8_t payload[2 + 33 + 65];
    payload[0] = ssid_len;
    payload[1] = pwd_len;
    memcpy(&payload[2], saved_ssid_rows[idx], ssid_len);
    memcpy(&payload[2 + ssid_len], saved_pwd_rows[idx], pwd_len);
    
    wifi_ota_task_enqueue(WIFI_TASK_CMD_CONNECT, payload, 2 + ssid_len + pwd_len);
    
    wifi_list_screen_refresh();
    if(scan_poll_timer) lv_timer_del(scan_poll_timer);
    scan_poll_timer = lv_timer_create(scan_poll_cb, 500, NULL);
}

void wifi_list_screen_create(lv_obj_t *parent)
{
    scr = parent;
    lv_obj_add_style(scr,&g_styles.screen,0);

    lv_obj_t *header = lv_obj_create(scr);
    lv_obj_add_style(header,&g_styles.card,0);
    lv_obj_set_size(header, DISPLAY_WIDTH, 90);
    lv_obj_align(header,LV_ALIGN_TOP_MID,0,0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *wifi_title = lv_label_create(header);
    lv_obj_add_style(wifi_title, &g_styles.title, 0);
    lv_label_set_text(wifi_title, LV_SYMBOL_WIFI "  SELECT WIFI");
    lv_obj_align(wifi_title, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t *back = lv_btn_create(header);
    lv_obj_add_style(back,&g_styles.btn_secondary,0);
    lv_obj_set_size(back, 160, 65);
    lv_obj_align(back,LV_ALIGN_RIGHT_MID,-10,0);
    lv_label_set_text(lv_label_create(back), LV_SYMBOL_LEFT " BACK");
    lv_obj_add_event_cb(back,back_evt,LV_EVENT_RELEASED,NULL);

    list = lv_list_create(scr);
    lv_obj_set_size(list, DISPLAY_WIDTH - 40, DISPLAY_HEIGHT - 105);
    lv_obj_align(list,LV_ALIGN_BOTTOM_MID,0,-5);
    lv_obj_set_style_bg_color(list, ui_theme_card(), 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(list, ui_theme_border(), 0);
    lv_obj_set_style_border_width(list, 1, 0);
    lv_obj_set_style_radius(list, 12, 0);
    lv_obj_set_style_text_font(list, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(list, ui_theme_text(), 0);
}


/* ── Forget‑WiFi confirmation popup ── */
static lv_obj_t *forget_overlay = NULL;

static void forget_confirm_cb(lv_event_t *e)
{
    /* Forget only the targeted SSID */
    String conn = wifi_service_get_connected_ssid();
    storage_forget_wifi_ssid(forget_target_ssid);

    /* If we just forgot the currently connected network, disconnect */
    if(conn.length() > 0 && conn == forget_target_ssid) {
        wifi_service_disconnect();
    }

    if(forget_overlay) { lv_obj_del(forget_overlay); forget_overlay = NULL; }
    wifi_list_screen_refresh();
}

static void forget_cancel_cb(lv_event_t *e)
{
    if(forget_overlay) { lv_obj_del(forget_overlay); forget_overlay = NULL; }
}

static void forget_clicked(lv_event_t *e)
{
    /* Capture which saved SSID to forget */
    uintptr_t idx = (uintptr_t)lv_event_get_user_data(e);
    if(idx < MAX_SAVED_ROWS && saved_ssid_rows[idx][0]) {
        strncpy(forget_target_ssid, saved_ssid_rows[idx], sizeof(forget_target_ssid) - 1);
        forget_target_ssid[sizeof(forget_target_ssid) - 1] = 0;
    }

    /* Dim overlay */
    forget_overlay = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(forget_overlay);
    lv_obj_set_size(forget_overlay, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(forget_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(forget_overlay, LV_OPA_60, 0);
    lv_obj_clear_flag(forget_overlay, LV_OBJ_FLAG_SCROLLABLE);

    /* Card */
    lv_obj_t *card = lv_obj_create(forget_overlay);
    lv_obj_add_style(card, &g_styles.card, 0);
    lv_obj_set_size(card, 460, 220);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *msg = lv_label_create(card);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(msg, COLOR_TEXT, 0);
    String prompt = "Forget \"" + String(forget_target_ssid) + "\"?";
    lv_label_set_text(msg, prompt.c_str());
    lv_obj_align(msg, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *yes = lv_btn_create(card);
    lv_obj_add_style(yes, &g_styles.btn_danger, 0);
    lv_obj_set_size(yes, 140, 60);
    lv_obj_align(yes, LV_ALIGN_BOTTOM_LEFT, 30, -15);
    lv_obj_t *ylbl = lv_label_create(yes);
    lv_obj_set_style_text_font(ylbl, &lv_font_montserrat_20, 0);
    lv_label_set_text(ylbl, "FORGET");
    lv_obj_center(ylbl);
    lv_obj_add_event_cb(yes, forget_confirm_cb, LV_EVENT_RELEASED, NULL);

    lv_obj_t *no = lv_btn_create(card);
    lv_obj_add_style(no, &g_styles.btn_secondary, 0);
    lv_obj_set_size(no, 140, 60);
    lv_obj_align(no, LV_ALIGN_BOTTOM_RIGHT, -30, -15);
    lv_obj_t *nlbl = lv_label_create(no);
    lv_obj_set_style_text_font(nlbl, &lv_font_montserrat_20, 0);
    lv_label_set_text(nlbl, "CANCEL");
    lv_obj_center(nlbl);
    lv_obj_add_event_cb(no, forget_cancel_cb, LV_EVENT_RELEASED, NULL);
}

void wifi_list_screen_refresh(void)
{
    lv_obj_clean(list);

    /* ⭐ UPDATED: Use Core 1 task API */
    wifi_state_t ws = wifi_ota_task_get_state();
    String conn_ssid = wifi_service_get_connected_ssid();

    /* ── Saved networks section ── */
    uint8_t saved_cnt = storage_get_wifi_count();

    /* Cache all saved networks for per-row callbacks */
    for(uint8_t i = 0; i < MAX_SAVED_ROWS; i++) {
        saved_ssid_rows[i][0] = 0;
        saved_pwd_rows[i][0] = 0;
    }
    for(uint8_t i = 0; i < saved_cnt && i < MAX_SAVED_ROWS; i++) {
        storage_get_wifi_at(i, saved_ssid_rows[i], sizeof(saved_ssid_rows[i]),
                            saved_pwd_rows[i], sizeof(saved_pwd_rows[i]));
    }

    if(saved_cnt > 0)
    {
        lv_obj_t *sec = lv_label_create(list);
        lv_obj_set_style_text_font(sec, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(sec, COLOR_MUTED, 0);
        lv_label_set_text(sec, saved_cnt == 1 ? "SAVED NETWORK" : "SAVED NETWORKS");
        lv_obj_set_style_pad_top(sec, 8, 0);
        lv_obj_set_style_pad_bottom(sec, 2, 0);

        for(uint8_t si = 0; si < saved_cnt; si++)
        {
            char s_ssid[33] = {0}, s_pwd[65] = {0};
            if(!storage_get_wifi_at(si, s_ssid, sizeof(s_ssid), s_pwd, sizeof(s_pwd)))
                continue;

            lv_obj_t *row = lv_obj_create(list);
            lv_obj_remove_style_all(row);
            lv_obj_set_size(row, lv_pct(100), 56);
            lv_obj_set_style_bg_color(row, (si % 2 == 0) ? ui_theme_row_even() : ui_theme_row_odd(), 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(row, 8, 0);
            lv_obj_set_style_pad_all(row, 6, 0);
            lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

            /* Status for this specific SSID */
            bool is_connected = (ws == WIFI_CONNECTED && conn_ssid == s_ssid);
            bool is_connecting = (ws == WIFI_CONNECTING);

            const char *icon = is_connected ? LV_SYMBOL_WIFI : LV_SYMBOL_WARNING;
            const char *status = is_connected ? " Connected"
                               : is_connecting ? " Connecting..."
                               : "";

            lv_obj_t *lbl = lv_label_create(row);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
            lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
            String display = String(icon) + " " + s_ssid + status;
            lv_label_set_text(lbl, display.c_str());
            lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 4, 0);

            /* Forget button (always shown) */
            lv_obj_t *fbtn = lv_btn_create(row);
            lv_obj_add_style(fbtn, &g_styles.btn_danger, 0);
            lv_obj_set_size(fbtn, 100, 40);
            lv_obj_align(fbtn, LV_ALIGN_RIGHT_MID, -4, 0);
            lv_obj_t *flbl = lv_label_create(fbtn);
            lv_obj_set_style_text_font(flbl, &lv_font_montserrat_14, 0);
            lv_label_set_text(flbl, LV_SYMBOL_TRASH " Forget");
            lv_obj_center(flbl);
            lv_obj_add_event_cb(fbtn, forget_clicked, LV_EVENT_RELEASED,
                                (void*)(uintptr_t)si);

            /* Connect button (only if not already connected) */
            if(!is_connected) {
                lv_obj_t *cbtn = lv_btn_create(row);
                lv_obj_add_style(cbtn, &g_styles.btn_primary, 0);
                lv_obj_set_size(cbtn, 110, 40);
                lv_obj_align(cbtn, LV_ALIGN_RIGHT_MID, -110, 0);
                lv_obj_t *clbl = lv_label_create(cbtn);
                lv_obj_set_style_text_font(clbl, &lv_font_montserrat_14, 0);
                lv_label_set_text(clbl, LV_SYMBOL_WIFI " Connect");
                lv_obj_center(clbl);
                lv_obj_add_event_cb(cbtn, saved_connect_clicked, LV_EVENT_RELEASED,
                                    (void*)(uintptr_t)si);
            }
        }

        /* Separator */
        lv_obj_t *sep = lv_label_create(list);
        lv_obj_set_style_text_font(sep, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(sep, COLOR_MUTED, 0);
        lv_label_set_text(sep, "AVAILABLE NETWORKS");
        lv_obj_set_style_pad_top(sep, 12, 0);
        lv_obj_set_style_pad_bottom(sep, 2, 0);
    }

    /* ── Scanned networks (deduplicated) ── */
    /* ⭐ UPDATED: Use Core 1 task API and convert scan count */
    int scan_status = wifi_ota_task_get_scan_count();
    uint8_t count = (scan_status >= 0) ? (uint8_t)scan_status : 0;

    if(count == 0)
    {
        lv_obj_t *lbl = lv_label_create(list);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(lbl, COLOR_MUTED, 0);
        lv_label_set_text(lbl, "No networks found. Try again.");
        lv_obj_set_width(lbl, lv_pct(100));
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_top(lbl, 30, 0);
        return;
    }

    /* Collect unique SSIDs into ssid_store, skipping duplicates */
    uint8_t unique_count = 0;
    uint8_t raw = (count > MAX_SCAN_APS) ? MAX_SCAN_APS : count;
    for(int i = 0; i < raw && unique_count < MAX_SCAN_APS; i++)
    {
        /* ⭐ UPDATED: Use Core 1 task API */
        String ssid = wifi_ota_task_get_ssid(i);
        if(ssid.length() == 0) continue;                          /* skip hidden */
        if(!conn_ssid.isEmpty() && ssid == conn_ssid) continue;   /* skip connected */

        /* Skip SSIDs already shown in saved networks section */
        bool is_saved = false;
        for(uint8_t si = 0; si < saved_cnt && si < MAX_SAVED_ROWS; si++) {
            if(ssid == saved_ssid_rows[si]) { is_saved = true; break; }
        }
        if(is_saved) continue;

        /* Check for duplicate */
        bool dup = false;
        for(uint8_t j = 0; j < unique_count; j++) {
            if(ssid == ssid_store[j]) { dup = true; break; }
        }
        if(dup) continue;

        strncpy(ssid_store[unique_count], ssid.c_str(), 32);
        ssid_store[unique_count][32] = 0;

        lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_WIFI, ssid_store[unique_count]);
        lv_obj_add_style(btn, &g_styles.list_btn, 0);
        lv_obj_add_event_cb(btn, ssid_clicked, LV_EVENT_RELEASED,
                            (void*)(uintptr_t)unique_count);
        unique_count++;
    }
}

/* Timer callback — polls async scan status */
static void scan_poll_cb(lv_timer_t *t)
{
    /* ⭐ UPDATED: Use Core 1 task API */
    int status = wifi_ota_task_get_scan_count();
    wifi_state_t ws = wifi_ota_task_get_state();
    if(status == -1) return;  /* still scanning */

    wifi_list_screen_refresh();
    if(ws != WIFI_CONNECTING) {
        lv_timer_del(t);
        scan_poll_timer = NULL;
    }
}

/* Start async scan and show scanning indicator */
void wifi_list_screen_start_scan(void)
{
    /* ⭐ NEW: Cancel scan using Core 1 task API */
    wifi_ota_task_enqueue(WIFI_TASK_CMD_CANCEL_SCAN, nullptr, 0);

    char saved_ssid[33] = {0};
    char saved_pwd[65] = {0};
    bool has_saved = storage_load_wifi_credentials(saved_ssid, sizeof(saved_ssid),
                                                   saved_pwd, sizeof(saved_pwd));

    /* ⭐ UPDATED: Use Core 1 task API for state check */
    if(has_saved && wifi_ota_task_get_state() == WIFI_CONNECTING) {
        /* A connection attempt is in progress — just refresh and poll status */
        wifi_list_screen_refresh();
        if(scan_poll_timer) lv_timer_del(scan_poll_timer);
        scan_poll_timer = lv_timer_create(scan_poll_cb, 500, NULL);
        return;
    }

    /* Show saved network row immediately (if any), then start scan */
    wifi_list_screen_refresh();

    /* ⭐ NEW: Start async scan using Core 1 task API */
    wifi_ota_task_enqueue(WIFI_TASK_CMD_START_SCAN, nullptr, 0);

    /* Poll for completion every 500ms */
    if(scan_poll_timer) lv_timer_del(scan_poll_timer);
    scan_poll_timer = lv_timer_create(scan_poll_cb, 500, NULL);
}

// ⭐ NEW FUNCTION TO SHOW THIS SCREEN
void wifi_list_screen_show(void)
{
    if(scr)
        lv_scr_load(scr);
}

#ifdef __cplusplus
}  // extern "C"
#endif

#else  // LV_VERSION_MAJOR not defined - provide stubs

void wifi_list_screen_create(void *parent) {}
void wifi_list_screen_show(void) {}
void wifi_list_screen_refresh(void) {}
void wifi_list_screen_start_scan(void) {}
void wifi_list_screen_register_back(void (*cb)(void)) {}
void wifi_list_screen_register_select(void (*cb)(const char *ssid)) {}

#endif  // LV_VERSION_MAJOR
