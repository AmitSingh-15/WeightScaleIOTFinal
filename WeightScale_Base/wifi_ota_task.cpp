#include "config/app_config.h"

#if ENABLE_WIFI_SERVICE

#include "wifi_ota_task.h"
#include "wifi_service.h"
#include "ota_service.h"
#include "devlog.h"
#include <freertos/queue.h>
#include <freertos/event_groups.h>

/* ========================================================
   TASK STATE & SYNCHRONIZATION
   ======================================================== */

static TaskHandle_t wifi_ota_task_handle = nullptr;
static QueueHandle_t wifi_ota_cmd_queue = nullptr;
static EventGroupHandle_t wifi_ota_events = nullptr;

#define WIFI_OTA_EVENT_IDLE         (1 << 0)  /* Task waiting for commands */
#define WIFI_OTA_EVENT_SCAN_DONE    (1 << 1)  /* Scan completed */
#define WIFI_OTA_EVENT_CONNECT_DONE (1 << 2)  /* Connect attempt completed */
#define WIFI_OTA_EVENT_OTA_DONE     (1 << 3)  /* OTA attempt completed */

/* Command queue structure */
typedef struct {
    wifi_task_cmd_t cmd;
    uint8_t payload[128];  /* Flexible payload for different commands */
    size_t payload_size;
} wifi_task_message_t;

/* State tracking */
static wifi_state_t current_state = WIFI_DISCONNECTED;
static int scan_count = -2;  /* -2 = not started, -1 = in progress, >=0 = count */
static char ssid_cache[20][33];  /* Cache last 20 SSIDs from scan */
static uint8_t cached_count = 0;

static void (*state_callback)(wifi_state_t) = nullptr;

/* ========================================================
   CORE 1 WORKER TASK
   ======================================================== */

