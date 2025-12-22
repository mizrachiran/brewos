/**
 * BrewOS Scale Manager Implementation
 * 
 * Multi-scale BLE support with auto-detection
 */

#include "scale/scale_manager.h"
#include "scale/scale_interface.h"
#include "config.h"
#include <Preferences.h>

// Static instance for callbacks
ScaleManager* ScaleManager::_instance = nullptr;

// Global instance - now a pointer, constructed in main.cpp setup()
ScaleManager* scaleManager = nullptr;

// =============================================================================
// BLE UUIDs for different scale types
// =============================================================================

// Acaia
static const NimBLEUUID ACAIA_SERVICE_UUID("49535343-FE7D-4AE5-8FA9-9FAFD205E455");
static const NimBLEUUID ACAIA_CHAR_UUID("49535343-8841-43F4-A8D4-ECBE34729BB3");

// Felicita  
static const NimBLEUUID FELICITA_SERVICE_UUID("FFE0");
static const NimBLEUUID FELICITA_CHAR_UUID("FFE1");

// Decent Scale
static const NimBLEUUID DECENT_SERVICE_UUID("FFF0");
static const NimBLEUUID DECENT_WEIGHT_CHAR_UUID("FFF4");
static const NimBLEUUID DECENT_CMD_CHAR_UUID("36F5");

// Timemore
static const NimBLEUUID TIMEMORE_SERVICE_UUID("0000FFE0-0000-1000-8000-00805F9B34FB");
static const NimBLEUUID TIMEMORE_CHAR_UUID("0000FFE1-0000-1000-8000-00805F9B34FB");

// Generic Weight Scale Service (Bluetooth SIG standard)
static const NimBLEUUID WSS_SERVICE_UUID((uint16_t)0x181D);
static const NimBLEUUID WSS_WEIGHT_CHAR_UUID((uint16_t)0x2A9D);

// =============================================================================
// Acaia Protocol Constants
// =============================================================================

// Acaia message types
#define ACAIA_MSG_SYSTEM      0
#define ACAIA_MSG_WEIGHT      5
#define ACAIA_MSG_TIMER       7
#define ACAIA_MSG_KEY         8
#define ACAIA_MSG_ACK         11

// Acaia commands
static const uint8_t ACAIA_HEADER[] = {0xEF, 0xDD};
static const uint8_t ACAIA_IDENT[] = {0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D};
static const uint8_t ACAIA_HEARTBEAT[] = {0x00, 0x02, 0x00};
static const uint8_t ACAIA_TARE_CMD[] = {0x00, 0x04};
static const uint8_t ACAIA_TIMER_START[] = {0x00, 0x0D, 0x00};
static const uint8_t ACAIA_TIMER_STOP[] = {0x00, 0x0D, 0x01};
static const uint8_t ACAIA_TIMER_RESET[] = {0x00, 0x0D, 0x02};

// =============================================================================
// Constructor / Destructor
// =============================================================================

ScaleManager::ScaleManager()
    : _client(nullptr)
    , _scan(nullptr)
    , _initialized(false)
    , _scanning(false)
    , _connected(false)
    , _connecting(false)
    , _scaleType(SCALE_TYPE_UNKNOWN)
    , _lastWeightTime(0)
    , _lastReconnectAttempt(0)
    , _autoReconnect(true)
    , _lastWeight(0)
    , _lastFlowTime(0)
    , _weightChar(nullptr)
    , _commandChar(nullptr) {
    
    _instance = this;
    memset(&_state, 0, sizeof(_state));
    memset(_scaleName, 0, sizeof(_scaleName));
    memset(_scaleAddress, 0, sizeof(_scaleAddress));
}

ScaleManager::~ScaleManager() {
    end();
    _instance = nullptr;
}

// =============================================================================
// Initialization
// =============================================================================

bool ScaleManager::begin() {
    if (_initialized) return true;
    
    LOG_I("Initializing Scale Manager...");
    
    // Initialize NimBLE
    NimBLEDevice::init("BrewOS");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);  // Max power for range
    NimBLEDevice::setSecurityAuth(false, false, false);  // No bonding needed
    
    // Create client
    _client = NimBLEDevice::createClient();
    _client->setClientCallbacks(this, false);
    _client->setConnectionParams(12, 12, 0, 200);  // Fast connection
    _client->setConnectTimeout(10);
    
    // Get scanner
    _scan = NimBLEDevice::getScan();
    _scan->setAdvertisedDeviceCallbacks(this, false);
    _scan->setActiveScan(true);
    _scan->setInterval(100);
    _scan->setWindow(99);
    
    // Load saved scale from preferences
    loadSavedScale();
    
    _initialized = true;
    LOG_I("Scale Manager initialized");
    
    return true;
}

