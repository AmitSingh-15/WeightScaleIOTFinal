#include "config/app_config.h"

#if ENABLE_OTA_UPDATES

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>
#include "ota_service.h"
#include "home_screen.h"
#include "wifi_service.h"
#include "devlog.h"

// ================= STATE =================
static bool ota_busy = false;
static unsigned long last_ota_check = 0;

// ================= CALLBACKS =================
static ota_display_cb_t display_cb = nullptr;
static ota_progress_cb_t progress_cb = nullptr;

// ================= API =================
void ota_service_set_display_callback(ota_display_cb_t cb)
{
    display_cb = cb;
}

void ota_service_set_progress_callback(ota_progress_cb_t cb)
{
    progress_cb = cb;
}

bool ota_service_is_busy(void)
{
    return ota_busy;
}

// ================= VERSION =================
#define OTA_VERSION "2.1.1"
#define OTA_VERSION_URL "https://raw.githubusercontent.com/AmitSingh-15/WeightScaleIOTFinal/main/firmware/version.txt"
#define OTA_BIN_URL "https://raw.githubusercontent.com/AmitSingh-15/WeightScaleIOTFinal/main/firmware/Weights.bin"

static Preferences prefs;

// ================= HELPERS =================
static bool wait_for_wifi_settle(int timeout_ms)
{
    int elapsed = 0;
    while (elapsed < timeout_ms) {
        if (wifi_service_state() == WIFI_CONNECTED) {
            unsigned long connected_ms = wifi_service_connected_since_ms();
            if (connected_ms != 0 && (millis() - connected_ms) >= OTA_WIFI_SETTLE_MS) {
                devlog_printf("[OTA] WiFi settled");
                return true;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200));
        elapsed += 200;
    }
    return false;
}

static String get_stored_version()
{
    prefs.begin("ota", true);
    String ver = prefs.getString("version", "");
    prefs.end();
    return ver;
}

static void save_version(const String &ver)
{
    prefs.begin("ota", false);
    prefs.putString("version", ver);
    prefs.end();
}

// ================= VERSION COMPARE =================
static bool is_newer_version(String remote)
{
    remote.trim();

    int rM, rN, rP;
    int lM, lN, lP;

    if (sscanf(remote.c_str(), "%d.%d.%d", &rM, &rN, &rP) != 3) {
        devlog_printf("[OTA] Invalid remote version");
        return false;
    }

    String current = get_stored_version();
    current.trim();

    if (current.length() == 0) return true;

    if (sscanf(current.c_str(), "%d.%d.%d", &lM, &lN, &lP) != 3) {
        devlog_printf("[OTA] Invalid local version -> force update");
        return true;
    }

    if (rM != lM) return rM > lM;
    if (rN != lN) return rN > lN;
    return rP > lP;
}

// ================= DOWNLOAD WITH RETRY =================
static bool ota_download_firmware(int totalSize)
{
    static const int retry_delays[] = { 5000, 15000, 30000 };

    for (int attempt = 0; attempt < OTA_MAX_RETRIES; attempt++) {
        if (attempt > 0) {
            int delay_ms = retry_delays[attempt - 1];
            devlog_printf("[OTA] Retry %d/%d in %dms", attempt + 1, OTA_MAX_RETRIES, delay_ms);
            if (display_cb) display_cb("OTA: Retrying...");
            vTaskDelay(pdMS_TO_TICKS(delay_ms));

            // Re-check WiFi before retry
            if (wifi_service_state() != WIFI_CONNECTED) {
                devlog_printf("[OTA] WiFi lost before retry");
                if (!wait_for_wifi_settle(15000)) {
                    devlog_printf("[OTA] WiFi not available for retry");
                    continue;
                }
            }
        }

        WiFiClientSecure client;
        client.setInsecure();
        client.setTimeout(OTA_HTTP_TIMEOUT_MS / 1000);

        HTTPClient http;
        http.begin(client, OTA_BIN_URL);
        http.setTimeout(OTA_HTTP_TIMEOUT_MS);
        int code = http.GET();

        if (code != 200) {
            devlog_printf("[OTA] HTTP %d on attempt %d", code, attempt + 1);
            http.end();
            continue;
        }

        int reported_size = http.getSize();
        if (reported_size > 0 && reported_size != totalSize) {
            totalSize = reported_size;  // Use server-reported size
        }

        if (!Update.begin(totalSize)) {
            devlog_printf("[OTA] Update.begin() failed, attempt %d", attempt + 1);
            http.end();
            continue;
        }

        WiFiClient *stream = http.getStreamPtr();
        uint8_t buf[OTA_CHUNK_SIZE];
        int written = 0;
        int lastPct = -1;
        unsigned long last_progress_ms = millis();
        bool download_ok = true;

        if (display_cb) display_cb("OTA: Downloading...");

        while (written < totalSize) {
            // Stall detection
            if ((millis() - last_progress_ms) > OTA_STALL_TIMEOUT_MS) {
                devlog_printf("[OTA] STALL at %d/%d (%d%%)",
                              written, totalSize, (totalSize > 0) ? (written * 100 / totalSize) : 0);
                download_ok = false;
                break;
            }

            int avail = stream->available();
            if (avail <= 0) {
                vTaskDelay(pdMS_TO_TICKS(50));
                continue;
            }

            int toRead = (avail < OTA_CHUNK_SIZE) ? avail : OTA_CHUNK_SIZE;
            int len = stream->readBytes(buf, toRead);

            if (len <= 0) {
                vTaskDelay(pdMS_TO_TICKS(50));
                continue;
            }

            size_t wr = Update.write(buf, len);
            if (wr != (size_t)len) {
                devlog_printf("[OTA] Flash write error at %d", written);
                download_ok = false;
                break;
            }

            written += len;
            last_progress_ms = millis();

            // Progress UI (every 2%)
            int pct = (totalSize > 0) ? (written * 100 / totalSize) : 0;
            if (pct >= lastPct + 2) {
                lastPct = pct;
                if (progress_cb) progress_cb(pct);
                if (pct % 10 == 0) {
                    devlog_printf("[OTA] %d%% (%d/%d)", pct, written, totalSize);
                }
            }

            // SDIO breathing room
            vTaskDelay(pdMS_TO_TICKS(OTA_CHUNK_DELAY_MS));
        }

        http.end();

        if (download_ok && written >= totalSize) {
            if (Update.end(true)) {
                devlog_printf("[OTA] Download+verify SUCCESS on attempt %d", attempt + 1);
                return true;
            }
            devlog_printf("[OTA] Update.end() failed on attempt %d", attempt + 1);
        } else {
            Update.abort();
            devlog_printf("[OTA] Download aborted on attempt %d (ok=%d written=%d)",
                          attempt + 1, download_ok, written);
        }
    }

    return false;  // All retries exhausted
}

