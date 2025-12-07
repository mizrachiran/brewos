/**
 * MQTT Power Meter Implementation
 */

#include "power_meter/mqtt_power_meter.h"
#include "config.h"

MQTTPowerMeter::MQTTPowerMeter(const char* topic, const char* format)
    : _topic(topic)
    , _lastUpdateTime(0)
    , _hasData(false)
{
    _lastError[0] = '\0';
    memset(&_lastReading, 0, sizeof(_lastReading));
    
    // Parse format
    if (strcmp(format, "shelly") == 0) {
        _format = MqttFormat::SHELLY;
    } else if (strcmp(format, "tasmota") == 0) {
        _format = MqttFormat::TASMOTA;
    } else if (strcmp(format, "generic") == 0) {
        _format = MqttFormat::GENERIC;
    } else {
        _format = MqttFormat::AUTO;
    }
}

MQTTPowerMeter::~MQTTPowerMeter() {
    // Nothing to clean up
}

bool MQTTPowerMeter::begin() {
    LOG_I("MQTT power meter initialized: topic=%s, format=%s", 
          _topic.c_str(), getFormat());
    return true;
}

void MQTTPowerMeter::loop() {
    // No polling needed - data comes via callback
}

bool MQTTPowerMeter::read(PowerMeterReading& reading) {
    if (!_hasData || isStale()) {
        return false;
    }
    
    reading = _lastReading;
    return true;
}

const char* MQTTPowerMeter::getName() const {
    return _topic.c_str();
}

bool MQTTPowerMeter::isConnected() const {
    return _hasData && !isStale();
}

const char* MQTTPowerMeter::getFormat() const {
    switch (_format) {
        case MqttFormat::SHELLY: return "shelly";
        case MqttFormat::TASMOTA: return "tasmota";
        case MqttFormat::GENERIC: return "generic";
        case MqttFormat::AUTO: return "auto";
        default: return "unknown";
    }
}

bool MQTTPowerMeter::isStale() const {
    return (millis() - _lastUpdateTime) > STALE_THRESHOLD_MS;
}

void MQTTPowerMeter::setJsonPaths(const char* power, const char* voltage, 
                                  const char* current, const char* energy) {
    _jsonPathPower = power ? power : "";
    _jsonPathVoltage = voltage ? voltage : "";
    _jsonPathCurrent = current ? current : "";
    _jsonPathEnergy = energy ? energy : "";
    _format = MqttFormat::GENERIC;
}

void MQTTPowerMeter::onMqttData(const char* payload, size_t length) {
    // Parse JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    
    if (error) {
        snprintf(_lastError, sizeof(_lastError), "JSON parse error: %s", error.c_str());
        LOG_W("MQTT power meter JSON parse error: %s", error.c_str());
        return;
    }
    
    onMqttData(doc);
}

void MQTTPowerMeter::onMqttData(JsonDocument& doc) {
    bool parsed = false;
    
    // Try format-specific parsing
    switch (_format) {
        case MqttFormat::SHELLY:
            parsed = parseShelly(doc);
            break;
        case MqttFormat::TASMOTA:
            parsed = parseTasmota(doc);
            break;
        case MqttFormat::GENERIC:
            parsed = parseGeneric(doc);
            break;
        case MqttFormat::AUTO:
            parsed = tryAutoParse(doc);
            break;
    }
    
    if (parsed) {
        _lastReading.timestamp = millis();
        _lastReading.valid = true;
        _lastUpdateTime = millis();
        _hasData = true;
        _lastError[0] = '\0';
    } else {
        snprintf(_lastError, sizeof(_lastError), "Failed to parse MQTT data");
    }
}

bool MQTTPowerMeter::parseShelly(JsonDocument& doc) {
    // Shelly Plug format:
    // {
    //   "wifi_sta": {...},
    //   "cloud": {...},
    //   "mqtt": {...},
    //   "time": "...",
    //   "serial": ...,
    //   "has_update": false,
    //   "mac": "...",
    //   "relays": [{...}],
    //   "meters": [{
    //     "power": 123.45,
    //     "overpower": 0,
    //     "is_valid": true,
    //     "timestamp": ...,
    //     "counters": [1234, 5678, ...],
    //     "total": 1234
    //   }],
    //   "temperature": 25.67,
    //   "overtemperature": false,
    //   "tmp": {...}
    // }
    
    if (doc.containsKey("meters") && doc["meters"].is<JsonArray>()) {
        JsonArray meters = doc["meters"].as<JsonArray>();
        if (meters.size() > 0) {
            JsonObject meter = meters[0].as<JsonObject>();
            
            if (meter.containsKey("power")) {
                _lastReading.power = meter["power"].as<float>();
            }
            if (meter.containsKey("total")) {
                _lastReading.energy_import = meter["total"].as<float>() / 60.0f / 1000.0f;  // Watt-minutes to kWh
            }
            
            // Shelly doesn't provide voltage/current in status, estimate from power
            // Assume 230V for European plugs
            _lastReading.voltage = 230.0f;
            if (_lastReading.power > 0 && _lastReading.voltage > 0) {
                _lastReading.current = _lastReading.power / _lastReading.voltage;
            }
            
            return true;
        }
    }
    
    return false;
}

