/*
 * LVGL Port Function Stubs
 * Provides missing ESP32 display panel integration functions
 */

#define LV_CONF_INCLUDE_SIMPLE 1
#include "lvgl.h"

/* ============================================================
   LVGL PORT STUBS - Required functions not in LVGL 8.3.11
   ============================================================ */

bool lvgl_port_init(void *lcd_handle, void *touch_handle)
{
    /* Stub: basic initialization */
    (void)lcd_handle;
    (void)touch_handle;
    return true;
}

void lvgl_port_loop(void)
{
    /* Stub: no-op for now */
}

void lvgl_port_lock(void)
{
    /* Stub: no thread safety for now */
}

void lvgl_port_unlock(void)
{
    /* Stub: no thread safety for now */
}

/* Note: Board and USBSerial implementations are provided by:
   - esp_panel_board_wrapper.cpp (for esp_panel::board::Board)
   - Arduino core (for USBSerial)
   
   If linking still fails for these symbols, ensure:
   1. ESP32_Display_Panel library is properly installed
   2. Arduino board support for ESP32 is correctly configured
   3. The selected board has USB CDC support enabled
*/
