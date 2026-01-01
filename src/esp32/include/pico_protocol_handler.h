#ifndef PICO_PROTOCOL_HANDLER_H
#define PICO_PROTOCOL_HANDLER_H

#include "pico_uart.h"
#include <stdint.h>

// Forward declarations
class BrewWebServer;
class PowerMeterManager;

namespace BrewOS {
    class StateManager;
}

/**
 * PicoProtocolHandler
 * 
 * Encapsulates Pico protocol V1.1 handling logic:
 * - NACK backoff management
 * - Handshake processing
 * - Packet routing to appropriate handlers
 * 
 * This class improves maintainability by separating protocol logic
 * from main.cpp, making it easier to test and modify.
 */
class PicoProtocolHandler {
public:
    PicoProtocolHandler();
    
    /**
     * Initialize handler with required dependencies
     */
    void begin(PicoUART* uart, BrewWebServer* server, BrewOS::StateManager* state, PowerMeterManager* powerMeter);
    
    /**
     * Handle incoming Pico packet
     * Routes packet to appropriate handler based on message type
     */
    void handlePacket(const PicoPacket& packet);
    
    /**
     * Handle NACK message (backpressure from Pico)
     * Implements non-blocking exponential backoff
     */
    void handleNACK(const PicoPacket& packet);
    
    /**
     * Handle handshake message
     * Responds with protocol version and capabilities
     */
    void handleHandshake(const PicoPacket& packet);
    
    /**
     * Get current backoff timestamp (for PicoUART to check)
     */
    uint32_t getBackoffUntil() const { return _backoffUntil; }

private:
    PicoUART* _uart;
    BrewWebServer* _server;
    BrewOS::StateManager* _state;
    PowerMeterManager* _powerMeter;
    
    // NACK backoff state
    uint32_t _nackCount;
    uint32_t _lastNackTime;
    uint32_t _backoffUntil;
    
    // Helper to update backoff state
    void updateBackoff(uint32_t now);
};

#endif // PICO_PROTOCOL_HANDLER_H

