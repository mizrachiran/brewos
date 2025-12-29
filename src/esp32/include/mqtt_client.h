/**
 * BrewOS MQTT Client
 * 
 * Handles MQTT connection, publishing status, and subscribing to commands
 * Supports Home Assistant auto-discovery
 */

#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

// MQTT task configuration
#define MQTT_TASK_STACK_SIZE 4096
#define MQTT_TASK_PRIORITY 3  // Lower than WiFi task
#define MQTT_TASK_CORE 0      // Run on Core 0 with other network tasks

// Include ui_state_t definition
#include "ui/ui.h"

// Include power meter types
#include "power_meter/power_meter.h"

// =============================================================================
// MQTT Configuration Structure
// =============================================================================

struct MQTTConfig {
    bool enabled;
    char broker[64];
    uint16_t port;
    char username[32];
    char password[64];
    char client_id[32];
    char topic_prefix[32];
    bool use_tls;
    bool ha_discovery;
    char ha_device_id[32];
    
    // Default constructor
    MQTTConfig() {
        enabled = false;
        strcpy(broker, "");
        port = 1883;
        strcpy(username, "");
        strcpy(password, "");
        strcpy(client_id, "");
        strcpy(topic_prefix, "brewos");
        use_tls = false;
        ha_discovery = true;
        strcpy(ha_device_id, "");
    }
};

// =============================================================================
// MQTT Client Class
// =============================================================================

class MQTTClient {
public:
    MQTTClient();
    
    /**
     * Initialize MQTT client
     * Loads configuration from NVS
     */
    bool begin();
    
    /**
     * Update - call in main loop
     */
    void loop();
    
    /**
     * Get current configuration
     */
    MQTTConfig getConfig() const { return _config; }
    
    /**
     * Set configuration and save to NVS
     */
    bool setConfig(const MQTTConfig& config);
    
    /**
     * Test connection to MQTT broker using current config
     */
    bool testConnection();
    
    /**
     * Test connection with temporary config (doesn't modify permanent config)
     * Returns: 0 = success, 1 = broker empty, 2 = WiFi not connected, 3 = connection failed
     */
    int testConnectionWithConfig(const MQTTConfig& testConfig);
    
    /**
     * Check if connected to broker
     */
    bool isConnected() const { return _connected; }
    
    /**
     * Enable/disable MQTT (for OTA updates)
     * When disabled, disconnects and prevents reconnection
     */
    void setEnabled(bool enabled);
    
    /**
     * Publish machine status
     */
    void publishStatus(const ui_state_t& state);
    
    /**
     * Publish shot data
     */
    void publishShot(const char* shot_json);
    
    /**
     * Publish statistics as raw JSON (legacy)
     */
    void publishStatisticsJson(const char* stats_json);
    
    /**
     * Publish power meter data
     */
    void publishPowerMeter(const PowerMeterReading& reading);
    
    /**
     * Publish statistics (shots, energy, etc.)
     * @param shotsToday Daily shot count
     * @param totalShots Lifetime shot count
     * @param kwhToday Daily energy usage
     */
    void publishStatistics(uint16_t shotsToday, uint32_t totalShots, float kwhToday);
    
    /**
     * Get connection status string
     */
    String getStatusString() const;
    
    /**
     * Events - Simple function pointers to avoid std::function PSRAM allocation issues
     */
    typedef void (*mqtt_event_callback_t)();
    typedef void (*mqtt_command_callback_t)(const char* cmd, const JsonDocument& doc);
    
    void onConnected(mqtt_event_callback_t callback) { _onConnected = callback; }
    void onDisconnected(mqtt_event_callback_t callback) { _onDisconnected = callback; }
    
    /**
     * Command callback - called when a command is received via MQTT
     * @param cmd Command name (e.g., "set_temp", "set_mode", "tare")
     * @param doc JSON document with command parameters
     */
    void onCommand(mqtt_command_callback_t callback) { _commandCallback = callback; }
    
private:
    WiFiClient _wifiClient;
    PubSubClient _client;
    Preferences _prefs;
    MQTTConfig _config;
    
    // Connection state
    bool _connected;
    bool _wasConnected;          // Track previous state for disconnect detection
    unsigned long _lastReconnectAttempt;
    unsigned long _lastStatusPublish;
    int _reconnectDelay;
    
    // Callbacks - simple function pointers
    mqtt_event_callback_t _onConnected = nullptr;
    mqtt_event_callback_t _onDisconnected = nullptr;
    mqtt_command_callback_t _commandCallback = nullptr;
    
    // FreeRTOS task management
    TaskHandle_t _taskHandle = nullptr;
    SemaphoreHandle_t _mutex = nullptr;
    volatile bool _taskRunning = false;
    
    // Internal methods
    void loadConfig();
    void saveConfig();
    bool connect();
    void disconnect();
    void publishHomeAssistantDiscovery();
    void publishAvailability(bool online);
    void onMessage(char* topic, byte* payload, unsigned int length);
    
    // Static callback wrapper
    static void messageCallback(char* topic, byte* payload, unsigned int length);
    static MQTTClient* _instance;
    
    // Topic builders
    String topic(const char* suffix) const;
    String haTopic(const char* component, const char* object) const;
    
    // Generate device ID from MAC address
    void generateDeviceID();
    
    // FreeRTOS task
    static void taskCode(void* parameter);
    void taskLoop();
};

// Global instance
// extern MQTTClient mqttClient;  // Commented out - now using pointer in main.cpp

#endif // MQTT_CLIENT_H
