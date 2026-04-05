#pragma once
#include <stdint.h>


enum entry_type_t {
    ENTRY_AUTO = 0,
    ENTRY_MANUAL = 1
};

typedef struct {
    uint32_t invoice_id;
    float weight;
    uint16_t quantity;
    float total_weight;
    uint32_t timestamp;
    entry_type_t type;
    uint8_t synced; /* 0 = pending, 1 = synced */
} invoice_record_t;

void invoice_service_init(void);

uint32_t invoice_service_current_id(void);
void invoice_service_next(void);
void invoice_service_reset(void);

void invoice_service_daily_reset_if_needed(void);

bool invoice_service_save(float weight,
                          uint16_t quantity,
                          entry_type_t type,
                          invoice_record_t *out_record);

