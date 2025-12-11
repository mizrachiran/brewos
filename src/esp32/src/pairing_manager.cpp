#include "pairing_manager.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <mbedtls/md.h>
#include <Preferences.h>
#include <esp_random.h>

// Token validity duration (10 minutes)
static const unsigned long TOKEN_VALIDITY_MS = 10 * 60 * 1000;

// NVS namespace for device key
static const char* NVS_NAMESPACE = "brewos_sec";
static const char* NVS_KEY_DEVICE_KEY = "devKey";

PairingManager::PairingManager()
    : _cloudUrl("")
    , _deviceId("")
    , _deviceKey("")
    , _currentToken("")
    , _tokenExpiry(0)
    , _onPairingSuccess(nullptr)
{
}

void PairingManager::begin(const String& cloudUrl) {
    _cloudUrl = cloudUrl;
    initDeviceId();
    initDeviceKey();
    
    Serial.printf("[Pairing] Device ID: %s\n", _deviceId.c_str());
    Serial.printf("[Pairing] Device key initialized (length=%d)\n", _deviceKey.length());
}

void PairingManager::initDeviceId() {
    // Generate device ID from chip ID
    uint64_t chipId = ESP.getEfuseMac();
    char idBuf[16];
    snprintf(idBuf, sizeof(idBuf), "BRW-%08X", (uint32_t)(chipId >> 16));
    _deviceId = String(idBuf);
}

void PairingManager::initDeviceKey() {
    // Try to load existing key from NVS
    Preferences prefs;
    // After fresh flash, NVS namespace won't exist - this is expected
    if (!prefs.begin(NVS_NAMESPACE, true)) { // Read-only first
        Serial.println("[Pairing] No saved device key (fresh flash) - generating new one");
        // Generate new key on first boot
        _deviceKey = generateRandomToken(43); // base64url of 32 bytes ≈ 43 chars
        
        // Try to create namespace and save
        if (prefs.begin(NVS_NAMESPACE, false)) { // Read-write
            prefs.putString(NVS_KEY_DEVICE_KEY, _deviceKey);
            prefs.end();
            Serial.println("[Pairing] Generated and stored new device key");
        } else {
            Serial.println("[Pairing] Failed to save device key (NVS error)");
        }
        return;
    }
    
    String storedKey = prefs.getString(NVS_KEY_DEVICE_KEY, "");
    prefs.end();
    
    if (storedKey.length() == 43) {
        // Use existing key
        _deviceKey = storedKey;
        Serial.println("[Pairing] Loaded existing device key from NVS");
    } else {
        // Generate new key on first boot
        _deviceKey = generateRandomToken(43); // base64url of 32 bytes ≈ 43 chars
        
        // Store in NVS
        if (prefs.begin(NVS_NAMESPACE, false)) { // Read-write
            prefs.putString(NVS_KEY_DEVICE_KEY, _deviceKey);
            prefs.end();
            Serial.println("[Pairing] Generated and stored new device key");
        } else {
            Serial.println("[Pairing] Failed to save device key (NVS error)");
        }
    }
}

String PairingManager::generateRandomToken(size_t length) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    String token;
    token.reserve(length);
    
    for (size_t i = 0; i < length; i++) {
        uint32_t r = esp_random();
        token += charset[r % (sizeof(charset) - 1)];
    }
    
    return token;
}

String PairingManager::generateToken() {
    _currentToken = generateRandomToken(32);
    _tokenExpiry = millis() + TOKEN_VALIDITY_MS;
    
    Serial.printf("[Pairing] Generated new token (expires in %lu ms)\n", TOKEN_VALIDITY_MS);
    
    return _currentToken;
}

String PairingManager::getPairingUrl() const {
    if (_currentToken.isEmpty() || !isTokenValid()) {
        return "";
    }
    
    String url;
    if (_cloudUrl.isEmpty()) {
        // Use default or return just the params for display
        url = "brewos://pair";
    } else {
        url = _cloudUrl + "/pair";
    }
    
    url += "?id=" + _deviceId;
    url += "&token=" + _currentToken;
    
    return url;
}

String PairingManager::getDeviceId() const {
    return _deviceId;
}

String PairingManager::getDeviceKey() const {
    return _deviceKey;
}

String PairingManager::getCurrentToken() const {
    return _currentToken;
}

bool PairingManager::isTokenValid() const {
    if (_currentToken.isEmpty()) {
        return false;
    }
    return millis() < _tokenExpiry;
}

unsigned long PairingManager::getTokenExpiry() const {
    return _tokenExpiry;
}

bool PairingManager::registerTokenWithCloud() {
    if (_cloudUrl.isEmpty() || !WiFi.isConnected()) {
        Serial.println("[Pairing] Cannot register token: no cloud URL or WiFi");
        return false;
    }
    
    if (!isTokenValid()) {
        // Generate new token if expired
        generateToken();
    }
    
    // Retry up to 3 times with delay (network stack may need time after WiFi connects)
    const int maxRetries = 3;
    const int retryDelayMs = 1000;
    
    // Convert WebSocket URL to HTTP URL
    // wss://cloud.brewos.io -> https://cloud.brewos.io
    // wss://cloud.brewos.io -> https://cloud.brewos.io
    // Use substring to avoid replacing occurrences elsewhere in URL
    String httpUrl;
    if (_cloudUrl.startsWith("wss://")) {
        httpUrl = "https://" + _cloudUrl.substring(6); // 6 = length of "wss://"
    } else if (_cloudUrl.startsWith("ws://")) {
        httpUrl = "http://" + _cloudUrl.substring(5);  // 5 = length of "ws://"
    } else {
        httpUrl = _cloudUrl;
    }
    
    for (int attempt = 1; attempt <= maxRetries; attempt++) {
        HTTPClient http;
        String url = httpUrl + "/api/devices/register-claim";
        
        http.begin(url);
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(10000);  // 10 second timeout
        
        // Create request body - include device key for authentication setup
        JsonDocument doc;
        doc["deviceId"] = _deviceId;
        doc["token"] = _currentToken;
        doc["deviceKey"] = _deviceKey;  // Send device key for cloud to store
        
        String body;
        serializeJson(doc, body);
        
        int httpCode = http.POST(body);
        http.end();
        
        if (httpCode == 200) {
            Serial.println("[Pairing] Token and device key registered with cloud");
            return true;
        }
        
        Serial.printf("[Pairing] Registration attempt %d/%d failed: %d\n", attempt, maxRetries, httpCode);
        
        if (attempt < maxRetries) {
            Serial.printf("[Pairing] Retrying in %dms...\n", retryDelayMs);
            delay(retryDelayMs);
        }
    }
    
    Serial.println("[Pairing] All registration attempts failed");
    return false;
}

void PairingManager::onPairingSuccess(std::function<void(const String& userId)> callback) {
    _onPairingSuccess = callback;
}

void PairingManager::notifyPairingSuccess(const String& userId) {
    Serial.printf("[Pairing] Device claimed by user: %s\n", userId.c_str());
    
    if (_onPairingSuccess) {
        _onPairingSuccess(userId);
    }
    
    // Clear the token
    _currentToken = "";
    _tokenExpiry = 0;
}

