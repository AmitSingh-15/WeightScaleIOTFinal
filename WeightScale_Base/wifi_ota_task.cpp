#include "config/app_config.h"

#if ENABLE_WIFI_SERVICE

#include "wifi_ota_task.h"
#include "wifi_service.h"
#include "ota_service.h"
#include "sync_service.h"
#include "system_state.h"
#include "scale_service_v2.h"
#include "devlog.h"
#include <freertos/queue.h>
#include <freertos/event_groups.h>
#include <esp_task_wdt.h>
#include <lvgl.h>

/* ======================================================== */
/* FORWARD DECLARATION */
static void wifi_ota_worker_task(void *param);

/* ======================================================== */
/* STATE */
static TaskHandle_t wifi_ota_task_handle = nullptr;
static QueueHandle_t wifi_ota_cmd_queue = nullptr;
static EventGroupHandle_t wifi_ota_events = nullptr;

typedef struct {
    wifi_task_cmd_t cmd;
    uint8_t payload[128];
    size_t payload_size;
} wifi_task_message_t;

static wifi_state_t current_state = WIFI_DISCONNECTED;
static int scan_count = -2;
static char ssid_cache[WIFI_SCAN_MAX_APS][33];
static int8_t rssi_cache[WIFI_SCAN_MAX_APS];
static uint8_t cached_count = 0;

static void (*state_callback)(wifi_state_t) = nullptr;
static unsigned long g_last_ota_cmd_ms = 0;

/* ===== OTA ISOLATION (Req 3) ===== */
static volatile bool g_ota_in_progress = false;

/* ===== SCAN SPAM PREVENTION (Req 5) ===== */
static unsigned long g_last_scan_start_ms = 0;
static bool g_scan_in_progress = false;

/* ===== SYNC AUTO-SCHEDULING ===== */
static unsigned long g_last_sync_trigger_ms = 0;

/* ======================================================== */
/* AUTO INIT */
static bool wifi_ota_task_ensure_initialized()
{
    if (wifi_ota_cmd_queue && wifi_ota_task_handle) {
        return true;
    }

    devlog_printf("[WIFI_TASK] Auto-initializing");

    if (!wifi_ota_cmd_queue) {
        wifi_ota_cmd_queue = xQueueCreate(10, sizeof(wifi_task_message_t));
        if (!wifi_ota_cmd_queue) return false;
    }

    if (!wifi_ota_events) {
        wifi_ota_events = xEventGroupCreate();
        if (!wifi_ota_events) return false;
    }

    if (!wifi_ota_task_handle) {
        BaseType_t ret = xTaskCreatePinnedToCore(
            wifi_ota_worker_task,
            "WiFi/OTA",
            WIFI_OTA_TASK_STACK_SIZE,
            nullptr,
            WIFI_OTA_TASK_PRIORITY,
            &wifi_ota_task_handle,
            WIFI_OTA_TASK_CORE
        );
        if (ret != pdPASS) return false;
    }

    /* Send INIT */
    wifi_task_message_t msg = {};
    msg.cmd = WIFI_TASK_CMD_INIT;
    xQueueSend(wifi_ota_cmd_queue, &msg, pdMS_TO_TICKS(10));

    devlog_printf("[WIFI_TASK] Init done (stack=%d, prio=%d, core=%d)",
                  WIFI_OTA_TASK_STACK_SIZE, WIFI_OTA_TASK_PRIORITY, WIFI_OTA_TASK_CORE);
    return true;
}

/* ======================================================== */
/* OTA SAFE MODE ENTER/EXIT (Req 3)                         */
/* ======================================================== */
static void ota_enter_safe_mode(void)
{
    g_ota_in_progress = true;
    system_state_transition(SYS_OTA);

    /* 1. Pause LVGL rendering */
    lv_timer_enable(false);
    devlog_printf("[OTA-SAFE] LVGL timers paused");

    /* 2. Suspend HX711 scale task */
    scale_service_suspend();
    devlog_printf("[OTA-SAFE] Scale suspended");

    /* 3. Stop sync service */
    sync_service_pause();
    devlog_printf("[OTA-SAFE] Sync paused");

    /* 4. WiFi locked — g_ota_in_progress blocks scan/reconnect in this task */
    devlog_printf("[OTA-SAFE] ENTERED — all subsystems paused");
}

