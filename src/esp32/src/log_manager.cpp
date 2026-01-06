#include "log_manager.h"
#include "config.h"  // For BrewOSLogLevel enum definition
#include "protocol_defs.h"
#include "state/state_manager.h"  // For State macro to check debug logs setting
#include "web_server.h"  // For BrewWebServer
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdarg.h>
#include <LittleFS.h>
#include <esp_attr.h>  // For RTC_NOINIT_ATTR

// Forward declaration for web server (defined in main.cpp)
// Helper function to broadcast log via WebSocket (defined in web_server_broadcast.cpp)
extern "C" void platform_broadcast_log(const char* level, const char* message);
extern BrewWebServer* webServer;  // Global webServer pointer from main.cpp

// RTC memory buffer for crash log persistence (survives software reset)
// RTC_NOINIT_ATTR ensures this memory is NOT cleared on boot
RTC_NOINIT_ATTR RTCLogBuffer rtcLogs;
const uint32_t RTC_MAGIC = 0xDEADBEEF;

// Global instance pointer
LogManager* g_logManager = nullptr;

// Singleton instance
static LogManager s_instance;

LogManager& LogManager::instance() {
    return s_instance;
}

LogManager::LogManager()
    : _buffer(nullptr)
    , _head(0)
    , _tail(0)
    , _size(0)
    , _wrapped(false)
    , _mutex(nullptr)  // Don't create mutex here - created lazily in enable()
    , _picoLogForwarding(false)
    , _enabled(false)
    , _lastSaveTime(0)
{
    // Set global pointer
    // Note: Mutex is created in enable() to avoid static initialization order issues
    // Global constructors run before FreeRTOS scheduler is fully initialized
    g_logManager = this;
}

LogManager::~LogManager() {
    disable();
    if (_mutex) {
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
    }
    g_logManager = nullptr;
}

bool LogManager::enable() {
    if (_enabled && _buffer) {
        return true;  // Already enabled
    }
    
    // 1. Safe Mutex Creation (Lazy Initialization)
    // CRITICAL: Create mutex here, not in constructor, to avoid static initialization order issues
    // Global constructors run before FreeRTOS scheduler is fully initialized
    if (!_mutex) {
        _mutex = xSemaphoreCreateMutex();
        if (!_mutex) {
            Serial.println("[LogManager] CRITICAL: Failed to create mutex!");
            return false;
        }
    }
    
    // Allocate buffer in PSRAM if available, otherwise use regular heap
    _buffer = (char*)heap_caps_malloc(LOG_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!_buffer) {
        // Fallback to regular malloc
        _buffer = (char*)malloc(LOG_BUFFER_SIZE);
    }
    
    if (!_buffer) {
        Serial.println("[LogManager] ERROR: Failed to allocate 50KB log buffer");
        return false;
    }
    
    // 1. Restore previous session logs from flash FIRST
    // This sets up the head/tail/buffer state from the last normal shutdown
    // CRITICAL: Must restore flash first, otherwise restoreFromFlash() will wipe out
    // any crash logs we add to the buffer (it does memset on the buffer)
    bool restored = restoreFromFlash();
    
    // Only clear if we didn't restore (first time or restore failed)
    if (!restored) {
        memset(_buffer, 0, LOG_BUFFER_SIZE);
        _head = 0;
        _tail = 0;
        _size = 0;
        _wrapped = false;
    }
    
    // 2. NOW recover Crash Logs from RTC Memory (append them to the end)
    // RTC memory survives software resets, so we can recover crash logs
    // This must happen AFTER restoreFromFlash() to avoid being wiped out
    if (rtcLogs.magic == RTC_MAGIC && rtcLogs.head > 0) {
        // We have a valid crash log from previous run!
        // Add it to the main buffer (which already has restored flash logs)
        addLog(BREWOS_LOG_ERROR, LOG_SOURCE_ESP32, "CRASH DETECTED! Recovering crash log:");
        
        // Ensure null termination
        if (rtcLogs.head < RTC_LOG_SIZE) {
            rtcLogs.data[rtcLogs.head] = '\0';
        } else {
            rtcLogs.data[RTC_LOG_SIZE - 1] = '\0';
        }
        
        // Add the crash log to main buffer (appended after restored flash logs)
        addLog(BREWOS_LOG_ERROR, LOG_SOURCE_ESP32, rtcLogs.data);
        
        // Clean up RTC buffer so we don't report it again
        rtcLogs.magic = 0;
        rtcLogs.head = 0;
        memset(rtcLogs.data, 0, RTC_LOG_SIZE);
    } else {
        // Cold boot init - initialize RTC buffer for future crash logs
        if (rtcLogs.magic != RTC_MAGIC) {
            rtcLogs.head = 0;
            memset(rtcLogs.data, 0, RTC_LOG_SIZE);
            rtcLogs.magic = RTC_MAGIC;
        }
    }
    
    _enabled = true;
    _lastSaveTime = 0;  // Initialize save time (will be set on first loop call)
    
    Serial.printf("[LogManager] Enabled - allocated %dKB buffer\n", LOG_BUFFER_SIZE / 1024);
    if (restored) {
        addLog(BREWOS_LOG_INFO, LOG_SOURCE_ESP32, "Log buffer enabled (50KB) - restored from flash");
    } else {
        addLog(BREWOS_LOG_INFO, LOG_SOURCE_ESP32, "Log buffer enabled (50KB)");
    }
    
    return true;
}

