# Firmware Architecture

## Overview

The ECM firmware runs on the RP2040 microcontroller (Raspberry Pi Pico), utilizing both cores for real-time control and communication.

**Language:** C (Pico SDK)  
**Cores:** Dual-core ARM Cortex-M0+ @ 133MHz  
**Memory:** 264KB SRAM, 2MB Flash  

---

## Dual-Core Design

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           RP2040 DUAL-CORE ARCHITECTURE                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   ┌───────────────────────────────┐   ┌───────────────────────────────┐     │
│   │          CORE 0               │   │          CORE 1               │     │
│   │    (Real-Time Control)        │   │      (Communication)          │     │
│   │                               │   │                               │     │
│   │  Priority: CRITICAL           │   │  Priority: NORMAL             │     │
│   │  Timing: Deterministic        │   │  Timing: Best-effort          │     │
│   │                               │   │                               │     │
│   │  ┌─────────────────────────┐  │   │  ┌─────────────────────────┐  │     │
│   │  │ Safety Interlocks       │  │   │  │ ESP32 UART Handler     │  │     │
│   │  │ (every 100ms, first)    │  │   │  │ (interrupt-driven)     │  │     │
│   │  └─────────────────────────┘  │   │  └─────────────────────────┘  │     │
│   │            │                  │   │            │                  │     │
│   │            ▼                  │   │            ▼                  │     │
│   │  ┌─────────────────────────┐  │   │  ┌─────────────────────────┐  │     │
│   │  │ Sensor Reading          │  │   │  │ PZEM UART Handler      │  │     │
│   │  │ (NTC, TC, Pressure)     │  │   │  │ (1000ms polling)       │  │     │
│   │  └─────────────────────────┘  │   │  └─────────────────────────┘  │     │
│   │            │                  │   │            │                  │     │
│   │            ▼                  │   │            ▼                  │     │
│   │  ┌─────────────────────────┐  │   │  ┌─────────────────────────┐  │     │
│   │  │ PID Computation         │  │   │  │ Protocol Encoding      │  │     │
│   │  │ (Brew + Steam)          │  │   │  │ (state → binary)       │  │     │
│   │  └─────────────────────────┘  │   │  └─────────────────────────┘  │     │
│   │            │                  │   │            │                  │     │
│   │            ▼                  │   │            ▼                  │     │
│   │  ┌─────────────────────────┐  │   │  ┌─────────────────────────┐  │     │
│   │  │ Output Control          │  │   │  │ Command Processing     │  │     │
│   │  │ (SSR PWM, Relays)       │  │   │  │ (from ESP32)           │  │     │
│   │  └─────────────────────────┘  │   │  └─────────────────────────┘  │     │
│   │            │                  │   │                               │     │
│   │            ▼                  │   │                               │     │
│   │  ┌─────────────────────────┐  │   │                               │     │
│   │  │ Watchdog Feed           │  │   │                               │     │
│   │  │ (end of loop)           │  │   │                               │     │
│   │  └─────────────────────────┘  │   │                               │     │
│   │                               │   │                               │     │
│   └───────────────────────────────┘   └───────────────────────────────┘     │
│                    │                               │                         │
│                    └───────────────┬───────────────┘                         │
│                                    │                                         │
│                    ┌───────────────▼───────────────┐                         │
│                    │      SHARED STATE             │                         │
│                    │   (Mutex-Protected)           │                         │
│                    │                               │                         │
│                    │  • Temperatures               │                         │
│                    │  • Pressure                   │                         │
│                    │  • Levels                     │                         │
│                    │  • Output states              │                         │
│                    │  • Setpoints                  │                         │
│                    │  • Alarms                     │                         │
│                    │  • Machine state              │                         │
│                    │                               │                         │
│                    └───────────────────────────────┘                         │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Core 0: Control Loop

The control loop runs every 100ms with strict timing requirements.

### Loop Structure

