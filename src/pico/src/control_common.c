/**
 * Pico Firmware - Control System Common Implementation
 * 
 * Shared control code used by all machine types:
 * - PID computation with derivative filtering
 * - Hardware output control (SSRs, relays)
 * - Heating strategy implementation (dual boiler)
 * - Public API wrappers
 */

#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/mutex.h"     // For mutex_t
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"  // For save_and_disable_interrupts/restore_interrupts
#include "control.h"
#include "control_impl.h"
#include "config.h"
#include "sensors.h"
#include "protocol.h"
#include "machine_config.h"
#include "environmental_config.h"
#include "hardware.h"
#include "pcb_config.h"
#include "safety.h"
#include "state.h"
#include "power_meter.h"
#include <math.h>
#include <string.h>

// =============================================================================
// Global State (shared with machine-specific implementations)
// =============================================================================

pid_state_t g_brew_pid;
pid_state_t g_steam_pid;
heating_strategy_t g_heating_strategy = HEAT_SEQUENTIAL;

// =============================================================================
// Concurrency Protection
// =============================================================================
// Mutex to protect PID and control state from torn reads/writes.
// Core 1 (protocol) updates settings, Core 0 (control loop) reads them.
// Without protection, Core 0 could read partially-updated state (e.g., new Kp
// but old Ki/Kd), causing output glitches.
static mutex_t g_control_mutex;
static bool g_control_mutex_initialized = false;

static inline void control_lock(void) {
    if (g_control_mutex_initialized) {
        mutex_enter_blocking(&g_control_mutex);
    }
}

static inline void control_unlock(void) {
    if (g_control_mutex_initialized) {
        mutex_exit(&g_control_mutex);
    }
}

// =============================================================================
// Private State
// =============================================================================

static control_outputs_t g_outputs = {0};
static bool g_outputs_initialized = false;

// Hardware PWM slices
static uint8_t g_pwm_slice_brew = 0xFF;
static uint8_t g_pwm_slice_steam = 0xFF;

// Heating strategy configuration
static float g_sequential_threshold_pct = 80.0f;
static float g_max_combined_duty = 150.0f;
static uint8_t g_stagger_priority = 0;

// Phase synchronization for HEAT_SMART_STAGGER
#define PHASE_SYNC_PERIOD_MS 1000  // 1 second period for phase sync
typedef struct {
    uint32_t start_ms;      // Start time within period (0-1000ms)
    uint32_t duration_ms;   // Duration ON (0-1000ms)
    bool active;            // True if schedule is active
} ssr_schedule_t;

// Note: These are accessed from both main loop and timer interrupt.
// Write access is protected by critical sections in set_ssr_schedule().
// volatile ensures compiler doesn't cache values in the timer callback.
static volatile ssr_schedule_t g_brew_schedule = {0};
static volatile ssr_schedule_t g_steam_schedule = {0};
static struct repeating_timer g_phase_timer;
static bool g_phase_sync_active = false;
static uint32_t g_phase_period_start = 0;  // Timestamp of current period start

// Forward declaration for phase sync helper
static void set_ssr_schedule(uint8_t ssr_id, uint32_t start_ms, uint32_t duration_ms);

// =============================================================================
// PID Initialization
// =============================================================================

void pid_init(pid_state_t* pid, float setpoint) {
    pid->kp = PID_DEFAULT_KP;
    pid->ki = PID_DEFAULT_KI;
    pid->kd = PID_DEFAULT_KD;
    pid->setpoint = setpoint;
    pid->setpoint_target = setpoint;
    pid->integral = 0.0f;
    pid->last_error = 0.0f;
    pid->last_measurement = 0.0f;
    pid->last_derivative = 0.0f;
    pid->output = 0.0f;
    pid->setpoint_ramping = false;
    pid->ramp_rate = 1.0f;
    pid->first_run = true;  // Skip derivative on first call to avoid spike
}

// =============================================================================
// PID Computation with Derivative Filtering
// =============================================================================

