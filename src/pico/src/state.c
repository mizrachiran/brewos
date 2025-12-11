/**
 * ECM Pico Firmware - State Machine
 * 
 * Manages the overall machine state with complete state transitions,
 * brew cycle control, and entry/exit actions.
 */

#include "pico/stdlib.h"
#include "state.h"
#include "config.h"
#include "sensors.h"
#include "control.h"
#include "safety.h"
#include "pcb_config.h"
#include "machine_config.h"
#include "hardware.h"
#include "hardware/gpio.h"
#include "protocol.h"
#include "cleaning.h"
#include "config_persistence.h"

// =============================================================================
// Brew Cycle Configuration
// =============================================================================

#define BREW_SWITCH_DEBOUNCE_MS       50      // Debounce time for brew switch
#define POST_BREW_SOLENOID_DELAY_MS  2000    // Solenoid delay after brew (FUNC-022)
#define PREINFUSION_DEFAULT_ON_MS    3000    // Default pre-infusion pump on time
#define PREINFUSION_DEFAULT_PAUSE_MS 5000    // Default pre-infusion pause time

// Temperature tolerance for "ready" state
#define TEMP_READY_TOLERANCE_C       1.0f    // 1°C tolerance
#define TEMP_HEATING_THRESHOLD_C     2.0f    // 2°C below setpoint = initial heating phase
#define TEMP_COLD_THRESHOLD_C        5.0f    // 5°C below setpoint = cold, need heating

// =============================================================================
// Machine-Type Aware Temperature Reading
// =============================================================================

/**
 * Get the brew-relevant temperature for the current machine type.
 * - Dual Boiler: brew_temp (brew boiler NTC)
 * - Single Boiler: brew_temp (shared NTC)
 * - Heat Exchanger: group_temp (passive HX, no brew NTC)
 */
static float get_brew_temp_for_machine(float brew_temp, float group_temp) {
    const machine_features_t* features = machine_get_features();
    if (features && features->type == MACHINE_TYPE_HEAT_EXCHANGER) {
        // HX: Use group temp as brew water indicator (no brew NTC)
        return group_temp;
    }
    // Dual boiler and single boiler: use brew NTC
    return brew_temp;
}

// =============================================================================
// Private State
// =============================================================================

static machine_state_t g_state = STATE_INIT;
static machine_state_t g_previous_state = STATE_INIT;
static machine_mode_t g_mode = MODE_BREW;
static bool g_brewing = false;

// Brew cycle state
typedef enum {
    BREW_PHASE_NONE = 0,
    BREW_PHASE_PREINFUSION,
    BREW_PHASE_BREWING,
    BREW_PHASE_POST_BREW
} brew_phase_t;

static brew_phase_t g_brew_phase = BREW_PHASE_NONE;
static uint32_t g_brew_start_time = 0;
static uint32_t g_brew_stop_time = 0;
static uint32_t g_post_brew_start_time = 0;

// Pre-infusion configuration
static bool g_preinfusion_enabled = false;
static uint16_t g_preinfusion_on_ms = PREINFUSION_DEFAULT_ON_MS;
static uint16_t g_preinfusion_pause_ms = PREINFUSION_DEFAULT_PAUSE_MS;

// Brew switch debounce
static uint32_t g_brew_switch_last_change = 0;
static bool g_brew_switch_state = false;
static bool g_brew_switch_debounced = false;

// State entry/exit tracking
static uint32_t g_state_entry_time = 0;

// Eco mode state
static eco_config_t g_eco_config = {
    .enabled = true,
    .eco_brew_temp = 800,       // 80.0°C
    .timeout_minutes = 30       // 30 minutes
};
static uint32_t g_last_activity_time = 0;    // Time of last user activity
static int16_t g_saved_brew_setpoint = 0;    // Saved setpoint before entering eco
static machine_mode_t g_saved_mode = MODE_IDLE;  // Saved mode before entering eco

// =============================================================================
// State Names (for debugging)
// =============================================================================

