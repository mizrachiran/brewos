/**
 * BrewOS Scale Manager
 * 
 * Manages BLE scale discovery, connection, and communication.
 * Automatically detects scale type and uses appropriate driver.
 * 
 * Supported scales:
 * - Acaia (Lunar, Pearl, Pyxis, Cinco, Orion)
 * - Bookoo (Themis Mini, Themis Ultra)
 * - Felicita (Arc, Parallel, Incline)
 * - Decent Scale
 * - Timemore (Black Mirror, Basic)
 * - Hiroia (Jimmy)
 * - Generic BLE Weight Scale Service
 */

#ifndef SCALE_MANAGER_H
#define SCALE_MANAGER_H

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <vector>
#include "scale_interface.h"

// =============================================================================
// Configuration
// =============================================================================

#define SCALE_MAX_DISCOVERED     10      // Max scales to track during scan
#define SCALE_SCAN_DURATION_MS   10000   // Default scan duration
#define SCALE_RECONNECT_DELAY_MS 5000    // Delay between reconnect attempts
#define SCALE_WEIGHT_TIMEOUT_MS  3000    // Consider disconnected if no weight update

// NVS storage keys
#define NVS_SCALE_NAMESPACE      "scale"
#define NVS_SCALE_ADDRESS        "address"
#define NVS_SCALE_TYPE           "type"
#define NVS_SCALE_NAME           "name"

// =============================================================================
// Scale Manager Class
// =============================================================================

class ScaleManager : public NimBLEClientCallbacks, public NimBLEAdvertisedDeviceCallbacks {
public:
    ScaleManager();
    ~ScaleManager();
    
    /**
     * Initialize the scale manager
     * Starts BLE and loads saved scale from NVS
     */
    bool begin();
    
    /**
     * Shutdown and cleanup
     */
    void end();
    
    /**
     * Process - call in main loop
     */
    void loop();
    
    // =========================================================================
    // Scanning
    // =========================================================================
    
    /**
     * Start scanning for BLE scales
     * @param duration_ms Scan duration (0 = use default)
     */
    void startScan(uint32_t duration_ms = 0);
    
    /**
     * Stop scanning
     */
    void stopScan();
    
    /**
     * Check if currently scanning
     */
    bool isScanning() const { return _scanning; }
    
    /**
     * Get discovered scales
     */
    const std::vector<scale_info_t>& getDiscoveredScales() const { return _discoveredScales; }
    
    /**
     * Clear discovered scales list
     */
    void clearDiscovered() { _discoveredScales.clear(); }
    
    // =========================================================================
    // Connection
    // =========================================================================
    
    /**
     * Connect to a scale by address
     * @param address BLE MAC address (or nullptr to use saved)
     */
    bool connect(const char* address = nullptr);
    
    /**
     * Connect to a discovered scale by index
     */
    bool connectByIndex(int index);
    
    /**
     * Disconnect from current scale
     */
    void disconnect();
    
    /**
     * Check if connected
     */
    bool isConnected() const { return _connected; }
    
    /**
     * Forget saved scale (clear NVS)
     */
    void forgetScale();
    
    // =========================================================================
    // Scale Operations
    // =========================================================================
    
    /**
     * Get current scale state
     */
    scale_state_t getState() const { return _state; }
    
    /**
     * Tare the scale
     */
    void tare();
    
    /**
     * Start timer on scale (if supported)
     */
    void startTimer();
    
    /**
     * Stop timer on scale
     */
    void stopTimer();
    
    /**
     * Reset timer on scale
     */
    void resetTimer();
    
    /**
     * Get connected scale type
     */
    scale_type_t getScaleType() const { return _scaleType; }
    
    /**
     * Get connected scale name
     */
    const char* getScaleName() const { return _scaleName; }
    
    // =========================================================================
    // Callbacks
    // =========================================================================
    
    // Simple function pointers to avoid std::function PSRAM allocation issues
    typedef void (*weight_callback_t)(const scale_state_t&);
    typedef void (*connection_callback_t)(bool connected);
    typedef void (*discovery_callback_t)(const scale_info_t&);
    
    void onWeight(weight_callback_t cb) { _weightCallback = cb; }
    void onConnection(connection_callback_t cb) { _connectionCallback = cb; }
    void onDiscovery(discovery_callback_t cb) { _discoveryCallback = cb; }

private:
    // BLE components
    NimBLEClient* _client;
    NimBLEScan* _scan;
    
    // State
    bool _initialized;
    bool _scanning;
    bool _connected;
    bool _connecting;
    scale_state_t _state;
    scale_type_t _scaleType;
    char _scaleName[32];
    char _scaleAddress[18];
    
    // Discovered scales
    std::vector<scale_info_t> _discoveredScales;
    
    // Timing
    uint32_t _lastWeightTime;
    uint32_t _lastReconnectAttempt;
    bool _autoReconnect;
    
    // Flow rate calculation
    float _lastWeight;
    uint32_t _lastFlowTime;
    
    // Callbacks
    weight_callback_t _weightCallback;
    connection_callback_t _connectionCallback;
    discovery_callback_t _discoveryCallback;
    
    // BLE characteristics (set based on scale type)
    NimBLERemoteCharacteristic* _weightChar;
    NimBLERemoteCharacteristic* _commandChar;
    
    // NimBLE callbacks
    void onConnect(NimBLEClient* client) override;
    void onDisconnect(NimBLEClient* client) override;
    void onResult(NimBLEAdvertisedDevice* device) override;
    
    // Internal methods
    void loadSavedScale();
    void saveScale();
    bool connectToDevice(NimBLEAdvertisedDevice* device);
    bool setupCharacteristics();
    void processWeightData(const uint8_t* data, size_t length);
    void updateFlowRate(float weight);
    
    // Scale-specific handlers
    bool setupAcaia();
    bool setupFelicita();
    bool setupDecent();
    bool setupTimemore();
    bool setupBookoo();
    bool setupGenericWSS();
    
    void parseAcaiaWeight(const uint8_t* data, size_t length);
    void parseFelicitaWeight(const uint8_t* data, size_t length);
    void parseDecentWeight(const uint8_t* data, size_t length);
    void parseTimemoreWeight(const uint8_t* data, size_t length);
    void parseBookooWeight(const uint8_t* data, size_t length);
    void parseGenericWeight(const uint8_t* data, size_t length);
    
    void sendAcaiaTare();
    void sendFelicitaTare();
    void sendDecentTare();
    void sendTimemoreTare();
    void sendBookooTare();
    
    // Notification callback wrapper
    static void notifyCallback(NimBLERemoteCharacteristic* chr, uint8_t* data, 
                               size_t length, bool isNotify);
    static ScaleManager* _instance;
};

// Global instance
extern ScaleManager* scaleManager;

#endif // SCALE_MANAGER_H

