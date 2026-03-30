#define LV_CONF_INCLUDE_SIMPLE

#include <Arduino.h>
#include <lvgl.h>

#include <Wire.h>
#include <SPI.h>

// Forward declare Board class to avoid including library headers
namespace esp_panel {
namespace board {
class Board;
}
}

#include "lvgl_v8_port.h"

#include "home_screen.h"
#include "settings_screen.h"
#include "calibration_screen.h"

#include "invoice_service.h"
#include "storage_service.h"
#include "wifi_service.h"
#include "ota_service.h"
#include "scale_service_v2.h"
#include "ui_events.h"
#include "ui_styles.h"
#include "device_name_screen.h"
#include "history_screen.h"
#include "sync_service.h"
#include "devlog.h"


/* =========================================================
   GLOBAL STATE
=========================================================*/

static lv_obj_t *home_scr = NULL;
static lv_obj_t *settings_scr = NULL;
static lv_obj_t *cal_scr = NULL;

static uint16_t qty = 1;
static float weight = 0.0f;
static bool ui_frozen = false;
static bool reset_pending = false;
bool wifi_critical_section = false;
static lv_obj_t *history_scr = NULL;


/* Industrial scale profiles */

static const scale_profile_t PROFILE_1KG =
{
    "RAW",
    1.0f,
    1.0,
    0.35f,
    0.002f,
 
    500
};

static const scale_profile_t PROFILE_100KG =
{
    "1KG",
   // 100.0f,
    //2280.0f,
    //0.25f,
    //0.02f,
    //1200
    1.0f,
    58281.3,
    0.35f,
    0.002f,
    500
};

static const scale_profile_t PROFILE_500KG =
{
    "500KG",
   // 500.0f,
    //49000.0f,
    //0.15f,
    //0.08f,
    //1800
     1.0f,
    //61287.5,//100=215052.297,83.5=179996.649 ,zero=164,83.5=180382.010 =214888.297
      //58281.3,
      //2148.8,
      2174.0,
    0.35f,
    //0.002f,
    0.08f,
    500
};

static lv_obj_t *device_scr = NULL;
static char device_name[64] = {0};


/* =========================================================
   RESET CONFIRMATION POPUP (INDUSTRIAL)
=========================================================*/

static lv_obj_t *reset_msgbox = NULL;

static lv_obj_t *admin_popup = NULL;
static lv_obj_t *admin_ta = NULL;

static void admin_close_async(void *p)
{
    if(admin_popup && lv_obj_is_valid(admin_popup))
        lv_obj_del(admin_popup);

    admin_popup = NULL;
}

static void admin_kb_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_READY)
    {
        const char *pwd = lv_textarea_get_text(admin_ta);

        if(strcmp(pwd,"1234") == 0)   // 🔐 CHANGE PASSWORD HERE
        {
            lv_async_call(admin_close_async,NULL);
            lv_scr_load(settings_scr);
        }
        else
        {
            /* WRONG PASSWORD MESSAGE */
            lv_async_call(admin_close_async,NULL);

            lv_obj_t *msg = lv_msgbox_create(NULL,"ACCESS DENIED",
                                             "Incorrect Password",
                                             NULL,true);
            lv_obj_center(msg);

            lv_timer_t *t = lv_timer_create(
                [](lv_timer_t *timer){
                    lv_obj_del((lv_obj_t*)timer->user_data);
                    lv_scr_load(home_scr);
                    lv_timer_del(timer);
                },
                1500,
                msg
            );
        }
    }

    if(code == LV_EVENT_CANCEL)
    {
        lv_async_call(admin_close_async,NULL);
        lv_scr_load(home_scr);
    }
}


static void show_admin_popup(void)
{
    admin_popup = lv_obj_create(NULL);
    lv_obj_add_style(admin_popup,&g_styles.screen,0);

    lv_obj_t *lbl = lv_label_create(admin_popup);
    lv_label_set_text(lbl,"ENTER ADMIN PASSWORD");
    lv_obj_align(lbl,LV_ALIGN_TOP_MID,0,40);

    admin_ta = lv_textarea_create(admin_popup);
    lv_textarea_set_password_mode(admin_ta,true);
    lv_textarea_set_one_line(admin_ta,true);
    lv_obj_set_width(admin_ta,400);
    lv_obj_align(admin_ta,LV_ALIGN_TOP_MID,0,100);

    lv_obj_t *kb = lv_keyboard_create(admin_popup);
    lv_keyboard_set_textarea(kb,admin_ta);
    lv_obj_add_event_cb(kb,admin_kb_event,LV_EVENT_ALL,NULL);
    lv_obj_align(kb,LV_ALIGN_BOTTOM_MID,0,0);

    lv_scr_load(admin_popup);
}


/* =========================================================
   UI EVENTS
=========================================================*/