void ScaleManager::end() {
    if (!_initialized) return;
    
    disconnect();
    stopScan();
    
    if (_client) {
        NimBLEDevice::deleteClient(_client);
        _client = nullptr;
    }
    
    NimBLEDevice::deinit(true);
    _initialized = false;
    
    LOG_I("Scale Manager shutdown");
}

// =============================================================================
// Main Loop
// =============================================================================

void ScaleManager::loop() {
    if (!_initialized) return;
    
    uint32_t now = millis();
    
    // Check for connection timeout
    if (_connected && _lastWeightTime > 0) {
        if (now - _lastWeightTime > SCALE_WEIGHT_TIMEOUT_MS) {
            LOG_W("Scale weight timeout - disconnecting");
            disconnect();
        }
    }
    
    // Auto-reconnect to saved scale
    if (!_connected && !_connecting && !_scanning && _autoReconnect) {
        if (strlen(_scaleAddress) > 0 && (now - _lastReconnectAttempt > SCALE_RECONNECT_DELAY_MS)) {
            _lastReconnectAttempt = now;
            LOG_I("Attempting to reconnect to %s", _scaleAddress);
            connect(_scaleAddress);
        }
    }
    
    // Send heartbeat to Acaia and Bookoo scales (same protocol)
    if (_connected && (_scaleType == SCALE_TYPE_ACAIA || _scaleType == SCALE_TYPE_BOOKOO)) {
        static uint32_t lastHeartbeat = 0;
        if (now - lastHeartbeat > 3000) {
            lastHeartbeat = now;
            if (_commandChar) {
                uint8_t heartbeat[5];
                heartbeat[0] = ACAIA_HEADER[0];
                heartbeat[1] = ACAIA_HEADER[1];
                memcpy(&heartbeat[2], ACAIA_HEARTBEAT, 3);
                _commandChar->writeValue(heartbeat, 5, false);
            }
        }
    }
}

// =============================================================================
// Scanning
// =============================================================================

void ScaleManager::startScan(uint32_t duration_ms) {
    if (_scanning || _connecting) return;
    
    if (duration_ms == 0) duration_ms = SCALE_SCAN_DURATION_MS;
    
    LOG_I("Starting BLE scan for %d ms", duration_ms);
    
    _discoveredScales.clear();
    _scanning = true;
    
    _scan->start(duration_ms / 1000, [](NimBLEScanResults results) {
        if (_instance) {
            _instance->_scanning = false;
            LOG_I("Scan complete, found %d devices", results.getCount());
        }
    }, false);
}

void ScaleManager::stopScan() {
    if (!_scanning) return;
    
    _scan->stop();
    _scanning = false;
    LOG_I("Scan stopped");
}

// =============================================================================
// Connection
// =============================================================================

bool ScaleManager::connect(const char* address) {
    if (_connected || _connecting) {
        disconnect();
    }
    
    if (!address && strlen(_scaleAddress) > 0) {
        address = _scaleAddress;
    }
    
    if (!address) {
        LOG_E("No scale address provided");
        return false;
    }
    
    LOG_I("Connecting to scale: %s", address);
    _connecting = true;
    
    NimBLEAddress addr(address);
    
    if (!_client->connect(addr)) {
        LOG_E("Failed to connect to %s", address);
        _connecting = false;
        return false;
    }
    
    // Connection successful - setup will happen in onConnect callback
    return true;
}

bool ScaleManager::connectByIndex(int index) {
    if (index < 0 || index >= (int)_discoveredScales.size()) {
        LOG_E("Invalid scale index: %d", index);
        return false;
    }
    
    return connect(_discoveredScales[index].address);
}

void ScaleManager::disconnect() {
    if (_client && _client->isConnected()) {
        _client->disconnect();
    }
    
    _connected = false;
    _connecting = false;
    _weightChar = nullptr;
    _commandChar = nullptr;
    _state.connected = false;
    
    if (_connectionCallback) {
        _connectionCallback(false);
    }
}

