#pragma once

#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>
#include <lvgl_v8_port.h>
#include <Arduino.h>

/* ✅ OFFICIAL LVGL Port Functions from ESP32_Display_Panel
 * 
 * The ESP32_Display_Panel library provides the official LVGL 8 port
 * with all rendering, buffer management, and task handling built-in.
 * 
 * Public API:
 * - lvgl_port_init(LCD*, Touch*)  : Initialize LVGL with display devices
 * - lvgl_port_deinit()            : Deinitialize and cleanup
 * - lvgl_port_lock(timeout_ms)    : Lock LVGL mutex  
 * - lvgl_port_unlock()            : Unlock LVGL mutex
 * 
 * Usage:
 *   auto lcd = g_board->getLCD();
 *   auto touch = g_board->getTouch();  
 *   lvgl_port_init(lcd, touch);
 *   // LVGL rendering now runs automatically in background task
 */

