/**
 * Packet Handler Module Implementation
 * 
 * Breaks down large packet handling into smaller, testable functions.
 * Includes validation and bounds checking.
 */

#include "packet_handlers.h"
#include "validation.h"
#include "logging.h"
#include "config.h"
#include "control.h"
#include "state.h"
#include "config_persistence.h"
#include "environmental_config.h"
#include "cleaning.h"
#include "bootloader.h"
#include "diagnostics.h"
#include "power_meter.h"
#include "log_forward.h"
#include "safety.h"
#include "pico/stdlib.h"  // For sleep_ms
#include <string.h>
#include <stdio.h>  // For snprintf
// =============================================================================
// Helper: Safe Memory Copy with Validation
// =============================================================================

static inline bool safe_memcpy(void* dest, const void* src, size_t size, size_t dest_size) {
    validation_result_t result = validate_buffer_copy(dest, src, size, dest_size);
    if (result != VALIDATION_OK) {
        LOG_ERROR("Buffer validation failed: %s\n", validation_error_string(result));
        return false;
    }
    memcpy(dest, src, size);
    return true;
}

// =============================================================================
// Individual Packet Handlers
// =============================================================================

void handle_cmd_ping(const packet_t* packet) {
    protocol_send_ack(MSG_PING, packet->seq, ACK_SUCCESS);
}

void handle_cmd_set_temp(const packet_t* packet) {
    if (packet->length < sizeof(cmd_set_temp_t)) {
        LOG_WARN("SET_TEMP: Payload too short (%d < %d)\n", 
                 packet->length, (int)sizeof(cmd_set_temp_t));
        protocol_send_ack(MSG_CMD_SET_TEMP, packet->seq, ACK_ERROR_INVALID);
        return;
    }
    
    cmd_set_temp_t cmd;
    if (!safe_memcpy(&cmd, packet->payload, sizeof(cmd_set_temp_t), sizeof(cmd_set_temp_t))) {
        protocol_send_ack(MSG_CMD_SET_TEMP, packet->seq, ACK_ERROR_INVALID);
        return;
    }
    
    // Validate target
    validation_result_t result = validate_setpoint_target(cmd.target);
    if (result != VALIDATION_OK) {
        LOG_WARN("SET_TEMP: Invalid target %d: %s\n", cmd.target, validation_error_string(result));
        protocol_send_ack(MSG_CMD_SET_TEMP, packet->seq, ACK_ERROR_INVALID);
        return;
    }
    
    // Validate temperature
    result = validate_temperature(cmd.temperature, 0, 2000);
    if (result != VALIDATION_OK) {
        LOG_WARN("SET_TEMP: Invalid temperature %d: %s\n", cmd.temperature, validation_error_string(result));
        protocol_send_ack(MSG_CMD_SET_TEMP, packet->seq, ACK_ERROR_INVALID);
        return;
    }
    
    // Apply setpoint
    control_set_setpoint(cmd.target, cmd.temperature);
    
    // Save to flash
    persisted_config_t config;
    config_persistence_get(&config);
    if (cmd.target == 0) {
        config.brew_setpoint = cmd.temperature;
    } else {
        config.steam_setpoint = cmd.temperature;
    }
    config_persistence_set(&config);
    config_persistence_save();
    
    LOG_INFO("Setpoint updated: %s=%d.%d°C\n", 
             cmd.target == 0 ? "brew" : "steam",
             cmd.temperature / 10, cmd.temperature % 10);
    
    protocol_send_ack(MSG_CMD_SET_TEMP, packet->seq, ACK_SUCCESS);
}

