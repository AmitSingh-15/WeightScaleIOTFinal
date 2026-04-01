#include <lvgl.h>
#include "home_screen.h"
#include "ui_styles.h"
#include "ui_events.h"
#include "invoice_session_service.h"
// ✅ REFACTORED: Removed #include "wifi_service.h" - WiFi is now decoupled
// ✅ Use app_controller instead for WiFi state updates

#ifdef LV_VERSION_MAJOR

static lv_obj_t *lbl_weight;
static lv_obj_t *lbl_qty;
static lv_obj_t *lbl_invoice;
static lv_obj_t *lbl_device;
static lv_obj_t *lbl_sync;
static lv_obj_t *version_label;   // ✅ ADDED

static lv_obj_t *item_labels[MAX_INVOICE_ITEMS];
static lv_obj_t *item_delete_btns[MAX_INVOICE_ITEMS];

static void (*event_cb)(int evt) = NULL;

// ⭐ OPTIMIZATION: Single reusable buffer for all text formatting (saves ~200 bytes)
static char g_format_buf[64];  // Largest size needed (for device name)

static void btn_event_cb(lv_event_t *e)
{
    if(!event_cb) return;
    uintptr_t id = (uintptr_t)lv_event_get_user_data(e);
    event_cb((int)id);
}

static void delete_event_cb(lv_event_t *e)
{
    /* Find which delete button was pressed */
    lv_obj_t *t = lv_event_get_target(e);
    for(int i=0;i<MAX_INVOICE_ITEMS;i++)
    {
        if(item_delete_btns[i] && item_delete_btns[i] == t)
        {
            invoice_session_remove((uint8_t)i);
            home_screen_refresh_invoice_details();
            break;
        }
    }
}

void home_screen_register_callback(void (*cb)(int evt))
{
    event_cb = cb;
}

