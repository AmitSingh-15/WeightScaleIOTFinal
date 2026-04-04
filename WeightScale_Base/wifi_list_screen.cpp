#include <lvgl.h>

// ✅ Ensure LVGL is detected
#ifndef LV_VERSION_MAJOR
#define LV_VERSION_MAJOR 8
#endif

#ifdef LV_VERSION_MAJOR

#include "config/app_config.h"
#include "wifi_list_screen.h"
#include "wifi_service.h"
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

/* Static SSID storage — avoids strdup/free heap fragmentation */
#define MAX_SCAN_APS 20
static char ssid_store[MAX_SCAN_APS][33];

static void ssid_clicked(lv_event_t *e)
{
    if(!select_cb) return;
    uintptr_t idx = (uintptr_t)lv_event_get_user_data(e);
    if(idx < MAX_SCAN_APS)
        select_cb(ssid_store[idx]);
}

void wifi_list_screen_register_back(void (*cb)(void)) { back_cb = cb; }
void wifi_list_screen_register_select(void (*cb)(const char*)) { select_cb = cb; }

static void back_evt(lv_event_t *e)
{
    if(back_cb) back_cb();
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
    lv_obj_set_style_bg_color(list, COLOR_CARD, 0);
    lv_obj_set_style_border_color(list, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(list, 1, 0);
    lv_obj_set_style_radius(list, 12, 0);
    lv_obj_set_style_text_font(list, &lv_font_montserrat_28, 0);
}


/* ── Forget‑WiFi confirmation popup ── */
static lv_obj_t *forget_overlay = NULL;

static void forget_confirm_cb(lv_event_t *e)
{
    storage_forget_wifi_credentials();
    wifi_service_disconnect();
    if(forget_overlay) { lv_obj_del(forget_overlay); forget_overlay = NULL; }
    wifi_list_screen_refresh();            /* redraw list without saved row */
}

static void forget_cancel_cb(lv_event_t *e)
{
    if(forget_overlay) { lv_obj_del(forget_overlay); forget_overlay = NULL; }
}

static void forget_clicked(lv_event_t *e)
{
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
    lv_label_set_text(msg, "Forget saved WiFi network?");
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

    /* ── Saved network row ── */
    char saved_ssid[33] = {0};
    char saved_pwd[65]  = {0};
    if(storage_load_wifi_credentials(saved_ssid, sizeof(saved_ssid),
                                     saved_pwd, sizeof(saved_pwd)))
    {
        /* Section header */
        lv_obj_t *sec = lv_label_create(list);
        lv_obj_set_style_text_font(sec, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(sec, COLOR_MUTED, 0);
        lv_label_set_text(sec, "SAVED NETWORK");
        lv_obj_set_style_pad_top(sec, 8, 0);
        lv_obj_set_style_pad_bottom(sec, 2, 0);

        /* Row container */
        lv_obj_t *row = lv_obj_create(list);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, lv_pct(100), 60);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x1e293b), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_pad_all(row, 8, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        /* SSID + status */
        wifi_state_t ws = wifi_service_state();
        const char *status_icon = (ws == WIFI_CONNECTED) ? LV_SYMBOL_WIFI : LV_SYMBOL_WARNING;
        const char *status_txt  = (ws == WIFI_CONNECTED) ? " Connected"
                                : (ws == WIFI_CONNECTING) ? " Connecting..."
                                : " Saved";
        lv_obj_t *lbl = lv_label_create(row);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
        lv_obj_set_flex_grow(lbl, 1);
        String display_str = String(status_icon) + " " + saved_ssid + status_txt;
        lv_label_set_text(lbl, display_str.c_str());

        /* Forget button */
        lv_obj_t *fbtn = lv_btn_create(row);
        lv_obj_add_style(fbtn, &g_styles.btn_danger, 0);
        lv_obj_set_size(fbtn, 130, 44);
        lv_obj_t *flbl = lv_label_create(fbtn);
        lv_obj_set_style_text_font(flbl, &lv_font_montserrat_16, 0);
        lv_label_set_text(flbl, LV_SYMBOL_TRASH " FORGET");
        lv_obj_center(flbl);
        lv_obj_add_event_cb(fbtn, forget_clicked, LV_EVENT_RELEASED, NULL);

        /* Separator label */
        lv_obj_t *sep = lv_label_create(list);
        lv_obj_set_style_text_font(sep, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(sep, COLOR_MUTED, 0);
        lv_label_set_text(sep, "AVAILABLE NETWORKS");
        lv_obj_set_style_pad_top(sep, 12, 0);
        lv_obj_set_style_pad_bottom(sep, 2, 0);
    }

    /* ── Scanned networks ── */
    uint8_t count = wifi_service_get_ap_count();
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
    if(count > MAX_SCAN_APS) count = MAX_SCAN_APS;
    for(int i=0;i<count;i++)
    {
        String ssid = wifi_service_get_ssid(i);
        strncpy(ssid_store[i], ssid.c_str(), 32);
        ssid_store[i][32] = 0;
        if(ssid_store[i][0] == 0) continue;  // skip empty/hidden SSIDs
        lv_obj_t *btn = lv_list_add_btn(list, LV_SYMBOL_WIFI, ssid_store[i]);
        lv_obj_add_event_cb(btn, ssid_clicked, LV_EVENT_RELEASED,
                            (void*)(uintptr_t)i);
    }
}

/* Timer callback — polls async scan status */
static void scan_poll_cb(lv_timer_t *t)
{
    int status = wifi_service_scan_status();
    if(status == -1) return;  /* still scanning */

    /* Scan finished (success or failure) — stop timer */
    lv_timer_del(t);
    scan_poll_timer = NULL;

    wifi_list_screen_refresh();
}

/* Start async scan and show scanning indicator */
void wifi_list_screen_start_scan(void)
{
    lv_obj_clean(list);

    /* Show scanning message */
    lv_obj_t *lbl = lv_label_create(list);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl, COLOR_MUTED, 0);
    lv_label_set_text(lbl, LV_SYMBOL_REFRESH "  Scanning for networks...");
    lv_obj_set_width(lbl, lv_pct(100));
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(lbl, 30, 0);

    /* Start async scan */
    wifi_service_start_scan();

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