static void ui_event(int evt)
{
    if (evt == UI_EVT_SETTINGS)
    {
        show_admin_popup();
    }

    if (evt == UI_EVT_HISTORY)
    {
        history_screen_refresh();
        lv_scr_load(history_scr);
    }

    if (evt == UI_EVT_QTY_INC)
    {
        qty++;
        home_screen_set_quantity(qty);
    }

    if (evt == UI_EVT_QTY_DEC && qty > 1)
    {
        qty--;
        home_screen_set_quantity(qty);
    }

    /* Handle remove-item events encoded as base + index */
    if(evt >= UI_EVT_REMOVE_ITEM_BASE && evt < UI_EVT_REMOVE_ITEM_BASE + MAX_INVOICE_ITEMS)
    {
        int idx = evt - UI_EVT_REMOVE_ITEM_BASE;
        invoice_session_remove((uint8_t)idx);
        home_screen_refresh_invoice_details();
    }

    if (evt == UI_EVT_SAVE)
    {
        if(invoice_session_add(weight, qty))
        {
            home_screen_refresh_invoice_details();
            qty = 1;
            home_screen_set_quantity(qty);
        }
    }

    if (evt == UI_EVT_RESET)
    {
        invoice_session_commit();
        invoice_service_next();
        home_screen_set_invoice(invoice_service_current_id());
        home_screen_refresh_invoice_details();
    }

    if(evt == UI_EVT_RESET_ALL)
    {
        invoice_session_clear();
        storage_clear_all_records();

        /* RESET INVOICE TO 1 */
        storage_save_invoice(1);
        invoice_service_init();

        home_screen_set_invoice(invoice_service_current_id());
        home_screen_refresh_invoice_details();
    }

    if(evt == 1002)
    {
        qty *= 2;
        home_screen_set_quantity(qty);
    }

    if(evt == 1005)
    {
        qty *= 5;
        home_screen_set_quantity(qty);
    }

    if(evt == 1010)
    {
        qty *= 10;
        home_screen_set_quantity(qty);
    }
    if(evt == 2001)
    {
        lv_scr_load(cal_scr);
    }


}


/* =========================================================
   SCREEN NAVIGATION
=========================================================*/

static void open_calibration()
{
  ////// if(display_cb) display_cb("[CAL] Open calibration screen");
    devlog_printf("[CAL] Open calibration screen");
    lv_scr_load(cal_scr);
}

static void back_cb(void)
{
    lv_scr_load(home_scr);
}

static void device_name_saved(int evt, const char *name)
{
    if(evt != DEVNAME_EVT_SAVE) return;
    Serial.printf("[DEV] Saved: %s\n", name);
    devlog_printf("[DEV] Saved: %s", name);

    storage_save_device_name(name);

    /* ✅ CRITICAL — update UI NOW */
    home_screen_set_device(name);

    lv_scr_load(home_scr);
}



static void calibration_wizard_event(int evt)
{
    switch (evt)
    {
        case CAL_EVT_BACK:
            lv_scr_load(home_scr);
            break;
        case CAL_EVT_PROFILE_1KG:
            scale_service_set_profile(&PROFILE_1KG);
            break;

        case CAL_EVT_PROFILE_100KG:
            scale_service_set_profile(&PROFILE_100KG);
            break;

        case CAL_EVT_PROFILE_500KG:
            scale_service_set_profile(&PROFILE_500KG);
            break;    

        case CAL_EVT_CAPTURE_ZERO:
            scale_service_tare();
          ////// if(display_cb) display_cb("[CAL] Tare");
            devlog_printf("[CAL] Tare");
            break;

        case CAL_EVT_CAPTURE_LOAD:
         // //// if(display_cb) display_cb("[CAL] Capture load (TODO)");
            devlog_printf("[CAL] Capture load (TODO)");
            break;

        case CAL_EVT_SAVE:
          ////// if(display_cb) display_cb("[CAL] Save calibration (TODO)");
            devlog_printf("[CAL] Save calibration (TODO)");
            break;
    }
}


/* =========================================================
   CALIBRATION CALLBACKS
=========================================================*/

static void calib_offset()
{
  ////// if(display_cb) display_cb("[CAL] OFFSET (tare)");
    devlog_printf("[CAL] OFFSET (tare)");
    scale_service_tare();
}

static void calib_scale()
{
  ////// if(display_cb) display_cb("[CAL] SCALE requested");
    devlog_printf("[CAL] SCALE requested");
}

static void calib_both()
{
    calib_offset();
    calib_scale();
}

/* =========================================================
   WEIGHT UPDATE LOOP
=========================================================*/

