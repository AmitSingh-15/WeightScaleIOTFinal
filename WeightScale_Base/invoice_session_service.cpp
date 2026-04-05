#include "invoice_session_service.h"
#include "invoice_service.h"

static invoice_item_t items[MAX_INVOICE_ITEMS];
static uint8_t item_count = 0;
static int g_selected_qty = 1;
static char summary_buffer[256] = {0};

void invoice_session_init(void)
{
    item_count = 0;
    g_selected_qty = 1;
}

/* ===== SELECTED QUANTITY MODEL ===== */

void invoice_session_set_selected_qty(int qty)
{
    if (qty < 1)    qty = 1;
    if (qty > 9999) qty = 9999;

    g_selected_qty = qty;
    Serial.printf("[INVOICE] Selected qty = %d\n", g_selected_qty);
}

int invoice_session_get_selected_qty(void)
{
    return g_selected_qty;
}

/* ===== SENSOR-DRIVEN ENTRY CREATION ===== */

void invoice_session_add_weight_entry(float weight)
{
    if (item_count >= MAX_INVOICE_ITEMS) return;
    if (weight < 0.05f) return;  /* Safety: ignore noise */

    items[item_count].weight = weight;
    items[item_count].qty    = g_selected_qty;

    Serial.printf("[INVOICE] Added -> W=%.2f Q=%d\n",
                  weight, g_selected_qty);

    item_count++;
}

void invoice_session_remove(uint8_t index)
{
    if (index >= item_count) return;

    for (uint8_t i = index; i < item_count - 1; i++)
        items[i] = items[i + 1];

    item_count--;
}

void invoice_session_clear(void)
{
    item_count = 0;
    g_selected_qty = 1;
}

const char* invoice_session_get_summary(void)
{
    if (item_count == 0) {
        snprintf(summary_buffer, sizeof(summary_buffer), "No items");
        return summary_buffer;
    }

    /* Build multi-line summary: "2.35 kg x 5\n1.80 kg x 3\n" */
    int offset = 0;
    for (uint8_t i = 0; i < item_count; i++) {
        int written = snprintf(summary_buffer + offset,
                               sizeof(summary_buffer) - offset,
                               "%.2f kg x %d\n",
                               items[i].weight,
                               items[i].qty);
        if (written > 0) offset += written;
        if (offset >= (int)sizeof(summary_buffer) - 1) break;
    }

    return summary_buffer;
}

uint8_t invoice_session_count(void)
{
    return item_count;
}

const invoice_item_t* invoice_session_get(uint8_t index)
{
    if (index >= item_count) return NULL;
    return &items[index];
}

void invoice_session_commit(void)
{
    Serial.printf("[INVOICE] Committing %d items\n", item_count);
    for (uint8_t i = 0; i < item_count; i++)
    {
        bool ok = invoice_service_save(
            items[i].weight,
            (uint16_t)items[i].qty,
            ENTRY_AUTO,
            NULL
        );
        Serial.printf("[INVOICE] Item %d: W=%.2f Q=%d -> %s\n",
                       i, items[i].weight, items[i].qty, ok ? "SAVED" : "FAILED");
    }

    invoice_session_clear();
}
