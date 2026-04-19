#pragma once
#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <Arduino.h>

/* ===================================================================
 * System State Machine
 * 
 * Central state that guards all subsystem operations.
 * Transition rules enforced — invalid transitions are rejected.
 * =================================================================== */

typedef enum {
    SYS_BOOT,       // Hardware init, before any services
    SYS_INIT,       // Services starting up
    SYS_IDLE,       // Normal operation, all services running
    SYS_BUSY,       // Active operation (sync, scan, etc.)
    SYS_OTA,        // OTA in progress — blocks everything except OTA
    SYS_ERROR,      // Recoverable error state
    SYS_RECOVERY    // Crash loop recovery — WiFi disabled
} system_state_t;

// State query
system_state_t  system_state_get(void);
const char*     system_state_name(system_state_t s);

// State transitions (return true if transition was valid)
bool system_state_transition(system_state_t new_state);

// Guards — check if an operation is allowed in current state
bool system_state_allow_wifi(void);
bool system_state_allow_ota(void);
bool system_state_allow_sync(void);
bool system_state_allow_scale(void);

// Callback on state change
typedef void (*system_state_cb_t)(system_state_t old_state, system_state_t new_state);
void system_state_register_callback(system_state_cb_t cb);

#endif // SYSTEM_STATE_H
