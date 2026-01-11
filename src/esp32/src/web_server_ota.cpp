#include "web_server.h"
#include "config.h"
#include "pico_uart.h"
#if SWD_SUPPORTED
#include "pico_swd.h"  // SWD support (only when SWD pins are physically wired)
#endif
#include "cloud_connection.h"
#include "mqtt_client.h"
#include "scale/scale_manager.h"
#include "power_meter/power_meter_manager.h"
#include "notifications/notification_manager.h"
#include "state/state_manager.h"
#include "log_manager.h"  // For Pico log forwarding during OTA
#if ENABLE_SCREEN
#include "display/display.h"
#endif
#include <LittleFS.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <esp_partition.h>
#include <esp_task_wdt.h>
#include <Arduino.h>  // Must be included first for ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/portmacro.h>  // For portTICK_PERIOD_MS
#include <ArduinoJson.h>
#include <esp_ota_ops.h>
#include <FS.h>
#include <AsyncTCP.h>
#include <AsyncWebSocket.h>
#include <memory>
#include <driver/gpio.h>  // For gpio_reset_pin() to detach pins from UART2

// Forward declare PicoUART class if not already included
class PicoUART;

// External references to global service instances (for pausing during OTA)
extern MQTTClient* mqttClient;
extern ScaleManager* scaleManager;
extern PowerMeterManager* powerMeterManager;
extern NotificationManager* notificationManager;

// =============================================================================
// OTA Configuration Constants
// =============================================================================

// Watchdog timeout during OTA (seconds) - long enough for slow downloads
static constexpr uint32_t OTA_WDT_TIMEOUT_SECONDS = 60;

// Default watchdog timeout (seconds) - restored after OTA
// Increased to 10s to allow longer blocking operations (MQTT tests, SSL connections)
// while still catching real hangs quickly
static constexpr uint32_t DEFAULT_WDT_TIMEOUT_SECONDS = 10;

// Console log interval during download (ms)
static constexpr uint32_t OTA_CONSOLE_LOG_INTERVAL_MS = 5000;

// Pico OTA specific settings
static constexpr uint32_t PICO_RESET_DELAY_MS = 2000;

// Minimum contiguous memory needed for SSL OTA (bytes)
// SSL needs ~20KB for buffers (16KB in + 4KB out) plus ~10KB overhead
// 30KB is sufficient when running in minimal boot mode
static constexpr size_t OTA_MIN_CONTIGUOUS_HEAP = 30000;

// NVS namespace and keys for pending OTA
static const char* OTA_NVS_NAMESPACE = "ota";
static const char* OTA_NVS_KEY_VERSION = "pending_ver";
static const char* OTA_NVS_KEY_RETRIES = "retries";

// Maximum OTA boot retries before giving up (prevents crash loops)
static constexpr uint8_t OTA_MAX_BOOT_RETRIES = 2;

// =============================================================================
// Pending OTA Management (for reboot-first approach)
// =============================================================================

/**
 * @brief Check if there's a pending OTA request saved in NVS
 * @param version Output string for the version to update to
 * @return true if pending OTA exists
 */
bool hasPendingOTA(String& version) {
    Preferences prefs;
    if (!prefs.begin(OTA_NVS_NAMESPACE, true)) {  // Read-only
        return false;
    }
    version = prefs.getString(OTA_NVS_KEY_VERSION, "");
    prefs.end();
    return version.length() > 0;
}

/**
 * @brief Get the current OTA boot retry count
 * @return Number of times OTA boot has been attempted
 */
uint8_t getPendingOTARetries() {
    Preferences prefs;
    uint8_t retries = 0;
    if (prefs.begin(OTA_NVS_NAMESPACE, true)) {  // Read-only
        retries = prefs.getUChar(OTA_NVS_KEY_RETRIES, 0);
        prefs.end();
    }
    return retries;
}

/**
 * @brief Increment and save the OTA boot retry count
 * @return New retry count after increment
 */
uint8_t incrementPendingOTARetries() {
    Preferences prefs;
    uint8_t retries = 0;
    if (prefs.begin(OTA_NVS_NAMESPACE, false)) {  // Read-write
        retries = prefs.getUChar(OTA_NVS_KEY_RETRIES, 0) + 1;
        prefs.putUChar(OTA_NVS_KEY_RETRIES, retries);
        prefs.end();
    }
    return retries;
}

/**
 * @brief Save OTA version to NVS for execution after reboot
 * @param version Version to update to
 */
void savePendingOTA(const String& version) {
    Preferences prefs;
    if (prefs.begin(OTA_NVS_NAMESPACE, false)) {  // Read-write
        prefs.putString(OTA_NVS_KEY_VERSION, version);
        prefs.putUChar(OTA_NVS_KEY_RETRIES, 0);  // Reset retry counter
        prefs.end();
        LOG_I("Saved pending OTA version: %s", version.c_str());
    }
}

/**
 * @brief Clear pending OTA from NVS (called after successful OTA or on cancel)
 */
void clearPendingOTA() {
    Preferences prefs;
    if (prefs.begin(OTA_NVS_NAMESPACE, false)) {
        prefs.remove(OTA_NVS_KEY_VERSION);
        prefs.remove(OTA_NVS_KEY_RETRIES);
        prefs.end();
        LOG_I("Cleared pending OTA");
    }
}

/**
 * @brief Get firmware variant from NVS (for OTA binary selection)
 * @return Variant string ("screen" or "noscreen"), defaults to build-time variant if not found
 */
String getFirmwareVariant() {
    Preferences prefs;
    if (prefs.begin("firmware", true)) {  // Read-only
        String variant = prefs.getString("variant", "");
        prefs.end();
        if (variant.length() > 0) {
            return variant;
        }
    }
    // Fallback to build-time variant if NVS not available
    return FIRMWARE_VARIANT;
}

/**
 * @brief Get ESP32 firmware asset name based on variant
 * @return Asset name string
 */
String getESP32AssetName() {
    String variant = getFirmwareVariant();
    if (variant == "noscreen") {
        return GITHUB_ESP32_NOSCREEN_ASSET;
    }
    // Default to screen variant (backward compatibility)
    return GITHUB_ESP32_ASSET;
}

// =============================================================================
// Forward Declarations
// =============================================================================

/**
 * @brief Pauses all background services that might interfere with OTA
 */
