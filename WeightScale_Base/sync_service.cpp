#include "sync_service.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "storage_service.h"
#include "wifi_service.h"
#include "devlog.h"

/* =========================================================
   CONFIG
========================================================= */

static const char *HEALTH_URL =
"https://dev.etranscargo.in/weightscale/health";

static const char *BULK_URL =
"https://dev.etranscargo.in/weightscale/api/WeightIngestion/bulk";

static const unsigned long SYNC_INTERVAL_MS = 30000; // 30 seconds

static unsigned long last_sync_attempt = 0;

/* ========================================================= */

void sync_service_init(void)
{
    last_sync_attempt = 0;
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

static String build_payload(void)
{
    uint32_t count = storage_get_record_count();
    if (count == 0)
        return "";

    char devname[64] = {0};
    storage_load_device_name(devname, sizeof(devname));

    String safeName = escape_json(String(devname));

    String s = "{";
    s += "\"deviceId\":0,";
    s += "\"deviceName\":\"" + safeName + "\",";
    s += "\"weightsInKg\":[";

    bool first = true;
    bool hasData = false;

    for (uint32_t i = 0; i < count; i++)
    {
        invoice_record_t rec;
        if (storage_get_record_by_index(i, &rec))
        {
            if (rec.synced) continue;

            if (!first) s += ",";
            s += String(rec.total_weight, 3);

            first = false;
            hasData = true;
        }
    }

    s += "]}";

    if (!hasData)
        return "";

    return s;
}

/* =========================================================
   MAIN LOOP (5 MINUTE INTERVAL)
========================================================= */

void sync_service_loop(void)
{
    unsigned long now = millis();

    // Only run once every 5 minutes
    if (now - last_sync_attempt < SYNC_INTERVAL_MS)
        return;

    last_sync_attempt = now;

    if (wifi_service_state() != WIFI_CONNECTED)
    {
        devlog_printf("[SYNC] Skipped - WiFi not connected");
        return;
    }

    uint32_t pending = storage_get_pending_count();
    uint32_t rec_count = storage_get_record_count();

    if (pending == 0 || rec_count == 0)
    {
        devlog_printf("[SYNC] No pending records");
        return;
    }

    devlog_printf("[SYNC] Starting upload attempt");

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(10000);

    /* ================= HEALTH CHECK ================= */

    if (!http.begin(client, HEALTH_URL))
    {
        devlog_printf("[SYNC] Health begin failed");
        return;
    }

    int healthCode = http.GET();
    String healthResp = http.getString();
    http.end();

    devlog_printf("[SYNC] Health code=%d body=%s",
                  healthCode, healthResp.c_str());

    if (healthCode < 200 || healthCode >= 300)
        return;

    healthResp.trim();
    if (healthResp.indexOf("Healthy") < 0)
    {
        devlog_printf("[SYNC] Server not healthy");
        return;
    }

    /* ================= BUILD JSON ================= */

    String postBody = build_payload();
    if (postBody.length() == 0)
    {
        devlog_printf("[SYNC] Nothing to upload");
        return;
    }

    devlog_printf("[SYNC] POST JSON: %s", postBody.c_str());

    /* ================= POST ================= */

    if (!http.begin(client, BULK_URL))
    {
        devlog_printf("[SYNC] POST begin failed");
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
        devlog_printf("[SYNC] Upload success");

        for (uint32_t i = 0; i < rec_count; i++)
        {
            invoice_record_t rec;
            if (storage_get_record_by_index(i, &rec))
            {
                if (rec.synced == 0)
                {
                    rec.synced = 1;
                    storage_update_record(i, &rec);
                }
            }
        }

        storage_reset_pending();
    }
    else
    {
        devlog_printf("[SYNC] Upload failed - server rejected");
    }
}