```c
// Core 0 main loop - 100ms period
void core0_main(void) {
    const uint32_t LOOP_PERIOD_MS = 100;
    
    while (true) {
        uint32_t loop_start = time_us_32();
        
        // ═══════════════════════════════════════════════════════════
        // 1. SAFETY INTERLOCKS (Always first, non-negotiable)
        // ═══════════════════════════════════════════════════════════
        if (!check_safety_interlocks()) {
            enter_safe_state();
            set_machine_state(STATE_FAULT);
            // Continue loop to keep checking
        }
        
        // ═══════════════════════════════════════════════════════════
        // 2. SENSOR READING
        // ═══════════════════════════════════════════════════════════
        sensor_data_t sensors = {0};
        
        sensors.brew_ntc = read_ntc_filtered(ADC_BREW_NTC);
        sensors.steam_ntc = read_ntc_filtered(ADC_STEAM_NTC);
        sensors.group_tc = read_thermocouple();
        sensors.pressure = read_pressure();
        sensors.levels = read_level_switches();
        sensors.brew_switch = read_brew_switch();
        
        // ═══════════════════════════════════════════════════════════
        // 3. UPDATE SHARED STATE (with mutex)
        // ═══════════════════════════════════════════════════════════
        mutex_enter_blocking(&state_mutex);
        shared_state.sensors = sensors;
        shared_state.uptime_ms = to_ms_since_boot(get_absolute_time());
        mutex_exit(&state_mutex);
        
        // ═══════════════════════════════════════════════════════════
        // 4. STATE MACHINE UPDATE
        // ═══════════════════════════════════════════════════════════
        update_state_machine(&sensors);
        
        // ═══════════════════════════════════════════════════════════
        // 5. PID CONTROL WITH HEATING STRATEGY
        // ═══════════════════════════════════════════════════════════
        if (get_machine_state() >= STATE_HEATING) {
            float brew_duty = 0;
            float steam_duty = 0;
            
            // Compute raw PID outputs
            float brew_demand = pid_compute(&brew_pid, 
                                            sensors.brew_ntc, 
                                            get_setpoint_brew());
            float steam_demand = pid_compute(&steam_pid,
                                             sensors.steam_ntc,
                                             get_setpoint_steam());
            
            // Apply heating strategy
            apply_heating_strategy(
                get_heating_strategy(),
                brew_demand, steam_demand,
                sensors.brew_ntc, sensors.steam_ntc,
                &brew_duty, &steam_duty
            );
            
            set_ssr_duty(SSR_BREW, brew_duty);
            set_ssr_duty(SSR_STEAM, steam_duty);
        }
        
        // ═══════════════════════════════════════════════════════════
        // 6. BREW CYCLE CONTROL
        // ═══════════════════════════════════════════════════════════
        update_brew_cycle(&sensors);
        
        // ═══════════════════════════════════════════════════════════
        // 7. UI UPDATE (LED patterns, buzzer)
        // ═══════════════════════════════════════════════════════════
        update_status_led(get_machine_state());
        update_buzzer();
        
        // ═══════════════════════════════════════════════════════════
        // 8. FEED WATCHDOG (only after successful loop)
        // ═══════════════════════════════════════════════════════════
        watchdog_update();
        
        // ═══════════════════════════════════════════════════════════
        // 9. MAINTAIN LOOP TIMING
        // ═══════════════════════════════════════════════════════════
        uint32_t elapsed_us = time_us_32() - loop_start;
        if (elapsed_us < LOOP_PERIOD_MS * 1000) {
            sleep_us(LOOP_PERIOD_MS * 1000 - elapsed_us);
        } else {
            // Loop overrun - log warning
            log_warning(WARN_LOOP_OVERRUN, elapsed_us);
        }
    }
}
```

### Timing Budget (100ms loop)

| Task | Typical | Max | Notes |
|------|---------|-----|-------|
| Safety interlocks | 50µs | 200µs | Digital GPIO reads |
| NTC ADC (×2) | 200µs | 500µs | With averaging |
| Thermocouple SPI | 100µs | 300µs | 32-bit read |
| Pressure ADC | 100µs | 200µs | With filtering |
| Level switches | 50µs | 100µs | Digital reads |
| Shared state update | 20µs | 50µs | Mutex + copy |
| State machine | 10µs | 50µs | Logic only |
| PID computation (×2) | 20µs | 50µs | Math |
| Brew cycle logic | 20µs | 100µs | State transitions |
| UI update | 10µs | 50µs | GPIO writes |
| Watchdog | 5µs | 10µs | Register write |
| **Total** | **~600µs** | **~1.6ms** | **98+ ms margin** |

---

## Core 1: Communication

Core 1 handles all UART communication asynchronously.

### Structure

