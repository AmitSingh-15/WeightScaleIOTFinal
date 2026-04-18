#include "storage_service.h"
#include <Preferences.h>
#include "time_service.h"
#include "devlog.h"


static Preferences prefs;

void storage_service_init(void)
{
    prefs.begin("weights", false);
}

void storage_save_invoice(uint32_t id)
{
    prefs.putUInt("invoice_id", id);
}

void storage_load_invoice(uint32_t *id)
{
    *id = prefs.getUInt("invoice_id", 1);
}

void storage_save_last_day(uint32_t day)
{
    prefs.putUInt("last_day", day);
}

uint32_t storage_load_last_day(void)
{
    return prefs.getUInt("last_day", 0);
}

void storage_save_last_auto_clear_day(uint32_t day)
{
    prefs.putUInt("auto_clr_day", day);
}

uint32_t storage_load_last_auto_clear_day(void)
{
    return prefs.getUInt("auto_clr_day", 0);
}

/*
 * Offline queue hook
 * STEP-4 will attach Wi-Fi + sync
 */
bool storage_enqueue_record(const invoice_record_t *rec)
{
    // For now: persist minimal record count
    uint32_t count = prefs.getUInt("pending", 0);
    prefs.putUInt("pending", count + 1);

    // Real SPIFFS/JSON batching comes in Step-4
    return true;
}

void storage_save_offset(float val)
{
    prefs.putFloat("offset", val);
}

float storage_load_offset(void)
{
    return prefs.getFloat("offset", 0.0f);
}

void storage_add_full_record(const invoice_record_t *rec)
{
    uint32_t count = prefs.getUInt("rec_count", 0);

    char key[16];
    snprintf(key, sizeof(key), "rec_%lu", count);

    prefs.putBytes(key, rec, sizeof(invoice_record_t));
    prefs.putUInt("rec_count", count + 1);
}

uint8_t storage_get_last_records(invoice_record_t *out, uint8_t max)
{
    uint32_t count = storage_get_record_count();
    if (count == 0) return 0;

    uint8_t loaded = 0;
    int32_t start = count - 1;

    for (int32_t i = start; i >= 0 && loaded < max; i--)
    {
        char key[16];
        snprintf(key, sizeof(key), "rec_%ld", i);

        prefs.getBytes(key, &out[loaded], sizeof(invoice_record_t));
        loaded++;
    }

    return loaded;
}

void storage_clear_all_records(void)
{
    uint32_t count = storage_get_record_count();

    for (uint32_t i = 0; i < count; i++)
    {
        char key[16];
        snprintf(key, sizeof(key), "rec_%lu", i);
        prefs.remove(key);
    }

    prefs.putUInt("rec_count", 0);
}

void storage_check_new_day_and_reset(void)
{
    uint32_t last_day = prefs.getUInt("day_epoch", 0);
    uint32_t today = time_service_today_epoch_day();

    if (last_day != today)
    {
        storage_clear_all_records();
        prefs.putUInt("day_epoch", today);
    }
}

uint32_t storage_get_record_count(void)
{
    return prefs.getUInt("rec_count", 0);
}

uint32_t storage_get_pending_count(void)
{
    return prefs.getUInt("pending", 0);
}

void storage_set_pending(uint32_t count)
{
    prefs.putUInt("pending", count);
}

void storage_reset_pending(void)
{
    prefs.putUInt("pending", 0);
}

bool storage_update_record(uint32_t index, const invoice_record_t *rec)
{
    uint32_t count = storage_get_record_count();
    if(index >= count) return false;

    char key[16];
    snprintf(key, sizeof(key), "rec_%lu", index);
    prefs.putBytes(key, rec, sizeof(invoice_record_t));
    return true;
}

void storage_save_dev_mode(bool enabled)
{
    prefs.putBool("dev_mode", enabled);
}

bool storage_load_dev_mode(void)
{
    return prefs.getBool("dev_mode", false);
}

void storage_save_light_mode(bool on)
{
    prefs.putBool("light_mode", on);
}

bool storage_load_light_mode(void)
{
    return prefs.getBool("light_mode", false);
}

/* =========================================================
   DEVELOPER LOG STORAGE (persistent across reboots)
=========================================================*/

void storage_save_devlog(const char *text)
{
    if(!text) return;
    prefs.putString("devlog", text);
}

String storage_load_devlog(void)
{
    return prefs.getString("devlog", "");
}

void storage_clear_devlog(void)
{
    prefs.remove("devlog");
}

/* =========================================================
   DEVICE NAME STORAGE
=========================================================*/

void storage_save_device_name(const char *name)
{
    if(!name) return;

    prefs.putString("dev_name", name);
}

bool storage_load_device_name(char *out, size_t max)
{
    if(!out || max == 0) return false;

    String n = prefs.getString("dev_name", "");

    if(n.length() == 0)
        return false;

    strncpy(out, n.c_str(), max);
    out[max-1] = 0;

    return true;
}