static void pauseBackgroundServices() {
    LOG_I("Pausing background services for OTA...");
    
    // Disable MQTT client during OTA
    if (mqttClient) {
        if (mqttClient->isConnected()) {
            mqttClient->setEnabled(false);
        }
    }
    
    // Pause other services with proper method existence checking
    // (Implementation depends on your actual service interfaces)
    
    // Give some time for services to pause
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

/**
 * @brief Resumes all background services after OTA
 * Note: Usually not called since device restarts after OTA (success or failure)
 */
static void resumeBackgroundServices() {
    LOG_I("Resuming background services after OTA...");
    
    // Turn display back on
#if ENABLE_SCREEN
    extern Display display;
    display.backlightOn();
#endif
    
    // Re-enable MQTT client after OTA
    if (mqttClient) {
        mqttClient->setEnabled(true);
    }
    
    // Resume other services with proper method existence checking
    // (Implementation depends on your actual service interfaces)
    
    // Give some time for services to resume
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

/**
 * @brief Configures watchdog timer for OTA operations
 * @param enable Whether to enable or disable watchdog
 * @param timeoutSec Timeout in seconds (if enabling)
 */
static void configureWatchdog(bool enable, uint32_t timeoutSec = OTA_WDT_TIMEOUT_SECONDS);

static void disableWatchdogForOTA();
static void enableWatchdogAfterOTA();
static inline void feedWatchdog();
static void broadcastOtaProgress(AsyncWebSocket* ws, const char* stage, int progress, const char* message);
static void handleOTAFailure(AsyncWebSocket* ws);

// =============================================================================
// OTA Service Control - Pause/Resume background services during update
// =============================================================================

/**
 * Stop all background services before OTA to free memory for SSL
 * 
 * SSL/TLS needs ~50KB contiguous memory. We stop (not just pause) services
 * to free their FreeRTOS task stacks and internal buffers.
 * 
 * Services STOPPED (task deleted, memory freed):
 * - CloudConnection: SSL WebSocket task (6KB stack) + SSL buffers
 * - ScaleManager: NimBLE stack completely deinitialized
 * 
 * Services DISABLED (task still running, but idle):
 * - MQTTClient: Disconnected from broker
 * - PowerMeterManager: HTTP polling stopped
 * - NotificationManager: Push notifications stopped
 * - Display: Backlight and RGB signals turned off
 * 
 * Services NOT paused (needed for OTA):
 * - WiFiManager: Network connectivity
 * - PicoUART: Communication with Pico for Pico OTA
 * 
 * Note: WebSocket connections are closed to free memory and prevent
 * clients from reconnecting during OTA (they will reconnect after reboot).
 */
static void pauseServicesForOTA(CloudConnection* cloudConnection, AsyncWebSocket* ws) {
    LOG_I("Pausing services for OTA...");
    
    size_t heapBefore = ESP.getFreeHeap();
    size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    LOG_I("Heap before pausing: %zu bytes (largest block: %zu)", heapBefore, largestBlock);
    
    // 0. Disable watchdog - OTA has long-blocking operations
    disableWatchdogForOTA();
    
    // 0.5. Close all WebSocket connections - prevents reconnection attempts during OTA
    // Clients will reconnect after device reboots with new firmware
    if (ws) {
        LOG_I("  - Closing all WebSocket connections...");
        ws->closeAll(1001, "OTA in progress");  // 1001 = Going Away
        ws->cleanupClients();  // Force cleanup
    }
    
    // Ensure WiFi is in high performance mode (no sleep)
    // This significantly improves OTA download speed (prevents ~100ms latency per packet)
    WiFi.setSleep(false);
    
    // 1. STOP cloud connection completely (not just disable)
    // This deletes the FreeRTOS task and frees its 6KB stack
    if (cloudConnection) {
        LOG_I("  - Stopping cloud connection (freeing task)...");
        cloudConnection->end();  // This stops the task and frees memory
        // CRITICAL: After end(), the object may be in an invalid state
        // The caller must set _cloudConnection = nullptr to prevent crashes
    }
    
    // 2. Disconnect MQTT (task still runs but disconnected saves buffer memory)
    if (mqttClient) {
        LOG_I("  - Disabling MQTT...");
        mqttClient->setEnabled(false);
    }
    
    // 3. Stop BLE completely (frees NimBLE stack memory)
    if (scaleManager) {
        LOG_I("  - Stopping BLE scale...");
        scaleManager->end();
    }
    
    // 4. Pause power meter polling (HTTP requests)
    if (powerMeterManager) {
        LOG_I("  - Pausing power meter...");
        powerMeterManager->setEnabled(false);
    }
    
    // 5. Pause notifications (prevents cloud push attempts)
    if (notificationManager) {
        LOG_I("  - Pausing notifications...");
        notificationManager->setEnabled(false);
    }
    
    // 6. Turn off display to free memory and reduce interference
    // Display uses PSRAM for buffers but turning it off reduces DMA activity
#if ENABLE_SCREEN
    extern Display display;
    LOG_I("  - Turning off display...");
    display.backlightOff();
#endif
    
    // Give all services time to cleanly shut down and memory to be freed
    // Wait 3 seconds total for tasks to terminate and memory to be freed
    LOG_I("Waiting for memory to be freed...");
    for (int i = 0; i < 30; i++) {
        delay(100);
        yield();
    }
    
    size_t heapAfter = ESP.getFreeHeap();
    largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    LOG_I("All services stopped for OTA. Heap: %zu bytes (freed %d bytes, largest block: %zu)", 
          heapAfter, (int)(heapAfter - heapBefore), largestBlock);
}

/**
 * Handle OTA failure - restart device to ensure clean state
 * After a failed OTA attempt, the device may be in an inconsistent state.
 * Restarting ensures all services are properly re-initialized.
 * 
 * Note: _otaInProgress flag is reset on device restart, no explicit reset needed here.
 */
static void handleOTAFailure(AsyncWebSocket* ws) {
    LOG_E("OTA failed - restarting device to restore clean state");
    
    // Broadcast failure to UI
    if (ws) {
        broadcastOtaProgress(ws, "error", 0, "Update failed - restarting...");
    }
    
    // Give time for the error message to be sent
    for (int i = 0; i < 20; i++) {
        delay(100);
        yield();
    }
    
    // Restart the device - this is the safest way to recover
    ESP.restart();
}

// =============================================================================
// OTA Constants and Configuration
// =============================================================================

// Timeouts (in milliseconds)
constexpr unsigned long OTA_TOTAL_TIMEOUT_MS = 300000;     // 5 minutes total OTA timeout
constexpr unsigned long OTA_DOWNLOAD_TIMEOUT_MS = 300000;  // 5 minutes per download (accommodate slow networks)
constexpr unsigned long OTA_HTTP_TIMEOUT_MS = 30000;       // 30 seconds HTTP timeout
constexpr unsigned long OTA_WATCHDOG_FEED_INTERVAL_MS = 20;// Feed watchdog every 20ms to prevent slow loop warnings

// Buffer sizes
constexpr size_t OTA_BUFFER_SIZE = 512;                    // Smaller buffer for stack safety

// Retry configuration
constexpr int OTA_MAX_RETRIES = 3;
constexpr unsigned long OTA_RETRY_DELAY_MS = 3000;

// =============================================================================
// Watchdog Management
// =============================================================================

// Track if watchdog is disabled (to avoid reset errors)
static bool _watchdogDisabled = false;

/**
 * @brief Feeds the watchdog timer to prevent timeouts
 */
static inline void feedWatchdog() {
    yield();
    // Reset task watchdog only if we haven't disabled it
    if (!_watchdogDisabled) {
        esp_task_wdt_reset();
    }
}

/**
 * @brief Configures the watchdog timer for OTA operations
 * ESP-IDF 5.x: esp_task_wdt_init now takes a config struct
 */
static void configureWatchdog(bool enable, uint32_t timeoutSec) {
    if (enable) {
        esp_task_wdt_config_t wdt_config = {
            .timeout_ms = timeoutSec * 1000,
            .idle_core_mask = 0,  // No idle task monitoring
            .trigger_panic = true
        };
        esp_task_wdt_reconfigure(&wdt_config);
        esp_task_wdt_add(NULL); // Add current task to WDT watch
    } else {
        esp_task_wdt_delete(NULL);
    }
}

// =============================================================================
// Progress Reporting
// =============================================================================

/**
 * @brief Broadcasts OTA progress updates to WebSocket clients
 */
// broadcastOtaProgress is already defined above

// =============================================================================
// Main OTA Entry Points
// =============================================================================

/**
 * Clean up any leftover OTA files
 * Called at start and end of OTA process
 */
static void cleanupOtaFiles() {
    if (LittleFS.exists(OTA_FILE_PATH)) {
        LittleFS.remove(OTA_FILE_PATH);
        Serial.println("[OTA] Cleaned up temporary firmware file");
    }
}

/**
 * Disable the Task Watchdog Timer for OTA operations
 * OTA can involve long-blocking operations (SSL, flash erase) that would trigger WDT
 * 
 * Strategy: 
 * 1. Remove current task (loopTask) from WDT monitoring
 * 2. Try to remove async_tcp from WDT (may fail - owned by AsyncTCP library)
 * 3. Try to deinit WDT completely
 * 4. If deinit fails, reinit with longer timeout (60 seconds)
 * 
 * Note: ESP-IDF 4.4 doesn't have esp_task_wdt_reconfigure(), so we use deinit+init
 */
static void disableWatchdogForOTA() {
    LOG_I("Disabling watchdog for OTA...");
    
    // First, try to remove current task from WDT
    esp_err_t err = esp_task_wdt_delete(NULL);
    if (err == ESP_OK) {
        LOG_I("Removed loopTask from WDT");
    } else if (err == ESP_ERR_NOT_FOUND) {
        LOG_D("loopTask not subscribed to WDT");
    } else {
        LOG_D("loopTask WDT delete returned: %d", err);
    }
    
    // Try to remove async_tcp task from WDT
    // This task is created by AsyncTCP library and runs on CPU 1
    TaskHandle_t asyncTcpTask = xTaskGetHandle("async_tcp");
    if (asyncTcpTask != NULL) {
        err = esp_task_wdt_delete(asyncTcpTask);
        if (err == ESP_OK) {
            LOG_I("Removed async_tcp from WDT");
        } else if (err == ESP_ERR_NOT_FOUND) {
            LOG_D("async_tcp not subscribed to WDT");
        } else {
            LOG_W("Could not remove async_tcp from WDT: %d", err);
        }
    } else {
        LOG_D("async_tcp task not found");
    }
    
    // Try to deinit the WDT completely
    err = esp_task_wdt_deinit();
    if (err == ESP_OK) {
        LOG_I("WDT deinitialized successfully");
        _watchdogDisabled = true;
        return;
    }
    
    // Deinit failed - tasks still subscribed. Try to reinit with longer timeout.
    // Note: This requires deinit to succeed first, so we'll try a workaround
    LOG_W("WDT deinit failed (err=%d) - tasks still subscribed", err);
    
    // Last resort: try to reconfigure with longer timeout
    // ESP-IDF 5.x: use esp_task_wdt_reconfigure instead of init
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = OTA_WDT_TIMEOUT_SECONDS * 1000,
        .idle_core_mask = 0,
        .trigger_panic = false  // Long timeout, no panic
    };
    err = esp_task_wdt_reconfigure(&wdt_config);
    if (err == ESP_OK) {
        LOG_I("WDT reconfigured with 60 second timeout");
    } else {
        // Reconfiguration failed - the WDT is still active with its original timeout.
        // We've removed loopTask, so it won't trigger for us. The async_tcp might
        // still trigger, but we've done what we can.
        LOG_W("WDT reconfigure returned: %d - async_tcp may still trigger WDT", err);
    }
    
    _watchdogDisabled = true;
}

/**
 * Re-enable the Task Watchdog Timer after OTA
 * Note: After successful OTA, the device restarts so this is mainly for failed OTA recovery
 */
static void enableWatchdogAfterOTA() {
    _watchdogDisabled = false;
    
    // Try to re-add current task to watchdog
    // Note: Full WDT recovery happens on device restart
    esp_err_t err = esp_task_wdt_add(NULL);
    if (err == ESP_OK) {
        LOG_I("Task watchdog re-enabled for current task");
    } else if (err == ESP_ERR_INVALID_STATE) {
        // WDT not initialized or task not added - try to reconfigure
        // ESP-IDF 5.x: use config struct
        esp_task_wdt_config_t wdt_config = {
            .timeout_ms = DEFAULT_WDT_TIMEOUT_SECONDS * 1000,
            .idle_core_mask = 0,
            .trigger_panic = true  // Default timeout, panic on trigger
        };
        err = esp_task_wdt_reconfigure(&wdt_config);
        if (err == ESP_OK) {
            LOG_I("WDT reconfigured with default config");
            esp_task_wdt_add(NULL);
        }
    } else {
        // Don't worry about errors - device will restart anyway
        LOG_D("WDT add returned: %d (device will restart)", err);
    }
}

/**
 * Broadcast OTA stage update - simplified to just send stage transitions
 * 
 * The UI shows a simple animation during OTA, not a progress bar.
 * We only need to notify: stage change, error, or completion.
 * Progress percentage is unused and removed to avoid WebSocket queue overflow.
 */
static void broadcastOtaProgress(AsyncWebSocket* ws, const char* stage, int /* progress */, const char* message) {
    if (!ws) return;
    
    feedWatchdog();
    ws->cleanupClients();
    
    if (ws->count() == 0) {
        LOG_D("OTA: No clients to notify");
        return;
    }
    
    // Skip non-critical updates if queue is full
    bool isCritical = (strcmp(stage, "error") == 0 || strcmp(stage, "complete") == 0);
    if (!ws->availableForWriteAll()) {
        if (!isCritical) {
            LOG_D("OTA: Skipping non-critical update (queue full)");
            return;
        }
        // For critical messages, wait briefly
        for (int i = 0; i < 3 && !ws->availableForWriteAll(); i++) {
            delay(50);
            yield();
            feedWatchdog();
        }
    }
    
    LOG_I("OTA: stage=%s, message=%s", stage, message ? message : "");
    
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    StaticJsonDocument<192> doc;
    #pragma GCC diagnostic pop
    doc["type"] = "ota_progress";
    doc["stage"] = stage;
    doc["message"] = message;
    // Note: progress field removed - UI uses simple animation, not progress bar
    
    char jsonBuffer[192];
    serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
    ws->textAll(jsonBuffer);
    
    // Brief yield to allow message to be sent
    delay(50);
    yield();
    feedWatchdog();
    
    feedWatchdog();
}

/**
 * Download a file from URL to LittleFS with proper error handling
 * Returns true on success, false on failure
 */
static bool downloadToFile(const char* url, const char* filePath, 
                           size_t* outFileSize = nullptr) {
    LOG_I("Downloading: %s", url);
    
    // Configure secure client (services are paused to free memory for SSL buffers)
    WiFiClientSecure client;
    client.setInsecure();
    // CRITICAL FIX: Set underlying TCP timeout to prevent indefinite hangs
    client.setTimeout(15); // 15 seconds read timeout
    
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(OTA_HTTP_TIMEOUT_MS);
    
    int httpCode = 0;
    unsigned long downloadStart = millis();
    
    // Retry loop for transient errors
    for (int retry = 0; retry < OTA_MAX_RETRIES; retry++) {
        feedWatchdog();
        
        if (!http.begin(client, url)) {
            LOG_E("HTTP begin failed (attempt %d/%d)", retry + 1, OTA_MAX_RETRIES);
            if (retry < OTA_MAX_RETRIES - 1) {
                for (int i = 0; i < 30; i++) { delay(100); feedWatchdog(); }
                continue;
            }
            return false;
        }
        
        http.addHeader("User-Agent", "BrewOS-ESP32/" ESP32_VERSION);
        
        feedWatchdog();
        httpCode = http.GET();
        feedWatchdog();
        
        if (httpCode == HTTP_CODE_OK) {
            break;
        }
        
        LOG_W("HTTP error %d (attempt %d/%d)", httpCode, retry + 1, OTA_MAX_RETRIES);
        http.end();
        
        // Retry on transient errors
        if ((httpCode == 503 || httpCode == 429 || httpCode == 500) && retry < OTA_MAX_RETRIES - 1) {
            LOG_I("Retrying in %lu ms...", OTA_RETRY_DELAY_MS);
            for (unsigned long i = 0; i < OTA_RETRY_DELAY_MS / 100; i++) {
                delay(100);
                feedWatchdog();
            }
            continue;
        }
        
        return false;
    }
    
    if (httpCode != HTTP_CODE_OK) {
        LOG_E("HTTP failed after retries: %d", httpCode);
        return false;
    }
    
    int contentLength = http.getSize();
    if (contentLength <= 0 || contentLength > OTA_MAX_SIZE) {
        LOG_E("Invalid content length: %d", contentLength);
        http.end();
        return false;
    }
    
    LOG_I("Content length: %d bytes", contentLength);
    if (outFileSize) *outFileSize = contentLength;
    
    // Check available space
    size_t freeSpace = LittleFS.totalBytes() - LittleFS.usedBytes();
    if ((size_t)contentLength > freeSpace) {
        LOG_E("Not enough space: need %d, have %d", contentLength, freeSpace);
        http.end();
        return false;
    }
    
    // Delete existing file
    if (LittleFS.exists(filePath)) {
        LittleFS.remove(filePath);
        feedWatchdog();
    }
    
    // Create file
    File file = LittleFS.open(filePath, "w");
    feedWatchdog();
    if (!file) {
        LOG_E("Failed to create file: %s", filePath);
        http.end();
        return false;
    }
    
    // Download to file
    WiFiClient* stream = http.getStreamPtr();
    if (!stream) {
        LOG_E("Failed to get HTTP stream pointer");
        file.close();
        LittleFS.remove(filePath);
        http.end();
        return false;
    }
    
    // FIX: Allocate buffer on HEAP, not stack.
    // WiFiClientSecure uses a lot of stack; a stack buffer increases crash risk.
    std::unique_ptr<uint8_t[]> buffer(new (std::nothrow) uint8_t[OTA_BUFFER_SIZE]);
    if (!buffer) {
        LOG_E("Failed to allocate buffer");
        file.close();
        http.end();
        return false;
    }
    
    size_t written = 0;
    unsigned long lastYield = millis();
    unsigned long lastDataReceived = millis();
    unsigned long lastConsoleLog = millis();
    unsigned long lastConnectionCheck = millis();
    // FIX: Increased stall timeout for slow GitHub connections
    constexpr unsigned long STALL_TIMEOUT_MS = 30000;  // 30 seconds
    constexpr unsigned long CONNECTION_CHECK_INTERVAL_MS = 2000;  // Check connection health every 2s
    unsigned long noDataCount = 0;
    
    while (written < (size_t)contentLength) {
        // Check for overall timeout
        if (millis() - downloadStart > OTA_DOWNLOAD_TIMEOUT_MS) {
            LOG_E("Download timeout after %lu ms (wrote %d/%d)", millis() - downloadStart, written, contentLength);
            file.close();
            LittleFS.remove(filePath);
            http.end();
            return false;
        }
        
        // Check connection health periodically
        if (millis() - lastConnectionCheck > CONNECTION_CHECK_INTERVAL_MS) {
            bool httpConnected = http.connected();
            bool streamConnected = stream->connected();
            
            if (!httpConnected || !streamConnected) {
                LOG_E("Connection lost: http=%d stream=%d (wrote %d/%d)", httpConnected, streamConnected, written, contentLength);
                file.close();
                LittleFS.remove(filePath);
                http.end();
                return false;
            }
            lastConnectionCheck = millis();
        }
        
        // Check for stall (no data received for too long)
        if (millis() - lastDataReceived > STALL_TIMEOUT_MS) {
            LOG_E("Download stalled - no data for %lu ms (wrote %d/%d)", STALL_TIMEOUT_MS, written, contentLength);
            file.close();
            LittleFS.remove(filePath);
            http.end();
            return false;
        }
        
        // Feed watchdog frequently and yield to prevent blocking main loop
        if (millis() - lastYield >= OTA_WATCHDOG_FEED_INTERVAL_MS) {
            feedWatchdog();
            yield();  // Yield to other tasks
            lastYield = millis();
        }
        
        size_t available = stream->available();
        if (available > 0) {
            lastDataReceived = millis();
            noDataCount = 0;  // Reset stall counter
            
            // Limit read size to prevent blocking main loop for too long
            // Read in smaller chunks with yields to allow main loop to run
            size_t maxChunkSize = min(available, (size_t)4096);  // Max 4KB per chunk
            size_t toRead = min(maxChunkSize, (size_t)OTA_BUFFER_SIZE);  // Use allocated size
            
            // Yield before potentially blocking read operation
            feedWatchdog();
            yield();
            
            // Read with the heap buffer
            size_t bytesRead = stream->readBytes(buffer.get(), toRead);
            
            if (bytesRead > 0) {
                // Yield before potentially blocking write operation
                feedWatchdog();
                yield();
                
                size_t bytesWritten = file.write(buffer.get(), bytesRead);
                if (bytesWritten != bytesRead) {
                    LOG_E("Write error: %d/%d bytes (filesystem full?)", bytesWritten, bytesRead);
                    file.close();
                    LittleFS.remove(filePath);
                    http.end();
                    return false;
                }
                written += bytesWritten;
                
                // Yield after write to allow main loop to run
                // This prevents blocking the main loop for >1 second
                feedWatchdog();
                yield();
                vTaskDelay(pdMS_TO_TICKS(1));  // 1ms delay using FreeRTOS for better task scheduling
                
                // Log to console every 5 seconds (progress not sent to WebSocket - UI uses simple animation)
                if (millis() - lastConsoleLog > OTA_CONSOLE_LOG_INTERVAL_MS) {
                    int pct = (written * 100) / contentLength;
                    LOG_I("Download: %d%% (%d/%d bytes)", pct, written, contentLength);
                    lastConsoleLog = millis();
                }
            }
        } else {
            // No data available - increment counter and check for potential issues
            noDataCount++;
            
            // If we've had no data for a while, check connection more aggressively
            if (noDataCount > 100) {  // ~1 second at 10ms delay
                if (!http.connected() || !stream->connected()) {
                    LOG_E("Connection dropped during download (wrote %d/%d)", written, contentLength);
                    file.close();
                    LittleFS.remove(filePath);
                    http.end();
                    return false;
                }
            }
            
            // FIX: If connected but no data, wait briefly to avoid spinning too tight
            yield();
            delay(5);  // 5ms delay to allow other tasks to run
            feedWatchdog();
        }
    }
    
    file.close();
    http.end();
    feedWatchdog();
    
    // Verify download complete
    if (written != (size_t)contentLength) {
        LOG_E("Download incomplete: %d/%d bytes", written, contentLength);
        LittleFS.remove(filePath);
        return false;
    }
    
    unsigned long downloadTime = millis() - downloadStart;
    LOG_I("Download complete: %d bytes in %lu ms (%.1f KB/s)", 
          contentLength, downloadTime, (contentLength / 1024.0f) / (downloadTime / 1000.0f));
    
    // Verify file size matches
    File verifyFile = LittleFS.open(filePath, "r");
    if (!verifyFile || verifyFile.size() != (size_t)contentLength) {
        LOG_E("File verification failed");
        if (verifyFile) verifyFile.close();
        LittleFS.remove(filePath);
        return false;
    }
    verifyFile.close();
    
    LOG_I("Download complete: %d bytes", written);
    return true;
}

// =============================================================================
// Pico OTA - Download and flash Pico firmware
// =============================================================================

bool BrewWebServer::startPicoGitHubOTA(const String& version) {
    LOG_I("Starting Pico GitHub OTA for version: %s", version.c_str());
    
    // Get machine type from StateManager
    uint8_t machineType = State.getMachineType();
    const char* picoAsset = getPicoAssetName(machineType);
    
    if (!picoAsset) {
        LOG_E("Unknown machine type: %d", machineType);
        broadcastLogLevel("error", "Update error: Device not ready");
        broadcastOtaProgress(&_ws, "error", 0, "Device not ready");
        return false;
    }
    
    LOG_I("Pico asset: %s", picoAsset);
    
    // Build URL
    char tag[32];
    if (strcmp(version.c_str(), "dev-latest") != 0 && strncmp(version.c_str(), "v", 1) != 0) {
        snprintf(tag, sizeof(tag), "v%s", version.c_str());
    } else {
        strncpy(tag, version.c_str(), sizeof(tag) - 1);
        tag[sizeof(tag) - 1] = '\0';
    }
    
    char downloadUrl[256];
    snprintf(downloadUrl, sizeof(downloadUrl), 
             "https://github.com/" GITHUB_OWNER "/" GITHUB_REPO "/releases/download/%s/%s", 
             tag, picoAsset);
    
    LOG_I("Pico download URL: %s", downloadUrl);
    
    // Clean up any leftover files
    cleanupOtaFiles();
    
    // Download firmware
    broadcastOtaProgress(&_ws, "download", 0, "Downloading Pico firmware...");
    
    size_t firmwareSize = 0;
    if (!downloadToFile(downloadUrl, OTA_FILE_PATH, &firmwareSize)) {
        LOG_E("Pico firmware download failed");
        broadcastLogLevel("error", "Update error: Download failed");
        broadcastOtaProgress(&_ws, "error", 0, "Download failed");
        cleanupOtaFiles();
        return false;
    }
    
    // Flash to Pico
    // Method selection: SWD if hardware supports it (SWD_SUPPORTED), otherwise UART bootloader
    // Screen variant: SWD pins not wired → uses UART bootloader
    // No-screen variant: SWD pins wired → can use SWD or UART bootloader
#if SWD_SUPPORTED
    // SWD method: Uses GPIO 21 (SWDIO) and GPIO 45 (SWCLK) for direct flash programming
    broadcastOtaProgress(&_ws, "flash", 40, "Installing Pico firmware (SWD)...");
#else
    // UART bootloader method: Uses existing UART connection for firmware transfer
    broadcastOtaProgress(&_ws, "flash", 40, "Installing Pico firmware (UART)...");
#endif
    
    File flashFile = LittleFS.open(OTA_FILE_PATH, "r");
    if (!flashFile) {
        LOG_E("Failed to open firmware file");
        broadcastLogLevel("error", "Update error: Cannot read firmware");
        broadcastOtaProgress(&_ws, "error", 0, "Cannot read firmware");
        cleanupOtaFiles();
        return false;
    }
    
#if SWD_SUPPORTED
    // =============================================================================
    // SWD METHOD
    // Available only on no-screen variant (SWD pins are physically wired)
    // =============================================================================
    
    broadcastOtaProgress(&_ws, "flash", 42, "Connecting via SWD...");
    feedWatchdog();
    
    // Pause UART to prevent interference with SWD
    _picoUart.pause();
    LOG_I("Paused UART packet processing for SWD");
    
    // CRITICAL: End Serial1 (UART1 on GPIO41/42) to prevent any interference
    // Then reconfigure SWD pins (GPIO21/GPIO45) as GPIO outputs
    // These pins are safe GPIOs (not UART2, not PSRAM) but we still reset them to ensure clean state
    Serial1.end();
    delay(10); // Small delay to ensure UART1 is fully stopped
    
    // Reset SWD pins to default state and configure as GPIO outputs
    // gpio_reset_pin() resets the pin to default state and detaches it from any peripheral
    // GPIO 21/45 are safe pins (no conflicts), but we reset them to ensure clean configuration
    LOG_I("SWD: Resetting SWD pins (GPIO%d/GPIO%d) to default state...", SWD_DIO_PIN, SWD_CLK_PIN);
    gpio_reset_pin((gpio_num_t)SWD_DIO_PIN);
    gpio_reset_pin((gpio_num_t)SWD_CLK_PIN);
    delay(10); // Allow pins to reset
    
    // Use ESP-IDF GPIO functions directly for maximum control
    // This ensures pins are fully configured as GPIO, not attached to any peripheral
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << SWD_DIO_PIN) | (1ULL << SWD_CLK_PIN);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
    
    // Set pins HIGH (idle state for SWD) using ESP-IDF
    gpio_set_level((gpio_num_t)SWD_DIO_PIN, 1);
    gpio_set_level((gpio_num_t)SWD_CLK_PIN, 1);
    delay(5); // Allow pins to stabilize
    
    // CRITICAL: Sync Arduino HAL with ESP-IDF configuration
    // This ensures digitalWrite()/digitalRead() work correctly
    pinMode(SWD_DIO_PIN, OUTPUT);
    pinMode(SWD_CLK_PIN, OUTPUT);
    digitalWrite(SWD_DIO_PIN, HIGH);
    digitalWrite(SWD_CLK_PIN, HIGH);
    
    LOG_I("SWD: Pins configured using ESP-IDF gpio_config() and synced with Arduino HAL");
    
    // Verify pins can actually be driven LOW (hardware test)
    // Test 1: Drive LOW and verify it reads LOW while in OUTPUT mode
    digitalWrite(SWD_DIO_PIN, LOW);
    delayMicroseconds(50); // Longer delay for signal to settle
    // Read while still in OUTPUT mode (ESP32 can read its own output)
    bool swdio_low_output = digitalRead(SWD_DIO_PIN);
    
    // Test 2: Switch to INPUT and check if external pull-up keeps it HIGH
    pinMode(SWD_DIO_PIN, INPUT);
    delayMicroseconds(50);
    bool swdio_low_input = digitalRead(SWD_DIO_PIN);
    
    // Restore to OUTPUT HIGH
    pinMode(SWD_DIO_PIN, OUTPUT);
    digitalWrite(SWD_DIO_PIN, HIGH);
    
    LOG_I("SWD: Pin drive test - OUTPUT mode reads: %d, INPUT mode reads: %d", 
          swdio_low_output, swdio_low_input);
    
    if (swdio_low_output == 1) {
        LOG_E("SWD: CRITICAL - SWDIO pin cannot be driven LOW (reads HIGH in OUTPUT mode)");
        LOG_E("SWD: This indicates pin is stuck HIGH or being driven by another source");
        LOG_E("SWD: Possible causes: hardware fault, strong pull-up, or pin conflict");
    } else if (swdio_low_input == 1) {
        LOG_W("SWD: Pin can be driven LOW, but external pull-up keeps it HIGH when floating");
        LOG_W("SWD: This is normal - pull-up ensures idle state for SWD communication");
    } else {
        LOG_I("SWD: Pin reset successful - SWDIO can be driven LOW");
    }
    
    LOG_I("Reconfigured SWD pins as GPIO (SWDIO=GPIO%d, SWCLK=GPIO%d)", SWD_DIO_PIN, SWD_CLK_PIN);
    
    // Initialize SWD interface
    PicoSWD swd(SWD_DIO_PIN, SWD_CLK_PIN, SWD_RESET_PIN);
    
    if (!swd.begin()) {
        LOG_E("SWD connection failed");
        broadcastLogLevel("error", "Update error: SWD connection failed");
        broadcastOtaProgress(&_ws, "error", 0, "SWD connection failed");
        
        // CRITICAL: Always reset Pico even on failure to ensure it's not stuck
        LOG_W("SWD: Resetting Pico after failed connection attempt...");
        swd.end();  // Clean up SWD connection
        swd.resetTarget();  // Reset Pico to ensure it boots normally
        
        // Reinitialize Serial1 UART on failure
        Serial1.begin(PICO_UART_BAUD, SERIAL_8N1, PICO_UART_RX_PIN, PICO_UART_TX_PIN);
        delay(10);
        _picoUart.resume();  // Resume on failure
        flashFile.close();
        cleanupOtaFiles();
        return false;
    }
    
    broadcastOtaProgress(&_ws, "flash", 45, "Flashing firmware...");
    feedWatchdog();
    
    // Flash firmware via SWD
    bool success = swd.flashFirmware(flashFile, firmwareSize);
    
    // Clean up SWD connection
    swd.end();
    
    flashFile.close();
    
    // Clean up temp file regardless of success
    cleanupOtaFiles();
    
    if (!success) {
        LOG_E("SWD firmware flashing failed");
        broadcastLogLevel("error", "Update error: Installation failed");
        broadcastOtaProgress(&_ws, "error", 0, "Installation failed");
        
        // CRITICAL: Always reset Pico even on failure to ensure it's not stuck
        LOG_W("SWD: Resetting Pico after failed flash attempt...");
        swd.end();  // Clean up SWD connection
        swd.resetTarget();  // Reset Pico to ensure it boots normally
        
        // Reinitialize Serial1 UART on failure
        Serial1.begin(PICO_UART_BAUD, SERIAL_8N1, PICO_UART_RX_PIN, PICO_UART_TX_PIN);
        delay(10);
        _picoUart.resume();  // Resume on failure
        return false;
    }
    
    // Reset Pico via SWD or hardware pin after successful flash
    LOG_I("Resetting Pico after successful SWD flash...");
    swd.end();  // Clean up SWD connection first
    swd.resetTarget();  // Then reset
    
    broadcastOtaProgress(&_ws, "flash", 55, "Waiting for device restart...");
    
    // Reinitialize Serial1 UART after SWD is done
    // SWD pins are now back to GPIO, but we need to reinitialize UART1 for Pico communication
    Serial1.begin(PICO_UART_BAUD, SERIAL_8N1, PICO_UART_RX_PIN, PICO_UART_TX_PIN);
    delay(10); // Allow UART to initialize
    
    // Resume packet processing to detect when Pico comes back
    _picoUart.resume();
    LOG_I("Resumed UART packet processing");
    
    // Clear connection state so we can detect when Pico actually reconnects
    _picoUart.clearConnectionState();
    
#else
    // =============================================================================
    // UART BOOTLOADER METHOD (Default - SWD disabled until fixed)
    // =============================================================================
    
    // Retry configuration (defined at function scope for use in multiple retry loops)
    const int MAX_HANDSHAKE_RETRIES = 3;  // Retry bootloader handshake up to 3 times
    
    // CRITICAL: Enable Pico log forwarding BEFORE starting OTA for better diagnostics
    // This allows us to see Pico logs over Serial during the entire OTA process
    // Note: Logs are printed to Serial even without the log buffer enabled (see LogManager::handlePicoLog)
    bool picoLogForwardingWasEnabled = LogManager::instance().isPicoLogForwardingEnabled();
    
    // Enable Pico log forwarding (if not already enabled)
    // This sends MSG_CMD_LOG_CONFIG to Pico to start forwarding logs
    // Logs will be printed to Serial for diagnostics (no buffer required)
    // CRITICAL: Enable BEFORE pausing UART so Pico can process the command
    if (!picoLogForwardingWasEnabled) {
        LOG_I("Enabling Pico log forwarding for OTA diagnostics (Serial output)");
        LogManager::instance().setPicoLogForwarding(true, [this](uint8_t* payload, size_t len) {
            return _picoUart.sendCommand(MSG_CMD_LOG_CONFIG, payload, len);
        });
        
        // Give Pico time to process the log forwarding command and start forwarding logs
        // This ensures we see "Entering bootloader mode" and other bootloader logs
        delay(200);
        
        // Process any pending packets to ensure log forwarding ACK is received
        _picoUart.loop();
        delay(50);
    }
    
    // Helper lambda to restore log forwarding state on cleanup
    auto restoreLogForwarding = [picoLogForwardingWasEnabled, this]() {
        // Restore Pico log forwarding state
        if (!picoLogForwardingWasEnabled && LogManager::instance().isPicoLogForwardingEnabled()) {
            LOG_I("Restoring Pico log forwarding state (disabling)");
            LogManager::instance().setPicoLogForwarding(false, [this](uint8_t* payload, size_t len) {
                return _picoUart.sendCommand(MSG_CMD_LOG_CONFIG, payload, len);
            });
        }
    };
    
    // Send bootloader command with retry mechanism
    broadcastOtaProgress(&_ws, "flash", 42, "Preparing device...");
    feedWatchdog();
    
    // NOTE: We keep UART processing ACTIVE during bootloader handshake so we can receive log messages
    // The waitForBootloaderAck() function reads directly from Serial1, so it won't be affected
    // We only pause UART processing AFTER receiving the bootloader ACK, before firmware streaming
    bool handshakeSuccess = false;
    
    for (int handshakeRetry = 0; handshakeRetry < MAX_HANDSHAKE_RETRIES && !handshakeSuccess; handshakeRetry++) {
        if (handshakeRetry > 0) {
            LOG_W("Retrying bootloader handshake (attempt %d/%d)...", handshakeRetry + 1, MAX_HANDSHAKE_RETRIES);
            broadcastOtaProgress(&_ws, "flash", 42, "Retrying device connection...");
            feedWatchdog();
            
            // Drain UART buffer before retry
            while (Serial1.available()) {
                Serial1.read();
            }
            delay(500);  // Wait before retry
        }
        
        if (!_picoUart.sendCommand(MSG_CMD_BOOTLOADER, nullptr, 0)) {
            LOG_W("Failed to send bootloader command (attempt %d/%d)", handshakeRetry + 1, MAX_HANDSHAKE_RETRIES);
            if (handshakeRetry < MAX_HANDSHAKE_RETRIES - 1) {
                continue;  // Retry
            } else {
                LOG_E("Failed to send bootloader command after %d attempts", MAX_HANDSHAKE_RETRIES);
                broadcastLogLevel("error", "Update error: Device not responding");
                broadcastOtaProgress(&_ws, "error", 0, "Device not responding");
                _picoUart.resume();  // Resume on failure
                flashFile.close();
                cleanupOtaFiles();
                restoreLogForwarding();  // Restore log forwarding state
                return false;
            }
        }
        
        // Give Pico time to process command and enter bootloader mode
        // Pico sends protocol ACK, waits 50ms, then sends 0xAA 0x55
        LOG_I("Sent bootloader command, waiting for Pico to enter bootloader...");
        feedWatchdog();
        
        // Process UART packets while waiting for bootloader ACK
        // This allows us to receive log messages from Pico (e.g., "Entering bootloader mode")
        // waitForBootloaderAck() reads directly from Serial1 and looks for the specific pattern
        // The protocol handler might consume some bytes, but bootloader ACK (0xB0 0x07 0xAC 0x4B)
        // doesn't start with 0xAA, so it should pass through
        unsigned long ackStartTime = millis();
        while (millis() - ackStartTime < 5000) {
            _picoUart.loop();  // Process incoming packets (including log messages)
            feedWatchdog();
            if (_picoUart.waitForBootloaderAck(100)) {  // Check with short timeout
                handshakeSuccess = true;
                LOG_I("Bootloader handshake successful");
                break;
            }
        }
        
        if (!handshakeSuccess) {
            LOG_W("Bootloader ACK timeout (attempt %d/%d)", handshakeRetry + 1, MAX_HANDSHAKE_RETRIES);
            if (handshakeRetry < MAX_HANDSHAKE_RETRIES - 1) {
                // Will retry
            } else {
                LOG_E("Bootloader ACK timeout after %d attempts", MAX_HANDSHAKE_RETRIES);
                broadcastLogLevel("error", "Update error: Device not ready");
                broadcastOtaProgress(&_ws, "error", 0, "Device not ready");
                _picoUart.resume();  // Resume on failure
                flashFile.close();
                cleanupOtaFiles();
                restoreLogForwarding();  // Restore log forwarding state
                return false;
            }
        }
    }
    
    // Now pause UART processing before firmware streaming
    // Bootloader ACK received, so we can safely pause protocol processing
    // This prevents the protocol handler from consuming bootloader data during firmware streaming
    _picoUart.pause();
    LOG_I("Paused UART packet processing for firmware streaming");
    
    // CRITICAL: Wait for Pico to fully enter bootloader mode
    // The ACK detection might have false-positived on protocol data.
    // Pico needs ~150ms from command to be ready (50ms sleep + 100ms in bootloader_prepare).
    // We already waited 200ms before looking for ACK, but add extra safety margin.
    LOG_I("ACK received, waiting for Pico to be ready...");
    delay(150);  // Give Pico time to fully enter bootloader mode
    
    // Drain any remaining bytes from UART (old protocol data, false ACK remnants)
    int drained = 0;
    while (Serial1.available()) {
        Serial1.read();
        drained++;
    }
    if (drained > 0) {
        LOG_I("Drained %d bytes from UART before streaming", drained);
    }
    
    broadcastOtaProgress(&_ws, "flash", 45, "Installing...");
    feedWatchdog();
    
    // Stream to Pico via UART bootloader with retry mechanism
    const int MAX_UPDATE_RETRIES = 2;  // Retry entire update once if it fails
    bool success = false;
    
    for (int updateRetry = 0; updateRetry < MAX_UPDATE_RETRIES && !success; updateRetry++) {
        if (updateRetry > 0) {
            LOG_W("Retrying Pico firmware update (attempt %d/%d)...", updateRetry + 1, MAX_UPDATE_RETRIES);
            broadcastOtaProgress(&_ws, "flash", 45, "Retrying installation...");
            feedWatchdog();
            
            // Reset file position for retry
            flashFile.seek(0);
            
            // Drain UART and wait a bit before retry
            while (Serial1.available()) {
                Serial1.read();
            }
            delay(500);
            
            // Re-do bootloader handshake for retry (with its own retry mechanism)
            bool retryHandshakeSuccess = false;
            for (int retryHandshakeAttempt = 0; retryHandshakeAttempt < MAX_HANDSHAKE_RETRIES && !retryHandshakeSuccess; retryHandshakeAttempt++) {
                if (retryHandshakeAttempt > 0) {
                    LOG_W("Retrying bootloader handshake on update retry (attempt %d/%d)...", 
                          retryHandshakeAttempt + 1, MAX_HANDSHAKE_RETRIES);
                    delay(500);
                    while (Serial1.available()) {
                        Serial1.read();
                    }
                }
                
                if (!_picoUart.sendCommand(MSG_CMD_BOOTLOADER, nullptr, 0)) {
                    LOG_W("Failed to send bootloader command on update retry (attempt %d/%d)", 
                          retryHandshakeAttempt + 1, MAX_HANDSHAKE_RETRIES);
                    if (retryHandshakeAttempt < MAX_HANDSHAKE_RETRIES - 1) {
                        continue;  // Retry handshake
                    } else {
                        break;  // Give up on this update retry
                    }
                }
                
                if (_picoUart.waitForBootloaderAck(5000)) {
                    retryHandshakeSuccess = true;
                } else {
                    LOG_W("Bootloader ACK timeout on update retry (attempt %d/%d)", 
                          retryHandshakeAttempt + 1, MAX_HANDSHAKE_RETRIES);
                    if (retryHandshakeAttempt < MAX_HANDSHAKE_RETRIES - 1) {
                        continue;  // Retry handshake
                    } else {
                        break;  // Give up on this update retry
                    }
                }
            }
            
            if (!retryHandshakeSuccess) {
                LOG_E("Bootloader handshake failed on update retry, will try next update retry...");
                continue;  // Try next update retry
            }
            
            delay(150);
            while (Serial1.available()) {
                Serial1.read();
            }
        }
        
        success = streamFirmwareToPico(flashFile, firmwareSize);
        
        if (!success && updateRetry < MAX_UPDATE_RETRIES - 1) {
            LOG_W("Update attempt %d failed, will retry...", updateRetry + 1);
        }
    }
    
    flashFile.close();
    
    // Clean up temp file regardless of success
    cleanupOtaFiles();
    
    if (!success) {
        LOG_E("Pico firmware streaming failed after %d attempts", MAX_UPDATE_RETRIES);
        broadcastLogLevel("error", "Update error: Installation failed after retries");
        broadcastOtaProgress(&_ws, "error", 0, "Installation failed after retries");
        _picoUart.resume();  // Resume on failure
        restoreLogForwarding();  // Restore log forwarding state
        return false;
    }
    
    // After streaming, the Pico bootloader will:
    // 1. Send success ACK (0xAA 0x55 0x00)
    // 2. Copy firmware from staging to main (~1-3 seconds)
    // 3. Self-reset via AIRCR register (not watchdog_reboot - that's in erased flash!)
    // 4. Boot with new firmware and send heartbeats
    
    broadcastOtaProgress(&_ws, "flash", 55, "Waiting for device restart...");
    
    // Resume packet processing to detect when Pico comes back
    _picoUart.resume();
    LOG_I("Resumed UART packet processing");
    
    // Drain any leftover bootloader bytes from UART buffer
    while (Serial1.available()) {
        Serial1.read();
    }
    
    // Clear connection state so we can detect when Pico actually reconnects
    _picoUart.clearConnectionState();
    
#endif // SWD_SUPPORTED
    
    // Wait for Pico to self-reset and reconnect
    // The bootloader copies firmware (~3-5s for 22 sectors * ~100ms each) then resets.
    // Total time: copy (~5s) + reboot (~1s) + reconnect (~1s) = ~7s minimum
    // Use generous 25s timeout to be safe.
    LOG_I("Waiting for Pico to self-reset and boot with new firmware...");
    
    bool picoReconnected = false;
    for (int i = 0; i < 350; i++) {  // Wait up to 35 seconds
        delay(100);
        feedWatchdog();
        _picoUart.loop();  // Process incoming packets
        
        // Check if Pico sent any packets (heartbeat or boot info)
        if (_picoUart.isConnected()) {
            LOG_I("Pico reconnected after self-reset (%d ms)", i * 100);
            picoReconnected = true;
            break;
        }
    }
    
    // Only force reset if Pico didn't come back on its own
    if (!picoReconnected) {
        LOG_W("Pico did not self-reset, forcing manual reset...");
        _picoUart.resetPico();
        
        // Wait for boot after manual reset (up to 10 seconds)
        LOG_I("Waiting for Pico to boot after manual reset...");
        for (int i = 0; i < 100; i++) {  // 10 seconds
            delay(100);
            feedWatchdog();
            _picoUart.loop();
            
            if (_picoUart.isConnected()) {
                LOG_I("Pico connected after manual reset (%d ms)", i * 100);
                picoReconnected = true;
                break;
            }
        }
        
        if (!picoReconnected) {
            LOG_E("Pico failed to connect after manual reset");
            restoreLogForwarding();  // Restore log forwarding state
            return false;
        }
    }
    
    LOG_I("Pico OTA complete!");
    restoreLogForwarding();  // Restore log forwarding state
    return true;
}

