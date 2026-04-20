#include "config/app_config.h"

#if ENABLE_WIFI_SERVICE

#include "wifi_service.h"
#include <WiFi.h>
#include "devlog.h"
#include "storage_service.h"
#include "sync_service.h"

/* =========================================================
   STATE
========================================================= */
static void *wifi_debug_label = NULL;  /* opaque — no LVGL on Core 1 */
static bool wifi_critical_section = false;
static wifi_state_t state = WIFI_DISCONNECTED;
static int scan_count = 0;
static unsigned long connected_since_ms = 0;
static unsigned long last_link_poll_ms = 0;
static uint8_t link_poll_failures = 0;

/* SDIO bus recovery tracking */
static uint8_t consecutive_connect_failures = 0;
static const uint8_t SDIO_RESET_THRESHOLD = 3;  /* reset WiFi HW after 3 consecutive fails */

/* SDIO bus stuck detection — after repeated scan+connect failures,
   back off for a long time instead of hammering the broken bus */
static bool sdio_bus_stuck = false;
static unsigned long sdio_stuck_until_ms = 0;
static uint8_t sdio_stuck_count = 0;  /* how many times we've detected stuck bus */

/* Scan state */
static bool scan_requested = false;
static bool scan_in_progress = false;  /* ⭐ NEW: Track async scan status */
static int scan_retry_count = 0;
static const int SCAN_MAX_RETRIES = 8;  /* increased: retry 3 triggers WiFi reinit */
static unsigned long scan_retry_ms = 0;
static unsigned long scan_start_ms = 0;   /* ⭐ NEW: Track scan timeout */
static const unsigned long SCAN_TIMEOUT_MS = 8000;  /* ⭐ NEW: 8s max for async scan */

/* Connection state */
static unsigned long connect_start_ms = 0;
static const unsigned long CONNECT_TIMEOUT_MS = 30000;  /* 30s — mid-retries handle stuck states */
static unsigned long last_begin_ms = 0;        /* when WiFi.begin() was last called */
static int connect_mid_retries = 0;            /* retries within a single connect attempt */
static const int CONNECT_MID_RETRY_MAX = 3;    /* max mid-connect retries */
static volatile bool connect_request = false;   /* volatile: set from Core 0, read from Core 1 */
static char req_ssid[33] = {0};
static char req_pwd[65] = {0};
static int32_t req_channel = 0;          /* 0 = let chip scan all channels */
static uint8_t req_bssid[6] = {0};       /* zeroed = no BSSID hint */
static bool    req_has_bssid = false;
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

/* =========================================================
   CALLBACKS — NO LVGL here! wifi_service runs on Core 1.
   Callbacks must only set flags / call devlog.
========================================================= */
static void (*state_cbs[6])(wifi_state_t) = {0};
static int state_cb_count = 0;

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

    /* Call registered callbacks directly — NO lvgl_port_lock!
       These run on Core 1. Callbacks must not call LVGL. */
    for(int i = 0; i < state_cb_count; i++) {
        if(state_cbs[i]) state_cbs[i](s);
    }
}

/* =========================================================
   DEBUG LABEL — kept for wifi_password_popup but NO direct
   LVGL writes from Core 1. Only store the pointer.
========================================================= */
void wifi_service_set_debug_label(void *label)
{
    wifi_debug_label = label;
}

