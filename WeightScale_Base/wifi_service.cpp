#include "app_config.h"  /* Must be first for feature flags */

#if ENABLE_WIFI_SERVICE

#include <lvgl.h>
#include "lvgl_v8_port.h"
#include "wifi_service.h"
#include <WiFi.h>
#include "devlog.h"
#include "storage_service.h"
#include "scale_service_v2.h"
#include "sync_service.h"

static lv_obj_t *wifi_debug_label = NULL;
static bool wifi_critical_section = false;
static wifi_state_t state = WIFI_DISCONNECTED;
static int scan_count = 0;

/* Scan state */
static bool scan_requested = false;
static int scan_retry_count = 0;
static const int SCAN_MAX_RETRIES = 3;
static unsigned long scan_retry_ms = 0;

/* Connection state */
static unsigned long connect_start_ms = 0;
static const unsigned long CONNECT_TIMEOUT_MS = 30000;

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
static const int AUTO_CONNECT_MAX_RETRIES = 2;

void wifi_service_register_state_callback(void (*cb)(wifi_state_t))
{
    if(!cb) return;
    for(int i = 0; i < state_cb_count; i++) if(state_cbs[i] == cb) return;
    if(state_cb_count < (int)(sizeof(state_cbs)/sizeof(state_cbs[0])))
        state_cbs[state_cb_count++] = cb;
}

static void set_state(wifi_state_t s)
{
    if(state == s) return;
    state = s;
    if(lvgl_port_lock(100)) {
        for(int i = 0; i < state_cb_count; i++)
            if(state_cbs[i]) state_cbs[i](s);
        lvgl_port_unlock();
    }
}

/* ===== DEBUG HELPERS ===== */

void wifi_service_set_debug_label(lv_obj_t *label)
{
    wifi_debug_label = label;
}

static void wifi_debug(const char *msg)
{
    if(!wifi_debug_label) return;
    if(lvgl_port_lock(100)) {
        if(wifi_debug_label && lv_obj_is_valid(wifi_debug_label))
            lv_label_set_text(wifi_debug_label, msg);
        lvgl_port_unlock();
    }
}

/* ===== INIT ===== */

void wifi_service_init(void)
{
    /*
     * CRITICAL ESP32-P4 + C6 SDIO NOTES:
     * - WiFi.disconnect(true) corrupts the SDIO rx queue — NEVER call it
     * - WiFi.mode(WIFI_OFF) kills the SDIO tunnel permanently — NEVER call it
     * - Just set WIFI_STA once and use WiFi.begin()/WiFi.scanNetworks() directly
     * - WiFi.setSleep(false) prevents SDIO power management issues
     */
    WiFi.mode(WIFI_STA);
    delay(3000);                     // C6 SDIO bridge needs generous init time
    WiFi.setSleep(false);            // prevent SDIO sleep/wake corruption
    delay(500);

    state = WIFI_DISCONNECTED;
    devlog_printf("[WIFI] STA mode initialized (sleep disabled)");
    devlog_printf("[WIFI] Mode=%d Status=%d", WiFi.getMode(), WiFi.status());

    /* Schedule auto-connect */
    char saved_ssid[33] = {0};
    char saved_pwd[65] = {0};
    if(storage_load_wifi_credentials(saved_ssid, sizeof(saved_ssid), saved_pwd, sizeof(saved_pwd)))
    {
        devlog_printf("[WIFI] Saved network: %s (auto-connect in 20s)", saved_ssid);
        strncpy(req_ssid, saved_ssid, sizeof(req_ssid));
        req_ssid[sizeof(req_ssid)-1] = 0;
        strncpy(req_pwd, saved_pwd, sizeof(req_pwd));
        req_pwd[sizeof(req_pwd)-1] = 0;
        auto_connect_pending = true;
        auto_connect_retries = 0;
        auto_connect_after_ms = millis() + 20000;
    }
}

/* ===== SCAN ===== */

void wifi_service_start_scan(void)
{
    /* Cancel any pending auto-connect */
    auto_connect_pending = false;
    connect_request = false;

    if(state == WIFI_CONNECTING) {
        scale_service_resume();
        set_state(WIFI_DISCONNECTED);
    }

    devlog_printf("[WIFI] Scan requested");
    wifi_debug("Scanning...");
    scan_count = -1;
    scan_requested = true;
    scan_retry_count = 0;
    scan_retry_ms = 0;
}

int wifi_service_scan_status(void)
{
    if(scan_count == -1) return -1;
    return scan_count;
}

/* ===== CONNECT ===== */

void wifi_service_connect(const char *ssid, const char *password)
{
    if(!ssid || !ssid[0]) return;

    strncpy(req_ssid, ssid, sizeof(req_ssid));
    req_ssid[sizeof(req_ssid)-1] = 0;
    strncpy(req_pwd, password ? password : "", sizeof(req_pwd));
    req_pwd[sizeof(req_pwd)-1] = 0;

    connect_request = true;
    connect_start_ms = millis();
    wifi_debug("Connect requested...");
}

/* ===== AP LIST ===== */

