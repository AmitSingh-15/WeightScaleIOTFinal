#pragma once

enum {
    UI_EVT_SETTINGS = 1,
    UI_EVT_HISTORY,
    UI_EVT_QTY_CHANGED,    /* Keypad set a new quantity value */
    UI_EVT_RESET = 5,
    UI_EVT_SAVE,
    UI_EVT_RESET_ALL,
    UI_EVT_CALIBRATE,
    UI_EVT_HOME,
    UI_EVT_WIFI_DIRECT = 20,       /* Home → WiFi list directly */
    UI_EVT_RESTART = 21            /* Restart device */
};

/* Remove item events: UI_EVT_REMOVE_ITEM_BASE + index */
#define UI_EVT_REMOVE_ITEM_BASE 3000
