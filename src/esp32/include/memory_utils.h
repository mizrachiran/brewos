#ifndef MEMORY_UTILS_H
#define MEMORY_UTILS_H

/**
 * Memory Utilities for ESP32-S3 with PSRAM
 * 
 * Provides custom allocators for ArduinoJson that force large allocations
 * to PSRAM, preserving internal RAM for SSL/WiFi operations.
 */

#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <esp_log.h>

// Tag for ESP-IDF logging
static const char* MEMORY_TAG = "PSRAM";

// =============================================================================
// PSRAM Allocator for ArduinoJson
// =============================================================================

/**
 * Custom allocator that forces ArduinoJson to use PSRAM (SPI RAM).
 * 
 * Use this for large JSON documents (>1KB) to preserve internal heap
 * for SSL handshakes and WiFi buffers which require internal RAM.
 * 
 * Falls back to regular malloc if PSRAM allocation fails.
 */
struct SpiRamAllocator {
    void* allocate(size_t size) {
        // Check if PSRAM is available before trying to allocate
        size_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        
        // Only try PSRAM if it's actually available and has free space
        if (psramFree > 0) {
            void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (ptr) {
                return ptr;
            }
            // Only log if PSRAM exists but allocation failed (indicates fragmentation or exhaustion)
            // Skip logging if PSRAM is not available at all (hardware limitation)
            if (psramFree >= size) {
                // PSRAM exists and has space, but allocation failed - log as warning
                ESP_LOGW(MEMORY_TAG, "Alloc failed: %u bytes (PSRAM free: %u, internal free: %u)",
                         (unsigned)size, 
                         (unsigned)psramFree,
                         (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
            }
        }
        
        // Fallback to internal RAM if PSRAM fails or unavailable
        void* ptr = malloc(size);
        if (!ptr) {
            ESP_LOGE(MEMORY_TAG, "CRITICAL: Both PSRAM and internal alloc failed for %u bytes!", (unsigned)size);
        }
        return ptr;
    }
    
    void deallocate(void* ptr) {
        // heap_caps_free works for both PSRAM and internal RAM allocations
        heap_caps_free(ptr);
    }
    
    void* reallocate(void* ptr, size_t new_size) {
        // Check if PSRAM is available before trying to reallocate
        size_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        
        // Only try PSRAM if it's actually available and has free space
        if (psramFree > 0) {
            void* new_ptr = heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (new_ptr) {
                return new_ptr;
            }
            // Only log if PSRAM exists but reallocation failed
            if (psramFree >= new_size) {
                ESP_LOGW(MEMORY_TAG, "Realloc failed: %u bytes (PSRAM free: %u)",
                         (unsigned)new_size, 
                         (unsigned)psramFree);
            }
        }
        
        // Fallback: allocate new block in internal RAM
        void* new_ptr = malloc(new_size);
        if (!new_ptr) {
            ESP_LOGE(MEMORY_TAG, "CRITICAL: Realloc failed completely for %u bytes!", (unsigned)new_size);
        }
        return new_ptr;
    }
};

/**
 * JSON Document type that allocates in PSRAM.
 * 
 * Usage:
 *   SpiRamJsonDocument doc(4096);  // 4KB document in PSRAM
 *   doc["key"] = "value";
 * 
 * Use this instead of JsonDocument for:
 * - Status broadcasts (broadcastFullStatus)
 * - Settings serialization
 * - Any JSON document larger than 1KB
 * 
 * Keep using StaticJsonDocument<N> for:
 * - Small fixed-size documents (WebSocket commands, log messages)
 * - Stack-allocated documents in tight loops
 * 
 * Note: BasicJsonDocument is deprecated in ArduinoJson 7.x but is required
 * to use a custom allocator. The standard JsonDocument uses the default allocator.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
using SpiRamJsonDocument = BasicJsonDocument<SpiRamAllocator>;
#pragma GCC diagnostic pop

// =============================================================================
// Memory Allocation Helpers
// =============================================================================

/**
 * Allocate a buffer in PSRAM with fallback to internal RAM.
 * 
 * @param size Size in bytes to allocate
 * @return Pointer to allocated memory, or nullptr on failure
 */
inline void* psram_malloc(size_t size) {
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!ptr) {
        ptr = malloc(size);
    }
    return ptr;
}

/**
 * Allocate a buffer strictly in internal RAM (for DMA, ISR, etc).
 * 
 * @param size Size in bytes to allocate
 * @return Pointer to allocated memory, or nullptr on failure
 */
inline void* internal_malloc(size_t size) {
    return heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

/**
 * Free memory allocated with psram_malloc or internal_malloc.
 * Also works with regular malloc.
 */
inline void safe_free(void* ptr) {
    if (ptr) {
        heap_caps_free(ptr);
    }
}

// =============================================================================
// Memory Statistics
// =============================================================================

/**
 * Get the largest free block in internal RAM.
 * This indicates heap fragmentation - a small value relative to total free
 * means the heap is fragmented.
 * 
 * @return Size of largest contiguous free block in internal RAM
 */
inline size_t get_largest_free_block() {
    return heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

/**
 * Calculate heap fragmentation percentage.
 * 0% = no fragmentation (largest block == free heap)
 * 100% = completely fragmented (no usable blocks)
 * 
 * @param free_heap Total free heap size
 * @param largest_block Largest contiguous free block
 * @return Fragmentation percentage (0-100)
 */
inline int calculate_fragmentation(size_t free_heap, size_t largest_block) {
    if (free_heap == 0) return 100;
    return 100 - (int)((largest_block * 100) / free_heap);
}

// =============================================================================
// JSON Parsing Optimization Guidelines
// =============================================================================

/**
 * JSON Filter Usage for Large Payloads
 * 
 * When parsing large JSON payloads where only a few fields are needed,
 * use ArduinoJson's Filter feature to reduce memory allocation:
 * 
 * Example - Parsing only specific fields from a large payload:
 * 
 *   // Define filter document (only fields you need)
 *   StaticJsonDocument<64> filter;
 *   filter["schedule"]["enabled"] = true;
 *   filter["schedule"]["time"] = true;
 *   // Other fields will be ignored during parsing
 *   
 *   // Parse with filter
 *   JsonDocument doc;
 *   deserializeJson(doc, payload, DeserializationOption::Filter(filter));
 *   
 * Benefits:
 * - Reduces memory allocation (only stores filtered fields)
 * - Faster parsing (skips unwanted fields)
 * - Lower PSRAM/internal RAM pressure
 * 
 * When to use:
 * - Large incoming payloads (>1KB) where only a few fields are needed
 * - Memory-constrained situations (low heap)
 * - High-frequency parsing operations
 * 
 * When NOT to use:
 * - Small payloads (<512 bytes) - overhead not worth it
 * - When all fields are needed
 * - Simple command messages
 */

#endif // MEMORY_UTILS_H