float pid_compute(pid_state_t* pid, float process_value, float dt) {
    // Lock for entire PID computation to prevent race with control_set_pid on Core 1
    // This protects all state fields: setpoint, integral, last_error, last_measurement,
    // last_derivative, first_run, output - which could be modified by control_set_pid
    //
    // PERFORMANCE NOTE: This is a blocking mutex, but the risk is low because:
    // 1. control_set_pid() (called by Core 1) is very fast (just variable assignments)
    // 2. Pico mutex implementation uses fast spinlocks (not OS-level blocking)
    // 3. PID parameter updates are infrequent (user-initiated, not continuous)
    //
    // Alternative approaches (double-buffering PID params) would add complexity
    // for minimal benefit. This trade-off prioritizes thread safety and code simplicity.
    control_lock();
    
    // Read parameters (protected by lock)
    float kp = pid->kp;
    float ki = pid->ki;
    float kd = pid->kd;
    float setpoint_target = pid->setpoint_target;
    bool ramping = pid->setpoint_ramping;
    float ramp_rate = pid->ramp_rate;
    float setpoint = pid->setpoint;
    
    // Update setpoint with ramping if enabled
    if (ramping) {
        float setpoint_diff = setpoint_target - setpoint;
        float max_change = ramp_rate * dt;
        
        if (fabsf(setpoint_diff) <= max_change) {
            setpoint = setpoint_target;
            ramping = false;
        } else {
            if (setpoint_diff > 0) {
                setpoint += max_change;
            } else {
                setpoint -= max_change;
            }
        }
        
        pid->setpoint = setpoint;
        pid->setpoint_ramping = ramping;
    }
    
    float error = setpoint - process_value;
    
    // Proportional
    float p_term = kp * error;
    
    // Integral with anti-windup
    // Guard against division by zero when ki is 0 or very small (P-only control)
    float i_term = 0.0f;
    if (ki > 0.001f) {
        pid->integral += error * dt;
        float max_integral = PID_OUTPUT_MAX / ki;
        if (pid->integral > max_integral) pid->integral = max_integral;
        if (pid->integral < -max_integral) pid->integral = -max_integral;
        i_term = ki * pid->integral;
    } else {
        // Ki disabled or negligible - reset integral to prevent windup when re-enabled
        pid->integral = 0.0f;
    }
    
    // Derivative with filtering (first-order low-pass filter)
    // IMPORTANT: Use derivative-on-measurement, NOT derivative-on-error
    // This prevents "derivative kick" when setpoint changes
    // 
    // When measurement increases (approaching setpoint from below),
    // d(measurement)/dt is positive, so we use negative sign to reduce output
    // 
    // On first call, skip derivative calculation to avoid spike
    float d_term = 0.0f;
    if (pid->first_run) {
        // First call: initialize last_measurement, skip derivative
        pid->last_measurement = process_value;
        pid->last_derivative = 0.0f;
        pid->first_run = false;
    } else {
        // Normal operation: derivative on measurement (negative sign for correct action)
        float measurement_derivative = (process_value - pid->last_measurement) / dt;
        
        // Low-pass filter on derivative
        // alpha = dt / (tau + dt), where tau is filter time constant
        float tau = PID_DERIVATIVE_FILTER_TAU;
        float alpha = dt / (tau + dt);
        pid->last_derivative = alpha * measurement_derivative + (1.0f - alpha) * pid->last_derivative;
        
        // Negative sign: increasing measurement should reduce output
        d_term = -kd * pid->last_derivative;
        
        pid->last_measurement = process_value;
    }
    
    pid->last_error = error;
    
    // Sum and clamp
    float output = p_term + i_term + d_term;
    if (output > PID_OUTPUT_MAX) output = PID_OUTPUT_MAX;
    if (output < PID_OUTPUT_MIN) output = PID_OUTPUT_MIN;
    
    pid->output = output;
    
    control_unlock();
    
    return output;
}

// =============================================================================
// Heating Strategy Implementation (Dual Boiler Only)
// =============================================================================

// Function pointer type for heating strategies
typedef void (*strategy_func_t)(
    float brew_demand, float steam_demand,
    float brew_temp, float steam_temp,
    float* brew_duty, float* steam_duty
);

// Strategy: Brew only - only brew boiler heats, steam stays off
static void strategy_brew_only(
    float brew_demand, float steam_demand,
    float brew_temp, float steam_temp,
    float* brew_duty, float* steam_duty
) {
    (void)steam_demand;  // Unused
    (void)brew_temp;      // Unused
    (void)steam_temp;     // Unused
    *brew_duty = brew_demand;
    *steam_duty = 0.0f;
}