// =============================================================================
// ESP32 OTA - Download and flash ESP32 firmware + LittleFS
// =============================================================================

void BrewWebServer::startGitHubOTA(const String& version) {
    LOG_I("Starting ESP32 GitHub OTA for version: %s", version.c_str());
    
    // Build URL
    char tag[32];
    if (strcmp(version.c_str(), "dev-latest") != 0 && strncmp(version.c_str(), "v", 1) != 0) {
        snprintf(tag, sizeof(tag), "v%s", version.c_str());
    } else {
        strncpy(tag, version.c_str(), sizeof(tag) - 1);
        tag[sizeof(tag) - 1] = '\0';
    }
    
    // Get variant-specific asset name
    String esp32AssetName = getESP32AssetName();
    String firmwareVariant = getFirmwareVariant();
    LOG_I("Firmware variant: %s, asset: %s", firmwareVariant.c_str(), esp32AssetName.c_str());
    
    char downloadUrl[256];
    snprintf(downloadUrl, sizeof(downloadUrl), 
             "https://github.com/" GITHUB_OWNER "/" GITHUB_REPO "/releases/download/%s/%s", 
             tag, esp32AssetName.c_str());
    LOG_I("ESP32 download URL: %s", downloadUrl);
    
    broadcastOtaProgress(&_ws, "download", 65, "Downloading ESP32 firmware...");
    
    // Configure secure client (services are paused to free memory for SSL buffers)
    WiFiClientSecure client;
    client.setInsecure(); // Skip cert verification for speed/simplicity
    // CRITICAL FIX: Set explicit read timeout.
    // Default is often -1 (wait forever) or too short.
    client.setTimeout(15); // 15 seconds read timeout
    
    // Download ESP32 firmware
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(OTA_HTTP_TIMEOUT_MS);
    
    int httpCode = 0;
    
    for (int retry = 0; retry < OTA_MAX_RETRIES; retry++) {
        feedWatchdog();
        
        if (!http.begin(client, downloadUrl)) {
            LOG_E("HTTP begin failed (attempt %d/%d)", retry + 1, OTA_MAX_RETRIES);
            if (retry < OTA_MAX_RETRIES - 1) {
                for (int i = 0; i < 30; i++) { delay(100); feedWatchdog(); }
                continue;
            }
            broadcastLogLevel("error", "Update error: Cannot connect");
            broadcastOtaProgress(&_ws, "error", 0, "Connection failed");
            return;
        }
        
        http.addHeader("User-Agent", "BrewOS-ESP32/" ESP32_VERSION);
        feedWatchdog();
        httpCode = http.GET();
        feedWatchdog();
        
        if (httpCode == HTTP_CODE_OK) break;
        
        LOG_W("HTTP error %d (attempt %d/%d)", httpCode, retry + 1, OTA_MAX_RETRIES);
        http.end();
        
        if ((httpCode == 503 || httpCode == 429 || httpCode == 500) && retry < OTA_MAX_RETRIES - 1) {
            for (unsigned long i = 0; i < OTA_RETRY_DELAY_MS / 100; i++) {
                delay(100);
                feedWatchdog();
            }
            continue;
        }
        
        broadcastLogLevel("error", "Update error: HTTP %d", httpCode);
        broadcastOtaProgress(&_ws, "error", 0, "Download failed");
        return;
    }
    
    if (httpCode != HTTP_CODE_OK) {
        broadcastLogLevel("error", "Update error: Download failed");
        broadcastOtaProgress(&_ws, "error", 0, "Download failed");
        return;
    }
    
    int contentLength = http.getSize();
    if (contentLength <= 0) {
        LOG_E("Invalid content length: %d", contentLength);
        broadcastLogLevel("error", "Update error: Invalid firmware");
        broadcastOtaProgress(&_ws, "error", 0, "Invalid firmware");
        http.end();
        return;
    }
    
    LOG_I("ESP32 firmware size: %d bytes", contentLength);
    
    // Begin OTA update
    if (!Update.begin(contentLength)) {
        LOG_E("Not enough space for OTA");
        broadcastLogLevel("error", "Update error: Not enough space");
        broadcastOtaProgress(&_ws, "error", 0, "Not enough space");
        http.end();
        return;
    }
    
    broadcastOtaProgress(&_ws, "download", 70, "Installing ESP32 firmware...");
    
    // Stream firmware to flash
    WiFiClient* stream = http.getStreamPtr();
    const size_t HEAP_BUFFER_SIZE = 4096;
    std::unique_ptr<uint8_t[]> buffer(new (std::nothrow) uint8_t[HEAP_BUFFER_SIZE]);
    if (!buffer) {
        LOG_E("Failed to allocate buffer");
        Update.abort();
        http.end();
        broadcastLogLevel("error", "Update error: Out of memory");
        broadcastOtaProgress(&_ws, "error", 0, "Out of memory");
        return;
    }
    size_t written = 0;
    unsigned long lastYield = millis();
    unsigned long downloadStart = millis();
    unsigned long lastProgressLog = 0;  // For console progress logging
    unsigned long lastDataReceived = millis();  // Track stalls
    constexpr unsigned long STALL_TIMEOUT_MS = 30000;  // 30 second stall timeout
    
    LOG_I("Starting ESP32 firmware download...");
    
    while (http.connected() && written < (size_t)contentLength) {
        // Check overall timeout
        if (millis() - downloadStart > OTA_DOWNLOAD_TIMEOUT_MS) {
            LOG_E("Download timeout after %lu ms (wrote %d/%d bytes)", 
                  millis() - downloadStart, written, contentLength);
            Update.abort();
            http.end();
            broadcastLogLevel("error", "Update error: Timeout");
            broadcastOtaProgress(&_ws, "error", 0, "Timeout");
            return;
        }
        
        // Check for stall (no data for 30 seconds)
        if (millis() - lastDataReceived > STALL_TIMEOUT_MS) {
            LOG_E("Download stalled - no data for %lu ms (wrote %d/%d bytes)", 
                  STALL_TIMEOUT_MS, written, contentLength);
            Update.abort();
            http.end();
            broadcastLogLevel("error", "Update error: Connection stalled");
            broadcastOtaProgress(&_ws, "error", 0, "Connection stalled");
            return;
        }
        
        // Feed watchdog
        if (millis() - lastYield >= OTA_WATCHDOG_FEED_INTERVAL_MS) {
            feedWatchdog();
            yield();
            lastYield = millis();
        }
        
        size_t available = stream->available();
        if (available > 0) {
            lastDataReceived = millis();  // Reset stall timer
            
            // Limit read size to prevent blocking main loop for too long
            size_t maxChunkSize = min(available, (size_t)4096);  // Max 4KB per chunk
            size_t toRead = min(maxChunkSize, HEAP_BUFFER_SIZE);
            
            // Yield before potentially blocking read operation
            feedWatchdog();
            yield();
            
            size_t bytesRead = stream->readBytes(buffer.get(), toRead);
            
            if (bytesRead > 0) {
                // Yield before potentially blocking write operation
                feedWatchdog();
                yield();
                
                size_t bytesWritten = Update.write(buffer.get(), bytesRead);
                if (bytesWritten != bytesRead) {
                    LOG_E("Write error at %d", written);
                    Update.abort();
                    http.end();
                    broadcastLogLevel("error", "Update error: Write failed");
                    broadcastOtaProgress(&_ws, "error", 0, "Write failed");
                    return;
                }
                
                // Yield after write to allow main loop to run
                // This prevents blocking the main loop for >1 second
                feedWatchdog();
                yield();
                vTaskDelay(pdMS_TO_TICKS(1));  // 1ms delay using FreeRTOS for better task scheduling
                written += bytesWritten;
                
                // Log to console every 2 seconds (UI uses animation, not progress bar)
                if (millis() - lastProgressLog > 2000) {
                    int pct = (written * 100) / contentLength;
                    LOG_I("ESP32 OTA: %d%% (%d/%d bytes)", pct, written, contentLength);
                    lastProgressLog = millis();
                }
            }
        } else {
            yield();  // Yield to other tasks while waiting for data
            feedWatchdog();
        }
    }
    
    http.end();
    feedWatchdog();
    
    if (written != (size_t)contentLength) {
        LOG_E("Download incomplete: %d/%d", written, contentLength);
        Update.abort();
        broadcastLogLevel("error", "Update error: Incomplete download");
        broadcastOtaProgress(&_ws, "error", 0, "Incomplete download");
        return;
    }
    
    broadcastOtaProgress(&_ws, "flash", 95, "Finalizing...");
    
    if (!Update.end(true)) {
        LOG_E("Update failed: %s", Update.errorString());
        broadcastLogLevel("error", "Update error: %s", Update.errorString());
        broadcastOtaProgress(&_ws, "error", 0, "Installation failed");
        return;
    }
    
    LOG_I("ESP32 firmware update successful!");
    
    // Update LittleFS (optional - continue even if fails)
    // Use timeout to ensure we don't hang forever
    unsigned long littlefsStart = millis();
    updateLittleFS(tag);
    unsigned long littlefsTime = millis() - littlefsStart;
    LOG_I("LittleFS update completed in %lu ms", littlefsTime);
    
    // CRITICAL: Always restart after successful OTA, regardless of LittleFS result
    // The new firmware is already flashed, we MUST reboot to use it
    LOG_I("OTA complete - restarting device in 2 seconds...");
    broadcastOtaProgress(&_ws, "complete", 100, "Update complete!");
    broadcastLogLevel("info", "BrewOS updated! Restarting...");
    
    // Flush any pending serial output
    Serial.flush();
    delay(100);
    
    // Give time for WebSocket message to send
    for (int i = 0; i < 20; i++) {
        delay(100);
        feedWatchdog();
        yield();
        Serial.flush();  // Ensure logs are sent
    }
    
    LOG_I("Restarting ESP32 now (firmware update complete)...");
    Serial.flush();
    delay(500);  // Final delay to ensure all messages are sent
    
    // Force restart - this should never return
    ESP.restart();
    
    // Should never reach here, but if ESP.restart() somehow fails, force reset via watchdog
    LOG_E("ESP.restart() returned - this should never happen!");
    delay(1000);
    // Force watchdog reset as last resort
    while(1) {
        // Don't feed watchdog - let it reset the device
        delay(100);
    }
}

