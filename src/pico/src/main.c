/**
 * Coffee Machine Controller - Pico Firmware
 * 
 * Main entry point for the RP2350-based control board (Raspberry Pi Pico 2).
 * 
 * Core 0: Real-time control loop (safety, sensors, PID, outputs)
 * Core 1: Communication with ESP32
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/mutex.h"
#include "hardware/watchdog.h"

#include "config.h"
#include "protocol.h"
#include "safety.h"
#include "state.h"
#include "sensors.h"
#include "control.h"
#include "machine_config.h"
#include "environmental_config.h"
#include "pcb_config.h"
#include "gpio_init.h"
#include "hardware.h"
#include "water_management.h"
#include "config_persistence.h"
#include "cleaning.h"
#include "bootloader.h"
#include "power_meter.h"
#include "diagnostics.h"
#include "flash_safe.h"
#include "class_b.h"
#include "log_forward.h"
#include "packet_handlers.h"
#include "logging.h"
#include "validation.h"

// -----------------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------------
static volatile bool g_core1_ready = false;
static uint32_t g_boot_time = 0;

// Core 1 alive flag for watchdog monitoring
// Core 1 sets this to true each iteration; Core 0 checks and resets it.
// If Core 1 stops responding, Core 0 will stop kicking the watchdog.
static volatile bool g_core1_alive = false;
static volatile uint32_t g_core1_last_seen = 0;
#define CORE1_TIMEOUT_MS 1000  // Core 1 must respond within 1 second

// Status payload (updated by control loop on Core 0, read by comms on Core 1)
// Double-buffered for non-blocking access: Core 0 writes to inactive buffer, Core 1 reads from active
static status_payload_t g_status_buffers[2];
static volatile uint32_t g_active_buffer = 0;  // 0 or 1 - index of buffer Core 1 should read
static volatile bool g_status_updated = false;  // Flag to indicate new data available

// Alarm state tracking - only send alarm messages when state changes
static uint8_t g_last_sent_alarm = ALARM_NONE;

// -----------------------------------------------------------------------------
// Helper: Send environmental config to ESP32
// -----------------------------------------------------------------------------
static void send_environmental_config(void) {
    environmental_electrical_t env;
    environmental_config_get(&env);
    electrical_state_t elec_state;
    electrical_state_get(&elec_state);
    
    env_config_payload_t payload;
    payload.nominal_voltage = env.nominal_voltage;
    payload.max_current_draw = env.max_current_draw;
    payload.brew_heater_current = elec_state.brew_heater_current;
    payload.steam_heater_current = elec_state.steam_heater_current;
    payload.max_combined_current = elec_state.max_combined_current;
    protocol_send_env_config(&payload);
}

// -----------------------------------------------------------------------------
// Core 1 Entry Point (Communication)
// -----------------------------------------------------------------------------
void core1_main(void) {
    LOG_PRINT("Core 1: Starting communication loop\n");
    
    // Initialize protocol
    protocol_init();
    
    // Initiate protocol handshake with ESP32
    protocol_request_handshake();
    LOG_PRINT("Protocol v1.1 handshake initiated\n");
    
    // Send boot message and environmental config
    protocol_send_boot();
    send_environmental_config();
    
    // Signal ready
    g_core1_ready = true;
    
    uint32_t last_status_send = 0;
    uint32_t last_boot_info_send = 0;  // For periodic boot info resend
    uint32_t last_protocol_stats_log = 0;  // For periodic protocol diagnostics
    
    while (true) {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        
        // Process incoming packets (skips automatically when bootloader is active)
        protocol_process();
        
        // Skip all periodic sends when bootloader is active
        // Bootloader has full control of UART
        if (bootloader_is_active()) {
            // Still signal alive so Core 0 doesn't think we're dead
            g_core1_alive = true;
            sleep_us(100);
            continue;
        }
        
        // Send status periodically
        if (now - last_status_send >= STATUS_SEND_PERIOD_MS) {
            last_status_send = now;
            
            // Double-buffered read: read from active buffer (no mutex needed)
            status_payload_t status_copy;
            bool should_send = false;
            
            if (g_status_updated) {
                uint32_t read_idx = g_active_buffer;  // Read from active buffer
                __dmb();  // Data memory barrier - ensure we see latest active_buffer value
                memcpy(&status_copy, &g_status_buffers[read_idx], sizeof(status_payload_t));
                should_send = true;
            }
            
            if (should_send) {
                protocol_send_status(&status_copy);
            }
        }
        
        // Periodically resend boot info (version, env config) to ensure ESP32 stays in sync
        // This helps recover from missed messages or ESP32 restarts
        if (now - last_boot_info_send >= BOOT_INFO_RESEND_MS) {
            last_boot_info_send = now;
            
            protocol_send_boot();
            send_environmental_config();
            
            DEBUG_PRINT("Core 1: Periodic boot info resend complete\n");
        }
        
        // Monitor protocol health and log statistics periodically
        if (now - last_protocol_stats_log >= 60000) {  // Every 60 seconds
            last_protocol_stats_log = now;
            
            protocol_stats_t stats;
            protocol_get_stats(&stats);
            
            // Log protocol health metrics (combined into one message to avoid rate limiting drops)
            LOG_INFO("Protocol: RX=%lu TX=%lu CRC_err=%u PKT_err=%u TO=%u | Retry=%u ACK_TO=%u NACK=%u Pending=%u\n",
                     stats.packets_received, stats.packets_sent,
                     stats.crc_errors, stats.packet_errors, stats.timeout_errors,
                     stats.retries, stats.ack_timeouts,
                     stats.nacks_received, stats.pending_cmd_count);
            
            // Check for protocol health issues
            if (stats.crc_errors > 100) {
                LOG_WARN("High CRC error rate detected - check wiring/EMI\n");
            }
            if (stats.timeout_errors > 50) {
                LOG_WARN("High parser timeout rate - possible UART issues\n");
            }
            if (stats.ack_timeouts > 20) {
                LOG_WARN("High ACK timeout rate - ESP32 may be overloaded\n");
            }
            if (!stats.handshake_complete) {
                LOG_WARN("Protocol handshake not complete - retrying\n");
                protocol_request_handshake();
            }
        }
        
        // Process pending log messages from ring buffer (non-blocking logging)
        // This drains messages queued by Core 0 to prevent printf() from blocking
        logging_process_pending();
        
        // Process pending flash writes for log forwarding (deferred to avoid blocking protocol handler)
        log_forward_process();
        
        // Signal that Core 1 is alive (for watchdog monitoring by Core 0)
        g_core1_alive = true;
        
        // Small sleep to not hog CPU
        sleep_us(100);
    }
}

// -----------------------------------------------------------------------------
// Command name lookup for logging
// -----------------------------------------------------------------------------
static const char* get_msg_name(uint8_t type) {
    switch (type) {
        case MSG_PING:                    return "PING";
        case MSG_STATUS:                  return "STATUS";
        case MSG_ALARM:                   return "ALARM";
        case MSG_BOOT:                    return "BOOT";
        case MSG_ACK:                     return "ACK";
        case MSG_CONFIG:                  return "CONFIG";
        case MSG_DEBUG:                   return "DEBUG";
        case MSG_ENV_CONFIG:              return "ENV_CONFIG";
        case MSG_STATISTICS:              return "STATISTICS";
        case MSG_DIAGNOSTICS:             return "DIAGNOSTICS";
        case MSG_HANDSHAKE:               return "HANDSHAKE";
        case MSG_NACK:                    return "NACK";
        case MSG_CMD_SET_TEMP:            return "SET_TEMP";
        case MSG_CMD_SET_PID:             return "SET_PID";
        case MSG_CMD_BREW:                return "BREW";
        case MSG_CMD_MODE:                return "MODE";
        case MSG_CMD_CONFIG:              return "CONFIG";
        case MSG_CMD_GET_CONFIG:          return "GET_CONFIG";
        case MSG_CMD_GET_ENV_CONFIG:      return "GET_ENV_CONFIG";
        case MSG_CMD_CLEANING_START:      return "CLEANING_START";
        case MSG_CMD_CLEANING_STOP:       return "CLEANING_STOP";
        case MSG_CMD_CLEANING_RESET:      return "CLEANING_RESET";
        case MSG_CMD_CLEANING_SET_THRESHOLD: return "CLEANING_SET_THRESHOLD";
        case MSG_CMD_GET_STATISTICS:      return "GET_STATISTICS";
        case MSG_CMD_DEBUG:               return "DEBUG";
        case MSG_CMD_SET_ECO:             return "SET_ECO";
        case MSG_CMD_BOOTLOADER:          return "BOOTLOADER";
        case MSG_CMD_DIAGNOSTICS:         return "DIAGNOSTICS";
        case MSG_CMD_POWER_METER_CONFIG:  return "POWER_METER_CONFIG";
        case MSG_CMD_POWER_METER_DISCOVER: return "POWER_METER_DISCOVER";
        case MSG_CMD_GET_BOOT:            return "GET_BOOT";
        case MSG_CMD_LOG_CONFIG:          return "LOG_CONFIG";
        case MSG_LOG:                     return "LOG";
        default:                          return "UNKNOWN";
    }
}

// -----------------------------------------------------------------------------
// Packet Handler (called from Core 1)
// -----------------------------------------------------------------------------
void handle_packet(const packet_t* packet) {
    LOG_INFO("CMD: %s (0x%02X) len=%d\n", get_msg_name(packet->type), packet->type, packet->length);
    
    // Register heartbeat from ESP32 - critical for safety system
    safety_esp32_heartbeat();
    
    // Dispatch to modular packet handlers
    // Each handler validates inputs, applies changes, and sends responses
    switch (packet->type) {
        case MSG_PING:
            handle_cmd_ping(packet);
            break;
            
        case MSG_CMD_SET_TEMP:
            handle_cmd_set_temp(packet);
            break;
            
        case MSG_CMD_SET_PID:
            handle_cmd_set_pid(packet);
            break;
            
        case MSG_CMD_BREW:
            handle_cmd_brew(packet);
            break;
            
        case MSG_CMD_MODE:
            handle_cmd_mode(packet);
            break;
            
        case MSG_CMD_GET_CONFIG:
            handle_cmd_get_config(packet);
            break;
            
        case MSG_CMD_CONFIG:
            handle_cmd_config(packet);
            break;
            
        case MSG_CMD_GET_ENV_CONFIG:
            handle_cmd_get_env_config(packet);
            break;
            
        case MSG_CMD_CLEANING_START:
        case MSG_CMD_CLEANING_STOP:
        case MSG_CMD_CLEANING_RESET:
        case MSG_CMD_CLEANING_SET_THRESHOLD:
            handle_cmd_cleaning(packet);
            break;
            
        case MSG_CMD_GET_STATISTICS:
            handle_cmd_get_statistics(packet);
            break;
            
        case MSG_CMD_DEBUG:
            handle_cmd_debug(packet);
            break;
            
        case MSG_CMD_SET_ECO:
            handle_cmd_set_eco(packet);
            break;
            
        case MSG_CMD_BOOTLOADER:
            handle_cmd_bootloader(packet);
            break;
            
        case MSG_CMD_DIAGNOSTICS:
            handle_cmd_diagnostics(packet);
            break;
            
        case MSG_CMD_POWER_METER_CONFIG:
        case MSG_CMD_POWER_METER_DISCOVER:
            handle_cmd_power_meter(packet);
            break;
            
        case MSG_CMD_GET_BOOT:
            handle_cmd_get_boot(packet);
            break;
            
        case MSG_CMD_LOG_CONFIG:
            handle_cmd_log_config(packet);
            break;
            
        default:
            LOG_WARN("Unknown packet type: 0x%02X\n", packet->type);
            break;
    }
}


// -----------------------------------------------------------------------------
// Core 0 Entry Point (Control)
// -----------------------------------------------------------------------------
int main(void) {
    // Record boot time
    g_boot_time = to_ms_since_boot(get_absolute_time());
    
    // Double-buffering initialized: g_status_buffers and g_active_buffer are statically initialized
    // No mutex needed - Core 0 writes to inactive buffer, Core 1 reads from active buffer
    
    // Initialize stdio (USB serial for logging)
    stdio_init_all();
    sleep_ms(100);  // Brief delay for USB enumeration
    
    // Disable stdout/stdin buffering to free malloc'd RAM (~1KB savings)
    // For a control system with sporadic logging, buffering is unnecessary overhead
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stdin, NULL, _IONBF, 0);
    
    // Always print boot banner to USB serial
    LOG_PRINT("\n========================================\n");
    LOG_PRINT("ECM Pico Controller v%d.%d.%d\n", 
                FIRMWARE_VERSION_MAJOR, 
                FIRMWARE_VERSION_MINOR, 
                FIRMWARE_VERSION_PATCH);
    LOG_PRINT("Build: %s %s\n", BUILD_DATE, BUILD_TIME);
    LOG_PRINT("========================================\n");
    
    // Initialize hardware abstraction layer
    if (!hw_init()) {
        LOG_PRINT("ERROR: Failed to initialize hardware abstraction layer\n");
        // Continue anyway, but hardware may not work correctly
    } else {
        LOG_PRINT("Hardware: %s mode\n", hw_is_simulation_mode() ? "SIMULATION" : "REAL");
    }
    
    // Initialize PCB configuration and GPIO
    if (!gpio_init_all()) {
        LOG_PRINT("ERROR: Failed to initialize GPIO (invalid PCB config)\n");
        // Continue anyway, but GPIO may not work correctly
    } else {
        const pcb_config_t* pcb = pcb_config_get();
        if (pcb) {
            LOG_PRINT("PCB: %s v%d.%d.%d\n",
                       pcb->name,
                       pcb->version.major,
                       pcb->version.minor,
                       pcb->version.patch);
        }
    }
    
    // Log machine configuration (lazy initialized on first access)
    const machine_features_t* features = machine_get_features();
    if (features) {
        LOG_PRINT("Machine: %s\n", features->name);
        LOG_PRINT("  Type: %s\n", 
            features->type == MACHINE_TYPE_DUAL_BOILER ? "Dual Boiler" :
            features->type == MACHINE_TYPE_SINGLE_BOILER ? "Single Boiler" :
            features->type == MACHINE_TYPE_HEAT_EXCHANGER ? "Heat Exchanger" : "Unknown");
        LOG_PRINT("  Boilers: %d, SSRs: %d\n", features->num_boilers, features->num_ssrs);
        LOG_PRINT("  Sensors: brew_ntc=%d steam_ntc=%d\n",
                   features->has_brew_ntc, features->has_steam_ntc);
    } else {
        LOG_PRINT("ERROR: Machine configuration not available!\n");
    }
    
    // SAF-001: Enable watchdog immediately after GPIO initialization
    watchdog_enable(SAFETY_WATCHDOG_TIMEOUT_MS, true);
    LOG_PRINT("Watchdog enabled (%dms timeout)\n", SAFETY_WATCHDOG_TIMEOUT_MS);
    
    // Check reset reason
    if (watchdog_caused_reboot()) {
        LOG_PRINT("WARNING: Watchdog reset!\n");
        // SAF-004: On watchdog timeout, outputs are already OFF from gpio_init_outputs()
        // which sets all outputs to 0 (safe state) on boot
    }
    
    // Initialize safety system FIRST
    safety_init();
    LOG_PRINT("Safety system initialized\n");
    
    // Initialize Class B safety routines (IEC 60730/60335 compliance)
    if (class_b_init() != CLASS_B_PASS) {
        LOG_PRINT("ERROR: Class B initialization failed!\n");
        // Continue but log the error - safety system will catch issues
    }
    
    // Run Class B startup self-test
    class_b_result_t class_b_result = class_b_startup_test();
    if (class_b_result != CLASS_B_PASS) {
        LOG_PRINT("ERROR: Class B startup test failed: %s\n", 
                   class_b_result_string(class_b_result));
        // Enter safe state if startup tests fail
        safety_enter_safe_state();
    } else {
        LOG_PRINT("Class B startup tests PASSED\n");
    }
    
    // Initialize sensors
    sensors_init();
    DEBUG_PRINT("Sensors initialized\n");
    
    // Initialize configuration persistence (loads from flash)
    bool env_valid = config_persistence_init();
    if (!env_valid) {
        DEBUG_PRINT("ERROR: Environmental configuration not set - machine disabled\n");
        DEBUG_PRINT("ERROR: Please configure voltage and current limits via ESP32\n");
        // Machine will remain in safe state until environmental config is set
    } else {
        electrical_state_t elec_state;
        electrical_state_get(&elec_state);
        DEBUG_PRINT("Electrical: %dV, %dW brew, %dW steam, %.1fA max\n",
                    elec_state.nominal_voltage,
                    elec_state.brew_heater_power,
                    elec_state.steam_heater_power,
                    elec_state.max_current_draw);
    }
    
    // Initialize log forwarding (dev mode feature)
    // Must be done after config_persistence_init() so flash is available
    // Note: Boot logs (lines 348-407) happen before this, so they won't be forwarded
    // But all subsequent logs will be forwarded if enabled
    log_forward_init();
    // Enable forwarding in logging system if it was enabled in flash
    if (log_forward_is_enabled()) {
        logging_set_forward_enabled(true);
        // Use direct printf to avoid recursion during initialization
        printf("Log forwarding enabled (loaded from flash)\n");
    }
    
    // Initialize control
    control_init();
    DEBUG_PRINT("Control initialized\n");
    
    // Initialize state machine
    state_init();
    DEBUG_PRINT("State machine initialized\n");
    
    // Initialize water management
    water_management_init();
    DEBUG_PRINT("Water management initialized\n");
    
    // Initialize cleaning mode
    cleaning_init();
    DEBUG_PRINT("Cleaning mode initialized\n");
    
    // Note: Statistics are now tracked by ESP32 (has NTP for accurate timestamps)
    // Pico only sends brew completion events to ESP32 via alarms
    
    // Initialize flash safety system on Core 0 BEFORE launching Core 1
    // This allows Core 1 to pause Core 0 during flash operations (XIP safety)
    // CRITICAL: Must be done before Core 1 launches, otherwise if Core 1
    // tries to write flash immediately, the lockout handshake will fail/hang.
    flash_safe_init();
    
    // Launch Core 1 for communication
    multicore_launch_core1(core1_main);
    DEBUG_PRINT("Core 1 launched\n");
    
    // Wait for Core 1 to be ready
    while (!g_core1_ready) {
        sleep_ms(1);
    }
    
    // Set up packet handler
    protocol_set_callback(handle_packet);
    
    DEBUG_PRINT("Entering main control loop\n");
    
    // Timing
    uint32_t last_control = 0;
    uint32_t last_sensor = 0;
    uint32_t last_water = 0;
    
    // Initialize Core 1 last-seen timestamp to current time
    // This prevents false watchdog triggers if boot takes longer than CORE1_TIMEOUT_MS
    g_core1_last_seen = to_ms_since_boot(get_absolute_time());
    
    // Main control loop (Core 0)
    while (true) {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        
        // Skip all operations when bootloader is active
        // Core 1 (running bootloader) handles everything during OTA
        if (bootloader_is_active()) {
            // Feed watchdog only - don't do any special lockout init
            // The SDK's flash_safe_execute handles multicore coordination internally
            // when called from Core 1. Core 0 just needs to be in a simple state.
            watchdog_update();
            
            // Simple tight loop - be responsive to any SDK coordination
            tight_loop_contents();
            continue;
        }
        
        // Read sensors (20Hz)
        if (now - last_sensor >= SENSOR_READ_PERIOD_MS) {
            last_sensor = now;
            sensors_read();
        }
        
        // Update water management (10Hz)
        if (now - last_water >= 100) {
            last_water = now;
            water_management_update();
        }
        
        // Update cleaning mode (10Hz)
        cleaning_update();
        
        // Control loop (10Hz)
        if (now - last_control >= CONTROL_LOOP_PERIOD_MS) {
            last_control = now;
            
            // Check safety first
            safety_state_t safety = safety_check();
            
            // Only send alarm messages when the alarm state changes
            uint8_t current_alarm = ALARM_NONE;
            if (safety == SAFETY_CRITICAL) {
                // Enter safe state - all outputs off
                safety_enter_safe_state();
                current_alarm = safety_get_last_alarm();
            } else if (safety == SAFETY_FAULT) {
                // Warning condition - may continue with limits
                current_alarm = safety_get_last_alarm();
            }
            // else: safety == SAFETY_OK or SAFETY_WARNING, current_alarm stays ALARM_NONE
            
            // Send alarm message only if state changed
            if (current_alarm != g_last_sent_alarm) {
                if (safety == SAFETY_CRITICAL) {
                    protocol_send_alarm(current_alarm, 2, 0);
                } else if (safety == SAFETY_FAULT) {
                    protocol_send_alarm(current_alarm, 1, 0);
                } else if (g_last_sent_alarm != ALARM_NONE) {
                    // Safety is OK now, but we had an alarm before - send ALARM_NONE to clear
                    protocol_send_alarm(ALARM_NONE, 0, 0);
                }
                // else: both current and last are ALARM_NONE, no need to send
                g_last_sent_alarm = current_alarm;
            }
            
            // Run periodic Class B self-tests (IEC 60730/60335)
            // Tests are staggered across cycles to minimize latency impact
            class_b_result_t class_b_periodic = class_b_periodic_test();
            if (class_b_periodic != CLASS_B_PASS) {
                // Class B failure - enter safe state
                DEBUG_PRINT("CLASS B FAILURE: %s - entering safe state\n",
                           class_b_result_string(class_b_periodic));
                safety_enter_safe_state();
                protocol_send_alarm(ALARM_WATCHDOG, 2, 0);  // Use watchdog alarm for internal fault
            }
            
            // SAF-003: Feed watchdog only from main control loop after safety checks pass
            // Also verify Core 1 (communication) is still responsive before kicking.
            // If Core 1 hangs, we want the watchdog to reset the system.
            //
            // Note: Watchdog is fed after safety checks but before loop timing check.
            // This means the watchdog catches CPU freezes/hangs, but not timing violations.
            // Timing violations are logged separately (see loop overrun detection below).
            // This design prioritizes safety (catching freezes) over timing precision.
            if (g_core1_alive) {
                // Core 1 is alive - reset flag and kick watchdog
                g_core1_alive = false;
                g_core1_last_seen = now;
                safety_kick_watchdog();
            } else if ((now - g_core1_last_seen) < CORE1_TIMEOUT_MS) {
                // Core 1 flag not set this cycle, but within timeout - still kick
                // This handles cases where Core 0 runs faster than Core 1
                safety_kick_watchdog();
            } else {
                // Core 1 hasn't responded within timeout - don't kick watchdog
                // System will reset after watchdog timeout
                DEBUG_PRINT("WARNING: Core 1 not responding, watchdog will reset!\n");
            }
            
            // Update state machine
            state_update();
            
            // Run control (PID, outputs)
            if (!safety_is_safe_state()) {
                control_update();
            }
            
            // Update status for Core 1 (double-buffered, non-blocking)
            sensor_data_t sensor_data;
            sensors_get_data(&sensor_data);
            
            control_outputs_t outputs;
            control_get_outputs(&outputs);
            
            // Build status locally first, then copy to inactive buffer
            status_payload_t new_status = {0};
            
            // Machine-type aware status population
            // HX machines: brew_temp is invalid (no brew NTC), use group_temp
            // Single boiler: steam_temp is invalid (no steam NTC)
            const machine_features_t* machine_features = machine_get_features();
            
            if (machine_features && machine_features->type == MACHINE_TYPE_HEAT_EXCHANGER) {
                // HX: No brew NTC, no group temp sensor - only steam boiler NTC
                // brew_temp = 0 indicates no sensor (UI shows only steam boiler)
                new_status.brew_temp = 0;
                new_status.steam_temp = sensor_data.steam_temp;
                new_status.group_temp = 0;  // No group temp sensor on HX
            } else if (machine_features && machine_features->type == MACHINE_TYPE_SINGLE_BOILER) {
                // Single boiler: use brew NTC for both (same physical sensor)
                new_status.brew_temp = sensor_data.brew_temp;
                new_status.steam_temp = sensor_data.brew_temp;  // Same as brew for display
                new_status.group_temp = sensor_data.group_temp;
            } else {
                // Dual boiler: all sensors independent
                new_status.brew_temp = sensor_data.brew_temp;
                new_status.steam_temp = sensor_data.steam_temp;
                new_status.group_temp = sensor_data.group_temp;
            }
            
            new_status.pressure = sensor_data.pressure;
            new_status.brew_setpoint = control_get_setpoint(0);
            new_status.steam_setpoint = control_get_setpoint(1);
            new_status.brew_output = outputs.brew_heater;
            new_status.steam_output = outputs.steam_heater;
            new_status.pump_output = outputs.pump;
            new_status.state = state_get();
            new_status.water_level = sensor_data.water_level;
            new_status.power_watts = outputs.power_watts;
            new_status.uptime_ms = now;
            new_status.shot_start_timestamp_ms = state_get_brew_start_timestamp_ms();
            new_status.heating_strategy = control_get_heating_strategy();
            new_status.cleaning_reminder = cleaning_is_reminder_due() ? 1 : 0;
            new_status.brew_count = cleaning_get_brew_count();
            
            // Set flags
            new_status.flags = 0;
            if (state_is_brewing()) new_status.flags |= STATUS_FLAG_BREWING;
            if (outputs.pump > 0) new_status.flags |= STATUS_FLAG_PUMP_ON;
            if (outputs.brew_heater > 0 || outputs.steam_heater > 0) new_status.flags |= STATUS_FLAG_HEATING;
            if (sensor_data.water_level < SAFETY_MIN_WATER_LEVEL) new_status.flags |= STATUS_FLAG_WATER_LOW;
            if (safety_is_safe_state()) new_status.flags |= STATUS_FLAG_ALARM;
            
            // Double-buffered update: write to inactive buffer, then atomically swap
            // This avoids blocking in the critical control loop
            uint32_t write_idx = 1 - g_active_buffer;  // Write to inactive buffer
            memcpy(&g_status_buffers[write_idx], &new_status, sizeof(status_payload_t));
            __dmb();  // Data memory barrier - ensure write completes before swap
            g_active_buffer = write_idx;  // Atomically swap active buffer
            g_status_updated = true;
            __dmb();  // Ensure flag update is visible to Core 1
        }
        
        // Small sleep
        sleep_us(100);
    }
    
    return 0;
}

