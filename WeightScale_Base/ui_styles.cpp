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
    lv_style_set_pad_all(&g_styles.screen, 0);

    /* ===== CARD - dark panel with subtle border ===== */
    lv_style_init(&g_styles.card);
    lv_style_set_bg_color(&g_styles.card, COLOR_CARD);
    lv_style_set_radius(&g_styles.card, 12);
    lv_style_set_pad_all(&g_styles.card, 12);
    lv_style_set_border_width(&g_styles.card, 1);
    lv_style_set_border_color(&g_styles.card, lv_color_hex(0x334155));
    lv_style_set_shadow_width(&g_styles.card, 0);

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
}

#else  // LV_VERSION_MAJOR not defined

#include "ui_styles.h"

ui_styles_t g_styles = {};

void ui_styles_init(void) {}

#endif  // LV_VERSION_MAJOR
