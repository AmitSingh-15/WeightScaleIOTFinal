#ifdef LV_VERSION_MAJOR

#include "ui_styles.h"

ui_styles_t g_styles;

void ui_styles_init(void)
{
    lv_style_init(&g_styles.screen);
    lv_style_set_bg_color(&g_styles.screen, lv_color_hex(0xF2F2F2));
    lv_style_set_pad_all(&g_styles.screen, 0);

    lv_style_init(&g_styles.card);
    lv_style_set_bg_color(&g_styles.card, lv_color_white());
    lv_style_set_radius(&g_styles.card, 12);
    lv_style_set_pad_all(&g_styles.card, 12);
    lv_style_set_shadow_width(&g_styles.card, 12);
    lv_style_set_shadow_opa(&g_styles.card, LV_OPA_20);

    lv_style_init(&g_styles.title);
    lv_style_set_text_font(&g_styles.title, &lv_font_montserrat_14);
    lv_style_set_text_color(&g_styles.title, lv_color_hex(0x555555));

    lv_style_init(&g_styles.value_big);
    lv_style_set_text_font(&g_styles.value_big, &lv_font_montserrat_28);
    lv_style_set_text_color(&g_styles.value_big, lv_color_black());

    lv_style_init(&g_styles.value);
    lv_style_set_text_font(&g_styles.value, &lv_font_montserrat_14);

    lv_style_init(&g_styles.btn_primary);
    lv_style_set_bg_color(&g_styles.btn_primary, lv_color_hex(0x2E7D32));
    lv_style_set_radius(&g_styles.btn_primary, 8);

    lv_style_init(&g_styles.btn_secondary);
    lv_style_set_bg_color(&g_styles.btn_secondary, lv_color_hex(0x1976D2));
    lv_style_set_radius(&g_styles.btn_secondary, 8);

    lv_style_init(&g_styles.btn_danger);
    lv_style_set_bg_color(&g_styles.btn_danger, lv_color_hex(0xC62828));
    lv_style_set_radius(&g_styles.btn_danger, 8);
}

#else  // LV_VERSION_MAJOR not defined

#include "ui_styles.h"

ui_styles_t g_styles = {};

void ui_styles_init(void) {}

#endif  // LV_VERSION_MAJOR
