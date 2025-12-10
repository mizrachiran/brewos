#include "web_server.h"
#include "config.h"
#include "pico_uart.h"
#include "state/state_manager.h"
#include <LittleFS.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_heap_caps.h>
#include <esp_partition.h>
#include <ArduinoJson.h>

// =============================================================================
// GitHub OTA - Download and install ESP32 firmware from GitHub releases
// =============================================================================

// Helper to broadcast OTA progress - uses stack allocation
static void broadcastOtaProgress(AsyncWebSocket* ws, const char* stage, int progress, const char* message) {
    if (!ws) return;
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
        free(jsonBuffer);
    }
}

void WebServer::startGitHubOTA(const String& version) {
    LOG_I("Starting ESP32 GitHub OTA for version: %s", version.c_str());
    
    // Define macro for consistent OTA progress broadcasting
    #define broadcastProgress(stage, progress, message) broadcastOtaProgress(&_ws, stage, progress, message)
    
    broadcastProgress("flash", 65, "Completing update...");
    
    // Build the GitHub release download URL
    // Build download URL using stack buffer to avoid PSRAM
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
    LOG_I("Download URL: %s", downloadUrl);
    
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(30000);
    
    if (!http.begin(downloadUrl)) {
        LOG_E("Failed to connect to GitHub");
        broadcastLog("Update error: Cannot connect to server", "error");
        broadcastOtaProgress(&_ws, "error", 0, "Connection failed");
        return;
    }
    
    http.addHeader("User-Agent", "BrewOS-ESP32/" ESP32_VERSION);
    
    int httpCode = http.GET();
    
    if (httpCode != HTTP_CODE_OK) {
        LOG_E("HTTP error: %d", httpCode);
        const char* errorMsg = (httpCode == 404) ? "Update not found" : "Download failed";
        broadcastLog("Update error: %s", "error", errorMsg);
        broadcastOtaProgress(&_ws, "error", 0, errorMsg);
        http.end();
        return;
    }
    
    int contentLength = http.getSize();
    if (contentLength <= 0) {
        LOG_E("Invalid content length: %d", contentLength);
        broadcastLog("Update error: Invalid firmware", "error");
        broadcastOtaProgress(&_ws, "error", 0, "Invalid firmware");
        http.end();
        return;
    }
    
    LOG_I("ESP32 firmware size: %d bytes", contentLength);
    broadcastOtaProgress(&_ws, "download", 70, "Downloading...");
    
    // Begin OTA update
    if (!Update.begin(contentLength)) {
        LOG_E("Not enough space for OTA");
        broadcastLog("Update error: Not enough space", "error");
        broadcastOtaProgress(&_ws, "error", 0, "Not enough space");
        http.end();
        return;
    }
    
    // Stream the firmware to Update
    WiFiClient* stream = http.getStreamPtr();
    uint8_t buffer[1024];
    size_t written = 0;
    int lastProgress = 0;
    
    while (http.connected() && written < (size_t)contentLength) {
        size_t available = stream->available();
        if (available > 0) {
            size_t toRead = min(available, sizeof(buffer));
            size_t bytesRead = stream->readBytes(buffer, toRead);
            
            if (bytesRead > 0) {
                size_t bytesWritten = Update.write(buffer, bytesRead);
                if (bytesWritten != bytesRead) {
                    LOG_E("Write error at offset %d", written);
                    broadcastLog("Update error: Write failed", "error");
                    broadcastProgress("error", 0, "Write failed");
                    Update.abort();
                    http.end();
                    return;
                }
                written += bytesWritten;
                
                // Report progress 70-95%
                int progress = 70 + (written * 25) / contentLength;
                if (progress >= lastProgress + 5) {
                    lastProgress = progress;
                    LOG_I("OTA progress: %d%% (%d/%d)", progress, written, contentLength);
                    broadcastProgress("download", progress, "Installing...");
                }
            }
        }
        delay(1);  // Yield to other tasks
    }
    
    http.end();
    
    if (written != (size_t)contentLength) {
        LOG_E("Download incomplete: %d/%d bytes", written, contentLength);
        broadcastLog("Update error: Download incomplete", "error");
        broadcastProgress("error", 0, "Download incomplete");
        Update.abort();
        return;
    }
    
    broadcastProgress("flash", 95, "Finalizing firmware...");
    
    if (!Update.end(true)) {
        LOG_E("Update failed: %s", Update.errorString());
        broadcastLog("Update error: Installation failed", "error");
        broadcastProgress("error", 0, "Installation failed");
        return;
    }
    
    LOG_I("Firmware update successful! Now updating filesystem...");
    broadcastProgress("flash", 96, "Updating web UI...");
    
    // Step 2: Download and flash LittleFS filesystem image
    char littlefsUrl[256];
    snprintf(littlefsUrl, sizeof(littlefsUrl), 
             "https://github.com/" GITHUB_OWNER "/" GITHUB_REPO "/releases/download/%s/" GITHUB_ESP32_LITTLEFS_ASSET, 
             tag);
    LOG_I("LittleFS download URL: %s", littlefsUrl);
    
    HTTPClient httpFs;
    httpFs.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    httpFs.setTimeout(30000);
    
    if (!httpFs.begin(littlefsUrl)) {
        LOG_W("Failed to connect for LittleFS download - continuing with firmware update only");
        broadcastLog("Firmware updated, but web UI update failed", "warn");
        broadcastProgress("complete", 100, "Firmware updated");
        delay(1000);
        ESP.restart();
        return;
    }
    
    httpFs.addHeader("User-Agent", "BrewOS-ESP32/" ESP32_VERSION);
    int httpCodeFs = httpFs.GET();
    
    if (httpCodeFs != HTTP_CODE_OK) {
        LOG_W("LittleFS download failed: %d - continuing with firmware update only", httpCodeFs);
        broadcastLog("Firmware updated, but web UI update failed", "warn");
        httpFs.end();
        broadcastProgress("complete", 100, "Firmware updated");
        delay(1000);
        ESP.restart();
        return;
    }
    
    int fsContentLength = httpFs.getSize();
    if (fsContentLength <= 0) {
        LOG_W("Invalid LittleFS content length: %d", fsContentLength);
        broadcastLog("Firmware updated, but web UI update failed", "warn");
        httpFs.end();
        broadcastProgress("complete", 100, "Firmware updated");
        delay(1000);
        ESP.restart();
        return;
    }
    
    LOG_I("LittleFS image size: %d bytes", fsContentLength);
    broadcastProgress("flash", 97, "Installing web UI...");
    
    // Find the LittleFS partition
    // Store partition info we need (address and size) since iterator pointers may not persist
    uint32_t fsPartitionAddr = 0;
    size_t fsPartitionSize = 0;
    bool foundPartition = false;
    
    // Constants for partition detection
    constexpr uint32_t DEFAULT_LITTLEFS_ADDRESS = 0x670000;  // Default address for 8MB partition layout
    constexpr size_t MIN_PARTITION_SIZE = 0x100000;          // 1MB minimum size for filesystem partition
    constexpr uint32_t MIN_PARTITION_ADDRESS = 0x600000;     // 6MB - filesystem typically starts after app partitions
    
    // Method 1: Find by label "littlefs" (most reliable)
    const esp_partition_t* partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "littlefs");
    if (partition) {
        fsPartitionAddr = partition->address;
        fsPartitionSize = partition->size;
        foundPartition = true;
        LOG_I("Found LittleFS partition by label at 0x%08X, size: %d", fsPartitionAddr, fsPartitionSize);
    }
    
    // Method 2: Find by expected address (fallback for partition layouts without label)
    // Note: This is specific to default_8MB.csv partition layout. If partition layout changes,
    // this fallback may need adjustment or removal.
    esp_partition_iterator_t addressIt = NULL;
    if (!foundPartition) {
        addressIt = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NULL);
        while (addressIt != NULL) {
            const esp_partition_t* p = esp_partition_get(addressIt);
            if (p->address == DEFAULT_LITTLEFS_ADDRESS) {
                fsPartitionAddr = p->address;
                fsPartitionSize = p->size;
                foundPartition = true;
                LOG_I("Found filesystem partition by address at 0x%08X, size: %d", fsPartitionAddr, fsPartitionSize);
                break;
            }
            addressIt = esp_partition_next(addressIt);
        }
        if (addressIt) {
            esp_partition_iterator_release(addressIt);
            addressIt = NULL;
        }
    }
    
    // Method 3: Find largest data partition (excluding nvs, otadata, etc.)
    // This is a last-resort fallback that looks for large data partitions after the app region
    esp_partition_iterator_t sizeIt = NULL;
    if (!foundPartition) {
        sizeIt = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NULL);
        size_t largestSize = 0;
        while (sizeIt != NULL) {
            const esp_partition_t* p = esp_partition_get(sizeIt);
            // Skip small system partitions, look for large data partition after app region
            if (p->size > MIN_PARTITION_SIZE && p->address >= MIN_PARTITION_ADDRESS) {
                if (p->size > largestSize) {
                    fsPartitionAddr = p->address;
                    fsPartitionSize = p->size;
                    largestSize = p->size;
                    foundPartition = true;
                }
            }
            sizeIt = esp_partition_next(sizeIt);
        }
        if (sizeIt) {
            esp_partition_iterator_release(sizeIt);
            sizeIt = NULL;
        }
        if (foundPartition) {
            LOG_I("Found filesystem partition by size at 0x%08X, size: %d", fsPartitionAddr, fsPartitionSize);
        }
    }
    
    if (!foundPartition || fsPartitionAddr == 0 || fsPartitionSize == 0) {
        LOG_E("LittleFS partition not found");
        broadcastLog("Firmware updated, but web UI update failed (partition not found)", "warn");
        httpFs.end();
        broadcastProgress("complete", 100, "Firmware updated");
        delay(1000);
        ESP.restart();
        return;
    }
    
    // Re-find partition by address for writing (need stable pointer for esp_partition_write)
    esp_partition_iterator_t writeIt = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NULL);
    partition = NULL;
    while (writeIt != NULL) {
        const esp_partition_t* p = esp_partition_get(writeIt);
        if (p->address == fsPartitionAddr) {
            partition = p;  // This pointer is valid while iterator exists
            break;
        }
        writeIt = esp_partition_next(writeIt);
    }
    
    if (!partition || partition->address != fsPartitionAddr) {
        LOG_E("Failed to get partition handle for writing");
        broadcastLog("Firmware updated, but web UI update failed (partition handle error)", "warn");
        if (writeIt) {
            esp_partition_iterator_release(writeIt);
        }
        httpFs.end();
        broadcastProgress("complete", 100, "Firmware updated");
        delay(1000);
        ESP.restart();
        return;
    }
    
    // Erase the partition first
    broadcastProgress("flash", 98, "Erasing filesystem...");
    if (esp_partition_erase_range(partition, 0, fsPartitionSize) != ESP_OK) {
        LOG_E("Failed to erase LittleFS partition");
        broadcastLog("Firmware updated, but web UI update failed (erase failed)", "warn");
        if (writeIt) {
            esp_partition_iterator_release(writeIt);
        }
        httpFs.end();
        broadcastProgress("complete", 100, "Firmware updated");
        delay(1000);
        ESP.restart();
        return;
    }
    
    // Download and write to partition
    WiFiClient* streamFs = httpFs.getStreamPtr();
    uint8_t bufferFs[1024];
    size_t writtenFs = 0;
    size_t offset = 0;
    
    while (httpFs.connected() && writtenFs < (size_t)fsContentLength && offset < fsPartitionSize) {
        size_t available = streamFs->available();
        if (available > 0) {
            size_t toRead = min(available, sizeof(bufferFs));
            size_t bytesRead = streamFs->readBytes(bufferFs, toRead);
            
            if (bytesRead > 0) {
                // Write to partition
                esp_err_t err = esp_partition_write(partition, offset, bufferFs, bytesRead);
                if (err != ESP_OK) {
                    LOG_E("Failed to write to LittleFS partition at offset %d: %s", offset, esp_err_to_name(err));
                    broadcastLog("Firmware updated, but web UI update failed (write failed)", "warn");
                    if (writeIt) {
                        esp_partition_iterator_release(writeIt);
                    }
                    httpFs.end();
                    broadcastProgress("complete", 100, "Firmware updated");
                    delay(1000);
                    ESP.restart();
                    return;
                }
                writtenFs += bytesRead;
                offset += bytesRead;
                
                // Report progress 97-99%
                int progress = 97 + (writtenFs * 2) / fsContentLength;
                if (progress > 99) progress = 99;
                broadcastProgress("flash", progress, "Installing web UI...");
            }
        }
        delay(1);  // Yield to other tasks
    }
    
    httpFs.end();
    
    if (writtenFs != (size_t)fsContentLength) {
        LOG_W("LittleFS download incomplete: %d/%d bytes", writtenFs, fsContentLength);
        broadcastLog("Firmware updated, but web UI update incomplete", "warn");
        if (writeIt) {
            esp_partition_iterator_release(writeIt);
        }
        broadcastProgress("complete", 100, "Firmware updated");
        delay(1000);
        ESP.restart();
        return;
    }
    
    LOG_I("LittleFS update successful! Total written: %d bytes", writtenFs);
    broadcastProgress("complete", 100, "Update complete!");
    
    // Release partition iterator
    if (writeIt) {
        esp_partition_iterator_release(writeIt);
    }
    
    LOG_I("OTA update successful (firmware + filesystem)!");
    broadcastLog("BrewOS updated successfully! Restarting...", "info");
    
    // Give time for the message to be sent
    delay(1000);
    
    // Restart to apply the update
    ESP.restart();
    
    #undef broadcastProgress
}

