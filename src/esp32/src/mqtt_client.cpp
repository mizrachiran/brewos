/**
 * BrewOS MQTT Client Implementation
 */

#include "mqtt_client.h"
#include "config.h"
#include "ui/ui.h"  // For ui_state_t definition
#include <ArduinoJson.h>

// Static instance for callback
MQTTClient* MQTTClient::_instance = nullptr;

// MQTT buffer size - must be large enough for HA discovery messages (~600 bytes)
static const uint16_t MQTT_BUFFER_SIZE = 1024;

// =============================================================================
// MQTT Client Implementation
// =============================================================================

MQTTClient::MQTTClient()
    : _client(_wifiClient)
    , _connected(false)
    , _lastReconnectAttempt(0)
    , _lastStatusPublish(0)
    , _reconnectDelay(1000)
    , _wasConnected(false) {
    
    _instance = this;
    _client.setCallback(messageCallback);
    _client.setBufferSize(MQTT_BUFFER_SIZE);
}

bool MQTTClient::begin() {
    LOG_I("Initializing MQTT client...");
    
    // Load configuration from NVS
    loadConfig();
    
    if (!_config.enabled) {
        LOG_I("MQTT is disabled");
        return true;
    }
    
    // Set broker
    _client.setServer(_config.broker, _config.port);
    
    // Generate device ID if not set
    if (strlen(_config.ha_device_id) == 0) {
        generateDeviceID();
    }
    
    // Generate client ID if not set
    if (strlen(_config.client_id) == 0) {
        snprintf(_config.client_id, sizeof(_config.client_id), "brewos_%s", _config.ha_device_id);
    }
    
    LOG_I("MQTT configured: broker=%s:%d, client_id=%s", 
          _config.broker, _config.port, _config.client_id);
    
    // Try initial connection
    if (WiFi.status() == WL_CONNECTED) {
        connect();
    }
    
    return true;
}

void MQTTClient::loop() {
    if (!_config.enabled) return;
    
    // Check for disconnect event
    bool clientConnected = _client.connected();
    if (_wasConnected && !clientConnected) {
        _connected = false;
        _wasConnected = false;
        LOG_W("MQTT connection lost");
        if (_onDisconnected) {
            _onDisconnected();
        }
    }
    
    if (!clientConnected) {
        unsigned long now = millis();
        if (now - _lastReconnectAttempt > (unsigned long)_reconnectDelay) {
            _lastReconnectAttempt = now;
            if (connect()) {
                _reconnectDelay = 1000;  // Reset delay on success
            } else {
                // Exponential backoff, max 60 seconds
                _reconnectDelay = min(_reconnectDelay * 2, 60000);
            }
        }
    } else {
        _client.loop();
        _wasConnected = true;
    }
}

bool MQTTClient::setConfig(const MQTTConfig& config) {
    _config = config;
    
    // Validate
    if (strlen(_config.broker) == 0) {
        LOG_E("MQTT broker cannot be empty");
        return false;
    }
    
    if (_config.port == 0 || _config.port > 65535) {
        LOG_E("Invalid MQTT port: %d", _config.port);
        return false;
    }
    
    // Generate device ID if not set
    if (strlen(_config.ha_device_id) == 0) {
        generateDeviceID();
    }
    
    // Generate client ID if not set
    if (strlen(_config.client_id) == 0) {
        snprintf(_config.client_id, sizeof(_config.client_id), "brewos_%s", _config.ha_device_id);
    }
    
    // Save to NVS
    saveConfig();
    
    // Reconnect if enabled
    if (_config.enabled) {
        disconnect();
        _client.setServer(_config.broker, _config.port);
        if (WiFi.status() == WL_CONNECTED) {
            connect();
        }
    } else {
        disconnect();
    }
    
    LOG_I("MQTT configuration updated");
    return true;
}

bool MQTTClient::testConnection() {
    if (!_config.enabled) {
        return false;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        LOG_W("Cannot test MQTT: WiFi not connected");
        return false;
    }
    
    return connect();
}

