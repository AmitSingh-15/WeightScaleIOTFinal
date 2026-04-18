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

/* =========================================================
   STATE
========================================================= */
static lv_obj_t *wifi_debug_label = NULL;
static bool wifi_critical_section = false;
static wifi_state_t state = WIFI_DISCONNECTED;
static int scan_count = 0;
static unsigned long connected_since_ms = 0;
static unsigned long last_link_poll_ms = 0;
static uint8_t link_poll_failures = 0;

/* Scan state */
static bool scan_requested = false;
static bool scan_in_progress = false;  /* ⭐ NEW: Track async scan status */
static int scan_retry_count = 0;
static const int SCAN_MAX_RETRIES = 5;
static unsigned long scan_retry_ms = 0;
static unsigned long scan_start_ms = 0;   /* ⭐ NEW: Track scan timeout */
static const unsigned long SCAN_TIMEOUT_MS = 8000;  /* ⭐ NEW: 8s max for async scan */

/* Connection state */
static unsigned long connect_start_ms = 0;
static const unsigned long CONNECT_TIMEOUT_MS = 20000;  /* 20s — reduced from 30s */

/* Callbacks */
static void (*state_cbs[6])(wifi_state_t) = {0};
static int state_cb_count = 0;

/* Connection request */
static bool connect_request = false;
static char req_ssid[33] = {0};
static char req_pwd[65] = {0};
static char connected_ssid[33] = {0};

/* Auto-connect state — cycles through saved networks */
static bool auto_connect_pending = false;
static unsigned long auto_connect_after_ms = 0;
static int auto_connect_retries = 0;
static const int AUTO_CONNECT_MAX_RETRIES = 3;  /* per network */
static uint8_t auto_connect_net_index = 0;       /* which saved network to try */
static int auto_connect_cycle_retries = 0;       /* retries for current network */

/* Link monitoring */
static const unsigned long LINK_POLL_MS = 5000;   /* 5s between checks */
static const uint8_t LINK_POLL_FAIL_MAX = 3;      /* 3 failures → link lost */

/* Scale blocking */
static bool scale_was_suspended = false;

/* =========================================================
   CALLBACKS
========================================================= */
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

/* =========================================================
   DEBUG LABEL
========================================================= */
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

/* =========================================================
   SCALE BLOCKING HELPERS
   Suspend scale during WiFi operations to reduce load.
   Resume only if we were the ones who suspended it.
========================================================= */
static void wifi_suspend_scale(void)
{
    if(!scale_was_suspended) {
        scale_service_suspend();
        scale_was_suspended = true;
        devlog_printf("[WIFI] Scale suspended for WiFi operation");
    }
}

static void wifi_resume_scale(void)
{
    if(scale_was_suspended) {
        scale_service_resume();
        scale_was_suspended = false;
        devlog_printf("[WIFI] Scale resumed");
    }
}

/* =========================================================
   INIT
========================================================= */
void wifi_service_init(void)
{
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    WiFi.setSleep(false);
    /* ⭐ REMOVED: delay(2000) — handled async in loop via auto_connect_after_ms */
    state = WIFI_DISCONNECTED;
    connected_since_ms = 0;
    last_link_poll_ms = 0;
    link_poll_failures = 0;
    scale_was_suspended = false;
    scan_in_progress = false;
    devlog_printf("[WIFI] STA mode initialized");

    /* Schedule auto-connect if any saved networks exist */
    uint8_t cnt = storage_get_wifi_count();
    if(cnt > 0) {
        auto_connect_pending = true;
        auto_connect_net_index = 0;
        auto_connect_cycle_retries = 0;
        auto_connect_retries = 0;
        auto_connect_after_ms = millis() + 18000;  /* ⭐ 18s (was 15s) — accounts for removed 2s init delay */
        char ssid[33] = {0};
        char pwd[65] = {0};
        storage_get_wifi_at(0, ssid, sizeof(ssid), pwd, sizeof(pwd));
        devlog_printf("[WIFI] %u saved networks. Will auto-connect to '%s' in 15s", cnt, ssid);
    }
}