bool MQTTPowerMeter::parseTasmota(JsonDocument& doc) {
    // Tasmota SENSOR format:
    // {
    //   "Time": "2024-01-01T00:00:00",
    //   "ENERGY": {
    //     "TotalStartTime": "2024-01-01T00:00:00",
    //     "Total": 12.345,
    //     "Yesterday": 1.234,
    //     "Today": 0.123,
    //     "Power": 123,
    //     "ApparentPower": 125,
    //     "ReactivePower": 20,
    //     "Factor": 0.98,
    //     "Voltage": 230,
    //     "Current": 0.536
    //   }
    // }
    
    if (doc.containsKey("ENERGY")) {
        JsonObject energy = doc["ENERGY"].as<JsonObject>();
        
        if (energy.containsKey("Power")) {
            _lastReading.power = energy["Power"].as<float>();
        }
        if (energy.containsKey("Voltage")) {
            _lastReading.voltage = energy["Voltage"].as<float>();
        }
        if (energy.containsKey("Current")) {
            _lastReading.current = energy["Current"].as<float>();
        }
        if (energy.containsKey("Total")) {
            _lastReading.energy_import = energy["Total"].as<float>();
        }
        if (energy.containsKey("Factor")) {
            _lastReading.power_factor = energy["Factor"].as<float>();
        }
        
        _lastReading.frequency = 50.0f;  // Assume 50Hz
        
        return true;
    }
    
    return false;
}

bool MQTTPowerMeter::parseGeneric(JsonDocument& doc) {
    bool success = false;
    
    if (_jsonPathPower.length() > 0) {
        success |= extractJsonValue(doc, _jsonPathPower, _lastReading.power);
    }
    if (_jsonPathVoltage.length() > 0) {
        success |= extractJsonValue(doc, _jsonPathVoltage, _lastReading.voltage);
    }
    if (_jsonPathCurrent.length() > 0) {
        success |= extractJsonValue(doc, _jsonPathCurrent, _lastReading.current);
    }
    if (_jsonPathEnergy.length() > 0) {
        success |= extractJsonValue(doc, _jsonPathEnergy, _lastReading.energy_import);
    }
    
    return success;
}

bool MQTTPowerMeter::tryAutoParse(JsonDocument& doc) {
    // Try Shelly format first
    if (parseShelly(doc)) {
        _format = MqttFormat::SHELLY;
        LOG_I("Auto-detected Shelly format");
        return true;
    }
    
    // Try Tasmota format
    if (parseTasmota(doc)) {
        _format = MqttFormat::TASMOTA;
        LOG_I("Auto-detected Tasmota format");
        return true;
    }
    
    // Try simple direct fields
    bool found = false;
    if (doc.containsKey("power")) {
        _lastReading.power = doc["power"].as<float>();
        found = true;
    }
    if (doc.containsKey("voltage")) {
        _lastReading.voltage = doc["voltage"].as<float>();
        found = true;
    }
    if (doc.containsKey("current")) {
        _lastReading.current = doc["current"].as<float>();
        found = true;
    }
    if (doc.containsKey("energy")) {
        _lastReading.energy_import = doc["energy"].as<float>();
        found = true;
    }
    
    if (found) {
        LOG_I("Auto-detected simple JSON format");
        return true;
    }
    
    return false;
}

bool MQTTPowerMeter::extractJsonValue(JsonDocument& doc, const String& path, float& value) {
    if (path.length() == 0) {
        return false;
    }
    
    // Simple path support (no nested paths for now - just top-level keys)
    if (doc.containsKey(path.c_str())) {
        value = doc[path.c_str()].as<float>();
        return true;
    }
    
    return false;
}

