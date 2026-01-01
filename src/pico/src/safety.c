/**
 * Pico Firmware - Safety System
 * 
 * Critical safety functions that run first and can override all other control.
 * Implements all safety requirements (SAF-001 through SAF-034).
 */

#include "pico/stdlib.h"
#include "pico/platform.h"  // For __not_in_flash_func()
#include "hardware/watchdog.h"
#include "safety.h"
#include "config.h"
#include "sensors.h"
#include "control.h"
#include "hardware.h"
#include "pcb_config.h"
#include "machine_config.h"
#include "protocol.h"
#include "config_persistence.h"
#include "state.h"
#include "bootloader.h"
#include <math.h>

// =============================================================================
// Safety Thresholds
// =============================================================================

#define SAFETY_BREW_MAX_TEMP_C        130.0f   // SAF-020: Brew boiler max
#define SAFETY_STEAM_MAX_TEMP_C       165.0f   // SAF-021: Steam boiler max
#define SAFETY_NTC_OPEN_CIRCUIT_C     150.0f   // SAF-023: NTC open circuit threshold
#define SAFETY_NTC_SHORT_CIRCUIT_C    -20.0f   // SAF-024: NTC short circuit threshold
#define SAFETY_TEMP_HYSTERESIS_C      10.0f    // SAF-025: Re-enable 10°C below max

#define SAFETY_SSR_MAX_DUTY           95       // SAF-032: Maximum SSR duty cycle
#define SAFETY_SSR_MAX_ON_TIME_MS      60000    // SAF-031: Max SSR on time without temp change

// Water sensor debounce (SAF-013)
#define WATER_SENSOR_DEBOUNCE_SAMPLES 5        // 5 samples = 250ms at 20Hz
#define WATER_SENSOR_DEBOUNCE_MS      250      // Total debounce time

// LED blink rate in safe state (2Hz = 500ms period)
#define SAFE_STATE_LED_PERIOD_MS      500
#define BUZZER_BEEP_COUNT             3
#define BUZZER_BEEP_DURATION_MS       200

// =============================================================================
// Private State
// =============================================================================

static bool g_safe_state = false;
static uint8_t g_safety_flags = 0;
static uint8_t g_last_alarm = ALARM_NONE;
static uint32_t g_last_esp32_heartbeat = 0;
static uint32_t g_safe_state_entered_time = 0;
static bool g_defensive_mode = false;  // True when ESP32 not connected - machine forced to IDLE

// Water sensor debounce state
static uint8_t g_reservoir_debounce_count = 0;
static uint8_t g_tank_level_debounce_count = 0;
static uint8_t g_steam_level_debounce_count = 0;
static bool g_reservoir_state = true;  // Assume OK initially
static bool g_tank_level_state = true;
static bool g_steam_level_state = true;

// Over-temperature state (for hysteresis)
static bool g_brew_over_temp = false;
static bool g_steam_over_temp = false;

// SSR monitoring (SAF-031)
static uint32_t g_brew_ssr_on_time = 0;
static uint32_t g_steam_ssr_on_time = 0;
static float g_brew_temp_when_on = 0.0f;
static float g_steam_temp_when_on = 0.0f;

// Safe state UI (LED and buzzer)
static uint32_t g_led_last_toggle = 0;
static bool g_led_state = false;
static uint8_t g_buzzer_beep_count = 0;
static uint32_t g_buzzer_last_beep = 0;

// Rate-limiting for safety log messages (avoid spamming during faults)
// Messages only print once per SAFETY_MSG_RATE_LIMIT_MS while condition persists
#define SAFETY_MSG_RATE_LIMIT_MS  5000  // 5 seconds between repeated messages
static uint32_t g_last_reservoir_msg = 0;
static uint32_t g_last_tank_msg = 0;
static uint32_t g_last_steam_level_msg = 0;


// =============================================================================
// Helper Functions
// =============================================================================

/**
 * Debounce water sensor input
 * Returns true if sensor indicates low water (after debounce)
 * 
 * @param gpio_pin GPIO pin number (use int8_t to support -1 for unconfigured)
 * @param debounce_count Pointer to debounce counter
 * @param last_state Pointer to last stable state
 * @return true if sensor indicates low water (after debounce)
 */
