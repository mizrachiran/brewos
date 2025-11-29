# Error Handling & Exception Management

This document describes the firmware's error handling architecture, including safety mechanisms, error tracking, and recovery procedures.

**See also:** [Feature Status Table](Feature_Status_Table.md) for implementation status.

---

## Overview

The firmware uses a **multi-layered error handling approach** with safety as the highest priority. Since C doesn't have exceptions, errors are handled through return codes, state machines, and the safety system.

---

## Error Handling Architecture

### 1. Safety System (Primary Error Handler)

The **safety system** is the primary error handling mechanism that runs **first** in every control cycle (10Hz).

```
Main Loop (10Hz)
  ↓
Safety Check (safety_check())
  ├─→ SAFETY_CRITICAL → Enter Safe State (all outputs OFF)
  ├─→ SAFETY_FAULT → Send Alarm, may continue with limits
  └─→ SAFETY_WARNING → Log warning, continue operation
```

**Location:** `src/pico/src/safety.c`

**Error Severity Levels:**

| Level | Action | Recovery |
|-------|--------|----------|
| **SAFETY_CRITICAL** | Enter safe state immediately, all outputs OFF | Manual reset required |
| **SAFETY_FAULT** | Send alarm, may continue with limits | Automatic recovery when condition clears |
| **SAFETY_WARNING** | Log warning, continue operation | Automatic recovery |

---

## Error Categories & Handling

### 1. Sensor Errors

#### Temperature Sensor Failures (NTC)

**Detection:**
- Open circuit: Temperature > 150°C (SAF-023)
- Short circuit: Temperature < -20°C (SAF-024)
- Invalid readings: `isnan()` or `isinf()` checks
- Out of range: Temperature validation (-10°C to 200°C)

**Error Tracking:**
- Consecutive failure counters for each sensor:
  - `g_brew_ntc_error_count`
  - `g_steam_ntc_error_count`
  - `g_group_tc_error_count`
- Error threshold: 10 consecutive failures before reporting
- Recovery detection: Logs when sensors recover

**Handling:**
```c
// In sensors.c
float brew_temp_raw = read_brew_ntc();
if (!isnan(brew_temp_raw)) {
    // Valid reading - reset error count
    if (g_brew_ntc_error_count > 0) {
        DEBUG_PRINT("SENSOR: Brew NTC recovered after %d failures\n", g_brew_ntc_error_count);
    }
    g_brew_ntc_error_count = 0;
    g_brew_ntc_fault = false;
    // Use it
    g_sensor_data.brew_temp = (int16_t)(brew_temp_filtered * 10.0f);
} else {
    // Invalid - increment error count
    g_brew_ntc_error_count++;
    if (g_brew_ntc_error_count >= SENSOR_ERROR_THRESHOLD && 
        g_brew_ntc_error_count == SENSOR_ERROR_THRESHOLD) {
        DEBUG_PRINT("SENSOR ERROR: Brew NTC invalid reading - %d consecutive failures\n", 
                   g_brew_ntc_error_count);
    }
    // Use last valid value (filter maintains it)
    // Safety system will detect fault
}
```

**Safety Response:**
- Sets `SAFETY_FLAG_SENSOR_FAULT`
- Returns `SAFETY_CRITICAL`
- Enters safe state (heaters OFF)
- Sends `ALARM_SENSOR_FAULT` to ESP32

**Recovery:**
- Automatic when sensor reading becomes valid again
- Safety system clears flag
- Error count resets to zero
- Machine can resume operation

#### MAX31855 Thermocouple Errors

**Detection:**
- Fault bits in data register (bits 0-2)
- Open circuit, short circuit, or other faults
- Conversion failures
- Out of range: Temperature validation (-50°C to 200°C)

**Error Tracking:**
- `g_group_tc_error_count` - Tracks consecutive failures
- Error threshold: 10 consecutive failures before reporting
- Fault code tracking: Logs specific MAX31855 fault codes