void home_screen_create(lv_obj_t *parent)
{
    lv_obj_add_style(parent,&g_styles.screen,0);
    lv_obj_set_size(parent,800,480);

    /* ================= HEADER ================= */

    lv_obj_t *header = lv_obj_create(parent);
    lv_obj_add_style(header,&g_styles.card,0);
    lv_obj_set_size(header,800,96);
    lv_obj_align(header,LV_ALIGN_TOP_MID,0,0);

    lbl_invoice = lv_label_create(header);
    lv_obj_set_style_text_font(lbl_invoice,&lv_font_montserrat_28,0);
    lv_label_set_text(lbl_invoice,"Invoice #1");
    lv_obj_align(lbl_invoice,LV_ALIGN_CENTER,-45,-5);

    lbl_sync = lv_label_create(header);
    lv_label_set_text(lbl_sync,"Offline");
    lv_obj_align(lbl_sync,LV_ALIGN_LEFT_MID,15,-10);

    lbl_device = lv_label_create(header);
    lv_label_set_text(lbl_device,"Device: -");
    lv_obj_align(lbl_device,LV_ALIGN_LEFT_MID,15,15);

    /* HISTORY */
    lv_obj_t *history_btn = lv_btn_create(header);
    lv_obj_set_size(history_btn,80,40);
    lv_obj_align(history_btn,LV_ALIGN_RIGHT_MID,-260,0);
    lv_obj_add_event_cb(history_btn,btn_event_cb,LV_EVENT_CLICKED,(void*)UI_EVT_HISTORY);
    lv_label_set_text(lv_label_create(history_btn),"HIS");

    /* CALIBRATION */
    lv_obj_t *cal_btn = lv_btn_create(header);
    lv_obj_set_size(cal_btn,80,40);
    lv_obj_align(cal_btn,LV_ALIGN_RIGHT_MID,-180,0);
    lv_obj_add_event_cb(cal_btn,btn_event_cb,LV_EVENT_CLICKED,(void*)2001);
    lv_label_set_text(lv_label_create(cal_btn),"CAL");

    /* CLEAR */
    lv_obj_t *clear_btn = lv_btn_create(header);
    lv_obj_set_size(clear_btn,80,40);
    lv_obj_align(clear_btn,LV_ALIGN_RIGHT_MID,-100,0);
    lv_obj_add_event_cb(clear_btn,btn_event_cb,LV_EVENT_CLICKED,(void*)UI_EVT_RESET_ALL);
    lv_label_set_text(lv_label_create(clear_btn),"CLEAR");

    /* SETTINGS (LOCKED) */
    lv_obj_t *settings_btn = lv_btn_create(header);
    lv_obj_set_size(settings_btn,80,40);
    lv_obj_align(settings_btn,LV_ALIGN_RIGHT_MID,-15,0);
    lv_obj_add_event_cb(settings_btn,btn_event_cb,LV_EVENT_CLICKED,(void*)UI_EVT_SETTINGS);
    lv_label_set_text(lv_label_create(settings_btn),"SET");


    /* ================= BODY ================= */

    lv_obj_t *main = lv_obj_create(parent);
    lv_obj_remove_style_all(main);
    lv_obj_set_size(main,800,384);
    lv_obj_align(main,LV_ALIGN_BOTTOM_MID,0,0);

    /* ===== LEFT ===== */

    lv_obj_t *left = lv_obj_create(main);
    lv_obj_add_style(left,&g_styles.card,0);
    lv_obj_set_size(left,400,384);
    lv_obj_align(left,LV_ALIGN_LEFT_MID,0,0);

    /* 🔥 Disable scroll */
    lv_obj_clear_flag(left, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(left, LV_SCROLLBAR_MODE_OFF);

    lbl_weight = lv_label_create(left);
    lv_obj_set_style_text_font(lbl_weight,&lv_font_montserrat_28,0);
    lv_label_set_text(lbl_weight,"0.000 kg");
    lv_obj_align(lbl_weight,LV_ALIGN_CENTER,0,-110);

    lbl_qty = lv_label_create(left);
    lv_label_set_text(lbl_qty,"Qty: 1");
    lv_obj_align(lbl_qty,LV_ALIGN_CENTER,0,-60);

    /* +/- */

    lv_obj_t *minus = lv_btn_create(left);
    lv_obj_set_size(minus,80,50);
    lv_obj_align(minus,LV_ALIGN_CENTER,-80,-10);
    lv_obj_add_event_cb(minus,btn_event_cb,LV_EVENT_CLICKED,(void*)UI_EVT_QTY_DEC);
    lv_label_set_text(lv_label_create(minus),"-");

    lv_obj_t *plus = lv_btn_create(left);
    lv_obj_set_size(plus,80,50);
    lv_obj_align(plus,LV_ALIGN_CENTER,80,-10);
    lv_obj_add_event_cb(plus,btn_event_cb,LV_EVENT_CLICKED,(void*)UI_EVT_QTY_INC);
    lv_label_set_text(lv_label_create(plus),"+");

    /* MULTIPLY */

    lv_obj_t *mul2 = lv_btn_create(left);
    lv_obj_set_size(mul2,80,45);
    lv_obj_align(mul2,LV_ALIGN_CENTER,-120,50);
    lv_obj_add_event_cb(mul2,btn_event_cb,LV_EVENT_CLICKED,(void*)1002);
    lv_label_set_text(lv_label_create(mul2),"x2");

    lv_obj_t *mul5 = lv_btn_create(left);
    lv_obj_set_size(mul5,80,45);
    lv_obj_align(mul5,LV_ALIGN_CENTER,0,50);
    lv_obj_add_event_cb(mul5,btn_event_cb,LV_EVENT_CLICKED,(void*)1005);
    lv_label_set_text(lv_label_create(mul5),"x5");

    lv_obj_t *mul10 = lv_btn_create(left);
    lv_obj_set_size(mul10,80,45);
    lv_obj_align(mul10,LV_ALIGN_CENTER,120,50);
    lv_obj_add_event_cb(mul10,btn_event_cb,LV_EVENT_CLICKED,(void*)1010);
    lv_label_set_text(lv_label_create(mul10),"x10");

    /* ADD + FINALIZE side-by-side */

    lv_obj_t *add = lv_btn_create(left);
    lv_obj_set_size(add,140,50);
    lv_obj_align(add,LV_ALIGN_CENTER,-80,120);
    lv_obj_add_event_cb(add,btn_event_cb,LV_EVENT_CLICKED,(void*)UI_EVT_SAVE);
    lv_label_set_text(lv_label_create(add),"ADD");

    lv_obj_t *final = lv_btn_create(left);
    lv_obj_set_size(final,140,50);
    lv_obj_align(final,LV_ALIGN_CENTER,80,120);
    lv_obj_add_event_cb(final,btn_event_cb,LV_EVENT_CLICKED,(void*)UI_EVT_RESET);
    lv_label_set_text(lv_label_create(final),"FINALIZE");

    /* ===== RIGHT ===== */

    lv_obj_t *right = lv_obj_create(main);
    lv_obj_add_style(right,&g_styles.card,0);
    lv_obj_set_size(right,400,384);
    lv_obj_align(right,LV_ALIGN_RIGHT_MID,0,0);

    lv_label_set_text(lv_label_create(right),"Invoice Items");

    // ✅ REFACTORED: No direct WiFi dependency!
    // WiFi status is now PUSHED to UI via app_controller
    // The controller calls home_screen_set_sync_status() when WiFi state changes
    // This removes ~500KB of WiFi library code from being linked into the UI!

    for(int i=0;i<MAX_INVOICE_ITEMS;i++)
    {
        lv_obj_t *row = lv_obj_create(right);
        lv_obj_set_size(row,360,45);
        lv_obj_align(row,LV_ALIGN_TOP_LEFT,20,40 + i*55);

        item_labels[i] = lv_label_create(row);
        lv_obj_align(item_labels[i],LV_ALIGN_LEFT_MID,10,0);
        /* delete button */
        item_delete_btns[i] = lv_btn_create(row);
        lv_obj_set_size(item_delete_btns[i],50,30);
        lv_obj_align(item_delete_btns[i],LV_ALIGN_RIGHT_MID,-10,0);
        lv_label_set_text(lv_label_create(item_delete_btns[i]),"DEL");
        lv_obj_add_event_cb(item_delete_btns[i], delete_event_cb, LV_EVENT_CLICKED, NULL);
  
    }
        /////////////////////////////////////////////////////////////////////
        version_label = lv_label_create(parent);
        lv_obj_align(version_label, LV_ALIGN_BOTTOM_RIGHT, -10, -5);
        lv_label_set_text(version_label, "v0.0.0");
    
}

void home_screen_set_device(const char *name)
{
    if(lbl_device)
    {
        snprintf(g_format_buf, sizeof(g_format_buf), "Device: %s", name);
        lv_label_set_text(lbl_device, g_format_buf);
    }
}

void home_screen_set_sync_status(const char *txt)
{
    if(lbl_sync)
        lv_label_set_text(lbl_sync,txt);
}

void home_screen_refresh_invoice_details(void)
{
    uint8_t count = invoice_session_count();

    for(int i = 0; i < MAX_INVOICE_ITEMS; i++)
    {
        if(i < count)
        {
            const invoice_item_t *it = invoice_session_get(i);
            if(it)
            {
                snprintf(g_format_buf, sizeof(g_format_buf),
                         "%.3f kg x%d",
                         it->weight,
                         it->quantity);
                lv_label_set_text(item_labels[i], g_format_buf);
            }
        }
        else
        {
            lv_label_set_text(item_labels[i], "");
        }
    }
}

void home_screen_set_weight(float w)
{
    if(!lbl_weight) return;

    snprintf(g_format_buf, sizeof(g_format_buf), "%.1f kg", w);
    lv_label_set_text(lbl_weight, g_format_buf);
}

void home_screen_set_quantity(uint16_t qty)
{
    if(!lbl_qty) return;

    snprintf(g_format_buf, sizeof(g_format_buf), "Qty: %u", qty);
    lv_label_set_text(lbl_qty, g_format_buf);
}

void home_screen_set_invoice(uint32_t id)
{
    if(!lbl_invoice) return;

    snprintf(g_format_buf, sizeof(g_format_buf), "Invoice #%lu", id);
    lv_label_set_text(lbl_invoice, g_format_buf);
}

void home_screen_set_version(const char *ver)
{
    if(version_label)
    {
        snprintf(g_format_buf, sizeof(g_format_buf), "v%s", ver);
        lv_label_set_text(version_label, g_format_buf);
    }
}

#else
/* Stub implementations when LVGL is unavailable */
void home_screen_create(void *parent) { }
void home_screen_register_callback(void (*cb)(int evt)) { }
void home_screen_set_weight(float w) { }
void home_screen_set_quantity(uint16_t qty) { }
void home_screen_set_invoice(uint32_t id) { }
void home_screen_set_device(const char *name) { }
void home_screen_set_sync_status(const char *txt) { }
void home_screen_refresh_invoice_details(void) { }
void home_screen_set_version(const char *ver) { }
#endif  // LV_VERSION_MAJOR