/* =========================================================
   DEVICE ID STORAGE
=========================================================*/

void storage_save_device_id(uint32_t id)
{
    prefs.putUInt("dev_id", id);
}

uint32_t storage_load_device_id(void)
{
    return prefs.getUInt("dev_id", 0);
}

bool storage_get_record_by_index(uint32_t index, invoice_record_t *out)
{
    if(!out) return false;

    uint32_t count = storage_get_record_count();
    if(index >= count) return false;

    char key[16];
    snprintf(key, sizeof(key), "rec_%lu", index);

    size_t read = prefs.getBytes(key, out, sizeof(invoice_record_t));

    return read == sizeof(invoice_record_t);
}

/* =========================================================
   WIFI CREDENTIAL STORAGE
=========================================================*/

/* =========================================================
   MULTI-NETWORK WIFI CREDENTIAL STORAGE
   Stores up to WIFI_MAX_SAVED networks in NVS.
   Keys: wifi_cnt (uint8), wifi_s0..wifi_s4 (SSID), wifi_p0..wifi_p4 (pwd)
   The "primary" (index 0) is the most recently connected.
   Legacy keys wifi_ssid/wifi_pwd are migrated on first load.
========================================================= */

static bool wifi_legacy_migrated = false;

static void wifi_migrate_legacy(void)
{
    if(wifi_legacy_migrated) return;
    wifi_legacy_migrated = true;

    /* Already have new-format data? skip */
    if(prefs.isKey("wifi_cnt")) return;

    /* Migrate old single-credential keys */
    String old_ssid = prefs.getString("wifi_ssid", "");
    if(old_ssid.length() == 0) return;

    String old_pwd = prefs.getString("wifi_pwd", "");
    prefs.putUChar("wifi_cnt", 1);
    prefs.putString("wifi_s0", old_ssid);
    prefs.putString("wifi_p0", old_pwd);
    prefs.remove("wifi_ssid");
    prefs.remove("wifi_pwd");
}

uint8_t storage_get_wifi_count(void)
{
    wifi_migrate_legacy();
    return prefs.getUChar("wifi_cnt", 0);
}

bool storage_get_wifi_at(uint8_t index, char *ssid, size_t ssid_max, char *pwd, size_t pwd_max)
{
    wifi_migrate_legacy();
    uint8_t cnt = prefs.getUChar("wifi_cnt", 0);
    if(index >= cnt) return false;

    char key_s[10], key_p[10];
    snprintf(key_s, sizeof(key_s), "wifi_s%u", index);
    snprintf(key_p, sizeof(key_p), "wifi_p%u", index);

    String s = prefs.getString(key_s, "");
    if(s.length() == 0) return false;
    strncpy(ssid, s.c_str(), ssid_max);
    ssid[ssid_max - 1] = 0;

    String p = prefs.getString(key_p, "");
    strncpy(pwd, p.c_str(), pwd_max);
    pwd[pwd_max - 1] = 0;
    return true;
}

bool storage_find_wifi_password(const char *ssid, char *pwd, size_t pwd_max)
{
    if(!ssid || !ssid[0]) return false;
    wifi_migrate_legacy();
    uint8_t cnt = prefs.getUChar("wifi_cnt", 0);
    for(uint8_t i = 0; i < cnt; i++) {
        char key_s[10], key_p[10];
        snprintf(key_s, sizeof(key_s), "wifi_s%u", i);
        String s = prefs.getString(key_s, "");
        if(s == ssid) {
            snprintf(key_p, sizeof(key_p), "wifi_p%u", i);
            String p = prefs.getString(key_p, "");
            strncpy(pwd, p.c_str(), pwd_max);
            pwd[pwd_max - 1] = 0;
            return true;
        }
    }
    return false;
}

void storage_save_wifi_credentials(const char *ssid, const char *password)
{
    if(!ssid || !ssid[0]) return;
    wifi_migrate_legacy();
    uint8_t cnt = prefs.getUChar("wifi_cnt", 0);

    /* Check if SSID already saved — update password & promote to index 0 */
    int existing = -1;
    for(uint8_t i = 0; i < cnt; i++) {
        char key_s[10];
        snprintf(key_s, sizeof(key_s), "wifi_s%u", i);
        String s = prefs.getString(key_s, "");
        if(s == ssid) { existing = i; break; }
    }

    if(existing == 0) {
        /* Already at top — just update password */
        prefs.putString("wifi_p0", password ? password : "");
        return;
    }

    /* Shift entries down to make room at index 0 */
    int start = (existing >= 0) ? existing : cnt;
    if(start >= WIFI_MAX_SAVED) start = WIFI_MAX_SAVED - 1;

    for(int i = start; i > 0; i--) {
        char src_s[10], src_p[10], dst_s[10], dst_p[10];
        snprintf(src_s, sizeof(src_s), "wifi_s%u", i - 1);
        snprintf(src_p, sizeof(src_p), "wifi_p%u", i - 1);
        snprintf(dst_s, sizeof(dst_s), "wifi_s%u", i);
        snprintf(dst_p, sizeof(dst_p), "wifi_p%u", i);
        prefs.putString(dst_s, prefs.getString(src_s, ""));
        prefs.putString(dst_p, prefs.getString(src_p, ""));
    }

    /* Write new entry at index 0 */
    prefs.putString("wifi_s0", ssid);
    prefs.putString("wifi_p0", password ? password : "");

    /* Update count */
    if(existing < 0) {
        cnt++;
        if(cnt > WIFI_MAX_SAVED) cnt = WIFI_MAX_SAVED;
    }
    prefs.putUChar("wifi_cnt", cnt);
}

