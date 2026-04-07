#include <lvgl.h>

// Ensure LVGL is detected (same guard as other screen files)
#ifndef LV_VERSION_MAJOR
#define LV_VERSION_MAJOR 8
#endif

#ifdef LV_VERSION_MAJOR
#include "ui_styles.h"
#include "wifi_service.h"
#include "wifi_list_screen.h"
#include "devlog.h"

static char saved_password[65] = {0};

// ⭐ OPTIMIZATION: Single reusable buffer for text (saves ~64 bytes)
static char g_wpop_buf[64];

struct wifi_popup_t {
    lv_obj_t *scr;
    lv_obj_t *ta;
    lv_obj_t *kb;

    lv_timer_t *connect_delay_timer;
    lv_timer_t *result_timer;

    bool destroyed;
    int  poll_count;     // how many times result has been polled

    char ssid[33];
};

/* =======================================================
   SAFE DESTROY (ALWAYS ASYNC)
======================================================= */

static void wifi_popup_destroy_async(void *p)
{
    wifi_popup_t *wp = (wifi_popup_t*)p;
    if(!wp) return;

    if(wp->destroyed) return;
    wp->destroyed = true;

    if(wp->connect_delay_timer)
        lv_timer_del(wp->connect_delay_timer);

    if(wp->result_timer)
        lv_timer_del(wp->result_timer);

    if(wp->scr && lv_obj_is_valid(wp->scr))
        lv_obj_del(wp->scr);

    wifi_service_set_debug_label(NULL);

    delete wp;
}

/* =======================================================
   CHECK CONNECTION RESULT
======================================================= */

static void wifi_check_result(lv_timer_t *t)
{
    wifi_popup_t *wp = (wifi_popup_t*)t->user_data;
    if(!wp || wp->destroyed) {
        lv_timer_del(t);
        return;
    }

    wp->poll_count++;

    if(wifi_service_state() == WIFI_CONNECTED)
    {
        wp->result_timer = NULL;
        lv_timer_del(t);
        devlog_printf("[WIFI POPUP] Connected OK (poll %d)", wp->poll_count);
        lv_async_call(wifi_popup_destroy_async, wp);
        wifi_list_screen_show();
    }
    else if(wifi_service_state() == WIFI_DISCONNECTED || wp->poll_count >= 30)
    {
        /* wifi_service_loop() already set DISCONNECTED on failure/timeout,
           or we've polled 15 times (30s) without success.                */
        wp->result_timer = NULL;
        lv_timer_del(t);
        devlog_printf("[WIFI POPUP] Connection failed (poll %d)", wp->poll_count);
        lv_async_call(wifi_popup_destroy_async, wp);
        wifi_list_screen_show();
    }
    /* else: still WIFI_CONNECTING — keep polling */
}

/* =======================================================
   KEYBOARD EVENT
======================================================= */

static void wifi_popup_kb_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    wifi_popup_t *wp = (wifi_popup_t*)lv_event_get_user_data(e);

    if(!wp || wp->destroyed) return;

    if(code == LV_EVENT_READY)
    {
        const char *ta_text = lv_textarea_get_text(wp->ta);
        devlog_printf("[WIFI POPUP] TA text='%s' len=%d for %s",
                      ta_text ? ta_text : "(null)",
                      ta_text ? (int)strlen(ta_text) : -1,
                      wp->ssid);

        strncpy(saved_password,
                ta_text ? ta_text : "",
                sizeof(saved_password));
        saved_password[sizeof(saved_password)-1] = 0;

        devlog_printf("[WIFI POPUP] Password entered for %s: '%s'", wp->ssid, saved_password);

        lv_obj_t *debug_label = lv_label_create(wp->scr);
        lv_label_set_text(debug_label, "Connecting...");
        lv_obj_align(debug_label, LV_ALIGN_BOTTOM_MID, 0, -20);

        wifi_service_set_debug_label(debug_label);

        wp->connect_delay_timer =
            lv_timer_create(
                [](lv_timer_t *t)
                {
                    wifi_popup_t *wp_timer =
                        (wifi_popup_t*)t->user_data;

                    if(!wp_timer || wp_timer->destroyed)
                    {
                        lv_timer_del(t);
                        return;
                    }

                    wp_timer->connect_delay_timer = NULL;
                    lv_timer_del(t);

                    wifi_service_connect(
                        wp_timer->ssid,
                        saved_password
                    );

                    wp_timer->result_timer =
                        lv_timer_create(
                            wifi_check_result,
                            2000,
                            wp_timer
                        );
                },
                300,
                wp
            );
    }

    if(code == LV_EVENT_CANCEL)
    {
        devlog_printf("[WIFI POPUP] Cancel pressed");
        /* Synchronously kill timers NOW — async destroy runs later and may be
           too late if connect_delay_timer fires in the meantime (triggering
           an unintended wifi_service_connect for the wrong SSID).           */
        if(wp->connect_delay_timer) {
            lv_timer_del(wp->connect_delay_timer);
            wp->connect_delay_timer = NULL;
        }
        if(wp->result_timer) {
            lv_timer_del(wp->result_timer);
            wp->result_timer = NULL;
        }
        wp->destroyed = true;  /* prevent any remaining callbacks */
        lv_async_call(wifi_popup_destroy_async, wp);
        wifi_list_screen_show();
    }
}