/**
 * Update LittleFS filesystem (called after ESP32 firmware update)
 * Non-critical - continues even if fails
 */
void BrewWebServer::updateLittleFS(const char* tag) {
    LOG_I("Updating LittleFS...");
    broadcastOtaProgress(&_ws, "flash", 96, "Updating web UI...");
    
    char littlefsUrl[256];
    snprintf(littlefsUrl, sizeof(littlefsUrl), 
             "https://github.com/" GITHUB_OWNER "/" GITHUB_REPO "/releases/download/%s/" GITHUB_ESP32_LITTLEFS_ASSET, 
             tag);
    
    // Configure secure client (services are paused to free memory for SSL buffers)
    WiFiClientSecure client;
    client.setInsecure();
    // CRITICAL FIX: Set explicit read timeout.
    // Default is often -1 (wait forever) or too short.
    client.setTimeout(15); // 15 seconds read timeout
    
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(OTA_HTTP_TIMEOUT_MS);
    
    feedWatchdog();
    if (!http.begin(client, littlefsUrl)) {
        LOG_W("LittleFS download failed - continuing");
        return;
    }
    
    http.addHeader("User-Agent", "BrewOS-ESP32/" ESP32_VERSION);
    feedWatchdog();
    int httpCode = http.GET();
    feedWatchdog();
    
    if (httpCode != HTTP_CODE_OK) {
        LOG_W("LittleFS HTTP error: %d", httpCode);
        http.end();
        return;
    }
    
    int contentLength = http.getSize();
    if (contentLength <= 0) {
        LOG_W("Invalid LittleFS size");
        http.end();
        return;
    }
    
    // Find filesystem partition (could be named "littlefs" or "spiffs" depending on partition table)
    // PlatformIO's default partition tables use "spiffs" name even when using LittleFS filesystem
    const esp_partition_t* partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "littlefs");
    
    if (!partition) {
        // Try "spiffs" name (used in default_8MB.csv and other default partition tables)
        partition = esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
    }
    
    if (!partition) {
        LOG_W("Filesystem partition not found (tried littlefs and spiffs)");
        http.end();
        return;
    }
    
    LOG_I("Found filesystem partition: %s (%d bytes)", partition->label, partition->size);
    
    // Check if downloaded image fits in the partition
    // This prevents silent truncation when partition table has changed
    if ((size_t)contentLength > partition->size) {
        LOG_W("LittleFS image (%d bytes) exceeds partition size (%d bytes)", 
              contentLength, partition->size);
        LOG_W("Partition table mismatch - USB flash required for this upgrade");
        broadcastOtaProgress(&_ws, "error", 0, "Partition too small - USB flash required");
        http.end();
        return;
    }
    
    broadcastOtaProgress(&_ws, "flash", 97, "Erasing filesystem...");
    feedWatchdog();
    
    if (esp_partition_erase_range(partition, 0, partition->size) != ESP_OK) {
        LOG_W("Failed to erase LittleFS");
        http.end();
        return;
    }
    
    broadcastOtaProgress(&_ws, "flash", 98, "Installing web UI...");
    
    WiFiClient* stream = http.getStreamPtr();
    const size_t HEAP_BUFFER_SIZE = 4096;
    std::unique_ptr<uint8_t[]> buffer(new (std::nothrow) uint8_t[HEAP_BUFFER_SIZE]);
    if (!buffer) {
        LOG_W("Failed to allocate buffer for LittleFS update");
        http.end();
        return;
    }
    size_t written = 0;
    size_t offset = 0;
    unsigned long lastYield = millis();
    
    while (http.connected() && written < (size_t)contentLength && offset < partition->size) {
        if (millis() - lastYield >= OTA_WATCHDOG_FEED_INTERVAL_MS) {
            feedWatchdog();
            yield();
            lastYield = millis();
        }
        
        size_t available = stream->available();
        if (available > 0) {
            // Limit read size to prevent blocking main loop for too long
            size_t maxChunkSize = min(available, (size_t)4096);  // Max 4KB per chunk
            size_t toRead = min(maxChunkSize, HEAP_BUFFER_SIZE);
            
            // Yield before potentially blocking read operation
            feedWatchdog();
            yield();
            
            size_t bytesRead = stream->readBytes(buffer.get(), toRead);
            
            if (bytesRead > 0) {
                // Yield before potentially blocking write operation
                feedWatchdog();
                yield();
                
                if (esp_partition_write(partition, offset, buffer.get(), bytesRead) != ESP_OK) {
                    LOG_W("LittleFS write failed at offset %d", offset);
                    break;
                }
                written += bytesRead;
                offset += bytesRead;
                
                // Yield after write to allow main loop to run
                // This prevents blocking the main loop for >1 second
                feedWatchdog();
                yield();
                vTaskDelay(pdMS_TO_TICKS(1));  // 1ms delay using FreeRTOS for better task scheduling
            }
        } else {
            yield();
            feedWatchdog();
        }
    }
    
    http.end();
    feedWatchdog();
    
    if (written == (size_t)contentLength) {
        LOG_I("LittleFS updated: %d bytes", written);
    } else {
        LOG_W("LittleFS incomplete: %d/%d bytes (non-critical, continuing)", written, contentLength);
    }
    
    // NOTE: Do NOT remount LittleFS here - device will restart and mount on next boot
    // Attempting to mount/use LittleFS immediately after writing can cause corruption errors
    // The filesystem will be properly mounted on the next boot after ESP.restart()
}

