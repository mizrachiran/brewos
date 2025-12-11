#include "web_server.h"
#include "config.h"
#include "pico_uart.h"
#include "cloud_connection.h"
#include "mqtt_client.h"
#include "scale/scale_manager.h"
#include "power_meter/power_meter_manager.h"
#include "notifications/notification_manager.h"
#include "state/state_manager.h"
#include <LittleFS.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_heap_caps.h>
#include <esp_partition.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ArduinoJson.h>

// External references to global service instances (for pausing during OTA)
extern MQTTClient* mqttClient;
extern ScaleManager* scaleManager;
extern PowerMeterManager* powerMeterManager;
extern NotificationManager* notificationManager;

// =============================================================================
// Forward Declarations
// =============================================================================

static void disableWatchdogForOTA();
static void enableWatchdogAfterOTA();
static inline void feedWatchdog();
static void broadcastOtaProgress(AsyncWebSocket* ws, const char* stage, int progress, const char* message);
static void handleOTAFailure(AsyncWebSocket* ws);

// =============================================================================
// OTA Service Control - Pause/Resume background services during update
// =============================================================================

/**
 * Pause all background services before OTA
 * This prevents network/BLE interference and SSL crashes during update
 * 
 * Services paused:
 * - CloudConnection: SSL WebSocket to cloud server
 * - MQTTClient: MQTT broker connection
 * - ScaleManager: BLE scanning/connection (interferes with WiFi)
 * - PowerMeterManager: HTTP polling to Shelly/Tasmota
 * - NotificationManager: Push notifications to cloud
 * 
 * Services NOT paused (needed for OTA):
 * - WiFiManager: Network connectivity
 * - WebServer: Serves OTA UI and handles update
 * - PicoUART: Communication with Pico for Pico OTA
 */