```c
// Core 1 main function
void core1_main(void) {
    // Initialize communication (921600 baud for ESP32)
    uart_esp32_init(921600);
    uart_pzem_init(9600);
    
    // ═══════════════════════════════════════════════════════════════
    // BOOT SEQUENCE: Send MSG_BOOT + MSG_CONFIG
    // ═══════════════════════════════════════════════════════════════
    send_boot_packet();
    send_config_packet();  // ESP32 now has full configuration
    
    // Timing trackers
    uint32_t last_status_broadcast = 0;
    uint32_t last_pzem_poll = 0;
    
    while (true) {
        uint32_t now = to_ms_since_boot(get_absolute_time());
        
        // ═══════════════════════════════════════════════════════════
        // 1. PROCESS INCOMING ESP32 COMMANDS
        // ═══════════════════════════════════════════════════════════
        while (uart_esp32_available()) {
            uint8_t byte = uart_esp32_read();
            if (protocol_parse_byte(byte)) {
                // Complete packet received
                bool config_changed = process_command(protocol_get_packet());
                
                // If config was changed, send updated config to ESP32
                if (config_changed) {
                    send_config_packet();
                }
            }
        }
        
        // ═══════════════════════════════════════════════════════════
        // 2. POLL PZEM POWER METER (every 1000ms)
        // ═══════════════════════════════════════════════════════════
        if (now - last_pzem_poll >= 1000) {
            last_pzem_poll = now;
            pzem_request_data();
        }
        
        // Check for PZEM response
        if (pzem_response_ready()) {
            power_data_t power = pzem_get_data();
            
            // Update shared state (power data included in unified status)
            mutex_enter_blocking(&state_mutex);
            shared_state.power = power;
            mutex_exit(&state_mutex);
        }
        
        // ═══════════════════════════════════════════════════════════
        // 3. BROADCAST UNIFIED STATUS (every 500ms)
        //    Includes: temps, pressure, setpoints, outputs, power, state
        // ═══════════════════════════════════════════════════════════
        if (now - last_status_broadcast >= 500) {
            last_status_broadcast = now;
            
            // Read shared state (with mutex)
            status_snapshot_t snapshot;
            mutex_enter_blocking(&state_mutex);
            snapshot = shared_state;
            mutex_exit(&state_mutex);
            
            // Encode and send unified status packet (44 bytes @ 921600 = 0.48ms)
            send_status_packet(&snapshot);
        }
        
        // ═══════════════════════════════════════════════════════════
        // 4. SEND PENDING ALARMS (immediate, safety-critical)
        // ═══════════════════════════════════════════════════════════
        alarm_t alarm;
        while (alarm_queue_pop(&alarm)) {
            send_alarm_packet(&alarm);
        }
        
        // Small sleep to prevent busy-waiting
        sleep_ms(1);
    }
}

// Command handler returns true if config was changed
bool process_command(const packet_t* pkt) {
    bool config_changed = false;
    
    switch (pkt->type) {
        case MSG_CMD_SET_TEMP:
            config_changed = handle_set_temp(pkt);
            break;
            
        case MSG_CMD_SET_PID:
            config_changed = handle_set_pid(pkt);
            break;
            
        case MSG_CMD_CONFIG:
            config_changed = handle_set_config(pkt);
            break;
            
        case MSG_CMD_GET_CONFIG:
            // Just send current config, no change
            send_config_packet();
            break;
            
        case MSG_CMD_MODE:
            handle_mode_change(pkt);
            break;
            
            handle_output_control(pkt);
            break;
            
        case MSG_CMD_BREW:
            handle_brew_command(pkt);
            break;
            
        case MSG_CMD_BOOTLOADER:
            enter_bootloader();  // Does not return
            break;
            
        case MSG_PING:
            // Echo back
            send_ping_response(pkt);
            break;
    }
    
    return config_changed;
}
```

---

## Module Structure

```
src/pico
│
├── main.c                      # Entry point, core initialization
├── config.h                    # Global configuration, pin definitions
│
├── /machines                   # Machine-specific configurations
│   ├── machine_config.h        # Config structures
│   ├── ecm_synchronika.h       # ECM Synchronika definition
│   ├── rancilio_silvia.h       # Rancilio Silvia definition
│   └── generic_hx.h            # Generic HX definition
│
├── /safety                     # Safety-critical modules (AUDIT THESE)
│   ├── watchdog.h/c            # Watchdog timer management
│   ├── interlocks.h/c          # Water level, over-temp checks
│   └── safe_state.h/c          # Emergency shutdown
│
├── /sensors                    # Sensor drivers
│   ├── sensors.h               # Common sensor interface
│   ├── ntc.h/c                 # NTC thermistor driver
│   ├── max31855.h/c            # Thermocouple driver (SPI)
│   ├── pressure.h/c            # Pressure transducer driver
│   └── levels.h/c              # Water level sensors
│
├── /control                    # Control algorithms
│   ├── pid.h/c                 # PID controller implementation
│   ├── boiler.h/c              # Boiler control logic
│   ├── brew.h/c                # Brew cycle state machine
│   └── outputs.h/c             # SSR/relay GPIO control
│
├── /comms                      # Communication
│   ├── protocol.h/c            # Binary protocol encode/decode
│   ├── uart_esp32.h/c          # ESP32 UART handler
│   └── pzem.h/c                # PZEM-004T Modbus driver
│
├── /ui                         # User interface
│   ├── led.h/c                 # Status LED patterns
│   └── buzzer.h/c              # Buzzer tones
│
├── /state                      # State management
│   ├── machine_state.h/c       # State machine
│   ├── shared_data.h/c         # Thread-safe shared state
│   └── runtime_config.h/c      # Flash-stored configuration
│
└── /util                       # Utilities
    ├── crc.h/c                 # CRC16 calculation
    ├── ring_buffer.h/c         # UART ring buffers
    └── filter.h/c              # Moving average, low-pass
```