static const char* state_names[] = {
    "INIT",
    "IDLE",
    "HEATING",
    "READY",
    "BREWING",
    "FAULT",
    "SAFE",
    "ECO"
};

// =============================================================================
// Helper Functions
// =============================================================================

/**
 * Read and debounce brew switch
 */
static bool read_brew_switch_debounced(void) {
    const pcb_config_t* pcb = pcb_config_get();
    if (!pcb || pcb->pins.input_brew_switch < 0) {
        return false;  // Not configured
    }
    
    uint32_t now = to_ms_since_boot(get_absolute_time());
    bool current_state = !hw_read_gpio(pcb->pins.input_brew_switch);  // Active LOW
    
    if (current_state != g_brew_switch_state) {
        // State changed, reset debounce timer
        g_brew_switch_last_change = now;
        g_brew_switch_state = current_state;
    }
    
    // Check if debounced
    if (now - g_brew_switch_last_change >= BREW_SWITCH_DEBOUNCE_MS) {
        g_brew_switch_debounced = g_brew_switch_state;
    }
    
    return g_brew_switch_debounced;
}

/**
 * State entry actions
 */
static void state_entry_action(machine_state_t state) {
    const pcb_config_t* pcb = pcb_config_get();
    uint32_t now = to_ms_since_boot(get_absolute_time());
    g_state_entry_time = now;
    
    switch (state) {
        case STATE_INIT:
            // Initialize outputs
            if (pcb) {
                if (pcb->pins.relay_pump >= 0) {
                    hw_gpio_init_output(pcb->pins.relay_pump, false);
                }
                if (pcb->pins.relay_brew_solenoid >= 0) {
                    hw_gpio_init_output(pcb->pins.relay_brew_solenoid, false);
                }
            }
            break;
            
        case STATE_IDLE:
            // Ensure outputs are off
            control_set_pump(0);
            if (pcb && pcb->pins.relay_brew_solenoid >= 0) {
                hw_set_gpio(pcb->pins.relay_brew_solenoid, false);
            }
            break;
            
        case STATE_HEATING:
            // Start heating (PID will handle this)
            break;
            
        case STATE_READY:
            // At temperature, ready to brew
            break;
            
        case STATE_BREWING:
            // Start brew cycle and shot timer
            g_brew_start_time = now;
            g_brew_stop_time = 0;  // Reset stop time (timer running)
            g_brew_phase = g_preinfusion_enabled ? BREW_PHASE_PREINFUSION : BREW_PHASE_BREWING;
            
            if (g_preinfusion_enabled) {
                // Pre-infusion: pump on, solenoid on
                control_set_pump(100);
                if (pcb && pcb->pins.relay_brew_solenoid >= 0) {
                    hw_set_gpio(pcb->pins.relay_brew_solenoid, true);
                }
                DEBUG_PRINT("Brew: Pre-infusion started\n");
            } else {
                // Direct brew: pump on, solenoid on
                control_set_pump(100);
                if (pcb && pcb->pins.relay_brew_solenoid >= 0) {
                    hw_set_gpio(pcb->pins.relay_brew_solenoid, true);
                }
                DEBUG_PRINT("Brew: Started\n");
            }
            break;
            
        case STATE_FAULT:
            // Fault state - outputs already off from safety system
            break;
            
        case STATE_SAFE:
            // Safe state - outputs already off from safety system
            break;
            
        case STATE_ECO:
            // Eco mode - save current setpoint and reduce temperature
            g_saved_brew_setpoint = control_get_setpoint(0);
            g_saved_mode = g_mode;
            control_set_setpoint(0, g_eco_config.eco_brew_temp);
            DEBUG_PRINT("Eco: Entered eco mode (saved setpoint=%d, eco temp=%d)\n",
                       g_saved_brew_setpoint, g_eco_config.eco_brew_temp);
            break;
    }
}

/**
 * State exit actions
 */
