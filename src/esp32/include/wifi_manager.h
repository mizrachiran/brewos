#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

// WiFi modes
enum class WiFiManagerMode {
    DISCONNECTED,
    AP_MODE,        // Access Point (setup mode)
    STA_MODE,       // Station (connected to router)
    STA_CONNECTING  // Trying to connect
};

// Static IP configuration
struct StaticIPConfig {
    bool enabled;
    IPAddress ip;
    IPAddress gateway;
    IPAddress subnet;
    IPAddress dns1;
    IPAddress dns2;
};

// WiFi status for web UI
struct WiFiStatus {
    WiFiManagerMode mode;
    String ssid;
    String ip;
    int8_t rssi;
    bool configured;
    // Static IP info
    bool staticIp;
    String gateway;
    String subnet;
    String dns1;
    String dns2;
};

// Time/NTP status
struct TimeStatus {
    bool ntpSynced;
    String currentTime;     // Formatted local time
    String timezone;        // Current timezone description
    int32_t utcOffset;      // Current UTC offset in seconds
};

// Simple function pointer types to avoid std::function PSRAM allocation issues
typedef void (*WiFiEventCallback)();

// WiFi task configuration
#define WIFI_TASK_STACK_SIZE 4096
#define WIFI_TASK_PRIORITY 5
#define WIFI_TASK_CORE 0  // Run on Core 0 with other network tasks

// WiFi command types for the task queue
enum class WiFiCommand {
    CONNECT,           // Connect to stored WiFi
    START_AP,          // Start access point mode
    SET_CREDENTIALS,   // Update credentials
    CLEAR_CREDENTIALS, // Clear stored credentials
    CONFIGURE_NTP,     // Configure NTP settings
    SYNC_NTP           // Sync time with NTP
};

class WiFiManager {
public:
    WiFiManager();
    
    // Initialize - tries STA if configured, falls back to AP
    void begin();
    
    // Update - call in loop
    void loop();
    
    // Configuration
    bool hasStoredCredentials();
    bool checkCredentials();  // Load and check if credentials exist (without starting WiFi)
    bool setCredentials(const char* ssid, const char* password);
    void clearCredentials();
    
    // Static IP configuration
    void setStaticIP(bool enabled, const String& ip = "", const String& gateway = "", 
                     const String& subnet = "255.255.255.0", const String& dns1 = "", const String& dns2 = "");
    StaticIPConfig getStaticIPConfig() const { return _staticIP; }
    
    // Connect to stored WiFi
    bool connectToWiFi();
    
    // Start AP mode (setup)
    void startAP();
    
    // Get current status
    WiFiStatus getStatus();
    WiFiManagerMode getMode() { return _mode; }
    bool isAPMode() { return _mode == WiFiManagerMode::AP_MODE; }
    bool isConnected();
    String getIP();
    
    // Get stored credentials (raw pointers to internal buffers)
    const char* getStoredSSID() const { return _storedSSID; }
    
    // NTP/Time configuration
    void configureNTP(const char* server, int16_t utcOffsetMinutes, bool dstEnabled, int16_t dstOffsetMinutes);
    void syncNTP();
    bool isTimeSynced();
    TimeStatus getTimeStatus();
    time_t getLocalTime();
    String getFormattedTime(const char* format = "%Y-%m-%d %H:%M:%S");
    
    // Events - use simple function pointers to avoid std::function PSRAM issues
    void onConnected(WiFiEventCallback callback) { _onConnected = callback; }
    void onDisconnected(WiFiEventCallback callback) { _onDisconnected = callback; }
    void onAPStarted(WiFiEventCallback callback) { _onAPStarted = callback; }

private:
    WiFiManagerMode _mode;
    Preferences _prefs;
    
    // Fixed-size buffers to avoid String/PSRAM allocations
    char _storedSSID[64];
    char _storedPassword[128];
    
    // Pending credentials (for async set from main loop)
    char _pendingSSID[64];
    char _pendingPassword[128];
    
    // Static IP configuration
    StaticIPConfig _staticIP = {false, IPAddress(), IPAddress(), IPAddress(255,255,255,0), IPAddress(), IPAddress()};
    
    unsigned long _lastConnectAttempt;
    unsigned long _connectStartTime;
    
    // NTP settings
    char _ntpServer[64] = "pool.ntp.org";
    int32_t _utcOffsetSec = 0;
    int32_t _dstOffsetSec = 0;
    bool _ntpConfigured = false;
    
    // Pending NTP config (for async configure from main loop)
    char _pendingNtpServer[64];
    int16_t _pendingUtcOffsetMinutes = 0;
    bool _pendingDstEnabled = false;
    int16_t _pendingDstOffsetMinutes = 0;
    
    // Simple function pointers - no dynamic allocation
    WiFiEventCallback _onConnected = nullptr;
    WiFiEventCallback _onDisconnected = nullptr;
    WiFiEventCallback _onAPStarted = nullptr;
    
    // FreeRTOS task management
    TaskHandle_t _taskHandle = nullptr;
    SemaphoreHandle_t _mutex = nullptr;
    QueueHandle_t _commandQueue = nullptr;
    
    // Internal methods
    void loadCredentials();
    void saveCredentials(const String& ssid, const String& password);
    void loadStaticIPConfig();
    void saveStaticIPConfig();
    
    // Task implementation
    static void taskCode(void* parameter);
    void taskLoop();
    void processCommand(WiFiCommand cmd);
    void doConnectToWiFi();
    void doStartAP();
    void doConfigureNTP();
    void doSyncNTP();
};

#endif // WIFI_MANAGER_H