---

## State Machine

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           STATE MACHINE                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│                              ┌──────────┐                                    │
│                              │   BOOT   │                                    │
│                              │  (0x00)  │                                    │
│                              └────┬─────┘                                    │
│                                   │ HW initialized                           │
│                                   ▼                                          │
│                            ┌──────────────┐                                  │
│                            │  SELF_TEST   │                                  │
│                            │    (0x01)    │                                  │
│                            └──────┬───────┘                                  │
│                                   │                                          │
│              ┌────────────────────┼────────────────────┐                    │
│              │ Sensors OK         │ Sensors failed     │ Critical error    │
│              ▼                    ▼                    ▼                    │
│       ┌──────────────┐    ┌──────────────┐    ┌──────────────┐             │
│       │   HEATING    │    │  SAFE_MODE   │    │    FAULT     │◄────────┐   │
│       │    (0x02)    │    │    (0x06)    │    │    (0x07)    │         │   │
│       └──────┬───────┘    └──────────────┘    └──────────────┘         │   │
│              │                                       ▲                  │   │
│              │ At temperature                        │                  │   │
│              ▼                                       │ Critical error   │   │
│       ┌──────────────┐                               │ from any state   │   │
│       │    READY     │───────────────────────────────┤                  │   │
│       │    (0x03)    │                               │                  │   │
│       └──────┬───────┘                               │                  │   │
│              │                                       │                  │   │
│              │ Brew switch ON                        │                  │   │
│              ▼                                       │                  │   │
│       ┌──────────────┐                               │                  │   │
│       │   BREWING    │───────────────────────────────┘                  │   │
│       │    (0x04)    │                                                  │   │
│       └──────┬───────┘                                                  │   │
│              │                                                          │   │
│              │ Brew switch OFF                                          │   │
│              ▼                                                          │   │
│       ┌──────────────┐                                                  │   │
│       │  POST_BREW   │──────────────────────────────────────────────────┘   │
│       │    (0x05)    │                                                      │
│       └──────┬───────┘                                                      │
│              │                                                              │
│              │ 2s delay complete                                            │
│              ▼                                                              │
│            READY ◄──────────────────────────────────────────────────────    │
│                                                                              │
│   ┌──────────────────────────────────────────────────────────────────────┐  │
│   │ STANDBY (0x08): Low-power mode, heaters off, monitoring only        │  │
│   │ SERVICE (0x09): Manual control mode, ESP32 has direct output access │  │
│   └──────────────────────────────────────────────────────────────────────┘  │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### State Transitions

```c
typedef enum {
    STATE_BOOT       = 0x00,
    STATE_SELF_TEST  = 0x01,
    STATE_HEATING    = 0x02,
    STATE_READY      = 0x03,
    STATE_BREWING    = 0x04,
    STATE_POST_BREW  = 0x05,
    STATE_SAFE_MODE  = 0x06,
    STATE_FAULT      = 0x07,
    STATE_STANDBY    = 0x08,
    STATE_SERVICE    = 0x09,
} machine_state_t;

// Allowed transitions (from, to)
static const bool valid_transitions[10][10] = {
    //                BOOT SELF HEAT REDY BREW POST SAFE FALT STBY SRVC
    /* BOOT      */ { 0,   1,   0,   0,   0,   0,   0,   1,   0,   0 },
    /* SELF_TEST */ { 0,   0,   1,   0,   0,   0,   1,   1,   0,   0 },
    /* HEATING   */ { 0,   0,   0,   1,   0,   0,   0,   1,   1,   0 },
    /* READY     */ { 0,   0,   1,   0,   1,   0,   0,   1,   1,   1 },
    /* BREWING   */ { 0,   0,   0,   0,   0,   1,   0,   1,   0,   0 },
    /* POST_BREW */ { 0,   0,   0,   1,   0,   0,   0,   1,   0,   0 },
    /* SAFE_MODE */ { 0,   0,   0,   0,   0,   0,   0,   1,   0,   0 },
    /* FAULT     */ { 1,   0,   0,   0,   0,   0,   0,   0,   0,   0 },  // Only reset exits
    /* STANDBY   */ { 0,   0,   1,   0,   0,   0,   0,   1,   0,   0 },
    /* SERVICE   */ { 0,   0,   0,   1,   0,   0,   0,   1,   0,   0 },
};
```