// Strategy: Sequential - brew first, steam starts after brew reaches threshold
static void strategy_sequential(
    float brew_demand, float steam_demand,
    float brew_temp, float steam_temp,
    float* brew_duty, float* steam_duty
) {
    (void)steam_temp;  // Unused
    float brew_setpoint = g_brew_pid.setpoint;
    *brew_duty = brew_demand;
    float brew_pct = (brew_temp / brew_setpoint) * 100.0f;
    *steam_duty = (brew_pct >= g_sequential_threshold_pct) ? steam_demand : 0.0f;
}

// Strategy: Parallel - both heat simultaneously (fastest, highest power)
static void strategy_parallel(
    float brew_demand, float steam_demand,
    float brew_temp, float steam_temp,
    float* brew_duty, float* steam_duty
) {
    (void)brew_temp;   // Unused
    (void)steam_temp;  // Unused
    electrical_state_t elec_state;
    electrical_state_get(&elec_state);
    
    float brew_current = elec_state.brew_heater_current * (brew_demand / 100.0f);
    float steam_current = elec_state.steam_heater_current * (steam_demand / 100.0f);
    float total_current = brew_current + steam_current;
    
    if (total_current > elec_state.max_combined_current) {
        float scale_factor = elec_state.max_combined_current / total_current;
        brew_demand *= scale_factor;
        steam_demand *= scale_factor;
    }
    *brew_duty = brew_demand;
    *steam_duty = steam_demand;
}

// Strategy: Smart stagger - both heat with limited combined duty and phase synchronization
static void strategy_smart_stagger(
    float brew_demand, float steam_demand,
    float brew_temp, float steam_temp,
    float* brew_duty, float* steam_duty
) {
    (void)brew_temp;   // Unused
    (void)steam_temp;  // Unused
    // Phase-synchronized control: Calculate duty cycles and phase offsets
    float brew_duty_pct = brew_demand;
    float steam_duty_pct = steam_demand;
    
    // Clamp total based on max_combined_duty
    float max_total = g_max_combined_duty;
    float total_req = brew_duty_pct + steam_duty_pct;
    
    if (total_req > max_total) {
        if (g_stagger_priority == 0) {
            // Brew priority: give brew what it wants, steam gets remainder
            if (brew_duty_pct > max_total) brew_duty_pct = max_total;
            steam_duty_pct = max_total - brew_duty_pct;
        } else {
            // Steam priority
            if (steam_duty_pct > max_total) steam_duty_pct = max_total;
            brew_duty_pct = max_total - steam_duty_pct;
        }
    }
    
    // Convert duty % to time (ms) within 1-second period
    uint32_t brew_time_ms = (uint32_t)(brew_duty_pct * 10.0f);  // 0-1000ms
    uint32_t steam_time_ms = (uint32_t)(steam_duty_pct * 10.0f);
    
    // PHASE SHIFT: Brew starts at t=0, Steam starts AFTER brew finishes
    // This ensures strict interleaving (no overlap) when total <= 100%
    uint32_t brew_start = 0;
    uint32_t steam_start = brew_time_ms;
    
    // Wrap-around behavior when total > 100% (max_combined_duty > 100%)
    // Example: Brew 80%, Steam 40% (total 120%, max_combined_duty = 120%)
    //   - Brew runs 0ms-800ms
    //   - Steam wraps to 0ms, runs 0ms-400ms
    //   - Result: Parallel heating for first 400ms, then Brew-only for 400ms-800ms
    // 
    // IMPACT: Peak current draw (both heaters ON) occurs at the START of the period.
    // This is acceptable as long as max_combined_duty in environmental_config
    // correctly accounts for your hardware's breaker limits. The wrap-around ensures
    // the average power over the 1-second period matches the requested duty cycles.
    if (steam_start + steam_time_ms > PHASE_SYNC_PERIOD_MS) {
        steam_start = 0;  // Wrap to beginning - creates controlled overlap
    }
    
    // Set schedules for phase-synchronized control
    set_ssr_schedule(0, brew_start, brew_time_ms);      // Brew SSR
    set_ssr_schedule(1, steam_start, steam_time_ms);     // Steam SSR
    
    // Return duty values for reporting (but actual control uses phase sync)
    *brew_duty = brew_duty_pct;
    *steam_duty = steam_duty_pct;
}

