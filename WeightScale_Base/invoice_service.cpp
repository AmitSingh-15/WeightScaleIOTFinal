#include "invoice_service.h"
#include "storage_service.h"
#include "time_service.h"

static uint32_t current_invoice = 1;

void invoice_service_init(void)
{
    storage_load_invoice(&current_invoice);
    invoice_service_daily_reset_if_needed();
}

uint32_t invoice_service_current_id(void)
{
    return current_invoice;
}

void invoice_service_next(void)
{
    current_invoice++;
    storage_save_invoice(current_invoice);
}

void invoice_service_daily_reset_if_needed(void)
{
    uint32_t last_day = storage_load_last_day();
    uint32_t today = time_service_today_epoch_day();

    if (last_day != today) {
        current_invoice = 1;
        storage_save_invoice(current_invoice);
        storage_save_last_day(today);
        storage_clear_all_records();
    }
}

bool invoice_service_save(float weight,
                          uint16_t quantity,
                          entry_type_t type,
                          invoice_record_t *out)
{
    if (weight <= 0 || quantity == 0) return false;

    invoice_record_t rec;
    rec.invoice_id   = current_invoice;
    rec.weight       = weight;
    rec.quantity     = quantity;
    rec.total_weight = weight * quantity;
    rec.timestamp    = time_service_now();
    rec.type         = type;
    rec.synced       = 0;

    if (!storage_enqueue_record(&rec)) {
        return false;
    }

    storage_add_full_record(&rec);

    if (out) *out = rec;

    return true;   // 🔥 DO NOT increment here
}