// =============================================================================
// OTA Update Check - Query GitHub for available updates
// =============================================================================

/**
 * Compare semantic version strings (e.g., "0.4.4" vs "0.4.5")
 * Returns: -1 if v1 < v2, 0 if equal, 1 if v1 > v2
 */
static int compareVersions(const String& v1, const String& v2) {
    int major1 = 0, minor1 = 0, patch1 = 0;
    int major2 = 0, minor2 = 0, patch2 = 0;
    
    // Parse v1 (skip leading 'v' if present)
    String ver1 = v1.startsWith("v") ? v1.substring(1) : v1;
    sscanf(ver1.c_str(), "%d.%d.%d", &major1, &minor1, &patch1);
    
    // Parse v2
    String ver2 = v2.startsWith("v") ? v2.substring(1) : v2;
    sscanf(ver2.c_str(), "%d.%d.%d", &major2, &minor2, &patch2);
    
    if (major1 != major2) return major1 < major2 ? -1 : 1;
    if (minor1 != minor2) return minor1 < minor2 ? -1 : 1;
    if (patch1 != patch2) return patch1 < patch2 ? -1 : 1;
    return 0;
}

void WebServer::checkForUpdates() {
    LOG_I("Checking for updates...");
    broadcastLog("Checking for updates...", "info");
    
    // Query GitHub API for latest release
    // API endpoint: https://api.github.com/repos/OWNER/REPO/releases/latest
    String apiUrl = "https://api.github.com/repos/" GITHUB_OWNER "/" GITHUB_REPO "/releases/latest";
    
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(10000);  // 10 second timeout
    
    if (!http.begin(apiUrl)) {
        LOG_E("Failed to connect to GitHub API");
        broadcastLog("Update check failed: Cannot connect to GitHub", "error");
        return;
    }
    
    // GitHub API requires User-Agent and Accept headers
    http.addHeader("User-Agent", "BrewOS-ESP32/" ESP32_VERSION);
    http.addHeader("Accept", "application/vnd.github.v3+json");
    
    int httpCode = http.GET();
    
    if (httpCode != HTTP_CODE_OK) {
        LOG_E("GitHub API error: %d", httpCode);
        broadcastLog("Update check failed: HTTP %d", "error", httpCode);
        http.end();
        return;
    }
    
    // Parse JSON response
    String payload = http.getString();
    http.end();
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        LOG_E("JSON parse error: %s", error.c_str());
        broadcastLog("Update check failed: Invalid response", "error");
        return;
    }
    
    // Extract release information
    String latestVersion = doc["tag_name"] | "";
    String releaseName = doc["name"] | "";
    String releaseBody = doc["body"] | "";
    bool prerelease = doc["prerelease"] | false;
    String publishedAt = doc["published_at"] | "";
    
    if (latestVersion.isEmpty()) {
        LOG_E("No version found in release");
        broadcastLog("Update check failed: No version found", "error");
        return;
    }
    
    // Strip 'v' prefix for comparison
    String latestVersionNum = latestVersion.startsWith("v") ? latestVersion.substring(1) : latestVersion;
    String currentVersion = ESP32_VERSION;
    
    LOG_I("Current: %s, Latest: %s", currentVersion.c_str(), latestVersionNum.c_str());
    
    // Compare versions
    int cmp = compareVersions(currentVersion, latestVersionNum);
    bool updateAvailable = (cmp < 0);
    
    // Check for both ESP32 and Pico assets
    int esp32AssetSize = 0;
    int picoAssetSize = 0;
    bool esp32AssetFound = false;
    bool picoAssetFound = false;
    
    uint8_t machineType = State.getMachineType();
    const char* picoAssetName = getPicoAssetName(machineType);
    
    JsonArray assets = doc["assets"].as<JsonArray>();
    for (JsonObject asset : assets) {
        String assetName = asset["name"] | "";
        if (assetName == GITHUB_ESP32_ASSET) {
            esp32AssetSize = asset["size"] | 0;
            esp32AssetFound = true;
        }
        if (picoAssetName && assetName == picoAssetName) {
            picoAssetSize = asset["size"] | 0;
            picoAssetFound = true;
        }
    }
    
    // Determine if combined update is available
    // Both assets must exist for a proper combined update
    bool combinedUpdateAvailable = updateAvailable && esp32AssetFound && picoAssetFound;
    
    // Broadcast result to WebSocket clients
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
    
    // Truncate changelog if too long
    if (releaseBody.length() > 500) {
        releaseBody = releaseBody.substring(0, 497) + "...";
    }
    result["changelog"] = releaseBody;
    
    String response;
    serializeJson(result, response);
    _ws.textAll(response);
    
    // Log result
    if (updateAvailable && combinedUpdateAvailable) {
        broadcastLog("BrewOS %s available (current: %s)", latestVersionNum.c_str(), currentVersion.c_str());
    } else if (updateAvailable) {
        // Update available but missing some assets - log internally only
        LOG_W("Update available but missing assets: esp32=%d, pico=%d", esp32AssetFound, picoAssetFound);
        broadcastLog("BrewOS %s available (current: %s)", latestVersionNum.c_str(), currentVersion.c_str());
    } else {
        broadcastLog("BrewOS is up to date (%s)", currentVersion.c_str());
    }
}

