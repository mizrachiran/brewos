/**
 * Native/Simulator Platform Implementation
 * 
 * Provides platform functions for desktop builds.
 */

#ifndef NATIVE_IMPL_H
#define NATIVE_IMPL_H

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline uint32_t platform_millis(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((ts.tv_sec * 1000) + (ts.tv_nsec / 1000000));
}

static inline void platform_delay(uint32_t ms) {
    usleep(ms * 1000);
}

static inline void platform_log(log_level_t level, const char* fmt, ...) {
    const char* level_str;
    switch(level) {
        case BREWOS_LOG_DEBUG: level_str = "D"; break;
        case BREWOS_LOG_INFO:  level_str = "I"; break;
        case BREWOS_LOG_WARN:  level_str = "W"; break;
        case BREWOS_LOG_ERROR: level_str = "E"; break;
        default: level_str = "?"; break;
    }
    
    printf("[%" PRIu32 "][%s] ", platform_millis(), level_str);
    
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    
    printf("\n");
}

#ifdef __cplusplus
}
#endif

#endif // NATIVE_IMPL_H

