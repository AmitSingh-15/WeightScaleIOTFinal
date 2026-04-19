#include "config/app_config.h"  /* Must be first for feature flags */

#if ENABLE_CLOUD_SYNC

#include "sync_service.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "storage_service.h"
#include "wifi_service.h"
#include "ota_service.h"
#include "devlog.h"
#include <esp_task_wdt.h>

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

static const unsigned long HTTPS_STARTUP_COOLDOWN_MS = SYNC_HTTPS_COOLDOWN_MS;

static unsigned long last_sync_attempt = 0;
static bool sync_busy = false;
static bool sync_paused = false;
static unsigned long last_http_done_ms = 0;
static unsigned long transport_cooldown_until_ms = 0;
static uint8_t consecutive_transport_failures = 0;

static bool sync_deinited = false;
static bool g_env_is_prod = false;

static void finish_sync_attempt(void)
{
    sync_busy = false;
    last_http_done_ms = millis();
    g_active_http = nullptr;
}

static void note_transport_success(void)
{
    consecutive_transport_failures = 0;
    transport_cooldown_until_ms = 0;
}

static void note_transport_failure(const char *stage, int err_code)
{
    consecutive_transport_failures++;
    transport_cooldown_until_ms = millis() + SYNC_TRANSPORT_COOLDOWN_MS;
    devlog_printf("[SYNC] Transport failure during %s: %d (count=%u)",
                  stage, err_code, (unsigned)consecutive_transport_failures);

    if (consecutive_transport_failures >= 2) {
        devlog_printf("[SYNC] Requesting WiFi reconnect after repeated transport failures");
        wifi_service_request_reconnect();
    }
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
    transport_cooldown_until_ms = 0;
    consecutive_transport_failures = 0;
    g_env_is_prod = storage_load_env_prod();
    devlog_printf("[SYNC] Init in %s mode", g_env_is_prod ? "PROD" : "DEV");
}

/* =========================================================
   JSON ESCAPE — writes into caller-provided buffer
========================================================= */

static size_t escape_json_buf(const char *input, char *out, size_t out_size)
{
    size_t pos = 0;
    for (size_t i = 0; input[i] && pos < out_size - 2; i++)
    {
        char c = input[i];
        if (c == '\"' && pos < out_size - 3) { out[pos++] = '\\'; out[pos++] = '\"'; }
        else if (c == '\\' && pos < out_size - 3) { out[pos++] = '\\'; out[pos++] = '\\'; }
        else if (c == '\n' && pos < out_size - 3) { out[pos++] = '\\'; out[pos++] = 'n'; }
        else if (c == '\r' && pos < out_size - 3) { out[pos++] = '\\'; out[pos++] = 'r'; }
        else out[pos++] = c;
    }
    out[pos] = '\0';
    return pos;
}

/* =========================================================
   NEW: TRACK CURRENT INVOICE
========================================================= */

static uint32_t g_syncing_invoice_id = 0;
static uint32_t g_syncing_indices[64];
static uint32_t g_syncing_count = 0;

/* =========================================================
   BUILD PAYLOAD — uses static buffer to avoid heap fragmentation
========================================================= */

// Static buffer: ~4KB is enough for 64 weights * ~12 chars + overhead
static char g_payload_buf[4096];

