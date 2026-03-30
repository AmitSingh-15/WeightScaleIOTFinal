#include <lvgl.h>
#include "history_screen.h"
#include "ui_styles.h"
#include "storage_service.h"
#include "invoice_service.h"

#ifdef LV_VERSION_MAJOR

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
    lv_obj_set_size(parent,800,480);

    /* HEADER */
    lv_obj_t *header = lv_obj_create(parent);
    lv_obj_add_style(header,&g_styles.card,0);
    lv_obj_set_size(header,800,80);
    lv_obj_align(header,LV_ALIGN_TOP_MID,0,0);

    lv_label_set_text(lv_label_create(header),"INVOICE HISTORY");

    lv_obj_t *back = lv_btn_create(header);
    lv_obj_add_style(back,&g_styles.btn_secondary,0);
    lv_obj_align(back,LV_ALIGN_RIGHT_MID,-15,0);
    lv_obj_add_event_cb(back,back_event,LV_EVENT_CLICKED,NULL);
    lv_label_set_text(lv_label_create(back),"BACK");

    /* LIST */
    list = lv_list_create(parent);
    lv_obj_set_size(list,760,360);
    lv_obj_align(list,LV_ALIGN_BOTTOM_MID,0,-10);
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

#else
/* Stub implementations when LVGL is unavailable */
void history_screen_create(void *parent) { }
void history_screen_refresh(void) { }
void history_screen_register_back(void (*cb)(void)) { }
#endif  // LV_VERSION_MAJOR
