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

static lv_obj_t *wifi_debug_label = NULL;
static bool wifi_critical_section = false;
static wifi_state_t state = WIFI_DISCONNECTED;
static int scan_count = 0;
static unsigned long connected_since_ms = 0;
static unsigned long last_link_poll_ms = 0;
static uint8_t link_poll_failures = 0;

static bool scan_requested = false;
static int scan_retry_count = 0;
static const int SCAN_MAX_RETRIES = 5;
static unsigned long scan_retry_ms = 0;

static unsigned long connect_start_ms = 0;
static const unsigned long CONNECT_TIMEOUT_MS = 30000;

static void (*state_cbs[6])(wifi_state_t) = {0};
static int state_cb_count = 0;

static bool connect_request = false;
static char req_ssid[33] = {0};
static char req_pwd[65] = {0};
static char connected_ssid[33] = {0};

static bool auto_connect_pending = false;
static unsigned long auto_connect_after_ms = 0;
static int auto_connect_retries = 0;
static const int AUTO_CONNECT_MAX_RETRIES = 10;
static const unsigned long LINK_POLL_MS = 3000;
static const uint8_t LINK_POLL_FAIL_MAX = 2;

void wifi_service_register_state_callback(void (*cb)(wifi_state_t))
{
    if(!cb) return;
    for(int i = 0; i < state_cb_count; i++) {
        if(state_cbs[i] == cb) return;
    }
    if(state_cb_count < (int)(sizeof(state_cbs) / sizeof(state_cbs[0]))) {
        state_cbs[state_cb_count++] = cb;
    }
}

static void set_state(wifi_state_t s)
{
    if(state == s) return;
    state = s;

    if(lvgl_port_lock(100)) {
        for(int i = 0; i < state_cb_count; i++) {
            if(state_cbs[i]) state_cbs[i](s);
        }
        lvgl_port_unlock();
    }
}

void wifi_service_set_debug_label(lv_obj_t *label)
{
    wifi_debug_label = label;
}

static void wifi_debug(const char *msg)
{
    if(!wifi_debug_label) return;

    if(lvgl_port_lock(100)) {
        if(lv_obj_is_valid(wifi_debug_label)) {
            lv_label_set_text(wifi_debug_label, msg);
        }
        lvgl_port_unlock();
    }
}

void wifi_service_init(void)
{
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);  // service manages reconnection; library auto-reconnect bypasses state machine
    WiFi.setSleep(false);
    delay(3000);
    state = WIFI_DISCONNECTED;
    connected_since_ms = 0;
    last_link_poll_ms = 0;
    link_poll_failures = 0;
    devlog_printf("[WIFI] STA mode initialized");

    char saved_ssid[33] = {0};
    char saved_pwd[65] = {0};
    if(storage_load_wifi_credentials(saved_ssid, sizeof(saved_ssid), saved_pwd, sizeof(saved_pwd))) {
        devlog_printf("[WIFI] Saved network found: %s (will auto-connect in 20s)", saved_ssid);
        strncpy(req_ssid, saved_ssid, sizeof(req_ssid) - 1);
        req_ssid[sizeof(req_ssid) - 1] = 0;
        strncpy(req_pwd, saved_pwd, sizeof(req_pwd) - 1);
        req_pwd[sizeof(req_pwd) - 1] = 0;
        auto_connect_pending = true;
        auto_connect_retries = 0;
        auto_connect_after_ms = millis() + 20000;
    }
}

