#include "app/app_controller.h"
#include "config/app_config.h"  // Ensure app_config.h is included first
#include "ui_events.h"
#include "devlog.h"
#include <math.h>
#include <esp_heap_caps.h>
#include <esp_random.h>
#include <Preferences.h>

/* ========================================================
   SERVICE INCLUDES - Conditionally compiled based on app_config.h
   ======================================================== */

// Always available
#include "scale_service_v2.h"
#include "invoice_session_service.h"
#include "storage_service.h"
#include "ui_styles.h"

// Screen includes for UI event handling
#include "home_screen.h"
#include "settings_screen.h"
#include "history_screen.h"
#include "calibration_screen.h"
#include "calibration_wizard_screen.h"

// Conditionally available based on app_config.h
#if ENABLE_WIFI_SERVICE
  #include "wifi_service.h"
  #include "wifi_list_screen.h"
  #include "ota_service.h"
#endif

#if ENABLE_CLOUD_SYNC
  #include "sync_service.h"
#endif

/* ========================================================
   REGISTERED CALLBACKS - UI registers these
   ======================================================== */

static ui_sync_status_cb_t g_sync_status_cb = NULL;
static ui_weight_update_cb_t g_weight_update_cb = NULL;
static ui_calibration_cb_t g_calibration_cb = NULL;
static ui_invoice_cb_t g_invoice_cb = NULL;
static ui_device_name_cb_t g_device_name_cb = NULL;

/* ========================================================
   INTERNAL STATE
   ======================================================== */

static bool g_initialized = false;
static float g_last_weight = 0.0f;
static bool g_wifi_safe = true;       // false when crash loop detected
static bool g_crash_counter_cleared = false;

/* ===== MANUAL WEIGHT OFFSET ===== */
static float g_manual_offset = 0.0f;   // +/- kg added to non-zero readings

/* ===== TEST MODE ===== */
static bool g_test_mode = false;
static unsigned long g_test_weight_ms = 0;
static float g_test_weight_val = 0.0f;

/* ===== STABILITY DETECTION STATE ===== */
typedef enum {
    WEIGHT_WAIT_FOR_LOAD = 0,
    WEIGHT_WAIT_FOR_STABLE,
    WEIGHT_WAIT_FOR_REMOVE
} weight_flow_state_t;

static weight_flow_state_t g_weight_flow_state = WEIGHT_WAIT_FOR_LOAD;
static float g_stable_weight = 0.0f;
static unsigned long g_stable_start = 0;

/* Forward declaration */
static void app_controller_notify_invoice_update(void);
static void app_controller_restore_home_screen(void);

#if ENABLE_WIFI_SERVICE
static bool g_wifi_connected = false;
#endif

/* ========================================================
   INITIALIZATION
   ======================================================== */