static void state_exit_action(machine_state_t state) {
    switch (state) {
        case STATE_ECO:
            // Exiting eco mode - restore original setpoint and mode
            control_set_setpoint(0, g_saved_brew_setpoint);
            g_mode = g_saved_mode;
            g_last_activity_time = to_ms_since_boot(get_absolute_time());  // Reset idle timer
            DEBUG_PRINT("Eco: Exited eco mode (restored setpoint=%d, mode=%d)\n",
                       g_saved_brew_setpoint, g_saved_mode);
            break;
            
        case STATE_BREWING:
            // Stop brew cycle (shot timer already stopped in state_stop_brew)
            // Only set stop time if not already set (may have been set by WEIGHT_STOP or lever)
            if (g_brew_stop_time == 0) {
                g_brew_stop_time = to_ms_since_boot(get_absolute_time());
                
                uint32_t brew_duration = g_brew_stop_time - g_brew_start_time;
                
                // Record brew cycle for cleaning counter (if >= 15 seconds)
                // Note: Cleaning counter is separate and resets after cleaning
                // This is safety-critical and stays on Pico to work even without ESP32
                cleaning_record_brew_cycle(brew_duration);
                
                // Note: Statistics are tracked by ESP32 (has NTP for accurate timestamps)
                // ESP32 monitors brew alarms (ALARM_BREW_COMPLETED) and records statistics
            }
            control_set_pump(0);
            // Solenoid will be turned off after post-brew delay
            g_post_brew_start_time = to_ms_since_boot(get_absolute_time());
            g_brew_phase = BREW_PHASE_POST_BREW;
            DEBUG_PRINT("Brew: Stopped (shot time: %lu ms)\n", 
                       g_brew_stop_time - g_brew_start_time);
            break;
            
        default:
            break;
    }
}

// =============================================================================
// Initialization
// =============================================================================

void state_init(void) {
    g_state = STATE_INIT;
    g_previous_state = STATE_INIT;
    g_mode = MODE_IDLE;  // Start in IDLE mode - machine idle until heating strategy selected
    g_brewing = false;
    g_brew_phase = BREW_PHASE_NONE;
    g_brew_start_time = 0;
    g_brew_stop_time = 0;
    g_post_brew_start_time = 0;
    g_state_entry_time = 0;
    
    // Initialize brew switch debounce
    g_brew_switch_last_change = 0;
    g_brew_switch_state = false;
    g_brew_switch_debounced = false;
    
    // Initialize pre-infusion
    g_preinfusion_enabled = false;
    g_preinfusion_on_ms = PREINFUSION_DEFAULT_ON_MS;
    g_preinfusion_pause_ms = PREINFUSION_DEFAULT_PAUSE_MS;
    
    // Initialize eco mode from persisted config
    bool eco_enabled;
    int16_t eco_temp;
    uint16_t eco_timeout;
    config_persistence_get_eco(&eco_enabled, &eco_temp, &eco_timeout);
    g_eco_config.enabled = eco_enabled;
    g_eco_config.eco_brew_temp = eco_temp;
    g_eco_config.timeout_minutes = eco_timeout;
    g_last_activity_time = to_ms_since_boot(get_absolute_time());
    g_saved_brew_setpoint = 0;
    g_saved_mode = MODE_IDLE;
    
    // Initialize brew switch GPIO
    const pcb_config_t* pcb = pcb_config_get();
    if (pcb && pcb->pins.input_brew_switch >= 0) {
        hw_gpio_init_input(pcb->pins.input_brew_switch, true, false);  // Pull-up
    }
    
    // Perform entry action for INIT state
    state_entry_action(STATE_INIT);
    
    LOG_PRINT("State machine initialized: %s (eco: %s, timeout=%d min)\n", 
               state_names[g_state],
               g_eco_config.enabled ? "enabled" : "disabled",
               g_eco_config.timeout_minutes);
}

// =============================================================================
// State Update
// =============================================================================