static void pauseServicesForOTA(CloudConnection* cloudConnection) {
    LOG_I("Pausing services for OTA...");
    
    // 0. Disable watchdog - OTA has long-blocking operations
    disableWatchdogForOTA();
    
    // 1. Pause cloud connection (SSL WebSocket)
    if (cloudConnection) {
        LOG_I("  - Pausing cloud connection...");
        cloudConnection->setEnabled(false);
    }
    
    // 2. Pause MQTT client
    if (mqttClient) {
        LOG_I("  - Pausing MQTT...");
        mqttClient->setEnabled(false);
    }
    
    // 3. Stop BLE scale (can interfere with WiFi)
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
    
    // Give all services time to cleanly shut down
    for (int i = 0; i < 5; i++) {
        delay(100);
        yield();
    }
    
    LOG_I("All services paused for OTA");
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
constexpr unsigned long OTA_DOWNLOAD_TIMEOUT_MS = 120000;  // 2 minutes per download
constexpr unsigned long OTA_HTTP_TIMEOUT_MS = 30000;       // 30 seconds HTTP timeout
constexpr unsigned long OTA_WATCHDOG_FEED_INTERVAL_MS = 50;// Feed watchdog every 50ms

// Buffer sizes
constexpr size_t OTA_BUFFER_SIZE = 512;                    // Smaller buffer for more responsive yields

// Retry configuration
constexpr int OTA_MAX_RETRIES = 3;
constexpr unsigned long OTA_RETRY_DELAY_MS = 3000;

// =============================================================================
// OTA Helper Functions
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

// Track if watchdog is disabled (to avoid reset errors)
static bool _watchdogDisabled = false;

/**
 * Feed the watchdog and yield to other tasks
 * Call this frequently during long operations
 */
static inline void feedWatchdog() {
    yield();
    // Reset task watchdog only if we haven't disabled it
    if (!_watchdogDisabled) {
        esp_task_wdt_reset();
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
    
    // Last resort: try to reinit with longer timeout (this usually fails if already init)
    // But we try anyway in case the state is inconsistent
    err = esp_task_wdt_init(60, false);  // 60 second timeout, no panic
    if (err == ESP_OK) {
        LOG_I("WDT reinitialized with 60 second timeout");
    } else if (err == ESP_ERR_INVALID_STATE) {
        // Already initialized - this is expected. The WDT is still active with
        // its original timeout. We've removed loopTask, so it won't trigger for us.
        // The async_tcp might still trigger, but we've done what we can.
        LOG_W("WDT already initialized - async_tcp may still trigger WDT during long downloads");
    } else {
        LOG_W("WDT init returned: %d", err);
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
        // WDT not initialized - try to init with default settings
        err = esp_task_wdt_init(5, true);  // 5 second timeout, panic on trigger
        if (err == ESP_OK) {
            LOG_I("WDT reinitialized with default config");
            esp_task_wdt_add(NULL);
        }
    } else {
        // Don't worry about errors - device will restart anyway
        LOG_D("WDT add returned: %d (device will restart)", err);
    }
}

/**
 * Broadcast OTA progress - uses stack allocation to avoid PSRAM issues
 */
static void broadcastOtaProgress(AsyncWebSocket* ws, const char* stage, int progress, const char* message) {
    if (!ws) {
        LOG_W("OTA progress: ws is null!");
        return;
    }
    
    feedWatchdog();  // Don't let broadcast block watchdog
    
    size_t clientCount = ws->count();
    LOG_I("OTA broadcast: stage=%s, progress=%d, clients=%zu", stage, progress, clientCount);
    
    if (clientCount == 0) {
        LOG_W("No WebSocket clients connected for OTA progress");
        return;
    }
    
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    StaticJsonDocument<256> doc;
    #pragma GCC diagnostic pop
    doc["type"] = "ota_progress";
    doc["stage"] = stage;
    doc["progress"] = progress;
    doc["message"] = message;
    
    size_t jsonSize = measureJson(doc) + 1;
    char* jsonBuffer = (char*)heap_caps_malloc(jsonSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!jsonBuffer) jsonBuffer = (char*)malloc(jsonSize);
    if (jsonBuffer) {
        serializeJson(doc, jsonBuffer, jsonSize);
        ws->textAll(jsonBuffer);
        LOG_D("OTA progress sent: %s", jsonBuffer);
        free(jsonBuffer);
    } else {
        LOG_E("Failed to allocate buffer for OTA progress");
    }
    
    feedWatchdog();
}

/**
 * Download a file from URL to LittleFS with proper error handling
 * Returns true on success, false on failure
 */
static bool downloadToFile(const char* url, const char* filePath, 
                           AsyncWebSocket* ws, int progressStart, int progressEnd,
                           size_t* outFileSize = nullptr) {
    LOG_I("Downloading: %s", url);
    
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(OTA_HTTP_TIMEOUT_MS);
    
    int httpCode = 0;
    unsigned long downloadStart = millis();
    
    // Retry loop for transient errors
    for (int retry = 0; retry < OTA_MAX_RETRIES; retry++) {
        feedWatchdog();
        
        if (!http.begin(url)) {
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
    uint8_t buffer[OTA_BUFFER_SIZE];
    size_t written = 0;
    int lastProgress = progressStart;
    unsigned long lastYield = millis();
    unsigned long lastProgressUpdate = millis();
    
    unsigned long lastDataReceived = millis();
    unsigned long lastConsoleLog = millis();
    constexpr unsigned long STALL_TIMEOUT_MS = 30000;
    
    while (http.connected() && written < (size_t)contentLength) {
        // Check for overall timeout
        if (millis() - downloadStart > OTA_DOWNLOAD_TIMEOUT_MS) {
            LOG_E("Download timeout after %lu ms (wrote %d/%d)", millis() - downloadStart, written, contentLength);
            file.close();
            LittleFS.remove(filePath);
            http.end();
            return false;
        }
        
        // Check for stall
        if (millis() - lastDataReceived > STALL_TIMEOUT_MS) {
            LOG_E("Download stalled - no data for %lu ms (wrote %d/%d)", STALL_TIMEOUT_MS, written, contentLength);
            file.close();
            LittleFS.remove(filePath);
            http.end();
            return false;
        }
        
        // Feed watchdog frequently
        if (millis() - lastYield >= OTA_WATCHDOG_FEED_INTERVAL_MS) {
            feedWatchdog();
            lastYield = millis();
        }
        
        size_t available = stream->available();
        if (available > 0) {
            lastDataReceived = millis();
            size_t toRead = min(available, sizeof(buffer));
            size_t bytesRead = stream->readBytes(buffer, toRead);
            
            if (bytesRead > 0) {
                size_t bytesWritten = file.write(buffer, bytesRead);
                if (bytesWritten != bytesRead) {
                    LOG_E("Write error: %d/%d bytes (filesystem full?)", bytesWritten, bytesRead);
                    file.close();
                    LittleFS.remove(filePath);
                    http.end();
                    return false;
                }
                written += bytesWritten;
                
                // Log to console every 5 seconds
                if (millis() - lastConsoleLog > 5000) {
                    int pct = (written * 100) / contentLength;
                    LOG_I("Download: %d%% (%d/%d bytes)", pct, written, contentLength);
                    lastConsoleLog = millis();
                }
                
                // Update WebSocket progress (limit frequency to avoid flooding)
                // Broadcast at most every 2 seconds to prevent WebSocket queue overflow
                if (ws && millis() - lastProgressUpdate > 2000) {
                    int progress = progressStart + (written * (progressEnd - progressStart)) / contentLength;
                    if (progress > lastProgress) {
                        lastProgress = progress;
                        broadcastOtaProgress(ws, "download", progress, "Downloading...");
                        lastProgressUpdate = millis();
                    }
                }
            }
        } else {
            delay(10);  // Reduce CPU spinning while waiting for data
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

bool WebServer::startPicoGitHubOTA(const String& version) {
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
    broadcastOtaProgress(&_ws, "download", 10, "Downloading Pico firmware...");
    
    size_t firmwareSize = 0;
    if (!downloadToFile(downloadUrl, OTA_FILE_PATH, &_ws, 10, 40, &firmwareSize)) {
        LOG_E("Pico firmware download failed");
        broadcastLogLevel("error", "Update error: Download failed");
        broadcastOtaProgress(&_ws, "error", 0, "Download failed");
        cleanupOtaFiles();
        return false;
    }
    
    // Flash to Pico
    broadcastOtaProgress(&_ws, "flash", 40, "Installing Pico firmware...");
    
    File flashFile = LittleFS.open(OTA_FILE_PATH, "r");
    if (!flashFile) {
        LOG_E("Failed to open firmware file");
        broadcastLogLevel("error", "Update error: Cannot read firmware");
        broadcastOtaProgress(&_ws, "error", 0, "Cannot read firmware");
        cleanupOtaFiles();
        return false;
    }
    
    // Send bootloader command
    broadcastOtaProgress(&_ws, "flash", 42, "Preparing device...");
    feedWatchdog();
    
    // IMPORTANT: Pause packet processing BEFORE sending bootloader command
    // This prevents the main loop's picoUart.loop() from consuming the bootloader ACK bytes
    _picoUart.pause();
    LOG_I("Paused UART packet processing for bootloader handshake");
    
    if (!_picoUart.sendCommand(MSG_CMD_BOOTLOADER, nullptr, 0)) {
        LOG_E("Failed to send bootloader command");
        broadcastLogLevel("error", "Update error: Device not responding");
        broadcastOtaProgress(&_ws, "error", 0, "Device not responding");
        _picoUart.resume();  // Resume on failure
        flashFile.close();
        cleanupOtaFiles();
        return false;
    }
    
    // Give Pico time to process command and enter bootloader mode
    // Pico sends protocol ACK, waits 50ms, then sends 0xAA 0x55
    LOG_I("Sent bootloader command, waiting for Pico to enter bootloader...");
    delay(200);
    feedWatchdog();
    
    // Wait for bootloader ACK with timeout
    feedWatchdog();
    if (!_picoUart.waitForBootloaderAck(3000)) {
        LOG_E("Bootloader ACK timeout");
        broadcastLogLevel("error", "Update error: Device not ready");
        broadcastOtaProgress(&_ws, "error", 0, "Device not ready");
        _picoUart.resume();  // Resume on failure
        flashFile.close();
        cleanupOtaFiles();
        return false;
    }
    
    broadcastOtaProgress(&_ws, "flash", 45, "Installing...");
    feedWatchdog();
    
    // Stream to Pico
    bool success = streamFirmwareToPico(flashFile, firmwareSize);
    flashFile.close();
    
    // Clean up temp file regardless of success
    cleanupOtaFiles();
    
    if (!success) {
        LOG_E("Pico firmware streaming failed");
        broadcastLogLevel("error", "Update error: Installation failed");
        broadcastOtaProgress(&_ws, "error", 0, "Installation failed");
        _picoUart.resume();  // Resume on failure
        return false;
    }
    
    // Reset Pico
    broadcastOtaProgress(&_ws, "flash", 55, "Restarting device...");
    feedWatchdog();
    delay(500);
    feedWatchdog();
    _picoUart.resetPico();
    
    // Resume packet processing BEFORE waiting for Pico to boot
    // so we can receive the boot info packets
    _picoUart.resume();
    LOG_I("Resumed UART packet processing");
    
    // Wait for Pico to boot
    LOG_I("Waiting for Pico to boot...");
    for (int i = 0; i < 30; i++) {
        delay(100);
        feedWatchdog();
    }
    
    LOG_I("Pico OTA complete!");
    return true;
}

// =============================================================================
// ESP32 OTA - Download and flash ESP32 firmware + LittleFS
// =============================================================================

void WebServer::startGitHubOTA(const String& version) {
    LOG_I("Starting ESP32 GitHub OTA for version: %s", version.c_str());
    
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
             "https://github.com/" GITHUB_OWNER "/" GITHUB_REPO "/releases/download/%s/" GITHUB_ESP32_ASSET, 
             tag);
    LOG_I("ESP32 download URL: %s", downloadUrl);
    
    broadcastOtaProgress(&_ws, "download", 65, "Downloading ESP32 firmware...");
    
    // Download ESP32 firmware
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(OTA_HTTP_TIMEOUT_MS);
    
    int httpCode = 0;
    
    for (int retry = 0; retry < OTA_MAX_RETRIES; retry++) {
        feedWatchdog();
        
        if (!http.begin(downloadUrl)) {
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
    uint8_t buffer[OTA_BUFFER_SIZE];
    size_t written = 0;
    int lastProgress = 70;
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
            lastYield = millis();
        }
        
        size_t available = stream->available();
        if (available > 0) {
            lastDataReceived = millis();  // Reset stall timer
            size_t toRead = min(available, sizeof(buffer));
            size_t bytesRead = stream->readBytes(buffer, toRead);
            
            if (bytesRead > 0) {
                size_t bytesWritten = Update.write(buffer, bytesRead);
                if (bytesWritten != bytesRead) {
                    LOG_E("Write error at %d", written);
                    Update.abort();
                    http.end();
                    broadcastLogLevel("error", "Update error: Write failed");
                    broadcastOtaProgress(&_ws, "error", 0, "Write failed");
                    return;
                }
                written += bytesWritten;
                
                // Log progress to console every 5 seconds (in case WebSocket is dead)
                if (millis() - lastProgressLog > 5000) {
                    int pct = (written * 100) / contentLength;
                    LOG_I("ESP32 OTA: %d%% (%d/%d bytes)", pct, written, contentLength);
                    lastProgressLog = millis();
                }
                
                // Progress 70-95% (only broadcast every 10% to avoid WebSocket queue overflow)
                int progress = 70 + (written * 25) / contentLength;
                if (progress >= lastProgress + 10) {  // Report every 10% to limit message frequency
                    lastProgress = progress;
                    broadcastOtaProgress(&_ws, "download", progress, "Installing...");
                }
            }
        } else {
            delay(10);  // Reduce CPU spinning while waiting for data
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
    updateLittleFS(tag);
    
    broadcastOtaProgress(&_ws, "complete", 100, "Update complete!");
    broadcastLogLevel("info", "BrewOS updated! Restarting...");
    
    // Give time for message to send
    for (int i = 0; i < 10; i++) {
        delay(100);
        feedWatchdog();
    }
    
    ESP.restart();
}

/**
 * Update LittleFS filesystem (called after ESP32 firmware update)
 * Non-critical - continues even if fails
 */
void WebServer::updateLittleFS(const char* tag) {
    LOG_I("Updating LittleFS...");
    broadcastOtaProgress(&_ws, "flash", 96, "Updating web UI...");
    
    char littlefsUrl[256];
    snprintf(littlefsUrl, sizeof(littlefsUrl), 
             "https://github.com/" GITHUB_OWNER "/" GITHUB_REPO "/releases/download/%s/" GITHUB_ESP32_LITTLEFS_ASSET, 
             tag);
    
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(OTA_HTTP_TIMEOUT_MS);
    
    feedWatchdog();
    if (!http.begin(littlefsUrl)) {
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
    
    // Find LittleFS partition
    const esp_partition_t* partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "littlefs");
    
    if (!partition) {
        LOG_W("LittleFS partition not found");
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
    uint8_t buffer[OTA_BUFFER_SIZE];
    size_t written = 0;
    size_t offset = 0;
    unsigned long lastYield = millis();
    
    while (http.connected() && written < (size_t)contentLength && offset < partition->size) {
        if (millis() - lastYield >= OTA_WATCHDOG_FEED_INTERVAL_MS) {
            feedWatchdog();
            lastYield = millis();
        }
        
        size_t available = stream->available();
        if (available > 0) {
            size_t toRead = min(available, sizeof(buffer));
            size_t bytesRead = stream->readBytes(buffer, toRead);
            
            if (bytesRead > 0) {
                if (esp_partition_write(partition, offset, buffer, bytesRead) != ESP_OK) {
                    LOG_W("LittleFS write failed at offset %d", offset);
                    break;
                }
                written += bytesRead;
                offset += bytesRead;
            }
        } else {
            delay(1);
            feedWatchdog();
        }
    }
    
    http.end();
    feedWatchdog();
    
    if (written == (size_t)contentLength) {
        LOG_I("LittleFS updated: %d bytes", written);
    } else {
        LOG_W("LittleFS incomplete: %d/%d bytes", written, contentLength);
    }
}

// =============================================================================
// Combined OTA - Update Pico first, then ESP32
// =============================================================================

void WebServer::startCombinedOTA(const String& version) {
    LOG_I("Starting combined OTA for version: %s", version.c_str());
    
    // IMMEDIATELY tell UI that OTA is starting - this triggers the overlay
    broadcastOtaProgress(&_ws, "download", 0, "Starting update...");
    broadcastLog("Starting BrewOS update to v%s...", version.c_str());
    
    unsigned long otaStart = millis();
    
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
    // This includes: cloud (SSL), MQTT, BLE scale, power meter HTTP polling
    pauseServicesForOTA(_cloudConnection);
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

void WebServer::checkForUpdates() {
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
    
    JsonArray assets = doc["assets"].as<JsonArray>();
    for (JsonObject asset : assets) {
        String name = asset["name"] | "";
        if (name == GITHUB_ESP32_ASSET) {
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

const char* WebServer::getPicoAssetName(uint8_t machineType) {
    switch (machineType) {
        case 1: return GITHUB_PICO_DUAL_BOILER_ASSET;
        case 2: return GITHUB_PICO_SINGLE_BOILER_ASSET;
        case 3: return GITHUB_PICO_HEAT_EXCHANGER_ASSET;
        default: return nullptr;
    }
}

bool WebServer::checkVersionMismatch() {
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
