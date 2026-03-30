#pragma once

/* Local shim for Arduino builder when LVGL library isn't installed in global libraries path. */

/* Try to include the local LVGL library */
#if __has_include("libraries/lvgl/lvgl.h")
#include "libraries/lvgl/lvgl.h"
#define LV_VERSION_MAJOR 8
#elif __has_include("../libraries/lvgl/lvgl.h")
#include "../libraries/lvgl/lvgl.h"
#define LV_VERSION_MAJOR 8
#elif __has_include(<lvgl.h>)
#include <lvgl.h>
#else
/* LVGL not available - provide minimal stub definitions */
#define LV_VERSION_MAJOR 0
#endif
