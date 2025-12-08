#ifndef PAIRING_MANAGER_H
#define PAIRING_MANAGER_H

#include <Arduino.h>
#include <functional>

/**
 * PairingManager handles device pairing with the cloud service.
 * It generates claim tokens and QR codes for users to scan.
 * 
 * Also manages the device key - a secret generated on first boot that
 * authenticates WebSocket connections to the cloud.
 */
class PairingManager {
public:
    PairingManager();
    
    /**
     * Initialize the pairing manager
     * @param cloudUrl The base URL of the cloud service (e.g., "https://brewos.io")
     */
    void begin(const String& cloudUrl = "");
    
    /**
     * Generate a new pairing token
     * The token is valid for 10 minutes
     * @return The generated claim token
     */
    String generateToken();
    
    /**
     * Get the current pairing URL for QR code
     * Format: https://brewos.io/pair?id=DEVICE_ID&token=TOKEN
     * @return The pairing URL
     */
    String getPairingUrl() const;
    
    /**
     * Get the device ID
     * Format: BRW-XXXXXXXX (based on chip ID)
     */
    String getDeviceId() const;
    
    /**
     * Get the device key for cloud authentication
     * Generated on first boot and persisted in NVS
     */
    String getDeviceKey() const;
    
    /**
     * Get the current claim token
     */
    String getCurrentToken() const;
    
    /**
     * Check if current token is still valid
     */
    bool isTokenValid() const;
    
    /**
     * Get token expiry time (Unix timestamp)
     */
    unsigned long getTokenExpiry() const;
    
    /**
     * Register the claim token with the cloud service
     * Also sends the device key for authentication setup
     * @return true if successful
     */
    bool registerTokenWithCloud();
    
    /**
     * Set callback for when pairing is successful
     */
    void onPairingSuccess(std::function<void(const String& userId)> callback);
    
    /**
     * Called by cloud connection when device is claimed
     */
    void notifyPairingSuccess(const String& userId);

private:
    String _cloudUrl;
    String _deviceId;
    String _deviceKey;
    String _currentToken;
    unsigned long _tokenExpiry;
    std::function<void(const String&)> _onPairingSuccess;
    
    String generateRandomToken(size_t length = 32);
    void initDeviceId();
    void initDeviceKey();
};

#endif // PAIRING_MANAGER_H

