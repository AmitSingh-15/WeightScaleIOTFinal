#define ARDUINO_USB_CDC_ON_BOOT 1
#define LV_CONF_INCLUDE_SIMPLE

#include <Arduino.h>
#include <lvgl.h>
#include "config/app_config.h"

#include "lvgl_v8_port.h"

#include "ui_styles.h"
#include "ui/ui_main.h"
#include "ui/ui_calibration.h"

#include "ui_events.h"
#include "services/scale_service.h"
#include "services/calibration_service.h"

#include "storage_service.h"
#include "invoice_service.h"
#include "invoice_session_service.h"
#include "wifi_service.h"
#include "sync_service.h"
#include "ota_service.h"
#include "devlog.h"

static int quantity = 1;

// Forward declare Board class to avoid including library headers
namespace esp_panel {
namespace board {
class Board;
}
}

// Global board pointer (stub for display initialization)
static esp_panel::board::Board *board = nullptr;

static const scale_profile_t PROFILE_1KG = { "RAW", 1.0f, 1.0f, 0.35f, 0.002f, 500 };
static const scale_profile_t PROFILE_100KG = { "1KG", 1.0f, 58281.3f, 0.35f, 0.002f, 500 };
static const scale_profile_t PROFILE_500KG = { "500KG", 1.0f, 2174.0f, 0.35f, 0.08f, 500 };

static void ui_event_handler(int evt)
{
    switch(evt)
    {
        case UI_EVT_SETTINGS:
            // placeholder: password guard etc
            break;
        case UI_EVT_HISTORY:
            /* expanded old history handling if needed */
            break;
        case UI_EVT_QTY_INC:
            quantity = max(1, quantity + 1);
            ui_main_set_quantity(quantity);
            break;
        case UI_EVT_QTY_DEC:
            quantity = max(1, quantity - 1);
            ui_main_set_quantity(quantity);
            break;
        case UI_EVT_SAVE:
            if(invoice_session_add(scale_service_get_weight(), quantity))
            {
                // update invoice screen logic from existing design
            }
            break;
        case UI_EVT_RESET:
            invoice_session_commit();
            invoice_service_next();
            ui_main_set_quantity(1);
            quantity = 1;
            break;
        case UI_EVT_RESET_ALL:
            invoice_session_clear();
            storage_clear_all_records();
            storage_save_invoice(1);
            invoice_service_init();
            break;
        case UI_EVT_CALIBRATE:
            ui_calibration_init(ui_event_handler);
            break;
        case UI_EVT_HOME:
            ui_main_init(ui_event_handler);
            break;
        default:
            if(evt >= UI_EVT_REMOVE_ITEM_BASE && evt < UI_EVT_REMOVE_ITEM_BASE + MAX_INVOICE_ITEMS)
            {
                int idx = evt - UI_EVT_REMOVE_ITEM_BASE;
                invoice_session_remove((uint8_t)idx);
            }
            break;
    }
}
