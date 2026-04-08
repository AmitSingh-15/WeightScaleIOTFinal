#include "app_config.h"  /* Must be first for feature flags */

#if ENABLE_CLOUD_SYNC

#include "sync_service.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "storage_service.h"
#include "wifi_service.h"
#include "ota_service.h"
#include "devlog.h"

// Forward for deinit
static HTTPClient *g_active_http = nullptr;

/* =========================================================
   CONFIG
========================================================= */

static const char *HEALTH_URL =
"https://dev.etranscargo.in/weightscale/health";

static const char *BULK_URL =
"https://dev.etranscargo.in/weightscale/api/WeightIngestion/bulk";

static const unsigned long SYNC_INTERVAL_MS = 20000; // 20 seconds
static const unsigned long HTTPS_STARTUP_COOLDOWN_MS = 10000;

static unsigned long last_sync_attempt = 0;
static bool sync_busy = false;
static bool sync_paused = false;             // paused during OTA
static unsigned long last_http_done_ms = 0;  // track when HTTPS finished for SDIO cooldown

static bool sync_deinited = false;

/* ========================================================= */

bool sync_service_is_busy(void)
{
    return sync_busy;
}

unsigned long sync_service_last_http_ms(void)
{
    return last_http_done_ms;
}

void sync_service_init(void)
{
    last_sync_attempt = 0;
    sync_busy = false;
    sync_paused = false;
    sync_deinited = false;
    last_http_done_ms = 0;
}

/* =========================================================
   JSON ESCAPE
========================================================= */