// Strategy function pointer array - indexed by heating_strategy_t enum
static const strategy_func_t STRATEGIES[] = {
    strategy_brew_only,      // HEAT_BREW_ONLY = 0
    strategy_sequential,      // HEAT_SEQUENTIAL = 1
    strategy_parallel,       // HEAT_PARALLEL = 2
    strategy_smart_stagger   // HEAT_SMART_STAGGER = 3
};

void apply_heating_strategy(
    float brew_demand, float steam_demand,
    float brew_temp, float steam_temp,
    float* brew_duty, float* steam_duty
) {
    // Bounds check - use brew_only as safe fallback for invalid strategy
    if (g_heating_strategy >= sizeof(STRATEGIES) / sizeof(STRATEGIES[0])) {
        strategy_brew_only(brew_demand, steam_demand, brew_temp, steam_temp, brew_duty, steam_duty);
        return;
    }
    
    // Call appropriate strategy function
    STRATEGIES[g_heating_strategy](brew_demand, steam_demand, brew_temp, steam_temp, brew_duty, steam_duty);
    
    // Apply safety limit (common to all strategies)
    float max_duty = 95.0f;
    *brew_duty = fminf(*brew_duty, max_duty);
    *steam_duty = fminf(*steam_duty, max_duty);
}

// =============================================================================
// Phase Synchronization Timer (for HEAT_SMART_STAGGER)
// =============================================================================

/**
 * Timer callback for phase-synchronized SSR control
 * Called every 10ms to check and update SSR states based on schedules
 */
static bool phase_sync_timer_callback(struct repeating_timer *t) {
    (void)t;  // Unused
    
    const pcb_config_t* pcb = pcb_config_get();
    if (!pcb) return true;
    
    uint32_t now = to_ms_since_boot(get_absolute_time());
    uint32_t period_offset = (now - g_phase_period_start) % PHASE_SYNC_PERIOD_MS;
    
    // Update brew SSR based on schedule
    if (pcb->pins.ssr_brew >= 0 && g_brew_schedule.active) {
        bool should_be_on = (period_offset >= g_brew_schedule.start_ms) &&
                            (period_offset < (g_brew_schedule.start_ms + g_brew_schedule.duration_ms));
        hw_set_gpio((uint8_t)pcb->pins.ssr_brew, should_be_on);
    }
    
    // Update steam SSR based on schedule
    if (pcb->pins.ssr_steam >= 0 && g_steam_schedule.active) {
        bool should_be_on = (period_offset >= g_steam_schedule.start_ms) &&
                            (period_offset < (g_steam_schedule.start_ms + g_steam_schedule.duration_ms));
        hw_set_gpio((uint8_t)pcb->pins.ssr_steam, should_be_on);
    }
    
    return true;  // Continue repeating
}

/**
 * Start phase synchronization timer
 */
static bool start_phase_sync(void) {
    if (g_phase_sync_active) {
        return true;  // Already active
    }
    
    // Disable hardware PWM for SSRs (we'll control GPIO directly)
    const pcb_config_t* pcb = pcb_config_get();
    if (pcb) {
        if (pcb->pins.ssr_brew >= 0 && g_pwm_slice_brew != 0xFF) {
            hw_pwm_set_enabled(g_pwm_slice_brew, false);
            // Set GPIO to output mode for direct control
            hw_gpio_init_output((uint8_t)pcb->pins.ssr_brew, false);
        }
        if (pcb->pins.ssr_steam >= 0 && g_pwm_slice_steam != 0xFF) {
            hw_pwm_set_enabled(g_pwm_slice_steam, false);
            // Set GPIO to output mode for direct control
            hw_gpio_init_output((uint8_t)pcb->pins.ssr_steam, false);
        }
    }
    
    // Initialize schedules
    g_brew_schedule.active = false;
    g_steam_schedule.active = false;
    g_phase_period_start = to_ms_since_boot(get_absolute_time());
    
    // Start repeating timer (10ms resolution for smooth control)
    if (add_repeating_timer_ms(-10, phase_sync_timer_callback, NULL, &g_phase_timer)) {
        g_phase_sync_active = true;
        DEBUG_PRINT("Control: Phase sync timer started\n");
        return true;
    }
    
    return false;
}

/**
 * Stop phase synchronization timer and restore hardware PWM
 */