// =============================================================================
// Combined OTA - Update Pico first, then ESP32
// =============================================================================

void BrewWebServer::startCombinedOTA(const String& version, bool isPendingOTA) {
    LOG_I("Starting combined OTA for version: %s%s", version.c_str(), isPendingOTA ? " (resuming after reboot)" : "");
    
    // IMMEDIATELY tell UI that OTA is starting - this triggers the overlay
    broadcastOtaProgress(&_ws, "download", 0, "Starting update...");
    broadcastLog("Starting BrewOS update to v%s...", version.c_str());
    
    unsigned long otaStart = millis();
    
    // Check if we have enough contiguous memory for SSL
    // Skip this check for pending OTA - we already rebooted once, rebooting again won't help
    // and would cause an infinite loop
    size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    LOG_I("Largest contiguous heap block: %zu bytes (need %zu)", largestBlock, OTA_MIN_CONTIGUOUS_HEAP);
    
    if (!isPendingOTA && largestBlock < OTA_MIN_CONTIGUOUS_HEAP) {
        LOG_W("Memory too fragmented for SSL OTA - rebooting to defragment");
        broadcastOtaProgress(&_ws, "download", 0, "Preparing memory...");
        broadcastLog("Memory fragmented - restarting for clean OTA...");
        
        // Save OTA version to NVS
        savePendingOTA(version);
        
        // Give UI time to show the message
        delay(2000);
        
        // Reboot - OTA will continue on fresh boot
        ESP.restart();
        return;  // Won't reach here, but for clarity
    }
    
    // Clear any pending OTA since we're proceeding now
    clearPendingOTA();
    
    // Validate prerequisites BEFORE pausing services
    uint8_t machineType = State.getMachineType();
    if (machineType == 0) {
        // Machine type unknown - try requesting from Pico (handles case where Pico booted before ESP32)
        LOG_I("Machine type unknown, requesting from Pico...");
        broadcastLog("Waiting for device connection...");
        
        // Try up to 3 times with 500ms delay between attempts
        for (int attempt = 0; attempt < 3 && machineType == 0; attempt++) {
            if (_picoUart.requestBootInfo()) {
                // Wait for response
                for (int i = 0; i < 10 && machineType == 0; i++) {
                    delay(100);
                    _picoUart.loop();
                    machineType = State.getMachineType();
                }
            }
            if (machineType == 0) {
                LOG_W("Attempt %d: No response from Pico", attempt + 1);
            }
        }
        
        if (machineType == 0) {
            LOG_E("Machine type still unknown after 3 attempts");
            broadcastLogLevel("error", "Update error: Please ensure machine is powered on and connected");
            broadcastOtaProgress(&_ws, "error", 0, "Device not ready");
            return;
        }
        LOG_I("Machine type received: %d", machineType);
    }
    
    // Pause ALL background services to prevent interference during OTA
    // This includes: cloud (SSL), MQTT, BLE scale, power meter HTTP polling, WebSocket clients
    pauseServicesForOTA(_cloudConnection, &_ws);
    
    // CRITICAL: Set _cloudConnection to nullptr after stopping it
    // This prevents crashes when broadcastLogLevel() tries to use it
    // The object may be in an invalid state after end() is called
    if (_cloudConnection) {
        _cloudConnection = nullptr;
        LOG_D("Set _cloudConnection to nullptr after stopping");
    }
    
    feedWatchdog();
    
    // Suppress non-essential broadcasts during OTA
    _otaInProgress = true;
    
    // Clean up any leftover files from previous attempts
    cleanupOtaFiles();
    
    // Log initial state
    size_t freeHeap = ESP.getFreeHeap();
    size_t totalFs = LittleFS.totalBytes();
    size_t usedFs = LittleFS.usedBytes();
    LOG_I("OTA starting: Free heap=%u, FS total=%u, FS used=%u, FS free=%u bytes", 
          freeHeap, totalFs, usedFs, totalFs - usedFs);
    
    broadcastOtaProgress(&_ws, "download", 0, "Preparing update...");
    feedWatchdog();
    
    // Step 1: Update Pico firmware
    LOG_I("Step 1/2: Updating Pico...");
    broadcastOtaProgress(&_ws, "download", 5, "Updating internal controller...");
    
    bool picoSuccess = startPicoGitHubOTA(version);
    feedWatchdog();
    
    if (!picoSuccess) {
        LOG_E("Pico OTA failed - aborting combined update");
        cleanupOtaFiles();
        handleOTAFailure(&_ws);  // Will restart device
        return;  // Won't reach here due to restart
    }
    
    // Wait for Pico to stabilize
    broadcastOtaProgress(&_ws, "flash", 58, "Verifying internal controller...");
    for (int i = 0; i < 30; i++) {
        delay(100);
        feedWatchdog();
        _picoUart.loop();  // Process any incoming packets
    }
    
    // Check if Pico came back up
    bool picoOk = _picoUart.isConnected();
    if (!picoOk) {
        LOG_E("Pico not responding after update - aborting");
        cleanupOtaFiles();
        handleOTAFailure(&_ws);  // Will restart device
        return;  // Won't reach here due to restart
    }
    LOG_I("Pico responded after update");
    
    // Request boot info to get the new version
    // The Pico should send MSG_BOOT on boot, but we also explicitly request it
    // to ensure we get the version even if MSG_BOOT was missed
    _picoUart.requestBootInfo();
    
    // Wait for boot info with retries (up to 3 seconds)
    // Pico might need time to fully boot and send MSG_BOOT
    const char* picoVersion = nullptr;
    for (int attempt = 0; attempt < 30; attempt++) {
        delay(100);
        feedWatchdog();
        _picoUart.loop();
        
        // Check if version was received
        picoVersion = State.getPicoVersion();
        if (picoVersion && picoVersion[0] != '\0') {
            LOG_I("Pico version received after %d ms", (attempt + 1) * 100);
            break;
        }
        
        // Request again every 1 second if not received yet
        if (attempt > 0 && attempt % 10 == 0) {
            LOG_I("Still waiting for Pico version, requesting boot info again...");
            _picoUart.requestBootInfo();
        }
    }
    
    // Verify Pico version after update
    // For dev-latest and beta channels (versions containing "-"), skip exact version matching
    // since the tag name differs from the actual firmware version (e.g., "dev-latest" vs "0.7.5")
    bool isDevOrBeta = (strcmp(version.c_str(), "dev-latest") == 0) || 
                       (strstr(version.c_str(), "-") != nullptr);
    
    if (picoVersion && picoVersion[0] != '\0') {
        if (isDevOrBeta) {
            // For dev/beta channels, just log the version - we can't verify against tag name
            LOG_I("Pico version after update: %s (dev/beta channel: %s - skipping version check)", 
                  picoVersion, version.c_str());
        } else {
            // For stable releases, verify exact version match
            LOG_I("Pico version after update: %s (expected: %s)", picoVersion, version.c_str());
            if (strcmp(picoVersion, version.c_str()) != 0) {
                LOG_E("Pico update FAILED! Got %s, expected %s", picoVersion, version.c_str());
                broadcastLogLevel("error", "Internal controller update failed");
                broadcastOtaProgress(&_ws, "error", 0, "Update failed - restarting...");
                cleanupOtaFiles();
                handleOTAFailure(&_ws);  // Will restart device
                return;  // Won't reach here due to restart
            }
        }
        LOG_I("Pico version verified: %s", picoVersion);
    } else {
        LOG_E("Could not verify Pico version after update - aborting");
        broadcastLogLevel("error", "Internal controller not responding");
        broadcastOtaProgress(&_ws, "error", 0, "Update failed - restarting...");
        cleanupOtaFiles();
        handleOTAFailure(&_ws);  // Will restart device
        return;  // Won't reach here due to restart
    }
    
    // Check total timeout
    if (millis() - otaStart > OTA_TOTAL_TIMEOUT_MS) {
        LOG_E("OTA timeout exceeded");
        broadcastLogLevel("error", "Update error: Timeout");
        cleanupOtaFiles();
        handleOTAFailure(&_ws);  // Will restart device
        return;  // Won't reach here due to restart
    }
    
    broadcastOtaProgress(&_ws, "download", 60, "Completing update...");
    feedWatchdog();
    
    // Ensure Pico firmware file is cleaned up before ESP32 OTA
    LOG_I("Cleaning up Pico firmware before ESP32 OTA...");
    cleanupOtaFiles();
    
    // Log free space before ESP32 OTA
    LOG_I("Before ESP32 OTA: Free heap=%u, Free FS=%u bytes", 
          ESP.getFreeHeap(), LittleFS.totalBytes() - LittleFS.usedBytes());
    
    // Step 2: Update ESP32 (will reboot on success)
    LOG_I("Step 2/2: Updating ESP32...");
    startGitHubOTA(version);
    
    // If we reach here, ESP32 update failed
    LOG_E("ESP32 update failed - cleaning up");
    cleanupOtaFiles();
    handleOTAFailure(&_ws);  // Will restart device
}

