#include "devlog.h"
#include <cstdarg>

/* ✅ MINIMAL DEVLOG - No storage, just serial output - saves 25KB RAM */

void devlog_init(void) 
{
    Serial.println("[DEVLOG] Initialized (serial output only)");
}

void devlog_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    
    char buf[128];  /* 🔥 Reduced from 256 to 128 bytes (local stack var, not global) */
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    Serial.println(buf);
}

String devlog_get_text(void) 
{
    return String("[DEVLOG] Serial only");
}

void devlog_clear(void) 
{
}

void devlog_load_from_storage(void) 
{
}

void devlog_save_to_storage(void) 
{
}
