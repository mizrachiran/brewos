#ifndef PICO_UART_H
#define PICO_UART_H

#include <Arduino.h>

// Packet structure
struct PicoPacket {
    uint8_t type;
    uint8_t length;
    uint8_t seq;
    uint8_t payload[56];
    uint16_t crc;
    bool valid;
};

// Simple function pointer callback to avoid std::function PSRAM allocation issues
typedef void (*PacketCallback)(const PicoPacket& packet);

class PicoUART {
public:
    PicoUART(HardwareSerial& serial);
    
    // Initialize UART
    void begin();
    
    // Update - call in loop, processes incoming data
    void loop();
    
    // Send packet to Pico
    bool sendPacket(uint8_t type, const uint8_t* payload, uint8_t length);
    
    // Convenience methods
    bool sendPing();
    bool sendCommand(uint8_t cmdType, const uint8_t* data, uint8_t len);
    bool requestConfig();
    bool requestBootInfo();  // Request boot info (version, machine type)
    bool sendHandshake();    // Send protocol handshake
    
    // OTA control
    bool enterBootloader();
    void resetPico();
    void holdBootsel(bool hold);
    
    // Firmware streaming (for bootloader mode)
    size_t streamFirmwareChunk(const uint8_t* data, size_t len, uint32_t chunkNumber);
    bool waitForBootloaderAck(uint32_t timeoutMs = 1000);  // Wait for 0xAA 0x55 ACK from bootloader
    
    // Pause/resume packet processing (for OTA)
    void pause() { _paused = true; }    // Stop loop() from processing packets
    void resume() { _paused = false; _rxState = RxState::WAIT_START; }  // Resume packet processing
    bool isPaused() { return _paused; }
    
    // Brew-by-weight control
    void setWeightStop(bool active);       // Set WEIGHT_STOP signal (HIGH = stop brew)
    
    // Set callback for received packets - uses simple function pointer
    void onPacket(PacketCallback callback) { _packetCallback = callback; }
    
    // Statistics
    uint32_t getPacketsReceived() { return _packetsReceived; }
    uint32_t getPacketErrors() { return _packetErrors; }
    bool isConnected() { return _connected; }
    int bytesAvailable() { return Serial1.available(); }  // Check if any raw bytes are available
    
    // Reset connection state (used during OTA to properly detect reconnection)
    void clearConnectionState() { _connected = false; _lastPacketTime = 0; }
    
    // Non-blocking backoff for NACK handling (avoids blocking delay() in main loop)
    void setBackoffUntil(uint32_t timestamp) { _backoffUntil = timestamp; }
    bool isInBackoff() { return millis() < _backoffUntil; }

private:
    HardwareSerial& _serial;
    PacketCallback _packetCallback = nullptr;
    
    // Receive state machine
    enum class RxState {
        WAIT_START,
        GOT_TYPE,
        GOT_LENGTH,
        GOT_SEQ,
        READING_PAYLOAD,
        READING_CRC
    };
    
    RxState _rxState;
    uint8_t _rxBuffer[64];
    uint8_t _rxIndex;
    uint8_t _rxLength;
    uint8_t _txSeq;
    
    // Statistics
    uint32_t _packetsReceived;
    uint32_t _packetErrors;
    unsigned long _lastPacketTime;
    bool _connected;
    bool _paused;  // When true, loop() won't process incoming data (for OTA)
    uint32_t _backoffUntil;  // Non-blocking backoff timestamp (0 = no backoff)
    
    // CRC calculation
    uint16_t calculateCRC(const uint8_t* data, size_t length);
    
    // Process received byte
    void processByte(uint8_t byte);
    
    // Process complete packet
    void processPacket();
};

#endif // PICO_UART_H