void wifi_service_start_scan(void)
{
    if(sync_service_is_busy()) {
        wifi_debug("Sync busy");
        return;
    }

    auto_connect_pending = false;
    connect_request = false;

    if(state == WIFI_CONNECTING) {
        WiFi.disconnect(false, false);
        delay(300);
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

void wifi_service_cancel_scan(void)
{
    if(scan_count == -1 || scan_requested) {
        devlog_printf("[WIFI] Scan cancelled");
    }
    scan_requested = false;
    scan_retry_count = 0;
    scan_retry_ms = 0;
    if(scan_count == -1) {
        scan_count = 0;
    }
    wifi_debug("");
}

int wifi_service_scan_status(void)
{
    if(scan_count == -1) return -1;
    return scan_count;
}

void wifi_service_connect(const char *ssid, const char *password)
{
    if(!ssid || !ssid[0]) return;

    scan_requested = false;
    scan_retry_count = 0;
    scan_retry_ms = 0;
    if(scan_count == -1) {
        scan_count = 0;
    }
    WiFi.scanDelete();

    strncpy(req_ssid, ssid, sizeof(req_ssid) - 1);
    req_ssid[sizeof(req_ssid) - 1] = 0;
    strncpy(req_pwd, password ? password : "", sizeof(req_pwd) - 1);
    req_pwd[sizeof(req_pwd) - 1] = 0;

    connect_request = true;
    connect_start_ms = millis();
    wifi_debug("Connect requested...");
}

wifi_state_t wifi_service_state(void)
{
    return state;
}

unsigned long wifi_service_connected_since_ms(void)
{
    return connected_since_ms;
}

uint8_t wifi_service_get_ap_count(void)
{
    return scan_count > 0 ? (uint8_t)scan_count : 0;
}

String wifi_service_get_ssid(uint8_t index)
{
    if(index >= scan_count) return "";
    return WiFi.SSID(index);
}

void wifi_service_loop(void)
{
    if(auto_connect_pending && millis() >= auto_connect_after_ms) {
        /* Don't interrupt an ongoing connection attempt — the current
           connect already has the right credentials; let it finish.  */
        if(state == WIFI_CONNECTING) {
            /* Reschedule 5s later so we retry if this attempt also fails */
            auto_connect_after_ms = millis() + 5000;
        } else {
            auto_connect_pending = false;
            devlog_printf("[WIFI] Auto-connecting to %s", req_ssid);
            connect_request = true;
        }
    }

    if(scan_requested) {
        if(scan_retry_ms != 0 && millis() < scan_retry_ms) {
            return;
        }

        scan_retry_ms = 0;
        WiFi.disconnect(false, false);
        delay(300);
        WiFi.scanDelete();
        delay(200);

        devlog_printf("[WIFI] Starting synchronous scan...");
        int n = WiFi.scanNetworks(false, false, false, 5000);

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

    if(connect_request) {
        connect_request = false;
        devlog_printf("[WIFI] Connecting to %s", req_ssid);
        wifi_debug("Connecting...");

        wifi_critical_section = true;
        connected_since_ms = 0;
        link_poll_failures = 0;
        WiFi.disconnect(false, false);
        delay(300);

        scale_service_suspend();
        WiFi.begin(req_ssid, req_pwd);

        set_state(WIFI_CONNECTING);
        connect_start_ms = millis();
    }

    if(state == WIFI_CONNECTING) {
        wl_status_t s = WiFi.status();

        if(s == WL_CONNECTED) {
            devlog_printf("[WIFI] Connected to %s", req_ssid);
            wifi_debug("WiFi Connected");
            storage_save_wifi_credentials(req_ssid, req_pwd);
            devlog_printf("[WIFI] Credentials saved for %s", req_ssid);
            strncpy(connected_ssid, req_ssid, sizeof(connected_ssid) - 1);
            connected_ssid[sizeof(connected_ssid) - 1] = 0;
            connected_since_ms = millis();
            last_link_poll_ms = connected_since_ms;
            link_poll_failures = 0;
            auto_connect_retries = 0;
            scale_service_resume();
            set_state(WIFI_CONNECTED);
            delay(50);
            wifi_critical_section = false;
        } else if((millis() - connect_start_ms) > CONNECT_TIMEOUT_MS) {
            devlog_printf("[WIFI] Connection failed to %s", req_ssid);
            wifi_debug("WiFi Failed");
            WiFi.disconnect(false, false);
            delay(300);
            connected_since_ms = 0;
            link_poll_failures = 0;
            scale_service_resume();
            set_state(WIFI_DISCONNECTED);
            wifi_critical_section = false;

            if(auto_connect_retries < AUTO_CONNECT_MAX_RETRIES && req_ssid[0] != 0) {
                auto_connect_retries++;
                auto_connect_pending = true;
                auto_connect_after_ms = millis() + 20000;
                devlog_printf("[WIFI] Auto-connect retry %d/%d in 20s",
                              auto_connect_retries, AUTO_CONNECT_MAX_RETRIES);
            }
        }
    }

    if(state == WIFI_CONNECTED) {
        unsigned long now = millis();
        if((now - last_link_poll_ms) >= LINK_POLL_MS) {
            last_link_poll_ms = now;

            wl_status_t s = WiFi.status();
            if(s == WL_CONNECTED) {
                link_poll_failures = 0;
            } else {
                link_poll_failures++;
                devlog_printf("[WIFI] Link check failed (%u/%u), status=%d",
                              (unsigned)link_poll_failures,
                              (unsigned)LINK_POLL_FAIL_MAX,
                              (int)s);
                if(link_poll_failures >= LINK_POLL_FAIL_MAX) {
                    devlog_printf("[WIFI] Link lost");
                    wifi_debug("WiFi Lost");
                    connected_since_ms = 0;
                    connected_ssid[0] = 0;
                    wifi_critical_section = false;
                    set_state(WIFI_DISCONNECTED);

                    if(req_ssid[0] != 0 && auto_connect_retries < AUTO_CONNECT_MAX_RETRIES) {
                        auto_connect_retries++;
                        auto_connect_pending = true;
                        auto_connect_after_ms = millis() + 10000;
                        devlog_printf("[WIFI] Auto-connect retry %d/%d in 10s",
                                      auto_connect_retries, AUTO_CONNECT_MAX_RETRIES);
                    }
                }
            }
        }
    }
}

bool wifi_service_is_critical(void)
{
    return wifi_critical_section;
}

String wifi_service_get_connected_ssid(void)
{
    return String(connected_ssid);
}

void wifi_service_disconnect(void)
{
    devlog_printf("[WIFI] Disconnect requested");
    WiFi.disconnect(false, false);
    connected_since_ms = 0;
    link_poll_failures = 0;
    set_state(WIFI_DISCONNECTED);
    auto_connect_pending = false;
    connect_request = false;
    scan_requested = false;
}

#endif
