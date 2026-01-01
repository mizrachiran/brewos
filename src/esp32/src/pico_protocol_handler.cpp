#include "pico_protocol_handler.h"
#include "pico_uart.h"
#include "web_server.h"
#include "state/state_manager.h"
#include "power_meter/power_meter_manager.h"
#include "config.h"
#include <string.h>

// Forward declaration - parsePicoStatus is still in main.cpp
extern void parsePicoStatus(const uint8_t* payload, uint8_t length);

// External references from main.cpp
extern PicoUART* picoUart;
extern BrewWebServer* webServer;
extern PowerMeterManager* powerMeterManager;

PicoProtocolHandler::PicoProtocolHandler()
    : _uart(nullptr)
    , _server(nullptr)
    , _state(nullptr)
    , _powerMeter(nullptr)
    , _nackCount(0)
    , _lastNackTime(0)
    , _backoffUntil(0) {
}

void PicoProtocolHandler::begin(PicoUART* uart, BrewWebServer* server, StateManager* state, PowerMeterManager* powerMeter) {
    _uart = uart;
    _server = server;
    _state = state;
    _powerMeter = powerMeter;
}

void PicoProtocolHandler::handlePacket(const PicoPacket& packet) {
    // Route packet to appropriate handler
    switch (packet.type) {
        case MSG_BOOT:
            // Boot handling is complex and tightly coupled to main.cpp state
            // Keep it in main.cpp for now, but could be moved here in future refactoring
            // For now, this is handled directly in main.cpp::onPicoPacket()
            break;
            
        case MSG_HANDSHAKE:
            handleHandshake(packet);
            break;
            
        case MSG_NACK:
            handleNACK(packet);
            break;
            
        case MSG_STATUS:
            // Status parsing is complex and uses main.cpp state management
            // Keep it in main.cpp for now
            parsePicoStatus(packet.payload, packet.length);
            break;
            
        case MSG_POWER_METER:
            if (packet.length >= sizeof(PowerMeterReading) && _powerMeter) {
                PowerMeterReading reading;
                memcpy(&reading, packet.payload, sizeof(PowerMeterReading));
                _powerMeter->onPicoPowerData(reading);
            }
            break;
            
        case MSG_ALARM:
        case MSG_CONFIG:
        case MSG_ENV_CONFIG:
        case MSG_DEBUG_RESP:
        case MSG_DIAGNOSTICS:
        case MSG_LOG:
            // These are still handled in main.cpp::onPicoPacket()
            // Can be moved here in future refactoring if needed
            break;
            
        default:
            // Unknown packet type - handled in main.cpp
            break;
    }
}

void PicoProtocolHandler::handleNACK(const PicoPacket& packet) {
    // Pico is busy (backpressure) - reduce command rate
    // NOTE: Using non-blocking backoff to keep UI responsive
    if (packet.length < 4) {
        return;
    }
    
    uint8_t cmd_type = packet.payload[0];
    uint8_t cmd_seq = packet.payload[1];
    uint8_t result = packet.payload[2];
    
    LOG_W("Pico NACK: cmd=0x%02X seq=%d result=0x%02X (backpressure)", 
          cmd_type, cmd_seq, result);
    
    uint32_t now = millis();
    updateBackoff(now);
    
    // Set backoff in PicoUART so it defers next command
    if (_uart) {
        _uart->setBackoffUntil(_backoffUntil);
    }
}

void PicoProtocolHandler::handleHandshake(const PicoPacket& packet) {
    LOG_I("Pico handshake received");
    
    // Parse handshake payload
    if (packet.length < 6) {
        return;
    }
    
    uint8_t proto_major = packet.payload[0];
    uint8_t proto_minor = packet.payload[1];
    uint8_t capabilities = packet.payload[2];
    uint8_t max_retry = packet.payload[3];
    uint16_t ack_timeout = (packet.payload[5] << 8) | packet.payload[4];
    
    LOG_I("Protocol: v%d.%d, capabilities=0x%02X, retry=%d, timeout=%dms",
          proto_major, proto_minor, capabilities, max_retry, ack_timeout);
    
    // Send handshake response
    uint8_t handshake[6] = {
        1,  // protocol_version_major
        1,  // protocol_version_minor
        0,  // capabilities
        3,  // max_retry_count
        (uint8_t)(1000 & 0xFF),      // ack_timeout_ms low byte
        (uint8_t)((1000 >> 8) & 0xFF) // ack_timeout_ms high byte
    };
    
    if (_uart) {
        _uart->sendPacket(MSG_HANDSHAKE, handshake, 6);
    }
}

void PicoProtocolHandler::updateBackoff(uint32_t now) {
    // Initialize timing on first NACK
    if (_lastNackTime == 0) {
        _lastNackTime = now;
        _nackCount = 1;
    } else {
        _nackCount++;
        
        // If multiple NACKs in short time, warn about system overload
        if (now - _lastNackTime < 5000) {
            if (_nackCount > 10) {
                LOG_E("High NACK rate detected - Pico command queue overload");
                LOG_I("Consider reducing command frequency or increasing PROTOCOL_MAX_PENDING_CMDS");
                _nackCount = 0;  // Reset to avoid spam
            }
        } else {
            _nackCount = 1;  // Reset counter after quiet period
        }
        
        _lastNackTime = now;
    }
    
    // Non-blocking exponential backoff: set timestamp for when commands can resume
    // PicoUART should check this before sending commands
    // This avoids blocking delay() which freezes UI (encoder, display)
    uint32_t backoff_ms = min((uint32_t)(100 * _nackCount), (uint32_t)500);
    _backoffUntil = now + backoff_ms;
}