// =============================================================================
// Pico GitHub OTA - Download and install Pico firmware from GitHub releases
// =============================================================================

const char* WebServer::getPicoAssetName(uint8_t machineType) {
    switch (machineType) {
        case 1: return GITHUB_PICO_DUAL_BOILER_ASSET;
        case 2: return GITHUB_PICO_SINGLE_BOILER_ASSET;
        case 3: return GITHUB_PICO_HEAT_EXCHANGER_ASSET;
        default: return nullptr;
    }
}

void WebServer::startPicoGitHubOTA(const String& version) {
    LOG_I("Starting Pico GitHub OTA for version: %s", version.c_str());
    
    // Get machine type from StateManager
    uint8_t machineType = State.getMachineType();
    const char* picoAsset = getPicoAssetName(machineType);
    
    if (!picoAsset) {
        LOG_E("Unknown machine type: %d - cannot determine Pico firmware", machineType);
        broadcastLog("Update error: Device not ready", "error");
        return;
    }
    
    LOG_I("Pico asset: %s", picoAsset);
    
    // Build the GitHub release download URL using stack buffer to avoid PSRAM
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
    
    // Use static helper for OTA progress (broadcastOtaProgress defined above)
    #define broadcastProgress(stage, progress, message) broadcastOtaProgress(&_ws, stage, progress, message)
    
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(60000);  // 60 second timeout for larger Pico firmware
    
    if (!http.begin(downloadUrl)) {
        LOG_E("Failed to connect to GitHub");
        broadcastLog("Update error: Cannot connect to server", "error");
        broadcastProgress("error", 0, "Connection failed");
        return;
    }
    
    http.addHeader("User-Agent", "BrewOS-ESP32/" ESP32_VERSION);
    
    int httpCode = http.GET();
    
    if (httpCode != HTTP_CODE_OK) {
        LOG_E("HTTP error: %d", httpCode);
        const char* errorMsg = (httpCode == 404) ? "Update not found for this version" : "Download failed";
        broadcastLog("Update error: %s", "error", errorMsg);
        broadcastProgress("error", 0, errorMsg);
        http.end();
        return;
    }
    
    int contentLength = http.getSize();
    if (contentLength <= 0 || contentLength > OTA_MAX_SIZE) {
        LOG_E("Invalid content length: %d", contentLength);
        broadcastLog("Update error: Invalid firmware", "error");
        broadcastProgress("error", 0, "Invalid firmware");
        http.end();
        return;
    }
    
    LOG_I("Pico firmware size: %d bytes", contentLength);
    broadcastProgress("download", 10, "Downloading...");
    
    // Save to LittleFS first
    File firmwareFile = LittleFS.open(OTA_FILE_PATH, "w");
    if (!firmwareFile) {
        LOG_E("Failed to create firmware file");
        broadcastLog("Update error: Storage full", "error");
        broadcastProgress("error", 0, "Storage full");
        http.end();
        return;
    }
    
    // Stream to file
    WiFiClient* stream = http.getStreamPtr();
    uint8_t buffer[1024];
    size_t written = 0;
    int lastProgress = 0;
    
    while (http.connected() && written < (size_t)contentLength) {
        size_t available = stream->available();
        if (available > 0) {
            size_t toRead = min(available, sizeof(buffer));
            size_t bytesRead = stream->readBytes(buffer, toRead);
            
            if (bytesRead > 0) {
                size_t bytesWritten = firmwareFile.write(buffer, bytesRead);
                if (bytesWritten != bytesRead) {
                    LOG_E("File write error");
                    broadcastLog("Update error: Write failed", "error");
                    broadcastProgress("error", 0, "Write failed");
                    firmwareFile.close();
                    http.end();
                    return;
                }
                written += bytesWritten;
                
                // Report progress every 5%
                int progress = 10 + (written * 30) / contentLength;  // 10-40% for download
                if (progress >= lastProgress + 5) {
                    lastProgress = progress;
                    broadcastProgress("download", progress, "Downloading...");
                }
            }
        }
        delay(1);
    }
    
    firmwareFile.close();
    http.end();
    
    if (written != (size_t)contentLength) {
        LOG_E("Download incomplete: %d/%d bytes", written, contentLength);
        broadcastLog("Update error: Download incomplete", "error");
        broadcastProgress("error", 0, "Download incomplete");
        return;
    }
    
    broadcastProgress("flash", 40, "Installing...");
    
    // Now flash to Pico (reuse existing logic)
    File flashFile = LittleFS.open(OTA_FILE_PATH, "r");
    if (!flashFile) {
        LOG_E("Failed to open firmware file for flashing");
        broadcastLog("Update error: Cannot read firmware", "error");
        broadcastProgress("error", 0, "Cannot read firmware");
        return;
    }
    
    size_t firmwareSize = flashFile.size();
    
    // Send bootloader command
    broadcastProgress("flash", 42, "Preparing device...");
    if (!_picoUart.sendCommand(MSG_CMD_BOOTLOADER, nullptr, 0)) {
        LOG_E("Failed to send bootloader command");
        broadcastLog("Update error: Device not responding", "error");
        broadcastProgress("error", 0, "Device not responding");
        flashFile.close();
        return;
    }
    
    // Wait for bootloader ACK
    if (!_picoUart.waitForBootloaderAck(3000)) {
        LOG_E("Bootloader ACK timeout");
        broadcastLog("Update error: Device not ready", "error");
        broadcastProgress("error", 0, "Device not ready");
        flashFile.close();
        return;
    }
    
    broadcastProgress("flash", 45, "Installing...");
    
    // Stream to Pico
    bool success = streamFirmwareToPico(flashFile, firmwareSize);
    flashFile.close();
    
    if (!success) {
        LOG_E("Pico firmware streaming failed");
        broadcastLog("Update error: Installation failed", "error");
        broadcastProgress("error", 0, "Installation failed");
        return;
    }
    
    // Reset Pico
    broadcastProgress("flash", 55, "Restarting...");
    delay(1000);
    _picoUart.resetPico();
    
    // Wait for Pico to boot and send MSG_BOOT
    delay(3000);  // Give Pico time to boot
    
    LOG_I("Pico OTA complete!");
    
    #undef broadcastProgress
}

