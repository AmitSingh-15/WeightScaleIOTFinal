#include <lvgl.h>

// ✅ Ensure LVGL is detected
#ifndef LV_VERSION_MAJOR
#define LV_VERSION_MAJOR 8
#endif

#include "config/app_config.h"
#include "history_screen.h"
#include "ui_styles.h"
#include "storage_service.h"
#include "invoice_service.h"

#ifdef LV_VERSION_MAJOR

#ifdef __cplusplus
extern "C" {
#endif

static lv_obj_t *scroll_cont = NULL;
static lv_obj_t *count_label = NULL;
static void (*back_callback)(void) = NULL;

static void back_event(lv_event_t *e)
{
    if(back_callback) back_callback();
}

void history_screen_register_back(void (*cb)(void))
{
    back_callback = cb;
}

void history_screen_create(lv_obj_t *parent)
{
    ui_styles_init();
    lv_obj_add_style(parent, &g_styles.screen, 0);
    lv_obj_set_size(parent, DISPLAY_WIDTH, DISPLAY_HEIGHT);

    /* ===== HEADER ===== */
    lv_obj_t *header = lv_obj_create(parent);
    lv_obj_add_style(header, &g_styles.card, 0);
    lv_obj_set_size(header, DISPLAY_WIDTH, 90);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(header);
    lv_obj_add_style(title, &g_styles.title, 0);
    lv_label_set_text(title, LV_SYMBOL_LIST "  INVOICE HISTORY");
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);

    count_label = lv_label_create(header);
    lv_obj_set_style_text_font(count_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(count_label, COLOR_MUTED, 0);
    lv_label_set_text(count_label, "0 records");
    lv_obj_align(count_label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *back = lv_btn_create(header);
    lv_obj_add_style(back, &g_styles.btn_secondary, 0);
    lv_obj_set_size(back, 160, 65);
    lv_obj_align(back, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_add_event_cb(back, back_event, LV_EVENT_RELEASED, NULL);
    lv_label_set_text(lv_label_create(back), LV_SYMBOL_LEFT " BACK");

    /* ===== SCROLLABLE LIST AREA ===== */
    scroll_cont = lv_obj_create(parent);
    lv_obj_set_size(scroll_cont, DISPLAY_WIDTH - 40, DISPLAY_HEIGHT - 100);
    lv_obj_align(scroll_cont, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_color(scroll_cont, ui_theme_card(), 0);
    lv_obj_set_style_bg_opa(scroll_cont, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(scroll_cont, ui_theme_border(), 0);
    lv_obj_set_style_border_width(scroll_cont, 1, 0);
    lv_obj_set_style_radius(scroll_cont, 0, 0);
    lv_obj_set_style_pad_all(scroll_cont, 5, 0);
    lv_obj_set_style_pad_row(scroll_cont, 2, 0);
    lv_obj_set_flex_flow(scroll_cont, LV_FLEX_FLOW_COLUMN);

    /* Null cleanup on delete */
    lv_obj_add_event_cb(parent, [](lv_event_t *e) {
        scroll_cont = NULL;
        count_label = NULL;
    }, LV_EVENT_DELETE, NULL);
}

/* Create a single row — simplified to reduce LVGL object count */
static void add_record_row(uint32_t idx, const invoice_record_t *rec, float inv_total)
{
    if(!scroll_cont || !lv_obj_is_valid(scroll_cont)) return;

    lv_obj_t *row = lv_obj_create(scroll_cont);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, lv_pct(100), 40);
    lv_obj_set_style_bg_color(row,
        (idx % 2 == 0) ? ui_theme_row_even() : ui_theme_row_odd(), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, 4, 0);
    lv_obj_set_style_pad_left(row, 10, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    /* Single label: idx | INV# | unit weight x qty = item weight | invoice total | sync */
    char buf[160];
    snprintf(buf, sizeof(buf),
             "%lu. INV#%lu  %.2fkg x%d = %.2fkg  | Inv: %.2fkg  %s",
             idx + 1,
             rec->invoice_id,
             rec->weight,
             rec->quantity,
             rec->total_weight,
             inv_total,
             rec->synced ? LV_SYMBOL_OK : LV_SYMBOL_REFRESH);

    lv_obj_t *lbl = lv_label_create(row);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl, ui_theme_text(), 0);
    lv_label_set_text(lbl, buf);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
}

void history_screen_refresh(void)
{
    Serial.println("[HIST] refresh() called");
    Serial.flush();
    if(!scroll_cont || !lv_obj_is_valid(scroll_cont)) {
        Serial.println("[HIST] scroll_cont invalid, aborting");
        return;
    }

    lv_obj_clean(scroll_cont);

    uint32_t total = storage_get_record_count();
    Serial.printf("[HIST] record count = %lu\n", total);
    Serial.flush();

    /* Update count label */
    if(count_label && lv_obj_is_valid(count_label)) {
        char cbuf[32];
        snprintf(cbuf, sizeof(cbuf), "%lu records", total);
        lv_label_set_text(count_label, cbuf);
    }

    if(total == 0)
    {
        lv_obj_t *empty = lv_label_create(scroll_cont);
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(empty, COLOR_MUTED, 0);
        lv_label_set_text(empty, "No records found");
        lv_obj_set_width(empty, lv_pct(100));
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_top(empty, 60, 0);
        return;
    }

    /* Cap at 15 most recent to prevent OOM crash */
    uint32_t start = (total > 15) ? (total - 15) : 0;
    uint32_t visible = total - start;

    /* First pass: read visible records and compute per-invoice totals */
    invoice_record_t recs[15];
    float inv_totals[15];
    uint32_t rec_indices[15];
    uint32_t loaded = 0;

    for(int32_t i = (int32_t)total - 1; i >= (int32_t)start && loaded < 15; i--)
    {
        if(storage_get_record_by_index((uint32_t)i, &recs[loaded]))
        {
            rec_indices[loaded] = (uint32_t)i;
            loaded++;
        }
    }

    /* Compute total weight per invoice_id */
    for(uint32_t i = 0; i < loaded; i++)
    {
        float sum = 0.0f;
        uint32_t inv_id = recs[i].invoice_id;
        for(uint32_t j = 0; j < loaded; j++)
        {
            if(recs[j].invoice_id == inv_id)
                sum += recs[j].total_weight;
        }
        inv_totals[i] = sum;
    }

    /* Show newest first — yield to LVGL between rows to prevent UI freeze */
    for(uint32_t i = 0; i < loaded; i++)
    {
        add_record_row(i, &recs[i], inv_totals[i]);
        if((i + 1) % 5 == 0) lv_task_handler();
    }
    Serial.printf("[HIST] refresh done, showed %lu rows\n", loaded);
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // LV_VERSION_MAJOR