bool MQTTClient::connect() {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }
    
    LOG_I("Connecting to MQTT broker %s:%d...", _config.broker, _config.port);
    
    // Build will topic for LWT
    String willTopic = topic("availability");
    
    bool connected = false;
    if (strlen(_config.username) > 0) {
        connected = _client.connect(
            _config.client_id,
            _config.username,
            _config.password,
            willTopic.c_str(),
            1,  // QoS 1
            true,  // Retain
            "offline"
        );
    } else {
        connected = _client.connect(
            _config.client_id,
            willTopic.c_str(),
            1,
            true,
            "offline"
        );
    }
    
    if (connected) {
        LOG_I("MQTT connected!");
        _connected = true;
        _reconnectDelay = 1000;
        
        // Publish online status
        publishAvailability(true);
        
        // Subscribe to command topic
        String cmdTopic = topic("command");
        _client.subscribe(cmdTopic.c_str(), 1);
        LOG_I("Subscribed to: %s", cmdTopic.c_str());
        
        // Publish Home Assistant discovery if enabled
        if (_config.ha_discovery) {
            publishHomeAssistantDiscovery();
        }
        
        if (_onConnected) {
            _onConnected();
        }
    } else {
        LOG_W("MQTT connection failed: %d", _client.state());
        _connected = false;
    }
    
    return _connected;
}

void MQTTClient::disconnect() {
    if (_client.connected()) {
        publishAvailability(false);
        _client.disconnect();
    }
    _connected = false;
}

void MQTTClient::publishStatus(const ui_state_t& state) {
    if (!_connected) return;
    
    // Build status JSON using ArduinoJson for proper formatting
    JsonDocument doc;
    doc["state"] = state.machine_state;
    doc["brew_temp"] = serialized(String(state.brew_temp, 1));
    doc["brew_setpoint"] = serialized(String(state.brew_setpoint, 1));
    doc["steam_temp"] = serialized(String(state.steam_temp, 1));
    doc["steam_setpoint"] = serialized(String(state.steam_setpoint, 1));
    doc["pressure"] = serialized(String(state.pressure, 2));
    doc["is_brewing"] = state.is_brewing;
    doc["is_heating"] = state.is_heating;
    doc["water_low"] = state.water_low;
    doc["alarm_active"] = state.alarm_active;
    doc["pico_connected"] = state.pico_connected;
    doc["wifi_connected"] = state.wifi_connected;
    doc["scale_connected"] = state.scale_connected;
    
    String status;
    serializeJson(doc, status);
    
    // Publish to status topic (retained)
    String statusTopic = topic("status");
    if (!_client.publish(statusTopic.c_str(), (const uint8_t*)status.c_str(), status.length(), true)) {
        LOG_W("Failed to publish status");
    }
}

void MQTTClient::publishShot(const char* shot_json) {
    if (!_connected) return;
    
    String shotTopic = topic("shot");
    if (!_client.publish(shotTopic.c_str(), (const uint8_t*)shot_json, strlen(shot_json), false)) {
        LOG_W("Failed to publish shot data");
    }
}

void MQTTClient::publishStatistics(const char* stats_json) {
    if (!_connected) return;
    
    String statsTopic = topic("statistics");
    if (!_client.publish(statsTopic.c_str(), (const uint8_t*)stats_json, strlen(stats_json), true)) {
        LOG_W("Failed to publish statistics");
    }
}

void MQTTClient::publishAvailability(bool online) {
    // Allow publishing "offline" even if not connected (for graceful disconnect)
    if (!_client.connected() && online) return;
    
    String availTopic = topic("availability");
    const char* msg = online ? "online" : "offline";
    if (!_client.publish(availTopic.c_str(), (const uint8_t*)msg, strlen(msg), true)) {
        LOG_W("Failed to publish availability: %s", msg);
    } else {
        LOG_D("Published availability: %s", msg);
    }
}