static String escape_json(const String &input)
{
    String out;
    for (size_t i = 0; i < input.length(); i++)
    {
        char c = input[i];
        if (c == '\"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else out += c;
    }
    return out;
}

/* =========================================================
   BUILD PAYLOAD
========================================================= */

/* Tracks which invoice_id was synced and the indices of its records */
static uint32_t g_syncing_invoice_id = 0;
static uint32_t g_syncing_indices[64];
static uint32_t g_syncing_count = 0;

/* Collect all unsynced line items for the first unsynced invoice, build payload */
static String build_payload(void)
{
    uint32_t count = storage_get_record_count();
    if (count == 0)
        return "";

    char devname[64] = {0};
    storage_load_device_name(devname, sizeof(devname));
    uint32_t devId = storage_load_device_id();

    String safeName = escape_json(String(devname));
    String safeDevId = escape_json(String(devId));

    /* Find first unsynced invoice_id */
    g_syncing_count = 0;
    g_syncing_invoice_id = 0;
    bool found = false;

    for (uint32_t i = 0; i < count; i++)
    {
        invoice_record_t rec;
        if (!storage_get_record_by_index(i, &rec)) continue;
        if (rec.synced) continue;

        if (!found) {
            g_syncing_invoice_id = rec.invoice_id;
            found = true;
        }

        /* Collect all line items for this same invoice */
        if (rec.invoice_id == g_syncing_invoice_id && g_syncing_count < 64) {
            g_syncing_indices[g_syncing_count++] = i;
        }
    }

    if (!found || g_syncing_count == 0)
        return "";

    /* Build payload: one object with all weights in the array */
    uint32_t total_qty = 0;
    String weightsArr = "";

    for (uint32_t j = 0; j < g_syncing_count; j++)
    {
        invoice_record_t rec;
        if (!storage_get_record_by_index(g_syncing_indices[j], &rec)) continue;

        total_qty += rec.quantity;
        if (j > 0) weightsArr += ",";
        weightsArr += String(rec.total_weight, 3);
    }

    String s = "{";
    s += "\"invoiceId\":" + String(g_syncing_invoice_id) + ",";
    s += "\"deviceId\":\"" + safeDevId + "\",";
    s += "\"quantity\":" + String(total_qty) + ",";
    s += "\"deviceName\":\"" + safeName + "\",";
    s += "\"weightsInKg\":[" + weightsArr + "]";
    s += "}";

    return s;
}

/* =========================================================
   MAIN LOOP
========================================================= */

void sync_service_pause(void)
{
    sync_paused = true;
    devlog_printf("[SYNC] Paused for OTA");
}

void sync_service_resume(void)
{
    sync_paused = false;
    sync_deinited = false;
    last_sync_attempt = millis();
    devlog_printf("[SYNC] Resumed after OTA");
}

void sync_service_deinit(void)
{
    sync_paused = true;
    sync_busy = false;
    sync_deinited = true;
    if (g_active_http) {
        g_active_http->end();
        g_active_http = nullptr;
        devlog_printf("[SYNC] HTTP forcibly closed for OTA");
    }
    last_http_done_ms = millis();
    devlog_printf("[SYNC] Deinitialized for OTA safe mode");
}

void sync_service_loop(void)
{
    if (sync_paused || sync_deinited) return;

    unsigned long now = millis();

    // Only run once every configured interval
    if (now - last_sync_attempt < SYNC_INTERVAL_MS)
        return;

    last_sync_attempt = now;

    if (wifi_service_state() != WIFI_CONNECTED)
    {
        devlog_printf("[SYNC] Skipped - WiFi not connected");
        return;
    }

    if (ota_service_is_busy())
    {
        devlog_printf("[SYNC] Deferred - OTA active");
        last_sync_attempt = millis() - SYNC_INTERVAL_MS + 5000;
        return;
    }

    /* Don't do HTTPS while WiFi scan/connect is in progress — 
       TLS traffic over SDIO bus corrupts scan results on ESP32-P4 */
    if (wifi_service_is_critical())
    {
        devlog_printf("[SYNC] Deferred - WiFi busy (scan/connect)");
        last_sync_attempt = millis() - SYNC_INTERVAL_MS + 5000; // retry in 5s
        return;
    }

    unsigned long connected_ms = wifi_service_connected_since_ms();
    if (connected_ms == 0 || (millis() - connected_ms) < HTTPS_STARTUP_COOLDOWN_MS)
    {
        devlog_printf("[SYNC] Deferred - WiFi settling");
        last_sync_attempt = millis() - SYNC_INTERVAL_MS + 5000;
        return;
    }

    uint32_t pending = storage_get_pending_count();

    if (pending == 0)
    {
        devlog_printf("[SYNC] No pending records");
        return;
    }

    devlog_printf("[SYNC] Starting upload attempt");
    sync_busy = true;   // signal WiFi service: SDIO bus is busy with HTTPS

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(10000);

    g_active_http = &http;

    /* ================= HEALTH CHECK ================= */

    if (!http.begin(client, HEALTH_URL))
    {
        devlog_printf("[SYNC] Health begin failed");
        g_active_http = nullptr;
        sync_busy = false;
        last_http_done_ms = millis();
        return;
    }

    int healthCode = http.GET();
    String healthResp = http.getString();
    http.end();

    g_active_http = nullptr;

    devlog_printf("[SYNC] Health code=%d body=%s",
                  healthCode, healthResp.c_str());

    if (healthCode < 200 || healthCode >= 300)
    {
        sync_busy = false;
        last_http_done_ms = millis();
        return;
    }

    healthResp.trim();
    if (healthResp.indexOf("Healthy") < 0)
    {
        devlog_printf("[SYNC] Server not healthy");
        sync_busy = false;
        last_http_done_ms = millis();
        return;
    }

    /* ================= BUILD JSON ================= */

    String postBody = build_payload();
    if (postBody.length() == 0)
    {
        devlog_printf("[SYNC] Nothing to upload");
        sync_busy = false;
        last_http_done_ms = millis();
        return;
    }

    devlog_printf("[SYNC] POST JSON: %s", postBody.c_str());

    /* ================= POST ================= */

    if (!http.begin(client, BULK_URL))
    {
        devlog_printf("[SYNC] POST begin failed");
        g_active_http = nullptr;
        sync_busy = false;
        last_http_done_ms = millis();
        return;
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("Connection", "close");

    int postCode = http.POST(postBody);
    String resp = http.getString();
    http.end();

    g_active_http = nullptr;

    devlog_printf("[SYNC] POST code=%d resp=%s",
                  postCode, resp.c_str());

    if (postCode >= 200 && postCode < 300)
    {
        devlog_printf("[SYNC] Upload success (record %ld)", g_syncing_index);

        /* Mark only the single synced record */
        if (g_syncing_index >= 0)
        {
            invoice_record_t rec;
            if (storage_get_record_by_index((uint32_t)g_syncing_index, &rec))
            {
                rec.synced = 1;
                storage_update_record((uint32_t)g_syncing_index, &rec);
            }

            /* Decrement pending count */
            uint32_t pending = storage_get_pending_count();
            if (pending > 0)
            {
                pending--;
                if (pending == 0)
                    storage_reset_pending();
                else
                    storage_set_pending(pending);
            }
        }
        g_syncing_index = -1;
    }
    else
    {
        devlog_printf("[SYNC] Upload failed - server rejected");
        g_syncing_index = -1;
    }

    sync_busy = false;
    last_http_done_ms = millis();
    devlog_printf("[SYNC] HTTP done, SDIO cooldown starts");
}
#endif  /* ENABLE_CLOUD_SYNC */
