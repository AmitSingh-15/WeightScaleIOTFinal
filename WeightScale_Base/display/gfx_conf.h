#pragma once

#include "config/app_config.h"

#include <Arduino.h>
#include <driver/i2c.h>

#ifdef pinMode
#undef pinMode
#endif

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32/Bus_RGB.hpp>
#include <lgfx/v1/Touch_GT911.hpp>

#define screenWidth   DISPLAY_WIDTH
#define screenHeight  DISPLAY_HEIGHT

class LGFX : public lgfx::LGFX_Device
{
public:
    lgfx::Bus_RGB     bus;
    lgfx::Panel_RGB   panel;
    lgfx::Light_PWM   backlight;
    lgfx::Touch_GT911 touch;

    LGFX()
    {
        // Panel configuration
        {
            auto cfg = panel.config();
            cfg.memory_width  = screenWidth;
            cfg.memory_height = screenHeight;
            cfg.panel_width   = screenWidth;
            cfg.panel_height  = screenHeight;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            panel.config(cfg);
        }

        // RGB bus configuration
        {
            auto cfg = bus.config();
            cfg.panel = &panel;

            // CrowPanel 10.1 typical parallel pin assignment on ESP32-P4.
            // Adapt if manufacturer pinmap differs.
            cfg.pin_d0  = GPIO_NUM_15;
            cfg.pin_d1  = GPIO_NUM_7;
            cfg.pin_d2  = GPIO_NUM_6;
            cfg.pin_d3  = GPIO_NUM_5;
            cfg.pin_d4  = GPIO_NUM_4;
            cfg.pin_d5  = GPIO_NUM_9;
            cfg.pin_d6  = GPIO_NUM_46;
            cfg.pin_d7  = GPIO_NUM_3;
            cfg.pin_d8  = GPIO_NUM_8;
            cfg.pin_d9  = GPIO_NUM_16;
            cfg.pin_d10 = GPIO_NUM_1;
            cfg.pin_d11 = GPIO_NUM_14;
            cfg.pin_d12 = GPIO_NUM_21;
            cfg.pin_d13 = GPIO_NUM_47;
            cfg.pin_d14 = GPIO_NUM_48;
            cfg.pin_d15 = GPIO_NUM_45;

            cfg.pin_hsync = GPIO_NUM_39;
            cfg.pin_vsync = GPIO_NUM_40;
            cfg.pin_pclk  = GPIO_NUM_0;
            cfg.pin_de   = GPIO_NUM_42;
            cfg.pin_henable = GPIO_NUM_41;

            cfg.freq_write = 30000000; // 30MHz parallel bus

            cfg.hsync_front_porch = 40;
            cfg.hsync_pulse_width = 48;
            cfg.hsync_back_porch  = 40;
            cfg.vsync_front_porch = 1;
            cfg.vsync_pulse_width = 3;
            cfg.vsync_back_porch  = 13;

            cfg.pclk_active_neg = 0;
            cfg.de_idle_high    = 0;
            cfg.pclk_idle_high  = 0;

            bus.config(cfg);
            panel.setBus(&bus);
        }

        // Backlight
        {
            auto cfg = backlight.config();
            cfg.pin_bl = GPIO_NUM_2;
            backlight.config(cfg);
            panel.light(&backlight);
        }

        // Touch
        {
            auto cfg = touch.config();
            cfg.x_min = 0;
            cfg.x_max = screenWidth - 1;
            cfg.y_min = 0;
            cfg.y_max = screenHeight - 1;
            cfg.i2c_port = GT911_I2C_PORT;
            cfg.pin_sda = GT911_SDA_PIN;
            cfg.pin_scl = GT911_SCL_PIN;
            cfg.i2c_addr = GT911_ADDR;
            cfg.freq = 400000;
            cfg.bus_shared = true;
            cfg.pin_int = -1;
            cfg.pin_rst = -1;
            cfg.offset_rotation = 0;
            touch.config(cfg);
            panel.setTouch(&touch);
        }

        setPanel(&panel);
    }
};

extern LGFX tft;
