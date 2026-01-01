#include "json_buffer_pool.h"
#include <esp_heap_caps.h>
#include <string.h>

JsonBufferPool* JsonBufferPool::_instance = nullptr;

JsonBufferPool& JsonBufferPool::instance() {
    if (_instance == nullptr) {
        static JsonBufferPool pool;
        _instance = &pool;
    }
    return *_instance;
}

JsonBufferPool::JsonBufferPool() {
    // Initialize all buffers as available
    for (size_t i = 0; i < POOL_SIZE; i++) {
        _inUse[i] = false;
        _buffers[i][0] = '\0';  // Initialize to empty string
    }
    
    // Create mutex for thread-safe access
    _mutex = xSemaphoreCreateMutex();
    if (_mutex == nullptr) {
        // Should never happen, but handle gracefully
        // Pool will still work, just not thread-safe
    }
}

char* JsonBufferPool::allocate(size_t size) {
    if (size > BUFFER_SIZE) {
        // Requested size is larger than pool buffers
        // Fall back to dynamic allocation
        return (char*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    
    if (_mutex == nullptr) {
        // Mutex creation failed - fall back to dynamic allocation
        return (char*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    
    if (xSemaphoreTake(_mutex, portMAX_DELAY) != pdTRUE) {
        // Should never happen with portMAX_DELAY, but handle gracefully
        return (char*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    
    // Find an available buffer
    for (size_t i = 0; i < POOL_SIZE; i++) {
        if (!_inUse[i]) {
            _inUse[i] = true;
            xSemaphoreGive(_mutex);
            return _buffers[i];
        }
    }
    
    // All buffers in use - fall back to dynamic allocation
    xSemaphoreGive(_mutex);
    return (char*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void JsonBufferPool::release(char* buffer) {
    if (buffer == nullptr) {
        return;
    }
    
    // Check if buffer is from pool
    bool isPoolBuffer = false;
    for (size_t i = 0; i < POOL_SIZE; i++) {
        if (buffer == _buffers[i]) {
            isPoolBuffer = true;
            
            if (_mutex != nullptr && xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
                _inUse[i] = false;
                _buffers[i][0] = '\0';  // Clear buffer
                xSemaphoreGive(_mutex);
            }
            break;
        }
    }
    
    // If not a pool buffer, it was dynamically allocated - free it
    if (!isPoolBuffer) {
        heap_caps_free(buffer);
    }
}

JsonBufferPool::Stats JsonBufferPool::getStats() const {
    Stats stats;
    stats.totalBuffers = POOL_SIZE;
    stats.bufferSize = BUFFER_SIZE;
    stats.availableBuffers = 0;
    
    if (_mutex != nullptr && xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        for (size_t i = 0; i < POOL_SIZE; i++) {
            if (!_inUse[i]) {
                stats.availableBuffers++;
            }
        }
        xSemaphoreGive(_mutex);
    }
    
    return stats;
}

