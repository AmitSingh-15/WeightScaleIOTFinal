#include "app_config.h"  /* Must be first for feature flags */

#if ENABLE_WIFI_SERVICE

#include "wifi_service.h"
#include <WiFi.h>

#ifdef LV_VERSION_MAJOR
#include <lvgl.h>
#endif
#include "devlog.h"

#ifdef LV_VERSION_MAJOR
static lv_obj_t *wifi_debug_label = NULL;
#else
static void *wifi_debug_label = NULL;
#endif



static bool wifi_critical_section = false;   // ✅ FIX
static wifi_state_t state = WIFI_DISCONNECTED;
static int scan_count = 0;
static String connected_ssid = "";

// Connection timeout handling
static unsigned long connect_start_ms = 0;
static const unsigned long CONNECT_TIMEOUT_MS = 30000; // 30s
// State change callbacks (support multiple listeners)
static void (*state_cbs[6])(wifi_state_t) = {0};
static int state_cb_count = 0;

void wifi_service_register_state_callback(void (*cb)(wifi_state_t))
{
    if(!cb) return;
    // avoid duplicates
    for(int i=0;i<state_cb_count;i++) if(state_cbs[i] == cb) return;
    if(state_cb_count < (int)(sizeof(state_cbs)/sizeof(state_cbs[0])))
        state_cbs[state_cb_count++] = cb;
}

static void set_state(wifi_state_t s)
{
    if(state == s) return;
    state = s;
    for(int i=0;i<state_cb_count;i++)
    {
        if(state_cbs[i]) state_cbs[i](s);
    }
}

/* ===== deferred connect ===== */
static bool connect_request = false;
static char req_ssid[33] = {0};
static char req_pwd[65]  = {0};

/* =====================================================
   INIT
=====================================================*/

//#include "libraries/lvgl/lvgl.h"

//static lv_obj_t *wifi_debug_label = NULL;

#ifdef LV_VERSION_MAJOR
void wifi_service_set_debug_label(lv_obj_t *label)
{
    wifi_debug_label = label;
}
#else
void wifi_service_set_debug_label(void *label)
{
    wifi_debug_label = label;
}
#endif

static void wifi_debug(const char *msg)
{
    #ifdef LV_VERSION_MAJOR
    if (wifi_debug_label && lv_obj_is_valid(wifi_debug_label))
    {
        lv_label_set_text(wifi_debug_label, msg);
    }
    #endif
}

void wifi_service_init(void)
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    state = WIFI_DISCONNECTED;
}

/* =====================================================
   SCAN (blocking, stable)
=====================================================*/

void wifi_service_start_scan(void)
{
    devlog_printf("[WIFI] Start scan");
    wifi_debug("Scanning...");

    WiFi.scanDelete();
    scan_count = WiFi.scanNetworks(false);

    char buf[64];
    snprintf(buf, sizeof(buf), "Scan done: %d APs", scan_count);
    devlog_printf("[WIFI] %s", buf);
    wifi_debug(buf);
}

/* =====================================================
   CONNECT (PUBLIC API — SAFE)
=====================================================*/

void wifi_service_connect(const char *ssid, const char *password)
{
    if(!ssid || !ssid[0]) return;

    strncpy(req_ssid, ssid, sizeof(req_ssid));
    req_ssid[sizeof(req_ssid)-1] = 0;          // ✅ ADDED SAFETY

    strncpy(req_pwd, password ? password : "", sizeof(req_pwd));
    req_pwd[sizeof(req_pwd)-1] = 0;            // ✅ ADDED SAFETY

    connect_request = true;

    connect_start_ms = millis();
    wifi_debug("Connect requested...");        // ✅ ADDED
}

/* =====================================================
   ACCESS POINT LIST
=====================================================*/

uint8_t wifi_service_get_ap_count(void)
{
    return scan_count > 0 ? (uint8_t)scan_count : 0;
}

String wifi_service_get_ssid(uint8_t index)
{
    if(index >= scan_count) return "";
    return WiFi.SSID(index);
}

/* =====================================================
   STATE
=====================================================*/

wifi_state_t wifi_service_state(void)
{
    return state;
}

/* =====================================================
   LOOP (ONLY place WiFi actually starts)
=====================================================*/

//extern bool wifi_critical_section;

void wifi_service_loop(void)
{
    if(connect_request)
    {
        connect_request = false;
        devlog_printf("[WIFI] Entering critical section for %s", req_ssid);
        wifi_debug("Connecting...");

        wifi_critical_section = true;
        delay(150);   // 🔥 let RGB DMA stop
        vTaskDelay(pdMS_TO_TICKS(150));
        WiFi.setSleep(false);
        WiFi.scanDelete();
        
        WiFi.begin(req_ssid, req_pwd);

        set_state(WIFI_CONNECTING);
        connect_start_ms = millis();
    }

    if(state == WIFI_CONNECTING)
    {
        wl_status_t s = WiFi.status();

        if(s == WL_CONNECTED)
        {
            devlog_printf("[WIFI] Connected to %s", WiFi.SSID().c_str());
            wifi_debug("WiFi Connected");
            set_state(WIFI_CONNECTED);

            delay(50);   // 🔥 let WiFi stabilize
            wifi_critical_section = false;
        }
        else
        {
            // Treat explicit connect-failed or timeout as failure
            if(s == WL_CONNECT_FAILED || (millis() - connect_start_ms) > CONNECT_TIMEOUT_MS)
            {
                devlog_printf("[WIFI] Connection failed to %s", req_ssid);
                wifi_debug("WiFi Failed");
                set_state(WIFI_DISCONNECTED);
                wifi_critical_section = false;
            }
        }
    }
}
bool wifi_service_is_critical(void)
{
    return wifi_critical_section;
}

#endif  /* ENABLE_WIFI_SERVICE */
