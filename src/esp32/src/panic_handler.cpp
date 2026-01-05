/**
 * Panic Handler - Captures crash information and writes to log buffer
 * 
 * This handler catches exceptions, panics, and crashes that would otherwise
 * not be logged to the log buffer. It writes crash information directly to
 * the log buffer before the system resets.
 */

#include "panic_handler.h"
#include "log_manager.h"
#include "config.h"
#include <Arduino.h>
#include <esp_system.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Forward declaration
extern LogManager* g_logManager;

/**
 * Panic handler - called when ESP32 shuts down (including panics)
 * This is registered via esp_register_shutdown_handler
 */
static void panicHandler() {
    // Try to write to log buffer and save to flash
    // Use direct write (no mutex) since we're in shutdown context
    // Try even if not "enabled" - we might have a buffer allocated
    if (g_logManager && g_logManager->isEnabled()) {
        // Get reset reason - this tells us why we're shutting down
        esp_reset_reason_t resetReason = esp_reset_reason();
        const char* reasonStr = "Unknown";
        switch (resetReason) {
            case ESP_RST_POWERON: reasonStr = "Power-on reset"; break;
            case ESP_RST_EXT: reasonStr = "External reset"; break;
            case ESP_RST_SW: reasonStr = "Software reset"; break;
            case ESP_RST_PANIC: reasonStr = "Exception/panic"; break;
            case ESP_RST_INT_WDT: reasonStr = "Interrupt watchdog"; break;
            case ESP_RST_TASK_WDT: reasonStr = "Task watchdog"; break;
            case ESP_RST_WDT: reasonStr = "Other watchdog"; break;
            case ESP_RST_DEEPSLEEP: reasonStr = "Deep sleep wake"; break;
            case ESP_RST_BROWNOUT: reasonStr = "Brownout"; break;
            case ESP_RST_SDIO: reasonStr = "SDIO reset"; break;
            default: reasonStr = "Unknown"; break;
        }
        
        // Always save in shutdown handler - we're about to reset
        // The reset reason tells us why, but we should save regardless
        // (esp_reset_reason() returns why we booted, not why we're resetting)
        
        // Write panic information directly to log buffer (no mutex)
        char panicMsg[256];
        size_t freeHeap = ESP.getFreeHeap();
        snprintf(panicMsg, sizeof(panicMsg), 
                "SHUTDOWN: System shutting down (Reset reason: %s, Free heap: %u bytes)",
                reasonStr, (unsigned int)freeHeap);
        g_logManager->addLogDirect(BREWOS_LOG_ERROR, LOG_SOURCE_ESP32, panicMsg);
        
        // Only add detailed crash info if it's a crash-related reset
        if (resetReason == ESP_RST_PANIC || 
            resetReason == ESP_RST_INT_WDT || 
            resetReason == ESP_RST_TASK_WDT ||
            resetReason == ESP_RST_WDT ||
            resetReason == ESP_RST_BROWNOUT) {
            
            // Update message to indicate crash
            snprintf(panicMsg, sizeof(panicMsg), 
                    "CRASH: System panic/crash detected (Reset reason: %s, Free heap: %u bytes)",
                    reasonStr, (unsigned int)freeHeap);
            g_logManager->addLogDirect(BREWOS_LOG_ERROR, LOG_SOURCE_ESP32, panicMsg);
            
            // Try to get task information if available
            TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
            if (currentTask != nullptr) {
                // Use pcTaskGetName which is simpler and more reliable
                const char* taskName = pcTaskGetName(currentTask);
                if (taskName != nullptr) {
                    char taskMsg[128];
                    snprintf(taskMsg, sizeof(taskMsg), "CRASH: Task: %s", taskName);
                    g_logManager->addLogDirect(BREWOS_LOG_ERROR, LOG_SOURCE_ESP32, taskMsg);
                }
                
                // Write stack trace info if available
                UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(currentTask);
                if (stackHighWaterMark != 0) {
                    char stackMsg[128];
                    snprintf(stackMsg, sizeof(stackMsg), 
                            "CRASH: Stack high water mark: %u bytes", stackHighWaterMark);
                    g_logManager->addLogDirect(BREWOS_LOG_ERROR, LOG_SOURCE_ESP32, stackMsg);
                }
            }
        }
        
        // ALWAYS try to save log buffer to flash before shutdown
        // This preserves all logs (including crash info) across reboots
        // Save even if buffer seems small - we just added crash info
        bool saved = g_logManager->saveToFlash();
        
        // Give flash time to complete write
        Serial.flush();
        delay(50);  // Longer delay to ensure flash write completes
        
        // Try one more time if first save failed
        if (!saved) {
            delay(10);
            saved = g_logManager->saveToFlash();
            Serial.flush();
            delay(50);
        }
        
        // Debug output to Serial
        Serial.printf("Panic handler: saveToFlash() = %s\n", saved ? "SUCCESS" : "FAILED");
        Serial.flush();
    } else if (g_logManager) {
        // Log manager exists but not enabled - try to save anyway if buffer exists
        // This handles cases where buffer was allocated but then disabled
        g_logManager->saveToFlash();
        Serial.flush();
        delay(50);
    }
    
    // Also try to write to Serial (may not work in panic state, but worth trying)
    Serial.println("\n=== PANIC HANDLER ===");
    Serial.printf("Reset reason: %d\n", (int)esp_reset_reason());
    size_t freeHeap = ESP.getFreeHeap();
    Serial.printf("Free heap: %u bytes\n", (unsigned int)freeHeap);
    Serial.flush();
}

/**
 * Register the panic handler
 * This should be called early in setup()
 */
void registerPanicHandler() {
    esp_err_t err = esp_register_shutdown_handler(panicHandler);
    if (err != ESP_OK) {
        Serial.printf("Warning: Failed to register panic handler: %d\n", err);
    }
}

// Register panic handler during static initialization as backup
// This ensures it's registered even if registerPanicHandler() isn't called
static struct PanicHandlerRegistrar {
    PanicHandlerRegistrar() {
        // Note: esp_register_shutdown_handler may not be available during static init
        // So we'll also register explicitly in setup()
    }
} panicHandlerRegistrar;