void MQTTClient::publishHomeAssistantDiscovery() {
    if (!_connected) return;
    
    LOG_I("Publishing Home Assistant discovery...");
    
    String deviceId = String(_config.ha_device_id);
    String statusTopic = topic("status");
    String availTopic = topic("availability");
    
    // Helper to publish a sensor discovery message
    auto publishSensor = [&](const char* name, const char* sensorId, const char* valueTemplate, 
                             const char* unit, const char* deviceClass, const char* stateClass) {
        JsonDocument doc;
        
        // Device info (shared across all entities)
        JsonObject device = doc["device"].to<JsonObject>();
        device["identifiers"].to<JsonArray>().add("brewos_" + deviceId);
        device["name"] = "BrewOS Coffee Machine";
        device["model"] = "ECM Controller";
        device["manufacturer"] = "BrewOS";
        device["sw_version"] = ESP32_VERSION;
        
        // Entity config
        doc["name"] = name;
        doc["unique_id"] = "brewos_" + deviceId + "_" + String(sensorId);
        doc["object_id"] = "brewos_" + String(sensorId);
        doc["state_topic"] = statusTopic;
        doc["value_template"] = valueTemplate;
        doc["unit_of_measurement"] = unit;
        doc["device_class"] = deviceClass;
        doc["state_class"] = stateClass;
        doc["availability_topic"] = availTopic;
        doc["payload_available"] = "online";
        doc["payload_not_available"] = "offline";
        
        String payload;
        serializeJson(doc, payload);
        
        String configTopic = "homeassistant/sensor/brewos_" + deviceId + "/" + String(sensorId) + "/config";
        
        if (!_client.publish(configTopic.c_str(), (const uint8_t*)payload.c_str(), payload.length(), true)) {
            LOG_W("Failed to publish HA discovery for %s", sensorId);
        }
    };
    
    // Publish sensor discoveries
    publishSensor("Brew Temperature", "brew_temp", "{{ value_json.brew_temp }}", "°C", "temperature", "measurement");
    publishSensor("Steam Temperature", "steam_temp", "{{ value_json.steam_temp }}", "°C", "temperature", "measurement");
    publishSensor("Brew Pressure", "pressure", "{{ value_json.pressure }}", "bar", "pressure", "measurement");
    publishSensor("Brew Setpoint", "brew_setpoint", "{{ value_json.brew_setpoint }}", "°C", "temperature", "measurement");
    publishSensor("Steam Setpoint", "steam_setpoint", "{{ value_json.steam_setpoint }}", "°C", "temperature", "measurement");
    
    // Publish binary sensors for status
    auto publishBinarySensor = [&](const char* name, const char* sensorId, const char* valueTemplate, 
                                   const char* deviceClass) {
        JsonDocument doc;
        
        JsonObject device = doc["device"].to<JsonObject>();
        device["identifiers"].to<JsonArray>().add("brewos_" + deviceId);
        device["name"] = "BrewOS Coffee Machine";
        device["model"] = "ECM Controller";
        device["manufacturer"] = "BrewOS";
        
        doc["name"] = name;
        doc["unique_id"] = "brewos_" + deviceId + "_" + String(sensorId);
        doc["object_id"] = "brewos_" + String(sensorId);
        doc["state_topic"] = statusTopic;
        doc["value_template"] = valueTemplate;
        if (deviceClass) {
            doc["device_class"] = deviceClass;
        }
        doc["payload_on"] = "True";
        doc["payload_off"] = "False";
        doc["availability_topic"] = availTopic;
        
        String payload;
        serializeJson(doc, payload);
        
        String configTopic = "homeassistant/binary_sensor/brewos_" + deviceId + "/" + String(sensorId) + "/config";
        _client.publish(configTopic.c_str(), (const uint8_t*)payload.c_str(), payload.length(), true);
    };
    
    publishBinarySensor("Brewing", "is_brewing", "{{ value_json.is_brewing }}", "running");
    publishBinarySensor("Heating", "is_heating", "{{ value_json.is_heating }}", "heat");
    publishBinarySensor("Water Low", "water_low", "{{ value_json.water_low }}", "problem");
    publishBinarySensor("Alarm", "alarm_active", "{{ value_json.alarm_active }}", "problem");
    publishBinarySensor("Pico Connected", "pico_connected", "{{ value_json.pico_connected }}", "connectivity");
    
    LOG_I("Home Assistant discovery published (%d sensors)", 10);
}

