# Cleaning Mode Implementation

This document describes the cleaning mode feature implementation, which matches ECM Synchronika's cleaning system behavior.

**See also:** [Feature Status Table](Feature_Status_Table.md) for implementation status.

---

## Overview

Cleaning mode implements the same functionality as ECM Synchronika's cleaning system:
- **Cleaning Counter**: Simple counter that tracks brewing cycles (only counts brews ≥15 seconds) for cleaning reminders
- **Cleaning Reminder**: Alerts user after threshold is reached (default: 100 cycles)
- **Cleaning Cycle**: Backflush cycle with blind filter (10 seconds, repeatable)

**Important Note:** The cleaning counter is **separate from the statistics feature**:
- **Cleaning Counter**: Simple counter that **resets after cleaning** - used only for cleaning reminders
- **Statistics**: Comprehensive tracking with historical data, averages, and time-based analytics - see [Statistics_Feature.md](Statistics_Feature.md)

---

## ECM Synchronika Behavior

Based on research, the ECM Synchronika cleaning mode works as follows:

1. **Brew Counter**:
   - Counts brewing cycles when brew lever is activated for **>15 seconds**
   - Counter persists across reboots (stored in flash)
   - User can set reminder threshold (10-200 cycles, default: 100)

2. **Cleaning Reminder**:
   - When brew count ≥ threshold, display shows "CLn"
   - Reminder persists until user resets counter

3. **Cleaning Process**:
   - **IMPORTANT:** Machine must be at operating temperature (hot) before cleaning
   - User inserts blind filter with cleaning tablet
   - User activates brew lever (or ESP32 sends cleaning start command)
   - Pump runs for ~10 seconds (backflushing)
   - User can stop manually or let it auto-stop
   - Repeat multiple times as needed
   - Rinse with clean water

---

## Implementation

### Files Created

- **`src/pico/include/cleaning.h`**: Cleaning mode API
- **`src/pico/src/cleaning.c`**: Cleaning mode implementation

### Files Modified

- **`src/pico/src/state.c`**: 
  - Records brew cycles (≥15s) for cleaning counter
  - Integrates cleaning mode with state machine
  - Handles cleaning cycle via brew switch

- **`src/pico/src/main.c`**: 
  - Initializes cleaning mode
  - Updates cleaning mode in main loop
  - Handles cleaning commands from ESP32

- **`src/pico/CMakeLists.txt`**: Added `cleaning.c` to build

- **`src/shared/protocol_defs.h`**: Added cleaning mode commands

---

## API

### Initialization

```c
void cleaning_init(void);
```

Called once at boot. Initializes brew counter and threshold (default: 100).

### Brew Counter

```c
void cleaning_record_brew_cycle(uint32_t brew_duration_ms);
uint16_t cleaning_get_brew_count(void);
void cleaning_reset_brew_count(void);
```

- `cleaning_record_brew_cycle()`: Called automatically when brew stops (if duration ≥15s)
- `cleaning_get_brew_count()`: Get current brew count
- `cleaning_reset_brew_count()`: Reset counter (after cleaning completed)

### Cleaning Reminder

```c
uint16_t cleaning_get_threshold(void);
bool cleaning_set_threshold(uint16_t threshold);  // 10-200 cycles
bool cleaning_is_reminder_due(void);
```

- `cleaning_get_threshold()`: Get reminder threshold
- `cleaning_set_threshold()`: Set threshold (validates range)
- `cleaning_is_reminder_due()`: Returns true if brew_count ≥ threshold

### Cleaning Cycle

```c
bool cleaning_start_cycle(void);
void cleaning_stop_cycle(void);
bool cleaning_is_active(void);
cleaning_state_t cleaning_get_state(void);
```

- `cleaning_start_cycle()`: Start cleaning cycle (pump + solenoid on)
  - **Requires machine to be at operating temperature (STATE_READY)**
  - Returns `false` if machine is not ready (cold) or other conditions not met
- `cleaning_stop_cycle()`: Stop cleaning cycle (pump + solenoid off)
- `cleaning_is_active()`: Check if cleaning cycle is running
- `cleaning_get_state()`: Get current cleaning state

### Update Function

```c
void cleaning_update(void);
```

Called in main loop (10Hz). Handles auto-stop after 10 seconds.

---

## Configuration

| Parameter | Value | Description |
|-----------|-------|-------------|
| `CLEANING_DEFAULT_THRESHOLD` | 100 | Default reminder threshold (cycles) |
| `CLEANING_MIN_THRESHOLD` | 10 | Minimum threshold |
| `CLEANING_MAX_THRESHOLD` | 200 | Maximum threshold |
| `CLEANING_CYCLE_MIN_TIME_MS` | 15000 | Minimum brew time to count (15s) |
| `CLEANING_CYCLE_DURATION_MS` | 10000 | Cleaning cycle duration (10s) |

---

## Communication Protocol

### Commands (ESP32 → Pico)

#### MSG_CMD_CLEANING_START (0x18)

Start cleaning cycle. No payload.

```c
// No payload - just header with LENGTH=0
// Pico responds with MSG_ACK
```

#### MSG_CMD_CLEANING_STOP (0x19)

Stop cleaning cycle. No payload.

```c
// No payload - just header with LENGTH=0
// Pico responds with MSG_ACK
```

#### MSG_CMD_CLEANING_RESET (0x1A)

Reset brew counter. No payload.

```c
// No payload - just header with LENGTH=0
// Pico responds with MSG_ACK
```

#### MSG_CMD_CLEANING_SET_THRESHOLD (0x1B)

