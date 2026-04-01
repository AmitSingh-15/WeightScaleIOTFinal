#ifdef LV_VERSION_MAJOR

#include <lvgl.h>
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

    wp->result_timer = NULL;
    lv_timer_del(t);

    if(wifi_service_state() == WIFI_CONNECTED)
    {
        devlog_printf("[WIFI POPUP] Connected OK");
        lv_async_call(wifi_popup_destroy_async, wp);
        wifi_list_screen_show();
    }
    else
    {
        devlog_printf("[WIFI POPUP] Connection failed");
        lv_async_call(wifi_popup_destroy_async, wp);
        wifi_list_screen_show();
    }
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
        strncpy(saved_password,
                lv_textarea_get_text(wp->ta),
                sizeof(saved_password));
        saved_password[sizeof(saved_password)-1] = 0;

        devlog_printf("[WIFI POPUP] Password entered for %s", wp->ssid);

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
                            8000,
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
    snprintf(g_wpop_buf, sizeof(g_wpop_buf),
             "Password for %s", wp->ssid);
    lv_label_set_text(title, g_wpop_buf);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    wp->ta = lv_textarea_create(wp->scr);
    lv_textarea_set_password_mode(wp->ta, true);
    lv_textarea_set_one_line(wp->ta, true);
    lv_obj_set_width(wp->ta, 520);
    lv_obj_align(wp->ta, LV_ALIGN_TOP_MID, 0, 80);

    wp->kb = lv_keyboard_create(wp->scr);
    lv_keyboard_set_textarea(wp->kb, wp->ta);
    lv_obj_align(wp->kb, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_add_event_cb(
        wp->kb,
        wifi_popup_kb_event,
        LV_EVENT_ALL,
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
