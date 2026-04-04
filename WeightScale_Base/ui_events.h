#pragma once

enum {
    UI_EVT_SETTINGS = 1,
    UI_EVT_HISTORY,
    UI_EVT_QTY_INC,
    UI_EVT_QTY_DEC,
    UI_EVT_RESET,
    UI_EVT_SAVE,
    UI_EVT_RESET_ALL,
    UI_EVT_CALIBRATE,
    UI_EVT_HOME,
    UI_EVT_WIFI_DIRECT = 20,       /* Home → WiFi list directly */
    UI_EVT_QTY_MUL2 = 1002,   /* Multiply by 2 */
    UI_EVT_QTY_MUL5 = 1005,   /* Multiply by 5 */
    UI_EVT_QTY_MUL10 = 1010   /* Multiply by 10 */
};

/* Remove item events: UI_EVT_REMOVE_ITEM_BASE + index */
#define UI_EVT_REMOVE_ITEM_BASE 3000
