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

    /* ===== TABLE HEADER ROW ===== */
    lv_obj_t *tbl_hdr = lv_obj_create(parent);
    lv_obj_remove_style_all(tbl_hdr);
    lv_obj_set_size(tbl_hdr, DISPLAY_WIDTH - 40, 40);
    lv_obj_align(tbl_hdr, LV_ALIGN_TOP_MID, 0, 95);
    lv_obj_set_style_bg_color(tbl_hdr, lv_color_hex(0x164E63), 0);
    lv_obj_set_style_bg_opa(tbl_hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tbl_hdr, 8, 0);
    lv_obj_set_style_pad_left(tbl_hdr, 15, 0);
    lv_obj_set_style_pad_right(tbl_hdr, 15, 0);
    lv_obj_clear_flag(tbl_hdr, LV_OBJ_FLAG_SCROLLABLE);

    /* Column headers */
    const char *cols[] = {"#", "INV", "WEIGHT", "QTY", "TOTAL", "STATUS"};
    const int col_x[]  = {0,   60,    180,      420,   560,     780};

    for(int c = 0; c < 6; c++) {
        lv_obj_t *cl = lv_label_create(tbl_hdr);
        lv_obj_set_style_text_font(cl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(cl, lv_color_hex(0x67E8F9), 0);
        lv_label_set_text(cl, cols[c]);
        lv_obj_align(cl, LV_ALIGN_LEFT_MID, col_x[c], 0);
    }

    /* ===== SCROLLABLE LIST AREA ===== */
    scroll_cont = lv_obj_create(parent);
    lv_obj_set_size(scroll_cont, DISPLAY_WIDTH - 40, DISPLAY_HEIGHT - 145);
    lv_obj_align(scroll_cont, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_color(scroll_cont, COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(scroll_cont, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(scroll_cont, lv_color_hex(0x334155), 0);
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

/* Create a single row card for a record */
static void add_record_row(uint32_t idx, const invoice_record_t *rec)
{
    if(!scroll_cont || !lv_obj_is_valid(scroll_cont)) return;

    lv_obj_t *row = lv_obj_create(scroll_cont);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, lv_pct(100), 48);
    lv_obj_set_style_bg_color(row,
        (idx % 2 == 0) ? lv_color_hex(0x1E293B) : lv_color_hex(0x162032), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_pad_left(row, 15, 0);
    lv_obj_set_style_pad_right(row, 15, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    char buf[32];

    /* Row number */
    lv_obj_t *l_num = lv_label_create(row);
    lv_obj_set_style_text_font(l_num, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(l_num, COLOR_MUTED, 0);
    snprintf(buf, sizeof(buf), "%lu", idx + 1);
    lv_label_set_text(l_num, buf);
    lv_obj_align(l_num, LV_ALIGN_LEFT_MID, 0, 0);

    /* Invoice # */
    lv_obj_t *l_inv = lv_label_create(row);
    lv_obj_set_style_text_font(l_inv, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(l_inv, lv_color_hex(0x38BDF8), 0);
    snprintf(buf, sizeof(buf), "#%lu", rec->invoice_id);
    lv_label_set_text(l_inv, buf);
    lv_obj_align(l_inv, LV_ALIGN_LEFT_MID, 60, 0);

    /* Weight */
    lv_obj_t *l_wt = lv_label_create(row);
    lv_obj_set_style_text_font(l_wt, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(l_wt, COLOR_TEXT, 0);
    snprintf(buf, sizeof(buf), "%.3f kg", rec->weight);
    lv_label_set_text(l_wt, buf);
    lv_obj_align(l_wt, LV_ALIGN_LEFT_MID, 180, 0);

    /* Quantity */
    lv_obj_t *l_qty = lv_label_create(row);
    lv_obj_set_style_text_font(l_qty, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(l_qty, COLOR_TEXT, 0);
    snprintf(buf, sizeof(buf), "x%d", rec->quantity);
    lv_label_set_text(l_qty, buf);
    lv_obj_align(l_qty, LV_ALIGN_LEFT_MID, 420, 0);

    /* Total weight */
    lv_obj_t *l_total = lv_label_create(row);
    lv_obj_set_style_text_font(l_total, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(l_total, lv_color_hex(0x22D3EE), 0);
    snprintf(buf, sizeof(buf), "%.3f kg", rec->total_weight);
    lv_label_set_text(l_total, buf);
    lv_obj_align(l_total, LV_ALIGN_LEFT_MID, 560, 0);

    /* Sync status badge */
    lv_obj_t *badge = lv_obj_create(row);
    lv_obj_remove_style_all(badge);
    lv_obj_set_size(badge, 100, 30);
    lv_obj_align(badge, LV_ALIGN_LEFT_MID, 780, 0);
    lv_obj_set_style_radius(badge, 15, 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);

    if(rec->synced) {
        lv_obj_set_style_bg_color(badge, lv_color_hex(0x065F46), 0);
    } else {
        lv_obj_set_style_bg_color(badge, lv_color_hex(0x78350F), 0);
    }

    lv_obj_t *l_sync = lv_label_create(badge);
    lv_obj_set_style_text_font(l_sync, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l_sync,
        rec->synced ? COLOR_SUCCESS : COLOR_WARNING, 0);
    lv_label_set_text(l_sync, rec->synced ? LV_SYMBOL_OK " Synced" : LV_SYMBOL_REFRESH " Pending");
    lv_obj_center(l_sync);
}

void history_screen_refresh(void)
{
    if(!scroll_cont || !lv_obj_is_valid(scroll_cont)) return;

    lv_obj_clean(scroll_cont);

    uint32_t total = storage_get_record_count();

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

    /* Show newest first */
    invoice_record_t rec;
    for(int32_t i = (int32_t)total - 1; i >= 0; i--)
    {
        if(storage_get_record_by_index((uint32_t)i, &rec))
        {
            add_record_row((uint32_t)((int32_t)total - 1 - i), &rec);
        }
    }
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // LV_VERSION_MAJOR
