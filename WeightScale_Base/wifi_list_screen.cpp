#include <lvgl.h>

#ifdef LV_VERSION_MAJOR

#include "wifi_list_screen.h"
#include "wifi_service.h"
#include "ui_styles.h"

static lv_obj_t *scr;
static lv_obj_t *list;
static void (*back_cb)(void) = NULL;
static void (*select_cb)(const char*) = NULL;

static void ssid_clicked(lv_event_t *e)
{
    if(!select_cb) return;
    char *ssid = (char*)lv_event_get_user_data(e);
    select_cb(ssid);

    free(ssid);//new
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
    lv_obj_set_size(header,800,80);
    lv_obj_align(header,LV_ALIGN_TOP_MID,0,0);

    lv_label_set_text(lv_label_create(header),"SELECT WIFI");

    lv_obj_t *back = lv_btn_create(header);
    lv_obj_add_style(back,&g_styles.btn_secondary,0);
    lv_obj_align(back,LV_ALIGN_RIGHT_MID,-10,0);
    lv_label_set_text(lv_label_create(back),"BACK");
    lv_obj_add_event_cb(back,back_evt,LV_EVENT_CLICKED,NULL);

    list = lv_list_create(scr);
    lv_obj_set_size(list,760,360);
    lv_obj_align(list,LV_ALIGN_BOTTOM_MID,0,-10);
}


void wifi_list_screen_refresh(void)
{
    lv_obj_clean(list);

    uint8_t count = wifi_service_get_ap_count();
    for(int i=0;i<count;i++)
    {
        String ssid = wifi_service_get_ssid(i);
        lv_obj_t *btn = lv_list_add_btn(list,LV_SYMBOL_WIFI,ssid.c_str());
        lv_obj_add_event_cb(btn,ssid_clicked,LV_EVENT_CLICKED,
                            (void*)strdup(ssid.c_str()));
                            //(void*)ssid.c_str());
    }
}

// ⭐ NEW FUNCTION TO SHOW THIS SCREEN
void wifi_list_screen_show(void)
{
    if(scr)
        lv_scr_load(scr);
}

#else  // LV_VERSION_MAJOR not defined - provide stubs

void wifi_list_screen_create(void *parent) {}
void wifi_list_screen_show(void) {}
void wifi_list_screen_refresh(void) {}
void wifi_list_screen_register_back(void (*cb)(void)) {}
void wifi_list_screen_register_select(void (*cb)(const char *ssid)) {}

#endif  // LV_VERSION_MAJOR