static void ota_exit_safe_mode(void)
{
    /* Resume in reverse order */
    sync_service_resume();
    devlog_printf("[OTA-SAFE] Sync resumed");

    scale_service_resume();
    devlog_printf("[OTA-SAFE] Scale resumed");

    /* Resume LVGL rendering */
    lv_timer_enable(true);
    devlog_printf("[OTA-SAFE] LVGL timers resumed");

    g_ota_in_progress = false;
    system_state_transition(SYS_IDLE);
    devlog_printf("[OTA-SAFE] EXITED — all subsystems resumed");
}

/* ======================================================== */
/* WORKER TASK (Req 1: ALL networking on Core 1)            */
/* ======================================================== */
static void wifi_ota_worker_task(void *param)
{
    devlog_printf("[WIFI_TASK] Worker started on core %d", xPortGetCoreID());

    /* Register with task watchdog (30s timeout) */
    esp_task_wdt_add(NULL);

    wifi_task_message_t msg;
    unsigned long idle_poll_ms = 20;

    while (true) {
        esp_task_wdt_reset();

        if (xQueueReceive(wifi_ota_cmd_queue, &msg, pdMS_TO_TICKS(idle_poll_ms))) {

            switch (msg.cmd) {

                case WIFI_TASK_CMD_INIT:
                    wifi_service_init();
                    break;

                /* ===== SCAN with cooldown + OTA guard (Req 3,4,5) ===== */
                case WIFI_TASK_CMD_START_SCAN:
                    if (g_ota_in_progress) {
                        devlog_printf("[WIFI_TASK] Scan blocked — OTA in progress");
                        break;
                    }
                    if (g_scan_in_progress) {
                        devlog_printf("[WIFI_TASK] Scan already running");
                        break;
                    }
                    {
                        /* payload[0]==1 means force scan (user opened WiFi page) */
                        bool force = (msg.payload_size >= 1 && msg.payload[0] == 1);
                        if (!force && (millis() - g_last_scan_start_ms) < WIFI_SCAN_COOLDOWN_MS) {
                            devlog_printf("[WIFI_TASK] Scan cooldown active (%lums left)",
                                          WIFI_SCAN_COOLDOWN_MS - (millis() - g_last_scan_start_ms));
                            break;
                        }
                    }
                    g_scan_in_progress = true;
                    g_last_scan_start_ms = millis();
                    scan_count = -1;  /* signal: scanning in progress */
                    wifi_service_start_scan();
                    idle_poll_ms = 20;
                    break;

                case WIFI_TASK_CMD_CANCEL_SCAN:
                    wifi_service_cancel_scan();
                    g_scan_in_progress = false;
                    break;

                /* ===== CONNECT with OTA guard (Req 3,4) ===== */
                case WIFI_TASK_CMD_CONNECT:
                {
                    if (g_ota_in_progress) {
                        devlog_printf("[WIFI_TASK] Connect blocked — OTA in progress");
                        break;
                    }
                    uint8_t ssid_len = msg.payload[0];
                    uint8_t pwd_len  = msg.payload[1];

                    char ssid[33] = {0};
                    char pwd[65] = {0};

                    memcpy(ssid, &msg.payload[2], ssid_len);
                    memcpy(pwd, &msg.payload[2 + ssid_len], pwd_len);

                    /* Update current_state immediately so UI polling sees
                       CONNECTING before the WiFi reinit delays (700ms) */
                    current_state = WIFI_CONNECTING;

                    wifi_service_connect(ssid, pwd);
                    idle_poll_ms = 20;
                    break;
                }

                case WIFI_TASK_CMD_DISCONNECT:
                    if (!g_ota_in_progress) {
                        wifi_service_disconnect();
                    }
                    break;

                /* ===== OTA with full safe mode (Req 3,9) ===== */
                case WIFI_TASK_CMD_OTA_CHECK:
                    if (g_ota_in_progress) {
                        devlog_printf("[WIFI_TASK] OTA already active");
                        break;
                    }
                    if (!system_state_allow_ota()) {
                        devlog_printf("[WIFI_TASK] OTA blocked by system state (%s)",
                                      system_state_name(system_state_get()));
                        break;
                    }
                    if ((millis() - g_last_ota_cmd_ms) < OTA_CMD_COOLDOWN_MS) {
                        devlog_printf("[WIFI_TASK] OTA cooldown, skipping");
                        break;
                    }
                    g_last_ota_cmd_ms = millis();

                    /* Enter safe mode BEFORE OTA */
                    ota_enter_safe_mode();

                    /* Disable watchdog — OTA can take minutes */
                    esp_task_wdt_delete(NULL);

                    ota_service_check_and_update();

                    /* Re-enable watchdog */
                    esp_task_wdt_add(NULL);

                    /* Exit safe mode AFTER OTA (success or fail, ESP.restart() won't reach here on success) */
                    ota_exit_safe_mode();
                    break;

                /* ===== SYNC on Core 1 (Req 2) ===== */
                case WIFI_TASK_CMD_SYNC:
                    if (g_ota_in_progress) {
                        devlog_printf("[WIFI_TASK] Sync blocked — OTA active");
                        break;
                    }
                    if (!system_state_allow_sync()) {
                        break;
                    }
#if ENABLE_CLOUD_SYNC
                    sync_service_loop();
#endif
                    break;
            }
        }

        /* ===== Run WiFi loop (Req 1: on Core 1) ===== */
        if (!g_ota_in_progress) {
            wifi_service_loop();
        }

        /* ===== AUTO-SYNC scheduling (Req 2) ===== */
        /* Instead of Core 0 calling sync_service_loop(), we auto-trigger it here */
#if ENABLE_CLOUD_SYNC
        if (!g_ota_in_progress &&
            system_state_allow_sync() &&
            current_state == WIFI_CONNECTED &&
            (millis() - g_last_sync_trigger_ms) >= SYNC_INTERVAL_MS)
        {
            g_last_sync_trigger_ms = millis();
            sync_service_loop();
        }
#endif

        /* ===== SCAN CACHE (with RSSI) ===== */
        int scan_status = wifi_service_scan_status();
        if (scan_status >= 0 && scan_count != scan_status) {
            scan_count = scan_status;
            cached_count = (scan_status > WIFI_SCAN_MAX_APS) ? WIFI_SCAN_MAX_APS : (uint8_t)scan_status;

            for (uint8_t i = 0; i < cached_count; i++) {
                String s = wifi_service_get_ssid(i);
                strncpy(ssid_cache[i], s.c_str(), sizeof(ssid_cache[i]) - 1);
                ssid_cache[i][32] = 0;
                rssi_cache[i] = wifi_service_get_rssi(i);
            }
            g_scan_in_progress = false;  /* scan finished */
        }

        /* ===== STATE TRACKING ===== */
        wifi_state_t new_state = wifi_service_state();
        if (new_state != current_state) {
            current_state = new_state;
            if (state_callback) {
                state_callback(current_state);
            }
        }

        /* ===== ADAPTIVE POLLING ===== */
        if (current_state == WIFI_CONNECTED &&
            !g_scan_in_progress &&
            !g_ota_in_progress) {
            idle_poll_ms = 100;  /* Slow poll when stable */
        } else {
            idle_poll_ms = 20;   /* Fast poll during active operations */
        }

        vTaskDelay(pdMS_TO_TICKS(idle_poll_ms));
    }
}

