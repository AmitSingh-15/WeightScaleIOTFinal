#pragma once

#include <cstdint>
#include <lvgl.h>

/* Industrial color palette */
#define COLOR_PRIMARY   lv_color_hex(0x0052CC)  /* Blue */
#define COLOR_SUCCESS   lv_color_hex(0x2ECC71)  /* Green */
#define COLOR_WARNING   lv_color_hex(0xF39C12)  /* Orange */
#define COLOR_DANGER    lv_color_hex(0xE74C3C)  /* Red */
#define COLOR_BG        lv_color_hex(0x0F172A)  /* Dark background */
#define COLOR_CARD      lv_color_hex(0x1E293B)  /* Card background */
#define COLOR_TEXT      lv_color_hex(0xFFFFFF)  /* White text */
#define COLOR_MUTED     lv_color_hex(0x94A3B8)  /* Gray text */

typedef struct {
    lv_style_t screen;
    lv_style_t card;
    lv_style_t title;
    lv_style_t value_big;
    lv_style_t value_huge;      /* Extra large for weight display */
    lv_style_t value;
    lv_style_t btn_primary;     /* Blue - Primary actions */
    lv_style_t btn_secondary;   /* Blue */
    lv_style_t btn_danger;      /* Red - Delete/Reset */
    lv_style_t btn_success;     /* Green - Save/Finalize */
    lv_style_t btn_warning;     /* Orange - Quantity/Multiply */
    lv_style_t btn_action;      /* Bold for ADD/FINALIZE */
    lv_style_t btn_pressed;     /* Darker variant for pressed state */
    lv_style_t kb_bg;           /* Keyboard background */
    lv_style_t kb_btn;          /* Keyboard button items */
    lv_style_t ta;              /* Dark textarea */
    lv_style_t list_btn;        /* Dark list button item */
} ui_styles_t;

#ifdef __cplusplus
extern "C" {
#endif

extern ui_styles_t g_styles;

void ui_styles_init(void);
void ui_styles_set_theme(bool light_mode);
bool ui_styles_is_light_mode(void);

/* ── Runtime theme-aware color helpers ─────────────────────────────────────
   Use these instead of COLOR_* macros or hardcoded hex values when setting
   inline styles in screen-create functions, so every screen respects the
   active theme regardless of when it was created.                          */
static inline lv_color_t ui_theme_bg(void)      { return ui_styles_is_light_mode() ? lv_color_hex(0xF1F5F9) : lv_color_hex(0x0F172A); }
static inline lv_color_t ui_theme_card(void)    { return ui_styles_is_light_mode() ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x1E293B); }
static inline lv_color_t ui_theme_surface(void) { return ui_styles_is_light_mode() ? lv_color_hex(0xDDE3EA) : lv_color_hex(0x0C1222); }
static inline lv_color_t ui_theme_text(void)    { return ui_styles_is_light_mode() ? lv_color_hex(0x0F172A) : lv_color_hex(0xFFFFFF); }
static inline lv_color_t ui_theme_muted(void)   { return ui_styles_is_light_mode() ? lv_color_hex(0x475569) : lv_color_hex(0x94A3B8); }
static inline lv_color_t ui_theme_accent(void)  { return ui_styles_is_light_mode() ? lv_color_hex(0x0369A1) : lv_color_hex(0x38BDF8); }
static inline lv_color_t ui_theme_border(void)  { return ui_styles_is_light_mode() ? lv_color_hex(0xCBD5E1) : lv_color_hex(0x334155); }
static inline lv_color_t ui_theme_row_even(void){ return ui_styles_is_light_mode() ? lv_color_hex(0xF1F5F9) : lv_color_hex(0x1E293B); }
static inline lv_color_t ui_theme_row_odd(void) { return ui_styles_is_light_mode() ? lv_color_hex(0xE2E8F0) : lv_color_hex(0x162032); }

#ifdef __cplusplus
}
#endif
