/**
 * BrewOS MQTT Client Implementation
 */

#include "mqtt_client.h"
#include "config.h"
#include "ui/ui.h"  // For ui_state_t definition
#include "power_meter/power_meter.h"
#include <ArduinoJson.h>
#include <initializer_list>

// Static instance for callback
MQTTClient* MQTTClient::_instance = nullptr;

// MQTT buffer size - must be large enough for HA discovery messages (~600 bytes)
static const uint16_t MQTT_BUFFER_SIZE = 1024;

// Total number of entities published to Home Assistant
// Sensors: 5 temps + 7 power + 5 shot/scale + 3 stats = 20
// Binary: 7 status sensors
// Controls: 1 switch + 3 buttons + 3 numbers + 1 select = 8
static const uint8_t HA_TOTAL_ENTITY_COUNT = 35;

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

void MQTTClient::setEnabled(bool enabled) {
    if (enabled && !_config.enabled) {
        // Re-enable MQTT
        _config.enabled = true;
        _reconnectDelay = 1000;
        _lastReconnectAttempt = 0;  // Allow immediate reconnect
        LOG_I("MQTT enabled");
    } else if (!enabled && _config.enabled) {
        // Disable MQTT - disconnect first
        LOG_I("MQTT disabling...");
        disconnect();
        _config.enabled = false;
        LOG_I("MQTT disabled");
    }
}