---

## Shared State

Thread-safe shared data between cores.

```c
// shared_data.h

typedef struct {
    // Sensor readings (updated by Core 0)
    struct {
        float brew_ntc;         // °C
        float steam_ntc;        // °C
        float group_tc;         // °C
        float pressure;         // bar
        uint8_t levels;         // Bitmask
        bool brew_switch;
    } sensors;
    
    // Output states (updated by Core 0)
    struct {
        uint8_t ssr_brew_duty;  // 0-100%
        uint8_t ssr_steam_duty; // 0-100%
        uint8_t relay_state;    // Bitmask
    } outputs;
    
    // Setpoints (updated by Core 1 via commands)
    struct {
        float brew;             // °C
        float steam;            // °C
    } setpoints;
    
    // PID gains (updated by Core 1 via commands)
    struct {
        float brew_kp, brew_ki, brew_kd;
        float steam_kp, steam_ki, steam_kd;
    } pid;
    
    // Machine state
    machine_state_t state;
    uint32_t uptime_ms;
    bool brewing;
    uint16_t brew_time_ms;
    
    // Alarms
    uint8_t error_flags;
    
    // Power data (updated by Core 1)
    power_data_t power;
    
} shared_state_t;

// Global instance
extern shared_state_t shared_state;
extern mutex_t state_mutex;

// Thread-safe accessors
float get_brew_temp(void);
float get_steam_temp(void);
void set_setpoint_brew(float temp);
void set_setpoint_steam(float temp);
machine_state_t get_machine_state(void);
void set_machine_state(machine_state_t state);
```

---

## PID Controller

```c
// pid.h

typedef struct {
    // Gains
    float kp;
    float ki;
    float kd;
    
    // State
    float integral;
    float prev_error;
    float prev_derivative;  // For filtering
    
    // Limits
    float output_min;
    float output_max;
    float integral_min;
    float integral_max;
    
    // Configuration
    float dt;               // Sample time (seconds)
    float derivative_alpha; // Low-pass filter coefficient (0-1)
    
} pid_controller_t;

// Initialize PID controller
void pid_init(pid_controller_t* pid, float kp, float ki, float kd, float dt);

// Compute PID output
float pid_compute(pid_controller_t* pid, float measurement, float setpoint);

// Reset integral and derivative state
void pid_reset(pid_controller_t* pid);

// Update gains (thread-safe, for runtime tuning)
void pid_set_gains(pid_controller_t* pid, float kp, float ki, float kd);
```

### Implementation

```c
// pid.c

float pid_compute(pid_controller_t* pid, float measurement, float setpoint) {
    float error = setpoint - measurement;
    
    // Proportional term
    float p_term = pid->kp * error;
    
    // Integral term with anti-windup
    pid->integral += pid->ki * error * pid->dt;
    
    // Clamp integral
    if (pid->integral > pid->integral_max) {
        pid->integral = pid->integral_max;
    } else if (pid->integral < pid->integral_min) {
        pid->integral = pid->integral_min;
    }
    
    float i_term = pid->integral;
    
    // Derivative term with low-pass filter
    float derivative = (error - pid->prev_error) / pid->dt;
    float filtered_derivative = pid->derivative_alpha * derivative 
                              + (1.0f - pid->derivative_alpha) * pid->prev_derivative;
    pid->prev_derivative = filtered_derivative;
    pid->prev_error = error;
    
    float d_term = pid->kd * filtered_derivative;
    
    // Sum and clamp output
    float output = p_term + i_term + d_term;
    
    if (output > pid->output_max) {
        output = pid->output_max;
        // Anti-windup: prevent integral from growing further
        if (error > 0) pid->integral -= pid->ki * error * pid->dt;
    } else if (output < pid->output_min) {
        output = pid->output_min;
        if (error < 0) pid->integral -= pid->ki * error * pid->dt;
    }
    
    return output;
}
```

---

## Heating Strategy Implementation

The heating strategy controls how both boilers are heated to manage power consumption and startup behavior.

