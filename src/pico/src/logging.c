/**
 * Logging System Implementation
 * 
 * Provides structured logging with multiple levels and ESP32 forwarding.
 * 
 * NON-BLOCKING: Uses ring buffer to prevent printf() from blocking the control loop
 * when USB CDC buffer is full. Messages are queued and processed asynchronously.
 */

#include "logging.h"
#include "log_forward.h"
#include "pico/sync.h"  // For critical sections
#include <stdio.h>
#include <string.h>

// =============================================================================
// Ring Buffer for Non-Blocking Logging
// =============================================================================

#define LOG_BUFFER_SIZE 1024  // Ring buffer size in bytes
#define LOG_MAX_MESSAGE 96   // Maximum message length (reduced from 256 to save stack space)

// Ring buffer structure
typedef struct {
    uint8_t buffer[LOG_BUFFER_SIZE];
    volatile uint32_t write_pos;  // Next write position
    volatile uint32_t read_pos;   // Next read position
    volatile uint32_t count;      // Number of bytes in buffer
} log_ring_buffer_t;

static log_ring_buffer_t g_log_buffer = {0};
static bool g_use_ring_buffer = true;  // Enable ring buffer by default

// =============================================================================
// Private State
// =============================================================================

static log_level_t g_log_level = LOG_LEVEL_INFO;  // Default to INFO
static bool g_forward_enabled = false;
static bool g_initialized = false;

// =============================================================================
// Configuration
// =============================================================================

void logging_init(void) {
    if (g_initialized) {
        return;
    }
    
    // Log forwarding is initialized separately in main.c
    // This is just for the logging level system
    g_log_level = LOG_LEVEL_INFO;
    g_forward_enabled = false;
    g_initialized = true;
}

void logging_set_level(log_level_t level) {
    if (level <= LOG_LEVEL_TRACE) {
        g_log_level = level;
    }
}

log_level_t logging_get_level(void) {
    return g_log_level;
}

void logging_set_forward_enabled(bool enable) {
    // IMPORTANT: Set g_forward_enabled AFTER log_forward_set_enabled() completes
    // This ensures flash write completes before any logs try to forward
    // Prevents crashes from forwarding during flash operations
    if (enable) {
        log_forward_set_enabled(true);
        // Only enable forwarding in logging system after flash write is complete
        g_forward_enabled = true;
    } else {
        // Disable forwarding first to prevent new logs from trying to forward
        g_forward_enabled = false;
        // Then update the persisted state
        log_forward_set_enabled(false);
    }
}

bool logging_is_forward_enabled(void) {
    return g_forward_enabled;
}

// =============================================================================
// Ring Buffer Operations
// =============================================================================

/**
 * Write message to ring buffer (non-blocking)
 * Returns true if message was written, false if buffer was full
 */
static bool log_buffer_write(const char* message, size_t len) {
    if (len == 0 || len >= LOG_BUFFER_SIZE) {
        return false;  // Invalid length or too large
    }
    
    // Critical section to protect ring buffer
    uint32_t irq_state = save_and_disable_interrupts();
    
    // Check if we have space (leave 1 byte to distinguish full from empty)
    uint32_t available = LOG_BUFFER_SIZE - g_log_buffer.count - 1;
    if (len > available) {
        restore_interrupts(irq_state);
        return false;  // Buffer full - drop message (non-blocking)
    }
    
    // Write message to buffer (may wrap around)
    uint32_t write_pos = g_log_buffer.write_pos;
    for (size_t i = 0; i < len; i++) {
        g_log_buffer.buffer[write_pos] = message[i];
        write_pos = (write_pos + 1) % LOG_BUFFER_SIZE;
    }
    
    g_log_buffer.write_pos = write_pos;
    g_log_buffer.count += len;
    
    restore_interrupts(irq_state);
    return true;
}

/**
 * Read message from ring buffer
 * Returns number of bytes read, or 0 if buffer is empty
 */
static size_t log_buffer_read(char* dest, size_t max_len) {
    if (max_len == 0) {
        return 0;
    }
    
    // Critical section to protect ring buffer
    uint32_t irq_state = save_and_disable_interrupts();
    
    if (g_log_buffer.count == 0) {
        restore_interrupts(irq_state);
        return 0;  // Buffer empty
    }
    
    // Read up to max_len bytes (or until buffer is empty)
    size_t to_read = (g_log_buffer.count < max_len) ? g_log_buffer.count : max_len;
    uint32_t read_pos = g_log_buffer.read_pos;
    
    for (size_t i = 0; i < to_read; i++) {
        dest[i] = g_log_buffer.buffer[read_pos];
        read_pos = (read_pos + 1) % LOG_BUFFER_SIZE;
    }
    
    g_log_buffer.read_pos = read_pos;
    g_log_buffer.count -= to_read;
    
    restore_interrupts(irq_state);
    return to_read;
}

