#pragma once

#ifdef LV_VERSION_MAJOR
#include <lvgl.h>

extern "C" {

void wifi_list_screen_create(lv_obj_t *parent);
void wifi_list_screen_show(void);
void wifi_list_screen_refresh(void);
void wifi_list_screen_start_scan(void);
void wifi_list_screen_register_back(void (*cb)(void));
void wifi_list_screen_register_select(void (*cb)(const char *ssid));

}  // extern "C"

#else

extern "C" {

void wifi_list_screen_create(void *parent);
void wifi_list_screen_show(void);
void wifi_list_screen_refresh(void);
void wifi_list_screen_start_scan(void);
void wifi_list_screen_register_back(void (*cb)(void));
void wifi_list_screen_register_select(void (*cb)(const char *ssid));

}  // extern "C"

#endif
