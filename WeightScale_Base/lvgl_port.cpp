#ifdef LV_VERSION_MAJOR

#include "lvgl_port.h"
#include "gfx_conf.h"
#include "esp_heap_caps.h"
#include "devlog.h"
#include <lvgl_v8_port.h>

static void touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data);

/* =============================
   LVGL DRAW BUFFERS
============================= */

static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1 = nullptr;
static lv_color_t *buf2 = nullptr;

/* =============================

============================= */

static void flush_cb(lv_disp_drv_t *disp,
                     const lv_area_t *area,
                     lv_color_t *color_p)
{
    // Display flush handled by ESP32_Display_Panel library
    lv_disp_flush_ready(disp);
}

/* =============================
   INIT
============================= */

bool lvgl_port_init(void *lcd_handle, void *touch_handle)
{
    lv_init();      // MUST be first
    // Display initialization handled by ESP32_Display_Panel library

    buf1 = (lv_color_t *)heap_caps_malloc(
        screenWidth * 30 * sizeof(lv_color_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );

    buf2 = (lv_color_t *)heap_caps_malloc(
        screenWidth * 30 * sizeof(lv_color_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );

    if (!buf1 || !buf2) {
        Serial.println("LVGL buffer allocation failed");
        devlog_printf("LVGL buffer allocation failed");
        while (1);
    }

    lv_disp_draw_buf_init(
        &draw_buf,
        buf1,
        buf2,
        screenWidth * 30
    );

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = screenWidth;
    disp_drv.ver_res  = screenHeight;
    disp_drv.flush_cb = flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read;
    lv_indev_drv_register(&indev_drv);

    Serial.println("[LVGL] Port initialized OK");
    devlog_printf("[LVGL] Port initialized OK");
    
    return true;
}

/* =============================
   LOOP
============================= */

void lvgl_port_loop(void)
{
    /* LVGL tick is handled automatically via LV_TICK_CUSTOM in lv_conf.h */
    lv_timer_handler();
}

/* =============================
   TOUCH READ
============================= */

static void touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    uint16_t x, y;

    // Touch handling would go here if touch is available
    if (false) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = x;
        data->point.y = y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

#endif  // LV_VERSION_MAJOR
