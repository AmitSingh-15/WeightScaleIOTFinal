#pragma once
#include <Arduino.h>

#define MAX_INVOICE_ITEMS 10

typedef struct {
    float weight;
    int   qty;
} invoice_item_t;

void invoice_session_init(void);

/* ===== SELECTED QUANTITY (pre-set before weight capture) ===== */
void invoice_session_set_selected_qty(int qty);
int  invoice_session_get_selected_qty(void);

/* ===== SENSOR-DRIVEN: weight entry creation ===== */
void invoice_session_add_weight_entry(float weight);

void invoice_session_remove(uint8_t index);
void invoice_session_clear(void);

/* Get summary string for UI display: "2.35 kg x 5\n1.80 kg x 3\n" */
const char* invoice_session_get_summary(void);

uint8_t invoice_session_count(void);
const invoice_item_t* invoice_session_get(uint8_t index);

void invoice_session_commit(void);