void state_update(void) {
    // Check for safe state (overrides everything)
    if (safety_is_safe_state()) {
        if (g_state != STATE_SAFE) {
            state_exit_action(g_state);
            g_previous_state = g_state;
            g_state = STATE_SAFE;
            state_entry_action(STATE_SAFE);
            LOG_PRINT("State: %s -> SAFE\n", state_names[g_previous_state]);
        }
        return;
    }
    
    // Check for fault state
    if (safety_get_flags() != 0 && g_state != STATE_FAULT && g_state != STATE_SAFE) {
        state_exit_action(g_state);
        g_previous_state = g_state;
        g_state = STATE_FAULT;
        state_entry_action(STATE_FAULT);
        LOG_PRINT("State: %s -> FAULT\n", state_names[g_previous_state]);
        return;
    }
    
    // Get current temperatures
    sensor_data_t sensors;
    sensors_get_data(&sensors);
    
    float brew_temp_raw = sensors.brew_temp / 10.0f;
    float steam_temp = sensors.steam_temp / 10.0f;
    float group_temp = sensors.group_temp / 10.0f;
    
    // Get machine-appropriate brew temperature
    // HX: use group_temp (no brew NTC), others: use brew_temp
    float brew_temp = get_brew_temp_for_machine(brew_temp_raw, group_temp);
    
    // Get setpoints
    float brew_sp = control_get_setpoint(0) / 10.0f;
    float steam_sp = control_get_setpoint(1) / 10.0f;
    
    machine_state_t new_state = g_state;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    switch (g_state) {
        case STATE_INIT:
            // Transition to IDLE after initialization
            new_state = STATE_IDLE;
            break;
            
        case STATE_IDLE:
            // STATE_IDLE = machine is "on" but not heating (MODE_IDLE)
            // When mode is selected (MODE_BREW or MODE_STEAM), immediately transition to heating/ready
            if (g_mode == MODE_BREW) {
                // Brew mode: only check brew boiler temperature
                if (brew_temp >= brew_sp - TEMP_READY_TOLERANCE_C) {
                    // Already at temperature (e.g., machine was hot)
                    new_state = STATE_READY;
                } else {
                    // Not at temperature - enter initial heating phase
                    new_state = STATE_HEATING;
                }
            } else if (g_mode == MODE_STEAM) {
                // Steam mode: heats both boilers (steam for milk, brew for espresso)
                // Ready when steam boiler is at temperature (brew can still be heating)
                if (steam_temp >= steam_sp - TEMP_READY_TOLERANCE_C) {
                    // Already at temperature
                    new_state = STATE_READY;
                } else {
                    // Not at temperature - enter initial heating phase
                    new_state = STATE_HEATING;
                }
            } else {
                // MODE_IDLE - stay idle, no heating (PID not running)
                new_state = STATE_IDLE;
            }
            break;
            
        case STATE_HEATING:
            // Initial heating phase - transition to READY when at temperature
            // PID maintains temperature in READY state, not HEATING
            if (g_mode == MODE_BREW) {
                if (brew_temp >= brew_sp - TEMP_READY_TOLERANCE_C) {
                    new_state = STATE_READY;
                }
                // Stay in HEATING until at temperature
            } else if (g_mode == MODE_STEAM) {
                if (steam_temp >= steam_sp - TEMP_READY_TOLERANCE_C) {
                    new_state = STATE_READY;
                }
                // Stay in HEATING until at temperature
            } else {
                // Mode changed to IDLE - go back to IDLE
                new_state = STATE_IDLE;
            }
            break;
            
        case STATE_READY:
            // At temperature - PID maintains temperature here
            // Only go back to HEATING if temperature drops significantly (cold)
            if (g_mode == MODE_BREW) {
                if (brew_temp < brew_sp - TEMP_COLD_THRESHOLD_C) {
                    // Temperature dropped significantly - need heating phase again
                    new_state = STATE_HEATING;
                }
                // Otherwise stay in READY - PID handles small temperature variations
            } else if (g_mode == MODE_STEAM) {
                if (steam_temp < steam_sp - TEMP_COLD_THRESHOLD_C) {
                    // Temperature dropped significantly - need heating phase again
                    new_state = STATE_HEATING;
                }
                // Otherwise stay in READY - PID handles small temperature variations
            } else {
                // Mode changed to IDLE - go back to IDLE
                new_state = STATE_IDLE;
            }
            // Check if brewing started (via command or switch)
            if (g_brewing) {
                new_state = STATE_BREWING;
            }
            break;
            
        case STATE_BREWING:
            // Handle brew cycle phases
            if (g_brew_phase == BREW_PHASE_PREINFUSION) {
                // Check pre-infusion timing
                uint32_t preinfusion_elapsed = now - g_brew_start_time;
                
                if (preinfusion_elapsed >= g_preinfusion_on_ms) {
                    // Pre-infusion pause: turn off pump, keep solenoid on
                    control_set_pump(0);
                    DEBUG_PRINT("Brew: Pre-infusion pause\n");
                    
                    if (preinfusion_elapsed >= (g_preinfusion_on_ms + g_preinfusion_pause_ms)) {
                        // Pre-infusion complete, start full pressure brew
                        g_brew_phase = BREW_PHASE_BREWING;
                        control_set_pump(100);
                        DEBUG_PRINT("Brew: Full pressure started\n");
                    }
                }
            }
            
            // Check for brew stop (manual or automatic)
            if (!g_brewing) {
                new_state = STATE_READY;
            }
            break;
            
        case STATE_FAULT:
            // Stays in fault until explicitly reset
            if (safety_get_flags() == 0) {
                new_state = STATE_IDLE;
            }
            break;
            
        case STATE_SAFE:
            // Only exit safe state via explicit reset
            if (!safety_is_safe_state()) {
                new_state = STATE_IDLE;
            }
            break;
            
        case STATE_ECO:
            // In eco mode - maintain reduced temperature
            // Exit eco mode on user activity (mode change, brew switch, etc.)
            // This is handled by state_exit_eco() called from command handlers
            // Also check if eco mode was disabled while in eco
            if (!g_eco_config.enabled) {
                new_state = STATE_IDLE;
            }
            break;
    }
    
    // Handle post-brew phase (solenoid delay)
    if (g_brew_phase == BREW_PHASE_POST_BREW) {
        if (now - g_post_brew_start_time >= POST_BREW_SOLENOID_DELAY_MS) {
            // Post-brew delay complete, turn off solenoid
            const pcb_config_t* pcb = pcb_config_get();
            if (pcb && pcb->pins.relay_brew_solenoid >= 0) {
                hw_set_gpio(pcb->pins.relay_brew_solenoid, false);
            }
            g_brew_phase = BREW_PHASE_NONE;
            DEBUG_PRINT("Brew: Post-brew complete, solenoid off\n");
        }
    }
    
    // Check for cleaning mode (if active, handle cleaning cycle separately)
    if (cleaning_is_active()) {
        bool brew_switch = read_brew_switch_debounced();
        
        if (!g_brewing && brew_switch) {
            // Start cleaning cycle when brew switch pressed
            if (cleaning_start_cycle()) {
                g_brewing = true;  // Mark as brewing to prevent normal brew
                new_state = STATE_BREWING;
            }
        } else if (g_brewing && !brew_switch) {
            // Stop cleaning cycle when brew switch released
            cleaning_stop_cycle();
            g_brewing = false;
            new_state = STATE_READY;  // Return to ready after cleaning
        }
        
        // Cleaning cycle auto-stops after 10s (handled in cleaning_update())
        if (g_brewing && !cleaning_is_active()) {
            // Cleaning cycle auto-stopped
            g_brewing = false;
            new_state = STATE_READY;
        }
        
        // Skip normal brew switch handling when in cleaning mode
        // (cleaning mode has its own handling above)
    } else {
        // Normal brew switch handling (not in cleaning mode)
        // Check for WEIGHT_STOP signal (brew-by-weight) - can stop brewing even if switch is still pressed
        // This check happens BEFORE brew switch check so automatic stop takes priority
        const pcb_config_t* pcb = pcb_config_get();
        bool weight_stop_active = false;
        if (pcb && PIN_VALID(pcb->pins.input_weight_stop)) {
            weight_stop_active = hw_read_gpio(pcb->pins.input_weight_stop);
            if (weight_stop_active && g_brewing) {
                // Target weight reached - stop brew immediately (automatic stop)
                DEBUG_PRINT("Brew: WEIGHT_STOP signal received - stopping brew automatically\n");
                state_stop_brew();
                // Return to appropriate state based on temperature
                if (g_mode == MODE_BREW) {
                    if (brew_temp >= brew_sp - TEMP_READY_TOLERANCE_C) {
                        new_state = STATE_READY;
                    } else if (brew_temp < brew_sp - TEMP_COLD_THRESHOLD_C) {
                        new_state = STATE_HEATING;
                    } else {
                        new_state = STATE_READY;
                    }
                } else if (g_mode == MODE_STEAM) {
                    if (steam_temp >= steam_sp - TEMP_READY_TOLERANCE_C) {
                        new_state = STATE_READY;
                    } else if (steam_temp < steam_sp - TEMP_COLD_THRESHOLD_C) {
                        new_state = STATE_HEATING;
                    } else {
                        new_state = STATE_READY;
                    }
                } else {
                    new_state = STATE_IDLE;
                }
            }
        }
        
        // Check brew switch for manual brew control
        // IMPORTANT: Pump must work regardless of temperature (for first setup and brew boiler purge)
        // Allow brew switch to work in IDLE, HEATING, READY states (not just READY)
        bool brew_switch = read_brew_switch_debounced();
        
        if (!g_brewing) {
            // Brew switch pressed - start brew/pump
            // Allow this in IDLE, HEATING, READY states (for purging when cold)
            // BUT: Block if in safe state (e.g., reservoir empty in water tank mode)
            if (brew_switch && (g_state == STATE_IDLE || g_state == STATE_HEATING || g_state == STATE_READY)) {
                if (!safety_is_safe_state()) {
                    state_start_brew();
                    new_state = STATE_BREWING;
                } else {
                    DEBUG_PRINT("Brew: Switch pressed but blocked - machine in safe state\n");
                }
            }
        } else {
            // Brew switch released - stop brew/pump (manual stop)
            // Only stop if WEIGHT_STOP hasn't already stopped it
            if (!brew_switch && !weight_stop_active) {
                DEBUG_PRINT("Brew: Switch released - stopping brew manually\n");
                state_stop_brew();
                // Return to appropriate state based on temperature
                if (g_mode == MODE_BREW) {
                    if (brew_temp >= brew_sp - TEMP_READY_TOLERANCE_C) {
                        new_state = STATE_READY;
                    } else if (brew_temp < brew_sp - TEMP_COLD_THRESHOLD_C) {
                        // Temperature dropped significantly - need heating phase
                        new_state = STATE_HEATING;
                    } else {
                        // Close to setpoint - PID will maintain, go to READY
                        new_state = STATE_READY;
                    }
                } else if (g_mode == MODE_STEAM) {
                    if (steam_temp >= steam_sp - TEMP_READY_TOLERANCE_C) {
                        new_state = STATE_READY;
                    } else if (steam_temp < steam_sp - TEMP_COLD_THRESHOLD_C) {
                        // Temperature dropped significantly - need heating phase
                        new_state = STATE_HEATING;
                    } else {
                        // Close to setpoint - PID will maintain, go to READY
                        new_state = STATE_READY;
                    }
                } else {
                    new_state = STATE_IDLE;
                }
            }
        }
    }
    
    // Check eco mode auto-timeout (only in READY state when not brewing)
    if (g_eco_config.enabled && g_eco_config.timeout_minutes > 0 && 
        g_state == STATE_READY && !g_brewing && new_state == STATE_READY) {
        uint32_t idle_ms = now - g_last_activity_time;
        uint32_t timeout_ms = (uint32_t)g_eco_config.timeout_minutes * 60 * 1000;
        
        if (idle_ms >= timeout_ms) {
            // Idle timeout reached - enter eco mode
            DEBUG_PRINT("Eco: Idle timeout reached (%lu ms >= %lu ms), entering eco mode\n", 
                       idle_ms, timeout_ms);
            new_state = STATE_ECO;
        }
    }
    
    // State transition
    if (new_state != g_state) {
        state_exit_action(g_state);
        g_previous_state = g_state;
        g_state = new_state;
        state_entry_action(g_state);
        LOG_PRINT("State: %s -> %s (mode=%d, brew=%d)\n", 
                 state_names[g_previous_state], state_names[g_state], g_mode, g_brewing);
    }
}