void ScaleManager::forgetScale() {
    disconnect();
    
    Preferences prefs;
    prefs.begin(NVS_SCALE_NAMESPACE, false);
    prefs.clear();
    prefs.end();
    
    memset(_scaleAddress, 0, sizeof(_scaleAddress));
    memset(_scaleName, 0, sizeof(_scaleName));
    _scaleType = SCALE_TYPE_UNKNOWN;
    _autoReconnect = false;
    
    LOG_I("Scale forgotten");
}

// =============================================================================
// Scale Operations
// =============================================================================

void ScaleManager::tare() {
    if (!_connected || !_commandChar) return;
    
    switch (_scaleType) {
        case SCALE_TYPE_ACAIA:
            sendAcaiaTare();
            break;
        case SCALE_TYPE_BOOKOO:
            sendBookooTare();
            break;
        case SCALE_TYPE_FELICITA:
            sendFelicitaTare();
            break;
        case SCALE_TYPE_DECENT:
            sendDecentTare();
            break;
        case SCALE_TYPE_TIMEMORE:
            sendTimemoreTare();
            break;
        default:
            LOG_W("Tare not supported for this scale type");
            break;
    }
    
    LOG_I("Tare command sent");
}

void ScaleManager::startTimer() {
    if (!_connected || !_commandChar) return;
    
    if (_scaleType == SCALE_TYPE_ACAIA || _scaleType == SCALE_TYPE_BOOKOO) {
        uint8_t cmd[5];
        cmd[0] = ACAIA_HEADER[0];
        cmd[1] = ACAIA_HEADER[1];
        memcpy(&cmd[2], ACAIA_TIMER_START, 3);
        _commandChar->writeValue(cmd, 5, false);
        LOG_I("Timer start sent");
    }
}

void ScaleManager::stopTimer() {
    if (!_connected || !_commandChar) return;
    
    if (_scaleType == SCALE_TYPE_ACAIA || _scaleType == SCALE_TYPE_BOOKOO) {
        uint8_t cmd[5];
        cmd[0] = ACAIA_HEADER[0];
        cmd[1] = ACAIA_HEADER[1];
        memcpy(&cmd[2], ACAIA_TIMER_STOP, 3);
        _commandChar->writeValue(cmd, 5, false);
        LOG_I("Timer stop sent");
    }
}

void ScaleManager::resetTimer() {
    if (!_connected || !_commandChar) return;
    
    if (_scaleType == SCALE_TYPE_ACAIA || _scaleType == SCALE_TYPE_BOOKOO) {
        uint8_t cmd[5];
        cmd[0] = ACAIA_HEADER[0];
        cmd[1] = ACAIA_HEADER[1];
        memcpy(&cmd[2], ACAIA_TIMER_RESET, 3);
        _commandChar->writeValue(cmd, 5, false);
        LOG_I("Timer reset sent");
    }
}

// =============================================================================
// NimBLE Callbacks
// =============================================================================

void ScaleManager::onConnect(NimBLEClient* client) {
    LOG_I("BLE connected, setting up characteristics...");
    
    if (!setupCharacteristics()) {
        LOG_E("Failed to setup characteristics");
        disconnect();
        return;
    }
    
    _connected = true;
    _connecting = false;
    _lastWeightTime = millis();
    _state.connected = true;
    
    // Save to NVS for auto-reconnect
    saveScale();
    
    if (_connectionCallback) {
        _connectionCallback(true);
    }
    
    LOG_I("Scale connected: %s (%s)", _scaleName, getScaleTypeName(_scaleType));
}

void ScaleManager::onDisconnect(NimBLEClient* client) {
    LOG_W("BLE disconnected");
    
    _connected = false;
    _connecting = false;
    _weightChar = nullptr;
    _commandChar = nullptr;
    _state.connected = false;
    
    if (_connectionCallback) {
        _connectionCallback(false);
    }
}