static void stop_phase_sync(void) {
    if (!g_phase_sync_active) {
        return;
    }
    
    // Cancel timer
    cancel_repeating_timer(&g_phase_timer);
    g_phase_sync_active = false;
    
    // Turn off SSRs
    const pcb_config_t* pcb = pcb_config_get();
    if (pcb) {
        if (pcb->pins.ssr_brew >= 0) {
            hw_set_gpio((uint8_t)pcb->pins.ssr_brew, false);
            // Restore PWM function
            gpio_set_function((uint)pcb->pins.ssr_brew, GPIO_FUNC_PWM);
            if (g_pwm_slice_brew != 0xFF) {
                hw_pwm_set_enabled(g_pwm_slice_brew, true);
            }
        }
        if (pcb->pins.ssr_steam >= 0) {
            hw_set_gpio((uint8_t)pcb->pins.ssr_steam, false);
            // Restore PWM function
            gpio_set_function((uint)pcb->pins.ssr_steam, GPIO_FUNC_PWM);
            if (g_pwm_slice_steam != 0xFF) {
                hw_pwm_set_enabled(g_pwm_slice_steam, true);
            }
        }
    }
    
    // Use critical section when updating volatile schedules
    uint32_t irq_state = save_and_disable_interrupts();
    g_brew_schedule.active = false;
    g_steam_schedule.active = false;
    restore_interrupts(irq_state);
    
    DEBUG_PRINT("Control: Phase sync timer stopped\n");
}

/**
 * Set SSR schedule for phase-synchronized control
 * 
 * CRITICAL: This function is called from the main loop, but the schedule
 * is read from a timer interrupt (phase_sync_timer_callback). We use
 * critical sections to ensure atomic updates of the schedule struct.
 */
static void set_ssr_schedule(uint8_t ssr_id, uint32_t start_ms, uint32_t duration_ms) {
    // Validate and clamp inputs
    if (start_ms >= PHASE_SYNC_PERIOD_MS) {
        start_ms = start_ms % PHASE_SYNC_PERIOD_MS;
    }
    if (duration_ms > PHASE_SYNC_PERIOD_MS) {
        duration_ms = PHASE_SYNC_PERIOD_MS;
    }
    
    // Calculate active flag
    bool active = (duration_ms > 0);
    
    // Critical section: Disable interrupts while updating shared state
    // This prevents the timer interrupt from reading a partially-updated schedule
    uint32_t irq_state = save_and_disable_interrupts();
    
    // Update the volatile schedule struct field by field
    volatile ssr_schedule_t* schedule = (ssr_id == 0) ? &g_brew_schedule : &g_steam_schedule;
    schedule->start_ms = start_ms;
    schedule->duration_ms = duration_ms;
    schedule->active = active;
    
    // Reset period start when schedules change to ensure clean phase alignment
    g_phase_period_start = to_ms_since_boot(get_absolute_time());
    
    restore_interrupts(irq_state);
}

// =============================================================================
// Hardware Output Control
// =============================================================================

bool init_hardware_outputs(void) {
    const pcb_config_t* pcb = pcb_config_get();
    if (!pcb) return false;
    
    // Initialize PWM for SSR control
    if (pcb->pins.ssr_brew >= 0) {
        if (hw_pwm_init_ssr(pcb->pins.ssr_brew, &g_pwm_slice_brew)) {
            DEBUG_PRINT("Brew SSR PWM initialized (slice %d)\n", g_pwm_slice_brew);
        }
    }
    
    if (pcb->pins.ssr_steam >= 0) {
        if (hw_pwm_init_ssr(pcb->pins.ssr_steam, &g_pwm_slice_steam)) {
            DEBUG_PRINT("Steam SSR PWM initialized (slice %d)\n", g_pwm_slice_steam);
        }
    }
    
    // Initialize relay outputs
    if (pcb->pins.relay_pump >= 0) {
        hw_gpio_init_output(pcb->pins.relay_pump, false);
    }
    if (pcb->pins.relay_brew_solenoid >= 0) {
        hw_gpio_init_output(pcb->pins.relay_brew_solenoid, false);
    }
    
    g_outputs_initialized = true;
    return true;
}

