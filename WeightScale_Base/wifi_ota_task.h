#pragma once

#include <Arduino.h>
#include "config/app_config.h"
#include "wifi_service.h"  /* ⭐ REQUIRED: Defines wifi_state_t */

#if ENABLE_WIFI_SERVICE

/**
 * @file wifi_ota_task.h
 * @brief Core 1 dedicated WiFi/OTA worker task
 * 
 * This task runs on Core 1 and handles all WiFi operations asynchronously,
 * preventing UI freezes on Core 0. Communication via queues/callbacks.
 * 
 * Architecture:
 * - Core 0: app_controller, UI events, LVGL rendering
 * - Core 1: WiFi/OTA worker task (this file) + HX711 scale task
 * 
 * Benefits:
 * - Core 0 never blocks on WiFi operations
 * - WiFi has dedicated processing power
 * - UI remains responsive during WiFi operations
 */

typedef enum {
    WIFI_TASK_CMD_INIT,         /* Initialize WiFi */
    WIFI_TASK_CMD_START_SCAN,   /* Start WiFi network scan */
    WIFI_TASK_CMD_CANCEL_SCAN,  /* Cancel ongoing scan */
    WIFI_TASK_CMD_CONNECT,      /* Connect to network (ssid, pwd in payload) */
    WIFI_TASK_CMD_DISCONNECT,   /* Disconnect from network */
    WIFI_TASK_CMD_OTA_CHECK,    /* Check for OTA update */
    WIFI_TASK_CMD_SYNC,         /* Run one sync_service_loop() cycle on Core 1 */
} wifi_task_cmd_t;

/**
 * @brief Initialize and start the Core 1 WiFi/OTA worker task
 * 
 * Must be called from setup() or early in initialization.
 * Creates FreeRTOS task pinned to Core 1.
 */
void wifi_ota_task_init(void);

/**
 * @brief Enqueue a command for the WiFi/OTA task to process
 * 
 * @param cmd Command to execute
 * @param payload Optional payload (e.g., SSID+password for connect)
 * @param payload_size Size of payload in bytes
 * @return true if queued successfully, false if queue full
 */
bool wifi_ota_task_enqueue(wifi_task_cmd_t cmd, const void *payload, size_t payload_size);

/**
 * @brief Get current WiFi state (non-blocking read)
 * 
 * @return WiFi state enum (DISCONNECTED, CONNECTING, CONNECTED)
 */
wifi_state_t wifi_ota_task_get_state(void);

/**
 * @brief Get scan results from last completed scan
 * 
 * @param index Index of network (0 to count-1)
 * @return SSID string, or empty if index out of range
 */
String wifi_ota_task_get_ssid(uint8_t index);

/**
 * @brief Get count of networks found in last scan
 * 
 * @return Number of networks (-1 if scan in progress, >=0 if complete)
 */
int wifi_ota_task_get_scan_count(void);

/**
 * @brief Get RSSI for a cached scan result
 * 
 * @param index Index of network (0 to count-1)
 * @return RSSI in dBm, or -127 if index out of range
 */
int8_t wifi_ota_task_get_rssi(uint8_t index);

/**
 * @brief Register callback for WiFi state changes
 * 
 * @param cb Callback function, called when state changes
 */
void wifi_ota_task_register_state_callback(void (*cb)(wifi_state_t));

/**
 * @brief Check if WiFi task is currently busy (scan/connect/OTA)
 * 
 * @return true if busy, false if idle
 */
bool wifi_ota_task_is_busy(void);

/**
 * @brief Check if OTA is in progress (blocks scan/reconnect/sync)
 */
bool wifi_ota_task_is_ota_active(void);

#else

/* Stub implementations when WiFi is disabled */
inline void wifi_ota_task_init(void) {}
inline bool wifi_ota_task_enqueue(wifi_task_cmd_t cmd, const void *payload, size_t payload_size) { return false; }
inline wifi_state_t wifi_ota_task_get_state(void) { return WIFI_DISCONNECTED; }
inline String wifi_ota_task_get_ssid(uint8_t index) { return ""; }
inline int wifi_ota_task_get_scan_count(void) { return -2; }
inline int8_t wifi_ota_task_get_rssi(uint8_t index) { return -127; }
inline void wifi_ota_task_register_state_callback(void (*cb)(wifi_state_t)) {}
inline bool wifi_ota_task_is_busy(void) { return false; }
inline bool wifi_ota_task_is_ota_active(void) { return false; }

#endif