void LogManager::disable() {
    if (!_enabled) {
        return;  // Already disabled
    }
    
    _enabled = false;
    
    if (_buffer) {
        free(_buffer);
        _buffer = nullptr;
    }
    
    _head = 0;
    _tail = 0;
    _size = 0;
    _wrapped = false;
    
    // Also disable Pico log forwarding when buffer is disabled
    _picoLogForwarding = false;
    
    Serial.println("[LogManager] Disabled - freed buffer");
}

const char* LogManager::levelToString(BrewOSLogLevel level) {
    switch (level) {
        case BREWOS_LOG_ERROR: return "E";
        case BREWOS_LOG_WARN:  return "W";
        case BREWOS_LOG_INFO:  return "I";
        case BREWOS_LOG_DEBUG: return "D";
        default: return "?";
    }
}

const char* LogManager::sourceToString(LogSource source) {
    switch (source) {
        case LOG_SOURCE_ESP32: return "ESP";
        case LOG_SOURCE_PICO:  return "PICO";
        default: return "?";
    }
}

void LogManager::writeToBuffer(const char* data, size_t len) {
    if (!_buffer || len == 0) return;
    
    for (size_t i = 0; i < len; i++) {
        // Check if we're about to overwrite tail (buffer full)
        if (_wrapped && _head == _tail) {
            // Find next newline to maintain log entry integrity
            while (_buffer[_tail] != '\n' && _tail != _head) {
                _tail = (_tail + 1) % LOG_BUFFER_SIZE;
            }
            // Skip the newline
            if (_buffer[_tail] == '\n') {
                _tail = (_tail + 1) % LOG_BUFFER_SIZE;
            }
        }
        
        _buffer[_head] = data[i];
        _head = (_head + 1) % LOG_BUFFER_SIZE;
        
        // Detect wrap: if head wraps to 0, we've wrapped around
        if (_head == 0) {
            _wrapped = true;
        }
    }
    
    // Update size
    if (_wrapped) {
        _size = LOG_BUFFER_SIZE;
    } else {
        _size = _head;
    }
}