// =============================================================================
// Combined OTA - Update Pico first, then ESP32
// =============================================================================

void WebServer::startCombinedOTA(const String& version) {
    LOG_I("Starting combined OTA for version: %s", version.c_str());
    broadcastLog("Starting BrewOS update to v%s...", version.c_str());
    
    // Define macro for consistent OTA progress broadcasting
    #define broadcastProgress(stage, progress, message) broadcastOtaProgress(&_ws, stage, progress, message)
    // Note: broadcastProgress macro already defined above for startPicoGitHubOTA
    
    // Check machine type is known
    uint8_t machineType = State.getMachineType();
    if (machineType == 0) {
        LOG_E("Machine type unknown - Pico not connected?");
        broadcastLog("Update error: Please ensure the machine is powered on and connected.", "error");
        broadcastProgress("error", 0, "Device not ready");
        return;
    }
    
    broadcastProgress("download", 0, "Preparing update...");
    
    // Step 1: Update Pico (internal module)
    LOG_I("Step 1/2: Updating Pico firmware...");
    startPicoGitHubOTA(version);
    
    // Check if Pico reconnected (crude check - wait and verify)
    delay(5000);  // Wait for internal module to fully boot
    
    if (!_picoUart.isConnected()) {
        LOG_W("Pico not responding after update - continuing with ESP32 update anyway");
        broadcastLog("Finalizing update...", "info");
    } else {
        // Verify Pico version matches
        const char* picoVer = State.getPicoVersion();
        if (picoVer && String(picoVer) != version && String(picoVer) != version.substring(1)) {
            LOG_W("Pico version mismatch after update: %s vs %s", picoVer, version.c_str());
        }
        broadcastLog("Finalizing update...", "info");
    }
    
    broadcastProgress("flash", 60, "Completing update...");
    
    #undef broadcastProgress
    
    // Step 2: Update ESP32 (this will reboot)
    LOG_I("Step 2/2: Updating ESP32 firmware...");
    startGitHubOTA(version);
    
    // Note: startGitHubOTA reboots ESP32, so we won't reach here
}

