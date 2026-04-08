#pragma once
#include <Arduino.h>
#include "invoice_service.h"   // needed for invoice_record_t
#include "scale_service_v2.h"  // needed for cal_profile_t

void storage_service_init(void);

/* Invoice counter */
void storage_save_invoice(uint32_t id);
void storage_load_invoice(uint32_t *id);

/* Day tracking */
void storage_save_last_day(uint32_t day);
uint32_t storage_load_last_day(void);

/* Offline queue */
bool storage_enqueue_record(const invoice_record_t *rec);

void storage_save_offset(float val);
float storage_load_offset(void);

void storage_add_full_record(const invoice_record_t *rec);
uint32_t storage_get_record_count(void);
uint8_t storage_get_last_records(invoice_record_t *out, uint8_t max);
void storage_check_new_day_and_reset(void);
void storage_clear_all_records(void);
uint32_t storage_get_pending_count(void);
void storage_set_pending(uint32_t count);
void storage_reset_pending(void);
void storage_save_dev_mode(bool enabled);
bool storage_load_dev_mode(void);
void storage_save_light_mode(bool on);
bool storage_load_light_mode(void);
bool storage_update_record(uint32_t index, const invoice_record_t *rec);

/* ===== DEVELOPER LOG STORAGE ===== */
void storage_save_devlog(const char *text);
String storage_load_devlog(void);
void storage_clear_devlog(void);

/* ===== DEVICE NAME STORAGE ===== */

void storage_save_device_name(const char *name);
bool storage_load_device_name(char *out, size_t max);

/* ===== DEVICE ID STORAGE ===== */

void storage_save_device_id(uint32_t id);
uint32_t storage_load_device_id(void);

bool storage_get_record_by_index(uint32_t index, invoice_record_t *out);

/* ===== WIFI CREDENTIAL STORAGE ===== */
void storage_save_wifi_credentials(const char *ssid, const char *password);
bool storage_load_wifi_credentials(char *ssid, size_t ssid_max, char *pwd, size_t pwd_max);
void storage_forget_wifi_credentials(void);

/* ===== CALIBRATION PROFILE STORAGE ===== */
void storage_save_cal_profile(int profile_index, const cal_profile_t *cp);
bool storage_load_cal_profile(int profile_index, cal_profile_t *cp);
void storage_save_active_cal_index(int index);
int  storage_load_active_cal_index(void);