/* =========================================================
   SDIO BUS RECOVERY — full hardware reset after repeated
   connection failures to clear stuck SDIO state.
========================================================= */
static void wifi_sdio_recovery(void)
{
    devlog_printf("[WIFI] SDIO recovery: soft-resetting WiFi stack");
    /* NEVER use mode(NULL)/mode(STA) cycling on ESP-Hosted — it
       tears down the hosted SDIO interface and permanently corrupts
       the bus after a failed connect. Instead, mirror the proven
       init sequence: disconnect + scanDelete + generous delay. */
    WiFi.disconnect(false, false);   /* soft disconnect — keep SDIO link alive! */
    WiFi.scanDelete();
    vTaskDelay(pdMS_TO_TICKS(3000));  /* match init settle time */
    /* Double-tap: clear any residual C6 state */
    WiFi.disconnect(false, false);
    WiFi.scanDelete();
    vTaskDelay(pdMS_TO_TICKS(2000));
    consecutive_connect_failures = 0;
    scan_count = 0;
    scan_in_progress = false;
    scan_requested = false;
    devlog_printf("[WIFI] SDIO recovery complete");
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

    /* Clear stale state on C6 — CRITICAL when P4 resets but C6 doesn't.
       The C6 (separate power rail) may still be processing a previous
       connection or scan from the prior boot. Without this, the C6's
       state machine is stuck and all scans return -2. */
    WiFi.disconnect(false, false);
    WiFi.scanDelete();

    /* CRITICAL: ESP32-P4 uses an external ESP32-C6 for WiFi via SDIO
       (ESP-Hosted). The SDIO bus negotiation often drops packets during
       startup. Without this delay, scan/connect operations fail with
       WIFI_SCAN_FAILED (-2) because the hosted link isn't stable yet.
       5s settle gives the C6 enough time to stabilize after power-on. */
    vTaskDelay(pdMS_TO_TICKS(5000));
    devlog_printf("[WIFI] STA mode initialized (SDIO settle done)");

    state = WIFI_DISCONNECTED;
    connected_since_ms = 0;
    last_link_poll_ms = 0;
    link_poll_failures = 0;
    consecutive_connect_failures = 0;
    scan_in_progress = false;
    sdio_bus_stuck = false;
    sdio_stuck_count = 0;
    sdio_stuck_until_ms = 0;

    /* Schedule auto-connect if any saved networks exist */
    uint8_t cnt = storage_get_wifi_count();
    if(cnt > 0) {
        /* Start a background scan after SDIO bus stabilizes.
           Delay 5s so the C6 is fully ready before first SDIO scan traffic. */
        scan_requested = true;
        scan_in_progress = false;
        scan_count = -1;
        scan_retry_count = 0;
        scan_retry_ms = millis() + 5000;  /* Delay first scan by 5s */
        scan_start_ms = 0;

        auto_connect_pending = true;
        auto_connect_net_index = 0;
        auto_connect_cycle_retries = 0;
        auto_connect_retries = 0;
        auto_connect_after_ms = millis() + 18000;  /* 18s — scan will complete well before this */
        devlog_printf("[WIFI] %u saved networks. Scanning + auto-connect in 18s", cnt);
    }
}