/* =======================================================
   SHOW POPUP
======================================================= */

void wifi_password_popup_show(const char *ssid)
{
    if(!ssid) return;

    wifi_popup_t *wp = new wifi_popup_t();
    memset(wp, 0, sizeof(wifi_popup_t));

    strncpy(wp->ssid, ssid, sizeof(wp->ssid));
    wp->ssid[sizeof(wp->ssid)-1] = 0;

    wp->scr = lv_obj_create(NULL);
    lv_obj_add_style(wp->scr, &g_styles.screen, 0);

    lv_obj_t *title = lv_label_create(wp->scr);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x38BDF8), 0);
    snprintf(g_wpop_buf, sizeof(g_wpop_buf),
             LV_SYMBOL_WIFI " Password for %s", wp->ssid);
    lv_label_set_text(title, g_wpop_buf);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    wp->ta = lv_textarea_create(wp->scr);
    lv_textarea_set_password_mode(wp->ta, false);  // DEBUG: show chars to verify input
    lv_textarea_set_one_line(wp->ta, true);
    lv_obj_set_size(wp->ta, 700, 60);
    lv_obj_set_style_text_font(wp->ta, &lv_font_montserrat_28, 0);
    lv_obj_set_style_bg_color(wp->ta, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_bg_opa(wp->ta, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(wp->ta, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_color(wp->ta, lv_color_hex(0x2563EB), 0);
    lv_obj_set_style_border_width(wp->ta, 2, 0);
    lv_obj_set_style_radius(wp->ta, 10, 0);
    lv_obj_set_style_pad_all(wp->ta, 10, 0);
    lv_obj_align(wp->ta, LV_ALIGN_TOP_MID, 0, 55);

    wp->kb = lv_keyboard_create(wp->scr);
    lv_keyboard_set_textarea(wp->kb, wp->ta);
    lv_obj_set_height(wp->kb, 340);
    lv_obj_set_style_text_font(wp->kb, &lv_font_montserrat_20, 0);
    lv_obj_align(wp->kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_style(wp->kb, &g_styles.kb_bg, LV_PART_MAIN);
    lv_obj_add_style(wp->kb, &g_styles.kb_btn, LV_PART_ITEMS);

    lv_obj_add_event_cb(
        wp->kb,
        wifi_popup_kb_event,
        LV_EVENT_READY,
        wp
    );
    lv_obj_add_event_cb(
        wp->kb,
        wifi_popup_kb_event,
        LV_EVENT_CANCEL,
        wp
    );

    lv_scr_load(wp->scr);
}

#else  // LV_VERSION_MAJOR not defined - provide stub

#include "devlog.h"
#include "wifi_service.h"

void wifi_password_popup_show(const char *ssid)
{
    if(!ssid) return;
    devlog_printf("[WIFI POPUP STUB] Show popup for %s (LVGL not available)", ssid);
}

#endif  // LV_VERSION_MAJOR
