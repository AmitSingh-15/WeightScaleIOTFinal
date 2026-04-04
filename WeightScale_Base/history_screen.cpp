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

static lv_obj_t *list;
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
    lv_obj_add_style(parent,&g_styles.screen,0);
    lv_obj_set_size(parent, DISPLAY_WIDTH, DISPLAY_HEIGHT);

    /* HEADER */
    lv_obj_t *header = lv_obj_create(parent);
    lv_obj_add_style(header,&g_styles.card,0);
    lv_obj_set_size(header, DISPLAY_WIDTH, 90);
    lv_obj_align(header,LV_ALIGN_TOP_MID,0,0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(header);
    lv_obj_add_style(title, &g_styles.title, 0);
    lv_label_set_text(title, LV_SYMBOL_LIST "  INVOICE HISTORY");
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t *back = lv_btn_create(header);
    lv_obj_add_style(back,&g_styles.btn_secondary,0);
    lv_obj_set_size(back, 160, 65);
    lv_obj_align(back,LV_ALIGN_RIGHT_MID,-10,0);
    lv_obj_add_event_cb(back,back_event,LV_EVENT_RELEASED,NULL);
    lv_label_set_text(lv_label_create(back), LV_SYMBOL_LEFT " BACK");

    /* LIST */
    list = lv_list_create(parent);
    lv_obj_set_size(list, DISPLAY_WIDTH - 40, DISPLAY_HEIGHT - 105);
    lv_obj_align(list,LV_ALIGN_BOTTOM_MID,0,-5);
    lv_obj_set_style_bg_color(list, COLOR_CARD, 0);
    lv_obj_set_style_border_color(list, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(list, 1, 0);
    lv_obj_set_style_radius(list, 12, 0);
    lv_obj_set_style_text_font(list, &lv_font_montserrat_20, 0);
}

void history_screen_refresh(void)
{
    lv_obj_clean(list);

    uint32_t total = storage_get_record_count();

    if(total == 0)
    {
        lv_list_add_text(list,"No records found");
        return;
    }

    invoice_record_t rec;

    for(uint32_t i = 0; i < total; i++)
    {
        if(storage_get_record_by_index(i, &rec))
        {
            char buf[160];
            const char *sync_txt = rec.synced ? "Synced" : "Pending";
            snprintf(buf,sizeof(buf),
                     "Inv#%lu | %.3f kg x%d | Total %.3f kg [%s]",
                     rec.invoice_id,
                     rec.weight,
                     rec.quantity,
                     rec.total_weight,
                     sync_txt);

            lv_list_add_text(list, buf);
        }
    }
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // LV_VERSION_MAJOR