static const char* build_payload(void)
{
    uint32_t count = storage_get_record_count();
    if (count == 0)
        return nullptr;

    char devname[64] = {0};
    storage_load_device_name(devname, sizeof(devname));
    uint32_t devId = storage_load_device_id();

    char safeName[128];
    escape_json_buf(devname, safeName, sizeof(safeName));

    g_syncing_count = 0;
    g_syncing_invoice_id = 0;

    // Pick first unsynced invoice
    for (uint32_t i = 0; i < count; i++)
    {
        invoice_record_t rec;
        if (!storage_get_record_by_index(i, &rec)) continue;
        if (rec.synced) continue;

        g_syncing_invoice_id = rec.invoice_id;
        break;
    }

    if (g_syncing_invoice_id == 0)
        return nullptr;

    // Collect records for that invoice
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
        return nullptr;

    uint32_t total_qty = 0;

    // Build weights array into a temp portion of the buffer
    char weights_buf[2048];
    int wpos = 0;
    weights_buf[wpos++] = '[';

    for (uint32_t j = 0; j < g_syncing_count; j++)
    {
        invoice_record_t rec;
        if (!storage_get_record_by_index(g_syncing_indices[j], &rec)) continue;

        total_qty += rec.quantity;

        if (j > 0 && wpos < (int)sizeof(weights_buf) - 1)
            weights_buf[wpos++] = ',';

        int written = snprintf(weights_buf + wpos, sizeof(weights_buf) - wpos, "%.3f", rec.total_weight);
        if (written > 0) wpos += written;
    }

    if (wpos < (int)sizeof(weights_buf) - 1)
        weights_buf[wpos++] = ']';
    weights_buf[wpos] = '\0';

    // Build final JSON — check for truncation
    int written = snprintf(g_payload_buf, sizeof(g_payload_buf),
             "{\"invoiceId\":%lu,\"deviceId\":\"%lu\",\"quantity\":%lu,\"deviceName\":\"%s\",\"weightsInKg\":%s}",
             (unsigned long)g_syncing_invoice_id,
             (unsigned long)devId,
             (unsigned long)total_qty,
             safeName,
             weights_buf);

    if (written < 0 || written >= (int)sizeof(g_payload_buf)) {
        devlog_printf("[SYNC] Payload too large (%d bytes), skipping", written);
        g_syncing_count = 0;
        return nullptr;
    }

    return g_payload_buf;
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
    transport_cooldown_until_ms = 0;
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
    transport_cooldown_until_ms = 0;
    consecutive_transport_failures = 0;
    last_http_done_ms = millis();
    devlog_printf("[SYNC] Deinitialized for OTA safe mode");
}

void sync_service_loop(void)
{
    if (sync_paused || sync_deinited) return;
    if (sync_busy) return;

    unsigned long now = millis();

    if (transport_cooldown_until_ms != 0 &&
        (long)(now - transport_cooldown_until_ms) < 0)
        return;

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
    client.setTimeout(5000);

    HTTPClient http;
    http.setTimeout(5000);

    g_active_http = &http;

    /* HEALTH */

    if (!http.begin(client, current_health_url()))
    {
        note_transport_failure("health begin", -1);
        finish_sync_attempt();
        return;
    }

    esp_task_wdt_reset();  /* feed WDT before blocking HTTP */
    int healthCode = http.GET();
    esp_task_wdt_reset();  /* feed WDT after blocking HTTP */
    String healthResp;
    if (healthCode > 0) {
        healthResp = http.getString();
    }
    http.end();

    if (healthCode <= 0)
    {
        note_transport_failure("health GET", healthCode);
        finish_sync_attempt();
        return;
    }

    if (healthCode < 200 || healthCode >= 300 || healthResp.indexOf("Healthy") < 0)
    {
        note_transport_success();
        finish_sync_attempt();
        return;
    }

    /* BUILD */

    const char* postBody = build_payload();
    if (postBody == nullptr || postBody[0] == '\0')
    {
        finish_sync_attempt();
        return;
    }

    devlog_printf("[SYNC] POST JSON: %s", postBody);

    /* POST */

    if (!http.begin(client, current_bulk_url()))
    {
        note_transport_failure("bulk begin", -1);
        finish_sync_attempt();
        return;
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("Connection", "close");

    esp_task_wdt_reset();  /* feed WDT before blocking HTTP */
    int postCode = http.POST((uint8_t*)postBody, strlen(postBody));
    esp_task_wdt_reset();  /* feed WDT after blocking HTTP */
    String resp;
    if (postCode > 0) {
        resp = http.getString();
    }
    http.end();

    devlog_printf("[SYNC] POST code=%d resp=%s",
                  postCode, resp.c_str());

    if (postCode <= 0)
    {
        note_transport_failure("bulk POST", postCode);
        finish_sync_attempt();
        return;
    }

    note_transport_success();

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
