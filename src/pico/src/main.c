/**
 * ECM Coffee Machine Controller - Pico Firmware
 * 
 * Main entry point for the RP2040-based control board.
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
// Protected by mutex for thread-safe access between cores
static status_payload_t g_status;
static bool g_status_updated = false;
static mutex_t g_status_mutex;  // Protects g_status and g_status_updated

// -----------------------------------------------------------------------------
// Core 1 Entry Point (Communication)
// -----------------------------------------------------------------------------
void core1_main(void) {
    LOG_PRINT("Core 1: Starting communication loop\n");
    
    // Initialize protocol
    protocol_init();
    
    // Send boot message and environmental config
    protocol_send_boot();
    
    env_config_payload_t env_resp;
    environmental_electrical_t env;
    environmental_config_get(&env);
    electrical_state_t elec_state;
    electrical_state_get(&elec_state);
    
    env_resp.nominal_voltage = env.nominal_voltage;
    env_resp.max_current_draw = env.max_current_draw;
    env_resp.brew_heater_current = elec_state.brew_heater_current;
    env_resp.steam_heater_current = elec_state.steam_heater_current;
    env_resp.max_combined_current = elec_state.max_combined_current;
    protocol_send_env_config(&env_resp);
    
    // Signal ready
    g_core1_ready = true;
    
    uint32_t last_status_send = 0;
    uint32_t last_boot_info_send = 0;  // For periodic boot info resend
    
    while (true) {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        
        // Process incoming packets
        protocol_process();
        
        // Send status periodically
        if (now - last_status_send >= STATUS_SEND_PERIOD_MS) {
            last_status_send = now;
            
            // Thread-safe read of status with mutex protection
            status_payload_t status_copy;
            bool should_send = false;
            
            mutex_enter_blocking(&g_status_mutex);
            if (g_status_updated) {
                memcpy(&status_copy, &g_status, sizeof(status_payload_t));
                should_send = true;
            }
            mutex_exit(&g_status_mutex);
            
            if (should_send) {
                protocol_send_status(&status_copy);
            }
        }
        
        // Periodically resend boot info (version, env config) to ensure ESP32 stays in sync
        // This helps recover from missed messages or ESP32 restarts
        if (now - last_boot_info_send >= BOOT_INFO_RESEND_MS) {
            last_boot_info_send = now;
            
            // Resend boot message (contains version, machine type, etc.)
            protocol_send_boot();
            
            // Resend environmental config
            environmental_electrical_t env_info;
            environmental_config_get(&env_info);
            electrical_state_t elec_info;
            electrical_state_get(&elec_info);
            
            env_config_payload_t env_payload;
            env_payload.nominal_voltage = env_info.nominal_voltage;
            env_payload.max_current_draw = env_info.max_current_draw;
            env_payload.brew_heater_current = elec_info.brew_heater_current;
            env_payload.steam_heater_current = elec_info.steam_heater_current;
            env_payload.max_combined_current = elec_info.max_combined_current;
            protocol_send_env_config(&env_payload);
            
            DEBUG_PRINT("Core 1: Periodic boot info resend complete\n");
        }
        
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
        default:                          return "UNKNOWN";
    }
}

// -----------------------------------------------------------------------------
// Packet Handler (called from Core 1)
// -----------------------------------------------------------------------------
void handle_packet(const packet_t* packet) {
    LOG_PRINT("CMD: %s (0x%02X) len=%d\n", get_msg_name(packet->type), packet->type, packet->length);
    
    // Register heartbeat from ESP32
    safety_esp32_heartbeat();
    
    switch (packet->type) {
        case MSG_PING: {
            // Respond with ACK
            protocol_send_ack(MSG_PING, packet->seq, ACK_SUCCESS);
            break;
        }
        
        case MSG_CMD_SET_TEMP: {
            if (packet->length >= sizeof(cmd_set_temp_t)) {
                // Use memcpy for safe unaligned access (Cortex-M33 is stricter)
                cmd_set_temp_t cmd;
                memcpy(&cmd, packet->payload, sizeof(cmd_set_temp_t));
                
                // Validate target: 0=brew, 1=steam
                if (cmd.target > 1) {
                    DEBUG_PRINT("CMD_SET_TEMP: Invalid target %d\n", cmd.target);
                    protocol_send_ack(MSG_CMD_SET_TEMP, packet->seq, ACK_ERROR_INVALID);
                    break;
                }
                
                // Validate temperature range: 0-200°C (stored as 0-2000 in 0.1°C units)
                // Negative temperatures don't make sense for espresso machines
                if (cmd.temperature < 0 || cmd.temperature > 2000) {
                    DEBUG_PRINT("CMD_SET_TEMP: Invalid temperature %d (valid: 0-2000)\n", cmd.temperature);
                    protocol_send_ack(MSG_CMD_SET_TEMP, packet->seq, ACK_ERROR_INVALID);
                    break;
                }
                
                control_set_setpoint(cmd.target, cmd.temperature);
                protocol_send_ack(MSG_CMD_SET_TEMP, packet->seq, ACK_SUCCESS);
            } else {
                protocol_send_ack(MSG_CMD_SET_TEMP, packet->seq, ACK_ERROR_INVALID);
            }
            break;
        }
        
        case MSG_CMD_SET_PID: {
            if (packet->length >= sizeof(cmd_set_pid_t)) {
                // Use memcpy for safe unaligned access (Cortex-M33 is stricter)
                cmd_set_pid_t cmd;
                memcpy(&cmd, packet->payload, sizeof(cmd_set_pid_t));
                
                // Validate target: 0=brew, 1=steam
                if (cmd.target > 1) {
                    DEBUG_PRINT("CMD_SET_PID: Invalid target %d\n", cmd.target);
                    protocol_send_ack(MSG_CMD_SET_PID, packet->seq, ACK_ERROR_INVALID);
                    break;
                }
                
                // Validate PID gains: reasonable range 0-10000 (stored as value*100)
                // Max 100.0 for any gain is already very aggressive
                if (cmd.kp > 10000 || cmd.ki > 10000 || cmd.kd > 10000) {
                    DEBUG_PRINT("CMD_SET_PID: Invalid gains Kp=%d Ki=%d Kd=%d (max 10000)\n", 
                               cmd.kp, cmd.ki, cmd.kd);
                    protocol_send_ack(MSG_CMD_SET_PID, packet->seq, ACK_ERROR_INVALID);
                    break;
                }
                
                control_set_pid(cmd.target, cmd.kp / 100.0f, cmd.ki / 100.0f, cmd.kd / 100.0f);
                protocol_send_ack(MSG_CMD_SET_PID, packet->seq, ACK_SUCCESS);
            } else {
                protocol_send_ack(MSG_CMD_SET_PID, packet->seq, ACK_ERROR_INVALID);
            }
            break;
        }
        
        case MSG_CMD_BREW: {
            if (packet->length >= sizeof(cmd_brew_t)) {
                // Use memcpy for safe unaligned access (Cortex-M33 is stricter)
                cmd_brew_t cmd;
                memcpy(&cmd, packet->payload, sizeof(cmd_brew_t));
                if (cmd.action) {
                    state_start_brew();
                } else {
                    state_stop_brew();
                }
                protocol_send_ack(MSG_CMD_BREW, packet->seq, ACK_SUCCESS);
            } else {
                protocol_send_ack(MSG_CMD_BREW, packet->seq, ACK_ERROR_INVALID);
            }
            break;
        }
        
        case MSG_CMD_MODE: {
            if (packet->length >= sizeof(cmd_mode_t)) {
                // Use memcpy for safe unaligned access (Cortex-M33 is stricter)
                cmd_mode_t cmd;
                memcpy(&cmd, packet->payload, sizeof(cmd_mode_t));
                machine_mode_t mode = (machine_mode_t)cmd.mode;
                if (mode <= MODE_STEAM) {  // Valid modes: MODE_IDLE, MODE_BREW, MODE_STEAM
                    if (state_set_mode(mode)) {
                        protocol_send_ack(MSG_CMD_MODE, packet->seq, ACK_SUCCESS);
                    } else {
                        // Mode change rejected (e.g., brewing in progress)
                        protocol_send_ack(MSG_CMD_MODE, packet->seq, ACK_ERROR_REJECTED);
                    }
                } else {
                    protocol_send_ack(MSG_CMD_MODE, packet->seq, ACK_ERROR_INVALID);
                }
            } else {
                protocol_send_ack(MSG_CMD_MODE, packet->seq, ACK_ERROR_INVALID);
            }
            break;
        }
        
        case MSG_CMD_GET_CONFIG: {
            config_payload_t config;
            control_get_config(&config);
            protocol_send_config(&config);
            break;
        }
        
        case MSG_CMD_CONFIG: {
            // Variable payload based on config_type
            if (packet->length >= 1) {
                uint8_t config_type = packet->payload[0];
                
                if (config_type == CONFIG_ENVIRONMENTAL) {
                    // Set environmental configuration
                    if (packet->length >= sizeof(config_environmental_t) + 1) {
                        // Use memcpy for safe unaligned access (Cortex-M33 is stricter)
                        config_environmental_t env_cmd;
                        memcpy(&env_cmd, &packet->payload[1], sizeof(config_environmental_t));
                        
                        // Validate voltage: 100-250V (covers 110V and 220-240V systems)
                        if (env_cmd.nominal_voltage < 100 || env_cmd.nominal_voltage > 250) {
                            DEBUG_PRINT("CMD_CONFIG: Invalid voltage %d (valid: 100-250V)\n", 
                                       env_cmd.nominal_voltage);
                            protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_ERROR_INVALID);
                            break;
                        }
                        
                        // Validate max current: 1-50A (reasonable range for espresso machines)
                        if (env_cmd.max_current_draw < 1.0f || env_cmd.max_current_draw > 50.0f) {
                            DEBUG_PRINT("CMD_CONFIG: Invalid max current %.1f (valid: 1-50A)\n", 
                                       env_cmd.max_current_draw);
                            protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_ERROR_INVALID);
                            break;
                        }
                        
                        environmental_electrical_t env_config = {
                            .nominal_voltage = env_cmd.nominal_voltage,
                            .max_current_draw = env_cmd.max_current_draw
                        };
                        environmental_config_set(&env_config);
                        
                        // Save configuration to flash
                        persisted_config_t config;
                        config_persistence_get(&config);
                        config.environmental = env_config;
                        config_persistence_set(&config);
                        config_persistence_save();
                        
                        DEBUG_PRINT("Environmental config saved: %dV, %.1fA\n",
                                   env_config.nominal_voltage, env_config.max_current_draw);
                        
                        // Send ACK
                        protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_SUCCESS);
                        
                        // Send updated environmental config
                        env_config_payload_t env_resp;
                        environmental_electrical_t env;
                        environmental_config_get(&env);
                        electrical_state_t state;
                        electrical_state_get(&state);
                        
                        env_resp.nominal_voltage = env.nominal_voltage;
                        env_resp.max_current_draw = env.max_current_draw;
                        env_resp.brew_heater_current = state.brew_heater_current;
                        env_resp.steam_heater_current = state.steam_heater_current;
                        env_resp.max_combined_current = state.max_combined_current;
                        protocol_send_env_config(&env_resp);
                    } else {
                        protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_ERROR_INVALID);
                    }
                } else if (config_type == CONFIG_HEATING_STRATEGY) {
                    // Set heating strategy
                    if (packet->length >= 2) {
                        uint8_t strategy = packet->payload[1];
                        if (control_set_heating_strategy(strategy)) {
                            DEBUG_PRINT("CMD_CONFIG: Heating strategy set to %d\n", strategy);
                            protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_SUCCESS);
                        } else {
                            DEBUG_PRINT("CMD_CONFIG: Invalid heating strategy %d\n", strategy);
                            protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_ERROR_INVALID);
                        }
                    } else {
                        protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_ERROR_INVALID);
                    }
                } else if (config_type == CONFIG_PREINFUSION) {
                    // Set pre-infusion configuration
                    if (packet->length >= sizeof(config_preinfusion_t) + 1) {
                        // Use memcpy for safe unaligned access
                        config_preinfusion_t preinfusion_cmd;
                        memcpy(&preinfusion_cmd, &packet->payload[1], sizeof(config_preinfusion_t));
                        
                        // Validate timing parameters
                        // on_time_ms: 0-10000ms (0 = disabled effectively)
                        // pause_time_ms: 0-30000ms
                        if (preinfusion_cmd.on_time_ms > 10000) {
                            DEBUG_PRINT("CMD_CONFIG: Pre-infusion on_time too long %dms (max: 10000ms)\n", 
                                       preinfusion_cmd.on_time_ms);
                            protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_ERROR_INVALID);
                            break;
                        }
                        if (preinfusion_cmd.pause_time_ms > 30000) {
                            DEBUG_PRINT("CMD_CONFIG: Pre-infusion pause_time too long %dms (max: 30000ms)\n", 
                                       preinfusion_cmd.pause_time_ms);
                            protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_ERROR_INVALID);
                            break;
                        }
                        
                        // Apply pre-infusion settings
                        state_set_preinfusion(
                            preinfusion_cmd.enabled != 0,
                            preinfusion_cmd.on_time_ms,
                            preinfusion_cmd.pause_time_ms
                        );
                        
                        // Save configuration to flash
                        persisted_config_t config;
                        config_persistence_get(&config);
                        config.preinfusion_enabled = preinfusion_cmd.enabled != 0;
                        config.preinfusion_on_ms = preinfusion_cmd.on_time_ms;
                        config.preinfusion_pause_ms = preinfusion_cmd.pause_time_ms;
                        config_persistence_set(&config);
                        config_persistence_save();
                        
                        DEBUG_PRINT("Pre-infusion config saved: enabled=%d, on=%dms, pause=%dms\n",
                                   preinfusion_cmd.enabled, preinfusion_cmd.on_time_ms, 
                                   preinfusion_cmd.pause_time_ms);
                        
                        protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_SUCCESS);
                    } else {
                        DEBUG_PRINT("CMD_CONFIG: Pre-infusion payload too short (%d < %d)\n",
                                   packet->length, (int)(sizeof(config_preinfusion_t) + 1));
                        protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_ERROR_INVALID);
                    }
                } else {
                    // Other config types - just ACK for now
                    protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_SUCCESS);
                }
            }
            break;
        }
        
        case MSG_CMD_GET_ENV_CONFIG: {
            // Request environmental configuration
            env_config_payload_t env_resp;
            environmental_electrical_t env;
            environmental_config_get(&env);
            electrical_state_t state;
            electrical_state_get(&state);
            
            env_resp.nominal_voltage = env.nominal_voltage;
            env_resp.max_current_draw = env.max_current_draw;
            env_resp.brew_heater_current = state.brew_heater_current;
            env_resp.steam_heater_current = state.steam_heater_current;
            env_resp.max_combined_current = state.max_combined_current;
            protocol_send_env_config(&env_resp);
            break;
        }
        
        case MSG_CMD_CLEANING_START: {
            // Start cleaning cycle
            if (cleaning_start_cycle()) {
                protocol_send_ack(MSG_CMD_CLEANING_START, packet->seq, ACK_SUCCESS);
            } else {
                protocol_send_ack(MSG_CMD_CLEANING_START, packet->seq, ACK_ERROR_REJECTED);
            }
            break;
        }
        
        case MSG_CMD_CLEANING_STOP: {
            // Stop cleaning cycle
            cleaning_stop_cycle();
            protocol_send_ack(MSG_CMD_CLEANING_STOP, packet->seq, ACK_SUCCESS);
            break;
        }
        
        case MSG_CMD_CLEANING_RESET: {
            // Reset brew counter
            cleaning_reset_brew_count();
            protocol_send_ack(MSG_CMD_CLEANING_RESET, packet->seq, ACK_SUCCESS);
            break;
        }
        
        case MSG_CMD_CLEANING_SET_THRESHOLD: {
            // Set cleaning reminder threshold
            if (packet->length >= 2) {
                uint16_t threshold = (packet->payload[0] << 8) | packet->payload[1];
                
                // Validate threshold range (cleaning_set_threshold also validates, but add debug)
                if (threshold < 10 || threshold > 1000) {
                    DEBUG_PRINT("CMD_CLEANING_SET_THRESHOLD: Invalid threshold %d (valid: 10-1000)\n", 
                               threshold);
                    protocol_send_ack(MSG_CMD_CLEANING_SET_THRESHOLD, packet->seq, ACK_ERROR_INVALID);
                    break;
                }
                
                if (cleaning_set_threshold(threshold)) {
                    protocol_send_ack(MSG_CMD_CLEANING_SET_THRESHOLD, packet->seq, ACK_SUCCESS);
                } else {
                    protocol_send_ack(MSG_CMD_CLEANING_SET_THRESHOLD, packet->seq, ACK_ERROR_INVALID);
                }
            } else {
                protocol_send_ack(MSG_CMD_CLEANING_SET_THRESHOLD, packet->seq, ACK_ERROR_INVALID);
            }
            break;
        }
        
        case MSG_CMD_SET_ECO: {
            // Set eco mode configuration
            // Payload: [enabled:1][eco_brew_temp:2][timeout_minutes:2]
            if (packet->length >= 5) {
                bool enabled = packet->payload[0] != 0;
                int16_t eco_temp = (int16_t)((packet->payload[1] << 8) | packet->payload[2]);
                uint16_t timeout = (packet->payload[3] << 8) | packet->payload[4];
                
                // Validate eco temp: 500-900 (50.0-90.0°C) - must be lower than normal brew temp
                if (eco_temp < 500 || eco_temp > 900) {
                    DEBUG_PRINT("CMD_SET_ECO: Invalid eco temp %d (valid: 500-900)\n", eco_temp);
                    protocol_send_ack(MSG_CMD_SET_ECO, packet->seq, ACK_ERROR_INVALID);
                    break;
                }
                
                // Validate timeout: 0-480 minutes (0 = disabled, max 8 hours)
                if (timeout > 480) {
                    DEBUG_PRINT("CMD_SET_ECO: Invalid timeout %d (valid: 0-480 min)\n", timeout);
                    protocol_send_ack(MSG_CMD_SET_ECO, packet->seq, ACK_ERROR_INVALID);
                    break;
                }
                
                eco_config_t eco_config = {
                    .enabled = enabled,
                    .eco_brew_temp = eco_temp,
                    .timeout_minutes = timeout
                };
                state_set_eco_config(&eco_config);
                
                DEBUG_PRINT("Eco config set: enabled=%d, temp=%d, timeout=%d min\n",
                           enabled, eco_temp, timeout);
                protocol_send_ack(MSG_CMD_SET_ECO, packet->seq, ACK_SUCCESS);
            } else if (packet->length >= 1) {
                // Single byte payload: enable/disable or enter/exit eco mode
                uint8_t cmd = packet->payload[0];
                if (cmd == 0) {
                    // Exit eco mode
                    if (state_exit_eco()) {
                        protocol_send_ack(MSG_CMD_SET_ECO, packet->seq, ACK_SUCCESS);
                    } else {
                        protocol_send_ack(MSG_CMD_SET_ECO, packet->seq, ACK_ERROR_REJECTED);
                    }
                } else if (cmd == 1) {
                    // Enter eco mode
                    if (state_enter_eco()) {
                        protocol_send_ack(MSG_CMD_SET_ECO, packet->seq, ACK_SUCCESS);
                    } else {
                        protocol_send_ack(MSG_CMD_SET_ECO, packet->seq, ACK_ERROR_REJECTED);
                    }
                } else {
                    protocol_send_ack(MSG_CMD_SET_ECO, packet->seq, ACK_ERROR_INVALID);
                }
            } else {
                protocol_send_ack(MSG_CMD_SET_ECO, packet->seq, ACK_ERROR_INVALID);
            }
            break;
        }
        
        case MSG_CMD_BOOTLOADER: {
            // Enter bootloader mode and receive firmware
            DEBUG_PRINT("Entering bootloader mode!\n");
            protocol_send_ack(MSG_CMD_BOOTLOADER, packet->seq, ACK_SUCCESS);
            
            // Small delay to ensure ACK is sent
            sleep_ms(50);
            
            // Enter bootloader mode (does not return on success)
            bootloader_result_t result = bootloader_receive_firmware();
            
            // If we get here, bootloader failed
            DEBUG_PRINT("Bootloader error: %s\n", bootloader_get_status_message(result));
            
            // Send error ACK with appropriate error code
            uint8_t ack_result = ACK_ERROR_FAILED;
            if (result == BOOTLOADER_ERROR_TIMEOUT) {
                ack_result = ACK_ERROR_TIMEOUT;
            }
            // Note: Can't send ACK here as we're back in normal protocol mode
            // Send error message via debug instead
            char error_msg[64];
            snprintf(error_msg, sizeof(error_msg), "Bootloader failed: %s", 
                     bootloader_get_status_message(result));
            protocol_send_debug(error_msg);
            break;
        }
        
        case MSG_CMD_DIAGNOSTICS: {
            // Run diagnostic tests
            uint8_t test_id = DIAG_TEST_ALL;
            if (packet->length >= 1) {
                test_id = packet->payload[0];
            }
            
            DEBUG_PRINT("Running diagnostics (test_id=%d)\n", test_id);
            protocol_send_ack(MSG_CMD_DIAGNOSTICS, packet->seq, ACK_SUCCESS);
            
            if (test_id == DIAG_TEST_ALL) {
                // Run all tests
                diag_report_t report;
                diagnostics_run_all(&report);
                
                // Send header first
                diag_header_payload_t header = {
                    .test_count = report.test_count,
                    .pass_count = report.pass_count,
                    .fail_count = report.fail_count,
                    .warn_count = report.warn_count,
                    .skip_count = report.skip_count,
                    .is_complete = 0,  // More results coming
                    .duration_ms = (uint16_t)(report.duration_ms > 65535 ? 65535 : report.duration_ms)
                };
                protocol_send_diag_header(&header);
                
                // Send each result
                for (int i = 0; i < report.test_count && i < 16; i++) {
                    diag_result_t* r = &report.results[i];
                    diag_result_payload_t result_payload = {
                        .test_id = r->test_id,
                        .status = r->status,
                        .raw_value = r->raw_value,
                        .expected_min = r->expected_min,
                        .expected_max = r->expected_max
                    };
                    strncpy(result_payload.message, r->message, sizeof(result_payload.message) - 1);
                    result_payload.message[sizeof(result_payload.message) - 1] = '\0';
                    protocol_send_diag_result(&result_payload);
                    
                    // Small delay between packets
                    sleep_ms(10);
                }
                
                // Send final header with is_complete=1
                header.is_complete = 1;
                protocol_send_diag_header(&header);
            } else {
                // Run single test
                diag_result_t single_result;
                diagnostics_run_test(test_id, &single_result);
                
                // Send header
                diag_header_payload_t header = {
                    .test_count = 1,
                    .pass_count = (single_result.status == DIAG_STATUS_PASS) ? 1 : 0,
                    .fail_count = (single_result.status == DIAG_STATUS_FAIL) ? 1 : 0,
                    .warn_count = (single_result.status == DIAG_STATUS_WARN) ? 1 : 0,
                    .skip_count = (single_result.status == DIAG_STATUS_SKIP) ? 1 : 0,
                    .is_complete = 0,
                    .duration_ms = 0
                };
                protocol_send_diag_header(&header);
                
                // Send result
                diag_result_payload_t result_payload = {
                    .test_id = single_result.test_id,
                    .status = single_result.status,
                    .raw_value = single_result.raw_value,
                    .expected_min = single_result.expected_min,
                    .expected_max = single_result.expected_max
                };
                strncpy(result_payload.message, single_result.message, sizeof(result_payload.message) - 1);
                result_payload.message[sizeof(result_payload.message) - 1] = '\0';
                protocol_send_diag_result(&result_payload);
                
                // Send final header
                header.is_complete = 1;
                protocol_send_diag_header(&header);
            }
            break;
        }
        
        default:
            DEBUG_PRINT("Unknown packet type: 0x%02X\n", packet->type);
            break;
    }
}

// -----------------------------------------------------------------------------
// Core 0 Entry Point (Control)
// -----------------------------------------------------------------------------
int main(void) {
    // Record boot time
    g_boot_time = to_ms_since_boot(get_absolute_time());
    
    // Initialize mutex for thread-safe status sharing between cores
    mutex_init(&g_status_mutex);
    
    // Initialize stdio (USB serial for logging)
    stdio_init_all();
    sleep_ms(100);  // Brief delay for USB enumeration
    
    // Always print boot banner to USB serial
    LOG_PRINT("\n========================================\n");
    LOG_PRINT("ECM Pico Controller v%d.%d.%d\n", 
                FIRMWARE_VERSION_MAJOR, 
                FIRMWARE_VERSION_MINOR, 
                FIRMWARE_VERSION_PATCH);
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
            
            if (safety == SAFETY_CRITICAL) {
                // Enter safe state - all outputs off
                safety_enter_safe_state();
                protocol_send_alarm(safety_get_last_alarm(), 2, 0);
            } else if (safety == SAFETY_FAULT) {
                // Warning condition - may continue with limits
                protocol_send_alarm(safety_get_last_alarm(), 1, 0);
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
            
            // Update status for Core 1 (thread-safe with mutex)
            sensor_data_t sensor_data;
            sensors_get_data(&sensor_data);
            
            control_outputs_t outputs;
            control_get_outputs(&outputs);
            
            // Build status locally first, then copy under mutex
            status_payload_t new_status = {0};
            
            // Machine-type aware status population
            // HX machines: brew_temp is invalid (no brew NTC), use group_temp
            // Single boiler: steam_temp is invalid (no steam NTC)
            const machine_features_t* machine_features = machine_get_features();
            
            if (machine_features && machine_features->type == MACHINE_TYPE_HEAT_EXCHANGER) {
                // HX: brew_temp not available, report group_temp as brew indicator
                new_status.brew_temp = sensor_data.group_temp;  // Use group as brew proxy
                new_status.steam_temp = sensor_data.steam_temp;
                new_status.group_temp = sensor_data.group_temp;
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
            
            // Thread-safe update of shared status
            mutex_enter_blocking(&g_status_mutex);
            memcpy(&g_status, &new_status, sizeof(status_payload_t));
            g_status_updated = true;
            mutex_exit(&g_status_mutex);
        }
        
        // Small sleep
        sleep_us(100);
    }
    
    return 0;
}

