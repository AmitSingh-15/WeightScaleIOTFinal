#ifdef __cplusplus
extern "C" {
#endif
void wifi_recover(void);
#ifdef __cplusplus
}
#endif

#include "app_config.h"

#if ENABLE_WIFI_SERVICE

#include <lvgl.h>
#include "lvgl_v8_port.h"
#include "wifi_service.h"
#include <WiFi.h>
#include "devlog.h"
#include "storage_service.h"
#include "scale_service_v2.h"
#include "sync_service.h"

/* ================= STATE ================= */

static lv_obj_t *wifi_debug_label = NULL;
static bool wifi_critical_section = false;
static wifi_state_t state = WIFI_DISCONNECTED;

static int scan_count = 0;
static bool scan_in_progress = false;
static uint32_t last_scan_time = 0;

static bool scan_requested = false;

static unsigned long connect_start_ms = 0;
static const unsigned long CONNECT_TIMEOUT_MS = 45000;

/* Callbacks */
static void (*state_cbs[6])(wifi_state_t) = {0};
static int state_cb_count = 0;

/* Deferred connect */
static bool connect_request = false;
static char req_ssid[33] = {0};
static char req_pwd[65]  = {0};

/* Auto-connect */
static bool auto_connect_pending = false;
static unsigned long auto_connect_after_ms = 0;
static int auto_connect_retries = 0;
static const int AUTO_CONNECT_MAX_RETRIES = 3;
static const unsigned long AUTO_CONNECT_SCHEDULE_MS[] = {60000,180000,300000};
static unsigned long boot_millis = 0;

/* ================= CALLBACK ================= */

void wifi_service_register_state_callback(void (*cb)(wifi_state_t))
{
    if(!cb) return;

    for(int i=0;i<state_cb_count;i++)
        if(state_cbs[i]==cb) return;

    if(state_cb_count < 6)
        state_cbs[state_cb_count++] = cb;
}

static void set_state(wifi_state_t s)
{
    if(state == s) return;

    state = s;

    if(lvgl_port_lock(100)) {
        for(int i=0;i<state_cb_count;i++)
            if(state_cbs[i]) state_cbs[i](s);
        lvgl_port_unlock();
    }
}

/* ================= DEBUG ================= */

void wifi_service_set_debug_label(lv_obj_t *label)
{
    wifi_debug_label = label;
}

static void wifi_debug(const char *msg)
{
    if(!wifi_debug_label) return;

    if(lvgl_port_lock(100)) {
        if(lv_obj_is_valid(wifi_debug_label))
            lv_label_set_text(wifi_debug_label, msg);
        lvgl_port_unlock();
    }
}

/* ================= INIT ================= */

void wifi_service_init(void)
{
    WiFi.mode(WIFI_STA);
    delay(3000);
    WiFi.setSleep(false);
    delay(500);

    state = WIFI_DISCONNECTED;

    devlog_printf("[WIFI] STA init");

    boot_millis = millis();

    char ssid[33]={0}, pwd[65]={0};

    if(storage_load_wifi_credentials(ssid,sizeof(ssid),pwd,sizeof(pwd)))
    {
        strncpy(req_ssid,ssid,sizeof(req_ssid));
        strncpy(req_pwd,pwd,sizeof(req_pwd));

        auto_connect_pending = true;
        auto_connect_after_ms = boot_millis + AUTO_CONNECT_SCHEDULE_MS[0];
    }
}

/* ================= SCAN ================= */

void wifi_service_start_scan(void)
{
    if(scan_in_progress) return;

    if(millis() - last_scan_time < 10000) return;

    scan_requested = true;
    scan_in_progress = true;
    scan_count = -1;

    wifi_debug("Scanning...");
}

int wifi_service_scan_status(void)
{
    if(scan_in_progress) return -1;
    return scan_count;
}
/* ================= CONNECT ================= */

void wifi_service_connect(const char *ssid, const char *password)
{
    if(!ssid || !ssid[0]) return;

    strncpy(req_ssid,ssid,sizeof(req_ssid));
    strncpy(req_pwd,password?password:"",sizeof(req_pwd));

    connect_request = true;
    wifi_debug("Connect req");
}

/* ================= GETTERS ================= */

wifi_state_t wifi_service_state(void)
{
    return state;
}

uint8_t wifi_service_get_ap_count(void)
{
    return scan_count > 0 ? scan_count : 0;
}

String wifi_service_get_ssid(uint8_t i)
{
    if(i >= scan_count) return "";
    return WiFi.SSID(i);
}

/* ================= LOOP ================= */

void wifi_service_loop(void)
{
    /* Auto connect */
    if(auto_connect_pending && millis() >= auto_connect_after_ms)
    {
        auto_connect_pending = false;
        connect_request = true;
    }

    /* Connect */
    if(connect_request)
    {
        connect_request = false;

        wifi_critical_section = true;
        scale_service_suspend();

        delay(200);
        WiFi.begin(req_ssid, req_pwd);

        wifi_critical_section = false;

        set_state(WIFI_CONNECTING);
        connect_start_ms = millis();
    }

    /* Connection state */
    if(state == WIFI_CONNECTING)
    {
        wl_status_t s = WiFi.status();

        if(s == WL_CONNECTED)
        {
            storage_save_wifi_credentials(req_ssid, req_pwd);
            scale_service_resume();
            set_state(WIFI_CONNECTED);
        }
        else if(millis() - connect_start_ms > CONNECT_TIMEOUT_MS)
        {
            WiFi.disconnect(false);
            scale_service_resume();
            set_state(WIFI_DISCONNECTED);
        }
    }

    /* Scan handling */
    if(scan_requested)
    {
        scan_requested = false;
        WiFi.scanNetworks(true);
    }

    int res = WiFi.scanComplete();

    if(res >= 0)
    {
        scan_count = res;
        scan_in_progress = false;
        last_scan_time = millis();
    }
}

/* ================= UTILS ================= */

bool wifi_service_is_critical(void)
{
    return wifi_critical_section;
}

void wifi_service_disconnect(void)
{
    WiFi.disconnect(false);
    set_state(WIFI_DISCONNECTED);
    auto_connect_pending = false;
    connect_request = false;
}

#endif