void handle_cmd_set_pid(const packet_t* packet) {
    if (packet->length < sizeof(cmd_set_pid_t)) {
        LOG_WARN("SET_PID: Payload too short\n");
        protocol_send_ack(MSG_CMD_SET_PID, packet->seq, ACK_ERROR_INVALID);
        return;
    }
    
    cmd_set_pid_t cmd;
    if (!safe_memcpy(&cmd, packet->payload, sizeof(cmd_set_pid_t), sizeof(cmd_set_pid_t))) {
        protocol_send_ack(MSG_CMD_SET_PID, packet->seq, ACK_ERROR_INVALID);
        return;
    }
    
    // Validate target
    validation_result_t result = validate_setpoint_target(cmd.target);
    if (result != VALIDATION_OK) {
        LOG_WARN("SET_PID: Invalid target %d\n", cmd.target);
        protocol_send_ack(MSG_CMD_SET_PID, packet->seq, ACK_ERROR_INVALID);
        return;
    }
    
    // Validate PID gains
    result = validate_pid_gains(cmd.kp, cmd.ki, cmd.kd);
    if (result != VALIDATION_OK) {
        LOG_WARN("SET_PID: Invalid gains Kp=%d Ki=%d Kd=%d\n", cmd.kp, cmd.ki, cmd.kd);
        protocol_send_ack(MSG_CMD_SET_PID, packet->seq, ACK_ERROR_INVALID);
        return;
    }
    
    // Apply PID gains
    control_set_pid(cmd.target, cmd.kp / 100.0f, cmd.ki / 100.0f, cmd.kd / 100.0f);
    
    LOG_INFO("PID updated: %s Kp=%.2f Ki=%.2f Kd=%.2f\n",
             cmd.target == 0 ? "brew" : "steam",
             cmd.kp / 100.0f, cmd.ki / 100.0f, cmd.kd / 100.0f);
    
    protocol_send_ack(MSG_CMD_SET_PID, packet->seq, ACK_SUCCESS);
}

void handle_cmd_brew(const packet_t* packet) {
    if (packet->length < sizeof(cmd_brew_t)) {
        protocol_send_ack(MSG_CMD_BREW, packet->seq, ACK_ERROR_INVALID);
        return;
    }
    
    cmd_brew_t cmd;
    if (!safe_memcpy(&cmd, packet->payload, sizeof(cmd_brew_t), sizeof(cmd_brew_t))) {
        protocol_send_ack(MSG_CMD_BREW, packet->seq, ACK_ERROR_INVALID);
        return;
    }
    
    if (cmd.action) {
        state_start_brew();
        LOG_INFO("Brew started\n");
    } else {
        state_stop_brew();
        LOG_INFO("Brew stopped\n");
    }
    
    protocol_send_ack(MSG_CMD_BREW, packet->seq, ACK_SUCCESS);
}

void handle_cmd_mode(const packet_t* packet) {
    if (packet->length < sizeof(cmd_mode_t)) {
        protocol_send_ack(MSG_CMD_MODE, packet->seq, ACK_ERROR_INVALID);
        return;
    }
    
    cmd_mode_t cmd;
    if (!safe_memcpy(&cmd, packet->payload, sizeof(cmd_mode_t), sizeof(cmd_mode_t))) {
        protocol_send_ack(MSG_CMD_MODE, packet->seq, ACK_ERROR_INVALID);
        return;
    }
    
    machine_mode_t mode = (machine_mode_t)cmd.mode;
    if (mode > MODE_STEAM) {
        LOG_WARN("MODE: Invalid mode %d\n", cmd.mode);
        protocol_send_ack(MSG_CMD_MODE, packet->seq, ACK_ERROR_INVALID);
        return;
    }
    
    if (state_set_mode(mode)) {
        LOG_INFO("Mode changed to %d\n", mode);
        protocol_send_ack(MSG_CMD_MODE, packet->seq, ACK_SUCCESS);
    } else {
        LOG_WARN("MODE: Mode change rejected\n");
        protocol_send_ack(MSG_CMD_MODE, packet->seq, ACK_ERROR_REJECTED);
    }
}

void handle_cmd_get_config(const packet_t* packet) {
    config_payload_t config;
    control_get_config(&config);
    protocol_send_config(&config);
}

