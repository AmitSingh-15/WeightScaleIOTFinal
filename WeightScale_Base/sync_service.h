#pragma once

#include "config/app_config.h"
#include <Arduino.h>

#if ENABLE_CLOUD_SYNC
void sync_service_init(void);
void sync_service_loop(void);
bool sync_service_is_busy(void);
unsigned long sync_service_last_http_ms(void);

#else  /* !ENABLE_CLOUD_SYNC */

/* Stub implementations when Cloud Sync is disabled */
inline void sync_service_init(void) {}
inline void sync_service_loop(void) {}
inline bool sync_service_is_busy(void) { return false; }
inline unsigned long sync_service_last_http_ms(void) { return 0; }

#endif  /* ENABLE_CLOUD_SYNC */
