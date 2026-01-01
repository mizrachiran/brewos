#ifndef CLOUD_CONNECTION_H
#define CLOUD_CONNECTION_H

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

/**
 * CloudConnection
 * 
 * WebSocket client that maintains a persistent connection to the BrewOS cloud.
 * Enables remote access: cloud users receive real-time state updates and can
 * send commands to the device.
 * 
 * Protocol:
 * - Connect to: wss://cloud.server/ws/device?id=DEVICE_ID&key=DEVICE_KEY
 * - Messages are JSON with { type: "...", ... } format
 * - Receives commands from cloud users (forwarded to command handler)
 * - Sends state updates to cloud (broadcast to all connected cloud users)
 */
class CloudConnection {
public:
    // Command handler callback - receives commands from cloud users
    // Simple function pointer to avoid std::function PSRAM allocation issues
    // Uses const char* instead of String to avoid heap fragmentation
    typedef void (*CommandCallback)(const char* type, JsonDocument& doc);
    
    // Registration callback - called before first connect to register device with cloud
    typedef bool (*RegisterCallback)();
    
    // Regenerate key callback - called when authentication fails to recover
    typedef bool (*RegenerateKeyCallback)();
    
    CloudConnection();
    
    /**
     * Initialize cloud connection
     * @param serverUrl Cloud server URL (e.g., "https://cloud.brewos.io")
     * @param deviceId Device identifier (e.g., "BRW-XXXXXXXX")
     * @param deviceKey Secret key for authentication
     */
    void begin(const String& serverUrl, const String& deviceId, const String& deviceKey);
    
    /**
     * Disconnect and disable cloud connection
     */
    void end();
    
    /**
     * Call in loop() - handles reconnection and message processing
     */
    void loop();
    
    /**
     * Send JSON message to cloud (broadcast to all connected cloud users)
     */
    void send(const String& json);
    void send(const char* json);  // Overload to avoid String allocation
    
    /**
     * Send typed message to cloud
     */
    void send(const JsonDocument& doc);
    
    /**
     * Send binary MessagePack data to cloud
     */
    void sendBinary(const uint8_t* data, size_t len);
    
    /**
     * Set callback for receiving commands from cloud users
     */
    void onCommand(CommandCallback callback);
    
    /**
     * Set callback for registering device with cloud before connecting
     * Called once before first connection attempt (when WiFi is available)
     */
    void onRegister(RegisterCallback callback);
    
    /**
     * Set callback for regenerating device key when authentication fails
     * Called when connection is rejected due to invalid credentials
     */
    void onRegenerateKey(RegenerateKeyCallback callback);
    
    /**
     * Check if connected to cloud
     */
    bool isConnected() const;
    
    /**
     * Get connection status string
     */
    String getStatus() const;
    
    /**
     * Enable/disable connection (without clearing config)
     */
    void setEnabled(bool enabled);
    
    /**
     * Check if cloud is enabled
     */
    bool isEnabled() const;
    
    /**
     * Pause connection to free up resources (e.g. for Web UI loading)
     * Cloud will auto-resume after 30 seconds
     */
    void pause();
    
    /**
     * Resume connection immediately (e.g. when local WebSocket client disconnects)
     */
    void resume();
    
    /**
     * Notify of user activity - defers reconnection attempts to keep UI responsive
     * Call this when encoder/button events occur
     */
    void notifyUserActivity();
    
    /**
     * Cancel pending proactive state broadcast
     * Call when server sends request_state to avoid duplicate broadcast
     */
    void cancelPendingStateBroadcast();

private:
    WebSocketsClient _ws;
    String _serverUrl;
    String _deviceId;
    String _deviceKey;
    bool _enabled = false;
    bool _connected = false;
    bool _connecting = false;
    unsigned long _lastConnectAttempt = 0;
    unsigned long _reconnectDelay = 5000;  // 5 seconds between retries
    unsigned long _connectedAt = 0;        // Track when connection established for grace period
    unsigned long _lastDisconnectTime = 0; // Track disconnect time to detect auth failures
    int _authFailureCount = 0;             // Track consecutive auth failures
    
    CommandCallback _onCommand = nullptr;
    RegisterCallback _onRegister = nullptr;
    RegenerateKeyCallback _onRegenerateKey = nullptr;
    bool _registered = false;
    unsigned long _lastUserActivity = 0;  // For deferring connection during user interaction
    unsigned long _pausedUntil = 0;       // For pausing connection during web server activity
    int _failureCount = 0;                // Track consecutive failures for backoff
    
    // Initial state broadcast after cloud connection
    bool _pendingInitialStateBroadcast = false;
    unsigned long _initialStateBroadcastTime = 0;
    
    // FreeRTOS task for non-blocking operation
    TaskHandle_t _taskHandle = nullptr;
    static void taskCode(void* parameter);
    
    // Thread safety - WebSocketsClient is NOT thread-safe
    SemaphoreHandle_t _mutex = nullptr;
    QueueHandle_t _sendQueue = nullptr;  // Queue of messages to send
    static const int SEND_QUEUE_SIZE = 20;  // Increased to handle rapid state updates
    static const int MAX_MSG_SIZE = 2048;   // Reduced message size
    
    // Process queued messages (called from task)
    void processSendQueue();
    
    // Parse URL into host, port, path
    bool parseUrl(const String& url, String& host, uint16_t& port, String& path, bool& useSSL);
    
    // WebSocket event handler
    void handleEvent(WStype_t type, uint8_t* payload, size_t length);
    
    // Process incoming message
    void handleMessage(uint8_t* payload, size_t length);
    
    // Attempt to connect
    void connect();
};

#endif // CLOUD_CONNECTION_H