void handle_cmd_config(const packet_t* packet) {
    if (packet->length < 1) {
        protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_ERROR_INVALID);
        return;
    }
    
    uint8_t config_type = packet->payload[0];
    
    if (config_type == CONFIG_ENVIRONMENTAL) {
        if (packet->length < sizeof(config_environmental_t) + 1) {
            protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_ERROR_INVALID);
            return;
        }
        
        config_environmental_t env_cmd;
        if (!safe_memcpy(&env_cmd, &packet->payload[1], sizeof(config_environmental_t), 
                        packet->length - 1)) {
            protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_ERROR_INVALID);
            return;
        }
        
        // Validate voltage and current
        validation_result_t result = validate_voltage(env_cmd.nominal_voltage);
        if (result != VALIDATION_OK) {
            LOG_WARN("CONFIG_ENV: Invalid voltage %d\n", env_cmd.nominal_voltage);
            protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_ERROR_INVALID);
            return;
        }
        
        result = validate_current(env_cmd.max_current_draw);
        if (result != VALIDATION_OK) {
            LOG_WARN("CONFIG_ENV: Invalid current %.1fA\n", env_cmd.max_current_draw);
            protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_ERROR_INVALID);
            return;
        }
        
        // Apply and save
        environmental_electrical_t env_config = {
            .nominal_voltage = env_cmd.nominal_voltage,
            .max_current_draw = env_cmd.max_current_draw
        };
        environmental_config_set(&env_config);
        
        persisted_config_t config;
        config_persistence_get(&config);
        config.environmental = env_config;
        config_persistence_set(&config);
        config_persistence_save();
        
        LOG_INFO("Environmental config saved: %dV, %.1fA\n",
                 env_config.nominal_voltage, env_config.max_current_draw);
        
        protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_SUCCESS);
        
        // Send updated config
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
        
    } else if (config_type == CONFIG_HEATING_STRATEGY) {
        if (packet->length < 2) {
            protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_ERROR_INVALID);
            return;
        }
        
        uint8_t strategy = packet->payload[1];
        if (control_set_heating_strategy(strategy)) {
            LOG_INFO("Heating strategy set to %d\n", strategy);
            protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_SUCCESS);
        } else {
            LOG_WARN("Invalid heating strategy %d\n", strategy);
            protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_ERROR_INVALID);
        }
        
    } else if (config_type == CONFIG_PREINFUSION) {
        if (packet->length < sizeof(config_preinfusion_t) + 1) {
            protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_ERROR_INVALID);
            return;
        }
        
        config_preinfusion_t preinfusion_cmd;
        if (!safe_memcpy(&preinfusion_cmd, &packet->payload[1], sizeof(config_preinfusion_t),
                        packet->length - 1)) {
            protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_ERROR_INVALID);
            return;
        }
        
        // Validate timing
        validation_result_t result = validate_preinfusion_timing(
            preinfusion_cmd.on_time_ms, preinfusion_cmd.pause_time_ms);
        if (result != VALIDATION_OK) {
            LOG_WARN("CONFIG_PREINFUSION: Invalid timing on=%d pause=%d\n",
                     preinfusion_cmd.on_time_ms, preinfusion_cmd.pause_time_ms);
            protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_ERROR_INVALID);
            return;
        }
        
        // Apply and save
        state_set_preinfusion(
            preinfusion_cmd.enabled != 0,
            preinfusion_cmd.on_time_ms,
            preinfusion_cmd.pause_time_ms
        );
        
        persisted_config_t config;
        config_persistence_get(&config);
        config.preinfusion_enabled = preinfusion_cmd.enabled != 0;
        config.preinfusion_on_ms = preinfusion_cmd.on_time_ms;
        config.preinfusion_pause_ms = preinfusion_cmd.pause_time_ms;
        config_persistence_set(&config);
        config_persistence_save();
        
        LOG_INFO("Pre-infusion config saved: enabled=%d, on=%dms, pause=%dms\n",
                 preinfusion_cmd.enabled, preinfusion_cmd.on_time_ms, 
                 preinfusion_cmd.pause_time_ms);
        
        protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_SUCCESS);
        
    } else if (config_type == CONFIG_MACHINE_INFO) {
        if (packet->length < sizeof(config_machine_info_t) + 1) {
            protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_ERROR_INVALID);
            return;
        }
        
        config_machine_info_t machine_info_cmd;
        if (!safe_memcpy(&machine_info_cmd, &packet->payload[1], sizeof(config_machine_info_t),
                        packet->length - 1)) {
            protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_ERROR_INVALID);
            return;
        }
        
        // Ensure null termination (safety)
        machine_info_cmd.brand[sizeof(machine_info_cmd.brand) - 1] = '\0';
        machine_info_cmd.model[sizeof(machine_info_cmd.model) - 1] = '\0';
        
        // Save to flash (source of truth on Pico)
        if (config_persistence_save_machine_info(machine_info_cmd.brand, machine_info_cmd.model)) {
            LOG_INFO("Machine info saved: %s %s\n", machine_info_cmd.brand, machine_info_cmd.model);
            protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_SUCCESS);
        } else {
            LOG_WARN("Failed to save machine info\n");
            protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_ERROR_INVALID);
        }
        
    } else {
        // Unknown config type
        protocol_send_ack(MSG_CMD_CONFIG, packet->seq, ACK_SUCCESS);
    }
}

