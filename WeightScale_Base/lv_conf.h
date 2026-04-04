#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_CONF_INCLUDE_SIMPLE

#define LV_COLOR_DEPTH 16

/* 🔥 AGGRESSIVE: Reduce LVGL internal memory pool from 256KB → 64KB (saves ~200KB!) */
#define LV_MEM_SIZE (64U * 1024U)

#define LV_USE_LOG 0

/* ⭐ CORE: Only essential widgets */
#define LV_USE_LABEL 1
#define LV_USE_BTN 1
#define LV_USE_OBJ 1
#define LV_USE_KEYBOARD 1      /* Needed for WiFi password input */
#define LV_USE_MSGBOX 1        /* Needed for dialogs */
#define LV_USE_TEXTAREA 1      /* Needed for text input */

/* ✅ Disable ALL unused widgets (saves ~50KB code) */
#define LV_USE_ANIMIMG 0
#define LV_USE_CALENDAR 0
#define LV_USE_CHART 0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN 0
#define LV_USE_LED 0
#define LV_USE_LIST 1
#define LV_USE_MENU 0
#define LV_USE_METER 0
#define LV_USE_SPINBOX 0
#define LV_USE_SPINNER 0
#define LV_USE_TABVIEW 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0

/* 🔥 DISABLE ALL EXTRA LIBRARIES (saves ~100KB code + RAM) */
#define LV_USE_FFMPEG 0
#define LV_USE_FREETYPE 0
#define LV_USE_PNG 0
#define LV_USE_GIF 0
#define LV_USE_QRCODE 0
#define LV_USE_RLOTTIE 0
#define LV_USE_SJPG 0
#define LV_USE_TINY_TTF 0
#define LV_USE_BMP 0
#define LV_USE_FSDRV 0

/* 🔥 DISABLE ALL THEMES (saves ~20KB) */
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_BASIC 0
#define LV_USE_THEME_MONO 0

/* Keep only necessary fonts */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1     /* List items, status text */
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_36 1     /* Qty display */
#define LV_FONT_MONTSERRAT_40 1     /* ENABLED - needed for weight display */
#define LV_FONT_MONTSERRAT_48 1     /* ENABLED - giant weight readout */

#define LV_USE_ANIMATION 1

#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

/* Disable demos (wastes space) */
#define LV_USE_DEMO_WIDGETS 0
#define LV_USE_DEMO_BENCHMARK 0
#define LV_USE_DEMO_STRESS 0

#endif