// =============================================================================
// State Access
// =============================================================================

machine_state_t state_get(void) {
    return g_state;
}

machine_mode_t state_get_mode(void) {
    return g_mode;
}

bool state_set_mode(machine_mode_t mode) {
    if (g_brewing) {
        LOG_PRINT("State: Mode change blocked - brewing in progress\n");
        return false;  // Can't change mode while brewing
    }
    
    if (safety_is_defensive_mode()) {
        if (mode != MODE_IDLE) {
            LOG_PRINT("State: Mode change blocked - defensive mode (ESP32 disconnected)\n");
            return false;
        }
    }
    
    if (mode != g_mode) {
        LOG_PRINT("State: Mode change: %d -> %d\n", g_mode, mode);
    }
    
    
    // If entering IDLE from defensive mode, allow it (safety system needs this)
    g_mode = mode;
    return true;
}

// =============================================================================
// Brew Control
// =============================================================================

bool state_start_brew(void) {
    // Don't allow brew if in safe state (e.g., reservoir empty in water tank mode)
    if (safety_is_safe_state()) {
        LOG_PRINT("Brew: Cannot start - machine in safe state\n");
        return false;
    }
    
    if (g_brewing) {
        DEBUG_PRINT("Brew: Already brewing\n");
        return false;
    }
    
    LOG_PRINT("Brew: Starting (state=%s, mode=%d)\n", state_names[g_state], g_mode);
    
    // Allow brew to start in IDLE, HEATING, or READY states
    // This allows pump to work for purging even when machine is cold (first setup)
    if (g_state == STATE_IDLE || g_state == STATE_HEATING || g_state == STATE_READY) {
        DEBUG_PRINT("Brew: Starting (command)\n");
        g_brewing = true;
        return true;
    }
    return false;
}