void handle_cmd_get_env_config(const packet_t* packet) {
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
}

void handle_cmd_cleaning(const packet_t* packet) {
    switch (packet->type) {
        case MSG_CMD_CLEANING_START:
            if (cleaning_start_cycle()) {
                protocol_send_ack(MSG_CMD_CLEANING_START, packet->seq, ACK_SUCCESS);
            } else {
                protocol_send_ack(MSG_CMD_CLEANING_START, packet->seq, ACK_ERROR_REJECTED);
            }
            break;
            
        case MSG_CMD_CLEANING_STOP:
            cleaning_stop_cycle();
            protocol_send_ack(MSG_CMD_CLEANING_STOP, packet->seq, ACK_SUCCESS);
            break;
            
        case MSG_CMD_CLEANING_RESET:
            cleaning_reset_brew_count();
            protocol_send_ack(MSG_CMD_CLEANING_RESET, packet->seq, ACK_SUCCESS);
            break;
            
        case MSG_CMD_CLEANING_SET_THRESHOLD:
            if (packet->length >= 2) {
                uint16_t threshold = (packet->payload[0] << 8) | packet->payload[1];
                if (threshold < 10 || threshold > 1000) {
                    LOG_WARN("CLEANING_THRESHOLD: Invalid value %d\n", threshold);
                    protocol_send_ack(MSG_CMD_CLEANING_SET_THRESHOLD, packet->seq, ACK_ERROR_INVALID);
                } else if (cleaning_set_threshold(threshold)) {
                    protocol_send_ack(MSG_CMD_CLEANING_SET_THRESHOLD, packet->seq, ACK_SUCCESS);
                } else {
                    protocol_send_ack(MSG_CMD_CLEANING_SET_THRESHOLD, packet->seq, ACK_ERROR_INVALID);
                }
            } else {
                protocol_send_ack(MSG_CMD_CLEANING_SET_THRESHOLD, packet->seq, ACK_ERROR_INVALID);
            }
            break;
    }
}

// Placeholder implementations for other handlers
void handle_cmd_get_statistics(const packet_t* packet) {
    // Statistics handled by ESP32
    protocol_send_ack(MSG_CMD_GET_STATISTICS, packet->seq, ACK_SUCCESS);
}

void handle_cmd_debug(const packet_t* packet) {
    // Debug command handler - implementation depends on requirements
    protocol_send_ack(MSG_CMD_DEBUG, packet->seq, ACK_SUCCESS);
}