// =============================================================================
// Update Check - Query GitHub API
// =============================================================================

static int compareVersions(const String& v1, const String& v2) {
    int major1 = 0, minor1 = 0, patch1 = 0;
    int major2 = 0, minor2 = 0, patch2 = 0;
    
    String ver1 = v1.startsWith("v") ? v1.substring(1) : v1;
    sscanf(ver1.c_str(), "%d.%d.%d", &major1, &minor1, &patch1);
    
    String ver2 = v2.startsWith("v") ? v2.substring(1) : v2;
    sscanf(ver2.c_str(), "%d.%d.%d", &major2, &minor2, &patch2);
    
    if (major1 != major2) return major1 < major2 ? -1 : 1;
    if (minor1 != minor2) return minor1 < minor2 ? -1 : 1;
    if (patch1 != patch2) return patch1 < patch2 ? -1 : 1;
    return 0;
}

void BrewWebServer::checkForUpdates() {
    LOG_I("Checking for updates...");
    broadcastLogLevel("info", "Checking for updates...");
    
    String apiUrl = "https://api.github.com/repos/" GITHUB_OWNER "/" GITHUB_REPO "/releases/latest";
    
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(10000);
    
    feedWatchdog();
    if (!http.begin(apiUrl)) {
        LOG_E("Failed to connect to GitHub API");
        broadcastLogLevel("error", "Update check failed");
        return;
    }
    
    http.addHeader("User-Agent", "BrewOS-ESP32/" ESP32_VERSION);
    http.addHeader("Accept", "application/vnd.github.v3+json");
    
    feedWatchdog();
    int httpCode = http.GET();
    feedWatchdog();
    
    if (httpCode != HTTP_CODE_OK) {
        LOG_E("GitHub API error: %d", httpCode);
        broadcastLogLevel("error", "Update check failed: HTTP %d", httpCode);
        http.end();
        return;
    }
    
    String payload = http.getString();
    http.end();
    feedWatchdog();
    
    JsonDocument doc;
    if (deserializeJson(doc, payload)) {
        LOG_E("JSON parse error");
        broadcastLogLevel("error", "Update check failed");
        return;
    }
    
    String latestVersion = doc["tag_name"] | "";
    String releaseName = doc["name"] | "";
    String releaseBody = doc["body"] | "";
    bool prerelease = doc["prerelease"] | false;
    String publishedAt = doc["published_at"] | "";
    
    if (latestVersion.isEmpty()) {
        LOG_E("No version found");
        broadcastLogLevel("error", "Update check failed");
        return;
    }
    
    String latestVersionNum = latestVersion.startsWith("v") ? latestVersion.substring(1) : latestVersion;
    String currentVersion = ESP32_VERSION;
    
    LOG_I("Current: %s, Latest: %s", currentVersion.c_str(), latestVersionNum.c_str());
    
    int cmp = compareVersions(currentVersion, latestVersionNum);
    bool updateAvailable = (cmp < 0);
    
    // Check assets
    int esp32AssetSize = 0, picoAssetSize = 0;
    bool esp32AssetFound = false, picoAssetFound = false;
    
    uint8_t machineType = State.getMachineType();
    const char* picoAssetName = getPicoAssetName(machineType);
    
    // Get variant-specific ESP32 asset name
    String esp32AssetName = getESP32AssetName();
    String firmwareVariant = getFirmwareVariant();
    
    JsonArray assets = doc["assets"].as<JsonArray>();
    for (JsonObject asset : assets) {
        String name = asset["name"] | "";
        if (name == esp32AssetName) {
            esp32AssetSize = asset["size"] | 0;
            esp32AssetFound = true;
        }
        if (picoAssetName && name == picoAssetName) {
            picoAssetSize = asset["size"] | 0;
            picoAssetFound = true;
        }
    }
    
    bool combinedUpdateAvailable = updateAvailable && esp32AssetFound && picoAssetFound;
    
    // Broadcast result
    JsonDocument result;
    result["type"] = "update_check_result";
    result["updateAvailable"] = updateAvailable;
    result["combinedUpdateAvailable"] = combinedUpdateAvailable;
    result["currentVersion"] = currentVersion;
    result["currentPicoVersion"] = State.getPicoVersion();
    result["latestVersion"] = latestVersionNum;
    result["releaseName"] = releaseName;
    result["prerelease"] = prerelease;
    result["publishedAt"] = publishedAt;
    result["esp32AssetSize"] = esp32AssetSize;
    result["esp32AssetFound"] = esp32AssetFound;
    result["esp32AssetName"] = esp32AssetName;
    result["firmwareVariant"] = firmwareVariant;
    result["picoAssetSize"] = picoAssetSize;
    result["picoAssetFound"] = picoAssetFound;
    result["picoAssetName"] = picoAssetName ? picoAssetName : "unknown";
    result["machineType"] = machineType;
    
    if (releaseBody.length() > 500) {
        releaseBody = releaseBody.substring(0, 497) + "...";
    }
    result["changelog"] = releaseBody;
    
    String response;
    serializeJson(result, response);
    _ws.textAll(response);
    
    if (updateAvailable) {
        broadcastLog("BrewOS %s available (current: %s)", latestVersionNum.c_str(), currentVersion.c_str());
    } else {
        broadcastLog("BrewOS is up to date (%s)", currentVersion.c_str());
    }
}