**Handling:**
```c
// In sensors.c
if (hw_max31855_is_fault(max31855_data)) {
    g_group_tc_fault = true;
    g_group_tc_error_count++;
    uint8_t fault_code = hw_max31855_get_fault(max31855_data);
    if (g_group_tc_error_count >= SENSOR_ERROR_THRESHOLD && 
        g_group_tc_error_count == SENSOR_ERROR_THRESHOLD) {
        DEBUG_PRINT("SENSOR ERROR: MAX31855 fault code=%d - %d consecutive failures\n", 
                   fault_code, g_group_tc_error_count);
    }
    return NAN;  // Invalid reading
}

// Conversion failure
if (!hw_max31855_to_temp(max31855_data, &temp_c)) {
    g_group_tc_error_count++;
    if (g_group_tc_error_count >= SENSOR_ERROR_THRESHOLD && 
        g_group_tc_error_count == SENSOR_ERROR_THRESHOLD) {
        DEBUG_PRINT("SENSOR ERROR: MAX31855 conversion failed - %d consecutive failures\n", 
                   g_group_tc_error_count);
    }
    return NAN;
}
```

**Safety Response:**
- Warning level (not critical - group temp is secondary)
- Machine continues operation
- Uses NTC temperatures only

**Recovery:**
- Automatic when thermocouple recovers
- Error count resets to zero on valid reading

#### Pressure Sensor Errors

**Detection:**
- Voltage out of range (ADC: 0.2V to 3.0V, Transducer: 0.3V to 4.7V)
- Invalid pressure calculations

**Error Tracking:**
- `g_pressure_error_count` - Tracks consecutive failures
- Error threshold: 10 consecutive failures before reporting
- Fault state: `g_pressure_sensor_fault` prevents filter updates during faults

**Handling:**
```c
// In sensors.c
// Validate voltage range
if (voltage < 0.2f || voltage > 3.0f) {
    g_pressure_sensor_fault = true;
    g_pressure_error_count++;
    if (g_pressure_error_count >= SENSOR_ERROR_THRESHOLD && 
        g_pressure_error_count == SENSOR_ERROR_THRESHOLD) {
        DEBUG_PRINT("SENSOR ERROR: Pressure sensor voltage out of range (%.2fV) - %d consecutive failures\n", 
                   voltage, g_pressure_error_count);
    }
    return 0.0f;  // Return 0 bar on error
}

// Validate transducer voltage
if (v_transducer < 0.3f || v_transducer > 4.7f) {
    g_pressure_sensor_fault = true;
    g_pressure_error_count++;
    // ... error reporting ...
    return 0.0f;
}
```

**Safety Response:**
- **Non-critical** - Pressure sensor is for monitoring only
- Machine continues operation normally
- Uses last valid value (filter maintains it)

**Recovery:**
- Automatic when sensor reading becomes valid again
- Error count resets to zero on valid reading

#### PZEM Power Meter Errors

**Detection:**
- UART timeout (100ms)
- CRC mismatch
- Invalid response format
- Parse errors

**Handling:**
```c
// In pzem.c
if (length < 0) {
    g_pzem_error_count++;
    DEBUG_PRINT("PZEM: Read timeout\n");
    return false;  // Read failed
}

// In sensors.c - error reporting
if (!pzem_read(&pzem_data)) {
    g_pzem_error_count = pzem_get_error_count();
    if (g_pzem_error_count > 0 && g_pzem_error_count % 50 == 0) {
        // Report every 50 errors to avoid spam
        DEBUG_PRINT("SENSOR ERROR: PZEM read failures: %d total\n", g_pzem_error_count);
    }
}
```

**Error Tracking:**
- `pzem_get_error_count()` - Returns total PZEM error count
- Errors tracked internally in `pzem.c`
- Periodic reporting (every 50 errors) to avoid log spam

**Safety Response:**
- **Non-critical** - Power meter is optional
- Falls back to theoretical power estimation
- Machine continues operation normally
- Error count tracked for diagnostics

**Recovery:**
- Automatic retry on next sensor cycle
- No impact on machine operation

---

### 2. Communication Errors

#### ESP32 UART Communication

**Error Types:**
- CRC mismatch
- Invalid packet format
- Invalid packet length
- Buffer overflow
- Timeout (no heartbeat)

