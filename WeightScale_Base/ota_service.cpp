#include "app_config.h"  /* Must be first for feature flags */

#if ENABLE_OTA_UPDATES

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPUpdate.h>
#include <Preferences.h>
#include "ota_service.h"
#include "home_screen.h"
#include "devlog.h"  // ✅ Include devlog to use devlog_printf

// ===== OTA CONFIG =====
#define OTA_VERSION        "1.0.8"             // current firmware version
#define OTA_DEFAULT_VERSION OTA_VERSION
#define OTA_VERSION_URL    "https://raw.githubusercontent.com/rameshkadam61/weighingscale/refs/heads/main/firmware/version.txt"
#define OTA_BIN_URL        "https://raw.githubusercontent.com/rameshkadam61/weighingscale/refs/heads/main/firmware/Weights.bin"

static WiFiClientSecure client;
static ota_display_cb_t display_cb = nullptr;
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
        if(display_cb) display_cb("[OTA] Wi-Fi not connected");
        devlog_printf("[OTA] Wi-Fi not connected");
        return;
    }

    HTTPClient http;
    http.setTimeout(5000);
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

    if (display_cb) display_cb("Updating to " + remote_version);
    devlog_printf("[OTA] New version detected. Updating to %s", remote_version.c_str());
    delay(500);

    // OTA progress callback
    httpUpdate.onProgress([&](int cur, int total) {
        int percent = (cur * 100) / total;
        devlog_printf("[OTA] Update progress: %d%%", percent);
    });

    t_httpUpdate_return ret = httpUpdate.update(client, OTA_BIN_URL);

      String stored;
      String current;

    switch (ret) {
        case HTTP_UPDATE_OK:{
            save_version(remote_version); // ✅ save new version before reboot
            devlog_printf("[OTA] Update successful. New version saved: %s", remote_version.c_str());

        
            devlog_printf("[OTA] Update successful. New version: %s", remote_version.c_str());
            home_screen_set_version(remote_version.c_str());
            
        
                  // Fetch and log versions for debugging
             String stored = ota_service_stored_version();
              devlog_printf("[OTA] Stored version (from Preferences): %s", stored.c_str());
        
              String current = ota_service_current_version();
              devlog_printf("[OTA] Current version (from get_stored_version): %s", current.c_str());
                      
            
                if(display_cb) display_cb("Update success. Rebooting...");
                delay(2000);
           // ESP.restart();
            break;
    }
        case HTTP_UPDATE_FAILED:
            if(display_cb) {
                display_cb(String("Update failed: ") +
                           httpUpdate.getLastError() +
                           " " +
                           httpUpdate.getLastErrorString());
            }
            devlog_printf("[OTA] Update failed: %d %s", httpUpdate.getLastError(), httpUpdate.getLastErrorString());
            delay(500);
            break;

        case HTTP_UPDATE_NO_UPDATES:
            if(display_cb) display_cb("No Update");
            devlog_printf("[OTA] No updates available.");
            break;
    }
}

#endif  /* ENABLE_OTA_UPDATES */
