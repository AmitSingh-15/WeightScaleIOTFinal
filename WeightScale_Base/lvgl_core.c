#define LV_CONF_INCLUDE_SIMPLE 1

/* 
 * LVGL Core Infrastructure Wrapper
 * Contains all non-widget LVGL modules to avoid symbol conflicts
 */

/* Core modules */
#include "libraries/lvgl/src/core/lv_disp.c"
#include "libraries/lvgl/src/core/lv_event.c"
#include "libraries/lvgl/src/core/lv_group.c"
#include "libraries/lvgl/src/core/lv_indev.c"
#include "libraries/lvgl/src/core/lv_indev_scroll.c"
#include "libraries/lvgl/src/core/lv_obj.c"
#include "libraries/lvgl/src/core/lv_obj_class.c"
#include "libraries/lvgl/src/core/lv_obj_draw.c"
#include "libraries/lvgl/src/core/lv_obj_pos.c"
#include "libraries/lvgl/src/core/lv_obj_scroll.c"
#include "libraries/lvgl/src/core/lv_obj_style.c"
#include "libraries/lvgl/src/core/lv_obj_style_gen.c"
#include "libraries/lvgl/src/core/lv_obj_tree.c"
#include "libraries/lvgl/src/core/lv_refr.c"
#include "libraries/lvgl/src/core/lv_theme.c"

/* Drawing modules */
#include "libraries/lvgl/src/draw/lv_draw.c"
#include "libraries/lvgl/src/draw/lv_draw_arc.c"
#include "libraries/lvgl/src/draw/lv_draw_img.c"
#include "libraries/lvgl/src/draw/lv_draw_label.c"
#include "libraries/lvgl/src/draw/lv_draw_layer.c"
#include "libraries/lvgl/src/draw/lv_draw_line.c"
#include "libraries/lvgl/src/draw/lv_draw_mask.c"
#include "libraries/lvgl/src/draw/lv_draw_rect.c"
#include "libraries/lvgl/src/draw/lv_draw_transform.c"
#include "libraries/lvgl/src/draw/lv_draw_triangle.c"
#include "libraries/lvgl/src/draw/lv_img_buf.c"
#include "libraries/lvgl/src/draw/lv_img_cache.c"
#include "libraries/lvgl/src/draw/lv_img_decoder.c"

/* HAL modules */
#include "libraries/lvgl/src/hal/lv_hal_disp.c"
#include "libraries/lvgl/src/hal/lv_hal_indev.c"
#include "libraries/lvgl/src/hal/lv_hal_tick.c"

/* Font infrastructure (NOT individual font files - those are separate) */
#include "libraries/lvgl/src/font/lv_font.c"
#include "libraries/lvgl/src/font/lv_font_fmt_txt.c"
#include "libraries/lvgl/src/font/lv_font_loader.c"

/* Misc modules */
#include "libraries/lvgl/src/misc/lv_anim.c"
#include "libraries/lvgl/src/misc/lv_anim_timeline.c"
#include "libraries/lvgl/src/misc/lv_area.c"
#include "libraries/lvgl/src/misc/lv_async.c"
#include "libraries/lvgl/src/misc/lv_bidi.c"
#include "libraries/lvgl/src/misc/lv_color.c"
#include "libraries/lvgl/src/misc/lv_fs.c"
#include "libraries/lvgl/src/misc/lv_gc.c"
#include "libraries/lvgl/src/misc/lv_ll.c"
#include "libraries/lvgl/src/misc/lv_log.c"
#include "libraries/lvgl/src/misc/lv_lru.c"
#include "libraries/lvgl/src/misc/lv_math.c"
#include "libraries/lvgl/src/misc/lv_mem.c"
#include "libraries/lvgl/src/misc/lv_printf.c"
#include "libraries/lvgl/src/misc/lv_style.c"
#include "libraries/lvgl/src/misc/lv_style_gen.c"
#include "libraries/lvgl/src/misc/lv_txt.c"
#include "libraries/lvgl/src/misc/lv_txt_ap.c"
#include "libraries/lvgl/src/misc/lv_timer.c"
#include "libraries/lvgl/src/misc/lv_tlsf.c"
#include "libraries/lvgl/src/misc/lv_utils.c"

/* Extra modules */
#include "libraries/lvgl/src/extra/lv_extra.c"