void apply_hardware_outputs(uint8_t brew_heater, uint8_t steam_heater, uint8_t pump) {
    const pcb_config_t* pcb = pcb_config_get();
    if (!pcb || !g_outputs_initialized) return;
    
    // For HEAT_SMART_STAGGER, phase sync timer handles SSR control
    // Schedules are set in apply_heating_strategy()
    if (g_heating_strategy == HEAT_SMART_STAGGER) {
        // Phase sync is active, timer callback handles SSR states
        // Just ensure timer is running
        if (!g_phase_sync_active) {
            start_phase_sync();
        }
    } else {
        // Other strategies use hardware PWM
        if (g_phase_sync_active) {
            stop_phase_sync();
        }
        
        // Apply SSR outputs via hardware PWM
        if (g_pwm_slice_brew != 0xFF && pcb->pins.ssr_brew >= 0) {
            hw_set_pwm_duty(g_pwm_slice_brew, (float)brew_heater);
        }
        
        if (g_pwm_slice_steam != 0xFF && pcb->pins.ssr_steam >= 0) {
            hw_set_pwm_duty(g_pwm_slice_steam, (float)steam_heater);
        }
    }
    
    // Apply relay outputs (always direct GPIO)
    if (pcb->pins.relay_pump >= 0) {
        hw_set_gpio(pcb->pins.relay_pump, pump > 0);
    }
}

uint16_t estimate_power_watts(uint8_t brew_duty, uint8_t steam_duty) {
    const machine_electrical_t* elec = machine_get_electrical();
    uint16_t brew_watts = (brew_duty * elec->brew_heater_power) / 100;
    uint16_t steam_watts = (steam_duty * elec->steam_heater_power) / 100;
    return brew_watts + steam_watts;
}

// =============================================================================
// Public API: Initialization
// =============================================================================

void control_init(void) {
    // Initialize mutex for thread-safe control state access
    mutex_init(&g_control_mutex);
    g_control_mutex_initialized = true;
    
    // Initialize PIDs with default values
    pid_init(&g_brew_pid, TEMP_DECI_TO_C(DEFAULT_BREW_TEMP));
    pid_init(&g_steam_pid, TEMP_DECI_TO_C(DEFAULT_STEAM_TEMP));
    
    // Initialize hardware
    init_hardware_outputs();
    
    // Machine-specific initialization
    control_init_machine();
    
    LOG_PRINT("Control: Initialized (Brew SP=%.1fC, Steam SP=%.1fC, Strategy=%d)\n",
              g_brew_pid.setpoint, g_steam_pid.setpoint, g_heating_strategy);
    
    // Initialize power meter if configured (PZEM, JSY, Eastron, etc.)
    if (power_meter_init(NULL)) {
        const char* meter_name = power_meter_get_name();
        DEBUG_PRINT("Power meter initialized: %s\n", meter_name);
    }
}

// =============================================================================
// Public API: Control Update
// =============================================================================

void control_update(void) {
    // Don't update if in safe state
    if (safety_is_safe_state()) {
        g_outputs.brew_heater = 0;
        g_outputs.steam_heater = 0;
        g_outputs.pump = 0;
        apply_hardware_outputs(0, 0, 0);
        return;
    }
    
    // Don't heat if machine is in IDLE mode
    machine_mode_t mode = state_get_mode();
    if (mode == MODE_IDLE) {
        g_outputs.brew_heater = 0;
        g_outputs.steam_heater = 0;
        apply_hardware_outputs(0, 0, g_outputs.pump);
        return;
    }
    
    // Read sensors
    sensor_data_t sensors;
    sensors_get_data(&sensors);
    
    float dt = CONTROL_DT_SEC;  // Control loop time delta (0.1s = 100ms / 1000)
    float brew_temp = sensors.brew_temp / 10.0f;
    float steam_temp = sensors.steam_temp / 10.0f;
    float group_temp = sensors.group_temp / 10.0f;
    
    // Machine-specific control logic
    float brew_duty = 0.0f;
    float steam_duty = 0.0f;
    
    control_update_machine(mode, brew_temp, steam_temp, group_temp, dt,
                          &brew_duty, &steam_duty);
    
    // Store and apply outputs
    g_outputs.brew_heater = (uint8_t)brew_duty;
    g_outputs.steam_heater = (uint8_t)steam_duty;
    apply_hardware_outputs(g_outputs.brew_heater, g_outputs.steam_heater, g_outputs.pump);
    
    // Update simulation
    sensors_sim_set_heating(g_outputs.brew_heater > 0 || g_outputs.steam_heater > 0);
    
    // Power measurement
    power_meter_reading_t power_reading;
    if (power_meter_is_connected() && power_meter_get_reading(&power_reading) && power_reading.valid) {
        g_outputs.power_watts = (uint16_t)power_reading.power;
    } else {
        g_outputs.power_watts = estimate_power_watts(g_outputs.brew_heater, g_outputs.steam_heater);
    }
}