// =============================================================================
// Logging Functions
// =============================================================================

void log_message_va(log_level_t level, const char* format, va_list args) {
    if (!g_initialized) {
        logging_init();
    }
    
    // Filter by level
    if (level > g_log_level) {
        return;
    }
    
    // Format message to buffer
    char buffer[LOG_MAX_MESSAGE];
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    if (len < 0 || len >= (int)sizeof(buffer)) {
        len = sizeof(buffer) - 1;  // Truncate if needed
        buffer[len] = '\0';
    }
    
    // NON-BLOCKING: Always use ring buffer to prevent printf() from blocking
    // when USB CDC buffer is full. This is critical for Core 0 control loop.
    // Messages are queued and processed asynchronously by logging_process_pending()
    if (g_use_ring_buffer) {
        // Write to ring buffer (non-blocking - drops message if full)
        log_buffer_write(buffer, (size_t)len);
        
        // ESP32 forwarding: Try to send immediately if enabled
        // log_forward_send() should be non-blocking, but if it blocks,
        // the ring buffer ensures we don't block the control loop
        // Check both flags: g_forward_enabled (logging system) and log_forward_is_enabled() (persisted state)
        if (g_forward_enabled && log_forward_is_enabled()) {
            // Map log level to log_forward level
            uint8_t fwd_level = LOG_FWD_INFO;
            switch (level) {
                case LOG_LEVEL_ERROR: fwd_level = LOG_FWD_ERROR; break;
                case LOG_LEVEL_WARN:  fwd_level = LOG_FWD_WARN;  break;
                case LOG_LEVEL_INFO:  fwd_level = LOG_FWD_INFO;  break;
                case LOG_LEVEL_DEBUG: fwd_level = LOG_FWD_DEBUG; break;
                case LOG_LEVEL_TRACE: fwd_level = LOG_FWD_DEBUG; break;
            }
            // Forward the log message to ESP32
            log_forward_send(fwd_level, buffer);
        }
    } else {
        // Direct printf (may block if USB buffer is full - not recommended for Core 0)
        // This mode is provided for compatibility but should be avoided in production
        printf("%s", buffer);
        
        // Forward to ESP32 if enabled
        if (g_forward_enabled && log_forward_is_enabled()) {
            // Map log level to log_forward level
            uint8_t fwd_level = LOG_FWD_INFO;
            switch (level) {
                case LOG_LEVEL_ERROR: fwd_level = LOG_FWD_ERROR; break;
                case LOG_LEVEL_WARN:  fwd_level = LOG_FWD_WARN;  break;
                case LOG_LEVEL_INFO:  fwd_level = LOG_FWD_INFO;  break;
                case LOG_LEVEL_DEBUG: fwd_level = LOG_FWD_DEBUG; break;
                case LOG_LEVEL_TRACE: fwd_level = LOG_FWD_DEBUG; break;
            }
            log_forward_send(fwd_level, buffer);
        }
    }
}

void log_message(log_level_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_message_va(level, format, args);
    va_end(args);
}

// =============================================================================
// Utility Functions
// =============================================================================

const char* log_level_name(log_level_t level) {
    switch (level) {
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_TRACE: return "TRACE";
        default:              return "UNKNOWN";
    }
}

// =============================================================================
// Ring Buffer Processing
// =============================================================================

void logging_process_pending(void) {
    if (!g_use_ring_buffer) {
        return;
    }
    
    // Process messages from ring buffer and output via printf()
    // This should be called periodically (e.g., from Core 1 or background task)
    char temp_buffer[LOG_MAX_MESSAGE];
    size_t total_read = 0;
    
    // Read messages in chunks and output them
    while (true) {
        size_t read = log_buffer_read(temp_buffer, sizeof(temp_buffer) - 1);
        if (read == 0) {
            break;  // Buffer empty
        }
        
        temp_buffer[read] = '\0';  // Null-terminate
        
        // Output via printf() (may block, but we're not in critical path)
        printf("%s", temp_buffer);
        
        total_read += read;
        
        // Limit processing per call to avoid blocking too long
        if (total_read > LOG_BUFFER_SIZE / 2) {
            break;  // Process more on next call
        }
    }
    
    // Forward to ESP32 if enabled (process any queued messages)
    if (g_forward_enabled && log_forward_is_enabled() && total_read > 0) {
        // Note: log_forward_send() should also be non-blocking
        // If it's not, we may need to queue these messages separately
    }
}
