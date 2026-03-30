#pragma once
#include <Arduino.h>

#define MAX_INVOICE_ITEMS 10

typedef struct {
    float weight;
    uint16_t quantity;
} invoice_item_t;

void invoice_session_init(void);

bool invoice_session_add(float weight, uint16_t qty);
void invoice_session_remove(uint8_t index);
void invoice_session_clear(void);

uint8_t invoice_session_count(void);
const invoice_item_t* invoice_session_get(uint8_t index);

void invoice_session_commit(void);