void LogManager::addLog(BrewOSLogLevel level, LogSource source, const char* message) {
    // No-op if disabled or mutex not created yet
    // Added !_mutex check for defensive programming
    if (!_enabled || !_buffer || !message || !_mutex) return;
    
    // CRITICAL FIX: Check if we are in an ISR context
    // Calling xSemaphoreTake from an ISR causes a panic/crash on ESP32/FreeRTOS
    // We must check the context before attempting mutex operations
    if (xPortInIsrContext()) {
        // We cannot take a mutex or safely use snprintf/malloc in an ISR
        // Skip logging from ISR context to avoid crashes
        // Note: If ISR logging is needed, use a FreeRTOS queue to defer processing
        return;
    }
    
    // Take mutex (with timeout to avoid deadlock)
    // Now safe to take since we've checked _mutex is not null
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return; // Skip this log entry if can't get mutex
    }
    
    // Format: [timestamp] [SOURCE] LEVEL: message\n
    // Truncate message if too long to fit in entry buffer
    char entry[LOG_ENTRY_MAX_SIZE];
    unsigned long timestamp = millis();
    
    // Calculate available space for message (leave room for prefix and newline)
    size_t prefixLen = snprintf(entry, sizeof(entry), "[%lu] [%s] %s: ", 
                                 timestamp, sourceToString(source), levelToString(level));
    size_t maxMsgLen = sizeof(entry) - prefixLen - 2; // -2 for \n and \0
    
    // Truncate message if needed
    char truncatedMsg[LOG_ENTRY_MAX_SIZE];
    strncpy(truncatedMsg, message, maxMsgLen);
    truncatedMsg[maxMsgLen] = '\0';
    
    // Format complete entry
    int len = snprintf(entry, sizeof(entry), "[%lu] [%s] %s: %s\n",
                       timestamp,
                       sourceToString(source),
                       levelToString(level),
                       truncatedMsg);
    
    // Write to buffer (snprintf will truncate if needed, but we ensure it fits)
    if (len > 0) {
        size_t writeLen = (len < (int)sizeof(entry)) ? len : sizeof(entry) - 1;
        writeToBuffer(entry, writeLen);
    }
    
    xSemaphoreGive(_mutex);
}

void LogManager::addLogf(BrewOSLogLevel level, LogSource source, const char* format, ...) {
    // No-op if disabled
    if (!_enabled || !_buffer || !format) return;
    
    // CRITICAL FIX: Check if we are in an ISR context
    // Calling xSemaphoreTake from an ISR causes a panic/crash on ESP32/FreeRTOS
    if (xPortInIsrContext()) {
        // We cannot take a mutex or safely use vsnprintf in an ISR
        // Skip logging from ISR context to avoid crashes
        return;
    }
    
    char message[200]; // Larger buffer to avoid truncation
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    addLog(level, source, message);
}

String LogManager::getLogsUnsafe() {
    // Internal helper - assumes mutex is already held by caller
    if (!_enabled || !_buffer) return String();
    
    String result;
    result.reserve(_size + 1);
    
    if (_wrapped) {
        // Buffer has wrapped - read from tail to end, then from start to head
        // Use concat with length to avoid O(N²) character-by-character concatenation
        size_t len1 = LOG_BUFFER_SIZE - _tail;
        result.concat(_buffer + _tail, len1);
        
        // Read start -> head
        result.concat(_buffer, _head);
    } else {
        // Buffer hasn't wrapped - simple copy from start to head
        result.concat(_buffer, _head);
    }
    
    return result;
}

String LogManager::getLogs() {
    if (!_enabled || !_buffer || !_mutex) return String();
    
    // Use longer timeout (5 seconds) to handle cases where mutex is held during save operations
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        return String("ERROR: Could not acquire log mutex");
    }
    
    String result = getLogsUnsafe();
    
    xSemaphoreGive(_mutex);
    return result;
}

size_t LogManager::getLogsSize() {
    if (!_enabled) return 0;
    return _size;
}

void LogManager::clear() {
    if (!_enabled || !_buffer || !_mutex) return;
    
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return;
    }
    
    memset(_buffer, 0, LOG_BUFFER_SIZE);
    _head = 0;
    _tail = 0;
    _size = 0;
    _wrapped = false;
    
    xSemaphoreGive(_mutex);
    
    addLog(BREWOS_LOG_INFO, LOG_SOURCE_ESP32, "Logs cleared");
}

void LogManager::setPicoLogForwarding(bool enabled, std::function<bool(uint8_t*, size_t)> sendCommand) {
    // Can only enable Pico forwarding if log buffer is enabled
    if (enabled && !_enabled) {
        Serial.println("[LogManager] Cannot enable Pico forwarding - log buffer not enabled");
        return;
    }
    
    _picoLogForwarding = enabled;
    
    // Send command to Pico to enable/disable log forwarding
    if (sendCommand) {
        uint8_t payload[1] = { enabled ? 1 : 0 };
        if (sendCommand(payload, 1)) {
            addLogf(BREWOS_LOG_INFO, LOG_SOURCE_ESP32, 
                   "Pico log forwarding %s", enabled ? "enabled" : "disabled");
        } else {
            addLog(BREWOS_LOG_WARN, LOG_SOURCE_ESP32, 
                  "Failed to send log config to Pico");
        }
    }
}

