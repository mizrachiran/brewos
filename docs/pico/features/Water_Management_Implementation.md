# Water Management Implementation

This document describes the water management system, including steam boiler auto-fill logic based on dual boiler hydraulic principles.

**See also:** [Feature Status Table](Feature_Status_Table.md) for implementation status.

---

## Overview

Steam boiler auto-fill logic has been implemented based on the **dual boiler hydraulic system**. This follows the correct physical principles of how dual boiler espresso machines work.

---

## Hydraulic System Understanding

### The T-Junction

Water from the pump splits into two paths:
- **Path A (Left)**: Brew Boiler (via check valve)
- **Path B (Right)**: Steam Boiler (via fill solenoid)

### Steam Boiler: Auto-Fill (Active Logic)

The steam boiler is **partially full** (~60% water, rest is steam). As you steam milk, water leaves as steam, so the level drops and needs active refilling.

**Components:**
- **Level Probe (GPIO4)**: Metal rod hanging into boiler
  - HIGH = Water below probe (needs fill)
  - LOW = Water at/above probe (OK)
- **Fill Solenoid (2-way)**: Electronic gate on Path B
  - Normally CLOSED (Path B blocked)
  - Opens to allow water into steam boiler

### Brew Boiler: Displacement (Passive Logic)

The brew boiler is **always 100% full** of liquid water. No active filling needed during operation.

**Components:**
- **Check Valve (One-Way)**: Mechanical valve on Path A
  - Lets water IN but stops it from coming OUT
- **Coffee Puck**: Restrictor at the end

**Operation:**
- Pump forces cold water into bottom of brew boiler
- Since boiler is full, 1mL in = 1mL hot water out to coffee puck
- No level probe needed (always full)

### Brew Priority (Critical!)

**The Problem:**
- Brewing requires ~9 Bar pressure against coffee puck
- Steam filling is effectively 0 Bar (open pipe)

If steam fill happens **during brewing**, all water takes path of least resistance (steam boiler), causing espresso pressure to drop to zero.

**The Solution:**
**"Brew Priority"** - If brewing, **ignore** steam boiler low-level signal. Only fill steam boiler when **NOT brewing**.

---

## Steam Boiler Auto-Fill Cycle

### Fill Sequence

1. **Detection**: Level probe detects low water (GPIO4 HIGH)
2. **CRITICAL Safety**: Cut steam heater FIRST (before filling!)
   - Prevents explosion if cold water hits hot dry element
3. **Wait**: Brief delay (500ms) after cutting heater
4. **Open Gate**: Open fill solenoid (Path B)
5. **Push**: Turn on pump
6. **Fill**: Water flows into steam boiler
7. **Stop**: Probe touches water (GPIO4 LOW)
8. **Reset**: Turn off pump, close solenoid, restore heater

### State Machine
```
STEAM_FILL_IDLE
  ↓ (low water detected)
STEAM_FILL_HEATER_OFF (cut heater, wait 500ms)
  ↓
STEAM_FILL_ACTIVE (solenoid ON, pump ON)
  ↓ (probe touches water)
STEAM_FILL_COMPLETE (pump OFF, solenoid OFF)
  ↓ (1s delay)
STEAM_FILL_IDLE (heater restored)
```

### Brew Priority

- **Brew Check**: If brewing, don't start fill cycle
- **Active Fill Stop**: If fill starts and brewing begins, stop fill immediately
- **Pressure Protection**: Prevents pressure drop during espresso extraction

### Water LED Control (FUNC-032)

- LED ON: Reservoir present AND tank level OK
- LED OFF: Reservoir empty OR tank level low
- Updates continuously

### Safety Features

- **Heater Cut First**: Steam heater cut BEFORE filling (critical safety)
- **Timeout Protection**: 30-second maximum fill time
- **Reservoir Check**: Won't fill if reservoir empty
- **Safe State**: Fill disabled in safe state
- **Debouncing**: Level probe debounced (100ms)

---

## Files Created/Modified

### New Files
- `src/pico/src/water_management.c` - Steam boiler auto-fill implementation
- `src/pico/include/water_management.h` - Water management interface

### Modified Files
- `src/pico/src/main.c` - Added water management initialization and update
- `src/pico/CMakeLists.txt` - Added water_management.c to build

---

## Configuration

### Steam Fill Settings

Can be adjusted in `water_management.c`:

```c
#define STEAM_FILL_TIMEOUT_MS        30000   // 30s max fill time
#define STEAM_LEVEL_DEBOUNCE_MS     100     // Debounce time
#define STEAM_FILL_CHECK_INTERVAL_MS 100    // Check every 100ms
#define STEAM_HEATER_OFF_DELAY_MS   500     // Delay after cutting heater
```

---

## API Functions

```c
// Initialize water management
void water_management_init(void);

// Update water management (call periodically)
void water_management_update(void);

// Auto-fill control
void water_management_set_auto_fill(bool enabled);
bool water_management_is_auto_fill_enabled(void);
bool water_management_is_filling(void);

// Manual fill solenoid control
void water_management_set_fill_solenoid(bool on);
```

