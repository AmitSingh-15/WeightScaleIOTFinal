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
} ui_styles_t;

#ifdef __cplusplus
extern "C" {
#endif

extern ui_styles_t g_styles;

void ui_styles_init(void);

#ifdef __cplusplus
}
#endif