/* ======================================================== */
/* PUBLIC API */

void wifi_ota_task_init(void)
{
    wifi_ota_task_ensure_initialized();
}

bool wifi_ota_task_enqueue(wifi_task_cmd_t cmd, const void *payload, size_t payload_size)
{
    if (!wifi_ota_task_ensure_initialized()) {
        devlog_printf("[WIFI_TASK] Init failed");
        return false;
    }

    if (payload_size > sizeof(((wifi_task_message_t *)0)->payload)) {
        return false;
    }

    wifi_task_message_t msg = {};
    msg.cmd = cmd;
    msg.payload_size = payload_size;

    if (payload && payload_size > 0) {
        memcpy(msg.payload, payload, payload_size);
    }

    if (xQueueSend(wifi_ota_cmd_queue, &msg, pdMS_TO_TICKS(20)) != pdPASS) {
        devlog_printf("[WIFI_TASK] Queue full");
        return false;
    }

    return true;
}

wifi_state_t wifi_ota_task_get_state(void)
{
    return current_state;
}

int wifi_ota_task_get_scan_count(void)
{
    return scan_count;
}

String wifi_ota_task_get_ssid(uint8_t index)
{
    if (index >= cached_count) return "";
    return String(ssid_cache[index]);
}

int8_t wifi_ota_task_get_rssi(uint8_t index)
{
    if (index >= cached_count) return -127;
    return rssi_cache[index];
}

void wifi_ota_task_register_state_callback(void (*cb)(wifi_state_t))
{
    state_callback = cb;
}

bool wifi_ota_task_is_busy(void)
{
    return g_scan_in_progress ||
           (wifi_service_state() == WIFI_CONNECTING) ||
           g_ota_in_progress;
}

bool wifi_ota_task_is_ota_active(void)
{
    return g_ota_in_progress;
}

#endif