void handle_cmd_set_eco(const packet_t* packet) {
    if (packet->length >= 5) {
        bool enabled = packet->payload[0] != 0;
        int16_t eco_temp = (int16_t)((packet->payload[1] << 8) | packet->payload[2]);
        uint16_t timeout = (packet->payload[3] << 8) | packet->payload[4];
        
        // Validate
        if (eco_temp < 500 || eco_temp > 900) {
            LOG_WARN("SET_ECO: Invalid eco temp %d\n", eco_temp);
            protocol_send_ack(MSG_CMD_SET_ECO, packet->seq, ACK_ERROR_INVALID);
            return;
        }
        
        if (timeout > 480) {
            LOG_WARN("SET_ECO: Invalid timeout %d\n", timeout);
            protocol_send_ack(MSG_CMD_SET_ECO, packet->seq, ACK_ERROR_INVALID);
            return;
        }
        
        eco_config_t eco_config = {
            .enabled = enabled,
            .eco_brew_temp = eco_temp,
            .timeout_minutes = timeout
        };
        state_set_eco_config(&eco_config);
        
        LOG_INFO("Eco config set: enabled=%d, temp=%d, timeout=%d min\n",
                 enabled, eco_temp, timeout);
        protocol_send_ack(MSG_CMD_SET_ECO, packet->seq, ACK_SUCCESS);
        
    } else if (packet->length >= 1) {
        uint8_t cmd = packet->payload[0];
        if (cmd == 0) {
            if (state_exit_eco()) {
                protocol_send_ack(MSG_CMD_SET_ECO, packet->seq, ACK_SUCCESS);
            } else {
                protocol_send_ack(MSG_CMD_SET_ECO, packet->seq, ACK_ERROR_REJECTED);
            }
        } else if (cmd == 1) {
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
}

void handle_cmd_bootloader(const packet_t* packet) {
    LOG_INFO("Entering bootloader mode\n");
    protocol_send_ack(MSG_CMD_BOOTLOADER, packet->seq, ACK_SUCCESS);
    
    // Small delay to ensure ACK is sent
    sleep_ms(50);
    
    // Signal bootloader mode - pauses Core 0 control loop and protocol processing
    bootloader_prepare();
    LOG_INFO("Bootloader: System paused, starting firmware receive\n");
    
    // Enter bootloader mode (does not return on success)
    bootloader_result_t result = bootloader_receive_firmware();
    
    // If we get here, bootloader failed - resume normal operation
    bootloader_exit();
    LOG_ERROR("Bootloader error: %s\n", bootloader_get_status_message(result));
    
    // Send error message via debug
    char error_msg[64];
    snprintf(error_msg, sizeof(error_msg), "Bootloader failed: %s", 
             bootloader_get_status_message(result));
    protocol_send_debug(error_msg);
}

void handle_cmd_diagnostics(const packet_t* packet) {
    uint8_t test_id = DIAG_TEST_ALL;
    if (packet->length >= 1) {
        test_id = packet->payload[0];
    }
    
    LOG_INFO("Running diagnostics (test_id=%d)\n", test_id);
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
            .is_complete = 0,
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
            
            sleep_ms(10);
        }
        
        // Send final header with is_complete=1
        header.is_complete = 1;
        protocol_send_diag_header(&header);
    } else {
        // Run single test
        diag_result_t single_result;
        diagnostics_run_test(test_id, &single_result);
        
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
        
        header.is_complete = 1;
        protocol_send_diag_header(&header);
    }
}

void handle_cmd_power_meter(const packet_t* packet) {
    // Power meter configuration
    protocol_send_ack(packet->type, packet->seq, ACK_SUCCESS);
}

void handle_cmd_get_boot(const packet_t* packet) {
    protocol_send_boot();
}

void handle_cmd_log_config(const packet_t* packet) {
    log_forward_handle_command(packet->payload, packet->length);
    protocol_send_ack(MSG_CMD_LOG_CONFIG, packet->seq, ACK_SUCCESS);
}