void MQTTClient::onMessage(char* topicName, byte* payload, unsigned int length) {
    // Null-terminate payload
    char message[256];
    size_t len = min(length, sizeof(message) - 1);
    memcpy(message, payload, len);
    message[len] = '\0';
    
    LOG_D("MQTT message: topic=%s, payload=%s", topicName, message);
    
    // Parse command
    String topicStr = String(topicName);
    String cmdTopic = topic("command");
    
    if (topicStr == cmdTopic) {
        // Parse JSON command
        // Example: {"cmd":"set_temp","boiler":"brew","temp":94.0}
        // This would be handled by main.cpp or a command handler
        LOG_I("Received MQTT command: %s", message);
        // TODO: Parse and execute command
    }
}

void MQTTClient::messageCallback(char* topic, byte* payload, unsigned int length) {
    if (_instance) {
        _instance->onMessage(topic, payload, length);
    }
}

String MQTTClient::topic(const char* suffix) const {
    String t = String(_config.topic_prefix);
    t += "/";
    t += _config.ha_device_id;
    t += "/";
    t += suffix;
    return t;
}

String MQTTClient::getStatusString() const {
    if (!_config.enabled) {
        return "Disabled";
    }
    
    if (!_connected) {
        // Get state from non-const client (temporary)
        MQTTClient* nonConst = const_cast<MQTTClient*>(this);
        int state = nonConst->_client.state();
        return "Disconnected (" + String(state) + ")";
    }
    
    return "Connected";
}

void MQTTClient::loadConfig() {
    _prefs.begin("mqtt", true);
    
    _config.enabled = _prefs.getBool("enabled", false);
    _prefs.getString("broker", _config.broker, sizeof(_config.broker));
    _config.port = _prefs.getUShort("port", 1883);
    _prefs.getString("username", _config.username, sizeof(_config.username));
    _prefs.getString("password", _config.password, sizeof(_config.password));
    _prefs.getString("client_id", _config.client_id, sizeof(_config.client_id));
    
    // Topic prefix with fallback to default
    size_t prefixLen = _prefs.getString("topic_prefix", _config.topic_prefix, sizeof(_config.topic_prefix));
    if (prefixLen == 0 || strlen(_config.topic_prefix) == 0) {
        strcpy(_config.topic_prefix, "brewos");
    }
    
    _config.use_tls = _prefs.getBool("use_tls", false);
    _config.ha_discovery = _prefs.getBool("ha_discovery", true);
    _prefs.getString("ha_device_id", _config.ha_device_id, sizeof(_config.ha_device_id));
    
    _prefs.end();
    
    LOG_D("MQTT config loaded: enabled=%d, broker=%s, port=%d", 
          _config.enabled, _config.broker, _config.port);
}

void MQTTClient::saveConfig() {
    _prefs.begin("mqtt", false);
    
    _prefs.putBool("enabled", _config.enabled);
    _prefs.putString("broker", _config.broker);
    _prefs.putUShort("port", _config.port);
    _prefs.putString("username", _config.username);
    _prefs.putString("password", _config.password);
    _prefs.putString("client_id", _config.client_id);
    _prefs.putString("topic_prefix", _config.topic_prefix);
    _prefs.putBool("use_tls", _config.use_tls);
    _prefs.putBool("ha_discovery", _config.ha_discovery);
    _prefs.putString("ha_device_id", _config.ha_device_id);
    
    _prefs.end();
}

void MQTTClient::generateDeviceID() {
    // Generate device ID from MAC address
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(_config.ha_device_id, sizeof(_config.ha_device_id), 
             "%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

