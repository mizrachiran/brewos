/**
 * MQTT Power Meter Driver
 * 
 * Receives power data from MQTT topics (Shelly, Tasmota, generic smart plugs)
 */

#ifndef MQTT_POWER_METER_H
#define MQTT_POWER_METER_H

#include "power_meter.h"
#include <ArduinoJson.h>

// Supported MQTT formats
enum class MqttFormat {
    AUTO,      // Try to auto-detect
    SHELLY,    // Shelly Plug format
    TASMOTA,   // Tasmota SENSOR format
    GENERIC    // User-configured JSON paths
};

class MQTTPowerMeter : public IPowerMeter {
public:
    MQTTPowerMeter(const char* topic, const char* format = "auto");
    ~MQTTPowerMeter();
    
    // IPowerMeter interface
    bool begin() override;
    void loop() override;
    bool read(PowerMeterReading& reading) override;
    const char* getName() const override;
    PowerMeterSource getSource() const override { return PowerMeterSource::MQTT; }
    bool isConnected() const override;
    const char* getLastError() const override { return _lastError; }
    
    // MQTT callback - called when data arrives
    void onMqttData(const char* payload, size_t length);
    void onMqttData(JsonDocument& doc);
    
    // Configuration
    const char* getTopic() const { return _topic.c_str(); }
    const char* getFormat() const;
    void setJsonPaths(const char* power, const char* voltage, 
                     const char* current, const char* energy);
    
    // Timeout detection
    bool isStale() const;
    
private:
    String _topic;
    MqttFormat _format;
    PowerMeterReading _lastReading;
    uint32_t _lastUpdateTime;
    bool _hasData;
    char _lastError[64];
    
    // Custom JSON paths for GENERIC format
    String _jsonPathPower;
    String _jsonPathVoltage;
    String _jsonPathCurrent;
    String _jsonPathEnergy;
    
    // Parse different formats
    bool parseShelly(JsonDocument& doc);
    bool parseTasmota(JsonDocument& doc);
    bool parseGeneric(JsonDocument& doc);
    bool tryAutoParse(JsonDocument& doc);
    
    // JSON path extraction helper
    bool extractJsonValue(JsonDocument& doc, const String& path, float& value);
    
    // Timeout threshold
    static constexpr uint32_t STALE_THRESHOLD_MS = 10000;
};

#endif // MQTT_POWER_METER_H

