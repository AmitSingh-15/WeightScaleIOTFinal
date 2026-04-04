#pragma once

/* ===============================
   LVGL BASIC CONFIG
   =============================== */

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (128 * 1024)

#define LV_TICK_CUSTOM 0

#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL 1

/* ===============================
   FONTS
   =============================== */

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_48 1

/* ===============================
   WIDGETS
   =============================== */

#define LV_USE_LABEL 1
#define LV_USE_BTN 1
#define LV_USE_FLEX 1
#define LV_USE_GRID 1

/* ===============================
   THEME
   =============================== */

#define LV_USE_THEME_DEFAULT 1
