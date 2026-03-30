#include "lvgl_port.h"
#include "display/gfx_conf.h"
#include "config/app_config.h"
#include "esp_heap_caps.h"

static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1 = nullptr;
static lv_color_t *buf2 = nullptr;
static uint32_t last_tick = 0;

extern LGFX tft;

static void flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    // LovyanGFX in RGB mode expects 16-bit color (RGB565) for pushImage.
    tft.writePixels((uint16_t *)color_p, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

static void touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    uint16_t x, y;

    if (tft.getTouch(&x, &y)) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = x;
        data->point.y = y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void lvgl_port_init(void)
{
    lv_init();
    tft.begin();

    size_t buf_area = screenWidth * LVGL_BUFFER_LINES;

    buf1 = (lv_color_t *)heap_caps_malloc(buf_area * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    buf2 = (lv_color_t *)heap_caps_malloc(buf_area * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!buf1 || !buf2) {
        Serial.println("[LVGL] PSRAM allocation failed");
        while (1) {
            delay(1000);
        }
    }

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buf_area);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read;
    lv_indev_drv_register(&indev_drv);

    last_tick = millis();

    Serial.println("[LVGL] Port initialized");
}

void lvgl_port_loop(void)
{
    uint32_t now = millis();
    if (now - last_tick >= 5) {
        lv_tick_inc(now - last_tick);
        last_tick = now;
    }
    lv_timer_handler();
}
