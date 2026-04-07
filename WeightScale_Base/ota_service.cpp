#include "app_config.h"  /* Must be first for feature flags */

#if ENABLE_OTA_UPDATES

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include "ota_service.h"
#include "home_screen.h"
#include "sync_service.h"
#include "devlog.h"  // ✅ Include devlog to use devlog_printf


#include "scale_service_v2.h"  // For scale_service_suspend

#define OTA_MODE_ACTIVE 1

#include "esp_wifi.h"
#include "esp_wifi_types.h"

// ===== WIFI READY CHECK FOR OTA =====
static bool wait_for_wifi_ready(int timeout_ms)
{
    int elapsed = 0;
    while (elapsed < timeout_ms) {
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            devlog_printf("[OTA] WiFi ready: %s", (char*)ap.ssid);
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
        elapsed += 200;
    }
    devlog_printf("[OTA] WiFi NOT ready");
    return false;
}

// ===== OTA CONFIG =====
#define OTA_VERSION        "1.1.0"             // current firmware version
#define OTA_DEFAULT_VERSION OTA_VERSION
#define OTA_VERSION_URL    "https://raw.githubusercontent.com/AmitSingh-15/WeightScaleIOTFinal/refs/heads/main/firmware/version.txt"
#define OTA_BIN_URL        "https://raw.githubusercontent.com/AmitSingh-15/WeightScaleIOTFinal/refs/heads/main/firmware/Weights.bin"

static WiFiClientSecure client;
static ota_display_cb_t display_cb = nullptr;
static ota_progress_cb_t progress_cb = nullptr;
static Preferences prefs;

/* ================= VERSION STORAGE ================= */
static String get_stored_version()
{
    prefs.begin("ota", true); // read-only
    String ver = prefs.getString("version", "");
    prefs.end();

    devlog_printf("[OTA] Stored version fetched: %s", ver.c_str());
    return ver;
}

String ota_service_stored_version(void)
{
    return get_stored_version(); // reads Preferences
}

static void save_version(const String &ver)
{
    prefs.begin("ota", false); // read/write
    prefs.putString("version", ver);
    prefs.end();

    devlog_printf("[OTA] Saved new version: %s", ver.c_str());
}

/* ================= OTA CALLBACK ================= */
void ota_service_set_display_callback(ota_display_cb_t cb)
{
    display_cb = cb;
}

void ota_service_set_progress_callback(ota_progress_cb_t cb)
{
    progress_cb = cb;
}

String ota_service_current_version(void)
{
    String ver = get_stored_version();
    devlog_printf("[OTA] Current version returned: %s", ver.c_str());
    return ver;
}

/* ================= VERSION COMPARISON ================= */
static bool is_newer_version(const String &remote)
{
    int r_major, r_minor, r_patch;
    int l_major, l_minor, l_patch;

    devlog_printf("[OTA] Comparing versions. Remote=%s", remote.c_str());

    if (sscanf(remote.c_str(), "%d.%d.%d", &r_major, &r_minor, &r_patch) != 3) {
        devlog_printf("[OTA] Remote version format invalid");
        return false;
    }

    String current = get_stored_version(); // ✅ get stored version
    if (sscanf(current.c_str(), "%d.%d.%d", &l_major, &l_minor, &l_patch) != 3) {
        devlog_printf("[OTA] Current stored version format invalid");
        return false;
    }

    if (r_major != l_major) return r_major > l_major;
    if (r_minor != l_minor) return r_minor > l_minor;
    return r_patch > l_patch;
}

/* ================= INIT ================= */
void ota_service_init(void)
{
    client.setInsecure(); // replace with certificate validation if needed
    String stored = get_stored_version();
    if (stored == "") {
        save_version(OTA_VERSION);
        devlog_printf("[OTA] No version stored. Setting default: %s", OTA_VERSION);
    }

    devlog_printf("[OTA] OTA service initialized with version: %s", get_stored_version().c_str());
}