static bool check_water_sensor_debounced(int8_t gpio_pin, uint8_t* debounce_count, bool* last_state) {
    if (!PIN_VALID(gpio_pin)) {
        return false;  // Not configured or invalid pin
    }
    
    bool current_state = !hw_read_gpio((uint8_t)gpio_pin);  // Active LOW, so invert
    
    if (current_state != *last_state) {
        // State changed, reset debounce
        *debounce_count = 0;
        *last_state = current_state;
    } else {
        // State same, increment debounce
        (*debounce_count)++;
        if (*debounce_count >= WATER_SENSOR_DEBOUNCE_SAMPLES) {
            // Debounced - return true if low water
            return current_state;
        }
    }
    
    return *last_state;  // Return last stable state
}

/**
 * Check if NTC sensor has fault (open or short circuit)
 */
static bool check_ntc_fault(float temp_c) {
    if (isnan(temp_c) || isinf(temp_c)) {
        return true;  // Invalid reading
    }
    
    // Check for open circuit (SAF-023)
    if (temp_c > SAFETY_NTC_OPEN_CIRCUIT_C) {
        return true;
    }
    
    // Check for short circuit (SAF-024)
    if (temp_c < SAFETY_NTC_SHORT_CIRCUIT_C) {
        return true;
    }
    
    return false;
}

/**
 * Disable all outputs (SAF-030, SAF-004)
 */
static void disable_all_outputs(void) {
    // Disable heaters via control module
    control_set_output(0, 0, 1);  // Brew heater OFF (manual mode)
    control_set_output(1, 0, 1);  // Steam heater OFF (manual mode)
    control_set_pump(0);          // Pump OFF
    
    // Disable relays via hardware
    const pcb_config_t* pcb = pcb_config_get();
    if (pcb) {
        if (pcb->pins.relay_pump >= 0) {
            hw_set_gpio(pcb->pins.relay_pump, false);
        }
        if (pcb->pins.relay_brew_solenoid >= 0) {
            hw_set_gpio(pcb->pins.relay_brew_solenoid, false);
        }
        if (pcb->pins.relay_water_led >= 0) {
            hw_set_gpio(pcb->pins.relay_water_led, false);
        }
        if (pcb->pins.relay_spare >= 0) {
            hw_set_gpio(pcb->pins.relay_spare, false);
        }
        
        // Disable SSR via PWM
        if (pcb->pins.ssr_brew >= 0) {
            // Note: PWM would need to be initialized first
            // For now, control module handles this via output = 0
        }
        if (pcb->pins.ssr_steam >= 0) {
            // Same as above
        }
    }
}

/**
 * Update safe state UI (LED blinking and buzzer)
 */
static void update_safe_state_ui(void) {
    const pcb_config_t* pcb = pcb_config_get();
    if (!pcb) {
        return;
    }
    
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    // Blink status LED at 2Hz (SAF-030)
    if (pcb->pins.led_status >= 0) {
        if (now - g_led_last_toggle >= SAFE_STATE_LED_PERIOD_MS / 2) {
            g_led_state = !g_led_state;
            hw_set_gpio(pcb->pins.led_status, g_led_state);
            g_led_last_toggle = now;
        }
    }
    
    // Buzzer: 3 beeps then silent (SAF-030)
    if (pcb->pins.buzzer >= 0 && g_buzzer_beep_count < BUZZER_BEEP_COUNT) {
        if (now - g_buzzer_last_beep >= BUZZER_BEEP_DURATION_MS * 2) {
            // Toggle buzzer for beep
            hw_toggle_gpio(pcb->pins.buzzer);
            g_buzzer_beep_count++;
            g_buzzer_last_beep = now;
        }
    }
}

// =============================================================================
// Initialization
// =============================================================================

void safety_init(void) {
    g_safe_state = false;
    g_safety_flags = 0;
    g_last_alarm = ALARM_NONE;
    g_last_esp32_heartbeat = to_ms_since_boot(get_absolute_time());
    g_safe_state_entered_time = 0;
    
    // Initialize debounce state
    g_reservoir_debounce_count = 0;
    g_tank_level_debounce_count = 0;
    g_steam_level_debounce_count = 0;
    g_reservoir_state = true;
    g_tank_level_state = true;
    g_steam_level_state = true;
    
    // Initialize over-temp state
    g_brew_over_temp = false;
    g_steam_over_temp = false;
    
    // Initialize SSR monitoring
    g_brew_ssr_on_time = 0;
    g_steam_ssr_on_time = 0;
    g_brew_temp_when_on = 0.0f;
    g_steam_temp_when_on = 0.0f;
    
    // Initialize UI state
    g_led_last_toggle = 0;
    g_led_state = false;
    g_buzzer_beep_count = 0;
    g_buzzer_last_beep = 0;
    
    LOG_PRINT("Safety system initialized\n");
}

