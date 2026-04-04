#include "app_config.h"  /* Must be first for feature flags */

#if ENABLE_WIFI_SERVICE

#include <lvgl.h>          /* Must be before wifi_service.h so LV_VERSION_MAJOR is defined */
#include "lvgl_v8_port.h"   /* lvgl_port_lock/unlock — recursive mutex, safe from any context */
#include "wifi_service.h"
#include <WiFi.h>
#include "devlog.h"
#include "storage_service.h"
#include "scale_service_v2.h"

static lv_obj_t *wifi_debug_label = NULL;



static bool wifi_critical_section = false;
static wifi_state_t state = WIFI_DISCONNECTED;
static int scan_count = 0;
static String connected_ssid = "";

/* Scan state machine (polled from wifi_service_loop) */
static bool scan_requested = false;
static bool scan_running  = false;
static unsigned long scan_start_ms = 0;
static const unsigned long SCAN_TIMEOUT_MS = 20000;  // 20 s max
static int scan_retry_count = 0;
static const int SCAN_MAX_RETRIES = 5;
static unsigned long scan_retry_ms = 0;   // when to retry (0 = now)

// Connection timeout handling
static unsigned long connect_start_ms = 0;
static const unsigned long CONNECT_TIMEOUT_MS = 60000; // 60s
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
    /* Callbacks may update LVGL labels (e.g. home_screen_set_sync_status).
       Must hold the LVGL lock since this runs from the Arduino main loop,
       not the LVGL render task.  Lock is recursive so safe from any context. */
    if(lvgl_port_lock(100)) {
        for(int i=0;i<state_cb_count;i++)
        {
            if(state_cbs[i]) state_cbs[i](s);
        }
        lvgl_port_unlock();
    }
}

/* ===== deferred connect ===== */
static bool connect_request = false;
static char req_ssid[33] = {0};
static char req_pwd[65]  = {0};

/* ===== deferred auto-connect (wait for SDIO driver to be ready) ===== */
static bool auto_connect_pending = false;
static unsigned long auto_connect_after_ms = 0;
static int auto_connect_retries = 0;
static const int AUTO_CONNECT_MAX_RETRIES = 2;  // try twice total

/* =====================================================
   INIT
=====================================================*/

//#include "libraries/lvgl/lvgl.h"

//static lv_obj_t *wifi_debug_label = NULL;

void wifi_service_set_debug_label(lv_obj_t *label)
{
    wifi_debug_label = label;
}

static void wifi_debug(const char *msg)
{
    if(!wifi_debug_label) return;          // quick exit, no lock needed
    if(lvgl_port_lock(100)) {
        if (wifi_debug_label && lv_obj_is_valid(wifi_debug_label))
            lv_label_set_text(wifi_debug_label, msg);
        lvgl_port_unlock();
    }
}

void wifi_service_init(void)
{
    WiFi.mode(WIFI_STA);
    delay(1000);                     // let SDIO hosted driver settle fully
    state = WIFI_DISCONNECTED;
    devlog_printf("[WIFI] STA mode initialized");

    /* Schedule auto-connect for 5 seconds after boot (SDIO driver needs time) */
    char saved_ssid[33] = {0};
    char saved_pwd[65] = {0};
    if(storage_load_wifi_credentials(saved_ssid, sizeof(saved_ssid), saved_pwd, sizeof(saved_pwd)))
    {
        devlog_printf("[WIFI] Saved network found: %s (will auto-connect in 10s)", saved_ssid);
        strncpy(req_ssid, saved_ssid, sizeof(req_ssid));
        req_ssid[sizeof(req_ssid)-1] = 0;
        strncpy(req_pwd, saved_pwd, sizeof(req_pwd));
        req_pwd[sizeof(req_pwd)-1] = 0;
        auto_connect_pending = true;
        auto_connect_retries = 0;
        auto_connect_after_ms = millis() + 10000;
    }
}

/* =====================================================
   SCAN — async, polled from wifi_service_loop()
   No separate task (avoids SDIO driver rx-queue issues)
=====================================================*/

void wifi_service_start_scan(void)
{
    if(scan_running) return;          // already scanning

    devlog_printf("[WIFI] Scan requested");
    wifi_debug("Scanning...");
    scan_count = -1;
    scan_requested = true;
}

