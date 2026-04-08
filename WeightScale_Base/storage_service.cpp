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

void storage_save_wifi_credentials(const char *ssid, const char *password)
{
    if(!ssid || !ssid[0]) return;
    prefs.putString("wifi_ssid", ssid);
    prefs.putString("wifi_pwd", password ? password : "");
}

bool storage_load_wifi_credentials(char *ssid, size_t ssid_max, char *pwd, size_t pwd_max)
{
    if(!ssid || !pwd) return false;

    String s = prefs.getString("wifi_ssid", "");
    if(s.length() == 0) return false;

    strncpy(ssid, s.c_str(), ssid_max);
    ssid[ssid_max - 1] = 0;

    String p = prefs.getString("wifi_pwd", "");
    strncpy(pwd, p.c_str(), pwd_max);
    pwd[pwd_max - 1] = 0;

    return true;
}

void storage_forget_wifi_credentials(void)
{
    prefs.remove("wifi_ssid");
    prefs.remove("wifi_pwd");
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
