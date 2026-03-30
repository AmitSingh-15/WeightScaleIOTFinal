#include <lvgl.h>
#include "device_name_screen.h"
#include "ui_styles.h"
#include <stdio.h>
#include <string.h>

#ifdef LV_VERSION_MAJOR

/* =====================================================
   STATIC OBJECTS
=====================================================*/

static lv_obj_t *lbl_title;
static lv_obj_t *ta_name;
static lv_obj_t *kb;
static lv_obj_t *btn_save;

static void (*event_cb)(int evt, const char *name) = NULL;

/* =====================================================
   INTERNAL EVENTS
=====================================================*/

static void save_clicked(lv_event_t *e)
{
    if(!event_cb) return;

    const char *txt = lv_textarea_get_text(ta_name);

    if(!txt || strlen(txt) < 2)
    {
        return;
    }

    event_cb(DEVNAME_EVT_SAVE, txt);
}

/* Keyboard OK button handling */
static void kb_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_READY)
    {
        /* Enter pressed */
        save_clicked(e);
    }
}

/* =====================================================
   CREATE SCREEN
=====================================================*/

void device_name_screen_create(lv_obj_t *parent)
{
    ui_styles_init();

    lv_obj_add_style(parent, &g_styles.screen, 0);
    lv_obj_set_size(parent, 800, 480);

    /* ================= HEADER ================= */

    lbl_title = lv_label_create(parent);
    lv_label_set_text(lbl_title, "ENTER DEVICE NAME");
    lv_obj_set_style_text_font(lbl_title,&lv_font_montserrat_20, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 40);

    /* ================= TEXTAREA ================= */

    ta_name = lv_textarea_create(parent);
    lv_obj_set_size(ta_name, 520, 70);
    lv_obj_align(ta_name, LV_ALIGN_TOP_MID, 0, 100);

    lv_textarea_set_one_line(ta_name, true);
    lv_textarea_set_max_length(ta_name, 20);

    /* Industrial allowed chars */
    lv_textarea_set_accepted_chars(
        ta_name,
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-"
    );

    lv_textarea_set_placeholder_text(
        ta_name,
        "ACPL-BRANCH-NAME"
    );

    /* ================= SAVE BUTTON ================= */

    btn_save = lv_btn_create(parent);
    lv_obj_add_style(btn_save, &g_styles.btn_primary, 0);
    lv_obj_set_size(btn_save, 220, 70);
    lv_obj_align(btn_save, LV_ALIGN_TOP_MID, 0, 200);

    /* 🔥 FIX: CLICKED → RELEASED (touch-safe) */
    lv_obj_add_event_cb(
        btn_save,
        save_clicked,
        LV_EVENT_RELEASED,
        NULL
    );

    lv_label_set_text(
        lv_label_create(btn_save),
        "SAVE DEVICE"
    );

    /* ================= KEYBOARD ================= */

    kb = lv_keyboard_create(parent);
    lv_obj_set_size(kb, 800, 200);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_UPPER);

    /* Link keyboard to textarea */
    lv_keyboard_set_textarea(kb, ta_name);

    lv_obj_add_event_cb(kb, kb_event, LV_EVENT_ALL, NULL);
}

/* =====================================================
   CALLBACK REGISTRATION
=====================================================*/

void device_name_screen_register_callback(
    void (*cb)(int evt, const char *name))
{
    event_cb = cb;
}

/* =====================================================
   HELPERS
=====================================================*/

void device_name_screen_set_title(const char *txt)
{
    if(lbl_title)
        lv_label_set_text(lbl_title, txt);
}

void device_name_screen_focus(void)
{
    if(ta_name)
        lv_obj_add_state(ta_name, LV_STATE_FOCUSED);
}

#else
/* Stub implementations when LVGL is unavailable */
void device_name_screen_create(void *parent) { }
void device_name_screen_register_callback(void (*cb)(int evt, const char *name)) { }
void device_name_screen_set_title(const char *txt) { }
void device_name_screen_focus(void) { }
#endif  // LV_VERSION_MAJOR