// =============================================================================
// Version Mismatch Detection
// =============================================================================

bool WebServer::checkVersionMismatch() {
    const char* picoVersion = State.getPicoVersion();
    const char* esp32Version = ESP32_VERSION;
    
    // If Pico version not available, can't check
    if (!picoVersion || picoVersion[0] == '\0') {
        return false;  // No mismatch detected (not fully connected yet)
    }
    
    // Compare versions using stack buffers (strip 'v' prefix if present)
    char picoVer[16];
    char esp32Ver[16];
    strncpy(picoVer, picoVersion[0] == 'v' ? picoVersion + 1 : picoVersion, sizeof(picoVer) - 1);
    picoVer[sizeof(picoVer) - 1] = '\0';
    strncpy(esp32Ver, esp32Version[0] == 'v' ? esp32Version + 1 : esp32Version, sizeof(esp32Ver) - 1);
    esp32Ver[sizeof(esp32Ver) - 1] = '\0';
    
    bool mismatch = (strcmp(picoVer, esp32Ver) != 0);
    
    if (mismatch) {
        LOG_W("Internal version mismatch: %s vs %s", esp32Ver, picoVer);
        
        // Broadcast to clients using stack allocation
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        StaticJsonDocument<256> doc;
        #pragma GCC diagnostic pop
        doc["type"] = "version_mismatch";
        doc["currentVersion"] = esp32Ver;
        doc["message"] = "A firmware update is recommended to ensure optimal performance.";
        
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


