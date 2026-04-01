#pragma once

/**
 * @brief Initialize ESP32_Display_Panel hardware and get device handles
 * 
 * This function must be called before lvgl_port_init_board() to ensure
 * the display hardware is properly initialized.
 * 
 * @param[out] lcd_handle   Pointer to receive esp_lcd_panel_handle_t
 *                          This handle is required for LVGL display output
 * @param[out] touch_handle Pointer to receive touch driver handle (reserved)
 * 
 * @return true if initialization successful, false on error
 */
bool board_init(void **lcd_handle, void **touch_handle);