void ScaleManager::onResult(NimBLEAdvertisedDevice* device) {
    String name = device->getName().c_str();
    
    // Skip unnamed devices
    if (name.length() == 0) return;
    
    // Detect scale type
    scale_type_t type = detectScaleType(name.c_str());
    if (type == SCALE_TYPE_UNKNOWN) {
        // Check for service UUIDs
        if (device->isAdvertisingService(ACAIA_SERVICE_UUID)) {
            type = SCALE_TYPE_ACAIA;
        } else if (device->isAdvertisingService(FELICITA_SERVICE_UUID)) {
            type = SCALE_TYPE_FELICITA;
        } else if (device->isAdvertisingService(DECENT_SERVICE_UUID)) {
            type = SCALE_TYPE_DECENT;
        } else if (device->isAdvertisingService(WSS_SERVICE_UUID)) {
            type = SCALE_TYPE_GENERIC_WSS;
        } else {
            return;  // Not a known scale
        }
    }
    
    // Check if already in list
    String addr = device->getAddress().toString().c_str();
    for (const auto& s : _discoveredScales) {
        if (strcmp(s.address, addr.c_str()) == 0) {
            return;  // Already found
        }
    }
    
    // Add to discovered list
    if (_discoveredScales.size() < SCALE_MAX_DISCOVERED) {
        scale_info_t info;
        strncpy(info.name, name.c_str(), sizeof(info.name) - 1);
        strncpy(info.address, addr.c_str(), sizeof(info.address) - 1);
        info.type = type;
        info.rssi = device->getRSSI();
        
        _discoveredScales.push_back(info);
        
        LOG_I("Found scale: %s (%s) RSSI:%d", info.name, getScaleTypeName(type), info.rssi);
        
        if (_discoveryCallback) {
            _discoveryCallback(info);
        }
    }
}

// =============================================================================
// Notification Callback (static)
// =============================================================================

void ScaleManager::notifyCallback(NimBLERemoteCharacteristic* chr, uint8_t* data, 
                                   size_t length, bool isNotify) {
    if (_instance) {
        _instance->processWeightData(data, length);
    }
}

// =============================================================================
// Internal Methods
// =============================================================================

void ScaleManager::loadSavedScale() {
    Preferences prefs;
    
    // Try read-write first to create namespace if it doesn't exist
    // This is normal after a fresh flash - will use defaults
    bool beginOk = prefs.begin(NVS_SCALE_NAMESPACE, false);
    
    if (!beginOk) {
        // No saved scale (fresh flash) - use default values
        return;
    }
    
    // Read saved scale data from NVS
    prefs.getString(NVS_SCALE_ADDRESS, _scaleAddress, sizeof(_scaleAddress));
    prefs.getString(NVS_SCALE_NAME, _scaleName, sizeof(_scaleName));
    _scaleType = (scale_type_t)prefs.getUChar(NVS_SCALE_TYPE, SCALE_TYPE_UNKNOWN);
    
    prefs.end();
    
    if (strlen(_scaleAddress) > 0) {
        LOG_I("Loaded saved scale: %s (%s)", _scaleName, _scaleAddress);
    }
}

void ScaleManager::saveScale() {
    Preferences prefs;
    prefs.begin(NVS_SCALE_NAMESPACE, false);
    
    prefs.putString(NVS_SCALE_ADDRESS, _scaleAddress);
    prefs.putString(NVS_SCALE_NAME, _scaleName);
    prefs.putUChar(NVS_SCALE_TYPE, (uint8_t)_scaleType);
    
    prefs.end();
    
    LOG_I("Saved scale to NVS");
}

bool ScaleManager::setupCharacteristics() {
    // Get connected device info
    NimBLEAddress addr = _client->getPeerAddress();
    strncpy(_scaleAddress, addr.toString().c_str(), sizeof(_scaleAddress) - 1);
    
    // Try to get name from remote device
    NimBLERemoteService* gapSvc = _client->getService(NimBLEUUID((uint16_t)0x1800));
    if (gapSvc) {
        NimBLERemoteCharacteristic* nameChar = gapSvc->getCharacteristic(NimBLEUUID((uint16_t)0x2A00));
        if (nameChar && nameChar->canRead()) {
            std::string name = nameChar->readValue();
            if (name.length() > 0) {
                strncpy(_scaleName, name.c_str(), sizeof(_scaleName) - 1);
            }
        }
    }
    
    // Detect type from name or services
    _scaleType = detectScaleType(_scaleName);
    
    // Setup based on type
    bool success = false;
    switch (_scaleType) {
        case SCALE_TYPE_ACAIA:
            success = setupAcaia();
            break;
        case SCALE_TYPE_BOOKOO:
            success = setupBookoo();
            break;
        case SCALE_TYPE_FELICITA:
            success = setupFelicita();
            break;
        case SCALE_TYPE_DECENT:
            success = setupDecent();
            break;
        case SCALE_TYPE_TIMEMORE:
            success = setupTimemore();
            break;
        default:
            // Try generic WSS
            success = setupGenericWSS();
            if (success) _scaleType = SCALE_TYPE_GENERIC_WSS;
            break;
    }
    
    return success;
}