void MQTTClient::publishStatus(const ui_state_t& state) {
    if (!_connected) return;
    
    // Build comprehensive status JSON
    JsonDocument doc;
    
    // Machine state - convert to string for HA templates
    const char* stateStr = "unknown";
    const char* modeStr = "standby";
    switch (state.machine_state) {
        case UI_STATE_INIT: stateStr = "init"; modeStr = "standby"; break;
        case UI_STATE_IDLE: stateStr = "standby"; modeStr = "standby"; break;
        case UI_STATE_HEATING: stateStr = "heating"; modeStr = "on"; break;
        case UI_STATE_READY: stateStr = "ready"; modeStr = "on"; break;
        case UI_STATE_BREWING: stateStr = "brewing"; modeStr = "on"; break;
        case UI_STATE_FAULT: stateStr = "fault"; modeStr = "standby"; break;
        case UI_STATE_SAFE: stateStr = "safe"; modeStr = "standby"; break;
        case UI_STATE_ECO: stateStr = "eco"; modeStr = "eco"; break;
    }
    doc["state"] = stateStr;
    doc["mode"] = modeStr;
    doc["heating_strategy"] = state.heating_strategy;  // 0-3 for HA template
    
    // Temperature readings
    doc["brew_temp"] = serialized(String(state.brew_temp, 1));
    doc["brew_setpoint"] = serialized(String(state.brew_setpoint, 1));
    doc["steam_temp"] = serialized(String(state.steam_temp, 1));
    doc["steam_setpoint"] = serialized(String(state.steam_setpoint, 1));
    
    // Pressure
    doc["pressure"] = serialized(String(state.pressure, 2));
    
    // Scale data - use brew_weight as scale weight when brewing
    doc["scale_weight"] = serialized(String(state.brew_weight, 1));
    doc["flow_rate"] = serialized(String(state.flow_rate, 1));
    doc["scale_connected"] = state.scale_connected;
    
    // Active shot data
    doc["shot_duration"] = state.brew_time_ms / 1000.0f;
    doc["shot_weight"] = serialized(String(state.brew_weight, 1));
    doc["is_brewing"] = state.is_brewing;
    
    // Target weight (for BBW)
    doc["target_weight"] = serialized(String(state.target_weight, 1));
    
    // Status flags
    doc["is_heating"] = state.is_heating;
    doc["water_low"] = state.water_low;
    doc["alarm_active"] = state.alarm_active;
    doc["pico_connected"] = state.pico_connected;
    doc["wifi_connected"] = state.wifi_connected;
    
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

void MQTTClient::publishStatisticsJson(const char* stats_json) {
    if (!_connected) return;
    
    String statsTopic = topic("statistics");
    if (!_client.publish(statsTopic.c_str(), (const uint8_t*)stats_json, strlen(stats_json), true)) {
        LOG_W("Failed to publish statistics");
    }
}

void MQTTClient::publishStatistics(uint16_t shotsToday, uint32_t totalShots, float kwhToday) {
    if (!_connected) return;
    
    // Build stats JSON and add to status topic for HA sensors
    JsonDocument doc;
    doc["shots_today"] = shotsToday;
    doc["total_shots"] = totalShots;
    doc["kwh_today"] = serialized(String(kwhToday, 3));
    
    String stats;
    serializeJson(doc, stats);
    
    // Publish to statistics topic (retained)
    String statsTopic = topic("statistics");
    if (!_client.publish(statsTopic.c_str(), (const uint8_t*)stats.c_str(), stats.length(), true)) {
        LOG_W("Failed to publish statistics");
    }
}

void MQTTClient::publishPowerMeter(const PowerMeterReading& reading) {
    if (!_connected) return;
    
    // Build power JSON
    JsonDocument doc;
    doc["voltage"] = serialized(String(reading.voltage, 1));
    doc["current"] = serialized(String(reading.current, 2));
    doc["power"] = serialized(String(reading.power, 0));
    doc["energy_import"] = serialized(String(reading.energy_import, 3));
    doc["energy_export"] = serialized(String(reading.energy_export, 3));
    doc["frequency"] = serialized(String(reading.frequency, 1));
    doc["power_factor"] = serialized(String(reading.power_factor, 2));
    
    String powerJson;
    serializeJson(doc, powerJson);
    
    // Publish to power topic (retained)
    String powerTopic = topic("power");
    if (!_client.publish(powerTopic.c_str(), (const uint8_t*)powerJson.c_str(), powerJson.length(), true)) {
        LOG_W("Failed to publish power meter data");
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
    String commandTopic = topic("command");
    String powerTopic = topic("power");
    String shotTopic = topic("shot");
    
    // Device info helper - returns a consistent device object
    auto addDeviceInfo = [&](JsonDocument& doc) {
        JsonObject device = doc["device"].to<JsonObject>();
        device["identifiers"].to<JsonArray>().add("brewos_" + deviceId);
        device["name"] = "BrewOS Coffee Machine";
        device["model"] = "ECM Controller";
        device["manufacturer"] = "BrewOS";
        device["sw_version"] = ESP32_VERSION;
        device["configuration_url"] = "http://" + WiFi.localIP().toString();
    };
    
    // Helper to publish a sensor discovery message
    auto publishSensor = [&](const char* name, const char* sensorId, const char* valueTemplate, 
                             const char* unit, const char* deviceClass, const char* stateClass,
                             const char* stateTopic = nullptr, const char* icon = nullptr) {
        JsonDocument doc;
        addDeviceInfo(doc);
        
        doc["name"] = name;
        doc["unique_id"] = "brewos_" + deviceId + "_" + String(sensorId);
        doc["object_id"] = "brewos_" + String(sensorId);
        doc["state_topic"] = stateTopic ? stateTopic : statusTopic.c_str();
        doc["value_template"] = valueTemplate;
        if (unit && strlen(unit) > 0) doc["unit_of_measurement"] = unit;
        if (deviceClass && strlen(deviceClass) > 0) doc["device_class"] = deviceClass;
        if (stateClass && strlen(stateClass) > 0) doc["state_class"] = stateClass;
        if (icon) doc["icon"] = icon;
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
    
    // Publish binary sensors for status
    auto publishBinarySensor = [&](const char* name, const char* sensorId, const char* valueTemplate, 
                                   const char* deviceClass, const char* icon = nullptr) {
        JsonDocument doc;
        addDeviceInfo(doc);
        
        doc["name"] = name;
        doc["unique_id"] = "brewos_" + deviceId + "_" + String(sensorId);
        doc["object_id"] = "brewos_" + String(sensorId);
        doc["state_topic"] = statusTopic;
        doc["value_template"] = valueTemplate;
        if (deviceClass && strlen(deviceClass) > 0) {
            doc["device_class"] = deviceClass;
        }
        if (icon) doc["icon"] = icon;
        doc["payload_on"] = "True";
        doc["payload_off"] = "False";
        doc["availability_topic"] = availTopic;
        
        String payload;
        serializeJson(doc, payload);
        
        String configTopic = "homeassistant/binary_sensor/brewos_" + deviceId + "/" + String(sensorId) + "/config";
        _client.publish(configTopic.c_str(), (const uint8_t*)payload.c_str(), payload.length(), true);
    };
    
    // Helper to publish a switch
    auto publishSwitch = [&](const char* name, const char* switchId, const char* icon,
                             const char* payloadOn, const char* payloadOff, 
                             const char* stateTemplate) {
        JsonDocument doc;
        addDeviceInfo(doc);
        
        doc["name"] = name;
        doc["unique_id"] = "brewos_" + deviceId + "_" + String(switchId);
        doc["object_id"] = "brewos_" + String(switchId);
        doc["state_topic"] = statusTopic;
        doc["command_topic"] = commandTopic;
        doc["value_template"] = stateTemplate;
        doc["payload_on"] = payloadOn;
        doc["payload_off"] = payloadOff;
        doc["state_on"] = "ON";
        doc["state_off"] = "OFF";
        doc["icon"] = icon;
        doc["availability_topic"] = availTopic;
        
        String payload;
        serializeJson(doc, payload);
        
        String configTopic = "homeassistant/switch/brewos_" + deviceId + "/" + String(switchId) + "/config";
        _client.publish(configTopic.c_str(), (const uint8_t*)payload.c_str(), payload.length(), true);
    };
    
    // Helper to publish a button
    auto publishButton = [&](const char* name, const char* buttonId, const char* icon,
                             const char* payload) {
        JsonDocument doc;
        addDeviceInfo(doc);
        
        doc["name"] = name;
        doc["unique_id"] = "brewos_" + deviceId + "_" + String(buttonId);
        doc["object_id"] = "brewos_" + String(buttonId);
        doc["command_topic"] = commandTopic;
        doc["payload_press"] = payload;
        doc["icon"] = icon;
        doc["availability_topic"] = availTopic;
        
        String jsonPayload;
        serializeJson(doc, jsonPayload);
        
        String configTopic = "homeassistant/button/brewos_" + deviceId + "/" + String(buttonId) + "/config";
        _client.publish(configTopic.c_str(), (const uint8_t*)jsonPayload.c_str(), jsonPayload.length(), true);
    };
    
    // Helper to publish a number entity
    auto publishNumber = [&](const char* name, const char* numberId, const char* icon,
                             float min, float max, float step, const char* unit,
                             const char* valueTemplate, const char* commandTemplate) {
        JsonDocument doc;
        addDeviceInfo(doc);
        
        doc["name"] = name;
        doc["unique_id"] = "brewos_" + deviceId + "_" + String(numberId);
        doc["object_id"] = "brewos_" + String(numberId);
        doc["state_topic"] = statusTopic;
        doc["command_topic"] = commandTopic;
        doc["value_template"] = valueTemplate;
        doc["command_template"] = commandTemplate;
        doc["min"] = min;
        doc["max"] = max;
        doc["step"] = step;
        doc["unit_of_measurement"] = unit;
        doc["icon"] = icon;
        doc["mode"] = "slider";
        doc["availability_topic"] = availTopic;
        
        String payload;
        serializeJson(doc, payload);
        
        String configTopic = "homeassistant/number/brewos_" + deviceId + "/" + String(numberId) + "/config";
        _client.publish(configTopic.c_str(), (const uint8_t*)payload.c_str(), payload.length(), true);
    };
    
    // Helper to publish a select entity
    auto publishSelect = [&](const char* name, const char* selectId, const char* icon,
                             std::initializer_list<const char*> options, const char* valueTemplate,
                             const char* commandTemplate) {
        JsonDocument doc;
        addDeviceInfo(doc);
        
        doc["name"] = name;
        doc["unique_id"] = "brewos_" + deviceId + "_" + String(selectId);
        doc["object_id"] = "brewos_" + String(selectId);
        doc["state_topic"] = statusTopic;
        doc["command_topic"] = commandTopic;
        doc["value_template"] = valueTemplate;
        doc["command_template"] = commandTemplate;
        doc["icon"] = icon;
        doc["availability_topic"] = availTopic;
        
        JsonArray optionsArr = doc["options"].to<JsonArray>();
        for (const char* opt : options) {
            optionsArr.add(opt);
        }
        
        String payload;
        serializeJson(doc, payload);
        
        String configTopic = "homeassistant/select/brewos_" + deviceId + "/" + String(selectId) + "/config";
        _client.publish(configTopic.c_str(), (const uint8_t*)payload.c_str(), payload.length(), true);
    };
    
    // ==========================================================================
    // TEMPERATURE SENSORS
    // ==========================================================================
    publishSensor("Brew Temperature", "brew_temp", "{{ value_json.brew_temp }}", "°C", "temperature", "measurement");
    publishSensor("Steam Temperature", "steam_temp", "{{ value_json.steam_temp }}", "°C", "temperature", "measurement");
    publishSensor("Brew Setpoint", "brew_setpoint", "{{ value_json.brew_setpoint }}", "°C", "temperature", "measurement");
    publishSensor("Steam Setpoint", "steam_setpoint", "{{ value_json.steam_setpoint }}", "°C", "temperature", "measurement");
    publishSensor("Brew Pressure", "pressure", "{{ value_json.pressure }}", "bar", "pressure", "measurement");
    
    // ==========================================================================
    // SCALE & SHOT SENSORS
    // ==========================================================================
    publishSensor("Scale Weight", "scale_weight", "{{ value_json.scale_weight | default(0) }}", "g", "weight", "measurement", nullptr, "mdi:scale");
    publishSensor("Flow Rate", "flow_rate", "{{ value_json.flow_rate | default(0) }}", "g/s", nullptr, "measurement", nullptr, "mdi:water-outline");
    publishSensor("Shot Duration", "shot_duration", "{{ value_json.shot_duration | default(0) }}", "s", "duration", "measurement", nullptr, "mdi:timer");
    publishSensor("Shot Weight", "shot_weight", "{{ value_json.shot_weight | default(0) }}", "g", "weight", "measurement", nullptr, "mdi:coffee");
    publishSensor("Target Weight", "target_weight", "{{ value_json.target_weight | default(36) }}", "g", "weight", nullptr, nullptr, "mdi:target");
    
    // ==========================================================================
    // STATISTICS SENSORS (use statistics topic)
    // ==========================================================================
    String statisticsTopic = topic("statistics");
    publishSensor("Shots Today", "shots_today", "{{ value_json.shots_today | default(0) }}", "shots", nullptr, "total_increasing", statisticsTopic.c_str(), "mdi:counter");
    publishSensor("Total Shots", "total_shots", "{{ value_json.total_shots | default(0) }}", "shots", nullptr, "total_increasing", statisticsTopic.c_str(), "mdi:coffee-maker");
    publishSensor("Energy Today", "energy_today", "{{ value_json.kwh_today | default(0) }}", "kWh", "energy", "total_increasing", statisticsTopic.c_str(), nullptr);
    
    // ==========================================================================
    // BINARY SENSORS
    // ==========================================================================
    publishBinarySensor("Brewing", "is_brewing", "{{ value_json.is_brewing }}", "running", "mdi:coffee");
    publishBinarySensor("Heating", "is_heating", "{{ value_json.is_heating }}", "heat", nullptr);
    publishBinarySensor("Machine Ready", "ready", "{{ 'True' if value_json.state == 'ready' else 'False' }}", nullptr, "mdi:check-circle");
    publishBinarySensor("Water Low", "water_low", "{{ value_json.water_low }}", "problem", nullptr);
    publishBinarySensor("Alarm", "alarm_active", "{{ value_json.alarm_active }}", "problem", nullptr);
    publishBinarySensor("Pico Connected", "pico_connected", "{{ value_json.pico_connected }}", "connectivity", nullptr);
    publishBinarySensor("Scale Connected", "scale_connected", "{{ value_json.scale_connected }}", "connectivity", "mdi:bluetooth");
    
    // ==========================================================================
    // POWER METER SENSORS
    // ==========================================================================
    publishSensor("Voltage", "voltage", "{{ value_json.voltage }}", "V", "voltage", "measurement", powerTopic.c_str());
    publishSensor("Current", "current", "{{ value_json.current }}", "A", "current", "measurement", powerTopic.c_str());
    publishSensor("Power", "power", "{{ value_json.power }}", "W", "power", "measurement", powerTopic.c_str());
    publishSensor("Energy Import", "energy_import", "{{ value_json.energy_import }}", "kWh", "energy", "total_increasing", powerTopic.c_str());
    publishSensor("Energy Export", "energy_export", "{{ value_json.energy_export }}", "kWh", "energy", "total_increasing", powerTopic.c_str());
    publishSensor("Frequency", "frequency", "{{ value_json.frequency }}", "Hz", "frequency", "measurement", powerTopic.c_str());
    publishSensor("Power Factor", "power_factor", "{{ value_json.power_factor }}", "", "power_factor", "measurement", powerTopic.c_str());
    
    // ==========================================================================
    // SWITCH - Machine Power
    // ==========================================================================
    publishSwitch("Power", "power_switch", "mdi:power",
                  "{\"cmd\":\"set_mode\",\"mode\":\"on\"}",
                  "{\"cmd\":\"set_mode\",\"mode\":\"standby\"}",
                  "{{ 'ON' if value_json.state != 'standby' else 'OFF' }}");
    
    // ==========================================================================
    // BUTTONS - Actions
    // ==========================================================================
    publishButton("Start Brew", "start_brew", "mdi:coffee", "{\"cmd\":\"brew_start\"}");
    publishButton("Stop Brew", "stop_brew", "mdi:stop", "{\"cmd\":\"brew_stop\"}");
    publishButton("Tare Scale", "tare_scale", "mdi:scale-balance", "{\"cmd\":\"tare\"}");
    publishButton("Enter Eco Mode", "enter_eco", "mdi:leaf", "{\"cmd\":\"enter_eco\"}");
    publishButton("Exit Eco Mode", "exit_eco", "mdi:lightning-bolt", "{\"cmd\":\"exit_eco\"}");
    
    // ==========================================================================
    // NUMBER - Configurable values
    // ==========================================================================
    publishNumber("Brew Temperature Target", "brew_temp_target", "mdi:thermometer",
                  85.0, 100.0, 0.5, "°C",
                  "{{ value_json.brew_setpoint }}",
                  "{\"cmd\":\"set_temp\",\"boiler\":\"brew\",\"temp\":{{ value }}}");
    
    publishNumber("Steam Temperature Target", "steam_temp_target", "mdi:thermometer-high",
                  120.0, 160.0, 1.0, "°C",
                  "{{ value_json.steam_setpoint }}",
                  "{\"cmd\":\"set_temp\",\"boiler\":\"steam\",\"temp\":{{ value }}}");
    
    publishNumber("Target Weight", "bbw_target", "mdi:target",
                  18.0, 100.0, 0.5, "g",
                  "{{ value_json.target_weight | default(36) }}",
                  "{\"cmd\":\"set_target_weight\",\"weight\":{{ value }}}");
    
    // ==========================================================================
    // SELECT - Machine Mode
    // ==========================================================================
    publishSelect("Machine Mode", "mode_select", "mdi:coffee-maker-outline",
                  {"standby", "on", "eco"},
                  "{{ value_json.mode | default('standby') }}",
                  "{\"cmd\":\"set_mode\",\"mode\":\"{{ value }}\"}");
    
    // ==========================================================================
    // SELECT - Heating Strategy
    // ==========================================================================
    publishSelect("Heating Strategy", "heating_strategy", "mdi:fire",
                  {"brew_only", "sequential", "parallel", "smart_stagger"},
                  "{% set strategies = ['brew_only', 'sequential', 'parallel', 'smart_stagger'] %}{{ strategies[value_json.heating_strategy | int] | default('sequential') }}",
                  "{% set strategies = {'brew_only': 0, 'sequential': 1, 'parallel': 2, 'smart_stagger': 3} %}{\"cmd\":\"set_heating_strategy\",\"strategy\":{{ strategies[value] | default(1) }}}");
    
    LOG_I("Home Assistant discovery published (%d entities)", HA_TOTAL_ENTITY_COUNT);
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
        LOG_I("Received MQTT command: %s", message);
        
        // Parse JSON command
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, message);
        if (error) {
            LOG_W("Failed to parse MQTT command JSON: %s", error.c_str());
            return;
        }
        
        String cmd = doc["cmd"] | "";
        
        if (cmd == "set_temp") {
            // Set temperature: {"cmd":"set_temp","boiler":"brew","temp":94.0}
            String boiler = doc["boiler"] | "brew";
            float temp = doc["temp"] | 0.0f;
            
            if (temp > 0) {
                if (_commandCallback) {
                    _commandCallback(cmd.c_str(), doc);
                }
                LOG_I("MQTT: Set %s temp to %.1f°C", boiler.c_str(), temp);
            }
        }
        else if (cmd == "set_mode") {
            // Set mode: {"cmd":"set_mode","mode":"standby"}
            String mode = doc["mode"] | "";
            if (mode.length() > 0) {
                if (_commandCallback) {
                    _commandCallback(cmd.c_str(), doc);
                }
                LOG_I("MQTT: Set mode to %s", mode.c_str());
            }
        }
        else if (cmd == "tare") {
            // Tare scale: {"cmd":"tare"}
            if (_commandCallback) {
                _commandCallback(cmd.c_str(), doc);
            }
            LOG_I("MQTT: Tare scale");
        }
        else if (cmd == "set_target_weight") {
            // Set target weight: {"cmd":"set_target_weight","weight":36.0}
            float weight = doc["weight"] | 0.0f;
            if (weight > 0) {
                if (_commandCallback) {
                    _commandCallback(cmd.c_str(), doc);
                }
                LOG_I("MQTT: Set target weight to %.1fg", weight);
            }
        }
        else if (cmd == "set_eco") {
            // Set eco mode config: {"cmd":"set_eco","enabled":true,"brewTemp":80.0,"timeout":30}
            if (_commandCallback) {
                _commandCallback(cmd.c_str(), doc);
            }
            bool enabled = doc["enabled"] | true;
            float brewTemp = doc["brewTemp"] | 80.0f;
            int timeout = doc["timeout"] | 30;
            LOG_I("MQTT: Set eco config: enabled=%d, temp=%.1f°C, timeout=%dmin", 
                  enabled, brewTemp, timeout);
        }
        else if (cmd == "enter_eco") {
            // Enter eco mode: {"cmd":"enter_eco"}
            if (_commandCallback) {
                _commandCallback(cmd.c_str(), doc);
            }
            LOG_I("MQTT: enter_eco");
        }
        else if (cmd == "exit_eco") {
            // Exit eco mode: {"cmd":"exit_eco"}
            if (_commandCallback) {
                _commandCallback(cmd.c_str(), doc);
            }
            LOG_I("MQTT: exit_eco");
        }
        else if (cmd == "brew_start") {
            // Start brewing: {"cmd":"brew_start"}
            if (_commandCallback) {
                _commandCallback(cmd.c_str(), doc);
            }
            LOG_I("MQTT: brew_start");
        }
        else if (cmd == "brew_stop") {
            // Stop brewing: {"cmd":"brew_stop"}
            if (_commandCallback) {
                _commandCallback(cmd.c_str(), doc);
            }
            LOG_I("MQTT: brew_stop");
        }
        else if (cmd == "set_heating_strategy") {
            // Set heating strategy: {"cmd":"set_heating_strategy","strategy":1}
            uint8_t strategy = doc["strategy"] | 1;
            if (strategy <= 3) {
                if (_commandCallback) {
                    _commandCallback(cmd.c_str(), doc);
                }
                LOG_I("MQTT: set_heating_strategy to %d", strategy);
            } else {
                LOG_W("MQTT: Invalid heating strategy: %d", strategy);
            }
        }
        else {
            LOG_W("MQTT: Unknown command: %s", cmd.c_str());
        }
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
        // Use stack buffer to avoid String concatenation in PSRAM
        char buf[32];
        snprintf(buf, sizeof(buf), "Disconnected (%d)", state);
        return String(buf);
    }
    
    return "Connected";
}

void MQTTClient::loadConfig() {
    // After fresh flash, NVS namespace won't exist - this is expected
    if (!_prefs.begin("mqtt", true)) {
        LOG_I("No saved MQTT config (fresh flash) - using defaults");
        _config.enabled = false;
        _config.broker[0] = '\0';
        _config.port = 1883;
        _config.username[0] = '\0';
        _config.password[0] = '\0';
        _config.client_id[0] = '\0';
        strcpy(_config.topic_prefix, "brewos");
        _config.use_tls = false;
        return;
    }
    
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