// =============================================================================
// Main Safety Check
// =============================================================================

// CRITICAL: Execute from SRAM to prevent cache eviction stalls during flash CRC checks
// This ensures deterministic timing for safety checks regardless of flash access
safety_state_t __not_in_flash_func(safety_check)(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    safety_state_t result = SAFETY_OK;
    
    // Skip safety checks during bootloader mode
    // Machine is already in safe state (heaters off) during OTA
    if (bootloader_is_active()) {
        return SAFETY_OK;  // Already safe, don't spam logs
    }
    
    // Clear previous flags
    g_safety_flags = 0;
    
    // CRITICAL: Check if environmental config is valid
    // Machine must be disabled if environmental config is not set
    if (!config_persistence_is_env_valid()) {
        g_safety_flags |= SAFETY_FLAG_ENV_CONFIG_INVALID;
        return SAFETY_CRITICAL;  // Machine disabled - environmental config required
    }
    
    // Get current sensor readings
    sensor_data_t data;
    sensors_get_data(&data);
    
    float brew_temp_c = data.brew_temp / 10.0f;
    float steam_temp_c = data.steam_temp / 10.0f;
    
    // =========================================================================
    // SAF-010, SAF-011, SAF-012: Water Level Interlocks
    // =========================================================================
    const pcb_config_t* pcb = pcb_config_get();
    
    // Check reservoir (SAF-010)
    // Only check if machine is in water tank mode (set by physical switch)
    // Plumbed mode: water line always available, no reservoir check needed
    bool is_plumbed_mode = false;
    bool is_water_tank_mode = false;
    
    // Read water mode switch (physical switch)
    if (pcb && PIN_VALID(pcb->pins.input_water_mode)) {
        // Switch configured: HIGH = plumbed, LOW = water tank
        is_plumbed_mode = hw_read_gpio(pcb->pins.input_water_mode);
        is_water_tank_mode = !is_plumbed_mode;
    } else {
        // Switch not configured - default to water tank mode (safe default)
        is_water_tank_mode = true;
        is_plumbed_mode = false;
    }
    
    if (is_water_tank_mode) {
        // Water tank mode: reservoir empty = critical (disable heaters and pump)
        // Only check if reservoir sensor is configured
        if (pcb && PIN_VALID(pcb->pins.input_reservoir)) {
            bool reservoir_low = check_water_sensor_debounced(
                pcb->pins.input_reservoir,
                &g_reservoir_debounce_count,
                &g_reservoir_state
            );
            
            if (reservoir_low) {
                g_safety_flags |= SAFETY_FLAG_WATER_LOW;
                if (g_last_alarm == ALARM_NONE) {
                    g_last_alarm = ALARM_WATER_LOW;
                }
                result = SAFETY_CRITICAL;  // Critical: disable pump and heaters
                // Rate-limit message to avoid log spam
                if ((now - g_last_reservoir_msg) >= SAFETY_MSG_RATE_LIMIT_MS) {
                    g_last_reservoir_msg = now;
                    LOG_PRINT("SAFETY: Water reservoir empty! (Water tank mode - disabling heaters and pump)\n");
                }
            }
        }
    } else {
        // Plumbed mode: water line always available, no reservoir check
        // Reset reservoir debounce state
        g_reservoir_debounce_count = 0;
        g_reservoir_state = true;
    }
    
    // Check tank level (SAF-011)
    if (pcb && pcb->pins.input_tank_level >= 0) {
        bool tank_low = check_water_sensor_debounced(
            pcb->pins.input_tank_level,
            &g_tank_level_debounce_count,
            &g_tank_level_state
        );
        
        if (tank_low) {
            g_safety_flags |= SAFETY_FLAG_WATER_LOW;
            if (g_last_alarm == ALARM_NONE) {
                g_last_alarm = ALARM_WATER_LOW;
            }
            result = SAFETY_CRITICAL;  // Critical: disable pump and heaters
            // Rate-limit message to avoid log spam
            if ((now - g_last_tank_msg) >= SAFETY_MSG_RATE_LIMIT_MS) {
                g_last_tank_msg = now;
                DEBUG_PRINT("SAFETY: Tank level low!\n");
            }
        }
    }
    
    // Check steam level (SAF-012)
    if (pcb && pcb->pins.input_steam_level >= 0) {
        bool steam_low = check_water_sensor_debounced(
            pcb->pins.input_steam_level,
            &g_steam_level_debounce_count,
            &g_steam_level_state
        );
        
        if (steam_low) {
            g_safety_flags |= SAFETY_FLAG_WATER_LOW;
            if (g_last_alarm == ALARM_NONE) {
                g_last_alarm = ALARM_WATER_LOW;
            }
            // Critical for steam heater only
            if (result < SAFETY_CRITICAL) {
                result = SAFETY_CRITICAL;
            }
            // Rate-limit message to avoid log spam
            if ((now - g_last_steam_level_msg) >= SAFETY_MSG_RATE_LIMIT_MS) {
                g_last_steam_level_msg = now;
                DEBUG_PRINT("SAFETY: Steam boiler level low!\n");
            }
        }
    }
    
    // =========================================================================
    // SAF-020, SAF-021, SAF-022: Over-Temperature Protection
    // Machine-type aware - only check sensors that exist
    // =========================================================================
    
    const machine_features_t* features = machine_get_features();
    
    // Check brew boiler temperature (SAF-020) - only if machine has brew NTC
    if (features && machine_has_brew_ntc()) {
        if (!isnan(brew_temp_c) && !isinf(brew_temp_c)) {
            if (brew_temp_c >= SAFETY_BREW_MAX_TEMP_C) {
                g_brew_over_temp = true;
                g_safety_flags |= SAFETY_FLAG_OVER_TEMP;
                g_last_alarm = ALARM_OVER_TEMP;
                result = SAFETY_CRITICAL;
                LOG_PRINT("SAFETY: Brew boiler over temperature: %.1fC (max: %.1fC)\n", brew_temp_c, SAFETY_BREW_MAX_TEMP_C);
            } else if (brew_temp_c <= (SAFETY_BREW_MAX_TEMP_C - SAFETY_TEMP_HYSTERESIS_C)) {
                // Hysteresis: re-enable 10°C below max (SAF-025)
                g_brew_over_temp = false;
            }
        }
    }
    
    // Check steam boiler temperature (SAF-021) - only if machine has steam NTC
    if (features && machine_has_steam_ntc()) {
        if (!isnan(steam_temp_c) && !isinf(steam_temp_c)) {
            if (steam_temp_c >= SAFETY_STEAM_MAX_TEMP_C) {
                g_steam_over_temp = true;
                g_safety_flags |= SAFETY_FLAG_OVER_TEMP;
                g_last_alarm = ALARM_OVER_TEMP;
                result = SAFETY_CRITICAL;
                LOG_PRINT("SAFETY: Steam boiler over temperature: %.1fC (max: %.1fC)\n", steam_temp_c, SAFETY_STEAM_MAX_TEMP_C);
            } else if (steam_temp_c <= (SAFETY_STEAM_MAX_TEMP_C - SAFETY_TEMP_HYSTERESIS_C)) {
                g_steam_over_temp = false;
            }
        }
    }
    
    // Note: Group head thermocouple (SAF-022) removed in v2.24.3
    // Boiler NTC sensors provide sufficient over-temperature protection
    
    // =========================================================================
    // SAF-023, SAF-024: NTC Sensor Fault Detection
    // Machine-type aware - only check sensors that exist
    // =========================================================================
    
    // Only check brew NTC if machine should have one
    if (features && machine_has_brew_ntc()) {
        if (check_ntc_fault(brew_temp_c)) {
            g_safety_flags |= SAFETY_FLAG_SENSOR_FAIL;
            if (g_last_alarm == ALARM_NONE) {
                g_last_alarm = ALARM_SENSOR_FAIL;
            }
            result = SAFETY_CRITICAL;  // Critical: disable brew heater
            LOG_PRINT("SAFETY: Brew NTC sensor fault! (temp=%.1fC)\n", brew_temp_c);
        }
    }
    
    // Only check steam NTC if machine should have one
    if (features && machine_has_steam_ntc()) {
        if (check_ntc_fault(steam_temp_c)) {
            g_safety_flags |= SAFETY_FLAG_SENSOR_FAIL;
            if (g_last_alarm == ALARM_NONE) {
                g_last_alarm = ALARM_SENSOR_FAIL;
            }
            result = SAFETY_CRITICAL;  // Critical: disable steam heater
            LOG_PRINT("SAFETY: Steam NTC sensor fault! (temp=%.1fC)\n", steam_temp_c);
        }
    }
    
    // =========================================================================
    // SAF-031: SSR Maximum On Time Check
    // Machine-type aware - only check SSRs that exist
    // =========================================================================
    
    control_outputs_t outputs;
    control_get_outputs(&outputs);
    
    // Monitor brew SSR - only if machine has brew boiler (or single boiler)
    // For single boiler, this monitors the single SSR using brew_temp
    // For HX, brew_heater should always be 0 (control doesn't set it)
    if (features && (machine_has_brew_boiler() || features->type == MACHINE_TYPE_SINGLE_BOILER)) {
        if (outputs.brew_heater > 0) {
            if (g_brew_ssr_on_time == 0) {
                // SSR just turned on
                g_brew_ssr_on_time = now;
                g_brew_temp_when_on = brew_temp_c;
            } else {
                // Check if temperature is changing
                float temp_change = fabsf(brew_temp_c - g_brew_temp_when_on);
                if (temp_change < 1.0f) {  // Less than 1°C change
                    // Temperature not changing - check time
                    if (now - g_brew_ssr_on_time > SAFETY_SSR_MAX_ON_TIME_MS) {
                        g_safety_flags |= SAFETY_FLAG_OVER_TEMP;
                        if (g_last_alarm == ALARM_NONE) {
                            g_last_alarm = ALARM_OVER_TEMP;
                        }
                        result = SAFETY_FAULT;
                        LOG_PRINT("SAFETY: Brew SSR on too long without temp change! (on_time=%lu ms, temp=%.1fC)\n",
                                 now - g_brew_ssr_on_time, brew_temp_c);
                    }
                } else {
                    // Temperature changing - reset timer
                    g_brew_ssr_on_time = now;
                    g_brew_temp_when_on = brew_temp_c;
                }
            }
        } else {
            g_brew_ssr_on_time = 0;  // SSR off
        }
    }
    
    // Monitor steam SSR - only if machine has separate steam boiler
    // For HX, this is the only active heater
    if (features && (machine_has_steam_boiler() || machine_is_heat_exchanger())) {
        if (outputs.steam_heater > 0) {
            if (g_steam_ssr_on_time == 0) {
                g_steam_ssr_on_time = now;
                g_steam_temp_when_on = steam_temp_c;
            } else {
                float temp_change = fabsf(steam_temp_c - g_steam_temp_when_on);
                if (temp_change < 1.0f) {
                    if (now - g_steam_ssr_on_time > SAFETY_SSR_MAX_ON_TIME_MS) {
                        g_safety_flags |= SAFETY_FLAG_OVER_TEMP;
                        if (g_last_alarm == ALARM_NONE) {
                            g_last_alarm = ALARM_OVER_TEMP;
                        }
                        result = SAFETY_FAULT;
                        LOG_PRINT("SAFETY: Steam SSR on too long without temp change! (on_time=%lu ms, temp=%.1fC)\n",
                                 now - g_steam_ssr_on_time, steam_temp_c);
                    }
                } else {
                    g_steam_ssr_on_time = now;
                    g_steam_temp_when_on = steam_temp_c;
                }
            }
        } else {
            g_steam_ssr_on_time = 0;
        }
    }
    
    // =========================================================================
    // SAF-032: SSR Duty Cycle Limit
    // =========================================================================
    
    if (outputs.brew_heater > SAFETY_SSR_MAX_DUTY) {
        outputs.brew_heater = SAFETY_SSR_MAX_DUTY;
        control_set_output(0, SAFETY_SSR_MAX_DUTY, 1);  // Clamp to max
    }
    
    if (outputs.steam_heater > SAFETY_SSR_MAX_DUTY) {
        outputs.steam_heater = SAFETY_SSR_MAX_DUTY;
        control_set_output(1, SAFETY_SSR_MAX_DUTY, 1);  // Clamp to max
    }
    
    // =========================================================================
    // ESP32 Communication Timeout - Defensive Mode
    // =========================================================================
    
    bool esp32_connected = (now - g_last_esp32_heartbeat) < SAFETY_HEARTBEAT_TIMEOUT_MS;
    
        if (!esp32_connected) {
        g_safety_flags |= SAFETY_FLAG_COMM_TIMEOUT;
        
        // Enter defensive mode: force machine to IDLE state for safety
        // Machine should not operate without ESP32 control/monitoring
        if (!g_defensive_mode) {
            LOG_PRINT("ESP32 timeout - entering defensive mode (forcing IDLE)\n");
            g_defensive_mode = true;
            // Force machine to IDLE mode immediately
            state_set_mode(MODE_IDLE);
        }
        
        // Keep machine in IDLE if it tries to change mode
        machine_mode_t current_mode = state_get_mode();
        if (current_mode != MODE_IDLE) {
            LOG_PRINT("Defensive mode: forcing IDLE (current mode: %d)\n", current_mode);
            state_set_mode(MODE_IDLE);
        }
        
        if (result < SAFETY_WARNING) {
            result = SAFETY_WARNING;
        }
    } else {
        // ESP32 reconnected - exit defensive mode
        if (g_defensive_mode) {
            LOG_PRINT("ESP32 reconnected - exiting defensive mode\n");
            g_defensive_mode = false;
            g_safety_flags &= ~SAFETY_FLAG_COMM_TIMEOUT;
        }
    }
    
    // =========================================================================
    // Enter Safe State if Critical
    // =========================================================================
    
    if (result == SAFETY_CRITICAL) {
        safety_enter_safe_state();
    }
    
    // Update safe state UI if in safe state
    if (g_safe_state) {
        update_safe_state_ui();
    }
    
    return result;
}