bool state_stop_brew(void) {
    if (g_brewing) {
        LOG_PRINT("Brew: Stopping\n");
        // Stop shot timer immediately (capture exact stop time)
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (g_brew_stop_time == 0 && g_brew_start_time > 0) {
            g_brew_stop_time = now;
            DEBUG_PRINT("Brew: Stopping (shot time: %lu ms)\n", 
                       g_brew_stop_time - g_brew_start_time);
        }
        DEBUG_PRINT("Brew: Stopping (command)\n");
        g_brewing = false;
        return true;
    }
    return false;
}

bool state_is_brewing(void) {
    return g_brewing;
}

// =============================================================================
// Pre-Infusion Control
// =============================================================================

void state_set_preinfusion(bool enabled, uint16_t on_ms, uint16_t pause_ms) {
    g_preinfusion_enabled = enabled;
    g_preinfusion_on_ms = on_ms;
    g_preinfusion_pause_ms = pause_ms;
    DEBUG_PRINT("Pre-infusion: %s (on=%dms, pause=%dms)\n",
                enabled ? "enabled" : "disabled", on_ms, pause_ms);
}

void state_get_preinfusion(bool* enabled, uint16_t* on_ms, uint16_t* pause_ms) {
    if (enabled) *enabled = g_preinfusion_enabled;
    if (on_ms) *on_ms = g_preinfusion_on_ms;
    if (pause_ms) *pause_ms = g_preinfusion_pause_ms;
}