---

## Steam Fill Cycle Details

### Detection Phase

- Monitors steam level probe (GPIO4)
- Debounced reading (100ms)
- HIGH = low water (needs fill)
- LOW = water OK

### Safety Phase (CRITICAL!)

**Before filling:**
1. Check if steam heater is ON
2. **Cut steam heater** (set to 0%)
3. Wait 500ms delay
4. **Then** proceed to fill

**Why?** Cold water hitting a hot, dry heating element will cause it to explode. This is a **critical safety requirement**.

### Fill Phase

1. Open fill solenoid (Path B)
2. Turn on pump
3. Monitor level probe
4. Stop when probe touches water (goes LOW)

### Completion Phase

1. Turn off pump
2. Close fill solenoid
3. Wait 1 second
4. Restore steam heater (return to PID control)

---

## Brew Priority Logic

### During Brewing

```
IF brewing:
  - Ignore steam boiler low-level signal
  - Don't start fill cycle
  - If fill already active, stop immediately
```

### After Brewing

```
IF NOT brewing AND steam level low:
  - Start fill cycle
  - Follow normal fill sequence
```

### Why This Matters

- **Brewing**: Requires 9 Bar pressure
- **Steam Fill**: 0 Bar (open pipe)
- **Conflict**: If both active, water takes path of least resistance (steam boiler)
- **Result**: Espresso pressure drops to zero, shot ruined

**Solution**: Brew priority ensures steam fill only happens when NOT brewing.

---

## Hardware Requirements

### For Steam Auto-Fill:

1. **Steam Level Probe**: GPIO4 (input)
   - HIGH = water below probe
   - LOW = water at/above probe

2. **Fill Solenoid**: `relay_fill_solenoid` GPIO pin (output)
   - Controls Path B (steam boiler path)
   - ON = Path B open (can fill)
   - OFF = Path B closed (normal)

3. **Pump**: Already controlled by brew system
   - Used for both brewing and filling

4. **Reservoir Switch**: GPIO2 (input)
   - Must be present to allow filling

### Current PCB Configuration:

For ECM_V1 PCB, the fill solenoid is set to -1 (not used). To enable:
1. Connect fill solenoid to an available GPIO pin
2. Update PCB configuration (`relay_fill_solenoid`)
3. Rebuild firmware

---

## Testing

### Build Test
```bash
cd src/pico/build
cmake ..
make
```

### Hardware Test Checklist (When Available)

1. **Steam Fill Cycle**
   - [ ] Low water detected → Steam heater cuts
   - [ ] 500ms delay → Fill solenoid opens
   - [ ] Pump turns ON
   - [ ] Water fills until probe touches
   - [ ] Pump turns OFF
   - [ ] Solenoid closes
   - [ ] Steam heater restored

2. **Brew Priority**
   - [ ] Low water during brewing → Fill doesn't start
   - [ ] Fill active, brew starts → Fill stops immediately
   - [ ] After brewing stops → Fill can start

3. **Safety**
   - [ ] Heater cut BEFORE filling (critical!)
   - [ ] Timeout (30s) → Fill stops
   - [ ] Reservoir empty → Fill doesn't start
   - [ ] Safe state → Fill disabled

4. **Water LED**
   - [ ] Reservoir present AND tank OK → LED ON
   - [ ] Reservoir empty → LED OFF
   - [ ] Tank level low → LED OFF

---

## Critical Safety Notes

### ⚠️ Steam Heater Must Be Cut FIRST

**NEVER** fill steam boiler while heater is ON if water is below probe!

**Why:** Cold water hitting hot, dry heating element = **EXPLOSION**

**Implementation:**
1. Detect low water
2. **Cut heater immediately**
3. Wait 500ms
4. **Then** start filling

This is implemented and **cannot be bypassed**.

### ⚠️ Brew Priority Must Be Respected

**NEVER** fill steam boiler while brewing!

**Why:** All water will go to steam boiler (0 Bar), espresso pressure drops to zero

**Implementation:**
- Check `state_is_brewing()` before starting fill
- Stop fill immediately if brewing starts

---

## Notes

- **Default State**: Auto-fill is **enabled** by default (steam boiler needs it)
- **Brew Boiler**: No active filling needed (displacement system, always full)
- **Safety First**: Multiple safety checks prevent dangerous conditions
- **Integration**: Works with existing safety system and state machine

---

## Future Enhancements

Possible improvements:
1. **Fill Statistics**: Track fill duration and frequency
2. **Fill Rate Monitoring**: Detect slow fills (clogged filter, etc.)
3. **Multiple Fill Cycles**: Support multiple cycles if needed
4. **Configurable Delays**: Make delays configurable via protocol

---

## Summary

Steam boiler auto-fill follows dual boiler hydraulic principles with brew priority and critical safety measures. The system ensures safe operation by cutting the heater before filling and respecting brew priority to maintain proper extraction pressure.
