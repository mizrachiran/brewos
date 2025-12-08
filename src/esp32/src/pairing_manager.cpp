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
    prefs.begin(NVS_NAMESPACE, true); // Read-only first
    
    String storedKey = prefs.getString(NVS_KEY_DEVICE_KEY, "");
    prefs.end();
    
    if (storedKey.length() >= 32) {
        // Use existing key
        _deviceKey = storedKey;
        Serial.println("[Pairing] Loaded existing device key from NVS");
    } else {
        // Generate new key on first boot
        _deviceKey = generateRandomToken(43); // base64url of 32 bytes ≈ 43 chars
        
        // Store in NVS
        prefs.begin(NVS_NAMESPACE, false); // Read-write
        prefs.putString(NVS_KEY_DEVICE_KEY, _deviceKey);
        prefs.end();
        
        Serial.println("[Pairing] Generated and stored new device key");
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
    
    HTTPClient http;
    String url = _cloudUrl + "/api/devices/register-claim";
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    // Create request body - include device key for authentication setup
    JsonDocument doc;
    doc["deviceId"] = _deviceId;
    doc["token"] = _currentToken;
    doc["deviceKey"] = _deviceKey;  // Send device key for cloud to store
    
    String body;
    serializeJson(doc, body);
    
    int httpCode = http.POST(body);
    
    if (httpCode == 200) {
        Serial.println("[Pairing] Token and device key registered with cloud");
        http.end();
        return true;
    } else {
        Serial.printf("[Pairing] Failed to register token: %d\n", httpCode);
        http.end();
        return false;
    }
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