void LogManager::handlePicoLog(const uint8_t* payload, size_t length) {
    // No-op if disabled
    if (!_enabled || !payload || length == 0) return;
    
    // Safety: Don't log from interrupt context (mutex can't be used in ISR)
    // Note: We rely on mutex timeout to handle ISR cases
    // If called from ISR, xSemaphoreTake will fail safely
    
    // Payload format: [level (1 byte)] [message (rest)]
    if (length < 2) return;
    
    BrewOSLogLevel level = (BrewOSLogLevel)payload[0];
    if (level > BREWOS_LOG_DEBUG) level = BREWOS_LOG_INFO;
    
    // Copy message (ensure null termination, truncate if too long)
    char message[200]; // Sufficient size for log messages
    size_t msgLen = length - 1;
    if (msgLen >= sizeof(message)) msgLen = sizeof(message) - 1;
    memcpy(message, &payload[1], msgLen);
    message[msgLen] = '\0';
    
    // Add to log buffer
    addLog(level, LOG_SOURCE_PICO, message);
    
    // Also broadcast via WebSocket (same logic as ESP32 logs)
    // INFO and above, or DEBUG if debug logs enabled
    bool shouldBroadcast = (level >= BREWOS_LOG_INFO);
    if (level == BREWOS_LOG_DEBUG) {
        // Check if debug logs are enabled in settings
        shouldBroadcast = State.settings().system.debugLogsEnabled;
    }
    
    if (shouldBroadcast) {
        const char* level_name;
        switch(level) {
            case BREWOS_LOG_DEBUG: level_name = "debug"; break;
            case BREWOS_LOG_INFO:  level_name = "info"; break;
            case BREWOS_LOG_WARN:  level_name = "warn"; break;
            case BREWOS_LOG_ERROR: level_name = "error"; break;
            default: level_name = "info"; break;
        }
        // Broadcast Pico logs with source="pico" to distinguish from ESP32 logs
        if (webServer) {
            webServer->broadcastLogMessageWithSource(level_name, message, "pico");
        }
    }
}

// Helper function for LOG macros (takes int to avoid circular dependency)
void log_manager_add_logf(int level, LogSource source, const char* format, ...) {
    if (!g_logManager || !g_logManager->isEnabled()) {
        return;
    }
    
    // CRITICAL FIX: Check if we are in an ISR context
    // Calling xSemaphoreTake from an ISR causes a panic/crash on ESP32/FreeRTOS
    if (xPortInIsrContext()) {
        // We cannot take a mutex or safely use vsnprintf in an ISR
        // Skip logging from ISR context to avoid crashes
        return;
    }
    
    // Use larger buffer to avoid truncation (LOG_ENTRY_MAX_SIZE is 256, so use 200 for message)
    char message[200];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    g_logManager->addLog((BrewOSLogLevel)level, source, message);
}