/* =========================================================
   SCAN
========================================================= */
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
        /* ⭐ REMOVED: delay(200) — non-blocking disconnect */
        wifi_resume_scale();
        wifi_critical_section = false;
        set_state(WIFI_DISCONNECTED);
    }

    devlog_printf("[WIFI] Scan requested");
    wifi_debug("Scanning...");

    /* Suspend scale during scan to reduce SDIO contention */
    wifi_suspend_scale();

    scan_count = -1;
    scan_requested = true;
    scan_in_progress = false;  /* ⭐ Reset async scan flag */
    scan_retry_count = 0;
    scan_retry_ms = 0;
    scan_start_ms = 0;  /* ⭐ Reset timeout timer */
}

void wifi_service_cancel_scan(void)
{
    if(scan_count == -1 || scan_requested) {
        devlog_printf("[WIFI] Scan cancelled");
    }
    scan_requested = false;
    scan_in_progress = false;  /* ⭐ Cancel async operation */
    scan_retry_count = 0;
    scan_retry_ms = 0;
    if(scan_count == -1) {
        scan_count = 0;
    }
    wifi_resume_scale();
    wifi_debug("");
}

int wifi_service_scan_status(void)
{
    if(scan_count == -1) return -1;
    return scan_count;
}

/* =========================================================
   CONNECT
========================================================= */
void wifi_service_connect(const char *ssid, const char *password)
{
    if(!ssid || !ssid[0]) return;

    scan_requested = false;
    scan_retry_count = 0;
    scan_retry_ms = 0;
    if(scan_count == -1) scan_count = 0;
    WiFi.scanDelete();

    strncpy(req_ssid, ssid, sizeof(req_ssid) - 1);
    req_ssid[sizeof(req_ssid) - 1] = 0;
    strncpy(req_pwd, password ? password : "", sizeof(req_pwd) - 1);
    req_pwd[sizeof(req_pwd) - 1] = 0;

    /* Cancel any pending auto-connect — user explicitly chose a network */
    auto_connect_pending = false;
    scan_in_progress = false;  /* ⭐ Cancel any ongoing scan */

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

/* =========================================================
   AUTO-CONNECT: load next saved network credentials
========================================================= */
static bool auto_connect_load_next(void)
{
    uint8_t cnt = storage_get_wifi_count();
    if(cnt == 0) return false;

    /* Try each saved network, cycling through */
    for(uint8_t attempt = 0; attempt < cnt; attempt++) {
        uint8_t idx = (auto_connect_net_index + attempt) % cnt;
        char ssid[33] = {0}, pwd[65] = {0};
        if(storage_get_wifi_at(idx, ssid, sizeof(ssid), pwd, sizeof(pwd)) && ssid[0]) {
            strncpy(req_ssid, ssid, sizeof(req_ssid) - 1);
            req_ssid[sizeof(req_ssid) - 1] = 0;
            strncpy(req_pwd, pwd, sizeof(req_pwd) - 1);
            req_pwd[sizeof(req_pwd) - 1] = 0;
            auto_connect_net_index = idx;
            return true;
        }
    }
    return false;
}

/* =========================================================
   MAIN LOOP
========================================================= */
void wifi_service_loop(void)
{
    /* --- Auto-connect scheduler --- */
    if(auto_connect_pending && millis() >= auto_connect_after_ms) {
        if(state == WIFI_CONNECTING) {
            /* Already connecting — reschedule */
            auto_connect_after_ms = millis() + 5000;
        } else {
            if(auto_connect_load_next()) {
                devlog_printf("[WIFI] Auto-connect: trying '%s' (net %u, retry %d)",
                              req_ssid, auto_connect_net_index, auto_connect_cycle_retries);
                connect_request = true;
            } else {
                auto_connect_pending = false;
                devlog_printf("[WIFI] No saved networks to auto-connect");
            }
        }
    }

    /* --- Scan processing (async) --- */
    if(scan_requested && !scan_in_progress) {
        /* Start scan if retry timer elapsed */
        if(scan_retry_ms != 0 && millis() < scan_retry_ms) {
            return;  /* Not yet time to retry */
        }
        scan_retry_ms = 0;
        
        /* ⭐ REMOVED: delay() calls — start async scan immediately */
        WiFi.disconnect(false, false);
        WiFi.scanDelete();
        
        devlog_printf("[WIFI] Starting async scan...");
        /* ⭐ NEW: Use async WiFi.scanNetworks(true) — starts and returns immediately */
        int result = WiFi.scanNetworks(true);  /* true = async mode */
        if(result == WIFI_SCAN_RUNNING) {
            scan_in_progress = true;
            scan_start_ms = millis();
            devlog_printf("[WIFI] Async scan started");
        } else {
            /* Immediate result (cached or error) */
            scan_count = (result >= 0) ? result : 0;
            scan_requested = false;
            scan_retry_count = 0;
            wifi_resume_scale();
            devlog_printf("[WIFI] Scan immediate result: %d APs", result);
        }
    }
    
    /* Poll for async scan completion */
    if(scan_in_progress) {
        int result = WiFi.scanComplete();
        
        if(result >= 0) {
            /* Scan completed successfully */
            scan_count = result;
            scan_requested = false;
            scan_in_progress = false;
            scan_retry_count = 0;
            wifi_resume_scale();
            devlog_printf("[WIFI] Async scan done: %d APs", result);
        } else if(result == WIFI_SCAN_FAILED) {
            /* Scan failed */
            scan_in_progress = false;
            scan_retry_count++;
            devlog_printf("[WIFI] Async scan failed, retry %d/%d",
                          scan_retry_count, SCAN_MAX_RETRIES);
            if(scan_retry_count < SCAN_MAX_RETRIES) {
                scan_retry_ms = millis() + 3000;
            } else {
                scan_requested = false;
                scan_retry_count = 0;
                scan_count = 0;
                wifi_resume_scale();
                devlog_printf("[WIFI] Scan failed after %d retries", SCAN_MAX_RETRIES);
            }
        } else if((millis() - scan_start_ms) > SCAN_TIMEOUT_MS) {
            /* Timeout waiting for scan */
            scan_in_progress = false;
            WiFi.scanComplete();  /* Cancel pending scan */
            scan_retry_count++;
            devlog_printf("[WIFI] Async scan timeout, retry %d/%d",
                          scan_retry_count, SCAN_MAX_RETRIES);
            if(scan_retry_count < SCAN_MAX_RETRIES) {
                scan_retry_ms = millis() + 3000;
            } else {
                scan_requested = false;
                scan_retry_count = 0;
                scan_count = 0;
                wifi_resume_scale();
                devlog_printf("[WIFI] Scan timeout exhausted");
            }
        }
        /* else: still scanning (WIFI_SCAN_RUNNING) — keep polling */
    }

    /* --- Connection request --- */
    if(connect_request) {
        connect_request = false;
        devlog_printf("[WIFI] Connecting to '%s'", req_ssid);
        wifi_debug("Connecting...");

        wifi_critical_section = true;
        connected_since_ms = 0;
        link_poll_failures = 0;

        /* Suspend scale during connection to reduce load */
        wifi_suspend_scale();

        WiFi.disconnect(false, false);
        /* ⭐ REMOVED: delay(200) — WiFi.begin handles any needed delay internally */
        WiFi.begin(req_ssid, req_pwd);

        set_state(WIFI_CONNECTING);
        connect_start_ms = millis();
    }

    /* --- Connecting: poll for result --- */
    if(state == WIFI_CONNECTING) {
        wl_status_t s = WiFi.status();

        if(s == WL_CONNECTED) {
            devlog_printf("[WIFI] Connected to '%s' IP=%s",
                          req_ssid, WiFi.localIP().toString().c_str());
            wifi_debug("WiFi Connected");

            /* Save credentials (promotes to index 0) */
            storage_save_wifi_credentials(req_ssid, req_pwd);

            strncpy(connected_ssid, req_ssid, sizeof(connected_ssid) - 1);
            connected_ssid[sizeof(connected_ssid) - 1] = 0;
            connected_since_ms = millis();
            last_link_poll_ms = connected_since_ms;
            link_poll_failures = 0;
            auto_connect_retries = 0;
            auto_connect_pending = false;
            auto_connect_cycle_retries = 0;

            /* Resume scale — connection complete */
            wifi_resume_scale();
            wifi_critical_section = false;

            set_state(WIFI_CONNECTED);

        } else if((millis() - connect_start_ms) > CONNECT_TIMEOUT_MS) {
            devlog_printf("[WIFI] Connection failed to '%s' (status=%d)", req_ssid, (int)s);
            wifi_debug("WiFi Failed");
            WiFi.disconnect(false, false);
            /* ⭐ REMOVED: delay(200) — proceed without blocking */
            connected_since_ms = 0;
            link_poll_failures = 0;

            /* Resume scale — connection attempt done */
            wifi_resume_scale();
            wifi_critical_section = false;

            set_state(WIFI_DISCONNECTED);

            /* Schedule retry: try next saved network after 15s */
            auto_connect_cycle_retries++;
            uint8_t total_nets = storage_get_wifi_count();
            int max_total = AUTO_CONNECT_MAX_RETRIES * (total_nets > 0 ? total_nets : 1);

            if(auto_connect_cycle_retries < max_total && total_nets > 0) {
                /* Move to next saved network */
                auto_connect_net_index = (auto_connect_net_index + 1) % total_nets;
                auto_connect_pending = true;
                auto_connect_after_ms = millis() + 15000;
                devlog_printf("[WIFI] Will try next saved network in 15s (attempt %d/%d)",
                              auto_connect_cycle_retries, max_total);
            } else {
                auto_connect_pending = false;
                devlog_printf("[WIFI] Auto-connect exhausted all retries");
            }
        }
    }

    /* --- Connected: link monitoring --- */
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
                    devlog_printf("[WIFI] Link lost to '%s'", connected_ssid);
                    wifi_debug("WiFi Lost");
                    connected_since_ms = 0;
                    connected_ssid[0] = 0;
                    wifi_critical_section = false;

                    set_state(WIFI_DISCONNECTED);

                    /* Re-connect: start from the same network */
                    uint8_t cnt = storage_get_wifi_count();
                    if(cnt > 0) {
                        auto_connect_cycle_retries = 0;
                        auto_connect_net_index = 0;
                        auto_connect_pending = true;
                        auto_connect_after_ms = millis() + 5000;
                        devlog_printf("[WIFI] Will attempt reconnect in 5s");
                    }
                }
            }
        }
    }
}

/* =========================================================
   UTILITY
========================================================= */
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
    connected_ssid[0] = 0;
    wifi_resume_scale();
    wifi_critical_section = false;
    set_state(WIFI_DISCONNECTED);
    auto_connect_pending = false;
    connect_request = false;
    scan_requested = false;
}

void wifi_service_request_reconnect(void)
{
    devlog_printf("[WIFI] Reconnect requested");
    WiFi.disconnect(false, false);
    connected_since_ms = 0;
    link_poll_failures = 0;
    wifi_critical_section = false;
    connected_ssid[0] = 0;
    set_state(WIFI_DISCONNECTED);
    connect_request = false;
    scan_requested = false;

    uint8_t cnt = storage_get_wifi_count();
    if(cnt > 0) {
        auto_connect_net_index = 0;
        auto_connect_cycle_retries = 0;
        auto_connect_pending = true;
        auto_connect_after_ms = millis() + 2000;
        devlog_printf("[WIFI] Auto-reconnect scheduled in 2s");
    } else {
        auto_connect_pending = false;
        devlog_printf("[WIFI] Reconnect skipped - no saved networks");
    }
}

#endif
