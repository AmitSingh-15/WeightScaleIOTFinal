#include <lvgl.h>

#ifndef LV_VERSION_MAJOR
#define LV_VERSION_MAJOR 8
#endif

#ifdef LV_VERSION_MAJOR

#include "ui_styles.h"

ui_styles_t g_styles;

/* Minimum touch-friendly button dimensions enforced via styles */
#define BTN_MIN_H       65
#define BTN_PAD_H       18
#define BTN_PAD_V       14
#define BTN_RADIUS      12
#define BTN_FONT        &lv_font_montserrat_28

void ui_styles_init(void)
{
    /* ===== DARK INDUSTRIAL BACKGROUND ===== */
    lv_style_init(&g_styles.screen);
    lv_style_set_bg_color(&g_styles.screen, COLOR_BG);
    lv_style_set_bg_opa(&g_styles.screen, LV_OPA_COVER);
    lv_style_set_text_color(&g_styles.screen, COLOR_TEXT);
    lv_style_set_pad_all(&g_styles.screen, 0);

    /* ===== CARD - dark panel with subtle border ===== */
    lv_style_init(&g_styles.card);
    lv_style_set_bg_color(&g_styles.card, COLOR_CARD);
    lv_style_set_bg_opa(&g_styles.card, LV_OPA_COVER);
    lv_style_set_radius(&g_styles.card, 12);
    lv_style_set_pad_all(&g_styles.card, 12);
    lv_style_set_border_width(&g_styles.card, 1);
    lv_style_set_border_color(&g_styles.card, lv_color_hex(0x334155));
    lv_style_set_shadow_width(&g_styles.card, 0);
    lv_style_set_text_color(&g_styles.card, COLOR_TEXT);

    /* ===== TITLE - clear readable text ===== */
    lv_style_init(&g_styles.title);
    lv_style_set_text_font(&g_styles.title, &lv_font_montserrat_28);
    lv_style_set_text_color(&g_styles.title, lv_color_hex(0x38BDF8));

    /* ===== VALUE BIG - 48pt bright qty / info ===== */
    lv_style_init(&g_styles.value_big);
    lv_style_set_text_font(&g_styles.value_big, &lv_font_montserrat_48);
    lv_style_set_text_color(&g_styles.value_big, lv_color_hex(0x38BDF8));

    /* ===== VALUE HUGE - 48pt GIANT weight readout ===== */
    lv_style_init(&g_styles.value_huge);
    lv_style_set_text_font(&g_styles.value_huge, &lv_font_montserrat_48);
    lv_style_set_text_color(&g_styles.value_huge, lv_color_hex(0x22D3EE));

    /* ===== VALUE - standard 20pt ===== */
    lv_style_init(&g_styles.value);
    lv_style_set_text_font(&g_styles.value, &lv_font_montserrat_20);
    lv_style_set_text_color(&g_styles.value, COLOR_TEXT);

    /* ===== BUTTON: Primary - Steel Blue ===== */
    lv_style_init(&g_styles.btn_primary);
    lv_style_set_min_height(&g_styles.btn_primary, BTN_MIN_H);
    lv_style_set_pad_hor(&g_styles.btn_primary, BTN_PAD_H);
    lv_style_set_pad_ver(&g_styles.btn_primary, BTN_PAD_V);
    lv_style_set_text_font(&g_styles.btn_primary, BTN_FONT);
    lv_style_set_bg_color(&g_styles.btn_primary, lv_color_hex(0x1E3A5F));
    lv_style_set_bg_opa(&g_styles.btn_primary, LV_OPA_COVER);
    lv_style_set_bg_grad_color(&g_styles.btn_primary, lv_color_hex(0x0F2744));
    lv_style_set_bg_grad_dir(&g_styles.btn_primary, LV_GRAD_DIR_VER);
    lv_style_set_text_color(&g_styles.btn_primary, COLOR_TEXT);
    lv_style_set_radius(&g_styles.btn_primary, BTN_RADIUS);
    lv_style_set_border_width(&g_styles.btn_primary, 2);
    lv_style_set_border_color(&g_styles.btn_primary, lv_color_hex(0x2563EB));
    lv_style_set_shadow_width(&g_styles.btn_primary, 8);
    lv_style_set_shadow_color(&g_styles.btn_primary, lv_color_hex(0x1E3A5F));
    lv_style_set_shadow_opa(&g_styles.btn_primary, LV_OPA_40);

    /* ===== BUTTON: Secondary - Slate ===== */
    lv_style_init(&g_styles.btn_secondary);
    lv_style_set_min_height(&g_styles.btn_secondary, BTN_MIN_H);
    lv_style_set_pad_hor(&g_styles.btn_secondary, BTN_PAD_H);
    lv_style_set_pad_ver(&g_styles.btn_secondary, BTN_PAD_V);
    lv_style_set_text_font(&g_styles.btn_secondary, BTN_FONT);
    lv_style_set_bg_color(&g_styles.btn_secondary, lv_color_hex(0x334155));
    lv_style_set_bg_opa(&g_styles.btn_secondary, LV_OPA_COVER);
    lv_style_set_bg_grad_color(&g_styles.btn_secondary, lv_color_hex(0x1E293B));
    lv_style_set_bg_grad_dir(&g_styles.btn_secondary, LV_GRAD_DIR_VER);
    lv_style_set_text_color(&g_styles.btn_secondary, COLOR_TEXT);
    lv_style_set_radius(&g_styles.btn_secondary, BTN_RADIUS);
    lv_style_set_border_width(&g_styles.btn_secondary, 1);
    lv_style_set_border_color(&g_styles.btn_secondary, lv_color_hex(0x475569));

    /* ===== BUTTON: Danger - Deep Red ===== */
    lv_style_init(&g_styles.btn_danger);
    lv_style_set_min_height(&g_styles.btn_danger, BTN_MIN_H);
    lv_style_set_pad_hor(&g_styles.btn_danger, BTN_PAD_H);
    lv_style_set_pad_ver(&g_styles.btn_danger, BTN_PAD_V);
    lv_style_set_text_font(&g_styles.btn_danger, BTN_FONT);
    lv_style_set_bg_color(&g_styles.btn_danger, lv_color_hex(0x991B1B));
    lv_style_set_bg_opa(&g_styles.btn_danger, LV_OPA_COVER);
    lv_style_set_bg_grad_color(&g_styles.btn_danger, lv_color_hex(0x7F1D1D));
    lv_style_set_bg_grad_dir(&g_styles.btn_danger, LV_GRAD_DIR_VER);
    lv_style_set_text_color(&g_styles.btn_danger, COLOR_TEXT);
    lv_style_set_radius(&g_styles.btn_danger, BTN_RADIUS);
    lv_style_set_border_width(&g_styles.btn_danger, 2);
    lv_style_set_border_color(&g_styles.btn_danger, lv_color_hex(0xDC2626));
    lv_style_set_shadow_width(&g_styles.btn_danger, 8);
    lv_style_set_shadow_color(&g_styles.btn_danger, lv_color_hex(0x991B1B));
    lv_style_set_shadow_opa(&g_styles.btn_danger, LV_OPA_30);

    /* ===== BUTTON: Success - Deep Green ===== */
    lv_style_init(&g_styles.btn_success);
    lv_style_set_min_height(&g_styles.btn_success, BTN_MIN_H);
    lv_style_set_pad_hor(&g_styles.btn_success, BTN_PAD_H);
    lv_style_set_pad_ver(&g_styles.btn_success, BTN_PAD_V);
    lv_style_set_text_font(&g_styles.btn_success, BTN_FONT);
    lv_style_set_bg_color(&g_styles.btn_success, lv_color_hex(0x166534));
    lv_style_set_bg_opa(&g_styles.btn_success, LV_OPA_COVER);
    lv_style_set_bg_grad_color(&g_styles.btn_success, lv_color_hex(0x14532D));
    lv_style_set_bg_grad_dir(&g_styles.btn_success, LV_GRAD_DIR_VER);
    lv_style_set_text_color(&g_styles.btn_success, COLOR_TEXT);
    lv_style_set_radius(&g_styles.btn_success, BTN_RADIUS);
    lv_style_set_border_width(&g_styles.btn_success, 2);
    lv_style_set_border_color(&g_styles.btn_success, lv_color_hex(0x22C55E));
    lv_style_set_shadow_width(&g_styles.btn_success, 8);
    lv_style_set_shadow_color(&g_styles.btn_success, lv_color_hex(0x166534));
    lv_style_set_shadow_opa(&g_styles.btn_success, LV_OPA_30);

    /* ===== BUTTON: Warning/Qty - Amber ===== */
    lv_style_init(&g_styles.btn_warning);
    lv_style_set_min_height(&g_styles.btn_warning, BTN_MIN_H);
    lv_style_set_pad_hor(&g_styles.btn_warning, BTN_PAD_H);
    lv_style_set_pad_ver(&g_styles.btn_warning, BTN_PAD_V);
    lv_style_set_text_font(&g_styles.btn_warning, BTN_FONT);
    lv_style_set_bg_color(&g_styles.btn_warning, lv_color_hex(0x92400E));
    lv_style_set_bg_opa(&g_styles.btn_warning, LV_OPA_COVER);
    lv_style_set_bg_grad_color(&g_styles.btn_warning, lv_color_hex(0x78350F));
    lv_style_set_bg_grad_dir(&g_styles.btn_warning, LV_GRAD_DIR_VER);
    lv_style_set_text_color(&g_styles.btn_warning, lv_color_hex(0xFCD34D));
    lv_style_set_radius(&g_styles.btn_warning, BTN_RADIUS);
    lv_style_set_border_width(&g_styles.btn_warning, 2);
    lv_style_set_border_color(&g_styles.btn_warning, lv_color_hex(0xF59E0B));
    lv_style_set_shadow_width(&g_styles.btn_warning, 8);
    lv_style_set_shadow_color(&g_styles.btn_warning, lv_color_hex(0x92400E));
    lv_style_set_shadow_opa(&g_styles.btn_warning, LV_OPA_30);

    /* ===== BUTTON: Action - SAVE/FINALIZE highlight ===== */
    lv_style_init(&g_styles.btn_action);
    lv_style_set_min_height(&g_styles.btn_action, BTN_MIN_H);
    lv_style_set_pad_hor(&g_styles.btn_action, BTN_PAD_H);
    lv_style_set_pad_ver(&g_styles.btn_action, BTN_PAD_V);
    lv_style_set_text_font(&g_styles.btn_action, BTN_FONT);
    lv_style_set_bg_color(&g_styles.btn_action, lv_color_hex(0x1D4ED8));
    lv_style_set_bg_opa(&g_styles.btn_action, LV_OPA_COVER);
    lv_style_set_bg_grad_color(&g_styles.btn_action, lv_color_hex(0x1E40AF));
    lv_style_set_bg_grad_dir(&g_styles.btn_action, LV_GRAD_DIR_VER);
    lv_style_set_text_color(&g_styles.btn_action, COLOR_TEXT);
    lv_style_set_radius(&g_styles.btn_action, BTN_RADIUS);
    lv_style_set_border_width(&g_styles.btn_action, 2);
    lv_style_set_border_color(&g_styles.btn_action, lv_color_hex(0x3B82F6));
    lv_style_set_shadow_width(&g_styles.btn_action, 10);
    lv_style_set_shadow_color(&g_styles.btn_action, lv_color_hex(0x1D4ED8));
    lv_style_set_shadow_opa(&g_styles.btn_action, LV_OPA_50);

    /* ===== PRESSED STATE ===== */
    lv_style_init(&g_styles.btn_pressed);
    lv_style_set_bg_color(&g_styles.btn_pressed, lv_color_hex(0x0F172A));
    lv_style_set_transform_width(&g_styles.btn_pressed, -2);
    lv_style_set_transform_height(&g_styles.btn_pressed, -2);

    /* ===== KEYBOARD BACKGROUND (LV_PART_MAIN) ===== */
    lv_style_init(&g_styles.kb_bg);
    lv_style_set_bg_color(&g_styles.kb_bg, lv_color_hex(0x0F172A));
    lv_style_set_bg_opa(&g_styles.kb_bg, LV_OPA_COVER);
    lv_style_set_border_width(&g_styles.kb_bg, 0);
    lv_style_set_pad_all(&g_styles.kb_bg, 4);
    lv_style_set_pad_gap(&g_styles.kb_bg, 4);

    /* ===== KEYBOARD BUTTONS (LV_PART_ITEMS) ===== */
    lv_style_init(&g_styles.kb_btn);
    lv_style_set_bg_color(&g_styles.kb_btn, lv_color_hex(0x334155));
    lv_style_set_bg_opa(&g_styles.kb_btn, LV_OPA_COVER);
    lv_style_set_text_color(&g_styles.kb_btn, lv_color_hex(0xFFFFFF));
    lv_style_set_border_color(&g_styles.kb_btn, lv_color_hex(0x475569));
    lv_style_set_border_width(&g_styles.kb_btn, 1);
    lv_style_set_radius(&g_styles.kb_btn, 6);

    /* ===== TEXTAREA DARK ===== */
    lv_style_init(&g_styles.ta);
    lv_style_set_bg_color(&g_styles.ta, lv_color_hex(0x1E293B));
    lv_style_set_bg_opa(&g_styles.ta, LV_OPA_COVER);
    lv_style_set_text_color(&g_styles.ta, lv_color_hex(0xFFFFFF));
    lv_style_set_border_color(&g_styles.ta, lv_color_hex(0x475569));
    lv_style_set_border_width(&g_styles.ta, 2);
    lv_style_set_radius(&g_styles.ta, 8);
    lv_style_set_pad_all(&g_styles.ta, 10);

    /* ===== LIST BUTTON ITEM ===== */
    lv_style_init(&g_styles.list_btn);
    lv_style_set_bg_color(&g_styles.list_btn, lv_color_hex(0x1E293B));
    lv_style_set_bg_opa(&g_styles.list_btn, LV_OPA_COVER);
    lv_style_set_text_color(&g_styles.list_btn, lv_color_hex(0xFFFFFF));
    lv_style_set_border_color(&g_styles.list_btn, lv_color_hex(0x334155));
    lv_style_set_border_width(&g_styles.list_btn, 1);
    lv_style_set_border_side(&g_styles.list_btn, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_pad_ver(&g_styles.list_btn, 12);
    lv_style_set_pad_hor(&g_styles.list_btn, 10);
    lv_style_set_radius(&g_styles.list_btn, 0);
}

static bool g_light_mode = false;

bool ui_styles_is_light_mode(void) { return g_light_mode; }

void ui_styles_set_theme(bool light)
{
    g_light_mode = light;

    lv_color_t bg      = light ? lv_color_hex(0xF1F5F9) : lv_color_hex(0x0F172A);
    lv_color_t card    = light ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x1E293B);
    lv_color_t text    = light ? lv_color_hex(0x0F172A) : lv_color_hex(0xFFFFFF);
    lv_color_t muted   = light ? lv_color_hex(0x64748B) : lv_color_hex(0x94A3B8);
    lv_color_t border  = light ? lv_color_hex(0xCBD5E1) : lv_color_hex(0x334155);
    lv_color_t accent  = light ? lv_color_hex(0x0369A1) : lv_color_hex(0x38BDF8);
    lv_color_t surface = light ? lv_color_hex(0xDDE3EA) : lv_color_hex(0x0C1222);
    lv_color_t weight_color = light ? lv_color_hex(0x0E7490) : lv_color_hex(0x22D3EE);

    /* Screen */
    lv_style_set_bg_color(&g_styles.screen, bg);
    lv_style_set_text_color(&g_styles.screen, text);

    /* Card */
    lv_style_set_bg_color(&g_styles.card, card);
    lv_style_set_text_color(&g_styles.card, text);
    lv_style_set_border_color(&g_styles.card, border);

    /* Typography */
    lv_style_set_text_color(&g_styles.title, accent);
    lv_style_set_text_color(&g_styles.value_big, accent);
    lv_style_set_text_color(&g_styles.value_huge, weight_color);
    lv_style_set_text_color(&g_styles.value, text);

    /* Buttons — secondary adapts to theme, action buttons stay vivid */
    lv_style_set_bg_color(&g_styles.btn_secondary,
        light ? lv_color_hex(0xCBD5E1) : lv_color_hex(0x334155));
    lv_style_set_bg_grad_color(&g_styles.btn_secondary,
        light ? lv_color_hex(0xB0BCCD) : lv_color_hex(0x1E293B));
    lv_style_set_text_color(&g_styles.btn_secondary,
        light ? lv_color_hex(0x1E293B) : lv_color_hex(0xFFFFFF));
    lv_style_set_border_color(&g_styles.btn_secondary,
        light ? lv_color_hex(0x94A3B8) : lv_color_hex(0x475569));

    /* Pressed state */
    lv_style_set_bg_color(&g_styles.btn_pressed,
        light ? lv_color_hex(0xB0BCCD) : lv_color_hex(0x0F172A));

    /* Keyboard */
    lv_style_set_bg_color(&g_styles.kb_bg, surface);
    lv_style_set_bg_color(&g_styles.kb_btn,
        light ? lv_color_hex(0xE2E8F0) : lv_color_hex(0x334155));
    lv_style_set_text_color(&g_styles.kb_btn, text);
    lv_style_set_border_color(&g_styles.kb_btn, border);

    /* Textarea */
    lv_style_set_bg_color(&g_styles.ta,
        light ? lv_color_hex(0xF8FAFC) : lv_color_hex(0x1E293B));
    lv_style_set_text_color(&g_styles.ta, text);
    lv_style_set_border_color(&g_styles.ta, border);

    /* List button */
    lv_style_set_bg_color(&g_styles.list_btn, card);
    lv_style_set_text_color(&g_styles.list_btn, text);
    lv_style_set_border_color(&g_styles.list_btn, border);

    /* Force full redraw */
    lv_obj_invalidate(lv_scr_act());
}

#else  // LV_VERSION_MAJOR not defined

#include "ui_styles.h"

ui_styles_t g_styles = {};

void ui_styles_init(void) {}
void ui_styles_set_theme(bool light) {}
bool ui_styles_is_light_mode(void) { return false; }

#endif  // LV_VERSION_MAJOR
