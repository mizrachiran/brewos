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
#include <algorithm>  // For std::min
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
    
    // 5.5. Disable LogManager file logging during OTA
    // LogManager's loop() periodically calls saveToFlash() which writes to /littlefs/logs.txt
    // This conflicts with LittleFS partition erase/update operations
    // We'll disable it temporarily - logs will still go to Serial and RAM buffer
    // Note: Pico log forwarding doesn't work during OTA anyway, so disabling is safe
    if (LogManager::instance().isEnabled()) {
        LOG_I("  - Disabling LogManager file logging during OTA...");
        // Temporarily disable to prevent saveToFlash() calls during LittleFS update
        // Logs will still go to Serial, just not saved to flash
        // LogManager will be restored on reboot (or can be re-enabled after OTA)
        LogManager::instance().disable();
    }
    
    // 6. Turn off display to free memory and reduce interference
    // Display uses PSRAM for buffers but turning it off reduces DMA activity
#if ENABLE_SCREEN
    extern Display display;
    LOG_I("  - Turning off display completely...");
    display.sleep();  // Fully power down display (backlight + sleep mode)
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
constexpr unsigned long OTA_HTTP_TIMEOUT_MS = 15000;       // 15 seconds HTTP timeout (reduced to fail faster on DNS/SSL hangs)
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
    
    String currentUrl = String(url);
    
    // We need to handle up to 3 redirects (GitHub -> AWS S3 usually takes 1)
    for (int redirect = 0; redirect < 3; redirect++) {
        
        // 1. Setup Client
    WiFiClientSecure client;
        client.setInsecure(); // Skip cert verification
        client.setTimeout(15); // 15s TCP timeout
    
    HTTPClient http;
        // CRITICAL: Do NOT follow redirects automatically. We do it manually to clean up SSL context.
        http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS); 
    http.setTimeout(OTA_HTTP_TIMEOUT_MS);
    
        // CRITICAL: Force HTTPClient to collect headers even when redirects are disabled
        // This ensures header() method works for redirect responses
        const char* headers[] = {"Location"};
        http.collectHeaders(headers, 1);
    
        LOG_I("Connecting to: %s", currentUrl.c_str());
    
    // Retry loop for transient errors
        int httpCode = 0;
    for (int retry = 0; retry < OTA_MAX_RETRIES; retry++) {
        feedWatchdog();
        
            // 2. Begin Connection
            if (!http.begin(client, currentUrl)) {
            LOG_E("HTTP begin failed (attempt %d/%d)", retry + 1, OTA_MAX_RETRIES);
            if (retry < OTA_MAX_RETRIES - 1) {
                    LOG_D("Retrying HTTP begin in 3 seconds...");
                for (int i = 0; i < 30; i++) { delay(100); feedWatchdog(); }
                continue;
            }
            return false;
        }
        
        http.addHeader("User-Agent", "BrewOS-ESP32/" ESP32_VERSION);
        
            // CRITICAL: Check WiFi status before attempting GET
            if (WiFi.status() != WL_CONNECTED) {
                LOG_E("WiFi disconnected before HTTP GET (attempt %d/%d)", retry + 1, OTA_MAX_RETRIES);
                http.end();
                if (retry < OTA_MAX_RETRIES - 1) {
                    LOG_I("Waiting for WiFi reconnection before retry...");
                    for (int i = 0; i < 50; i++) {  // Wait up to 5 seconds for WiFi
                        delay(100);
        feedWatchdog();
                        if (WiFi.status() == WL_CONNECTED) {
                            LOG_I("WiFi reconnected, retrying HTTP GET...");
                            break;
                        }
                    }
                    continue;
                }
                return false;
            }
            
            feedWatchdog();
            LOG_I("Sending HTTP GET request (attempt %d/%d, timeout=%lu ms)...", 
                  retry + 1, OTA_MAX_RETRIES, OTA_HTTP_TIMEOUT_MS);
            LOG_I("WiFi status: %d, IP: %s", WiFi.status(), WiFi.localIP().toString().c_str());
            
            // 3. GET Request
            feedWatchdog();
            unsigned long getStart = millis();
        httpCode = http.GET();
            unsigned long getTime = millis() - getStart;
        feedWatchdog();
        
            LOG_I("HTTP GET completed: code=%d, time=%lu ms", httpCode, getTime);
            
            // Check if GET hung
            if (getTime > OTA_HTTP_TIMEOUT_MS + 5000) {
                LOG_E("HTTP GET took %lu ms (exceeded timeout) - retrying...", getTime);
                http.end();
                if (retry < OTA_MAX_RETRIES - 1) {
                    delay(2000);
                    continue;
                }
                return false;
            }
            
            if (httpCode == HTTP_CODE_OK || httpCode == 301 || httpCode == 302 || httpCode == 307) {
            break;
        }
        
        LOG_W("HTTP error %d (attempt %d/%d)", httpCode, retry + 1, OTA_MAX_RETRIES);
        // Log response body if small (helps diagnose S3 errors like "Access Denied" or "NoSuchKey")
        if (http.getSize() > 0 && http.getSize() < 512) {
            String errorResponse = http.getString();
            LOG_W("Error Response: %s", errorResponse.c_str());
        }
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
    
        // 4. Handle Redirects (301, 302, 307)
        if (httpCode == 301 || httpCode == 302 || httpCode == 307) {
            // Try HTTPClient's header() method first (should work with collectHeaders())
            String newUrl = http.header("Location");
            if (newUrl.length() == 0) {
                newUrl = http.header("location");  // Try lowercase
            }
            if (newUrl.length() == 0) {
                newUrl = http.header("LOCATION");  // Try uppercase
            }
            
            LOG_I("Redirect detected (code=%d), Location header length: %d", httpCode, newUrl.length());
            
            // If header() didn't work, try reading from stream (fallback for HTTP/2 or edge cases)
            if (newUrl.length() == 0) {
                LOG_W("Location header not found via header() method, trying stream read...");
                WiFiClient* stream = http.getStreamPtr();
                
                if (stream && stream->available() > 0) {
                    LOG_I("Stream available: %d bytes", stream->available());
                    // Read response headers line by line
                    String headerLine;
                    bool foundLocation = false;
                    int bytesRead = 0;
                    const int MAX_HEADER_BYTES = 4096;  // Support very long URLs
                    
                    while (stream->available() > 0 && bytesRead < MAX_HEADER_BYTES && !foundLocation) {
                        char c = stream->read();
                        bytesRead++;
                        
                        if (c == '\n') {
                            headerLine.trim();
                            if (headerLine.length() == 0) {
                                break;  // End of headers
                            }
                            
                            // Check if this is the Location header (case-insensitive)
                            if (headerLine.startsWith("Location:") || headerLine.startsWith("location:")) {
                                int colonIdx = headerLine.indexOf(':');
                                if (colonIdx >= 0) {
                                    newUrl = headerLine.substring(colonIdx + 1);
                                    newUrl.trim();
                                    
                                    // Handle HTTP header folding (continuation lines start with space/tab)
                                    while (stream->available() > 0 && bytesRead < MAX_HEADER_BYTES) {
                                        char peek = stream->peek();
                                        if (peek == ' ' || peek == '\t') {
                                            // Continuation line - read and append
                                            String continuation;
                                            while (stream->available() > 0 && bytesRead < MAX_HEADER_BYTES) {
                                                char contChar = stream->read();
                                                bytesRead++;
                                                if (contChar == '\n') {
                                                    continuation.trim();
                                                    if (continuation.length() > 0) {
                                                        newUrl += continuation;
                                                        newUrl.trim();
                                                    }
                                                    break;
                                                } else if (contChar != '\r') {
                                                    continuation += contChar;
                                                }
                                            }
                                        } else {
                                            break;  // Next header line
                                        }
                                    }
                                    
                                    foundLocation = true;
                                    LOG_I("Found Location header in stream: %s", newUrl.length() > 100 ? (newUrl.substring(0, 100) + "...").c_str() : newUrl.c_str());
                                    break;
                                }
                            }
                            headerLine = "";
                        } else if (c != '\r') {
                            headerLine += c;
                        }
                    }
                    
                    if (!foundLocation) {
                        LOG_E("Location header not found in stream (read %d bytes)", bytesRead);
                    }
                } else {
                    LOG_E("Stream not available (stream=%p, available=%d)", stream, stream ? stream->available() : 0);
                }
            }
            
            LOG_I("Redirect to: %s", newUrl.length() > 0 ? (newUrl.length() > 100 ? (newUrl.substring(0, 100) + "...").c_str() : newUrl.c_str()) : "(empty)");
            
            if (newUrl.length() == 0) {
                LOG_E("Redirect with no Location header - cannot follow redirect");
                http.end();
        return false;
    }
    
            // Handle relative URLs (if Location doesn't start with http:// or https://)
            if (!newUrl.startsWith("http://") && !newUrl.startsWith("https://")) {
                // Extract base URL from currentUrl
                int schemeEnd = currentUrl.indexOf("://");
                if (schemeEnd >= 0) {
                    int pathStart = currentUrl.indexOf("/", schemeEnd + 3);
                    if (pathStart >= 0) {
                        String baseUrl = currentUrl.substring(0, pathStart);
                        if (newUrl.startsWith("/")) {
                            newUrl = baseUrl + newUrl;
                        } else {
                            // Relative path - append to current path's directory
                            int lastSlash = currentUrl.lastIndexOf("/");
                            if (lastSlash > schemeEnd + 3) {
                                String basePath = currentUrl.substring(0, lastSlash + 1);
                                newUrl = basePath + newUrl;
                            } else {
                                newUrl = baseUrl + "/" + newUrl;
                            }
                        }
                        LOG_I("Resolved relative URL to: %s", newUrl.c_str());
                    }
                }
            }
            
            // CRITICAL: End the current connection completely to free SSL buffers
            http.end();
            client.stop();
            
            // Update URL and loop to try again with fresh connection
            currentUrl = newUrl;
            
            // Small delay to let heap settle
            delay(100);
            continue; 
        }
        
        // 5. Handle Success (200 OK)
        if (httpCode == HTTP_CODE_OK) {
            // Check Content Length
    int contentLength = http.getSize();
            LOG_I("Content Length: %d", contentLength);
            
    if (contentLength <= 0 || contentLength > OTA_MAX_SIZE) {
                LOG_E("Invalid size");
        http.end();
        return false;
    }
    
    if (outFileSize) *outFileSize = contentLength;
    
    // Check available space
    size_t freeSpace = LittleFS.totalBytes() - LittleFS.usedBytes();
    if ((size_t)contentLength > freeSpace) {
        LOG_E("Not enough space: need %d, have %d", contentLength, freeSpace);
        http.end();
        return false;
    }
    
            // Prepare File
            if (LittleFS.exists(filePath)) LittleFS.remove(filePath);
    File file = LittleFS.open(filePath, "w");
    if (!file) {
                LOG_E("Failed to open file: %s", filePath);
        http.end();
        return false;
    }
    
            // 6. Download Loop
    WiFiClient* stream = http.getStreamPtr();
    std::unique_ptr<uint8_t[]> buffer(new (std::nothrow) uint8_t[OTA_BUFFER_SIZE]);
            
    if (!buffer) {
                LOG_E("OOM: Buffer alloc failed");
        file.close();
        http.end();
        return false;
    }
    
    size_t written = 0;
            unsigned long lastData = millis();
            unsigned long downloadStart = millis();
            
            while (http.connected() && written < (size_t)contentLength) {
                // Check overall timeout
        if (millis() - downloadStart > OTA_DOWNLOAD_TIMEOUT_MS) {
                    LOG_E("Download timeout after %lu ms (wrote %d/%d)", 
                          millis() - downloadStart, written, contentLength);
                    break;
                }
                
                size_t available = stream->available();
                if (available > 0) {
                    lastData = millis();
                    size_t readSize = min(available, (size_t)OTA_BUFFER_SIZE);
                    int bytesRead = stream->readBytes(buffer.get(), readSize);
                    
                    if (bytesRead > 0) {
                        file.write(buffer.get(), bytesRead);
                        written += bytesRead;
                        
                        // Feed watchdog occasionally
                        if (written % 4096 == 0) feedWatchdog();
                    }
                } else {
                    // Check timeouts
                    if (millis() - lastData > 10000) { // 10s stall timeout
                        LOG_E("Download stalled");
                        break;
                    }
                    delay(10);
                }
            }
            
            file.close();
            
            if (written != contentLength) {
                LOG_E("Download truncated: %d/%d", written, contentLength);
            http.end();
            return false;
        }
        
            // Calculate CRC32 for integrity verification
            LOG_I("Calculating CRC32 for downloaded file...");
            File verifyFile = LittleFS.open(filePath, "r");
            if (!verifyFile) {
                LOG_E("Failed to open file for CRC32 calculation");
                    http.end();
                    return false;
                }
            
            uint32_t fileCRC32 = 0xFFFFFFFF;
            const uint32_t crc32Polynomial = 0xEDB88320;
            uint8_t crcBuffer[512];
            size_t bytesRead = 0;
            
            while ((bytesRead = verifyFile.read(crcBuffer, sizeof(crcBuffer))) > 0) {
                for (size_t i = 0; i < bytesRead; i++) {
                    fileCRC32 ^= crcBuffer[i];
                    for (int j = 0; j < 8; j++) {
                        if (fileCRC32 & 1) {
                            fileCRC32 = (fileCRC32 >> 1) ^ crc32Polynomial;
                        } else {
                            fileCRC32 >>= 1;
                        }
                    }
                }
                feedWatchdog();
            }
            fileCRC32 = ~fileCRC32;
            verifyFile.close();
            
            // Store CRC32 in Preferences for verification during streaming
            Preferences prefs;
            prefs.begin("ota", false);  // Read-write
            bool stored = prefs.putUInt("pico_crc32", fileCRC32);
            prefs.end();
            
            if (!stored) {
                LOG_W("Failed to store CRC32 in Preferences - integrity check will be disabled");
            } else {
                LOG_I("CRC32 stored successfully: 0x%08X", fileCRC32);
            }
            
            LOG_I("Download complete: %d bytes, CRC32=0x%08X", written, fileCRC32);
            http.end();
            return true;
        }
        
        // Handle Error - log response body if small (helps diagnose S3 errors like "Access Denied" or "NoSuchKey")
        LOG_E("HTTP Error: %d", httpCode);
        if (http.getSize() > 0 && http.getSize() < 512) {
            String errorResponse = http.getString();
            LOG_W("Error Response: %s", errorResponse.c_str());
        }
        http.end();
        return false;
    }
    
    LOG_E("Too many redirects");
        return false;
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
    // Method selection: SWD if enabled (SWD_SUPPORTED=1), otherwise UART bootloader
    // Both screen and no-screen variants use UART bootloader (SWD_SUPPORTED=0)
    // UART bootloader is the default and recommended method for OTA updates
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
    
    bool swdSuccess = false;
    if (swd.begin()) {
        swdSuccess = true;
        LOG_I("SWD connection successful, using SWD method");
    } else {
        LOG_W("SWD connection failed, falling back to UART bootloader");
        broadcastLogLevel("warning", "SWD unavailable, using UART bootloader");
        broadcastOtaProgress(&_ws, "flash", 40, "SWD unavailable, using UART...");
        
        // CRITICAL: Always reset Pico even on failure to ensure it's not stuck
        LOG_W("SWD: Resetting Pico after failed SWD attempt...");
        swd.end();  // Clean up SWD connection
        swd.resetTarget();  // Reset Pico to ensure it boots normally
        
        // Reinitialize Serial1 UART for fallback to UART bootloader
        Serial1.begin(PICO_UART_BAUD, SERIAL_8N1, PICO_UART_RX_PIN, PICO_UART_TX_PIN);
        delay(10);
        _picoUart.resume();  // Resume UART processing for bootloader method
        
        // Fall through to UART bootloader method below
        swdSuccess = false;
    }
    
    if (swdSuccess) {
        // Continue with SWD method
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
            LOG_E("SWD firmware flashing failed, falling back to UART bootloader");
            broadcastLogLevel("warning", "SWD flash failed, trying UART bootloader");
            broadcastOtaProgress(&_ws, "flash", 40, "SWD failed, using UART...");
        
        // CRITICAL: Always reset Pico even on failure to ensure it's not stuck
        LOG_W("SWD: Resetting Pico after failed flash attempt...");
        swd.end();  // Clean up SWD connection
        swd.resetTarget();  // Reset Pico to ensure it boots normally
        
            // Reinitialize Serial1 UART for fallback to UART bootloader
        Serial1.begin(PICO_UART_BAUD, SERIAL_8N1, PICO_UART_RX_PIN, PICO_UART_TX_PIN);
        delay(10);
            _picoUart.resume();  // Resume UART processing for bootloader method
            
            // Reset file position for UART bootloader
            flashFile = LittleFS.open(OTA_FILE_PATH, "r");
            if (!flashFile) {
                LOG_E("Failed to reopen firmware file for UART bootloader");
                cleanupOtaFiles();
        return false;
    }
    
            // Fall through to UART bootloader method below
            swdSuccess = false;
        } else {
            // SWD succeeded - reset and wait for reconnect
    LOG_I("Resetting Pico after successful SWD flash...");
    swd.end();  // Clean up SWD connection first
    swd.resetTarget();  // Then reset
    
    broadcastOtaProgress(&_ws, "flash", 55, "Waiting for device restart...");
    
    // Reinitialize Serial1 UART after SWD is done
    Serial1.begin(PICO_UART_BAUD, SERIAL_8N1, PICO_UART_RX_PIN, PICO_UART_TX_PIN);
            delay(10);
    
    // Resume packet processing to detect when Pico comes back
    _picoUart.resume();
    LOG_I("Resumed UART packet processing");
    
    // Clear connection state so we can detect when Pico actually reconnects
    _picoUart.clearConnectionState();
    
            // Wait for Pico to reconnect (same logic as UART bootloader)
            bool picoReconnected = false;
            for (int i = 0; i < 350; i++) {
                delay(100);
                feedWatchdog();
                _picoUart.loop();
                
                if (_picoUart.isConnected()) {
                    LOG_I("Pico reconnected after SWD flash (%d ms)", i * 100);
                    picoReconnected = true;
                    break;
                }
            }
            
            if (!picoReconnected) {
                LOG_W("Pico did not reconnect after SWD flash, forcing manual reset...");
                _picoUart.resetPico();
                
                for (int i = 0; i < 100; i++) {
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
                    LOG_E("Pico failed to connect after SWD flash and manual reset");
                    return false;
                }
            }
            
            LOG_I("Pico OTA complete!");
            return true;
        }
    }
    
