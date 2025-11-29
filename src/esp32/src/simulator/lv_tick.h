/**
 * Custom LVGL Tick Source for Simulator
 * 
 * Provides lv_tick_get() implementation for native builds
 */

#ifndef LV_TICK_CUSTOM_H
#define LV_TICK_CUSTOM_H

#include <stdint.h>
#include <time.h>

// Just provide millis() equivalent for the LV_TICK_CUSTOM_SYS_TIME_EXPR macro
static inline uint32_t millis(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((ts.tv_sec * 1000) + (ts.tv_nsec / 1000000));
}

#endif // LV_TICK_CUSTOM_H
