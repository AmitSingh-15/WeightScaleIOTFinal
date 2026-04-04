#include "lvgl_port.h"
#include "display/gfx_conf.h"
#include "config/app_config.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"

/* LVGL Buffers */
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1 = nullptr;
static lv_color_t *buf2 = nullptr;

/* Drivers */
static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;

static uint32_t last_tick = 0;

/* 🔴 IMPORTANT: Use panel handle from ESP32_Display_Panel */
extern esp_lcd_panel_handle_t lcd_handle;

/* ================= FLUSH CALLBACK ================= */
static void flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    int x1 = area->x1;
    int y1 = area->y1;
    int x2 = area->x2;
    int y2 = area->y2;

    /* Bounds safety */
    if (x2 >= screenWidth)  x2 = screenWidth - 1;
    if (y2 >= screenHeight) y2 = screenHeight - 1;

    /* 🔥 Correct API for RGB panel */
    esp_lcd_panel_draw_bitmap(
        lcd_handle,
        x1,
        y1,
        x2 + 1,
        y2 + 1,
        color_p
    );

    lv_disp_flush_ready(disp);
}

/* ================= TOUCH (WITH DEBUG) ================= */
static void touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    /* NOTE: This callback is called by LVGL periodically.
     * Official ESP32_Display_Panel should handle touch data feeding
     * once touch device handle is properly provided to lvgl_port_init.
     * 
     * If no touch events appear, check:
     * 1. Are pins 45 (SDA) & 46 (SCL) correct?
     * 2. Is touch device handle passed to init?
     * 3. Can GT911 be detected on I2C_NUM_1?
     */
    
    static uint32_t touch_calls = 0;
    touch_calls++;
    
    /* Log sampling to avoid spam */
    if (touch_calls % 100 == 0) {
        Serial.printf("[LVGL_INPUT] touch_read called (poll #%lu)\n", touch_calls);
        Serial.flush();
    }
    
    /* Return released state (official port feeds actual touch data) */
    if (data) {
        data->state = LV_INDEV_STATE_REL;
    }
}

/* ================= INIT ================= */
void lvgl_port_init(void)
{
    lv_init();

    size_t buf_area = screenWidth * LVGL_BUFFER_LINES;

    /* 🔴 CRITICAL: DMA-capable memory ONLY */
    buf1 = (lv_color_t *)heap_caps_malloc(
        buf_area * sizeof(lv_color_t),
        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL
    );

    /* 🔴 Start SINGLE buffer (more stable) */
    buf2 = NULL;

    if (!buf1) {
        Serial.println("[LVGL] Buffer allocation failed");
        while (1) delay(1000);
    }

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buf_area);

    Serial.printf("[LVGL] Buffer: %u pixels (%u x %u)\n",
                  (uint32_t)buf_area, screenWidth, (uint32_t)LVGL_BUFFER_LINES);

    /* Display driver */
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = flush_cb;
    disp_drv.draw_buf = &draw_buf;

    lv_disp_drv_register(&disp_drv);

    /* Input driver */
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read;
    lv_indev_drv_register(&indev_drv);

    last_tick = millis();

    Serial.println("[LVGL] Port initialized");
}

/* ================= LOOP ================= */
void lvgl_port_loop(void)
{
    uint32_t now = millis();

    if (now - last_tick >= 5) {
        lv_tick_inc(now - last_tick);
        last_tick = now;
    }

    lv_timer_handler();
}