#include "pairing_manager.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
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
    
    // Single attempt with longer timeout since we are now in a background task
    // Cloud registration will retry on next connection attempt if it fails
    const int maxRetries = 1;
    const int retryDelayMs = 100;
    
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
        // Test DNS resolution first
        IPAddress resolvedIP;
        String host = httpUrl;
        // Extract host from URL
        if (host.startsWith("https://")) host = host.substring(8);
        else if (host.startsWith("http://")) host = host.substring(7);
        int slashPos = host.indexOf('/');
        if (slashPos > 0) host = host.substring(0, slashPos);
        
        Serial.printf("[Pairing] Resolving DNS for: %s\n", host.c_str());
        unsigned long dnsStart = millis();
        int dnsResult = WiFi.hostByName(host.c_str(), resolvedIP);
        unsigned long dnsTime = millis() - dnsStart;
        
        if (dnsResult == 1) {
            Serial.printf("[Pairing] DNS resolved to %s in %lu ms\n", 
                resolvedIP.toString().c_str(), dnsTime);
        } else {
            Serial.printf("[Pairing] DNS FAILED after %lu ms (result=%d)\n", dnsTime, dnsResult);
        }
        
        // Log network diagnostics
        Serial.printf("[Pairing] WiFi RSSI: %d dBm\n", WiFi.RSSI());
        
        
        // Time each step to find the bottleneck
        unsigned long stepStart = millis();
        
        WiFiClientSecure client;
        client.setInsecure();  // Skip certificate validation
        client.setTimeout(15000);  // 15 second SSL timeout
        
        // Step 1: TCP connect
        Serial.printf("[Pairing] Step 1: TCP connect to %s:443...\n", host.c_str());
        stepStart = millis();
        bool tcpOk = client.connect(host.c_str(), 443);
        Serial.printf("[Pairing] Step 1 done: %s (%lu ms)\n", tcpOk ? "OK" : "FAIL", millis() - stepStart);
        
        if (!tcpOk) {
            Serial.println("[Pairing] TCP/SSL connect failed");
            return false;
        }
        
        // Step 2: Send HTTP request
        Serial.println("[Pairing] Step 2: Sending HTTP POST...");
        stepStart = millis();
        
        JsonDocument doc;
        doc["deviceId"] = _deviceId;
        doc["token"] = _currentToken;
        doc["deviceKey"] = _deviceKey;
        
        String body;
        serializeJson(doc, body);
        
        client.printf("POST /api/devices/register-claim HTTP/1.1\r\n");
        client.printf("Host: %s\r\n", host.c_str());
        client.printf("Content-Type: application/json\r\n");
        client.printf("Content-Length: %d\r\n", body.length());
        client.printf("Connection: close\r\n\r\n");
        client.print(body);
        
        Serial.printf("[Pairing] Step 2 done (%lu ms)\n", millis() - stepStart);
        
        // Step 3: Read response
        Serial.println("[Pairing] Step 3: Reading response...");
        stepStart = millis();
        
        // Wait for response with timeout
        unsigned long timeout = millis() + 10000;
        while (client.connected() && !client.available() && millis() < timeout) {
            delay(10);
        }
        
        String response = "";
        while (client.available()) {
            response += (char)client.read();
        }
        client.stop();
        
        Serial.printf("[Pairing] Step 3 done (%lu ms), response: %d bytes\n", millis() - stepStart, response.length());
        
        // Check for success (200 OK in response)
        int httpCode = -1;
        if (response.indexOf("200 OK") > 0 || response.indexOf("200 ") > 0) {
            httpCode = 200;
        } else if (response.indexOf("HTTP/1.") >= 0) {
            int codeStart = response.indexOf("HTTP/1.") + 9;
            httpCode = response.substring(codeStart, codeStart + 3).toInt();
        }
        
        if (httpCode == 200) {
            Serial.println("[Pairing] Token and device key registered with cloud");
            return true;
        }
        
        Serial.printf("[Pairing] Registration attempt %d/%d failed: %d\n", attempt, maxRetries, httpCode);
        
        if (attempt < maxRetries) {
            Serial.printf("[Pairing] Retrying in %dms...\n", retryDelayMs);
            // Use yield() during delay to keep system responsive
            unsigned long retryStart = millis();
            while (millis() - retryStart < retryDelayMs) {
                yield();
                delay(100);
            }
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