/* ================= OTA UPDATE ================= */
void ota_service_check_and_update(void)
{
    devlog_printf("[OTA] Starting OTA check...");

    if (WiFi.status() != WL_CONNECTED) {
        if(display_cb) display_cb("OTA: Wi-Fi not connected");
        devlog_printf("[OTA] Wi-Fi not connected");
        return;
    }

    HTTPClient http;
    http.setTimeout(15000);
    http.begin(client, OTA_VERSION_URL);

    int code = http.GET();
    if (code != 200) {
        if(display_cb) display_cb("OTA: Version fetch fail");
        devlog_printf("[OTA] Version fetch failed with code: %d", code);
        http.end();
        return;
    }

    String remote_version = http.getString();
    remote_version.trim();
    http.end();

    devlog_printf("[OTA] Remote version fetched: %s", remote_version.c_str());
    if (display_cb) display_cb("Remote: " + remote_version);
    delay(500);

    if (!is_newer_version(remote_version)) {
        if(display_cb) display_cb("Latest Version");
        devlog_printf("[OTA] Device already on latest version: %s", remote_version.c_str());
        return;
    }


    // ===== ENTER OTA SAFE MODE (CLEAN + CONTROLLED) =====
    devlog_printf("[OTA] Entering SAFE MODE");

#if ENABLE_CLOUD_SYNC
    sync_service_deinit();
    devlog_printf("[OTA] Sync stopped");
#endif

    scale_service_suspend();
    devlog_printf("[OTA] Scale suspended");

    // Reduce UI load (DO NOT kill LVGL completely)
    // lv_timer_pause_all(); // Not available in LVGL 8.x
    lv_obj_clean(lv_scr_act());

    // Show simple OTA message
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Updating...\nDo not turn off");
    lv_obj_center(label);

    // Heap check before OTA
    size_t heap = esp_get_free_heap_size();
    devlog_printf("[OTA] Heap before OTA: %lu", (unsigned long)heap);
    if (heap < 250 * 1024) {
        devlog_printf("[OTA] Heap too low for OTA: %lu bytes", (unsigned long)heap);
        if (display_cb) display_cb("OTA: Heap too low");
        vTaskDelay(pdMS_TO_TICKS(2000));
        return;
    }

    // ===== WIFI STABILIZATION =====
    if (!wait_for_wifi_ready(5000)) {
        devlog_printf("[OTA] Abort: WiFi not ready");
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(500)); // stabilization delay

    // ===== SINGLE-CONNECTION OTA DOWNLOAD =====
    // 4KB chunks, no artificial delay — let the TCP stack flow naturally.
    // All other bus-contending services (scale, sync) are suspended.

    #define OTA_CHUNK 4096
    client.setTimeout(60);  // 60s read timeout

    HTTPClient http2;
    http2.setTimeout(30000);
    http2.begin(client, OTA_BIN_URL);
    int httpCode = http2.GET();

    if (httpCode != 200) {
        devlog_printf("[OTA] Binary fetch failed: %d", httpCode);
        if (display_cb) display_cb("OTA: Download failed");
        http2.end();
        sync_service_resume();
        return;
    }

    int totalSize = http2.getSize();
    devlog_printf("[OTA] Binary size: %d bytes", totalSize);

    if (totalSize <= 0) {
        devlog_printf("[OTA] Invalid binary size");
        if (display_cb) display_cb("OTA: Bad file size");
        http2.end();
        sync_service_resume();
        return;
    }

    if (!Update.begin(totalSize)) {
        devlog_printf("[OTA] Not enough space for OTA");
        if (display_cb) display_cb("OTA: No space");
        http2.end();
        sync_service_resume();
        return;
    }

    WiFiClient *stream = http2.getStreamPtr();
    uint8_t buffer[OTA_CHUNK];
    int written = 0;
    int lastPct = -1;
    unsigned long lastActivity = millis();

    devlog_printf("[OTA] Starting download (4KB chunks, throttled)...");

    while (written < totalSize) {
        int avail = stream->available();
        if (avail <= 0) {
            if (millis() - lastActivity > 120000UL) {  // 2 min timeout
                devlog_printf("[OTA] Download stalled for 2min, aborting");
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        int toRead = min(avail, (int)OTA_CHUNK);
        toRead = min(toRead, totalSize - written);
        int len = stream->readBytes(buffer, toRead);

        if (len > 0) {
            size_t bytesWritten = Update.write(buffer, len);
            if (bytesWritten != (size_t)len) {
                devlog_printf("[OTA] Flash write failed: wrote %d of %d", bytesWritten, len);
                break;
            }
            written += len;
            vTaskDelay(pdMS_TO_TICKS(10)); // 🔥 critical: prevents SDIO starvation
        } else {
            if (millis() - lastActivity > 120000UL) break;
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        lastActivity = millis();

        int pct = (written * 100) / totalSize;
        if (pct != lastPct) {
            lastPct = pct;
            if (progress_cb) progress_cb(pct);
            if (pct % 5 == 0) {
                devlog_printf("[OTA] Progress: %d%% (%d/%d)", pct, written, totalSize);
            }
        }
    }


    http2.end();

    if (written != totalSize) {
        Update.abort();
        devlog_printf("[OTA] Download incomplete: %d/%d bytes", written, totalSize);
        if (display_cb) display_cb("OTA: Download incomplete");
        sync_service_resume();
        return;
    }

    if (!Update.end(true)) {
        devlog_printf("[OTA] Update finalize failed");
        if (display_cb) display_cb("OTA: Finalize failed");
        sync_service_resume();
        return;
    }

    // Success!
    save_version(remote_version);
    devlog_printf("[OTA] Update successful! New version: %s", remote_version.c_str());
    home_screen_set_version(remote_version.c_str());
    if (display_cb) display_cb("Update success. Rebooting...");
    delay(2000);
    ESP.restart();
}

#endif  /* ENABLE_OTA_UPDATES */