int wifi_service_scan_status(void)
{
    if(scan_count == -1) return -1;   /* still scanning */
    return scan_count;
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

void wifi_service_loop(void)
{
    /* ---- Deferred auto-connect (waits for SDIO driver to be ready) ---- */
    if(auto_connect_pending && millis() >= auto_connect_after_ms)
    {
        auto_connect_pending = false;
        devlog_printf("[WIFI] Auto-connecting to %s", req_ssid);
        connect_request = true;
    }

    /* ---- Scan state machine ---- */
    if(scan_requested && !scan_running)
    {
        /* If retry pending, wait until delay elapsed */
        if(scan_retry_ms != 0 && millis() < scan_retry_ms) {
            // not yet time to retry
        } else {
            scan_retry_ms = 0;
            WiFi.scanDelete();
            int rc = WiFi.scanNetworks(true);   // async=true
            if(rc == WIFI_SCAN_RUNNING || rc == -1) {
                scan_running = true;
                scan_requested = false;
                scan_retry_count = 0;
                scan_start_ms = millis();
                devlog_printf("[WIFI] Async scan started");
            } else {
                scan_retry_count++;
                if(scan_retry_count < SCAN_MAX_RETRIES) {
                    scan_retry_ms = millis() + 1000;  // retry in 1 s
                    devlog_printf("[WIFI] scanNetworks returned %d, retry %d/%d in 1s",
                                  rc, scan_retry_count, SCAN_MAX_RETRIES);
                } else {
                    scan_requested = false;
                    scan_retry_count = 0;
                    scan_count = 0;  // report failure
                    devlog_printf("[WIFI] Scan failed after %d retries", SCAN_MAX_RETRIES);
                }
            }
        }
    }

    if(scan_running)
    {
        int n = WiFi.scanComplete();
        if(n >= 0) {
            scan_count = n;
            scan_running = false;
            devlog_printf("[WIFI] Scan done: %d APs", n);
        } else if(n == -2 || (millis() - scan_start_ms > SCAN_TIMEOUT_MS)) {
            scan_count = 0;
            scan_running = false;
            devlog_printf("[WIFI] Scan failed (%d)", n);
        }
        // n == -1 → still running, will check again next loop
    }

    /* ---- Connect state machine ---- */
    if(connect_request)
    {
        connect_request = false;
        devlog_printf("[WIFI] Connecting to %s", req_ssid);
        wifi_debug("Connecting...");

        /* Do NOT call WiFi.scanDelete() or WiFi.disconnect() here.
           Rapid SDIO commands overwhelm the hosted-driver rx queue,
           causing "Dropping packet(s)" → pointer corruption → crash.
           WiFi.begin() handles disconnect internally.               */
        scale_service_suspend();   // reduce SDIO bus load during connect
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
            /* Save credentials for auto-connect on next boot */
            storage_save_wifi_credentials(req_ssid, req_pwd);
            devlog_printf("[WIFI] Credentials saved for %s", req_ssid);
            scale_service_resume();    // reconnect HX711
            set_state(WIFI_CONNECTED);
        }
        else
        {
            /* On SDIO hosted driver, WL_CONNECT_FAILED can be a transient state.
               Only treat timeout as failure — give the C6 coprocessor the full
               60s to establish the connection.                                   */
            if((millis() - connect_start_ms) > CONNECT_TIMEOUT_MS)
            {
                devlog_printf("[WIFI] Connection failed to %s", req_ssid);
                wifi_debug("WiFi Failed");
                scale_service_resume();    // reconnect HX711
                set_state(WIFI_DISCONNECTED);

                /* Auto-connect retry: if we still have retries left, schedule another attempt */
                if(auto_connect_retries < AUTO_CONNECT_MAX_RETRIES && req_ssid[0] != 0)
                {
                    auto_connect_retries++;
                    auto_connect_pending = true;
                    auto_connect_after_ms = millis() + 20000;  // retry in 20s (SDIO needs recovery time)
                    devlog_printf("[WIFI] Auto-connect retry %d/%d in 20s",
                                  auto_connect_retries, AUTO_CONNECT_MAX_RETRIES);
                }
            }
        }
    }
}
bool wifi_service_is_critical(void)
{
    return wifi_critical_section;
}

void wifi_service_disconnect(void)
{
    devlog_printf("[WIFI] Disconnect requested");
    WiFi.disconnect(true);
    set_state(WIFI_DISCONNECTED);
    auto_connect_pending = false;
    connect_request = false;
}

#endif  /* ENABLE_WIFI_SERVICE */