bool LogManager::saveToFlash() {
    if (!_buffer) {
        return false;  // Buffer not allocated
    }
    
    // Save even if size is 0 (might have crash info just added)
    // But skip if not enabled (unless we're in panic context - handled separately)
    if (!_enabled && _size == 0) {
        return false;
    }
    
    // Try to use LittleFS - first try if already mounted, then try to mount
    bool fsMounted = false;
    // Try to open a file to check if already mounted
    File testFile = LittleFS.open("/logs.bin", "r");
    if (testFile) {
        testFile.close();
        fsMounted = true;
    } else {
        // Try to mount (don't format in panic context)
        fsMounted = LittleFS.begin(false);
    }
    
    if (!fsMounted) {
        return false;  // LittleFS not available
    }
    
    // Try to get mutex (with short timeout - skip in panic context if can't get it)
    bool hasMutex = false;
    if (_mutex && xSemaphoreTake(_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        hasMutex = true;
    }
    // Continue even without mutex in panic context
    
    bool success = false;
    
    // Get current RAM buffer as text (use unsafe version since we already have mutex)
    String ramLogs;
    if (hasMutex) {
        ramLogs = getLogsUnsafe();  // Use unsafe version to avoid deadlock
    } else {
        // If we don't have mutex, try to get logs normally (will try to acquire mutex)
        ramLogs = getLogs();
    }
    
    if (ramLogs.length() == 0 && _size == 0) {
        if (hasMutex) {
            xSemaphoreGive(_mutex);
        }
        return false;  // Nothing to save
    }
    
    // Append RAM logs to flash file (text format, not binary)
    File file = LittleFS.open("/logs.txt", "a");  // Append mode
    if (file) {
        // Write RAM buffer logs
        file.print(ramLogs);
        file.flush();
        file.close();
        
        // Check file size and truncate if needed (keep only most recent logs)
        File sizeCheck = LittleFS.open("/logs.txt", "r");
        if (sizeCheck) {
            size_t fileSize = sizeCheck.size();
            sizeCheck.close();
            
            if (fileSize > LOG_FLASH_MAX_SIZE) {
                // Read entire file, keep only the last LOG_FLASH_MAX_SIZE bytes
                File readFile = LittleFS.open("/logs.txt", "r");
                if (readFile) {
                    String allLogs = readFile.readString();
                    readFile.close();
                    
                    // Keep only the last LOG_FLASH_MAX_SIZE bytes (most recent logs)
                    if (allLogs.length() > LOG_FLASH_MAX_SIZE) {
                        allLogs = allLogs.substring(allLogs.length() - LOG_FLASH_MAX_SIZE);
                        // Find first newline to avoid cutting in the middle of a log entry
                        int firstNewline = allLogs.indexOf('\n');
                        if (firstNewline > 0) {
                            allLogs = allLogs.substring(firstNewline + 1);
                        }
                    }
                    
                    // Write truncated logs back
                    File truncFile = LittleFS.open("/logs.txt", "w");
                    if (truncFile) {
                        truncFile.print(allLogs);
                        truncFile.flush();
                        truncFile.close();
                        success = true;
                    }
                }
            } else {
                success = true;
            }
        }
    }
    
    // Also keep binary format for restoreFromFlash (for backward compatibility)
    // But primary storage is now text format
    File binFile = LittleFS.open("/logs.bin", "w");
    if (binFile) {
        // Write header: size (4 bytes), head (4 bytes), tail (4 bytes), wrapped (1 byte)
        binFile.write((uint8_t*)&_size, sizeof(_size));
        binFile.write((uint8_t*)&_head, sizeof(_head));
        binFile.write((uint8_t*)&_tail, sizeof(_tail));
        binFile.write((uint8_t*)&_wrapped, sizeof(_wrapped));
        
        if (_size > 0) {
            if (_wrapped) {
                size_t firstPart = LOG_BUFFER_SIZE - _tail;
                binFile.write((uint8_t*)(_buffer + _tail), firstPart);
                binFile.write((uint8_t*)_buffer, _head);
            } else {
                binFile.write((uint8_t*)_buffer, _head);
            }
        }
        binFile.close();
    }
    
    if (hasMutex) {
        xSemaphoreGive(_mutex);
    }
    
    return success;
}

bool LogManager::restoreFromFlash() {
    if (!_buffer) {
        return false;  // Buffer not allocated yet
    }
    
    // Check if LittleFS is mounted
    if (!LittleFS.begin(false)) {  // false = don't format if mount fails
        return false;
    }
    
    // Check if log file exists
    if (!LittleFS.exists("/logs.bin")) {
        return false;  // No saved logs
    }
    
    File file = LittleFS.open("/logs.bin", "r");
    if (!file) {
        return false;
    }
    
    // Read header
    size_t savedSize = 0;
    size_t savedHead = 0;
    size_t savedTail = 0;
    bool savedWrapped = false;
    
    if (file.read((uint8_t*)&savedSize, sizeof(savedSize)) != sizeof(savedSize) ||
        file.read((uint8_t*)&savedHead, sizeof(savedHead)) != sizeof(savedHead) ||
        file.read((uint8_t*)&savedTail, sizeof(savedTail)) != sizeof(savedTail) ||
        file.read((uint8_t*)&savedWrapped, sizeof(savedWrapped)) != sizeof(savedWrapped)) {
        file.close();
        return false;  // Invalid header
    }
    
    // Validate header values
    if (savedHead >= LOG_BUFFER_SIZE || savedTail >= LOG_BUFFER_SIZE || savedSize > LOG_BUFFER_SIZE) {
        file.close();
        return false;  // Invalid data
    }
    
    // Clear buffer first
    memset(_buffer, 0, LOG_BUFFER_SIZE);
    
    // Read buffer data
    bool success = false;
    if (savedWrapped) {
        // Read from tail to end, then from start to head
        size_t firstPart = LOG_BUFFER_SIZE - savedTail;
        if (file.read((uint8_t*)(_buffer + savedTail), firstPart) == firstPart &&
            file.read((uint8_t*)_buffer, savedHead) == savedHead) {
            _head = savedHead;
            _tail = savedTail;
            _size = savedSize;
            _wrapped = savedWrapped;
            success = true;
        }
    } else {
        // Read from start to head
        if (file.read((uint8_t*)_buffer, savedHead) == savedHead) {
            _head = savedHead;
            _tail = savedTail;
            _size = savedSize;
            _wrapped = savedWrapped;
            success = true;
        }
    }
    
    file.close();
    
    if (success) {
        Serial.printf("[LogManager] Restored %zu bytes of logs from flash\n", _size);
    }
    
    return success;
}

void LogManager::loop() {
    if (!_enabled || !_buffer) {
        return;  // Not enabled, nothing to do
    }
    
    unsigned long now = millis();
    
    // Auto-save every 30 seconds
    const unsigned long AUTO_SAVE_INTERVAL_MS = 30000;  // 30 seconds
    
    // Check if buffer is 50% full (trigger immediate save)
    bool bufferNearlyFull = false;
    if (_wrapped) {
        // Buffer is full, always save
        bufferNearlyFull = true;
    } else {
        // Check if buffer is 50% full
        size_t fillPercent = (_head * 100) / LOG_BUFFER_SIZE;
        bufferNearlyFull = (fillPercent >= 50);
    }
    
    // Check if it's time to save (time-based or buffer full)
    bool shouldSave = false;
    if (bufferNearlyFull) {
        shouldSave = true;
    } else if (_lastSaveTime == 0) {
        // First time, initialize save time (don't save immediately)
        _lastSaveTime = now;
        return;
    } else if (now - _lastSaveTime >= AUTO_SAVE_INTERVAL_MS) {
        shouldSave = true;
    }
    
    if (shouldSave && _size > 0) {
        // Save to flash (non-blocking, uses mutex with timeout)
        if (saveToFlash()) {
            _lastSaveTime = now;
            // Log the save (but don't use LOG macros to avoid recursion)
            // Just update the timestamp, user can see it in the logs
        }
    }
}

String LogManager::getLogsFromFlash() {
    String result;
    
    // Check if LittleFS is mounted
    bool fsMounted = false;
    File testFile = LittleFS.open("/logs.txt", "r");
    if (testFile) {
        testFile.close();
        fsMounted = true;
    } else {
        fsMounted = LittleFS.begin(false);
    }
    
    if (!fsMounted) {
        return String("ERROR: LittleFS not available");
    }
    
    // Read from text file (primary storage)
    File file = LittleFS.open("/logs.txt", "r");
    if (file) {
        result = file.readString();
        file.close();
        return result;
    }
    
    // Fallback to binary format (for backward compatibility)
    File binFile = LittleFS.open("/logs.bin", "r");
    if (!binFile) {
        return String("ERROR: Could not open log file");
    }
    
    // Read header
    size_t savedSize = 0;
    size_t savedHead = 0;
    size_t savedTail = 0;
    bool savedWrapped = false;
    
    if (binFile.read((uint8_t*)&savedSize, sizeof(savedSize)) != sizeof(savedSize) ||
        binFile.read((uint8_t*)&savedHead, sizeof(savedHead)) != sizeof(savedHead) ||
        binFile.read((uint8_t*)&savedTail, sizeof(savedTail)) != sizeof(savedTail) ||
        binFile.read((uint8_t*)&savedWrapped, sizeof(savedWrapped)) != sizeof(savedWrapped)) {
        binFile.close();
        return String("ERROR: Invalid log file header");
    }
    
    // Validate header
    if (savedHead >= LOG_BUFFER_SIZE || savedTail >= LOG_BUFFER_SIZE || savedSize > LOG_BUFFER_SIZE) {
        binFile.close();
        return String("ERROR: Invalid log file data");
    }
    
    // Read buffer data - use temporary buffer to avoid O(N²) character-by-character concatenation
    result.reserve(savedSize + 1);
    
    if (savedWrapped) {
        size_t firstPart = LOG_BUFFER_SIZE - savedTail;
        // Read first part into temporary buffer
        char* tempBuf = (char*)malloc(firstPart);
        if (tempBuf) {
            if (binFile.read((uint8_t*)tempBuf, firstPart) == firstPart) {
                result.concat(tempBuf, firstPart);
            }
            free(tempBuf);
        }
        // Read second part
        if (savedHead > 0) {
            tempBuf = (char*)malloc(savedHead);
            if (tempBuf) {
                if (binFile.read((uint8_t*)tempBuf, savedHead) == savedHead) {
                    result.concat(tempBuf, savedHead);
                }
                free(tempBuf);
            }
        }
    } else {
        // Read single contiguous block
        if (savedHead > 0) {
            char* tempBuf = (char*)malloc(savedHead);
            if (tempBuf) {
                if (binFile.read((uint8_t*)tempBuf, savedHead) == savedHead) {
                    result.concat(tempBuf, savedHead);
                }
                free(tempBuf);
            }
        }
    }
    
    binFile.close();
    return result;
}

String LogManager::getLogsComplete() {
    // First, flush RAM buffer to flash to ensure we have latest logs
    if (_enabled && _buffer && _size > 0) {
        saveToFlash();
    }
    
    // Then get logs from flash (which has complete history)
    return getLogsFromFlash();
}

void LogManager::addLogDirect(BrewOSLogLevel level, LogSource source, const char* message) {
    // No-op if disabled - but try to write even if not enabled (panic context)
    if (!message) return;
    
    // 1. Write to standard buffer (in case we don't actually crash/reset)
    if (_buffer) {
        // Direct write without mutex - for panic handler use only
        // Format: [timestamp] [SOURCE] LEVEL: message\n
        char entry[LOG_ENTRY_MAX_SIZE];
        unsigned long timestamp = millis();
        
        // Calculate available space for message (leave room for prefix and newline)
        size_t prefixLen = snprintf(entry, sizeof(entry), "[%lu] [%s] %s: ", 
                                     timestamp, sourceToString(source), levelToString(level));
        size_t maxMsgLen = sizeof(entry) - prefixLen - 2; // -2 for \n and \0
        
        // Truncate message if needed
        char truncatedMsg[LOG_ENTRY_MAX_SIZE];
        strncpy(truncatedMsg, message, maxMsgLen);
        truncatedMsg[maxMsgLen] = '\0';
        
        // Format complete entry
        int len = snprintf(entry, sizeof(entry), "[%lu] [%s] %s: %s\n",
                           timestamp,
                           sourceToString(source),
                           levelToString(level),
                           truncatedMsg);
        
        // Write directly to buffer (no mutex - panic context)
        if (len > 0) {
            size_t writeLen = (len < (int)sizeof(entry)) ? len : sizeof(entry) - 1;
            writeToBuffer(entry, writeLen);
        }
    }
    
    // 2. CRITICAL: Write to RTC Buffer (The Flight Recorder)
    // RTC memory survives software resets, so crash logs persist across reboots
    // We use a simple linear append here for safety.
    // If buffer is full, we stop writing to preserve the *first* crash details.
    
    if (rtcLogs.magic != RTC_MAGIC) {
        rtcLogs.magic = RTC_MAGIC;
        rtcLogs.head = 0;
        memset(rtcLogs.data, 0, RTC_LOG_SIZE);
    }
    
    // Format crash log entry (keep it small for panic context)
    char entry[128];
    int len = snprintf(entry, sizeof(entry), "[CRASH] [%s] %s: %s\n", 
                       sourceToString(source), levelToString(level), message);
    
    if (len > 0 && rtcLogs.head + len < RTC_LOG_SIZE) {
        memcpy(&rtcLogs.data[rtcLogs.head], entry, len);
        rtcLogs.head += len;
        rtcLogs.data[rtcLogs.head] = '\0'; // Ensure null termination
    }
}