static void update_weight()
{
    if (ui_frozen) return;   // 🔥 ABSOLUTE RULE

    weight = scale_service_get_weight();

    if (lv_scr_act() == home_scr)
    {
        home_screen_set_weight(weight);
    }

    if (lv_scr_act() == cal_scr)
    {
        calibration_screen_set_live(
            weight,
            scale_service_get_raw()
        );
    }
    static float last_weight = 0;

    static float stable_weight = 0;
    static uint32_t stable_start = 0;

    last_weight = weight;

}



/* =========================================================
   SETUP
=========================================================*/

void setup()
{
    Serial.begin(115200);
    delay(500);

    devlog_printf("=== INDUSTRIAL SCALE START ===");

    // Note: Display panel initialization via Board class requires  
    // esp_panel::board::Board implementation from ESP32_Display_Panel library
    // For now, LVGL is initialized without display driver
    // This allows compilation while display integration is being configured
    
    // static esp_panel::board::Board *board = nullptr;
    // board = new esp_panel::board::Board();
    // board->init();
    // board->begin();

    Serial.println("Initializing LVGL...");
    lvgl_port_init(nullptr, nullptr);

    lvgl_port_lock(-1);
    ui_styles_init();
    lvgl_port_unlock();
    storage_service_init();
    devlog_init();
    
    /* Load saved logs from previous session */
    devlog_load_from_storage();
    
    if(storage_load_dev_mode())
    {
        devlog_printf("[SYSTEM] Developer mode enabled at boot");
    }
    invoice_service_init();
    sync_service_init();
    wifi_service_init();
    ota_service_init();

    invoice_session_init();

    /* START RTOS SCALE SERVICE */
    scale_service_init();
    scale_service_set_profile(&PROFILE_1KG);

    /* CREATE SCREENS */

    home_scr     = lv_obj_create(NULL);
    settings_scr = lv_obj_create(NULL);
    cal_scr      = lv_obj_create(NULL);

    home_screen_create(home_scr);
    home_screen_register_callback(ui_event);
/* Get current firmware version */
//const char* version = ota_service_current_version();
String version = ota_service_current_version();
//Serial.printf("[SYSTEM] Firmware version: %s\n", version);
//devlog_printf("[SYSTEM] Firmware version: %s", version);
Serial.printf("[SYSTEM] Firmware version: %s\n", version.c_str());
devlog_printf("[SYSTEM] Firmware version: %s", version.c_str());
String stored_version = ota_service_stored_version();
devlog_printf("Last OTA applied: %s", stored_version.c_str());
/* Send version to UI */
//home_screen_set_version(version);
//home_screen_set_version(ota_service_current_version());
home_screen_set_version(ota_service_current_version().c_str());

    settings_screen_create(settings_scr);
    settings_screen_register_back_callback(back_cb);
    settings_screen_register_calibration_callback(open_calibration);

    calibration_screen_create(cal_scr);
    calibration_screen_register_callback(calibration_wizard_event);


    /* INITIAL UI */

    home_screen_set_quantity(qty);
    home_screen_set_weight(0);
    home_screen_set_invoice(invoice_service_current_id());
    device_scr = lv_obj_create(NULL);
    device_name_screen_create(device_scr);
    device_name_screen_register_callback(device_name_saved);


    if(storage_load_device_name(device_name,sizeof(device_name)))
    {
      ////// if(display_cb) display_cb("[DEVICE] Existing name found");
        devlog_printf("[DEVICE] Existing name found");
        home_screen_set_device(device_name);
       // lv_scr_load(home_scr);
    }
    else
    {
      ////// if(display_cb) display_cb("[DEVICE] First boot — asking name");
        devlog_printf("[DEVICE] First boot — asking name");
        //lv_scr_load(device_scr);
       // strcpy(device_name, "SCALE-01");   // 🔹 DEFAULT NAME HERE
        uint64_t chipid = ESP.getEfuseMac();
          sprintf(device_name, "SCALE-%04X", (uint16_t)(chipid & 0xFFFF));
        storage_save_device_name(device_name);
   // storage_save_device_name(device_name);

    home_screen_set_device(device_name);
    }
     lv_scr_load(home_scr);
    history_scr = lv_obj_create(NULL);
    history_screen_create(history_scr);
    history_screen_register_back(back_cb);


}

/* =========================================================
   LOOP
=========================================================*/

void loop()
{
    // LVGL is handled by lvgl_port task in esp_panel::lvgl_v8_port.

    update_weight();
    wifi_service_loop();
    sync_service_loop();
    if(lv_scr_act() == settings_scr)
    {
        settings_screen_update_wifi_status();
    }
    invoice_service_daily_reset_if_needed();
    storage_check_new_day_and_reset();

    if (lv_scr_act() == home_scr)
    {
        home_screen_set_sync_status(
            wifi_service_state() == WIFI_CONNECTED ?
            "Online" : "Offline"
        );
    }
}
