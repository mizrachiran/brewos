#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <Arduino.h>
#include <functional>

// Include config.h for BrewOSLogLevel enum
// Include guards prevent infinite recursion when config.h includes this file
// We just need to avoid using LOG macros in this header (which we don't)
#include "config.h"

// Log buffer size (50KB)
#define LOG_BUFFER_SIZE (50 * 1024)

// Maximum log file size in LittleFS (100KB - limits flash usage)
#define LOG_FLASH_MAX_SIZE (100 * 1024)

// Maximum single log entry size
#define LOG_ENTRY_MAX_SIZE 256

// RTC memory size for crash log persistence (survives reboot)
#define RTC_LOG_SIZE 2048

// Log source identifier
enum LogSource {
    LOG_SOURCE_ESP32 = 0,
    LOG_SOURCE_PICO = 1
};

// Structure to hold crash logs in RTC memory (survives reboot)
struct RTCLogBuffer {
    char data[RTC_LOG_SIZE];
    size_t head;
    uint32_t magic; // To verify validity on boot
};

/**
 * Log Manager - Circular buffer for system logs
 * 
 * Features:
 * - 50KB ring buffer (only allocated when enabled)
 * - Zero memory/performance impact when disabled
 * - Captures ESP32 logs via hook
 * - Receives Pico logs via UART protocol
 * - Provides API for web download
 * - Thread-safe with mutex
 * 
 * The buffer is NOT allocated by default - call enable() to start logging.
 * Setting is persisted in NVS via StateManager.
 */
class LogManager {
public:
    LogManager();
    ~LogManager();
    
    /**
     * Enable the log buffer (allocates 50KB memory)
     * Called when user enables logging in settings
     * @return true if successfully enabled
     */
    bool enable();
    
    /**
     * Disable the log buffer (frees memory)
     * Called when user disables logging in settings
     */
    void disable();
    
    /**
     * Check if log buffer is enabled and active
     */
    bool isEnabled() const { return _enabled && _buffer != nullptr; }
    
    /**
     * Add a log entry (no-op if disabled)
     * @param level Log level (ERROR, WARN, INFO, DEBUG)
     * @param source Log source (ESP32 or PICO)
     * @param message Log message (will be truncated if too long)
     */
    void addLog(BrewOSLogLevel level, LogSource source, const char* message);
    
    /**
     * Add a formatted log entry (printf style) (no-op if disabled)
     */
    void addLogf(BrewOSLogLevel level, LogSource source, const char* format, ...);
    
    /**
     * Get all logs as a string
     * @return String containing all logs in buffer (empty if disabled)
     */
    String getLogs();
    
    /**
     * Get logs size in bytes
     */
    size_t getLogsSize();
    
    /**
     * Clear all logs
     */
    void clear();
    
    /**
     * Enable/disable Pico log forwarding
     * This sends a command to Pico to start/stop forwarding logs
     * @param enabled Whether to enable forwarding
     * @param sendCommand Function to send command to Pico
     */
    void setPicoLogForwarding(bool enabled, std::function<bool(uint8_t*, size_t)> sendCommand);
    
    /**
     * Check if Pico log forwarding is enabled
     */
    bool isPicoLogForwardingEnabled() const { return _picoLogForwarding; }
    
    /**
     * Handle incoming log from Pico (no-op if disabled)
     * Called by UART handler when MSG_LOG is received
     * @param payload Log message payload
     * @param length Payload length
     */
    void handlePicoLog(const uint8_t* payload, size_t length);
    
    /**
     * Get the singleton instance
     */
    static LogManager& instance();
    
    /**
     * Add log entry directly without mutex (for panic handler use only)
     * This bypasses mutex protection and should only be used in panic/exception context
     * where normal mutex operations may not be safe.
     */
    void addLogDirect(BrewOSLogLevel level, LogSource source, const char* message);
    
    /**
     * Save log buffer to flash (LittleFS) for persistence across reboots
     * @return true if saved successfully
     */
    bool saveToFlash();
    
    /**
     * Restore log buffer from flash (LittleFS) on boot
     * @return true if restored successfully
     */
    bool restoreFromFlash();
    
    /**
     * Get all logs from flash (complete history)
     * This reads the saved logs from LittleFS, which contains the full log history
     * @return String containing all logs from flash
     */
    String getLogsFromFlash();
    
    /**
     * Get all logs (RAM + Flash merged)
     * Flushes RAM to flash first, then returns complete log history from flash
     * @return String containing all logs
     */
    String getLogsComplete();
    
    /**
     * Periodic update - call from main loop
     * Checks if auto-save is needed and saves to flash if necessary
     * Auto-saves every 30 seconds or when buffer is 50% full
     */
    void loop();

private:
    char* _buffer;              // Circular buffer (nullptr when disabled)
    size_t _head;               // Write position
    size_t _tail;               // Read position (oldest entry)
    size_t _size;               // Current data size
    bool _wrapped;              // Buffer has wrapped around
    SemaphoreHandle_t _mutex;   // Thread safety
    bool _picoLogForwarding;    // Pico log forwarding enabled
    bool _enabled;              // Log buffer is enabled
    unsigned long _lastSaveTime; // Last auto-save time (millis)
    
    // Internal helpers
    void writeToBuffer(const char* data, size_t len);
    const char* levelToString(BrewOSLogLevel level);
    const char* sourceToString(LogSource source);
    
    /**
     * Get logs without acquiring mutex (assumes caller already has mutex)
     * Internal helper for saveToFlash() to avoid deadlock
     */
    String getLogsUnsafe();
};

// Global instance access (may be nullptr if never enabled)
extern LogManager* g_logManager;

// Helper functions for LOG macros (take int to avoid needing enum definition)
// These are implemented in log_manager.cpp where we have the full enum definition
void log_manager_add_logf(int level, LogSource source, const char* format, ...);

#endif // LOG_MANAGER_H