```c
// heating.c

void apply_heating_strategy(
    heating_strategy_t strategy,
    float brew_demand,      // PID output for brew (0-1)
    float steam_demand,     // PID output for steam (0-1)
    float brew_temp,        // Current brew temperature
    float steam_temp,       // Current steam temperature
    float* brew_duty,       // Output: actual brew duty
    float* steam_duty       // Output: actual steam duty
) {
    const heating_config_t* cfg = get_heating_config();
    float brew_setpoint = get_setpoint_brew();
    float steam_setpoint = get_setpoint_steam();
    
    switch (strategy) {
        
        case HEAT_BREW_ONLY:
            // Only brew boiler heats, steam stays off
            *brew_duty = brew_demand;
            *steam_duty = 0;
            break;
            
        case HEAT_SEQUENTIAL:
            // Brew first, steam starts after brew reaches threshold
            *brew_duty = brew_demand;
            
            // Calculate % of setpoint reached
            float brew_pct = (brew_temp / brew_setpoint) * 100.0f;
            
            if (brew_pct >= cfg->sequential_threshold_pct) {
                *steam_duty = steam_demand;
            } else {
                *steam_duty = 0;
            }
            break;
            
        case HEAT_PARALLEL:
            // Both heat simultaneously (fastest, highest power)
            *brew_duty = brew_demand;
            *steam_duty = steam_demand;
            break;
            
        case HEAT_STEAM_PRIORITY:
            // Steam first, brew starts after steam reaches threshold
            *steam_duty = steam_demand;
            
            float steam_pct = (steam_temp / steam_setpoint) * 100.0f;
            
            if (steam_pct >= cfg->sequential_threshold_pct) {
                *brew_duty = brew_demand;
            } else {
                *brew_duty = 0;
            }
            break;
            
        case HEAT_SMART_STAGGER:
            // Both heat, but combined duty is limited
            {
                float max_combined = cfg->max_combined_duty / 100.0f;  // e.g., 1.5
                float total_demand = brew_demand + steam_demand;
                
                if (total_demand <= max_combined) {
                    // Both can run at requested duty
                    *brew_duty = brew_demand;
                    *steam_duty = steam_demand;
                } else {
                    // Must scale back - prioritize based on config
                    float scale = max_combined / total_demand;
                    
                    if (cfg->stagger_priority == 0) {
                        // Brew priority: give brew what it needs, steam gets remainder
                        *brew_duty = fminf(brew_demand, max_combined);
                        *steam_duty = fminf(steam_demand, max_combined - *brew_duty);
                    } else {
                        // Steam priority
                        *steam_duty = fminf(steam_demand, max_combined);
                        *brew_duty = fminf(brew_demand, max_combined - *steam_duty);
                    }
                }
            }
            break;
            
        default:
            // Safe fallback: brew only
            *brew_duty = brew_demand;
            *steam_duty = 0;
            break;
    }
    
    // Apply global max duty limit
    float max_duty = machine->safety.max_ssr_duty / 100.0f;
    *brew_duty = fminf(*brew_duty, max_duty);
    *steam_duty = fminf(*steam_duty, max_duty);
}
```

### Strategy Selection at Runtime

```c
// Commands from ESP32 can change heating strategy
void cmd_set_heating_strategy(heating_strategy_t strategy, heating_config_t* params) {
    // Validate strategy
    if (strategy > HEAT_SMART_STAGGER) {
        send_ack(MSG_CMD_MODE, ACK_INVALID);
        return;
    }
    
    // Update runtime config
    mutex_enter_blocking(&config_mutex);
    runtime_config.heating.strategy = strategy;
    if (params) {
        runtime_config.heating.sequential_threshold_pct = params->sequential_threshold_pct;
        runtime_config.heating.max_combined_duty = params->max_combined_duty;
        runtime_config.heating.stagger_priority = params->stagger_priority;
    }
    runtime_config.flags |= CONFIG_FLAG_USE_RUNTIME_HEATING;
    mutex_exit(&config_mutex);
    
    // Save to flash
    save_runtime_config();
    
    send_ack(MSG_CMD_MODE, ACK_SUCCESS);
}
```

---

## SSR PWM Output

### How SSR Power Control Works