bool storage_load_wifi_credentials(char *ssid, size_t ssid_max, char *pwd, size_t pwd_max)
{
    /* Returns the primary (index 0) credential — backward compatible */
    return storage_get_wifi_at(0, ssid, ssid_max, pwd, pwd_max);
}

void storage_forget_wifi_credentials(void)
{
    /* Forget ALL saved networks */
    wifi_migrate_legacy();
    uint8_t cnt = prefs.getUChar("wifi_cnt", 0);
    for(uint8_t i = 0; i < cnt; i++) {
        char key_s[10], key_p[10];
        snprintf(key_s, sizeof(key_s), "wifi_s%u", i);
        snprintf(key_p, sizeof(key_p), "wifi_p%u", i);
        prefs.remove(key_s);
        prefs.remove(key_p);
    }
    prefs.putUChar("wifi_cnt", 0);
}

void storage_forget_wifi_ssid(const char *ssid)
{
    if(!ssid || !ssid[0]) return;
    wifi_migrate_legacy();
    uint8_t cnt = prefs.getUChar("wifi_cnt", 0);

    /* Find the SSID */
    int found = -1;
    for(uint8_t i = 0; i < cnt; i++) {
        char key_s[10];
        snprintf(key_s, sizeof(key_s), "wifi_s%u", i);
        String s = prefs.getString(key_s, "");
        if(s == ssid) { found = i; break; }
    }
    if(found < 0) return;

    /* Shift remaining entries up */
    for(int i = found; i < cnt - 1; i++) {
        char src_s[10], src_p[10], dst_s[10], dst_p[10];
        snprintf(src_s, sizeof(src_s), "wifi_s%u", i + 1);
        snprintf(src_p, sizeof(src_p), "wifi_p%u", i + 1);
        snprintf(dst_s, sizeof(dst_s), "wifi_s%u", i);
        snprintf(dst_p, sizeof(dst_p), "wifi_p%u", i);
        prefs.putString(dst_s, prefs.getString(src_s, ""));
        prefs.putString(dst_p, prefs.getString(src_p, ""));
    }

    /* Remove last entry and decrement count */
    char last_s[10], last_p[10];
    snprintf(last_s, sizeof(last_s), "wifi_s%u", cnt - 1);
    snprintf(last_p, sizeof(last_p), "wifi_p%u", cnt - 1);
    prefs.remove(last_s);
    prefs.remove(last_p);
    prefs.putUChar("wifi_cnt", cnt - 1);
}

/* =========================================================
   ENVIRONMENT (Dev/Prod) STORAGE
=========================================================*/

void storage_save_env_prod(bool is_prod)
{
    prefs.putBool("env_prod", is_prod);
}

bool storage_load_env_prod(void)
{
    return prefs.getBool("env_prod", false);  /* default = Dev */
}

/* =========================================================
   CALIBRATION PROFILE STORAGE
   Supports up to 4 profiles (index 0–3)
=========================================================*/

void storage_save_cal_profile(int profile_index, const cal_profile_t *cp)
{
    if(!cp || profile_index < 0 || profile_index > 3) return;

    char key[16];
    snprintf(key, sizeof(key), "cal_%d", profile_index);
    prefs.putBytes(key, cp, sizeof(cal_profile_t));
    devlog_printf("[STOR] Cal profile %d saved: %s", profile_index, cp->name);
}

bool storage_load_cal_profile(int profile_index, cal_profile_t *cp)
{
    if(!cp || profile_index < 0 || profile_index > 3) return false;

    char key[16];
    snprintf(key, sizeof(key), "cal_%d", profile_index);
    size_t read = prefs.getBytes(key, cp, sizeof(cal_profile_t));
    return (read == sizeof(cal_profile_t) && cp->valid);
}

void storage_save_active_cal_index(int index)
{
    prefs.putInt("cal_idx", index);
}

int storage_load_active_cal_index(void)
{
    return prefs.getInt("cal_idx", -1);  /* -1 = no active profile */
}