// =============================================================================
// Helper Functions
// =============================================================================

const char* BrewWebServer::getPicoAssetName(uint8_t machineType) {
    switch (machineType) {
        case 1: return GITHUB_PICO_DUAL_BOILER_ASSET;
        case 2: return GITHUB_PICO_SINGLE_BOILER_ASSET;
        case 3: return GITHUB_PICO_HEAT_EXCHANGER_ASSET;
        default: return nullptr;
    }
}

bool BrewWebServer::checkVersionMismatch() {
    const char* picoVersion = State.getPicoVersion();
    const char* esp32Version = ESP32_VERSION;
    
    if (!picoVersion || picoVersion[0] == '\0') {
        return false;
    }
    
    char picoVer[16], esp32Ver[16];
    strncpy(picoVer, picoVersion[0] == 'v' ? picoVersion + 1 : picoVersion, sizeof(picoVer) - 1);
    picoVer[sizeof(picoVer) - 1] = '\0';
    strncpy(esp32Ver, esp32Version[0] == 'v' ? esp32Version + 1 : esp32Version, sizeof(esp32Ver) - 1);
    esp32Ver[sizeof(esp32Ver) - 1] = '\0';
    
    bool mismatch = (strcmp(picoVer, esp32Ver) != 0);
    
    if (mismatch) {
        LOG_W("Version mismatch: ESP32=%s, Pico=%s", esp32Ver, picoVer);
        
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        StaticJsonDocument<256> doc;
        #pragma GCC diagnostic pop
        doc["type"] = "version_mismatch";
        doc["currentVersion"] = esp32Ver;
        doc["message"] = "Firmware update recommended";
        
        size_t jsonSize = measureJson(doc) + 1;
        char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (!jsonBuffer) jsonBuffer = (char*)malloc(jsonSize);
        if (jsonBuffer) {
            serializeJson(doc, jsonBuffer, jsonSize);
            _ws.textAll(jsonBuffer);
            free(jsonBuffer);
        }
    }
    
    return mismatch;
}
