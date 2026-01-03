#include "pico_protocol_handler.h"
#include "pico_uart.h"
#include "web_server.h"
#include "state/state_manager.h"
#include "power_meter/power_meter_manager.h"
#include "runtime_state.h"
#include "ui/ui.h"
#include "config.h"
#include "log_manager.h"
#include "../shared/protocol_defs.h"
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

void PicoProtocolHandler::begin(PicoUART* uart, BrewWebServer* server, BrewOS::StateManager* state, PowerMeterManager* powerMeter) {
    _uart = uart;
    _server = server;
    _state = state;
    _powerMeter = powerMeter;
}

void PicoProtocolHandler::handlePacket(const PicoPacket& packet) {
    // Route packet to appropriate handler
    switch (packet.type) {
        case MSG_BOOT:
            handleBoot(packet);
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

// Pre-infusion pause time constant (matches main.cpp)
static constexpr uint16_t DEFAULT_PREINFUSION_PAUSE_MS = 5000;

void PicoProtocolHandler::handleBoot(const PicoPacket& packet) {
    LOG_I("Pico boot info received");
    if (_server) _server->broadcastLog("Pico booted");
    
    // Update Pico connection state
    runtimeState().updatePicoConnection(true);
    
    // Parse boot payload (boot_payload_t structure)
    // Layout: ver(3) + machine(1) + pcb(1) + pcb_ver(2) + reset(4) + build_date(12) + build_time(7) + protocol_ver(2) = 32 bytes
    if (packet.length >= 4) {
        uint8_t ver_major = packet.payload[0];
        uint8_t ver_minor = packet.payload[1];
        uint8_t ver_patch = packet.payload[2];
        
        // Update machine type in runtime state
        ui_state_t& bootState = runtimeState().beginUpdate();
        bootState.machine_type = packet.payload[3];
        uint8_t machineType = bootState.machine_type;  // Save for later use
        runtimeState().endUpdate();
        
        // Store in StateManager
        if (_state) {
            _state->setPicoVersion(ver_major, ver_minor, ver_patch);
            _state->setMachineType(machineType);
        }
        
        LOG_I("Pico version: %d.%d.%d, Machine type: %d", 
              ver_major, ver_minor, ver_patch, machineType);
        
        // Parse reset reason if available (offset 7-10, uint32_t)
        if (packet.length >= 11) {
            // reset_reason is at offset 7 (after pcb_type and pcb_version), 4 bytes (uint32_t, little-endian)
            uint32_t reset_reason = 
                (static_cast<uint32_t>(packet.payload[7])      ) |
                (static_cast<uint32_t>(packet.payload[8]) << 8 ) |
                (static_cast<uint32_t>(packet.payload[9]) << 16) |
                (static_cast<uint32_t>(packet.payload[10]) << 24);
            if (_state) {
                _state->setPicoResetReason(reset_reason);
            }
        }
        
        // Parse build date/time if available (offset 11-29, protocol_ver at 30-31)
        if (packet.length >= 30) {
            char buildDate[12] = {0};
            char buildTimeCompact[7] = {0};  // "HHMMSS" format (no colons)
            char buildTime[9] = {0};          // "HH:MM:SS" format for display
            
            memcpy(buildDate, &packet.payload[11], 11);
            memcpy(buildTimeCompact, &packet.payload[23], 6);
            
            // Convert from "HHMMSS" to "HH:MM:SS" format
            if (strlen(buildTimeCompact) == 6) {
                buildTime[0] = buildTimeCompact[0];  // H
                buildTime[1] = buildTimeCompact[1];  // H
                buildTime[2] = ':';
                buildTime[3] = buildTimeCompact[2];  // M
                buildTime[4] = buildTimeCompact[3];  // M
                buildTime[5] = ':';
                buildTime[6] = buildTimeCompact[4];  // S
                buildTime[7] = buildTimeCompact[5];  // S
                buildTime[8] = '\0';
            } else {
                // Fallback: use compact format as-is
                strncpy(buildTime, buildTimeCompact, sizeof(buildTime) - 1);
            }
            
            if (_state) {
                _state->setPicoBuildDate(buildDate, buildTime);
            }
        }
        
        // Check for version mismatch
        char picoVerStr[16];
        snprintf(picoVerStr, sizeof(picoVerStr), "%d.%d.%d", ver_major, ver_minor, ver_patch);
        if (strcmp(picoVerStr, ESP32_VERSION) != 0) {
            LOG_W("Internal version mismatch: %s vs %s", ESP32_VERSION, picoVerStr);
            if (_server) _server->broadcastLogLevel("warn", "Firmware update recommended");
        }
    }
    
    // Request environmental config from Pico (Pico is source of truth)
    // Pico will send MSG_ENV_CONFIG with its persisted settings
    // This is required for Pico to exit STATE_FAULT (0x05) if config is missing
    // But we request it first to see what Pico has stored
    if (_uart) {
        _uart->sendCommand(MSG_CMD_GET_CONFIG, nullptr, 0);
        LOG_I("Requested config from Pico (Pico is source of truth)");
    }
    
    // Pico is the source of truth for temperature setpoints and eco mode
    // Pico loads settings from its own flash on boot and reports them in status messages
    // No need to send from ESP32 - Pico already has the persisted values
    
    // Send pre-infusion config to Pico
    if (_state) {
        const auto& brewSettings = _state->settings().brew;
        if (brewSettings.preinfusionTime > 0 || brewSettings.preinfusionPressure > 0) {
            bool enabled = brewSettings.preinfusionPressure > 0;
            uint16_t onTimeMs = (uint16_t)(brewSettings.preinfusionTime * 1000);
            uint16_t pauseTimeMs = enabled ? DEFAULT_PREINFUSION_PAUSE_MS : 0;
            
            uint8_t preinfPayload[6];
            preinfPayload[0] = CONFIG_PREINFUSION;
            preinfPayload[1] = enabled ? 1 : 0;
            preinfPayload[2] = onTimeMs & 0xFF;
            preinfPayload[3] = (onTimeMs >> 8) & 0xFF;
            preinfPayload[4] = pauseTimeMs & 0xFF;
            preinfPayload[5] = (pauseTimeMs >> 8) & 0xFF;
            if (_uart && _uart->sendCommand(MSG_CMD_CONFIG, preinfPayload, 6)) {
                LOG_I("Sent pre-infusion config to Pico: %s, on=%dms, pause=%dms",
                      enabled ? "enabled" : "disabled", onTimeMs, pauseTimeMs);
            }
        }
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

