#ifndef RUNTIME_STATE_H
#define RUNTIME_STATE_H

#include "ui/ui.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * RuntimeState
 * 
 * Encapsulates double-buffered state management for thread-safe access.
 * 
 * Uses double buffering to prevent data tearing:
 * - Writers update the inactive buffer, then atomically swap the pointer
 * - Readers always read from the active buffer (lock-free, no mutex needed)
 * - Mutex protects buffer operations to prevent lost updates from secondary writers
 * 
 * Thread-safe: All write operations are protected by mutex.
 * Lock-free reads: get() is lock-free and safe for concurrent access.
 */
class RuntimeState {
public:
    /**
     * Get singleton instance
     */
    static RuntimeState& instance();
    
    /**
     * Initialize the runtime state (creates mutex)
     * Must be called before any other operations
     */
    void begin();
    
    /**
     * Get current machine state (for readers) - thread-safe, lock-free
     * Returns const reference to active buffer
     * Safe for concurrent reads without mutex
     */
    const ui_state_t& get() const;
    
    /**
     * Begin a state update transaction
     * Takes mutex and returns reference to writing buffer
     * Must be paired with endUpdate() to complete the transaction
     * 
     * Usage:
     *   auto& state = runtimeState().beginUpdate();
     *   // ... modify state ...
     *   runtimeState().endUpdate();
     */
    ui_state_t& beginUpdate();
    
    /**
     * End a state update transaction
     * Swaps buffers atomically and releases mutex
     * Must be called after beginUpdate()
     */
    void endUpdate();
    
    /**
     * Update WiFi connection state (thread-safe helper)
     * Updates both buffers atomically to prevent lost updates
     */
    void updateWiFi(bool connected, bool apMode, int rssi);
    
    /**
     * Update Pico connection state (thread-safe helper)
     * Updates both buffers atomically to prevent lost updates
     */
    void updatePicoConnection(bool connected);
    
    /**
     * Update scale connection state (thread-safe helper)
     * Updates both buffers atomically to prevent lost updates
     */
    void updateScaleConnection(bool connected);

private:
    RuntimeState();
    ~RuntimeState() = default;
    
    // Non-copyable
    RuntimeState(const RuntimeState&) = delete;
    RuntimeState& operator=(const RuntimeState&) = delete;
    
    // Double buffers
    ui_state_t _bufferA;
    ui_state_t _bufferB;
    
    // Volatile pointers for atomic swapping
    // Use volatile to prevent compiler reordering of pointer assignments
    // On ESP32, 32-bit pointer writes are atomic, but volatile ensures memory visibility
    volatile ui_state_t* _active;   // Active buffer (readers)
    volatile ui_state_t* _writing;  // Inactive buffer (writers)
    
    // Mutex protects buffer copy/swap operations
    SemaphoreHandle_t _mutex;
    
    /**
     * Swap active and writing buffers atomically
     * Must be called while holding mutex
     */
    void swapBuffers();
    
    static RuntimeState* _instance;
};

/**
 * Convenience function to access singleton instance
 */
inline RuntimeState& runtimeState() {
    return RuntimeState::instance();
}

#endif // RUNTIME_STATE_H