static void wifi_ota_worker_task(void *param)
{
    devlog_printf("[WIFI_TASK] Core 1 WiFi/OTA worker started");
    
    wifi_task_message_t msg;
    const TickType_t queue_wait = pdMS_TO_TICKS(100);  /* 100ms timeout */
    
    while (true) {
        /* Wait for command or timeout */
        if (xQueueReceive(wifi_ota_cmd_queue, &msg, queue_wait)) {
            /* Command received */
            switch (msg.cmd) {
                case WIFI_TASK_CMD_INIT:
                    devlog_printf("[WIFI_TASK] Executing INIT");
                    wifi_service_init();
                    xEventGroupSetBits(wifi_ota_events, WIFI_OTA_EVENT_IDLE);
                    break;
                    
                case WIFI_TASK_CMD_START_SCAN:
                    devlog_printf("[WIFI_TASK] Executing START_SCAN");
                    wifi_service_start_scan();
                    break;
                    
                case WIFI_TASK_CMD_CANCEL_SCAN:
                    devlog_printf("[WIFI_TASK] Executing CANCEL_SCAN");
                    wifi_service_cancel_scan();
                    xEventGroupSetBits(wifi_ota_events, WIFI_OTA_EVENT_SCAN_DONE);
                    break;
                    
                case WIFI_TASK_CMD_CONNECT:
                {
                    /* Extract SSID and password from payload */
                    if (msg.payload_size >= 2) {
                        uint8_t ssid_len = msg.payload[0];
                        uint8_t pwd_len = msg.payload[1];
                        
                        if (ssid_len <= 32 && pwd_len <= 64 && 
                            (2 + ssid_len + pwd_len) <= msg.payload_size) {
                            
                            char ssid[33] = {0};
                            char pwd[65] = {0};
                            memcpy(ssid, &msg.payload[2], ssid_len);
                            memcpy(pwd, &msg.payload[2 + ssid_len], pwd_len);
                            
                            devlog_printf("[WIFI_TASK] Executing CONNECT to '%s'", ssid);
                            wifi_service_connect(ssid, pwd);
                        }
                    }
                    break;
                }
                    
                case WIFI_TASK_CMD_DISCONNECT:
                    devlog_printf("[WIFI_TASK] Executing DISCONNECT");
                    wifi_service_disconnect();
                    xEventGroupSetBits(wifi_ota_events, WIFI_OTA_EVENT_CONNECT_DONE);
                    break;
                    
                case WIFI_TASK_CMD_OTA_CHECK:
                    devlog_printf("[WIFI_TASK] Executing OTA_CHECK");
                    ota_service_check_and_update();
                    xEventGroupSetBits(wifi_ota_events, WIFI_OTA_EVENT_OTA_DONE);
                    break;
                    
                default:
                    devlog_printf("[WIFI_TASK] Unknown command: %d", msg.cmd);
                    break;
            }
        }
        
        /* Poll WiFi state (happens every ~100ms when no command) */
        wifi_service_loop();
        
        /* Check if scan completed and cache results */
        int scan_status = wifi_service_scan_status();
        if (scan_status >= 0 && scan_count != scan_status) {
            /* Scan results changed — cache them */
            scan_count = scan_status;
            cached_count = (uint8_t)scan_status;
            for (uint8_t i = 0; i < cached_count && i < 20; i++) {
                String s = wifi_service_get_ssid(i);
                strncpy(ssid_cache[i], s.c_str(), sizeof(ssid_cache[i]) - 1);
                ssid_cache[i][32] = 0;
            }
            if (scan_count >= 0) {
                xEventGroupSetBits(wifi_ota_events, WIFI_OTA_EVENT_SCAN_DONE);
            }
        }
        
        /* Check if WiFi state changed */
        wifi_state_t new_state = wifi_service_state();
        if (new_state != current_state) {
            current_state = new_state;
            devlog_printf("[WIFI_TASK] WiFi state changed to %d", new_state);
            if (state_callback) {
                state_callback(current_state);
            }
            if (new_state == WIFI_CONNECTED || new_state == WIFI_DISCONNECTED) {
                xEventGroupSetBits(wifi_ota_events, WIFI_OTA_EVENT_CONNECT_DONE);
            }
        }
        
        /* Periodically yield to other tasks on Core 1 (LVGL, scale) */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ========================================================
   PUBLIC API
   ======================================================== */

void wifi_ota_task_init(void)
{
    devlog_printf("[WIFI_TASK] Initializing Core 1 WiFi/OTA task");
    
    /* Create command queue (max 10 messages) */
    wifi_ota_cmd_queue = xQueueCreate(10, sizeof(wifi_task_message_t));
    if (!wifi_ota_cmd_queue) {
        devlog_printf("[WIFI_TASK] ⚠ Failed to create command queue");
        return;
    }
    
    /* Create event group for synchronization */
    wifi_ota_events = xEventGroupCreate();
    if (!wifi_ota_events) {
        devlog_printf("[WIFI_TASK] ⚠ Failed to create event group");
        return;
    }
    
    /* Create task pinned to Core 1 */
    BaseType_t ret = xTaskCreatePinnedToCore(
        wifi_ota_worker_task,
        "WiFi/OTA",
        4096,  /* Stack size: 4KB (WiFi needs some stack) */
        nullptr,
        2,  /* Priority: medium (above HX711 scale task) */
        &wifi_ota_task_handle,
        1  /* Core 1 */
    );
    
    if (ret != pdPASS) {
        devlog_printf("[WIFI_TASK] ⚠ Failed to create task");
        return;
    }
    
    devlog_printf("[WIFI_TASK] ✓ Core 1 WiFi/OTA task created");
    
    /* Initialize WiFi service (queued command) */
    wifi_ota_task_enqueue(WIFI_TASK_CMD_INIT, nullptr, 0);
}

bool wifi_ota_task_enqueue(wifi_task_cmd_t cmd, const void *payload, size_t payload_size)
{
    if (!wifi_ota_cmd_queue) {
        devlog_printf("[WIFI_TASK] ⚠ Queue not initialized");
        return false;
    }
    
    if (payload_size > sizeof(((wifi_task_message_t *)0)->payload)) {
        devlog_printf("[WIFI_TASK] ⚠ Payload too large: %u bytes", (unsigned)payload_size);
        return false;
    }
    
    wifi_task_message_t msg;
    msg.cmd = cmd;
    msg.payload_size = payload_size;
    if (payload_size > 0 && payload) {
        memcpy(msg.payload, payload, payload_size);
    }
    
    BaseType_t ret = xQueueSend(wifi_ota_cmd_queue, &msg, pdMS_TO_TICKS(10));
    if (ret != pdPASS) {
        devlog_printf("[WIFI_TASK] ⚠ Queue full, command dropped");
        return false;
    }
    
    return true;
}

wifi_state_t wifi_ota_task_get_state(void)
{
    return current_state;
}

String wifi_ota_task_get_ssid(uint8_t index)
{
    if (index >= cached_count) {
        return "";
    }
    return String(ssid_cache[index]);
}

int wifi_ota_task_get_scan_count(void)
{
    return scan_count;
}

void wifi_ota_task_register_state_callback(void (*cb)(wifi_state_t))
{
    state_callback = cb;
}

bool wifi_ota_task_is_busy(void)
{
    /* Task is busy if scan in progress or connecting */
    return (wifi_service_state() == WIFI_CONNECTING) || 
           (wifi_service_scan_status() == -1) ||
           ota_service_is_busy();
}

#endif  /* ENABLE_WIFI_SERVICE */
