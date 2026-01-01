#ifndef JSON_BUFFER_POOL_H
#define JSON_BUFFER_POOL_H

#include <stddef.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * JSON Buffer Pool
 * 
 * Pre-allocated buffer pool for JSON serialization to avoid heap fragmentation.
 * Useful for frequent JSON operations (diagnostics, broadcasts, etc.).
 * 
 * Thread-safe: Uses mutex protection for allocation/deallocation.
 * Falls back to dynamic allocation if pool is exhausted.
 */
class JsonBufferPool {
public:
    /**
     * Get singleton instance
     */
    static JsonBufferPool& instance();
    
    /**
     * Allocate a buffer from the pool
     * @param size Required buffer size in bytes
     * @return Pointer to buffer, or nullptr if allocation fails
     * 
     * Note: Buffer must be freed with release() when done
     */
    char* allocate(size_t size);
    
    /**
     * Release a buffer back to the pool
     * @param buffer Pointer to buffer previously allocated with allocate()
     */
    void release(char* buffer);
    
    /**
     * Get statistics about pool usage
     */
    struct Stats {
        size_t totalBuffers;
        size_t availableBuffers;
        size_t bufferSize;
    };
    Stats getStats() const;

private:
    JsonBufferPool();
    ~JsonBufferPool() = default;
    
    // Non-copyable
    JsonBufferPool(const JsonBufferPool&) = delete;
    JsonBufferPool& operator=(const JsonBufferPool&) = delete;
    
    static const size_t POOL_SIZE = 3;  // Number of buffers in pool
    static const size_t BUFFER_SIZE = 1024;  // Size of each buffer (1KB)
    
    char _buffers[POOL_SIZE][BUFFER_SIZE];
    bool _inUse[POOL_SIZE];
    SemaphoreHandle_t _mutex;
    
    static JsonBufferPool* _instance;
};

#endif // JSON_BUFFER_POOL_H

