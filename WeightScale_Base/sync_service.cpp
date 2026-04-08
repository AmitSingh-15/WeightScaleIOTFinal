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

static const char *DEV_HEALTH_URL =
"https://dev.etranscargo.in/weightscale/health";

static const char *DEV_BULK_URL =
"https://dev.etranscargo.in/weightscale/api/WeightIngestion/bulk";

static const char *PROD_HEALTH_URL =
"https://etranscargo.in/weightscale/health";

static const char *PROD_BULK_URL =
"https://etranscargo.in/weightscale/api/WeightIngestion/bulk";

static const unsigned long SYNC_INTERVAL_MS = 30000; // 30 seconds
static const unsigned long HTTPS_STARTUP_COOLDOWN_MS = 10000;

static unsigned long last_sync_attempt = 0;
static bool sync_busy = false;
static bool sync_paused = false;
static unsigned long last_http_done_ms = 0;

static bool sync_deinited = false;
static bool g_env_is_prod = false;

static void finish_sync_attempt(void)
{
    sync_busy = false;
    last_http_done_ms = millis();
    g_active_http = nullptr;
}

static const char *current_health_url(void)
{
    return g_env_is_prod ? PROD_HEALTH_URL : DEV_HEALTH_URL;
}

static const char *current_bulk_url(void)
{
    return g_env_is_prod ? PROD_BULK_URL : DEV_BULK_URL;
}

/* ========================================================= */

bool sync_service_is_busy(void)
{
    return sync_busy;
}

unsigned long sync_service_last_http_ms(void)
{
    return last_http_done_ms;
}

void sync_service_set_env(bool is_prod)
{
    g_env_is_prod = is_prod;
    storage_save_env_prod(is_prod);
    devlog_printf("[SYNC] Environment set to %s", is_prod ? "PROD" : "DEV");
}

bool sync_service_is_prod(void)
{
    return g_env_is_prod;
}

void sync_service_init(void)
{
    last_sync_attempt = 0;
    sync_busy = false;
    sync_paused = false;
    sync_deinited = false;
    last_http_done_ms = 0;
    g_env_is_prod = storage_load_env_prod();
    devlog_printf("[SYNC] Init in %s mode", g_env_is_prod ? "PROD" : "DEV");
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
   NEW: TRACK CURRENT INVOICE
========================================================= */

static uint32_t g_syncing_invoice_id = 0;
static uint32_t g_syncing_indices[64];
static uint32_t g_syncing_count = 0;

/* =========================================================
   BUILD PAYLOAD (FIXED)
========================================================= */

static String build_payload(void)
{
    uint32_t count = storage_get_record_count();
    if (count == 0)
        return "";

    char devname[64] = {0};
    storage_load_device_name(devname, sizeof(devname));
    uint32_t devId = storage_load_device_id();

    String safeName = escape_json(String(devname));

    g_syncing_count = 0;
    g_syncing_invoice_id = 0;

    // 👉 pick first unsynced invoice
    for (uint32_t i = 0; i < count; i++)
    {
        invoice_record_t rec;
        if (!storage_get_record_by_index(i, &rec)) continue;
        if (rec.synced) continue;

        g_syncing_invoice_id = rec.invoice_id;
        break;
    }

    if (g_syncing_invoice_id == 0)
        return "";

    // 👉 collect records for that invoice
    for (uint32_t i = 0; i < count; i++)
    {
        invoice_record_t rec;
        if (!storage_get_record_by_index(i, &rec)) continue;

        if (!rec.synced && rec.invoice_id == g_syncing_invoice_id)
        {
            if (g_syncing_count < 64)
                g_syncing_indices[g_syncing_count++] = i;
        }
    }

    if (g_syncing_count == 0)
        return "";

    uint32_t total_qty = 0;
    String weightsArr = "[";

    for (uint32_t j = 0; j < g_syncing_count; j++)
    {
        invoice_record_t rec;
        if (!storage_get_record_by_index(g_syncing_indices[j], &rec)) continue;

        total_qty += rec.quantity;

        if (j > 0) weightsArr += ",";
        weightsArr += String(rec.total_weight, 3);
    }

    weightsArr += "]";

    String s = "{";
    s += "\"invoiceId\":" + String(g_syncing_invoice_id) + ",";
    s += "\"deviceId\":\"" + String(devId) + "\",";
    s += "\"quantity\":" + String(total_qty) + ",";
    s += "\"deviceName\":\"" + safeName + "\",";
    s += "\"weightsInKg\":" + weightsArr;
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
    if (sync_busy) return;

    unsigned long now = millis();

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

    if (wifi_service_is_critical())
    {
        devlog_printf("[SYNC] Deferred - WiFi busy (scan/connect)");
        last_sync_attempt = millis() - SYNC_INTERVAL_MS + 5000;
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
    uint32_t rec_count = storage_get_record_count();

    if (pending == 0 || rec_count == 0)
    {
        devlog_printf("[SYNC] No pending records");
        return;
    }

    sync_busy = true;
    devlog_printf("[SYNC] Starting upload attempt (%s)",
                  g_env_is_prod ? "PROD" : "DEV");

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(10000);

    g_active_http = &http;

    /* HEALTH */

    if (!http.begin(client, current_health_url()))
    {
        finish_sync_attempt();
        return;
    }

    int healthCode = http.GET();
    String healthResp = http.getString();
    http.end();

    if (healthCode < 200 || healthCode >= 300 || healthResp.indexOf("Healthy") < 0)
    {
        finish_sync_attempt();
        return;
    }

    /* BUILD */

    String postBody = build_payload();
    if (postBody.length() == 0)
    {
        finish_sync_attempt();
        return;
    }

    devlog_printf("[SYNC] POST JSON: %s", postBody.c_str());

    /* POST */

    if (!http.begin(client, current_bulk_url()))
    {
        finish_sync_attempt();
        return;
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("Connection", "close");

    int postCode = http.POST(postBody);
    String resp = http.getString();
    http.end();

    devlog_printf("[SYNC] POST code=%d resp=%s",
                  postCode, resp.c_str());

    if (postCode >= 200 && postCode < 300)
    {
        devlog_printf("[SYNC] Upload success (invoice %lu)", g_syncing_invoice_id);

        // ✅ FIX: mark ONLY current invoice records
        for (uint32_t i = 0; i < g_syncing_count; i++)
        {
            uint32_t idx = g_syncing_indices[i];

            invoice_record_t rec;
            if (storage_get_record_by_index(idx, &rec))
            {
                rec.synced = 1;
                storage_update_record(idx, &rec);
            }
        }
    }
    else
    {
        devlog_printf("[SYNC] Upload failed - server rejected");
    }

    finish_sync_attempt();
}

#endif