/* =========================================================
   SCAN
========================================================= */
void wifi_service_start_scan(void)
{
    if(sync_service_is_busy()) {
        devlog_printf("[WIFI] Scan blocked — sync busy");
        return;
    }

    /* Don't cancel auto_connect_pending here — a scan from the WiFi screen
       shouldn't kill background auto-connect.  Only explicit user connect
       (wifi_service_connect) or exhausted retries cancel auto-connect. */
    connect_request = false;

    if(state == WIFI_CONNECTING) {
        /* Soft disconnect + scanDelete to reset C6 state.
           NEVER mode-cycle on ESP-Hosted — it kills the SDIO bus. */
        WiFi.disconnect(false, false);
        WiFi.scanDelete();
        vTaskDelay(pdMS_TO_TICKS(1000));
        wifi_critical_section = false;
        set_state(WIFI_DISCONNECTED);
    }

    devlog_printf("[WIFI] Scan requested");

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
    /* NOTE: Do NOT call WiFi.scanDelete() here — SDIO chatter before
       WiFi.begin() can leave the WiFi chip in a bad state on ESP32-P4 */

    strncpy(req_ssid, ssid, sizeof(req_ssid) - 1);
    req_ssid[sizeof(req_ssid) - 1] = 0;
    strncpy(req_pwd, password ? password : "", sizeof(req_pwd) - 1);
    req_pwd[sizeof(req_pwd) - 1] = 0;

    /* Look up channel & BSSID from recent scan results so WiFi.begin()
       can skip the internal re-scan (critical for SDIO WiFi reliability).
       Pick the strongest AP when multiple routers share the same SSID. */
    req_channel = 0;
    req_has_bssid = false;
    memset(req_bssid, 0, sizeof(req_bssid));
    int n = WiFi.scanComplete();
    int8_t best_manual_rssi = -128;
    for(int i = 0; i < n; i++) {
        if(strcmp(ssid, WiFi.SSID(i).c_str()) == 0) {
            int8_t rssi = (int8_t)WiFi.RSSI(i);
            if(rssi > best_manual_rssi) {
                best_manual_rssi = rssi;
                req_channel = WiFi.channel(i);
                memcpy(req_bssid, WiFi.BSSID(i), 6);
                req_has_bssid = true;
            }
        }
    }
    if(req_has_bssid) {
        devlog_printf("[WIFI] AP hint: ch=%d RSSI=%d BSSID=%02X:%02X:%02X:%02X:%02X:%02X",
                      req_channel, best_manual_rssi,
                      req_bssid[0], req_bssid[1], req_bssid[2],
                      req_bssid[3], req_bssid[4], req_bssid[5]);
    }

    /* Cancel any pending auto-connect — user explicitly chose a network */
    auto_connect_pending = false;
    auto_connect_cycle_retries = 0;  /* reset so retry logic works after manual fail */
    scan_in_progress = false;  /* ⭐ Cancel any ongoing scan */

    connect_request = true;
    connect_start_ms = millis();
    devlog_printf("[WIFI] Connect requested for '%s'", ssid);
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

int8_t wifi_service_get_rssi(uint8_t index)
{
    if(index >= scan_count) return -127;
    return (int8_t)WiFi.RSSI(index);
}

/* =========================================================
   AUTO-CONNECT: pick saved network with strongest signal
   If scan results are available, match saved SSIDs against
   scanned APs and pick the one with best RSSI. Falls back
   to sequential cycling if no scan data exists.
========================================================= */
static bool auto_connect_load_next(void)
{
    uint8_t cnt = storage_get_wifi_count();
    if(cnt == 0) return false;

    /* Try signal-strength ordering if scan results are available */
    if(scan_count > 0) {
        int8_t  best_rssi = -128;
        uint8_t best_saved_idx = 0xFF;

        for(uint8_t si = 0; si < cnt; si++) {
            char ssid[33] = {0}, pwd[65] = {0};
            if(!storage_get_wifi_at(si, ssid, sizeof(ssid), pwd, sizeof(pwd)) || !ssid[0])
                continue;

            /* Find this SSID in scan results — check ALL APs with same name
               (enterprise/office WiFi may have multiple routers sharing one SSID)
               and pick the one with the strongest signal */
            for(int ai = 0; ai < scan_count; ai++) {
                String scanned = WiFi.SSID(ai);
                if(scanned.length() == 0) continue;
                if(strcmp(ssid, scanned.c_str()) == 0) {
                    int8_t rssi = (int8_t)WiFi.RSSI(ai);
                    if(rssi > best_rssi) {
                        best_rssi = rssi;
                        best_saved_idx = si;
                    }
                    /* don't break — keep scanning for a stronger AP with same SSID */
                }
            }
        }

        if(best_saved_idx != 0xFF) {
            char ssid[33] = {0}, pwd[65] = {0};
            storage_get_wifi_at(best_saved_idx, ssid, sizeof(ssid), pwd, sizeof(pwd));
            strncpy(req_ssid, ssid, sizeof(req_ssid) - 1);
            req_ssid[sizeof(req_ssid) - 1] = 0;
            strncpy(req_pwd, pwd, sizeof(req_pwd) - 1);
            req_pwd[sizeof(req_pwd) - 1] = 0;
            /* Grab channel/BSSID for the strongest AP with this SSID */
            req_channel = 0;
            req_has_bssid = false;
            memset(req_bssid, 0, sizeof(req_bssid));
            int8_t best_ap_rssi = -128;
            for(int ai = 0; ai < scan_count; ai++) {
                if(strcmp(ssid, WiFi.SSID(ai).c_str()) == 0) {
                    int8_t rssi = (int8_t)WiFi.RSSI(ai);
                    if(rssi > best_ap_rssi) {
                        best_ap_rssi = rssi;
                        req_channel = WiFi.channel(ai);
                        memcpy(req_bssid, WiFi.BSSID(ai), 6);
                        req_has_bssid = true;
                    }
                }
            }
            auto_connect_net_index = best_saved_idx;
            if(req_has_bssid) {
                devlog_printf("[WIFI] Auto-connect: strongest saved='%s' RSSI=%d ch=%d BSSID=%02X:%02X:%02X:%02X:%02X:%02X",
                              ssid, best_rssi, req_channel,
                              req_bssid[0], req_bssid[1], req_bssid[2],
                              req_bssid[3], req_bssid[4], req_bssid[5]);
            } else {
                devlog_printf("[WIFI] Auto-connect: strongest saved='%s' RSSI=%d", ssid, best_rssi);
            }
            return true;
        }
        /* No saved network visible in scan — don't try blind */
        devlog_printf("[WIFI] Auto-connect: no saved network found in %d scanned APs", scan_count);
    }

    /* No scan results or no match — refuse to connect blind */
    return false;
}

/* =========================================================
   MAIN LOOP
========================================================= */
void wifi_service_loop(void)
{
    /* --- SDIO bus stuck guard --- */
    if(sdio_bus_stuck) {
        if(millis() < sdio_stuck_until_ms) {
            /* Bus is stuck — don't attempt any scan or connect */
            return;
        }
        /* Cooldown elapsed — try ONE recovery + scan */
        devlog_printf("[WIFI] SDIO stuck cooldown elapsed, attempting recovery");
        wifi_sdio_recovery();
        sdio_bus_stuck = false;
        /* Trigger a scan to probe if bus is back */
        scan_requested = true;
        scan_in_progress = false;
        scan_count = -1;
        scan_retry_count = 0;
        scan_retry_ms = 0;
        scan_start_ms = 0;
    }

    /* --- Auto-connect scheduler --- */
    if(auto_connect_pending && millis() >= auto_connect_after_ms) {
        if(state == WIFI_CONNECTING) {
            /* Already connecting — reschedule */
            auto_connect_after_ms = millis() + 5000;
        } else if(scan_requested || scan_in_progress) {
            /* Scan still running — wait for results before trying */
            auto_connect_after_ms = millis() + 5000;
        } else if(scan_count <= 0) {
            /* No scan results — can't pick a network.
               Request a new scan and wait longer. */
            devlog_printf("[WIFI] Auto-connect deferred — no scan results (count=%d)", scan_count);
            scan_requested = true;
            scan_in_progress = false;
            scan_count = -1;
            scan_retry_count = 0;
            scan_retry_ms = 0;
            scan_start_ms = 0;
            auto_connect_after_ms = millis() + 20000;  /* 20s — give scan time */
        } else {
            /* scan_count > 0 — we have results, pick best network */
            if(auto_connect_load_next()) {
                devlog_printf("[WIFI] Auto-connect: trying '%s' (net %u, retry %d)",
                              req_ssid, auto_connect_net_index, auto_connect_cycle_retries);
                auto_connect_pending = false;  /* one-shot: re-enabled by connect failure handler */
                connect_request = true;
            } else {
                auto_connect_pending = false;
                devlog_printf("[WIFI] No saved networks found in scan results");
            }
        }
    }

    /* --- Scan processing (async) --- */
    if(scan_requested && !scan_in_progress && state != WIFI_CONNECTING) {
        /* Start scan if retry timer elapsed (don't block loop with return!) */
        if(scan_retry_ms != 0 && millis() < scan_retry_ms) {
            /* Not yet time to retry — fall through to connect processing */
        } else {
        scan_retry_ms = 0;
        
        /* Only delete old scan results if we actually have some;
           calling scanDelete() with no prior data confuses SDIO WiFi chip */
        if(scan_count > 0) {
            WiFi.scanDelete();
            vTaskDelay(pdMS_TO_TICKS(100));  /* SDIO settle after delete */
        }
        
        devlog_printf("[WIFI] Starting async scan...");
        int result = WiFi.scanNetworks(true);  /* true = async mode */
        if(result == WIFI_SCAN_RUNNING) {
            scan_in_progress = true;
            scan_start_ms = millis();
            devlog_printf("[WIFI] Async scan started");
        } else if(result >= 0) {
            /* Immediate cached result */
            scan_count = result;
            scan_requested = false;
            scan_retry_count = 0;
            sdio_stuck_count = 0;  /* scan works — bus is healthy */
            devlog_printf("[WIFI] Scan immediate result: %d APs", result);
        } else {
            /* Scan failed to start (-2 = WIFI_SCAN_FAILED on SDIO) — retry */
            scan_retry_count++;
            devlog_printf("[WIFI] Scan failed to start (err=%d), retry %d/%d",
                          result, scan_retry_count, SCAN_MAX_RETRIES);
            if(scan_retry_count == 3) {
                /* After 3 failures, disconnect + scanDelete + sync scan probe.
                   NEVER mode-cycle — it kills the SDIO bus permanently
                   on ESP-Hosted after a failed connect. */
                devlog_printf("[WIFI] WiFi reset + sync scan probe");
                WiFi.disconnect(false, false);
                WiFi.scanDelete();
                vTaskDelay(pdMS_TO_TICKS(3000));

                /* Synchronous scan forces complete SDIO round-trip.
                   More reliable than async after a stack reset. */
                devlog_printf("[WIFI] Trying synchronous scan...");
                int sync_result = WiFi.scanNetworks(false);

                if(sync_result > 0) {
                    scan_count = sync_result;
                    scan_requested = false;
                    scan_in_progress = false;
                    scan_retry_count = 0;
                    sdio_stuck_count = 0;  /* bus recovered */
                    devlog_printf("[WIFI] Sync scan OK: %d APs", sync_result);
                    return;
                }
                devlog_printf("[WIFI] Sync scan result: %d", sync_result);
            }
            if(scan_retry_count < SCAN_MAX_RETRIES) {
                scan_retry_ms = millis() + 2000;  /* retry in 2s */
            } else {
                scan_requested = false;
                scan_retry_count = 0;
                scan_count = 0;
                devlog_printf("[WIFI] Scan start failed after %d retries", SCAN_MAX_RETRIES);

                /* SDIO bus is likely stuck — enter backoff to prevent
                   tight failure loops that eventually crash the device */
                sdio_stuck_count++;
                unsigned long backoff_ms;
                if(sdio_stuck_count <= 2)
                    backoff_ms = 60000;        /* 1 min first two times */
                else if(sdio_stuck_count <= 5)
                    backoff_ms = 180000;       /* 3 min */
                else
                    backoff_ms = 300000;       /* 5 min */
                sdio_bus_stuck = true;
                sdio_stuck_until_ms = millis() + backoff_ms;
                auto_connect_pending = false;  /* cancel auto-connect during backoff */
                devlog_printf("[WIFI] SDIO bus stuck (count=%u), backing off %lus",
                              (unsigned)sdio_stuck_count, backoff_ms / 1000);
            }
        }
        } /* end else (scan_ready) */
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
            sdio_stuck_count = 0;  /* scan works — bus is healthy */
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
                devlog_printf("[WIFI] Scan timeout exhausted");
            }
        }
        /* else: still scanning (WIFI_SCAN_RUNNING) — keep polling */
    }

    /* --- Connection request --- */
    if(connect_request) {
        connect_request = false;
        devlog_printf("[WIFI] Connecting to '%s'", req_ssid);

        wifi_critical_section = true;
        connected_since_ms = 0;
        link_poll_failures = 0;

        /* Capture previous state BEFORE transitioning — needed to decide
           whether a disconnect is necessary below */
        wifi_state_t prev_state = state;
        wl_status_t  prev_wl   = WiFi.status();

        /* Set CONNECTING state IMMEDIATELY — before WiFi reinit delays.
           This prevents the UI popup from seeing DISCONNECTED and
           prematurely declaring failure during the 700ms reinit window. */
        set_state(WIFI_CONNECTING);
        connect_start_ms = millis();

        /* SDIO recovery if bus is likely stuck from prior failure */
        if(consecutive_connect_failures >= SDIO_RESET_THRESHOLD) {
            wifi_sdio_recovery();
            /* Recovery already resets WiFi to clean STA mode */
        } else {
            /* ALWAYS soft-disconnect before WiFi.begin() on ESP-Hosted.
               Without this, the C6 may not start a new connection attempt
               and status stays at WL_DISCONNECTED (6) forever. */
            WiFi.disconnect(false, false);
            vTaskDelay(pdMS_TO_TICKS(200));
        }

        /* Clear stale scan results from firmware memory so the C6's
           internal scan for the connect can proceed unimpeded */
        WiFi.scanDelete();
        vTaskDelay(pdMS_TO_TICKS(100));

        connect_mid_retries = 0;
        last_begin_ms = millis();

        /* Pass channel hint from scan results (helps C6 find AP quickly)
           but NOT BSSID — iPhones randomize their MAC address */
        if(req_channel > 0) {
            devlog_printf("[WIFI] WiFi.begin('%s', ch=%d)", req_ssid, req_channel);
            WiFi.begin(req_ssid, req_pwd, req_channel);
        } else {
            devlog_printf("[WIFI] WiFi.begin('%s', no ch hint)", req_ssid);
            WiFi.begin(req_ssid, req_pwd);
        }
    }

    /* --- Connecting: poll for result --- */
    if(state == WIFI_CONNECTING) {
        wl_status_t s = WiFi.status();
        unsigned long elapsed = millis() - connect_start_ms;

        if(s == WL_CONNECTED) {
            devlog_printf("[WIFI] Connected to '%s' IP=%s",
                          req_ssid, WiFi.localIP().toString().c_str());

            /* Save credentials (promotes to index 0) */
            storage_save_wifi_credentials(req_ssid, req_pwd);

            strncpy(connected_ssid, req_ssid, sizeof(connected_ssid) - 1);
            connected_ssid[sizeof(connected_ssid) - 1] = 0;
            connected_since_ms = millis();
            last_link_poll_ms = connected_since_ms;
            link_poll_failures = 0;
            consecutive_connect_failures = 0;  /* reset on success */
            sdio_stuck_count = 0;              /* bus is healthy */
            auto_connect_retries = 0;
            auto_connect_pending = false;
            auto_connect_cycle_retries = 0;

            wifi_critical_section = false;

            set_state(WIFI_CONNECTED);

        } else if((s == WL_NO_SSID_AVAIL || s == WL_DISCONNECTED)
                  && (millis() - last_begin_ms) > 5000
                  && connect_mid_retries < CONNECT_MID_RETRY_MAX) {
            /* ESP-Hosted mid-connect retry:
               status=1 (NO_SSID): C6 did one-shot scan, didn't find AP
               status=6 (DISCONNECTED): C6 never started connecting
               Either way, we must call WiFi.begin() again. */
            connect_mid_retries++;
            devlog_printf("[WIFI] No progress (status=%d), re-trying begin %d/%d",
                          (int)s, connect_mid_retries, CONNECT_MID_RETRY_MAX);
            WiFi.disconnect(false, false);
            vTaskDelay(pdMS_TO_TICKS(500));
            WiFi.scanDelete();
            vTaskDelay(pdMS_TO_TICKS(100));
            last_begin_ms = millis();
            if(req_channel > 0) {
                WiFi.begin(req_ssid, req_pwd, req_channel);
            } else {
                WiFi.begin(req_ssid, req_pwd);
            }

        } else if(s == WL_CONNECT_FAILED && elapsed > 5000) {
            /* Early failure: auth rejected or AP refused connection */
            devlog_printf("[WIFI] Connect rejected by '%s' (status=%d, %lums)",
                          req_ssid, (int)s, elapsed);
            goto connect_failed;

        } else if(elapsed > CONNECT_TIMEOUT_MS) {
            devlog_printf("[WIFI] Connection timed out to '%s' (status=%d)", req_ssid, (int)s);

            connect_failed:
            /* Soft reset: disconnect + scanDelete + settle.
               NEVER mode-cycle (mode(NULL)/mode(STA)) on ESP-Hosted —
               it tears down the SDIO hosted interface and permanently
               kills the bus after a failed connect. This mirrors the
               proven init sequence that works at every boot. */
            WiFi.disconnect(false, false);
            WiFi.scanDelete();
            vTaskDelay(pdMS_TO_TICKS(3000));  /* match init settle time */

            connected_since_ms = 0;
            link_poll_failures = 0;
            consecutive_connect_failures++;  /* track for SDIO recovery */

            wifi_critical_section = false;

            set_state(WIFI_DISCONNECTED);

            /* Schedule retry: scan + try strongest saved network after 15s */
            auto_connect_cycle_retries++;
            uint8_t total_nets = storage_get_wifi_count();
            int max_total = AUTO_CONNECT_MAX_RETRIES * (total_nets > 0 ? total_nets : 1);

            if(auto_connect_cycle_retries < max_total && total_nets > 0) {
                /* Trigger a fresh scan so next auto_connect_load_next picks strongest */
                scan_requested = true;
                scan_in_progress = false;
                scan_count = -1;
                scan_retry_count = 0;
                scan_retry_ms = 0;
                scan_start_ms = 0;

                auto_connect_pending = true;
                auto_connect_after_ms = millis() + 15000;
                devlog_printf("[WIFI] Scanning + retry in 15s (attempt %d/%d)",
                              auto_connect_cycle_retries, max_total);
            } else if(total_nets == 0) {
                auto_connect_pending = false;
                devlog_printf("[WIFI] No saved networks — cannot auto-retry");
            } else {
                auto_connect_pending = false;
                devlog_printf("[WIFI] Auto-connect exhausted all retries (%d networks x %d attempts)",
                              total_nets, AUTO_CONNECT_MAX_RETRIES);
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
                    connected_since_ms = 0;
                    connected_ssid[0] = 0;
                    wifi_critical_section = false;

                    set_state(WIFI_DISCONNECTED);

                    /* Re-connect: scan first, then pick strongest saved network */
                    uint8_t cnt = storage_get_wifi_count();
                    if(cnt > 0) {
                        /* Trigger background scan so auto_connect_load_next picks strongest */
                        scan_requested = true;
                        scan_in_progress = false;
                        scan_count = -1;
                        scan_retry_count = 0;
                        scan_retry_ms = 0;
                        scan_start_ms = 0;

                        auto_connect_cycle_retries = 0;
                        auto_connect_net_index = 0;
                        auto_connect_pending = true;
                        auto_connect_after_ms = millis() + 12000;  /* 12s: enough for scan to finish */
                        devlog_printf("[WIFI] Scanning + reconnect in 12s");
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
    consecutive_connect_failures = 0;
    wifi_critical_section = false;
    sdio_bus_stuck = false;
    sdio_stuck_count = 0;
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
    sdio_bus_stuck = false;       /* user-initiated — allow fresh attempt */
    sdio_stuck_count = 0;
    consecutive_connect_failures = 0;
    set_state(WIFI_DISCONNECTED);
    connect_request = false;
    scan_requested = false;

    uint8_t cnt = storage_get_wifi_count();
    if(cnt > 0) {
        /* Scan first so auto_connect_load_next picks strongest signal */
        scan_requested = true;
        scan_in_progress = false;
        scan_count = -1;
        scan_retry_count = 0;
        scan_retry_ms = 0;
        scan_start_ms = 0;

        auto_connect_net_index = 0;
        auto_connect_cycle_retries = 0;
        auto_connect_pending = true;
        auto_connect_after_ms = millis() + 12000;  /* 12s: enough for scan */
        devlog_printf("[WIFI] Scanning + auto-reconnect in 12s");
    } else {
        auto_connect_pending = false;
        devlog_printf("[WIFI] Reconnect skipped - no saved networks");
    }
}

#endif
