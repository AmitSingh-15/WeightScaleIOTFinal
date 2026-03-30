#pragma once
#include <Arduino.h>
#include "invoice_service.h"   // needed for invoice_record_t

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
void storage_reset_pending(void);
void storage_save_dev_mode(bool enabled);
bool storage_load_dev_mode(void);
bool storage_update_record(uint32_t index, const invoice_record_t *rec);

/* ===== DEVELOPER LOG STORAGE ===== */
void storage_save_devlog(const char *text);
String storage_load_devlog(void);
void storage_clear_devlog(void);

/* ===== DEVICE NAME STORAGE ===== */

void storage_save_device_name(const char *name);
bool storage_load_device_name(char *out, size_t max);

bool storage_get_record_by_index(uint32_t index, invoice_record_t *out);