void app_controller_init(void)
{
    if (g_initialized) {
        devlog_printf("[CTRL] Already initialized");
        return;
    }

    devlog_printf("[CTRL] ✓ Initializing app controller");

    /* ===== CRASH LOOP PROTECTION ===== */
    {
        Preferences boot_prefs;
        boot_prefs.begin("boot", false);
        uint8_t crash_count = boot_prefs.getUChar("crashes", 0);
        crash_count++;
        boot_prefs.putUChar("crashes", crash_count);
        boot_prefs.end();
        devlog_printf("[CTRL] Boot crash counter: %d", crash_count);
        if (crash_count > 3) {
            g_wifi_safe = false;
            devlog_printf("[CTRL] ⚠ Crash loop detected (%d crashes). WiFi DISABLED.", crash_count);
            devlog_printf("[CTRL] Power-cycle the device to re-enable WiFi.");
        }
    }

    // Always init these services
    storage_service_init();     // Must be first — other services load from NVS
    scale_service_init();
    devlog_printf("[CTRL] ✓ Scale service initialized");

    /* Load saved calibration profile */
    {
        int cal_idx = storage_load_active_cal_index();
        if(cal_idx >= 0) {
            cal_profile_t cp;
            if(storage_load_cal_profile(cal_idx, &cp)) {
                scale_service_set_cal_profile(&cp);
                devlog_printf("[CTRL] ✓ Cal profile %d loaded: %s", cal_idx, cp.name);
            }
        }
    }

    invoice_session_init();
    invoice_service_init();
    devlog_printf("[CTRL] \u2713 Invoice service initialized (invoice #%lu)",
                  invoice_service_current_id());
    /* Load manual weight offset */
    g_manual_offset = storage_load_offset();
    if (g_manual_offset != 0.0f)
        devlog_printf("[CTRL] Manual offset loaded: %.2f kg", g_manual_offset);
#if ENABLE_WIFI_SERVICE
    if (g_wifi_safe) {
        devlog_printf("[CTRL] Heap before WiFi: %lu free, %lu largest",
                      (unsigned long)esp_get_free_heap_size(),
                      (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
        wifi_service_init();
        devlog_printf("[CTRL] Heap after WiFi: %lu free, %lu largest",
                      (unsigned long)esp_get_free_heap_size(),
                      (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
        // Register our internal WiFi callback
        wifi_service_register_state_callback([](wifi_state_t state) {
            g_wifi_connected = (state == WIFI_CONNECTED);
            const char *status;
            if(state == WIFI_CONNECTED)       status = "Online";
            else if(state == WIFI_CONNECTING) status = "Connecting...";
            else                              status = "Offline";
            home_screen_set_sync_status(status);
            if (g_sync_status_cb) {
                g_sync_status_cb(status);
            }
            devlog_printf("[CTRL] WiFi state: %s", status);
        });
        devlog_printf("[CTRL] ✓ WiFi service initialized");
    } else {
        devlog_printf("[CTRL] ⊘ WiFi skipped (crash loop protection)");
        home_screen_set_sync_status("Safe Mode");
    }
#else
    devlog_printf("[CTRL] ⊘ WiFi service DISABLED (app_config.h)");
#endif

#if ENABLE_OTA_UPDATES
    if (g_wifi_safe) {
        ota_service_init();
        devlog_printf("[CTRL] \u2713 OTA service initialized");
    }
#endif

#if ENABLE_CLOUD_SYNC
    if (g_wifi_safe) {
        devlog_printf("[CTRL] Heap before Sync init: %lu free, %lu internal",
                      (unsigned long)esp_get_free_heap_size(),
                      (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
        sync_service_init();
        devlog_printf("[CTRL] \u2713 Sync service initialized");
    }
#endif

    /* Load device name from NVS and display on home screen */
    {
        char dev_name[64] = {0};
        if(storage_load_device_name(dev_name, sizeof(dev_name))) {
            home_screen_set_device(dev_name);
            devlog_printf("[CTRL] Device name loaded: %s", dev_name);
        }
    }

    /* Apply stored display theme (light/dark) */
    if(storage_load_light_mode())
        ui_styles_set_theme(true);

    devlog_printf("[CTRL] ✓ All services initialized");
    g_initialized = true;
}

/* ========================================================
   MAIN LOOP - call from loop()
   ======================================================== */

void app_controller_loop(void)
{
    if (!g_initialized) return;

    /* Clear crash counter after 30s of stable running */
    if (!g_crash_counter_cleared && millis() > 30000) {
        g_crash_counter_cleared = true;
        Preferences boot_prefs;
        boot_prefs.begin("boot", false);
        boot_prefs.putUChar("crashes", 0);
        boot_prefs.end();
        devlog_printf("[CTRL] Crash counter cleared (stable boot)");
    }

#if ENABLE_WIFI_SERVICE
    if (g_wifi_safe) wifi_service_loop();
#endif

#if ENABLE_CLOUD_SYNC
    if (g_wifi_safe) sync_service_loop();
#endif

    if (g_test_mode) {
        unsigned long now = millis();
        if (g_test_weight_ms == 0 || (now - g_test_weight_ms) >= 1000) {
            g_test_weight_ms = now;
            uint32_t r = esp_random() % 2500U;  /* 0.00 .. 24.99 kg */
            g_test_weight_val = (float)r / 100.0f;
            if (g_test_weight_val < 0.10f) g_test_weight_val = 0.0f;
        }
    }

    // ===== AUTO ADD ITEM LOGIC =====
    float w = g_test_mode ? g_test_weight_val : scale_service_get_weight();
    if (w > 0.001f && g_manual_offset != 0.0f) {
        w += g_manual_offset;
        if (w < 0.0f) w = 0.0f;
    }
    if (w != g_last_weight) {
        g_last_weight = w;
        if (g_weight_update_cb) g_weight_update_cb(w);
    }

    if (w < 0.05f) {
        if (g_weight_flow_state != WEIGHT_WAIT_FOR_LOAD) {
            g_weight_flow_state = WEIGHT_WAIT_FOR_LOAD;
            g_stable_weight = 0.0f;
            g_stable_start = 0;
            devlog_printf("[FLOW] Weight removed, ready for next item");
        }
    } else {
        switch (g_weight_flow_state) {
            case WEIGHT_WAIT_FOR_LOAD:
                g_weight_flow_state = WEIGHT_WAIT_FOR_STABLE;
                g_stable_weight = w;
                g_stable_start = millis();
                break;

            case WEIGHT_WAIT_FOR_STABLE:
                if (fabsf(w - g_stable_weight) >= 0.02f) {
                    g_stable_weight = w;
                    g_stable_start = millis();
                } else if (g_stable_start != 0 &&
                           (millis() - g_stable_start) > 600) {
                    invoice_session_add_weight_entry(w);
                    invoice_session_set_selected_qty(1);
                    app_controller_notify_invoice_update();
                    g_weight_flow_state = WEIGHT_WAIT_FOR_REMOVE;
                    devlog_printf("[FLOW] Auto-added stable item: %.2f kg", w);
                }
                break;

            case WEIGHT_WAIT_FOR_REMOVE:
                /* One item per placement. Ignore further stability until scale returns to zero. */
                break;
        }
    }
}

/* ========================================================
   REGISTER UI CALLBACKS
   ======================================================== */

void app_controller_register_sync_status_callback(ui_sync_status_cb_t cb)
{
    g_sync_status_cb = cb;
    devlog_printf("[CTRL] Sync status callback registered");
}

void app_controller_register_weight_update_callback(ui_weight_update_cb_t cb)
{
    g_weight_update_cb = cb;
    devlog_printf("[CTRL] Weight update callback registered");
}

void app_controller_register_calibration_callback(ui_calibration_cb_t cb)
{
    g_calibration_cb = cb;
    devlog_printf("[CTRL] Calibration callback registered");
}

void app_controller_register_invoice_callback(ui_invoice_cb_t cb)
{
    g_invoice_cb = cb;
    devlog_printf("[CTRL] Invoice callback registered");
}

void app_controller_register_device_name_callback(ui_device_name_cb_t cb)
{
    g_device_name_cb = cb;
    devlog_printf("[CTRL] Device name callback registered");
}

/* ========================================================
   INTERNAL HELPERS  
   ======================================================== */

static void app_controller_notify_invoice_update(void)
{
    /* Update qty display on home screen */
    home_screen_set_quantity(invoice_session_get_selected_qty());

    /* Refresh invoice item list */
    home_screen_refresh_invoice_details();

    if (g_invoice_cb) {
        g_invoice_cb(invoice_session_get_summary());
    }
}

/* Restore the home screen with all callbacks properly wired */
static void app_controller_restore_home_screen(void)
{
    scale_service_resume();
    lv_obj_t *home_scr = lv_obj_create(NULL);
    home_screen_create(home_scr);
    home_screen_register_callback([](int evt) {
        app_controller_handle_ui_event(evt);
    });
    lv_scr_load(home_scr);

    /* Restore current state on the new home screen */
    home_screen_set_weight(g_last_weight);
    home_screen_set_quantity(invoice_session_get_selected_qty());
    home_screen_set_invoice(invoice_service_current_id());
    home_screen_refresh_invoice_details();
#if ENABLE_WIFI_SERVICE
    home_screen_set_sync_status(g_wifi_connected ? "Online" : "Offline");
#endif
    /* Restore device name — home_screen_create() resets the label to "Device: -" */
    {
        char dev_name[64] = {0};
        if(storage_load_device_name(dev_name, sizeof(dev_name)))
            home_screen_set_device(dev_name);
    }
    devlog_printf("[CTRL] Returned to home");
}

/* ========================================================
   3-POINT CALIBRATION WIZARD STATE MACHINE
   ======================================================== */

static struct {
    int step;                /* 0=not started, 1-3=capture points, 4=done */
    float known_weights[3];
    long  raw_values[3];
    float slope;
    float offset;
    bool  result_valid;
    lv_timer_t *live_timer;
} wiz_state;

/* Linear regression: slope and offset from 3 points */
static bool compute_linear_regression(const long raw[3], const float known[3],
                                       int n, float *out_slope, float *out_offset)
{
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;
    for(int i = 0; i < n; i++) {
        double x = (double)raw[i];
        double y = (double)known[i];
        sum_x  += x;
        sum_y  += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }
    double denom = (double)n * sum_x2 - sum_x * sum_x;
    if(fabs(denom) < 1e-10) return false;

    *out_slope  = (float)((n * sum_xy - sum_x * sum_y) / denom);
    *out_offset = (float)((sum_y - (*out_slope) * sum_x) / n);

    /* Validation: slope must be positive */
    if(*out_slope <= 0) return false;

    return true;
}

static void wiz_live_update(lv_timer_t *t)
{
    float w = scale_service_get_weight();
    long r = scale_service_get_raw();
    calibration_wizard_set_live(w, r);
}

static void open_calibration_wizard(lv_obj_t *from_scr)
{
    memset(&wiz_state, 0, sizeof(wiz_state));
    wiz_state.step = 1;  /* start at point 1 immediately */

    scale_service_resume();

    lv_obj_t *wiz_scr = lv_obj_create(NULL);
    calibration_wizard_create(wiz_scr);

    /* Start live update timer */
    wiz_state.live_timer = lv_timer_create(wiz_live_update, 200, NULL);

    calibration_wizard_register_callback([](int evt) {
        switch(evt) {
            case WIZ_EVT_BACK:
                if(wiz_state.live_timer) {
                    lv_timer_del(wiz_state.live_timer);
                    wiz_state.live_timer = NULL;
                }
                app_controller_restore_home_screen();
                break;

            case WIZ_EVT_NEXT: {
                if(wiz_state.step < 1 || wiz_state.step > 3) {
                    calibration_wizard_set_status("Calibration already complete!", 0xEF4444);
                    break;
                }

                /* Read user-entered weight from numeric keypad */
                float entered = calibration_wizard_get_entered_weight();
                if(entered <= 0.0f) {
                    calibration_wizard_set_status("Enter a valid weight (> 0) !", 0xEF4444);
                    break;
                }

                int idx = wiz_state.step - 1;
                wiz_state.known_weights[idx] = entered;

                /* Capture raw reading (averaged) */
                long raw = scale_service_get_raw_avg(20);
                wiz_state.raw_values[idx] = raw;

                char msg[64];
                snprintf(msg, sizeof(msg), "Point %d captured: raw=%ld for %.1f kg",
                         wiz_state.step, raw, entered);
                calibration_wizard_set_status(msg, 0x22C55E);
                devlog_printf("[CAL] %s", msg);

                calibration_wizard_clear_input();
                wiz_state.step++;

                if(wiz_state.step <= 3) {
                    snprintf(msg, sizeof(msg), "Step %d: Enter weight %d (kg) and CAPTURE",
                             wiz_state.step, wiz_state.step);
                    calibration_wizard_set_step(msg);
                } else {
                    /* All 3 points captured — compute regression */
                    calibration_wizard_set_step("Computing calibration...");

                    bool ok = compute_linear_regression(
                        wiz_state.raw_values, wiz_state.known_weights,
                        3, &wiz_state.slope, &wiz_state.offset
                    );

                    if(ok) {
                        wiz_state.result_valid = true;
                        wiz_state.step = 4;
                        calibration_wizard_set_step("Calibration successful!");
                        calibration_wizard_set_status("Review results and SAVE", 0x22C55E);
                        calibration_wizard_show_result(wiz_state.slope, wiz_state.offset);
                        calibration_wizard_enable_save(true);
                        devlog_printf("[CAL] Regression: slope=%.6f offset=%.2f",
                                      wiz_state.slope, wiz_state.offset);
                    } else {
                        wiz_state.result_valid = false;
                        calibration_wizard_set_step("Calibration FAILED");
                        calibration_wizard_set_status(
                            "Invalid readings. Check weights and retry.", 0xEF4444);
                        devlog_printf("[CAL] Regression failed!");
                    }
                }
                break;
            }

            case WIZ_EVT_SAVE: {
                if(!wiz_state.result_valid) {
                    calibration_wizard_set_status("No valid calibration to save!", 0xEF4444);
                    break;
                }

                /* Determine max capacity from the largest entered weight */
                float max_w = wiz_state.known_weights[0];
                for(int i = 1; i < 3; i++) {
                    if(wiz_state.known_weights[i] > max_w)
                        max_w = wiz_state.known_weights[i];
                }
                int max_cap = (int)(max_w + 0.5f);

                /* Build calibration profile */
                cal_profile_t cp;
                memset(&cp, 0, sizeof(cp));
                snprintf(cp.name, sizeof(cp.name), "%dKG", max_cap);
                cp.max_capacity = max_cap;
                cp.slope = wiz_state.slope;
                cp.offset = wiz_state.offset;
                cp.valid = true;

                /* Save to NVS as profile 0 (custom) */
                storage_save_cal_profile(0, &cp);
                storage_save_active_cal_index(0);

                /* Apply immediately */
                scale_service_set_cal_profile(&cp);

                calibration_wizard_set_status("Calibration saved! Returning...", 0x22C55E);
                devlog_printf("[CAL] Custom profile saved: %s (slope=%.6f offset=%.2f)",
                              cp.name, cp.slope, cp.offset);

                /* Return after brief delay */
                if(wiz_state.live_timer) {
                    lv_timer_del(wiz_state.live_timer);
                    wiz_state.live_timer = NULL;
                }
                lv_timer_create([](lv_timer_t *t) {
                    lv_timer_del(t);
                    app_controller_restore_home_screen();
                }, 1500, NULL);
                break;
            }
        }
    });

    lv_scr_load(wiz_scr);
    lv_obj_del_async(from_scr);
    devlog_printf("[CTRL] Calibration wizard opened");
}

/* ========================================================
   SETTINGS PASSWORD POPUP (numeric keypad)
   ======================================================== */

static const char *SETTINGS_PASSWORD = "1234";

static void settings_password_popup_show(void)
{
    lv_obj_t *overlay = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_70, 0);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);  /* block touches behind */
    lv_obj_center(overlay);

    lv_obj_t *card = lv_obj_create(overlay);
    lv_obj_set_size(card, 500, 420);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, ui_theme_card(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_border_color(card, ui_theme_border(), 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(card);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, ui_theme_accent(), 0);
    lv_label_set_text(title, LV_SYMBOL_SETTINGS "  Enter Password");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *ta = lv_textarea_create(card);
    lv_obj_set_size(ta, 400, 60);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 55);
    lv_textarea_set_password_mode(ta, true);
    lv_textarea_set_max_length(ta, 10);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_placeholder_text(ta, "Password");
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_28, 0);
    lv_obj_set_style_bg_color(ta, ui_theme_surface(), 0);
    lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(ta, ui_theme_text(), 0);
    lv_obj_set_style_border_color(ta, ui_theme_accent(), 0);
    lv_obj_set_style_pad_all(ta, 10, 0);

    lv_obj_t *err_lbl = lv_label_create(card);
    lv_obj_set_style_text_font(err_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(err_lbl, lv_color_hex(0xEF4444), 0);
    lv_label_set_text(err_lbl, "");
    lv_obj_align(err_lbl, LV_ALIGN_TOP_MID, 0, 120);

    /* Numeric keyboard */
    lv_obj_t *kb = lv_keyboard_create(card);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_set_size(kb, 480, 220);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 5);
    lv_obj_add_style(kb, &g_styles.kb_bg, LV_PART_MAIN);
    lv_obj_add_style(kb, &g_styles.kb_btn, LV_PART_ITEMS);

    /* Store overlay pointer in keyboard user_data for cleanup */
    struct pwd_ctx {
        lv_obj_t *overlay;
        lv_obj_t *err_lbl;
    };
    pwd_ctx *ctx = new pwd_ctx{overlay, err_lbl};

    lv_obj_add_event_cb(kb, [](lv_event_t *e) {
        lv_event_code_t code = lv_event_get_code(e);
        pwd_ctx *c = (pwd_ctx *)lv_event_get_user_data(e);
        if(!c) return;

        lv_obj_t *kb_obj = lv_event_get_target(e);
        lv_obj_t *ta_obj = lv_keyboard_get_textarea(kb_obj);

        if(code == LV_EVENT_READY) {
            const char *text = lv_textarea_get_text(ta_obj);
            if(text && strcmp(text, SETTINGS_PASSWORD) == 0) {
                /* Password correct — open settings */
                lv_obj_t *ov = c->overlay;
                delete c;
                lv_obj_del(ov);

                scale_service_suspend();
                lv_obj_t *scr = lv_scr_act();
                lv_obj_t *new_scr = lv_obj_create(NULL);
                settings_screen_create(new_scr);
                settings_screen_register_back_callback([]() {
                    app_controller_restore_home_screen();
                });
                settings_screen_register_calibration_callback([]() {
                    /* Open 3-point calibration wizard from settings */
                    lv_obj_t *cur = lv_scr_act();
                    open_calibration_wizard(cur);
                });
                lv_scr_load(new_scr);
                lv_obj_del_async(scr);
                devlog_printf("[CTRL] Settings unlocked");
            } else {
                /* Wrong password */
                lv_label_set_text(c->err_lbl, "Wrong password!");
                lv_textarea_set_text(ta_obj, "");
            }
        }
        if(code == LV_EVENT_CANCEL) {
            lv_obj_t *ov = c->overlay;
            delete c;
            lv_obj_del(ov);
        }
    }, LV_EVENT_READY, (void*)ctx);

    lv_obj_add_event_cb(kb, [](lv_event_t *e) {
        pwd_ctx *c = (pwd_ctx *)lv_event_get_user_data(e);
        if(!c) return;
        lv_obj_t *ov = c->overlay;
        delete c;
        lv_obj_del(ov);
    }, LV_EVENT_CANCEL, (void*)ctx);
}

#if ENABLE_WIFI_SERVICE
/* Open WiFi list screen directly from home */
static void open_wifi_direct(lv_obj_t *home_scr)
{
    /* Create a standalone WiFi list screen with back → home */
    lv_obj_t *wifi_scr = lv_obj_create(NULL);
    wifi_list_screen_create(wifi_scr);
    wifi_list_screen_register_back([]() {
        app_controller_restore_home_screen();
    });
    wifi_list_screen_register_select([](const char *ssid) {
        extern void wifi_password_popup_show(const char *ssid);
        wifi_password_popup_show(ssid);
    });
    lv_scr_load(wifi_scr);
    lv_obj_del_async(home_scr);

    /* Start scanning immediately */
    wifi_list_screen_start_scan();
    devlog_printf("[CTRL] WiFi screen opened from home");
}
#endif

/* ========================================================
   UI EVENT HANDLING - Entry point from UI layer
   ======================================================== */

void app_controller_handle_ui_event(int event_id)
{
    devlog_printf("[CTRL] UI Event: %d", event_id);

    lv_obj_t *scr = lv_scr_act();
    Serial.printf("[CTRL] event_id=%d, scr=%p\n", event_id, scr);
    
    switch (event_id) {
        case UI_EVT_SETTINGS:
            devlog_printf("[CTRL] → Settings (password required)");
            settings_password_popup_show();
            break;

#if ENABLE_WIFI_SERVICE
        case UI_EVT_WIFI_DIRECT:
            devlog_printf("[CTRL] → WiFi direct from home");
            scale_service_suspend();
            open_wifi_direct(scr);
            break;
#endif

        case UI_EVT_HISTORY:
            devlog_printf("[CTRL] → Open history");
            Serial.println("[CTRL] Creating new screen...");
            Serial.flush();
            scale_service_suspend();
            {
                lv_obj_t *new_scr = lv_obj_create(NULL);
                Serial.println("[CTRL] history_screen_create...");
                Serial.flush();
                history_screen_create(new_scr);
                Serial.println("[CTRL] history_screen_refresh...");
                Serial.flush();
                history_screen_refresh();
                Serial.println("[CTRL] Registering back callback...");
                Serial.flush();
                // Register back callback to return to home
                history_screen_register_back([]() {
                    app_controller_restore_home_screen();
                });
                lv_scr_load(new_scr);
                lv_obj_del_async(scr);
            }
            Serial.println("[CTRL] History screen loaded");
            break;

        case UI_EVT_RESET:
        {
            uint8_t cnt = invoice_session_count();
            devlog_printf("[CTRL] → Finalize invoice #%lu (%u items)",
                          invoice_service_current_id(), cnt);
            if (cnt > 0) {
                invoice_session_commit();
                invoice_service_next();
                devlog_printf("[CTRL] Invoice saved. Next invoice #%lu",
                              invoice_service_current_id());
            } else {
                devlog_printf("[CTRL] No items to finalize");
            }
            /* Reset keypad qty back to 1 */
            invoice_session_set_selected_qty(1);
            /* Update invoice number on home screen */
            home_screen_set_invoice(invoice_service_current_id());
            app_controller_notify_invoice_update();
            break;
        }

        case UI_EVT_RESET_ALL:
            devlog_printf("[CTRL] → Reset ALL invoices");
            invoice_session_clear();
            storage_clear_all_records();
            storage_reset_pending();
            invoice_service_reset();
            home_screen_set_invoice(invoice_service_current_id());
            app_controller_notify_invoice_update();
            break;

        case UI_EVT_CALIBRATE:
        {
            devlog_printf("[CTRL] → Auto-zero calibration (home)");
            const cal_profile_t *cp = scale_service_get_cal_profile();
            if(cp && cp->valid) {
                /* Shift the offset so the current displayed weight maps to 0.
                   Formula: new_offset = old_offset - current_weight
                   Proof: slope * raw + new_offset
                        = (slope * raw + old_offset) - current_weight
                        = current_weight - current_weight = 0             */
                float current_w = scale_service_get_weight();
                cal_profile_t updated = *cp;
                updated.offset = cp->offset - current_w;
                scale_service_set_cal_profile(&updated);
                /* Persist the updated offset so it survives reboot */
                int idx = storage_load_active_cal_index();
                if(idx >= 0) storage_save_cal_profile(idx, &updated);
                devlog_printf("[CTRL] Auto-zero: weight=%.3f kg, offset %.4f → %.4f",
                              current_w, cp->offset, updated.offset);
            } else {
                /* No calibration profile active — fall back to hardware tare */
                scale_service_tare();
                devlog_printf("[CTRL] Auto-zero: hardware tare (no cal profile)");
            }
            /* Flush the 16-sample rolling buffer so display snaps to 0 */
            scale_service_reset_filter();
            home_screen_set_weight(0.0f);
            home_screen_set_sync_status("\xEF\x80\x8C ZERO SET!");
            lv_timer_create([](lv_timer_t *t) {
                lv_timer_del(t);
#if ENABLE_WIFI_SERVICE
                home_screen_set_sync_status(g_wifi_connected ? "Online" : "Offline");
#else
                home_screen_set_sync_status("Offline");
#endif
            }, 2000, NULL);
            break;
        }

        case UI_EVT_SAVE:
            devlog_printf("[CTRL] → Save weight item");
            if (g_last_weight >= 0.05f) {
                invoice_session_add_weight_entry(g_last_weight);
                invoice_session_set_selected_qty(1);
                g_weight_flow_state = WEIGHT_WAIT_FOR_REMOVE;
                g_stable_weight = g_last_weight;
                g_stable_start = millis();
                devlog_printf("[CTRL] Added weight %.2f kg, qty %d",
                              g_last_weight, invoice_session_get_selected_qty());
            } else {
                devlog_printf("[CTRL] Weight too low to save (%.2f)", g_last_weight);
            }
            app_controller_notify_invoice_update();
            break;

        case UI_EVT_QTY_CHANGED:
            devlog_printf("[CTRL] → Qty set to %d", invoice_session_get_selected_qty());
            app_controller_notify_invoice_update();
            break;

        default:
            // Check if it's a "remove item" event
            if (event_id >= UI_EVT_REMOVE_ITEM_BASE) {
                uint8_t item_idx = event_id - UI_EVT_REMOVE_ITEM_BASE;
                devlog_printf("[CTRL] → Remove invoice item %d", item_idx);
                invoice_session_remove(item_idx);
            }
            break;
    }
}

/* ========================================================
   SCALE SERVICE PROXIES
   ======================================================== */

float app_controller_get_weight(void)
{
    return scale_service_get_weight();
}

void app_controller_tare_scale(void)
{
    devlog_printf("[CTRL] Tare scale");
    scale_service_tare();
}

void app_controller_calibrate_scale(float known_weight)
{
    devlog_printf("[CTRL] Calibrate scale with %.2f", known_weight);
    // TODO: Implement calibration flow
}

/* ========================================================
   INVOICE SERVICE PROXIES
   ======================================================== */

void app_controller_add_invoice_item(const char *item_code)
{
    devlog_printf("[CTRL] Add invoice item: %s", item_code);
    // TODO: Implement
}

void app_controller_remove_invoice_item(uint8_t index)
{
    devlog_printf("[CTRL] Remove invoice item: %d", index);
    invoice_session_remove(index);
}

void app_controller_clear_invoice(void)
{
    devlog_printf("[CTRL] Clear invoice");
    invoice_session_clear();
}

/* ========================================================
   SETTINGS PROXIES
   ======================================================== */

const char *app_controller_get_device_name(void)
{
    static char name_buffer[64] = {0};
    storage_load_device_name(name_buffer, sizeof(name_buffer));
    return name_buffer;
}

void app_controller_set_device_name(const char *name)
{
    devlog_printf("[CTRL] Set device name: %s", name);
    storage_save_device_name(name);
    home_screen_set_device(name);
    if (g_device_name_cb) {
        g_device_name_cb(name);
    }
}

/* ========================================================
   WiFi SERVICE PROXIES (Optional)
   ======================================================== */

#if ENABLE_WIFI_SERVICE

void app_controller_start_wifi_scan(void)
{
    devlog_printf("[CTRL] Start WiFi scan");
    wifi_service_start_scan();
}

void app_controller_connect_wifi(const char *ssid, const char *password)
{
    devlog_printf("[CTRL] Connect WiFi: %s", ssid);
    wifi_service_connect(ssid, password);
}

const char *app_controller_get_wifi_status(void)
{
    return g_wifi_connected ? "Online" : "Offline";
}

#endif // ENABLE_WIFI_SERVICE

/* ========================================================
   DIAGNOSTIC QUERIES
   ======================================================== */

bool app_controller_is_wifi_enabled(void)
{
#if ENABLE_WIFI_SERVICE
    return true;
#else
    return false;
#endif
}

bool app_controller_is_ota_enabled(void)
{
#if ENABLE_OTA_UPDATES
    return true;
#else
    return false;
#endif
}

bool app_controller_is_sync_enabled(void)
{
#if ENABLE_CLOUD_SYNC
    return true;
#else
    return false;
#endif
}

void app_controller_set_test_mode(bool on)
{
    g_test_mode = on;
    if (on) {
        g_test_weight_ms = 0; /* trigger immediate first weight */
        g_test_weight_val = 0.0f;
        devlog_printf("[CTRL] Test mode ENABLED");
    } else {
        g_test_weight_val = 0.0f;
        devlog_printf("[CTRL] Test mode DISABLED");
    }
}

bool app_controller_is_test_mode(void)
{
    return g_test_mode;
}

void app_controller_set_manual_offset(float offset_kg)
{
    g_manual_offset = offset_kg;
    storage_save_offset(offset_kg);
    devlog_printf("[CTRL] Manual offset set to %.2f kg", offset_kg);
}

float app_controller_get_manual_offset(void)
{
    return g_manual_offset;
}
