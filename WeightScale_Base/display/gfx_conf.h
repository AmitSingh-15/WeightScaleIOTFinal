#pragma once

#include "config/app_config.h"

/* ✅ CLEAN: Removed all LGFX/LovyanGFX code
   This file now only defines display constants.
   All actual hardware initialization is handled by
   ESP32_Display_Panel's official library. */

#define screenWidth   DISPLAY_WIDTH
#define screenHeight  DISPLAY_HEIGHT

/* LVGL buffer configuration - defined in app_config.h
   Recommended: 5-20 lines for stability (smaller = more frequent flushes, but safer) */