// =============================================================================
// Public API: Setpoint Control
// =============================================================================

void control_set_setpoint(uint8_t target, int16_t temp) {
    float temp_c = temp / 10.0f;
    
    // Lock to prevent torn reads by control loop
    control_lock();
    
    pid_state_t* pid = (target == 0) ? &g_brew_pid : &g_steam_pid;
    pid->setpoint_target = temp_c;
    pid->setpoint_ramping = true;
    
    control_unlock();
    
    LOG_PRINT("Control: %s setpoint changed: %.1fC\n", target == 0 ? "Brew" : "Steam", temp_c);
}

int16_t control_get_setpoint(uint8_t target) {
    pid_state_t* pid = (target == 0) ? &g_brew_pid : &g_steam_pid;
    // Return the target setpoint (what user set), not the ramping intermediate value
    // This ensures status messages report the user's desired temperature
    return (int16_t)(pid->setpoint_target * 10);
}

// =============================================================================
// Public API: PID Tuning
// =============================================================================

void control_set_pid(uint8_t target, float kp, float ki, float kd) {
    if (target > 1) return;
    if (kp < 0.0f || ki < 0.0f || kd < 0.0f) return;
    if (kp > 100.0f || ki > 100.0f || kd > 100.0f) return;
    
    // Lock to prevent torn reads by control loop (Core 0)
    // This function is called by Core 1 (protocol handler) and is very fast
    // (just variable assignments), so blocking Core 0 is minimal (< 1µs typically).
    // The mutex ensures atomic updates of all PID parameters together.
    control_lock();
    
    pid_state_t* pid = (target == 0) ? &g_brew_pid : &g_steam_pid;
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->integral = 0.0f;
    pid->last_error = 0.0f;
    pid->last_measurement = 0.0f;
    pid->last_derivative = 0.0f;
    // Set first_run to skip derivative on next compute (prevents spike from stale last_measurement)
    pid->first_run = true;
    
    control_unlock();
    
    LOG_PRINT("Control: PID[%d] updated: Kp=%.2f Ki=%.2f Kd=%.2f\n", target, kp, ki, kd);
}

void control_get_pid(uint8_t target, float* kp, float* ki, float* kd) {
    if (!kp || !ki || !kd) return;
    pid_state_t* pid = (target == 0) ? &g_brew_pid : &g_steam_pid;
    *kp = pid->kp;
    *ki = pid->ki;
    *kd = pid->kd;
}

// =============================================================================
// Public API: Output Control
// =============================================================================

void control_set_output(uint8_t output, uint8_t value, uint8_t mode) {
    if (value > 100) value = 100;
    if (mode > 1) return;
    
    switch (output) {
        case 0:
            g_outputs.brew_heater = value;
            break;
        case 1:
            g_outputs.steam_heater = value;
            break;
        case 2:
            g_outputs.pump = value;
            break;
        default:
            return;
    }
    
    if (mode == 1) {
        apply_hardware_outputs(g_outputs.brew_heater, g_outputs.steam_heater, g_outputs.pump);
    }
}

void control_get_outputs(control_outputs_t* outputs) {
    if (outputs) *outputs = g_outputs;
}

void control_set_pump(uint8_t value) {
    if (value > 100) value = 100;
    g_outputs.pump = value;
    apply_hardware_outputs(g_outputs.brew_heater, g_outputs.steam_heater, g_outputs.pump);
}

// =============================================================================
// Public API: Heating Strategy
// =============================================================================

static float calculate_strategy_max_current(uint8_t strategy) {
    electrical_state_t elec_state;
    electrical_state_get(&elec_state);
    
    float brew = elec_state.brew_heater_current;
    float steam = elec_state.steam_heater_current;
    
    switch (strategy) {
        case HEAT_BREW_ONLY: return brew;
        case HEAT_SEQUENTIAL: return fmaxf(brew, steam);
        case HEAT_PARALLEL:
        case HEAT_SMART_STAGGER: return brew + steam;
        default: return 0.0f;
    }
}