// ================= PUBLIC VERSION API =================
String ota_service_stored_version(void)
{
    return get_stored_version();
}

String ota_service_current_version(void)
{
    return String(OTA_VERSION);
}

// ================= INIT =================
void ota_service_init(void)
{
    if (get_stored_version() == "") {
        save_version(OTA_VERSION);
    }
}

// ================= OTA MAIN =================
void ota_service_check_and_update(void)
{
    devlog_printf("[OTA] Checking...");

    if (ota_busy) {
        devlog_printf("[OTA] Already running");
        if (display_cb) display_cb("OTA: Already running");
        return;
    }

    if (millis() - last_ota_check < OTA_CHECK_INTERVAL_MS) {
        devlog_printf("[OTA] Cooldown active");
        if (display_cb) display_cb("OTA: Wait 30s before retry");
        return;
    }

    last_ota_check = millis();
    ota_busy = true;

    if (!wait_for_wifi_settle(15000)) {
        devlog_printf("[OTA] WiFi not available");
        if (display_cb) display_cb("OTA: WiFi offline");
        ota_busy = false;
        return;
    }

    // ===== VERSION FETCH =====
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(OTA_HTTP_TIMEOUT_MS / 1000);

    HTTPClient http;
    http.begin(client, OTA_VERSION_URL);
    http.setTimeout(OTA_HTTP_TIMEOUT_MS);
    int code = http.GET();

    if (code != 200) {
        devlog_printf("[OTA] Version fetch failed: %d", code);
        http.end();
        if (display_cb) display_cb("OTA: Server error");
        ota_busy = false;
        return;
    }

    String remote = http.getString();
    http.end();
    remote.trim();

    devlog_printf("[OTA] Remote: %s, Local: %s", remote.c_str(), get_stored_version().c_str());

    if (!is_newer_version(remote)) {
        devlog_printf("[OTA] Up to date");
        if (display_cb) display_cb("OTA: Latest version (" + get_stored_version() + ")");
        ota_busy = false;
        return;
    }

    // ================= SAFE MODE (now handled by wifi_ota_task) =================
    devlog_printf("[OTA] Update available (%s). Downloading...", remote.c_str());

    if (display_cb) display_cb("OTA: Preparing...");

    // Let SDIO settle
    vTaskDelay(pdMS_TO_TICKS(2000));

    if (!wait_for_wifi_settle(10000)) {
        devlog_printf("[OTA] WiFi lost during download prep");
        if (display_cb) display_cb("OTA: WiFi lost");
        ota_busy = false;
        return;
    }

    // Get file size via HEAD request (avoids downloading firmware twice)
    {
        WiFiClientSecure sz_client;
        sz_client.setInsecure();
        sz_client.setTimeout(OTA_HTTP_TIMEOUT_MS / 1000);
        HTTPClient sz_http;
        sz_http.begin(sz_client, OTA_BIN_URL);
        sz_http.setTimeout(OTA_HTTP_TIMEOUT_MS);
        const char *headerKeys[] = { "Content-Length" };
        sz_http.collectHeaders(headerKeys, 1);
        int sz_code = sz_http.sendRequest("HEAD");
        int totalSize = 0;
        if (sz_code == 200) {
            totalSize = sz_http.getSize();
        }
        sz_http.end();

        if (totalSize <= 0) {
            devlog_printf("[OTA] Could not determine firmware size");
            if (display_cb) display_cb("OTA: Size check failed");
            ota_busy = false;
            return;
        }

        devlog_printf("[OTA] Firmware size: %d bytes", totalSize);

        // ===== DOWNLOAD WITH RETRY =====
        bool success = ota_download_firmware(totalSize);

        if (!success) {
            devlog_printf("[OTA] FAILED after %d retries", OTA_MAX_RETRIES);
            if (display_cb) display_cb("OTA: Update failed. Try later.");
            ota_busy = false;
            return;
        }
    }

    // ===== SUCCESS =====
    save_version(remote);
    devlog_printf("[OTA] SUCCESS — version %s installed", remote.c_str());

    if (display_cb) display_cb("Update success. Rebooting...");
    if (progress_cb) progress_cb(100);

    ota_busy = false;
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP.restart();
}

#endif