void ScaleManager::processWeightData(const uint8_t* data, size_t length) {
    _lastWeightTime = millis();
    
    switch (_scaleType) {
        case SCALE_TYPE_ACAIA:
            parseAcaiaWeight(data, length);
            break;
        case SCALE_TYPE_BOOKOO:
            parseBookooWeight(data, length);
            break;
        case SCALE_TYPE_FELICITA:
            parseFelicitaWeight(data, length);
            break;
        case SCALE_TYPE_DECENT:
            parseDecentWeight(data, length);
            break;
        case SCALE_TYPE_TIMEMORE:
            parseTimemoreWeight(data, length);
            break;
        case SCALE_TYPE_GENERIC_WSS:
            parseGenericWeight(data, length);
            break;
        default:
            break;
    }
    
    // Update flow rate
    updateFlowRate(_state.weight);
    
    // Notify callback
    if (_weightCallback) {
        _weightCallback(_state);
    }
}

void ScaleManager::updateFlowRate(float weight) {
    uint32_t now = millis();
    uint32_t dt = now - _lastFlowTime;
    
    if (dt >= 200) {  // Calculate every 200ms
        float dw = weight - _lastWeight;
        float instantFlow = (dw / (float)dt) * 1000.0f;  // g/s
        
        // Apply exponential smoothing
        _state.flow_rate = instantFlow * 0.3f + _state.flow_rate * 0.7f;
        
        // Clamp to reasonable values
        if (_state.flow_rate < 0) _state.flow_rate = 0;
        if (_state.flow_rate > 20) _state.flow_rate = 20;
        
        _lastWeight = weight;
        _lastFlowTime = now;
    }
}

// =============================================================================
// Acaia Scale Support
// =============================================================================

bool ScaleManager::setupAcaia() {
    LOG_I("Setting up Acaia scale");
    
    NimBLERemoteService* service = _client->getService(ACAIA_SERVICE_UUID);
    if (!service) {
        LOG_E("Acaia service not found");
        return false;
    }
    
    _weightChar = service->getCharacteristic(ACAIA_CHAR_UUID);
    _commandChar = _weightChar;  // Same characteristic for Acaia
    
    if (!_weightChar) {
        LOG_E("Acaia characteristic not found");
        return false;
    }
    
    // Subscribe to notifications
    if (!_weightChar->subscribe(true, notifyCallback, true)) {
        LOG_E("Failed to subscribe to Acaia notifications");
        return false;
    }
    
    // Send identification sequence
    uint8_t identCmd[17];
    identCmd[0] = ACAIA_HEADER[0];
    identCmd[1] = ACAIA_HEADER[1];
    memcpy(&identCmd[2], ACAIA_IDENT, 15);
    _weightChar->writeValue(identCmd, 17, false);
    
    delay(100);
    
    // Request weight updates
    uint8_t notifyCmd[] = {0xEF, 0xDD, 0x00, 0x0C, 0x09, 0x00, 0x01, 0x01, 0x02, 0x02, 0x05, 0x03, 0x04};
    _weightChar->writeValue(notifyCmd, sizeof(notifyCmd), false);
    
    LOG_I("Acaia setup complete");
    return true;
}

void ScaleManager::parseAcaiaWeight(const uint8_t* data, size_t length) {
    if (length < 6) return;
    
    // Check header
    if (data[0] != ACAIA_HEADER[0] || data[1] != ACAIA_HEADER[1]) return;
    
    uint8_t msgType = data[2];
    
    if (msgType == ACAIA_MSG_WEIGHT && length >= 7) {
        // Weight message: header(2) + type(1) + weight_lo + weight_hi + unit + stable
        int16_t raw = (data[4] << 8) | data[3];
        int8_t unit = data[5];  // 0=g, 1=oz
        bool stable = (data[6] & 0x01) != 0;
        
        float weight = raw / 10.0f;
        if (unit == 1) weight *= 28.3495f;  // Convert oz to g
        
        // Handle negative (Acaia uses sign bit)
        if (data[6] & 0x02) weight = -weight;
        
        _state.weight = weight;
        _state.stable = stable;
    }
}