bool control_is_heating_strategy_allowed(uint8_t strategy) {
    if (strategy > HEAT_SMART_STAGGER) return false;
    
    // Non-dual-boiler machines only support BREW_ONLY (single SSR)
    const machine_features_t* features = machine_get_features();
    if (features && features->type != MACHINE_TYPE_DUAL_BOILER) {
        return (strategy == HEAT_BREW_ONLY);
    }
    
    // Dual boiler: check electrical limits
    electrical_state_t elec_state;
    electrical_state_get(&elec_state);
    
    if (elec_state.nominal_voltage == 0 || elec_state.max_current_draw <= 0.0f) return false;
    
    float max_current = calculate_strategy_max_current(strategy);
    return max_current <= elec_state.max_combined_current;
}

uint8_t control_get_allowed_strategies(uint8_t* allowed, uint8_t max_count) {
    if (!allowed || max_count == 0) return 0;
    
    // Non-dual-boiler machines only support BREW_ONLY
    const machine_features_t* features = machine_get_features();
    if (features && features->type != MACHINE_TYPE_DUAL_BOILER) {
        allowed[0] = HEAT_BREW_ONLY;
        return 1;
    }
    
    // Dual boiler: return all allowed strategies based on electrical limits
    uint8_t count = 0;
    for (uint8_t s = 0; s <= HEAT_SMART_STAGGER && count < max_count; s++) {
        if (control_is_heating_strategy_allowed(s)) {
            allowed[count++] = s;
        }
    }
    return count;
}

bool control_set_heating_strategy(uint8_t strategy) {
    // Stop phase sync if switching away from HEAT_SMART_STAGGER
    if (g_heating_strategy == HEAT_SMART_STAGGER && strategy != HEAT_SMART_STAGGER) {
        stop_phase_sync();
    }
    
    // Start phase sync if switching to HEAT_SMART_STAGGER
    if (strategy == HEAT_SMART_STAGGER && g_heating_strategy != HEAT_SMART_STAGGER) {
        start_phase_sync();
    }
    if (strategy > HEAT_SMART_STAGGER) return false;
    
    // Heating strategies only apply to dual boiler machines
    // Single boiler and HX have only one SSR, so only BREW_ONLY is valid
    const machine_features_t* features = machine_get_features();
    if (features && features->type != MACHINE_TYPE_DUAL_BOILER) {
        if (strategy != HEAT_BREW_ONLY) {
            DEBUG_PRINT("Heating strategy: Only BREW_ONLY valid for non-dual-boiler machines\n");
            return false;
        }
    }
    
    if (!control_is_heating_strategy_allowed(strategy)) return false;
    
    g_heating_strategy = (heating_strategy_t)strategy;
    LOG_PRINT("Control: Heating strategy changed: %d\n", strategy);
    return true;
}

uint8_t control_get_heating_strategy(void) {
    return (uint8_t)g_heating_strategy;
}

// =============================================================================
// Public API: Configuration
// =============================================================================

void control_get_config(config_payload_t* config) {
    if (!config) return;
    
    config->brew_setpoint = (int16_t)(g_brew_pid.setpoint * 10);
    config->steam_setpoint = (int16_t)(g_steam_pid.setpoint * 10);
    config->temp_offset = DEFAULT_OFFSET_TEMP;
    config->pid_kp = (uint16_t)(g_brew_pid.kp * 100);
    config->pid_ki = (uint16_t)(g_brew_pid.ki * 100);
    config->pid_kd = (uint16_t)(g_brew_pid.kd * 100);
    config->heating_strategy = control_get_heating_strategy();
    config->machine_type = (uint8_t)machine_get_type();
}

void control_set_config(const config_payload_t* config) {
    if (!config) return;
    
    control_set_setpoint(0, config->brew_setpoint);
    control_set_setpoint(1, config->steam_setpoint);
    control_set_pid(0, config->pid_kp / 100.0f, config->pid_ki / 100.0f, config->pid_kd / 100.0f);
    control_set_heating_strategy(config->heating_strategy);
}

// =============================================================================
// Public API: Single Boiler Mode (delegated to machine-specific)
// =============================================================================

uint8_t control_get_single_boiler_mode(void) {
    return control_get_machine_mode();
}

bool control_is_single_boiler_switching(void) {
    return control_is_machine_switching();
}