**Handling:**
```c
// In protocol.c
if (received_crc != expected_crc) {
    g_crc_errors++;
    DEBUG_PRINT("CRC error: got=0x%04X exp=0x%04X (total errors: %lu)\n", 
               received_crc, expected_crc, g_crc_errors);
    // Packet discarded, state machine resets
    g_rx_state = RX_WAIT_SYNC;
}

// Packet length validation
if (packet.length > PROTOCOL_MAX_PAYLOAD) {
    g_packet_errors++;
    DEBUG_PRINT("Protocol: Invalid packet length %d (max %d)\n", 
               packet.length, PROTOCOL_MAX_PAYLOAD);
    g_rx_state = RX_WAIT_SYNC;
    break;
}

// Buffer overflow protection
if (g_rx_index >= sizeof(g_rx_buffer)) {
    g_packet_errors++;
    DEBUG_PRINT("Protocol: Buffer overflow, resetting state (total errors: %lu)\n", 
               g_packet_errors);
    g_rx_state = RX_WAIT_SYNC;
    g_rx_index = 0;
    g_rx_length = 0;
}
```

**Error Tracking Functions:**
- `protocol_get_crc_errors()` - Returns total CRC error count
- `protocol_get_packet_errors()` - Returns total packet error count
- `protocol_reset_error_counters()` - Clears error counters

**Safety Response:**
- **Heartbeat Timeout (>5s):**
  - Sets `SAFETY_FLAG_COMM_TIMEOUT`
  - Returns `SAFETY_WARNING`
  - Machine continues standalone operation
  - ESP32 can reconnect later

- **CRC/Packet Errors:**
  - Packet discarded
  - No safety action (non-critical)
  - State machine resets, waits for next packet

**Recovery:**
- Automatic when communication resumes
- No manual intervention needed

#### PZEM Modbus Communication

**Error Types:**
- Timeout (no response)
- CRC mismatch
- Invalid register data

**Handling:**
- Error count incremented
- Falls back to power estimation
- No safety impact

---

### 3. Control System Errors

#### PID Parameter Validation

**Detection:**
- Invalid PID parameters (negative values, excessive values)
- Invalid target (not 0 or 1)
- Invalid output values (>100%)

**Handling:**
```c
// In control.c
void control_set_pid(uint8_t target, float kp, float ki, float kd) {
    if (target > 1) {
        DEBUG_PRINT("CONTROL ERROR: Invalid PID target %d (must be 0 or 1)\n", target);
        return;
    }
    
    // Validate PID parameters
    if (kp < 0.0f || ki < 0.0f || kd < 0.0f) {
        DEBUG_PRINT("CONTROL ERROR: Invalid PID parameters (kp=%.2f, ki=%.2f, kd=%.2f) - must be >= 0\n", 
                   kp, ki, kd);
        return;
    }
    
    // Check for reasonable limits
    if (kp > 100.0f || ki > 100.0f || kd > 100.0f) {
        DEBUG_PRINT("CONTROL ERROR: PID parameters too large (max 100.0)\n");
        return;
    }
    // ... set PID values ...
}

void control_set_output(uint8_t output, uint8_t value, uint8_t mode) {
    // Validate output value (0-100%)
    if (value > 100) {
        DEBUG_PRINT("CONTROL ERROR: Output value %d exceeds maximum (100%%)\n", value);
        value = 100;  // Clamp to maximum
    }
    // ... set output ...
}
```

**Safety Response:**
- Invalid parameters are rejected (function returns early)
- Output values are clamped to valid range
- Detailed error logging for debugging

**Recovery:**
- User must provide valid parameters
- No automatic recovery needed

#### Water Management Errors

**Detection:**
- Fill timeout (30 seconds)
- Level probe stuck (no change after 10 seconds of filling)