void ScaleManager::sendAcaiaTare() {
    if (!_commandChar) return;
    
    uint8_t cmd[4];
    cmd[0] = ACAIA_HEADER[0];
    cmd[1] = ACAIA_HEADER[1];
    cmd[2] = ACAIA_TARE_CMD[0];
    cmd[3] = ACAIA_TARE_CMD[1];
    _commandChar->writeValue(cmd, 4, false);
}

// =============================================================================
// Bookoo Themis Scale Support (Acaia-compatible protocol)
// =============================================================================

bool ScaleManager::setupBookoo() {
    LOG_I("Setting up Bookoo Themis scale (Acaia-compatible)");
    
    // Bookoo Themis uses the same service and characteristics as Acaia
    NimBLERemoteService* service = _client->getService(ACAIA_SERVICE_UUID);
    if (!service) {
        LOG_E("Bookoo/Acaia service not found");
        return false;
    }
    
    _weightChar = service->getCharacteristic(ACAIA_CHAR_UUID);
    _commandChar = _weightChar;  // Same characteristic for Bookoo (like Acaia)
    
    if (!_weightChar) {
        LOG_E("Bookoo/Acaia characteristic not found");
        return false;
    }
    
    // Subscribe to notifications
    if (!_weightChar->subscribe(true, notifyCallback, true)) {
        LOG_E("Failed to subscribe to Bookoo notifications");
        return false;
    }
    
    // Send identification sequence (same as Acaia)
    uint8_t identCmd[17];
    identCmd[0] = ACAIA_HEADER[0];
    identCmd[1] = ACAIA_HEADER[1];
    memcpy(&identCmd[2], ACAIA_IDENT, 15);
    _weightChar->writeValue(identCmd, 17, false);
    
    delay(100);
    
    // Request weight updates (same as Acaia)
    uint8_t notifyCmd[] = {0xEF, 0xDD, 0x00, 0x0C, 0x09, 0x00, 0x01, 0x01, 0x02, 0x02, 0x05, 0x03, 0x04};
    _weightChar->writeValue(notifyCmd, sizeof(notifyCmd), false);
    
    LOG_I("Bookoo setup complete");
    return true;
}

void ScaleManager::parseBookooWeight(const uint8_t* data, size_t length) {
    // Bookoo uses same weight format as Acaia
    parseAcaiaWeight(data, length);
}

void ScaleManager::sendBookooTare() {
    // Bookoo uses same tare command as Acaia
    sendAcaiaTare();
}

// =============================================================================
// Felicita Scale Support
// =============================================================================

bool ScaleManager::setupFelicita() {
    LOG_I("Setting up Felicita scale");
    
    NimBLERemoteService* service = _client->getService(FELICITA_SERVICE_UUID);
    if (!service) {
        LOG_E("Felicita service not found");
        return false;
    }
    
    _weightChar = service->getCharacteristic(FELICITA_CHAR_UUID);
    _commandChar = _weightChar;
    
    if (!_weightChar) {
        LOG_E("Felicita characteristic not found");
        return false;
    }
    
    if (!_weightChar->subscribe(true, notifyCallback, true)) {
        LOG_E("Failed to subscribe to Felicita notifications");
        return false;
    }
    
    LOG_I("Felicita setup complete");
    return true;
}

void ScaleManager::parseFelicitaWeight(const uint8_t* data, size_t length) {
    // Felicita sends ASCII weight like "  123.4" or "-  12.3"
    if (length < 2) return;
    
    char buf[32];
    size_t copyLen = min(length, sizeof(buf) - 1);
    memcpy(buf, data, copyLen);
    buf[copyLen] = '\0';
    
    // Parse weight from ASCII
    float weight = atof(buf);
    
    _state.weight = weight;
    _state.stable = true;  // Felicita doesn't indicate stability
}

void ScaleManager::sendFelicitaTare() {
    if (!_commandChar) return;
    
    // Felicita tare command: "T"
    uint8_t cmd[] = {'T'};
    _commandChar->writeValue(cmd, 1, false);
}

// =============================================================================
// Decent Scale Support  
// =============================================================================