SSRs (Solid State Relays) switch AC power, but we control **average power** using PWM (Pulse Width Modulation). The SSR is rapidly turned ON and OFF to achieve a target power level.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        SSR PWM POWER CONTROL                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   100% Duty Cycle (full power - 1400W, ~6A):                                │
│   ──────────────────────────────────────────                                │
│   SSR:  ████████████████████████████████████████  (always ON)               │
│   Time: |←───────────── 1 second ──────────────→|                           │
│                                                                              │
│   50% Duty Cycle (half power - 700W, ~3A):                                  │
│   ─────────────────────────────────────────────                             │
│   SSR:  ████████████████████░░░░░░░░░░░░░░░░░░░░  (ON 500ms, OFF 500ms)     │
│   Time: |←───────────── 1 second ──────────────→|                           │
│                                                                              │
│   25% Duty Cycle (quarter power - 350W, ~1.5A):                             │
│   ─────────────────────────────────────────────                             │
│   SSR:  ██████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  (ON 250ms, OFF 750ms)     │
│   Time: |←───────────── 1 second ──────────────→|                           │
│                                                                              │
│   The heater element averages out the pulses due to thermal mass.           │
│   Result: Smooth, controllable heating.                                     │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Why 1-Second PWM Period?

Our SSRs are **zero-cross type** - they only switch ON/OFF when AC voltage crosses zero. This:
- Reduces electrical noise (EMI)
- Prevents contact arcing
- Requires slower PWM (can't switch mid-cycle)

At 50Hz mains, zero crossings occur every 10ms. A 1-second period gives us:
- 100 zero crossings per period
- 1% resolution (10ms steps)
- Smooth power control

### Power vs Duty Cycle

| Duty Cycle | Power (1400W heater) | Current | 
|------------|---------------------|---------|
| 100% | 1400W | ~6A |
| 75% | 1050W | ~4.5A |
| 50% | 700W | ~3A |
| 25% | 350W | ~1.5A |
| 0% | 0W | 0A |

### Combined Power (Both Heaters)

This is how **heating strategies** control total power draw:

| Brew Duty | Steam Duty | Combined | Total Power | Total Current |
|-----------|------------|----------|-------------|---------------|
| 100% | 100% | 200% | 2800W | ~12A |
| 100% | 50% | 150% | 2100W | ~9A |
| 75% | 75% | 150% | 2100W | ~9A |
| 100% | 0% | 100% | 1400W | ~6A |
| 50% | 50% | 100% | 1400W | ~6A |

**HEAT_SMART_STAGGER** limits combined duty to stay under a power budget (e.g., 150% = ~9A max).

### Implementation

```c
// outputs.c

#define SSR_PWM_PERIOD_MS 1000  // 1 second period
#define SSR_PWM_RESOLUTION 100  // 1% steps = 10ms minimum on-time

typedef struct {
    uint8_t gpio;
    uint8_t duty;           // 0-100%
    uint32_t period_start;
} ssr_state_t;

static ssr_state_t ssr_brew;
static ssr_state_t ssr_steam;

void ssr_update(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    // Update brew SSR
    uint32_t elapsed = now - ssr_brew.period_start;
    if (elapsed >= SSR_PWM_PERIOD_MS) {
        ssr_brew.period_start = now;
        elapsed = 0;
    }
    
    uint32_t on_time = (SSR_PWM_PERIOD_MS * ssr_brew.duty) / 100;
    gpio_put(ssr_brew.gpio, elapsed < on_time);
    
    // Update steam SSR
    elapsed = now - ssr_steam.period_start;
    if (elapsed >= SSR_PWM_PERIOD_MS) {
        ssr_steam.period_start = now;
        elapsed = 0;
    }
    
    on_time = (SSR_PWM_PERIOD_MS * ssr_steam.duty) / 100;
    gpio_put(ssr_steam.gpio, elapsed < on_time);
}

void set_ssr_duty(uint8_t ssr, uint8_t duty) {
    if (duty > 95) duty = 95;  // Safety limit (never 100% continuous)
    
    if (ssr == SSR_BREW) {
        ssr_brew.duty = duty;
    } else if (ssr == SSR_STEAM) {
        ssr_steam.duty = duty;
    }
}
```

---

## Initialization Sequence

```c
// main.c

int main(void) {
    // ═══════════════════════════════════════════════════════════════
    // 1. CRITICAL: Set all outputs LOW before anything else
    // ═══════════════════════════════════════════════════════════════
    init_outputs_safe();
    
    // ═══════════════════════════════════════════════════════════════
    // 2. Initialize stdio (USB/UART debug)
    // ═══════════════════════════════════════════════════════════════
    stdio_init_all();
    
    // ═══════════════════════════════════════════════════════════════
    // 3. Enable watchdog (2 second timeout)
    // ═══════════════════════════════════════════════════════════════
    watchdog_enable(2000, true);
    
    // ═══════════════════════════════════════════════════════════════
    // 4. Initialize hardware peripherals
    // ═══════════════════════════════════════════════════════════════
    init_adc();
    init_spi();
    init_gpio_inputs();
    init_gpio_outputs();
    
    // ═══════════════════════════════════════════════════════════════
    // 5. Initialize shared state
    // ═══════════════════════════════════════════════════════════════
    mutex_init(&state_mutex);
    init_shared_state();
    
    // ═══════════════════════════════════════════════════════════════
    // 6. Load runtime configuration from flash
    // ═══════════════════════════════════════════════════════════════
    load_runtime_config();
    
    // ═══════════════════════════════════════════════════════════════
    // 7. Initialize control modules
    // ═══════════════════════════════════════════════════════════════
    pid_init(&brew_pid, config.brew_kp, config.brew_ki, config.brew_kd, 0.1f);
    pid_init(&steam_pid, config.steam_kp, config.steam_ki, config.steam_kd, 0.1f);
    
    // ═══════════════════════════════════════════════════════════════
    // 8. Run self-test
    // ═══════════════════════════════════════════════════════════════
    set_machine_state(STATE_SELF_TEST);
    watchdog_update();
    
    if (run_self_test()) {
        set_machine_state(STATE_HEATING);
    } else {
        set_machine_state(STATE_SAFE_MODE);
    }
    
    // ═══════════════════════════════════════════════════════════════
    // 9. Launch Core 1 (communication)
    // ═══════════════════════════════════════════════════════════════
    multicore_launch_core1(core1_main);
    
    // ═══════════════════════════════════════════════════════════════
    // 10. Enter Core 0 main loop (control)
    // ═══════════════════════════════════════════════════════════════
    core0_main();  // Never returns
    
    return 0;  // Never reached
}
```

---

## Memory Map

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              FLASH (2MB)                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│  0x10000000  ┌────────────────────────────────────────────┐                 │
│              │              BOOT2 (256 bytes)             │                 │
│  0x10000100  ├────────────────────────────────────────────┤                 │
│              │                                            │                 │
│              │              APPLICATION CODE              │                 │
│              │              (~100-200 KB)                 │                 │
│              │                                            │                 │
│  0x101F0000  ├────────────────────────────────────────────┤                 │
│              │         RUNTIME CONFIG (4KB)               │                 │
│              │         (PID gains, setpoints)             │                 │
│  0x101F1000  ├────────────────────────────────────────────┤                 │
│              │              RESERVED                       │                 │
│  0x10200000  └────────────────────────────────────────────┘                 │
│                                                                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                              SRAM (264KB)                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│  0x20000000  ┌────────────────────────────────────────────┐                 │
│              │              STACK (Core 0)                │                 │
│              │              (~8 KB)                       │                 │
│              ├────────────────────────────────────────────┤                 │
│              │              STACK (Core 1)                │                 │
│              │              (~8 KB)                       │                 │
│              ├────────────────────────────────────────────┤                 │
│              │              HEAP                          │                 │
│              │              (minimal use)                 │                 │
│              ├────────────────────────────────────────────┤                 │
│              │              .bss (globals)                │                 │
│              │              shared_state, pid, etc.       │                 │
│              ├────────────────────────────────────────────┤                 │
│              │              .data (initialized)           │                 │
│              │              machine_config, etc.          │                 │
│  0x20042000  └────────────────────────────────────────────┘                 │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Build Configuration

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.13)

