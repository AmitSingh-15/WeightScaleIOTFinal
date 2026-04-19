#include "system_state.h"
#include "devlog.h"

/* ===================================================================
 * System State Machine Implementation
 * =================================================================== */

static system_state_t g_state = SYS_BOOT;
static system_state_cb_t g_state_cb = nullptr;

/* ===== TRANSITION TABLE =====
 * Each row = current state, columns = allowed next states
 * SYS_BOOT     → INIT, RECOVERY
 * SYS_INIT     → IDLE, ERROR, RECOVERY
 * SYS_IDLE     → BUSY, OTA, ERROR
 * SYS_BUSY     → IDLE, ERROR
 * SYS_OTA      → IDLE, ERROR  (OTA finish or fail)
 * SYS_ERROR    → IDLE, RECOVERY
 * SYS_RECOVERY → IDLE, INIT
 */
static bool is_valid_transition(system_state_t from, system_state_t to)
{
    switch (from) {
        case SYS_BOOT:
            return (to == SYS_INIT || to == SYS_RECOVERY);
        case SYS_INIT:
            return (to == SYS_IDLE || to == SYS_ERROR || to == SYS_RECOVERY);
        case SYS_IDLE:
            return (to == SYS_BUSY || to == SYS_OTA || to == SYS_ERROR);
        case SYS_BUSY:
            return (to == SYS_IDLE || to == SYS_ERROR);
        case SYS_OTA:
            return (to == SYS_IDLE || to == SYS_ERROR);
        case SYS_ERROR:
            return (to == SYS_IDLE || to == SYS_RECOVERY);
        case SYS_RECOVERY:
            return (to == SYS_IDLE || to == SYS_INIT);
        default:
            return false;
    }
}

/* ===== STATE NAMES ===== */
const char* system_state_name(system_state_t s)
{
    switch (s) {
        case SYS_BOOT:      return "BOOT";
        case SYS_INIT:      return "INIT";
        case SYS_IDLE:      return "IDLE";
        case SYS_BUSY:      return "BUSY";
        case SYS_OTA:       return "OTA";
        case SYS_ERROR:     return "ERROR";
        case SYS_RECOVERY:  return "RECOVERY";
        default:            return "UNKNOWN";
    }
}

/* ===== STATE QUERY ===== */
system_state_t system_state_get(void)
{
    return g_state;
}

/* ===== STATE TRANSITION ===== */
bool system_state_transition(system_state_t new_state)
{
    if (new_state == g_state) return true;  // No-op

    if (!is_valid_transition(g_state, new_state)) {
        devlog_printf("[STATE] REJECTED: %s -> %s",
                      system_state_name(g_state),
                      system_state_name(new_state));
        return false;
    }

    system_state_t old = g_state;
    g_state = new_state;

    devlog_printf("[STATE] %s -> %s",
                  system_state_name(old),
                  system_state_name(new_state));

    if (g_state_cb) {
        g_state_cb(old, new_state);
    }

    return true;
}

/* ===== GUARDS ===== */
bool system_state_allow_wifi(void)
{
    return (g_state == SYS_IDLE || g_state == SYS_BUSY || g_state == SYS_OTA);
}

bool system_state_allow_ota(void)
{
    return (g_state == SYS_IDLE);
}

bool system_state_allow_sync(void)
{
    return (g_state == SYS_IDLE);
}

bool system_state_allow_scale(void)
{
    return (g_state != SYS_OTA && g_state != SYS_BOOT);
}

/* ===== CALLBACK ===== */
void system_state_register_callback(system_state_cb_t cb)
{
    g_state_cb = cb;
}