#else // SWD_SUPPORTED == 0
    // =============================================================================
    // UART BOOTLOADER METHOD (when SWD_SUPPORTED is 0)
    // =============================================================================
    
    // Retry configuration (defined at function scope for use in multiple retry loops)
    const int MAX_HANDSHAKE_RETRIES = 5;  // Increased retries for robustness
    
    // Send bootloader command with retry mechanism
    broadcastOtaProgress(&_ws, "flash", 42, "Preparing device...");
    feedWatchdog();
    
    // NOTE: We keep UART processing ACTIVE during bootloader handshake so we can receive log messages
    // The waitForBootloaderAck() function reads directly from Serial1, so it won't be affected
    // We only pause UART processing AFTER receiving the bootloader ACK, before firmware streaming
    bool handshakeSuccess = false;
    
    for (int handshakeRetry = 0; handshakeRetry < MAX_HANDSHAKE_RETRIES && !handshakeSuccess; handshakeRetry++) {
        if (handshakeRetry > 0) {
            // Exponential backoff: 200ms, 400ms, 800ms, 1600ms, 3200ms
            unsigned long backoffDelay = 200 * (1UL << (handshakeRetry - 1));
            if (backoffDelay > 2000) backoffDelay = 2000;  // Cap at 2s
            
            LOG_W("Retrying bootloader handshake (attempt %d/%d, backoff: %lu ms)...", 
                  handshakeRetry + 1, MAX_HANDSHAKE_RETRIES, backoffDelay);
            broadcastOtaProgress(&_ws, "flash", 42, "Retrying device connection...");
            feedWatchdog();
            
            // Drain UART buffer before retry
            unsigned long drainStart = millis();
            while (Serial1.available() && (millis() - drainStart) < 200) {
                Serial1.read();
            }
            
            // Exponential backoff delay
            delay(backoffDelay);
            feedWatchdog();
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
                return false;
            }
        }
        
        // Give Pico time to process command and enter bootloader mode
        // Pico sequence: sends protocol ACK, calls bootloader_prepare(), then sends bootloader ACK
        // bootloader_prepare() ensures system is ready before ACK is sent
        LOG_I("Sent bootloader command, waiting for Pico to enter bootloader...");
        feedWatchdog();
        
        // Process UART packets while waiting for bootloader ACK
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
    
    // Stream to Pico via UART bootloader with robust retry mechanism
    const int MAX_UPDATE_RETRIES = 3;  // Increased retries for robustness
    bool success = false;
    
    for (int updateRetry = 0; updateRetry < MAX_UPDATE_RETRIES && !success; updateRetry++) {
        if (updateRetry > 0) {
            // Exponential backoff between update retries: 1s, 2s, 4s
            unsigned long backoffDelay = 1000 * (1UL << (updateRetry - 1));
            if (backoffDelay > 4000) backoffDelay = 4000;  // Cap at 4s
            
            LOG_W("Retrying Pico firmware update (attempt %d/%d, backoff: %lu ms)...", 
                  updateRetry + 1, MAX_UPDATE_RETRIES, backoffDelay);
            broadcastOtaProgress(&_ws, "flash", 45, "Retrying installation...");
            
            // Exponential backoff before retry
            unsigned long backoffStart = millis();
            while ((millis() - backoffStart) < backoffDelay) {
                delay(100);
            feedWatchdog();
            }
            
            // CRITICAL: Resume UART processing first so we can detect Pico state
            // UART was paused during firmware streaming, we need it active to receive messages
            _picoUart.resume();
            
            // CRITICAL: Wait for Pico to exit bootloader mode naturally before resetting
            // Resetting while Pico is in bootloader mode (especially during flash operations)
            // can corrupt flash or leave Pico unresponsive. We must wait for it to exit first.
            // The bootloader has a 30-second timeout, but we'll wait up to 10 seconds for it to exit
            // to avoid waiting too long while still being safe.
            LOG_I("Waiting for Pico to exit bootloader mode before retry (max 10 seconds)...");
            
            // Clear connection state to detect fresh packets
            _picoUart.clearConnectionState();
            
            // Wait for Pico to exit bootloader mode and resume normal operation
            // The Pico should exit bootloader mode when it times out waiting for chunks
            // (bootloader timeout is 30 seconds, but we'll check for up to 10 seconds)
            // We'll detect it exited bootloader when it sends protocol packets
            bool picoExitedBootloader = false;
            uint32_t waitStart = millis();
            uint32_t lastPacketCount = _picoUart.getPacketsReceived();
            
            for (int i = 0; i < 100; i++) {  // Wait up to 10 seconds
                delay(100);
                feedWatchdog();
                _picoUart.loop();
                
                // Check if we received a NEW protocol packet (Pico exited bootloader)
                uint32_t currentPacketCount = _picoUart.getPacketsReceived();
                if (currentPacketCount > lastPacketCount) {
                    // Pico sent a protocol packet - it has exited bootloader mode
                    uint32_t waitTime = millis() - waitStart;
                    LOG_I("Pico exited bootloader mode after %lu ms (received %lu packets)", 
                          waitTime, currentPacketCount - lastPacketCount);
                    picoExitedBootloader = true;
                    break;
                }
            }
            
            // If Pico didn't exit bootloader mode, reset it as a last resort
            // But only after waiting to ensure we're not interrupting a flash operation
            // Note: Bootloader timeout is 30 seconds, but we only wait 10 seconds to avoid long delays
            if (!picoExitedBootloader) {
                LOG_W("Pico did not exit bootloader mode after 10 seconds, resetting as last resort...");
                _picoUart.resetPico();
                delay(1000);  // Wait for Pico to boot after reset
                
                // Wait for Pico to reconnect and send a fresh boot message
                LOG_I("Waiting for Pico to reconnect and send boot message after reset...");
                bool picoReady = false;
                uint32_t resetTime = millis();
                lastPacketCount = _picoUart.getPacketsReceived();
                
                for (int i = 0; i < 100; i++) {  // Wait up to 10 seconds
                    delay(100);
                    feedWatchdog();
                    _picoUart.loop();
                    
                    // Check if we received a NEW packet after reset (boot message or handshake)
                    uint32_t currentPacketCount = _picoUart.getPacketsReceived();
                    if (currentPacketCount > lastPacketCount) {
                        // We got a new packet - Pico is alive and communicating
                        uint32_t reconnectTime = millis() - resetTime;
                        LOG_I("Pico reconnected after reset (%lu ms, received %lu packets)", 
                              reconnectTime, currentPacketCount - lastPacketCount);
                        
                        // Wait a bit more for Pico to fully initialize and be ready for commands
                        delay(1000);
                        picoReady = true;
                        break;
                    }
                }
                
                if (!picoReady) {
                    LOG_E("Pico did not send boot message after reset, aborting retry");
                    // Clear connection state and wait a bit more before trying next retry
                    _picoUart.clearConnectionState();
                    delay(2000);
                    continue;  // Try next update retry
                }
            } else {
                // Pico exited bootloader mode naturally - give it a moment to stabilize
                delay(500);
            }
            
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
                    // Exponential backoff: 200ms, 400ms, 800ms, 1600ms, 3200ms
                    unsigned long backoffDelay = 200 * (1UL << (retryHandshakeAttempt - 1));
                    if (backoffDelay > 2000) backoffDelay = 2000;  // Cap at 2s
                    
                    LOG_W("Retrying bootloader handshake on update retry (attempt %d/%d, backoff: %lu ms)...", 
                          retryHandshakeAttempt + 1, MAX_HANDSHAKE_RETRIES, backoffDelay);
                    
                    // Drain UART buffer before retry
                    unsigned long drainStart = millis();
                    while (Serial1.available() && (millis() - drainStart) < 200) {
                        Serial1.read();
                    }
                    
                    // Exponential backoff delay
                    delay(backoffDelay);
                    feedWatchdog();
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
    
    // Wait for Pico to self-reset and reconnect (common code for both SWD and UART methods)
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
            return false;
        }
    }
    
    LOG_I("Pico OTA complete!");
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
    
    String currentUrl = String(downloadUrl);
    HTTPClient http;
    WiFiClientSecure client;
    int httpCode = 0;
    
    // Handle redirects manually (up to 3 redirects)
    for (int redirect = 0; redirect < 3; redirect++) {
        // Setup client for this redirect attempt
        client.setInsecure();
        client.setTimeout(15);
        http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.setTimeout(OTA_HTTP_TIMEOUT_MS);
    
        // Retry loop for transient errors
    for (int retry = 0; retry < OTA_MAX_RETRIES; retry++) {
        feedWatchdog();
        
            if (!http.begin(client, currentUrl)) {
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
    // CRITICAL FIX: Force server to close connection after transfer
    // This prevents hanging on Keep-Alive when Content-Length is unknown/wrong
    http.addHeader("Connection", "close");
    // CRITICAL: Force HTTPClient to collect headers even when redirects are disabled
    const char* headers[] = {"Location"};
    http.collectHeaders(headers, 1);
        feedWatchdog();
        httpCode = http.GET();
        feedWatchdog();
        
            if (httpCode == HTTP_CODE_OK || httpCode == 301 || httpCode == 302 || httpCode == 307) {
                break;
            }
        
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
        
        // Handle redirects
        if (httpCode == 301 || httpCode == 302 || httpCode == 307) {
            // Try HTTPClient's header() method (should work with collectHeaders())
            String newUrl = http.header("Location");
            if (newUrl.length() == 0) {
                newUrl = http.header("location");  // Try lowercase
            }
            if (newUrl.length() == 0) {
                newUrl = http.header("LOCATION");  // Try uppercase
            }
            
            // If still empty, try reading from response stream
            // GitHub Location headers can be very long (1000+ chars with query params)
            if (newUrl.length() == 0) {
                WiFiClient* stream = http.getStreamPtr();
                if (stream && stream->available() > 0) {
                    String headerLine;
                    bool inLocationHeader = false;
                    while (stream->available() > 0 && headerLine.length() < 2000) {  // Increased limit for long URLs
                        char c = stream->read();
                        if (c == '\n') {
                            headerLine.trim();
                            if (headerLine.length() == 0) {
                                if (inLocationHeader) break;  // End of Location header
                                break;  // End of headers
                            }
                            if (headerLine.startsWith("Location:") || headerLine.startsWith("location:")) {
                                inLocationHeader = true;
                                int colonIdx = headerLine.indexOf(':');
                                if (colonIdx >= 0) {
                                    newUrl = headerLine.substring(colonIdx + 1);
                                    newUrl.trim();
                                    // Check if header continues on next line (HTTP header folding)
                                    if (stream->available() > 0) {
                                        char peek = stream->peek();
                                        if (peek == ' ' || peek == '\t') {
                                            continue;  // Header continuation
                                        }
                                    }
                                    LOG_I("Found Location header in stream (length: %d)", newUrl.length());
                                    break;
                                }
                            } else if (inLocationHeader && (headerLine.startsWith(" ") || headerLine.startsWith("\t"))) {
                                // Header continuation (HTTP header folding)
                                newUrl += headerLine;
                                newUrl.trim();
                            } else {
                                inLocationHeader = false;
                            }
                            if (!inLocationHeader) headerLine = "";
                        } else if (c != '\r') {
                            headerLine += c;
                        }
                    }
                }
            }
            
            LOG_I("Redirect detected (code=%d) to: %s", httpCode, newUrl.length() > 0 ? (newUrl.length() > 100 ? (newUrl.substring(0, 100) + "...").c_str() : newUrl.c_str()) : "(empty)");
            
            if (newUrl.length() == 0) {
                LOG_E("Redirect with no Location header");
                http.end();
                broadcastLogLevel("error", "Update error: Invalid redirect");
                broadcastOtaProgress(&_ws, "error", 0, "Invalid redirect");
                return;
            }
            
            // Handle relative URLs
            if (!newUrl.startsWith("http://") && !newUrl.startsWith("https://")) {
                int schemeEnd = currentUrl.indexOf("://");
                if (schemeEnd >= 0) {
                    int pathStart = currentUrl.indexOf("/", schemeEnd + 3);
                    if (pathStart >= 0) {
                        String baseUrl = currentUrl.substring(0, pathStart);
                        if (newUrl.startsWith("/")) {
                            newUrl = baseUrl + newUrl;
                        } else {
                            int lastSlash = currentUrl.lastIndexOf("/");
                            if (lastSlash > schemeEnd + 3) {
                                newUrl = currentUrl.substring(0, lastSlash + 1) + newUrl;
                            } else {
                                newUrl = baseUrl + "/" + newUrl;
                            }
                        }
                    }
                }
            }
            
            // CRITICAL: End the current connection completely to free SSL buffers
            http.end();
            client.stop();
            
            // Update URL and loop to try again with fresh connection
            currentUrl = newUrl;
            
            // Small delay to let heap settle
            delay(100);
            continue;
        }
        
        // Success - break out of redirect loop
        if (httpCode == HTTP_CODE_OK) {
            break;
        }
        
        // Error
        http.end();
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
    
    // Calculate CRC32 during download for integrity verification
    uint32_t streamCRC32 = 0xFFFFFFFF;
    const uint32_t crc32Polynomial = 0xEDB88320;
    
    LOG_I("Starting ESP32 firmware download with CRC32 verification...");
    
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
                // Calculate CRC32 for this chunk BEFORE writing to flash
                // This verifies data integrity during download
                for (size_t i = 0; i < bytesRead; i++) {
                    streamCRC32 ^= buffer[i];
                    for (int j = 0; j < 8; j++) {
                        if (streamCRC32 & 1) {
                            streamCRC32 = (streamCRC32 >> 1) ^ crc32Polynomial;
                        } else {
                            streamCRC32 >>= 1;
                        }
                    }
                }
                
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
    
    // Finalize CRC32 calculation
    streamCRC32 = ~streamCRC32;
    LOG_I("ESP32 firmware download complete: %d bytes, CRC32=0x%08X", written, streamCRC32);
    
    // Check if we have an expected CRC32 to verify against
    // (This could come from release metadata or a previous successful download)
    Preferences prefs;
    prefs.begin("ota", true);  // Read-only first
    uint32_t expectedCRC32 = prefs.getUInt("esp32_expected_crc32", 0);
    prefs.end();
    
    if (expectedCRC32 != 0) {
        if (streamCRC32 != expectedCRC32) {
            LOG_E("ESP32 firmware CRC32 MISMATCH! Expected: 0x%08X, Got: 0x%08X", expectedCRC32, streamCRC32);
            LOG_E("Data integrity check FAILED - firmware may be corrupted");
            Update.abort();
            broadcastLogLevel("error", "Firmware integrity check failed");
            broadcastOtaProgress(&_ws, "error", 0, "Integrity check failed");
            return;
        } else {
            LOG_I("ESP32 firmware CRC32 verified: 0x%08X - data integrity OK", streamCRC32);
        }
    } else {
        LOG_I("ESP32 firmware CRC32 calculated: 0x%08X (no expected value to verify)", streamCRC32);
    }
    
    // Store CRC32 in Preferences for future reference/verification
    prefs.begin("ota", false);  // Read-write
    prefs.putUInt("esp32_crc32", streamCRC32);
    prefs.end();
    LOG_I("Stored ESP32 firmware CRC32: 0x%08X", streamCRC32);
    
    broadcastOtaProgress(&_ws, "flash", 95, "Finalizing...");
    
    if (!Update.end(true)) {
        LOG_E("Update failed: %s", Update.errorString());
        broadcastLogLevel("error", "Update error: %s", Update.errorString());
        broadcastOtaProgress(&_ws, "error", 0, "Installation failed");
        return;
    }
    
    LOG_I("ESP32 firmware update successful! CRC32 verified: 0x%08X", streamCRC32);
    
    // Update LittleFS (optional - continue even if fails)
    // CRITICAL: LittleFS update is non-critical - firmware is already flashed
    // If LittleFS update hangs, we MUST still restart to boot the new firmware
    // The watchdog is disabled during OTA, so we can't rely on it for timeout
    // Instead, we'll try LittleFS update but ensure we always proceed to restart
    unsigned long littlefsStart = millis();
    const unsigned long LITTLEFS_MAX_TIME_MS = 120000;  // 2 minutes absolute max
    LOG_I("Starting LittleFS update (non-critical, max %lu seconds)...", LITTLEFS_MAX_TIME_MS / 1000);
    
    // Call updateLittleFS - it has internal timeouts, but we'll check duration
    updateLittleFS(tag);
    
    unsigned long littlefsTime = millis() - littlefsStart;
    if (littlefsTime > LITTLEFS_MAX_TIME_MS) {
        LOG_W("LittleFS update took too long (%lu ms > %lu ms), proceeding to restart", 
              littlefsTime, LITTLEFS_MAX_TIME_MS);
    } else {
        LOG_I("LittleFS update completed in %lu ms", littlefsTime);
    }
    
    // CRITICAL: Log that we're about to restart - this helps diagnose if we get stuck
    LOG_I("About to restart ESP32 - firmware update complete");
    
    // CRITICAL: Always restart after successful OTA, regardless of LittleFS result
    // The new firmware is already flashed, we MUST reboot to use it
    // Even if LittleFS update failed or timed out, we must restart
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
    
    LittleFS.end();
    
    char url[256];
    snprintf(url, sizeof(url), 
             "https://github.com/" GITHUB_OWNER "/" GITHUB_REPO "/releases/download/%s/" GITHUB_ESP32_LITTLEFS_ASSET, 
             tag);

    const esp_partition_t* partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "littlefs");
    if (!partition) partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
    
    if (!partition) {
        LOG_E("LittleFS partition not found");
        return;
    }

    // 1. Manual Redirect Loop with "Connection: close"
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(15); 
    
    String currentUrl = String(url);
    bool connected = false;
    int httpCode = 0;
    
    for (int i = 0; i < 3; i++) {
        http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
        http.begin(client, currentUrl);
        http.addHeader("User-Agent", "BrewOS/" ESP32_VERSION);
        http.addHeader("Connection", "close"); // Force server to close socket after send
        
        httpCode = http.GET();
        
        if (httpCode == 301 || httpCode == 302 || httpCode == 307) {
            String newUrl = http.header("Location");
            http.end();
            client.stop();
            if (newUrl.length() > 0) {
                currentUrl = newUrl;
                continue;
            }
        }
        if (httpCode == 200) {
            connected = true;
            break;
        }
        http.end();
        break;
    }
    
    if (!connected) {
        LOG_E("Download failed: %d", httpCode);
        return;
    }

    // 2. Parse Content-Length
    // CRITICAL: If server says 8MB, we MUST accept 8MB.
    // The file is likely padded to the full partition size.
    size_t contentLength = http.getSize();
    if (contentLength > partition->size) {
        LOG_E("File too large (%u > %u). Aborting.", contentLength, partition->size);
        http.end();
        return;
    }
    LOG_I("Downloading %u bytes...", contentLength);

    // 3. Erase Partition
    LOG_I("Erasing LittleFS partition...");
    const size_t ERASE_CHUNK = 65536;
    for (size_t offset = 0; offset < partition->size; offset += ERASE_CHUNK) {
        size_t len = std::min<size_t>(ERASE_CHUNK, partition->size - offset);
        esp_partition_erase_range(partition, offset, len);
        if (offset % (ERASE_CHUNK * 4) == 0) feedWatchdog();
        yield();
    }

    // 4. Stream Download
    WiFiClient* stream = http.getStreamPtr();
    const size_t BUFFER_SIZE = 4096;
    std::unique_ptr<uint8_t[]> buffer(new (std::nothrow) uint8_t[BUFFER_SIZE]);
    
    if (!buffer) {
        LOG_E("OOM");
        http.end();
        return;
    }

    size_t written = 0;
    unsigned long start = millis();
    unsigned long lastRx = millis();
    
    // Loop until:
    // a) Connection closes (http.connected() == false && stream empty)
    // b) We received expected Content-Length
    // c) Partition is full
    // d) Timeout
    while (written < partition->size) {
        // Timeouts
        if (millis() - start > 180000) { LOG_E("Timeout: >3min"); break; } // Increased to 3min for 8MB
        if (millis() - lastRx > 15000) { LOG_E("Stall: 15s"); break; }
        
        size_t available = stream->available();
        if (available > 0) {
            size_t toRead = std::min<size_t>(available, (size_t)BUFFER_SIZE);
            int len = stream->readBytes(buffer.get(), toRead);
            if (len > 0) {
                if (esp_partition_write(partition, written, buffer.get(), len) != ESP_OK) {
                    LOG_E("Write failed");
                    break;
                }
                written += len;
                lastRx = millis();
                if (written % 10240 == 0) feedWatchdog();
                
                // If we hit Content-Length exactly, we are done
                if (contentLength > 0 && written >= contentLength) {
                    LOG_I("Download complete (size match)");
                    break;
                }
            }
        } else {
            // No data available
            if (!http.connected()) {
                LOG_I("Connection closed by server. Download done.");
                break;
            }
            delay(10);
        }
    }
    
    http.end();
    LOG_I("LittleFS update complete: %u bytes", written);
    
    // Reboot to apply
    delay(500);
    ESP.restart();
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
    
    // Wait for Pico to stabilize and verify connection
    // The Pico may disconnect briefly during boot, so we wait and check for reconnection
    broadcastOtaProgress(&_ws, "flash", 58, "Verifying internal controller...");
    bool picoOk = false;
    for (int i = 0; i < 200; i++) {  // Wait up to 20 seconds for Pico to reconnect
        delay(100);
        feedWatchdog();
        _picoUart.loop();  // Process any incoming packets
        
        // Check if Pico is connected (may disconnect briefly during boot)
        if (_picoUart.isConnected()) {
            picoOk = true;
            // Wait a bit more to ensure connection is stable
            if (i >= 10) {  // At least 1 second of stable connection
                break;
            }
        }
    }
    
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