# Initialize Pico SDK
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(ecm_firmware C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

# Machine type selection
set(MACHINE_TYPE "ECM_SYNCHRONIKA" CACHE STRING "Target machine")
add_compile_definitions(MACHINE_${MACHINE_TYPE})

# Source files
add_executable(ecm_firmware
    main.c
    safety/watchdog.c
    safety/interlocks.c
    safety/safe_state.c
    sensors/ntc.c
    sensors/max31855.c
    sensors/pressure.c
    sensors/levels.c
    control/pid.c
    control/boiler.c
    control/brew.c
    control/outputs.c
    comms/protocol.c
    comms/uart_esp32.c
    comms/pzem.c
    ui/led.c
    ui/buzzer.c
    state/machine_state.c
    state/shared_data.c
    state/runtime_config.c
    util/crc.c
    util/filter.c
)

# Link libraries
target_link_libraries(ecm_firmware
    pico_stdlib
    pico_multicore
    hardware_adc
    hardware_spi
    hardware_uart
    hardware_pwm
    hardware_watchdog
    hardware_flash
    hardware_sync
)

# Enable USB and UART output
pico_enable_stdio_usb(ecm_firmware 1)
pico_enable_stdio_uart(ecm_firmware 0)

# Generate UF2 file
pico_add_extra_outputs(ecm_firmware)
```

