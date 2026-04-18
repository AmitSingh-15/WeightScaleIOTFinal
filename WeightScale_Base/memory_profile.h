/**
 * @file memory_profile.h
 * @brief Memory profiling utilities for heap monitoring
 * 
 * Usage:
 *   memory_profile_init();  // Call in setup()
 *   memory_profile_tick();  // Call every 10 seconds in loop
 *   // Devlog will auto-print heap stats
 */

#pragma once

#include <Arduino.h>
#include <esp_heap_caps.h>
#include "devlog.h"

// ========================================================
// MEMORY PROFILE DATA
// ========================================================

typedef struct {
    uint32_t heap_free_now;        // Current free heap
    uint32_t heap_free_min;        // Minimum observed
    uint32_t heap_free_max;        // Maximum observed
    uint32_t heap_free_prev;       // Previous snapshot
    int32_t  heap_delta;           // Change from previous
    uint32_t sample_count;         // Number of samples taken
    uint32_t last_sample_ms;       // Last sample time
    bool     memory_warning;       // True if heap < 50KB
} memory_profile_t;

static memory_profile_t g_mem_profile = {0};

// ========================================================
// MEMORY PROFILE API
// ========================================================

/**
 * Initialize memory profiling
 */
static inline void memory_profile_init(void)
{
    g_mem_profile.heap_free_max = esp_get_free_heap_size();
    g_mem_profile.heap_free_min = g_mem_profile.heap_free_max;
    g_mem_profile.heap_free_prev = g_mem_profile.heap_free_max;
    g_mem_profile.sample_count = 0;
    g_mem_profile.last_sample_ms = millis();
    
    devlog_printf("[MEMORY] Profiling initialized, heap: %lu bytes", 
                  g_mem_profile.heap_free_max);
}

/**
 * Collect memory sample and log if changed significantly
 * Call every 10 seconds from main loop
 */
static inline void memory_profile_tick(void)
{
    uint32_t now_ms = millis();
    
    // Sample every 10 seconds
    if (now_ms - g_mem_profile.last_sample_ms < 10000) {
        return;
    }
    
    g_mem_profile.last_sample_ms = now_ms;
    g_mem_profile.heap_free_now = esp_get_free_heap_size();
    g_mem_profile.heap_free_min = esp_get_minimum_free_heap_size();
    
    // Update min/max
    if (g_mem_profile.heap_free_now < g_mem_profile.heap_free_min) {
        g_mem_profile.heap_free_min = g_mem_profile.heap_free_now;
    }
    if (g_mem_profile.heap_free_now > g_mem_profile.heap_free_max) {
        g_mem_profile.heap_free_max = g_mem_profile.heap_free_now;
    }
    
    g_mem_profile.heap_delta = (int32_t)g_mem_profile.heap_free_now - 
                               (int32_t)g_mem_profile.heap_free_prev;
    g_mem_profile.heap_free_prev = g_mem_profile.heap_free_now;
    g_mem_profile.sample_count++;
    
    // Check for memory warning
    g_mem_profile.memory_warning = (g_mem_profile.heap_free_now < 50000);
    
    // Log memory status
    devlog_printf("[MEMORY] Free: %lu bytes | Delta: %+ld | Min: %lu | Max: %lu | Samples: %lu %s",
                  g_mem_profile.heap_free_now,
                  g_mem_profile.heap_delta,
                  g_mem_profile.heap_free_min,
                  g_mem_profile.heap_free_max,
                  g_mem_profile.sample_count,
                  g_mem_profile.memory_warning ? "⚠ WARNING" : "");
    
    // Log detailed analysis if significant change
    if (labs(g_mem_profile.heap_delta) > 5000) {
        const char *trend = g_mem_profile.heap_delta < 0 ? "DECLINING" : "IMPROVING";
        devlog_printf("[MEMORY] ⚠ Heap %s by %ld bytes (possible leak?)", 
                      trend, labs(g_mem_profile.heap_delta));
    }
}

/**
 * Get current memory status as a readable string
 */
static inline const char* memory_profile_status(void)
{
    if (g_mem_profile.heap_free_now < 50000) {
        return "CRITICAL";
    } else if (g_mem_profile.heap_free_now < 100000) {
        return "WARNING";
    } else if (g_mem_profile.heap_free_now < 200000) {
        return "OK";
    } else {
        return "GOOD";
    }
}

/**
 * Check if memory is in warning state
 */
static inline bool memory_profile_warning(void)
{
    return g_mem_profile.memory_warning;
}

/**
 * Get free heap size (non-blocking snapshot)
 */
static inline uint32_t memory_profile_free(void)
{
    return g_mem_profile.heap_free_now;
}