uint8_t wifi_service_get_ap_count(void)
{
    return scan_count > 0 ? (uint8_t)scan_count : 0;
}

String wifi_service_get_ssid(uint8_t index)
{
    if(index >= scan_count) return "";
    return WiFi.SSID(index);
}

/* ===== STATE ===== */

wifi_state_t wifi_service_state(void)
{
    return state;
}

/* ===== MAIN LOOP ===== */

void wifi_service_loop(void)
{
    /* ---- Deferred auto-connect ---- */
    if(auto_connect_pending && millis() >= auto_connect_after_ms)
    {
        auto_connect_pending = false;
        devlog_printf("[WIFI] Auto-connecting to %s", req_ssid);
        connect_request = true;
    }

    /* ---- Scan ---- */
    if(scan_requested)
    {
        if(scan_retry_ms != 0 && millis() < scan_retry_ms)
            goto skip_scan;

        /* Wait if sync is doing HTTPS — heavy SDIO traffic corrupts scan */
        if(sync_service_is_busy()) {
            devlog_printf("[WIFI] Scan deferred - sync HTTP in progress");
            goto skip_scan;
        }

        /* SDIO cooldown: wait 2s after sync's HTTPS before scanning */
        unsigned long last_http = sync_service_last_http_ms();
        if(last_http != 0 && (millis() - last_http) < 2000) {
            devlog_printf("[WIFI] Scan deferred - SDIO cooldown (%lums since HTTP)",
                          millis() - last_http);
            goto skip_scan;
        }

        scan_retry_ms = 0;

        /*  ESP32-P4 SDIO FIX:
         *  Do NOT call WiFi.disconnect() before scan — it corrupts SDIO rx queue.
         *  Just delete old results and scan directly.
         */
        wifi_critical_section = true;   // block sync HTTP during scan
        WiFi.scanDelete();
        delay(100);

        devlog_printf("[WIFI] Starting scan (mode=%d status=%d)...",
                      WiFi.getMode(), WiFi.status());

        int n = WiFi.scanNetworks(false, false, false, 5000);

        wifi_critical_section = false;

        if(n >= 0) {
            scan_count = n;
            scan_requested = false;
            scan_retry_count = 0;
            devlog_printf("[WIFI] Scan done: %d APs", n);
        } else {
            scan_retry_count++;
            devlog_printf("[WIFI] Scan returned %d, retry %d/%d",
                          n, scan_retry_count, SCAN_MAX_RETRIES);
            if(scan_retry_count < SCAN_MAX_RETRIES) {
                scan_retry_ms = millis() + 3000;
            } else {
                scan_requested = false;
                scan_retry_count = 0;
                scan_count = 0;
                devlog_printf("[WIFI] Scan failed after %d retries", SCAN_MAX_RETRIES);
            }
        }
    }
    skip_scan:

    /* ---- Connect ---- */
    if(connect_request)
    {
        connect_request = false;
        devlog_printf("[WIFI] Connecting to %s", req_ssid);
        wifi_debug("Connecting...");

        /*  ESP32-P4 SDIO FIX:
         *  Do NOT call WiFi.disconnect(true) before begin() — it corrupts SDIO.
         *  Just call WiFi.begin() directly — driver handles reconnection. */
        wifi_critical_section = true;   // block sync HTTP during connect
        scale_service_suspend();
        WiFi.begin(req_ssid, req_pwd);
        wifi_critical_section = false;

        set_state(WIFI_CONNECTING);
        connect_start_ms = millis();
    }

    if(state == WIFI_CONNECTING)
    {
        wl_status_t s = WiFi.status();

        if(s == WL_CONNECTED)
        {
            devlog_printf("[WIFI] Connected to %s (IP: %s)",
                          WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
            wifi_debug("WiFi Connected");
            storage_save_wifi_credentials(req_ssid, req_pwd);
            devlog_printf("[WIFI] Credentials saved for %s", req_ssid);
            scale_service_resume();
            set_state(WIFI_CONNECTED);
        }
        else if((millis() - connect_start_ms) > CONNECT_TIMEOUT_MS)
        {
            devlog_printf("[WIFI] Connection failed to %s (status=%d)", req_ssid, s);
            wifi_debug("WiFi Failed");

            /* Minimal cleanup — disconnect(false) to not corrupt SDIO */
            WiFi.disconnect(false);
            delay(500);

            scale_service_resume();
            set_state(WIFI_DISCONNECTED);

            if(auto_connect_retries < AUTO_CONNECT_MAX_RETRIES && req_ssid[0] != 0)
            {
                auto_connect_retries++;
                auto_connect_pending = true;
                auto_connect_after_ms = millis() + 30000;
                devlog_printf("[WIFI] Auto-connect retry %d/%d in 30s",
                              auto_connect_retries, AUTO_CONNECT_MAX_RETRIES);
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
    WiFi.disconnect(false);
    set_state(WIFI_DISCONNECTED);
    auto_connect_pending = false;
    connect_request = false;
}

#endif  /* ENABLE_WIFI_SERVICE */
