#include <lvgl.h>

// ✅ Ensure LVGL is detected
#ifndef LV_VERSION_MAJOR
#define LV_VERSION_MAJOR 8
#endif

#include "config/app_config.h"
#include "device_name_screen.h"
#include "ui_styles.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef LV_VERSION_MAJOR

#ifdef __cplusplus
extern "C" {
#endif

/* =====================================================
   STATIC OBJECTS
=====================================================*/

static lv_obj_t *lbl_title;
static lv_obj_t *ta_name;
static lv_obj_t *ta_id;
static lv_obj_t *kb;
static lv_obj_t *btn_save;
static lv_obj_t *active_ta = NULL;  /* Track which textarea has focus */

static devname_cb_t event_cb = NULL;

/* =====================================================
   INTERNAL EVENTS
=====================================================*/

static void save_clicked(lv_event_t *e)
{
    if(!event_cb) return;

    const char *txt = lv_textarea_get_text(ta_name);
    const char *id_txt = lv_textarea_get_text(ta_id);

    if(!txt || strlen(txt) < 2) return;

    uint32_t dev_id = 0;
    if(id_txt && strlen(id_txt) > 0) {
        dev_id = (uint32_t)atol(id_txt);
    }

    event_cb(DEVNAME_EVT_SAVE, txt, dev_id);
}

/* Keyboard OK button handling */
static void kb_event(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_READY) {
        save_clicked(e);
    }
}

/* Switch keyboard focus between textareas */
static void ta_focus_cb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    active_ta = ta;
    if(kb) {
        lv_keyboard_set_textarea(kb, ta);
        /* Switch keyboard mode based on which field is focused */
        if(ta == ta_id) {
            lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
        } else {
            lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_UPPER);
        }
    }
}

/* =====================================================
   CREATE SCREEN
=====================================================*/

void device_name_screen_create(lv_obj_t *parent)
{
    ui_styles_init();

    lv_obj_add_style(parent, &g_styles.screen, 0);
    lv_obj_set_size(parent, DISPLAY_WIDTH, DISPLAY_HEIGHT);

    /* ================= HEADER ================= */

    lbl_title = lv_label_create(parent);
    lv_label_set_text(lbl_title, "DEVICE CONFIGURATION");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x38BDF8), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 20);

    /* ================= DEVICE ID FIELD ================= */

    lv_obj_t *id_lbl = lv_label_create(parent);
    lv_obj_set_style_text_font(id_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(id_lbl, COLOR_MUTED, 0);
    lv_label_set_text(id_lbl, "DEVICE ID:");
    lv_obj_align(id_lbl, LV_ALIGN_TOP_LEFT, 80, 70);

    ta_id = lv_textarea_create(parent);
    lv_obj_set_size(ta_id, 350, 60);
    lv_obj_align(ta_id, LV_ALIGN_TOP_LEFT, 260, 60);
    lv_textarea_set_one_line(ta_id, true);
    lv_textarea_set_max_length(ta_id, 10);
    lv_textarea_set_accepted_chars(ta_id, "0123456789");
    lv_textarea_set_placeholder_text(ta_id, "e.g. 1");
    lv_obj_set_style_text_font(ta_id, &lv_font_montserrat_20, 0);
    lv_obj_add_event_cb(ta_id, ta_focus_cb, LV_EVENT_FOCUSED, NULL);

    /* ================= DEVICE NAME FIELD ================= */

    lv_obj_t *name_lbl = lv_label_create(parent);
    lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(name_lbl, COLOR_MUTED, 0);
    lv_label_set_text(name_lbl, "DEVICE NAME:");
    lv_obj_align(name_lbl, LV_ALIGN_TOP_LEFT, 80, 140);

    ta_name = lv_textarea_create(parent);
    lv_obj_set_size(ta_name, 520, 60);
    lv_obj_align(ta_name, LV_ALIGN_TOP_LEFT, 260, 130);
    lv_textarea_set_one_line(ta_name, true);
    lv_textarea_set_max_length(ta_name, 20);
    lv_textarea_set_accepted_chars(ta_name, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-");
    lv_textarea_set_placeholder_text(ta_name, "ACPL-BRANCH-NAME");
    lv_obj_set_style_text_font(ta_name, &lv_font_montserrat_20, 0);
    lv_obj_add_event_cb(ta_name, ta_focus_cb, LV_EVENT_FOCUSED, NULL);

    /* ================= SAVE BUTTON ================= */

    btn_save = lv_btn_create(parent);
    lv_obj_add_style(btn_save, &g_styles.btn_primary, 0);
    lv_obj_set_size(btn_save, 250, 65);
    lv_obj_align(btn_save, LV_ALIGN_TOP_MID, 0, 210);
    lv_obj_add_event_cb(btn_save, save_clicked, LV_EVENT_RELEASED, NULL);

    lv_obj_t *save_lbl = lv_label_create(btn_save);
    lv_obj_set_style_text_font(save_lbl, &lv_font_montserrat_20, 0);
    lv_label_set_text(save_lbl, LV_SYMBOL_SAVE " SAVE DEVICE");
    lv_obj_center(save_lbl);

    /* ================= KEYBOARD ================= */

    kb = lv_keyboard_create(parent);
    lv_obj_set_size(kb, DISPLAY_WIDTH, 300);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_UPPER);
    lv_keyboard_set_textarea(kb, ta_name);
    lv_obj_add_event_cb(kb, kb_event, LV_EVENT_ALL, NULL);

    active_ta = ta_name;

    /* Null out on delete */
    lv_obj_add_event_cb(parent, [](lv_event_t *e) {
        lbl_title = NULL;
        ta_name = NULL;
        ta_id = NULL;
        kb = NULL;
        btn_save = NULL;
        active_ta = NULL;
    }, LV_EVENT_DELETE, NULL);
}

/* =====================================================
   CALLBACK REGISTRATION
=====================================================*/

void device_name_screen_register_callback(devname_cb_t cb)
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

void device_name_screen_set_values(const char *name, uint32_t dev_id)
{
    if(ta_name && name) {
        lv_textarea_set_text(ta_name, name);
    }
    if(ta_id) {
        char id_buf[16];
        snprintf(id_buf, sizeof(id_buf), "%lu", dev_id);
        lv_textarea_set_text(ta_id, id_buf);
    }
}

void device_name_screen_focus(void)
{
    if(ta_name)
        lv_obj_add_state(ta_name, LV_STATE_FOCUSED);
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // LV_VERSION_MAJOR