bool ScaleManager::setupDecent() {
    LOG_I("Setting up Decent scale");
    
    NimBLERemoteService* service = _client->getService(DECENT_SERVICE_UUID);
    if (!service) {
        LOG_E("Decent service not found");
        return false;
    }
    
    _weightChar = service->getCharacteristic(DECENT_WEIGHT_CHAR_UUID);
    _commandChar = service->getCharacteristic(DECENT_CMD_CHAR_UUID);
    
    if (!_weightChar) {
        LOG_E("Decent weight characteristic not found");
        return false;
    }
    
    if (!_weightChar->subscribe(true, notifyCallback, true)) {
        LOG_E("Failed to subscribe to Decent notifications");
        return false;
    }
    
    // Turn on LED to indicate connection
    if (_commandChar) {
        uint8_t ledOn[] = {0x0A, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0B};
        _commandChar->writeValue(ledOn, sizeof(ledOn), false);
    }
    
    LOG_I("Decent setup complete");
    return true;
}

void ScaleManager::parseDecentWeight(const uint8_t* data, size_t length) {
    if (length < 7) return;
    
    // Decent format: type(1) + weight(3 LE signed) + unknown(3) + xor
    // Weight is in 0.1g units, little-endian signed 24-bit
    
    int32_t raw = data[1] | (data[2] << 8) | ((int8_t)data[3] << 16);
    
    _state.weight = raw / 10.0f;
    _state.stable = (data[0] & 0x02) != 0;  // Bit 1 indicates stable
}

void ScaleManager::sendDecentTare() {
    if (!_commandChar) return;
    
    // Decent tare command
    uint8_t cmd[] = {0x03, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C};
    _commandChar->writeValue(cmd, sizeof(cmd), false);
}

// =============================================================================
// Timemore Scale Support
// =============================================================================

bool ScaleManager::setupTimemore() {
    LOG_I("Setting up Timemore scale");
    
    NimBLERemoteService* service = _client->getService(TIMEMORE_SERVICE_UUID);
    if (!service) {
        // Try short UUID
        service = _client->getService(FELICITA_SERVICE_UUID);  // Same short UUID
    }
    
    if (!service) {
        LOG_E("Timemore service not found");
        return false;
    }
    
    _weightChar = service->getCharacteristic(TIMEMORE_CHAR_UUID);
    if (!_weightChar) {
        _weightChar = service->getCharacteristic(FELICITA_CHAR_UUID);
    }
    _commandChar = _weightChar;
    
    if (!_weightChar) {
        LOG_E("Timemore characteristic not found");
        return false;
    }
    
    if (!_weightChar->subscribe(true, notifyCallback, true)) {
        LOG_E("Failed to subscribe to Timemore notifications");
        return false;
    }
    
    LOG_I("Timemore setup complete");
    return true;
}

void ScaleManager::parseTimemoreWeight(const uint8_t* data, size_t length) {
    // Timemore is similar to Felicita - ASCII format
    parseFelicitaWeight(data, length);
}

void ScaleManager::sendTimemoreTare() {
    // Same as Felicita
    sendFelicitaTare();
}

// =============================================================================
// Generic Weight Scale Service
// =============================================================================

bool ScaleManager::setupGenericWSS() {
    LOG_I("Setting up Generic WSS scale");
    
    NimBLERemoteService* service = _client->getService(WSS_SERVICE_UUID);
    if (!service) {
        LOG_E("WSS service not found");
        return false;
    }
    
    _weightChar = service->getCharacteristic(WSS_WEIGHT_CHAR_UUID);
    _commandChar = nullptr;  // Generic WSS has no command characteristic
    
    if (!_weightChar) {
        LOG_E("WSS weight characteristic not found");
        return false;
    }
    
    // WSS uses indications, not notifications
    if (!_weightChar->subscribe(false, notifyCallback, true)) {
        LOG_E("Failed to subscribe to WSS indications");
        return false;
    }
    
    LOG_I("Generic WSS setup complete");
    return true;
}

void ScaleManager::parseGenericWeight(const uint8_t* data, size_t length) {
    // Bluetooth SIG Weight Measurement format
    // Flags(1) + Weight(2 or 4) + optional fields
    
    if (length < 3) return;
    
    uint8_t flags = data[0];
    bool imperial = (flags & 0x01) != 0;
    // bool timestamp = (flags & 0x02) != 0;
    // bool userId = (flags & 0x04) != 0;
    // bool bmi = (flags & 0x08) != 0;
    
    // Weight is 16-bit unsigned, resolution 0.005 kg or 0.01 lb
    uint16_t raw = data[1] | (data[2] << 8);
    
    float weight;
    if (imperial) {
        weight = raw * 0.01f * 453.592f;  // lb to g
    } else {
        weight = raw * 5.0f;  // 0.005 kg = 5g resolution
    }
    
    _state.weight = weight;
    _state.stable = true;
}