// =============================================================================
// State Queries
// =============================================================================

bool state_is_ready(void) {
    return g_state == STATE_READY;
}

bool state_is_heating(void) {
    return g_state == STATE_HEATING;
}

bool state_is_fault(void) {
    return g_state == STATE_FAULT || g_state == STATE_SAFE;
}

const char* state_get_name(machine_state_t state) {
    if (state < sizeof(state_names) / sizeof(state_names[0])) {
        return state_names[state];
    }
    return "UNKNOWN";
}

/**
 * Get shot timer duration in milliseconds
 * Returns 0 if not brewing or timer not started
 * Returns current elapsed time if brewing
 * Returns final duration if brewing stopped
 */
uint32_t state_get_brew_duration_ms(void) {
    if (g_brew_start_time == 0) {
        return 0;  // Timer not started
    }
    
    if (g_brewing) {
        // Timer is running - return current elapsed time
        return to_ms_since_boot(get_absolute_time()) - g_brew_start_time;
    } else if (g_brew_stop_time > 0) {
        // Timer stopped - return final duration
        return g_brew_stop_time - g_brew_start_time;
    }
    
    // Timer was started but not stopped yet (shouldn't happen, but safe fallback)
    return 0;
}

/**
 * Get brew start timestamp (milliseconds since boot)
 * Returns 0 if not brewing or timer not started
 * Client can calculate elapsed time: elapsed = now - timestamp
 */