**Handling:**
```c
// In water_management.c
// Check timeout (safety)
if (now - g_fill_start_time > STEAM_FILL_TIMEOUT_MS) {
    DEBUG_PRINT("Water: Steam fill timeout! Stopping fill cycle\n");
    // Report error via protocol
    protocol_send_alarm(ALARM_WATER_LOW, 1, 0);  // Severity 1 = error
    // Stop filling
    control_set_pump(0);
    set_steam_fill_solenoid(false);
    g_fill_state = STEAM_FILL_IDLE;
    return;
}

// Level probe monitoring
if (steam_level_low == last_level_state && 
    (now - g_fill_start_time) > 10000) {  // After 10 seconds
    DEBUG_PRINT("Water: WARNING - Steam level probe not changing after 10s fill\n");
}
```

**Safety Response:**
- Fill timeout triggers alarm (`ALARM_WATER_LOW`)
- Fill cycle stops immediately
- Heater restored if it was on

**Recovery:**
- Automatic retry on next fill cycle
- User intervention may be needed if probe is stuck

---

### 4. Hardware Errors

#### Over-Temperature Protection

**Detection:**
- Brew boiler > 130°C (SAF-020)
- Steam boiler > 165°C (SAF-021)
- Group head > 110°C (SAF-022)

**Handling:**
```c
// In safety.c
if (brew_temp_c > SAFETY_BREW_MAX_TEMP_C) {
    g_safety_flags |= SAFETY_FLAG_OVER_TEMP;
    g_brew_over_temp = true;
    result = SAFETY_CRITICAL;
    // Enters safe state
}
```

**Safety Response:**
- **CRITICAL** - Immediate safe state
- All heaters OFF
- Sends `ALARM_OVER_TEMP` to ESP32
- LED blinks, buzzer beeps

**Recovery:**
- Hysteresis: Must cool 10°C below max before re-enable
- Automatic recovery when temperature drops
- Manual reset may be required

#### Water Level Errors

**Detection:**
- Reservoir empty (water tank mode)
- Tank level low
- Steam boiler level low

**Handling:**
```c
// In safety.c
if (is_water_tank_mode() && !is_reservoir_present()) {
    g_safety_flags |= SAFETY_FLAG_WATER_LOW;
    result = SAFETY_CRITICAL;
    // Enters safe state
}
```

**Safety Response:**
- **CRITICAL** - Safe state
- Heaters and pump OFF
- Sends `ALARM_WATER_LOW` to ESP32

**Recovery:**
- Automatic when water level restored
- Debouncing prevents false triggers

#### SSR (Heater) Errors

**Detection:**
- SSR stuck ON (60s without temperature change)
- Duty cycle > 95%

**Handling:**
```c
// In safety.c
if (now - g_brew_ssr_on_time > SAFETY_SSR_MAX_ON_TIME_MS) {
    g_safety_flags |= SAFETY_FLAG_OVER_TEMP;
    result = SAFETY_FAULT;
    // SSR may be stuck
}
```

**Safety Response:**
- **FAULT** - May continue with limits
- Clamps duty cycle to 95% max
- Sends alarm

**Recovery:**
- Automatic when condition clears

---

### 4. Configuration Errors

#### Invalid Environmental Config

**Detection:**
- Voltage = 0 or max_current_draw = 0

**Handling:**
```c
// In safety.c
if (!config_persistence_is_env_valid()) {
    g_safety_flags |= SAFETY_FLAG_ENV_CONFIG_INVALID;
    result = SAFETY_CRITICAL;
}
```

**Safety Response:**
- **CRITICAL** - Machine disabled
- Safe state until configured
- Setup mode

**Recovery:**
- User must configure via ESP32
- Machine enables after valid config

---

## Safe State Mechanism

### What Happens in Safe State

1. **All Outputs OFF:**
   - Heaters (SSR1, SSR2) = 0%
   - Pump = OFF
   - Solenoids = OFF
   - LED = Blinking (2Hz)
   - Buzzer = 3 beeps

2. **State Machine:**
   - Transitions to `STATE_SAFE`
   - Control loop continues (for recovery)
   - PID disabled

3. **Communication:**
   - Status updates continue
   - Alarm sent to ESP32
   - Flags set in status payload

### Entering Safe State

