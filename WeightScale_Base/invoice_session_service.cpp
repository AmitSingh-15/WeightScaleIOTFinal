#include "invoice_session_service.h"
#include "invoice_service.h"

static invoice_item_t items[MAX_INVOICE_ITEMS];
static uint8_t item_count = 0;

void invoice_session_init(void)
{
    item_count = 0;
}

bool invoice_session_add(float weight, uint16_t qty)
{
    if(weight <= 0.001f) return false;
    if(qty == 0) return false;
    if(item_count >= MAX_INVOICE_ITEMS) return false;

    items[item_count].weight = weight;
    items[item_count].quantity = qty;
    item_count++;
    return true;
}

void invoice_session_remove(uint8_t index)
{
    if(index >= item_count) return;

    for(uint8_t i=index;i<item_count-1;i++)
        items[i] = items[i+1];

    item_count--;
}

void invoice_session_clear(void)
{
    item_count = 0;
}

uint8_t invoice_session_count(void)
{
    return item_count;
}

const invoice_item_t* invoice_session_get(uint8_t index)
{
    if(index >= item_count) return NULL;
    return &items[index];
}

void invoice_session_commit(void)
{
    for(uint8_t i=0;i<item_count;i++)
    {
        invoice_service_save(
            items[i].weight,
            items[i].quantity,
            ENTRY_MANUAL,
            NULL
        );
    }

    invoice_session_clear();
}