// =============================================================================
// Safe State Control
// =============================================================================

void safety_enter_safe_state(void) {
    if (!g_safe_state) {
        LOG_PRINT("SAFETY: Entering SAFE STATE!\n");
        g_safe_state_entered_time = to_ms_since_boot(get_absolute_time());
        
        // Disable all outputs (SAF-004, SAF-030)
        disable_all_outputs();
        
        // Reset UI state
        g_led_last_toggle = 0;
        g_led_state = false;
        g_buzzer_beep_count = 0;
        g_buzzer_last_beep = 0;
    }
    g_safe_state = true;
}

bool safety_is_safe_state(void) {
    return g_safe_state;
}

bool safety_reset(void) {
    // Only allow reset if all safety conditions are now OK
    if (g_safety_flags == 0) {
        LOG_PRINT("SAFETY: Resetting from safe state\n");
        g_safe_state = false;
        g_last_alarm = ALARM_NONE;
        g_safe_state_entered_time = 0;
        
        // Reset UI
        const pcb_config_t* pcb = pcb_config_get();
        if (pcb && pcb->pins.led_status >= 0) {
            hw_set_gpio(pcb->pins.led_status, true);  // LED on (normal)
        }
        
        return true;
    }
    
    LOG_PRINT("SAFETY: Cannot reset, conditions not cleared (flags=0x%02X)\n", g_safety_flags);
    return false;
}

// =============================================================================
// Flags and Status
// =============================================================================

uint8_t safety_get_flags(void) {
    return g_safety_flags;
}

uint8_t safety_get_last_alarm(void) {
    return g_last_alarm;
}

// =============================================================================
// Watchdog
// =============================================================================

void safety_kick_watchdog(void) {
    watchdog_update();
}

// =============================================================================
// ESP32 Heartbeat
// =============================================================================

void safety_esp32_heartbeat(void) {
    g_last_esp32_heartbeat = to_ms_since_boot(get_absolute_time());
}

bool safety_esp32_connected(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    return (now - g_last_esp32_heartbeat) < SAFETY_HEARTBEAT_TIMEOUT_MS;
}

bool safety_is_defensive_mode(void) {
    return g_defensive_mode;
}

