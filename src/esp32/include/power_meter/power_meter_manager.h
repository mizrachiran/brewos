/**
 * Power Meter Manager (ESP32)
 * 
 * Manages power meter data from two sources:
 * 1. Hardware meters (PZEM, JSY, Eastron) - Connected to Pico via UART1, data forwarded from Pico
 * 2. MQTT sources (Shelly, Tasmota) - Handled directly by ESP32
 * 
 * User selects one source via webapp configuration
 */

#ifndef POWER_METER_MANAGER_H
#define POWER_METER_MANAGER_H

#include "power_meter.h"
#include <ArduinoJson.h>

// Forward declarations
class MQTTPowerMeter;

// Discovery status
struct DiscoveryStatus {
    bool discovering;
    int currentStep;
    int totalSteps;
    const char* currentAction;
    const char* discoveredMeter;
};

class PowerMeterManager {
public:
    PowerMeterManager();
    ~PowerMeterManager();
    
    // Lifecycle
    void begin();
    void loop();
    
    // Configuration
    bool setSource(PowerMeterSource source);
    bool configureHardware();  // Hardware meters configured on Pico
    bool configureMqtt(const char* topic, const char* format = "auto");
    PowerMeterSource getSource() const { return _source; }
    
    // Auto-discovery for hardware meters (forwarded to Pico)
    void startAutoDiscovery();
    bool isDiscovering() const { return _autoDiscovering; }
    DiscoveryStatus getDiscoveryStatus() const;
    
    // Handle power data from Pico (hardware meter readings)
    void onPicoPowerData(const PowerMeterReading& reading);
    
    // Data access
    bool getReading(PowerMeterReading& reading);
    bool isConnected() const;
    const char* getMeterName() const;
    const char* getLastError() const;
    
    // Status for WebSocket/MQTT publishing
    void getStatus(JsonDocument& doc);
    
    // Save/load configuration
    bool saveConfig();
    bool loadConfig();
    
private:
    PowerMeterSource _source;
    PowerMeterReading _lastReading;
    uint32_t _lastReadTime;
    
    // MQTT meter (only MQTT handled by ESP32, hardware is on Pico)
    MQTTPowerMeter* _mqttMeter;
    
    // Auto-discovery state
    bool _autoDiscovering;
    int _discoveryStep;
    uint32_t _discoveryStepStartTime;
    
    // Helper methods
    void cleanupMeter();
    void performDiscoveryStep();
    
    // Polling interval (don't query too frequently)
    static constexpr uint32_t POLL_INTERVAL_MS = 1000;
    uint32_t _lastPollTime;
};

// Global instance
extern PowerMeterManager powerMeterManager;

#endif // POWER_METER_MANAGER_H

