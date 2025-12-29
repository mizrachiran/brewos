#include "log_manager.h"
#include "protocol_defs.h"
#include <esp_heap_caps.h>
#include <stdarg.h>

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
    , _mutex(nullptr)
    , _picoLogForwarding(false)
    , _enabled(false)
{
    // Create mutex early (small memory footprint)
    _mutex = xSemaphoreCreateMutex();
    
    // Set global pointer
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
    
    memset(_buffer, 0, LOG_BUFFER_SIZE);
    _head = 0;
    _tail = 0;
    _size = 0;
    _wrapped = false;
    _enabled = true;
    
    Serial.printf("[LogManager] Enabled - allocated %dKB buffer\n", LOG_BUFFER_SIZE / 1024);
    addLog(BREWOS_LOG_INFO, LOG_SOURCE_ESP32, "Log buffer enabled (50KB)");
    
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
        _buffer[_head] = data[i];
        _head = (_head + 1) % LOG_BUFFER_SIZE;
        
        // If we've caught up to tail, advance tail (discard oldest)
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
        
        if (_head == 0 && i > 0) {
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
    // No-op if disabled
    if (!_enabled || !_buffer || !message) return;
    
    // Take mutex (with timeout to avoid deadlock)
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return; // Skip this log entry if can't get mutex
    }
    
    // Format: [timestamp] [SOURCE] LEVEL: message\n
    char entry[LOG_ENTRY_MAX_SIZE];
    unsigned long timestamp = millis();
    int len = snprintf(entry, sizeof(entry), "[%lu] [%s] %s: %s\n",
                       timestamp,
                       sourceToString(source),
                       levelToString(level),
                       message);
    
    if (len > 0 && len < (int)sizeof(entry)) {
        writeToBuffer(entry, len);
    }
    
    xSemaphoreGive(_mutex);
}

void LogManager::addLogf(BrewOSLogLevel level, LogSource source, const char* format, ...) {
    // No-op if disabled
    if (!_enabled || !_buffer || !format) return;
    
    char message[LOG_ENTRY_MAX_SIZE - 32]; // Leave room for timestamp/source prefix
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    addLog(level, source, message);
}

String LogManager::getLogs() {
    if (!_enabled || !_buffer) return String();
    
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return String("ERROR: Could not acquire log mutex");
    }
    
    String result;
    result.reserve(_size + 1);
    
    if (_wrapped) {
        // Buffer has wrapped - read from tail to end, then from start to head
        for (size_t i = _tail; i < LOG_BUFFER_SIZE; i++) {
            if (_buffer[i]) result += _buffer[i];
        }
        for (size_t i = 0; i < _head; i++) {
            if (_buffer[i]) result += _buffer[i];
        }
    } else {
        // Buffer hasn't wrapped - simple copy from start to head
        for (size_t i = 0; i < _head; i++) {
            if (_buffer[i]) result += _buffer[i];
        }
    }
    
    xSemaphoreGive(_mutex);
    return result;
}

size_t LogManager::getLogsSize() {
    if (!_enabled) return 0;
    return _size;
}

void LogManager::clear() {
    if (!_enabled || !_buffer) return;
    
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
    
    // Payload format: [level (1 byte)] [message (rest)]
    if (length < 2) return;
    
    BrewOSLogLevel level = (BrewOSLogLevel)payload[0];
    if (level > BREWOS_LOG_DEBUG) level = BREWOS_LOG_INFO;
    
    // Copy message (ensure null termination)
    char message[LOG_ENTRY_MAX_SIZE];
    size_t msgLen = length - 1;
    if (msgLen >= sizeof(message)) msgLen = sizeof(message) - 1;
    memcpy(message, &payload[1], msgLen);
    message[msgLen] = '\0';
    
    addLog(level, LOG_SOURCE_PICO, message);
}
