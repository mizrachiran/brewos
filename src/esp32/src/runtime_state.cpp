#include "runtime_state.h"
#include "log_manager.h"
#include <string.h>

RuntimeState* RuntimeState::_instance = nullptr;

RuntimeState& RuntimeState::instance() {
    if (_instance == nullptr) {
        static RuntimeState state;
        _instance = &state;
    }
    return *_instance;
}

RuntimeState::RuntimeState()
    : _active(&_bufferA)
    , _writing(&_bufferB)
    , _mutex(nullptr) {
    // Initialize buffers to zero
    memset(&_bufferA, 0, sizeof(ui_state_t));
    memset(&_bufferB, 0, sizeof(ui_state_t));
}

void RuntimeState::begin() {
    if (_mutex == nullptr) {
        _mutex = xSemaphoreCreateMutex();
        if (_mutex == nullptr) {
            LOG_E("CRITICAL: Failed to create RuntimeState mutex!");
        } else {
            LOG_I("RuntimeState initialized");
        }
    }
}

const ui_state_t& RuntimeState::get() const {
    // Lock-free read - cast away volatile for const reference
    // Safe because we're only reading and pointer swap is atomic
    return *const_cast<ui_state_t*>(_active);
}

ui_state_t& RuntimeState::beginUpdate() {
    if (_mutex == nullptr) {
        LOG_E("RuntimeState not initialized - call begin() first");
        // Return active buffer as fallback (not ideal, but prevents crash)
        return *const_cast<ui_state_t*>(_active);
    }
    
    // Take mutex - use portMAX_DELAY to ensure updates are never dropped
    if (xSemaphoreTake(_mutex, portMAX_DELAY) != pdTRUE) {
        LOG_E("CRITICAL: Failed to acquire RuntimeState mutex in beginUpdate");
        // Return active buffer as fallback (not ideal, but prevents crash)
        return *const_cast<ui_state_t*>(_active);
    }
    
    // Copy current state to writing buffer to preserve fields not updated in this call
    // This copy happens INSIDE the mutex, so any secondary updates that happened
    // before we took the mutex are included in this copy
    ui_state_t* activePtr = const_cast<ui_state_t*>(_active);
    ui_state_t* writingPtr = const_cast<ui_state_t*>(_writing);
    *writingPtr = *activePtr;
    
    // Return reference to writing buffer
    return *writingPtr;
}

void RuntimeState::endUpdate() {
    if (_mutex == nullptr) {
        LOG_E("RuntimeState not initialized - call begin() first");
        return;
    }
    
    // Swap buffers atomically (inside mutex)
    swapBuffers();
    
    // Release mutex
    xSemaphoreGive(_mutex);
}

void RuntimeState::updateWiFi(bool connected, bool apMode, int rssi) {
    if (_mutex == nullptr) return;
    
    // Use portMAX_DELAY to ensure state updates are never dropped
    // Critical state updates must not be silently discarded
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        // Cast away volatile for field access (safe inside mutex)
        ui_state_t* activePtr = const_cast<ui_state_t*>(_active);
        ui_state_t* writingPtr = const_cast<ui_state_t*>(_writing);
        
        activePtr->wifi_connected = connected;
        activePtr->wifi_ap_mode = apMode;
        if (rssi != 0) activePtr->wifi_rssi = rssi;
        
        writingPtr->wifi_connected = connected;
        writingPtr->wifi_ap_mode = apMode;
        if (rssi != 0) writingPtr->wifi_rssi = rssi;
        
        xSemaphoreGive(_mutex);
    } else {
        // Defensive check - should never happen with portMAX_DELAY
        LOG_E("CRITICAL: Failed to acquire RuntimeState mutex in updateWiFi");
    }
}

void RuntimeState::updatePicoConnection(bool connected) {
    if (_mutex == nullptr) return;
    
    // Use portMAX_DELAY to ensure state updates are never dropped
    // Critical state updates must not be silently discarded
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        // Cast away volatile for field access (safe inside mutex)
        ui_state_t* activePtr = const_cast<ui_state_t*>(_active);
        ui_state_t* writingPtr = const_cast<ui_state_t*>(_writing);
        
        activePtr->pico_connected = connected;
        writingPtr->pico_connected = connected;
        
        xSemaphoreGive(_mutex);
    } else {
        // Defensive check - should never happen with portMAX_DELAY
        LOG_E("CRITICAL: Failed to acquire RuntimeState mutex in updatePicoConnection");
    }
}

void RuntimeState::updateScaleConnection(bool connected) {
    if (_mutex == nullptr) return;
    
    // Use portMAX_DELAY to ensure state updates are never dropped
    // Critical state updates must not be silently discarded
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        // Cast away volatile for field access (safe inside mutex)
        ui_state_t* activePtr = const_cast<ui_state_t*>(_active);
        ui_state_t* writingPtr = const_cast<ui_state_t*>(_writing);
        
        activePtr->scale_connected = connected;
        writingPtr->scale_connected = connected;
        
        xSemaphoreGive(_mutex);
    } else {
        // Defensive check - should never happen with portMAX_DELAY
        LOG_E("CRITICAL: Failed to acquire RuntimeState mutex in updateScaleConnection");
    }
}

void RuntimeState::swapBuffers() {
    // Atomic pointer swap (single 32-bit write on ESP32)
    // volatile qualifier ensures compiler doesn't reorder these assignments
    // On ESP32, 32-bit aligned pointer writes are guaranteed atomic by the architecture
    volatile ui_state_t* temp = _active;
    _active = _writing;
    _writing = temp;
    // Memory barrier not needed - mutex already provides synchronization
}