```c
// In main.c (every control cycle)
safety_state_t safety = safety_check();

if (safety == SAFETY_CRITICAL) {
    safety_enter_safe_state();
    protocol_send_alarm(safety_get_last_alarm(), 2, 0);  // Severity 2 = critical
}
```

### Exiting Safe State

```c
// In safety.c
bool safety_reset(void) {
    if (g_safety_flags == 0) {
        g_safe_state = false;
        return true;  // Can exit safe state
    }
    return false;  // Conditions not cleared
}
```

**Recovery Conditions:**
- All safety flags cleared
- Temperature within limits (with hysteresis)
- Water levels OK
- Sensors valid
- Environmental config valid

---

## Watchdog Timer

### Purpose
Detects if the control loop stops running (infinite loop, crash, etc.)

### Implementation
```c
// In main.c
watchdog_enable(SAFETY_WATCHDOG_TIMEOUT_MS, true);  // 2000ms timeout

// Every control cycle (after safety checks)
safety_kick_watchdog();  // Reset watchdog timer
```

### What Happens on Timeout
- **Hardware reset** (automatic)
- System reboots
- All outputs OFF (hardware default)
- Boot sequence runs again

### Detection
```c
if (watchdog_caused_reboot()) {
    DEBUG_PRINT("WARNING: Watchdog reset!\n");
    // Log the reset reason
}
```

---

### 5. Bootloader Errors

#### OTA Firmware Update Errors

**Error Types:**
- Timeout (30 seconds total, 5 seconds per chunk)
- Invalid magic bytes
- Invalid chunk size or number
- Checksum mismatch
- Flash erase/write failures
- Flash overflow (firmware too large)

**Error Codes:**
- `BOOTLOADER_ERROR_TIMEOUT` (1)
- `BOOTLOADER_ERROR_INVALID_MAGIC` (2)
- `BOOTLOADER_ERROR_INVALID_SIZE` (3)
- `BOOTLOADER_ERROR_INVALID_CHUNK` (4)
- `BOOTLOADER_ERROR_CHECKSUM` (5)
- `BOOTLOADER_ERROR_FLASH_WRITE` (6)
- `BOOTLOADER_ERROR_FLASH_ERASE` (7)
- `BOOTLOADER_ERROR_UNKNOWN` (8)

**Error Reporting:**
```c
// In bootloader.c
// All errors send error code to ESP32 before returning
uart_write_byte(0xFF);  // Error marker
uart_write_byte(BOOTLOADER_ERROR_CHECKSUM);  // Error code
return BOOTLOADER_ERROR_CHECKSUM;
```

**Validation:**
- Flash offset alignment (sector/page aligned)
- Flash bounds checking (prevents overflow)
- Chunk size validation (0-256 bytes)
- Chunk number validation (sequential)
- Flash erase/write validation

