#pragma once

#include "config/app_config.h"
#include <Arduino.h>

#if ENABLE_CLOUD_SYNC
void sync_service_init(void);
void sync_service_loop(void);

#else  /* !ENABLE_CLOUD_SYNC */

/* Stub implementations when Cloud Sync is disabled */
inline void sync_service_init(void) {}
inline void sync_service_loop(void) {}

#endif  /* ENABLE_CLOUD_SYNC */