uint32_t state_get_brew_start_timestamp_ms(void) {
    if (g_brewing && g_brew_start_time > 0) {
        // Brewing in progress - return start timestamp
        return g_brew_start_time;
    }
    // Not brewing - return 0
    return 0;
}

// =============================================================================
// Eco Mode
// =============================================================================

void state_set_eco_config(const eco_config_t* config) {
    if (!config) return;
    
    g_eco_config = *config;
    
    // Save to flash
    config_persistence_save_eco(config->enabled, config->eco_brew_temp, config->timeout_minutes);
    
    DEBUG_PRINT("Eco: Config updated (enabled=%d, temp=%d, timeout=%d min)\n",
               config->enabled, config->eco_brew_temp, config->timeout_minutes);
    
    // If eco was disabled while in eco mode, exit eco mode
    if (!config->enabled && g_state == STATE_ECO) {
        state_exit_eco();
    }
}

void state_get_eco_config(eco_config_t* config) {
    if (!config) return;
    *config = g_eco_config;
}

bool state_is_eco_mode(void) {
    return g_state == STATE_ECO;
}

bool state_enter_eco(void) {
    // Can only enter eco from READY or IDLE state
    if (g_state != STATE_READY && g_state != STATE_IDLE) {
        DEBUG_PRINT("Eco: Cannot enter eco mode from state %s\n", state_names[g_state]);
        return false;
    }
    
    if (g_brewing) {
        DEBUG_PRINT("Eco: Cannot enter eco mode while brewing\n");
        return false;
    }
    
    // Transition to eco mode
    state_exit_action(g_state);
    g_previous_state = g_state;
    g_state = STATE_ECO;
    state_entry_action(STATE_ECO);
    DEBUG_PRINT("State: %s -> ECO (manual)\n", state_names[g_previous_state]);
    
    return true;
}

bool state_exit_eco(void) {
    if (g_state != STATE_ECO) {
        return false;
    }
    
    // Transition out of eco mode to HEATING (to reach normal temp)
    state_exit_action(STATE_ECO);
    g_previous_state = STATE_ECO;
    
    // Determine target state based on saved mode
    if (g_saved_mode == MODE_IDLE) {
        g_state = STATE_IDLE;
    } else {
        g_state = STATE_HEATING;  // Start heating to reach normal temperature
    }
    
    state_entry_action(g_state);
    DEBUG_PRINT("State: ECO -> %s (wake)\n", state_names[g_state]);
    
    return true;
}

void state_reset_idle_timer(void) {
    g_last_activity_time = to_ms_since_boot(get_absolute_time());
    
    // If currently in eco mode and user activity detected, exit eco mode
    if (g_state == STATE_ECO) {
        state_exit_eco();
    }
}