Set cleaning reminder threshold.

```c
typedef struct __attribute__((packed)) {
    uint16_t threshold;  // 10-200 cycles
} cmd_cleaning_set_threshold_t;  // 2 bytes
```

---

## State Machine Integration

### Cleaning Mode vs. Normal Brew

When cleaning mode is **active**:
- Brew switch starts/stops cleaning cycle (not normal brew)
- Cleaning cycle runs for 10 seconds (auto-stop)
- Normal brew switch handling is skipped

When cleaning mode is **inactive**:
- Normal brew switch handling (as before)
- WEIGHT_STOP signal works normally
- Brew counter records cycles ≥15s

### Cleaning Cycle Flow

```
User presses brew switch (in cleaning mode)
  ↓
cleaning_start_cycle()
  ↓
Pump ON (100%)
Brew solenoid OPEN
  ↓
cleaning_update() runs every 100ms
  ↓
After 10 seconds: auto-stop
  OR
User releases brew switch: manual stop
  ↓
cleaning_stop_cycle()
  ↓
Pump OFF
Brew solenoid CLOSED
```

---

## Brew Counter Logic

### When Brew is Recorded

A brew cycle is counted when:
1. Brew duration ≥ 15 seconds (`CLEANING_CYCLE_MIN_TIME_MS`)
2. Brew stops (via `state_stop_brew()`)

### Implementation

```c
// In state.c, state_exit_action(STATE_BREWING)
if (g_brew_stop_time == 0) {
    g_brew_stop_time = to_ms_since_boot(get_absolute_time());
    
    // Record brew cycle for cleaning counter (if >= 15 seconds)
    uint32_t brew_duration = g_brew_stop_time - g_brew_start_time;
    cleaning_record_brew_cycle(brew_duration);
}
```

---

## Usage Example

### ESP32 Web UI

1. **Display Brew Count**:
   ```javascript
   // Query brew count (via status message or separate command)
   let brewCount = status.brew_count;
   let threshold = status.cleaning_threshold;
   
   if (brewCount >= threshold) {
       displayCleaningReminder();  // Show "CLn" or similar
   }
   ```

2. **Start Cleaning Mode**:
   ```javascript
   // Check if machine is ready (at operating temperature)
   if (status.state === STATE_READY) {
       // User clicks "Start Cleaning" button
       sendCommand(MSG_CMD_CLEANING_START);
   } else {
       // Show message: "Machine must be at operating temperature"
       showError("Machine must be hot before cleaning");
   }
   ```

3. **Run Cleaning Cycle**:
   - **IMPORTANT:** Ensure machine is at operating temperature (STATE_READY)
     - If machine is cold, `cleaning_start_cycle()` will return `false`
     - ESP32 should check `status.state === STATE_READY` before allowing cleaning
   - User inserts blind filter with cleaning tablet
   - User presses brew lever (or ESP32 sends `MSG_CMD_CLEANING_START`)
   - Pump runs for 10 seconds (backflushing)
   - User releases lever (or ESP32 sends `MSG_CMD_CLEANING_STOP`)
   - Repeat 3-5 times

4. **Reset Counter**:
   ```javascript
   // After cleaning completed
   sendCommand(MSG_CMD_CLEANING_RESET);
   ```

---

## Persistence

Brew count and cleaning threshold are persisted to flash via `config_persistence`:
- Saved automatically after each brew cycle (≥15s)
- Loaded on boot via `cleaning_init()`
- See `config_persistence.h` for `cleaning_brew_count` and `cleaning_threshold` fields

Cleaning mode is "always available" (matches ECM Synchronika behavior) - cleaning cycles can be started via `MSG_CMD_CLEANING_START` or brew switch.

---

## Potential Enhancements

1. **Status Payload Updates**:
   - Add brew count, threshold, reminder status to `MSG_STATUS`
   - ESP32 can display cleaning reminder

2. **Cleaning Mode Indicator**:
   - Add `STATUS_FLAG_CLEANING` to status flags
   - ESP32 can show "Cleaning Mode" indicator

3. **Multiple Cleaning Cycles**:
   - Track number of cleaning cycles run
   - Auto-advance through cleaning sequence
   - Optional: Count cleaning cycles separately

---

## Testing

### Manual Testing

1. **Brew Counter**:
   - Pull shot for >15 seconds → counter increments
   - Pull shot for <15 seconds → counter does not increment
   - Reset counter → counter = 0

2. **Cleaning Reminder**:
   - Set threshold to 5 cycles
   - Pull 5 shots (>15s each) → reminder should trigger
   - Reset counter → reminder clears

3. **Cleaning Cycle**:
   - Start cleaning cycle → pump + solenoid ON
   - Wait 10 seconds → auto-stops
   - OR release brew switch → manual stop
   - Verify pump and solenoid turn OFF

4. **Integration**:
   - Normal brew works when cleaning mode inactive
   - Cleaning cycle works when cleaning mode active
   - Brew counter records normal brews (not cleaning cycles)

---

## Summary

The cleaning mode provides:
- Brew counter (tracks cycles ≥15s) with flash persistence
- Cleaning reminder threshold (10-200 cycles, configurable)
- Cleaning cycle (10s backflush, auto-stop or manual)
- Temperature requirement: Cleaning cycles only start when machine is at operating temperature (STATE_READY)
- State machine integration with normal brewing
- Protocol commands (start, stop, reset, set threshold)

**Design Decision:** Cleaning cycles require the machine to be at operating temperature (hot) for effective cleaning, which is enforced by checking `state_is_ready()` before allowing cleaning cycles to start.