**Safety Response:**
- Bootloader returns to main firmware on error
- Error message sent via `protocol_send_debug()`
- Machine continues with existing firmware
- No safety impact (bootloader doesn't affect running firmware)

**Recovery:**
- User can retry OTA update
- ESP32 can use hardware bootloader entry as fallback

---

## Error Reporting

### Alarm System

**Alarm Types** (from `protocol_defs.h`):

| Code | Name | Severity | Description |
|------|------|----------|-------------|
| 0x00 | ALARM_NONE | - | No alarm |
| 0x01 | ALARM_OVER_TEMP | CRITICAL | Over-temperature detected |
| 0x02 | ALARM_WATER_LOW | CRITICAL | Water level low |
| 0x03 | ALARM_SENSOR_FAULT | CRITICAL | Sensor failure |
| 0x04 | ALARM_SSR_FAULT | FAULT | SSR stuck or fault |
| 0x05 | ALARM_COMM_TIMEOUT | WARNING | ESP32 communication lost |
| 0x06 | ALARM_ENV_CONFIG_INVALID | CRITICAL | Environmental config not set |

**Alarm Severity:**
- **0** = Info (logging only)
- **1** = Warning (non-critical)
- **2** = Critical (safe state)

### Status Flags

Status payload includes `flags` bitmask:

| Bit | Flag | Description |
|-----|------|-------------|
| 0 | STATUS_FLAG_BREWING | Brew cycle active |
| 1 | STATUS_FLAG_PUMP_ON | Pump running |
| 2 | STATUS_FLAG_HEATING | Heaters active |
| 3 | STATUS_FLAG_WATER_LOW | Water level low |
| 4 | STATUS_FLAG_ALARM | Safe state active |

---

## Error Tracking & Reporting

### Protocol Error Tracking

The protocol layer tracks communication errors:
- CRC error counter: `protocol_get_crc_errors()`
- Packet error counter: `protocol_get_packet_errors()`
- Error counter reset: `protocol_reset_error_counters()`
- Detailed error logging with context

### Sensor Error Tracking

Each sensor tracks consecutive failures:
- Brew NTC: `g_brew_ntc_error_count`
- Steam NTC: `g_steam_ntc_error_count`
- Group thermocouple: `g_group_tc_error_count`
- Pressure sensor: `g_pressure_error_count`
- PZEM: `pzem_get_error_count()`

Error threshold is 10 consecutive failures before reporting. Recovery is logged when sensors recover.

### Pressure Sensor Validation

Voltage range validation:
- ADC range: 0.2V to 3.0V
- Transducer range: 0.3V to 4.7V
- Out-of-range detection with error reporting
- Fault state prevents filter updates during faults

### Control System Validation

Parameter validation includes:
- PID parameters (negative values, maximum limits)
- Output values (0-100% clamping)
- Mode validation (auto/manual)
- Target validation (brew/steam)
- Detailed error logging for invalid parameters

### Water Management Error Reporting

Water system error reporting:
- Fill timeout via `ALARM_WATER_LOW`
- Level probe monitoring (stuck probe detection)
- Error logging for fill failures

## Current Limitations & Gaps

### 1. Limited Retry Logic

**Current State:**
- Sensor reads: No retry, just use last value (filters maintain last valid)
- PZEM reads: No retry, just increment error count
- Communication: No retry, packet discarded

**Impact:**
- Transient errors may cause unnecessary safe states
- No automatic recovery from temporary failures

**Recommendation:**
- Add retry logic with exponential backoff
- Distinguish transient vs. permanent failures

### 2. Error Logging

**Current State:**
- Errors logged via `DEBUG_PRINT` (USB serial)
- Error counters tracked internally
- No persistent error log
- No error history

**Impact:**
- Difficult to diagnose intermittent issues after reboot
- No way to track error patterns over time

**Recommendation:**
- Add circular error log buffer in RAM
- Store critical errors in flash (limited space)
- Include timestamp, error code, context

### 3. No Graceful Degradation

**Current State:**
- Some errors trigger full safe state
- No partial operation modes

**Examples:**
- Group thermocouple fault → Machine still works (uses NTC only)
- PZEM fault → Falls back to estimation (good!)
- But: Any critical sensor fault → Full shutdown

**Recommendation:**
- Implement graceful degradation modes
- Allow operation with reduced functionality when safe

### 4. Error Statistics

Error counters are tracked internally:
- PZEM: `pzem_get_error_count()`
- Protocol: `protocol_get_crc_errors()`, `protocol_get_packet_errors()`
- Sensors: Consecutive failure counters (brew NTC, steam NTC, group TC, pressure)

**Recommendation:**
- Expose error counters via protocol (MSG_STATUS or new MSG_ERROR_STATS)
- Track error rates over time
- Add error history buffer

---

## Best Practices

### Design Principles

1. **Safety First:**
   - Safety checks run before any control
   - Watchdog fed only after safety checks pass
   - Safe state disables all outputs

2. **Defensive Programming:**
   - Null pointer checks
   - Range validation
   - Invalid value detection (`isnan`, `isinf`)

3. **Debouncing:**
   - Water sensors debounced (5 samples)
   - Prevents false triggers

4. **Hysteresis:**
   - Over-temperature recovery requires 10°C drop
   - Prevents oscillation

5. **Error Tracking:**
   - Consecutive failure counters for sensors
   - Protocol error counters (CRC, packet errors)
   - Error thresholds and recovery detection
   - Detailed error logging with context

6. **Validation:**
   - Parameter validation (PID, outputs, commands)
   - Bounds checking (sensor values, flash offsets)
   - Input validation before processing

7. **Fallback Mechanisms:**
   - PZEM → Power estimation
   - Last valid sensor values
   - Default values on boot

8. **State Machine Protection:**
   - Safe state overrides all states
   - State transitions validated

### Potential Improvements

1. **Error Recovery:**
   - Automatic retry for transient failures
   - Exponential backoff

2. **Error Logging:**
   - Persistent error log in flash
   - Error history tracking

3. **Graceful Degradation:**
   - Partial operation modes
   - Reduced functionality when safe

4. **Error Reporting:**
   - More detailed error codes
   - Error context information
   - Error statistics exposure via protocol

---

## Error Handling Flow Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    Main Control Loop (10Hz)                  │
└───────────────────────────┬─────────────────────────────────┘
                            │
                            ▼
                    ┌───────────────┐
                    │ Safety Check  │
                    │ (safety_check)│
                    └───────┬───────┘
                            │
        ┌───────────────────┼───────────────────┐
        │                   │                   │
        ▼                   ▼                   ▼
  ┌──────────┐      ┌──────────┐      ┌──────────┐
  │ CRITICAL │      │  FAULT   │      │ WARNING   │
  └────┬─────┘      └────┬─────┘      └────┬─────┘
       │                 │                  │
       │                 │                  │
       ▼                 ▼                  ▼
┌─────────────┐   ┌─────────────┐   ┌─────────────┐
│ Safe State  │   │ Send Alarm  │   │ Log Warning │
│ All OFF     │   │ May Continue│   │ Continue    │
│ LED Blink   │   │ with Limits │   │ Operation   │
│ Buzzer Beep │   └─────────────┘   └─────────────┘
└─────────────┘
       │
       │ (Recovery when conditions clear)
       │
       ▼
┌─────────────┐
│ Normal Ops  │
└─────────────┘
```

---

## Recommendations for Enhancement

### Short Term (Easy Wins)

1. **Add Error Counters:**
   ```c
   // Track error rates
   static uint32_t sensor_error_count = 0;
   static uint32_t comm_error_count = 0;
   ```

2. **Improve Sensor Error Handling:**
   ```c
   // Retry failed sensor reads
   for (int retry = 0; retry < 3; retry++) {
       if (read_sensor() != NAN) break;
       sleep_ms(10);
   }
   ```

3. **Better Error Context:**
   ```c
   // Include more info in alarms
   protocol_send_alarm(code, severity, context_value);
   ```

### Medium Term (More Complex)

1. **Error Log Buffer:**
   - Circular buffer in RAM
   - Store last N errors with timestamps
   - Expose via protocol

2. **Automatic Retry with Backoff:**
   - Exponential backoff for transient failures
   - Distinguish permanent vs. transient

3. **Graceful Degradation Modes:**
   - Reduced functionality when non-critical sensors fail
   - Continue operation when safe

### Long Term (Architecture Changes)

1. **Error Handler Module:**
   - Centralized error handling
   - Error classification system
   - Recovery strategies

2. **Persistent Error Log:**
   - Store errors in flash
   - Error history analysis
   - Diagnostic mode

---

## Summary

The error handling system provides:

- **Safety system** as primary error handler
- **Safe state mechanism** (all outputs OFF on critical errors)
- **Watchdog timer** for crash detection
- **Alarm reporting** to ESP32
- **Automatic recovery** for many conditions
- **Protocol error tracking** (CRC errors, packet errors)
- **Sensor error tracking** (consecutive failures, recovery detection)
- **Validation** (pressure sensor voltage, PID parameters, output values)
- **Water management reporting** (timeout alarms, probe monitoring)
- **ACK result codes** (success, invalid, rejected, failed, timeout, busy, not ready)

**Key Principle:**
> **Safety First** - When in doubt, enter safe state. It's better to stop operation than risk damage or injury.

The firmware prioritizes safety over availability. Critical errors always trigger safe state, ensuring the machine cannot operate unsafely.

