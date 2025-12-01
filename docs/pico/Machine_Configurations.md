# Machine Configurations

> **Document Status:** This document has been reviewed and updated based on technical feedback. Key improvements include: corrected PWM period specification (25Hz), added voltage variation considerations, flash wear leveling notes, HX SSR safety requirements, and flow control interaction notes.

## Overview

The firmware supports multiple espresso machine types through compile-time configuration. Each machine type defines:

- **Feature flags** (what hardware is present)
- **Pin mappings** (GPIO assignments)
- **Safety thresholds** (temperature limits)
- **Electrical specifications** (heater power ratings, machine-specific)
- **Sensor characteristics** (NTC parameters, pressure ranges)
- **Default parameters** (centralized per machine)
- **Heating strategy** (configurable)

**Key Design Principle:**

- **Machine Configuration** = Hardware-specific, fixed per machine model (heater power, sensors, pins)
- **Environmental Configuration** = Installation-specific, varies by location (voltage, current limits)
- **Same environmental config works with different machine types** - System automatically adapts

---

## Heating Strategies

Different heating strategies optimize for different use cases:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          HEATING STRATEGIES                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   HEAT_BREW_ONLY                                                            │
│   ──────────────                                                            │
│   • Only brew boiler heats                                                  │
│   • Steam boiler stays off                                                  │
│   • Use case: Espresso-only sessions, energy saving                        │
│   • Power draw: ~6A (1400W)                                                 │
│                                                                              │
│   HEAT_SEQUENTIAL (Brew → Steam)                                            │
│   ──────────────────────────────                                            │
│   • Brew boiler heats first to setpoint                                    │
│   • Steam boiler starts after brew reaches target (or threshold %)         │
│   • Use case: Reduce peak power, 15A circuit friendly                      │
│   • Power draw: ~6A peak (1400W)                                           │
│   • Warm-up: Slightly longer, but safer                                    │
│                                                                              │
│   HEAT_PARALLEL                                                             │
│   ─────────────                                                             │
│   • Both boilers heat simultaneously                                        │
│   • Fastest warm-up time                                                    │
│   • Use case: 20A circuits, fast startup priority                          │
│   • Power draw: ~12A peak (2800W)                                          │
│   • ⚠️  May trip 15A breakers!                                              │
│                                                                              │
│   HEAT_SMART_STAGGER                                                        │
│   ───────────────────                                                       │
│   • Time-shares between boilers to limit simultaneous ON time              │
│   • Priority boiler gets its requested duty, other fills remaining time    │
│   • Both PIDs remain active (faster warm-up than sequential)               │
│   • Use case: Balance warm-up speed vs circuit capacity                    │
│   • ⚠️  IMPORTANT: Peak current is still full element current when ON!     │
│   • Does NOT reduce instantaneous power - only reduces overlap time        │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### How Power Control Works

All heating strategies control power via **SSR PWM** (Pulse Width Modulation):

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        SSR PWM POWER CONTROL                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   The SSR switches ON/OFF within a 40ms period (25Hz).                     │
│   Duty cycle (%) determines TIME the element is ON:                         │
│                                                                              │
│   100% duty: ████████████████████████████████████████  ON=100%, OFF=0%      │
│    50% duty: ████████████████████░░░░░░░░░░░░░░░░░░░░  ON=50%,  OFF=50%     │
│    25% duty: ██████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  ON=25%,  OFF=75%     │
│              |←────────────── 40ms (25Hz) ──────────→|                     │
│                                                                              │
│   The heater element's thermal mass smooths out the pulses.                 │
│   PID outputs duty cycle → SSR PWM → controlled heating.                   │
│                                                                              │
│   Note: 25Hz (40ms period) is optimal for zero-crossing SSRs:              │
│   • Fast enough to prevent visible flicker on shared circuits              │
│   • Slow enough to minimize EMI and SSR switching losses                    │
│   • Thermal mass of boiler water naturally filters the pulses               │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### ⚠️ CRITICAL: SSR Limitations - What PWM CANNOT Do

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                   SSR IS A SWITCH, NOT A DIMMER!                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   An SSR is either 100% ON or 100% OFF. It cannot reduce element power.    │
│                                                                              │
│   50% Duty Cycle on 1400W (6A) Element:                                     │
│   ────────────────────────────────────────                                  │
│   Time:      |███████████|           |███████████|           |              │
│   Power:     |  1400W    |    0W     |  1400W    |    0W     |              │
│   Current:   |   6A      |    0A     |   6A      |    0A     |              │
│              |←─ 0.5s ──→|←─ 0.5s ──→|←─ 0.5s ──→|←─ 0.5s ──→|              │
│                                                                              │
│   ✓ Average power over time: 700W                                           │
│   ✓ Average current over time: 3A                                           │
│   ✗ PEAK current (instantaneous): Still 6A!                                 │
│   ✗ PEAK power (instantaneous): Still 1400W!                                │
│                                                                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   WHAT PWM DUTY CYCLE CONTROLS:                                             │
│   ✓ Energy delivered over time (thermal mass smooths this)                  │
│   ✓ Average power consumption (for energy bills)                            │
│   ✓ Temperature control (PID adjusts duty based on error)                   │
│                                                                              │
│   WHAT PWM DUTY CYCLE DOES NOT CONTROL:                                     │
│   ✗ Instantaneous current draw (breakers see this!)                         │
│   ✗ Peak power demand (generators/inverters see this!)                      │
│   ✗ Inrush current (even worse - cold elements draw more)                   │
│                                                                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   IMPLICATIONS:                                                              │
│                                                                              │
│   • Tripping breakers? PWM duty won't help! Use HEAT_SEQUENTIAL to          │
│     ensure only one element is ON at a time.                                │
│                                                                              │
│   • Using a generator/inverter? Check its PEAK/SURGE rating, not just       │
│     continuous rating. A 1000W inverter can't run a 1000W element at        │
│     "50% duty" - it will still see 1000W spikes!                            │
│                                                                              │
│   • Running both boilers on 15A circuit? Use HEAT_SEQUENTIAL (like the      │
│     Synchronika's F.02=0/1 "15 Amp Mode"). This ensures peak current        │
│     never exceeds ~6A instead of ~12A.                                      │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### What Each Strategy Actually Does for Peak Current

| Strategy             | Both Elements ON Simultaneously? | Peak Current |
| -------------------- | -------------------------------- | ------------ |
| `HEAT_BREW_ONLY`     | No (steam off)                   | ~6A          |
| `HEAT_SEQUENTIAL`    | **No** (one after other)         | **~6A** ✓    |
| `HEAT_PARALLEL`      | **Yes**                          | ~12A         |
| `HEAT_SMART_STAGGER` | Partially (time-shared)          | ~6-12A       |

**For breaker/generator protection:** Use `HEAT_SEQUENTIAL` - this guarantees only one element is ever ON at a time.

**`HEAT_SMART_STAGGER`** reduces _average_ combined duty, but during the overlap periods, both elements CAN be ON simultaneously. It's better than `PARALLEL` but not as safe as `SEQUENTIAL` for current-limited installations.

**Future Enhancement - Flow Control Interaction:**

- High flow rates during pressure profiling can drain the boiler faster, causing temperature drops
- Consider adding feed-forward compensation to PID based on flow rate
- Would require flow meter integration (`has_flow_meter` feature flag)
- Advanced feature for future implementation

See [Architecture.md](Architecture.md#ssr-pwm-output) for implementation details.

---

## Electrical Configuration

### Machine Electrical Specifications

Machine electrical configuration defines hardware-specific power ratings that are fixed for each machine model. These are part of the machine configuration and do not vary by installation.

**See [Environmental_Configuration.md](Environmental_Configuration.md) for installation-specific electrical configuration (voltage, current limits).**

#### Power Calculations

**ECM Synchronika Specifications:**

- Brew boiler heater: **1500W**
- Steam boiler heater: **1000W**
- Total maximum power: **2500W**

```
At 230V (Europe/Israel):
  Brew heater:  1500W ÷ 230V = 6.5A
  Steam heater: 1000W ÷ 230V = 4.3A
  Both heaters: 2500W ÷ 230V = 10.9A

At 120V (USA):
  Brew heater:  1500W ÷ 120V = 12.5A
  Steam heater: 1000W ÷ 120V = 8.3A
  Both heaters: 2500W ÷ 120V = 20.8A
```

**Implications for ECM Synchronika (Peak Current Considerations):**

- **230V regions (10A limit):** Use `HEAT_SEQUENTIAL` - one heater at a time (6.5A max peak)
- **230V regions (16A limit):** Can use `HEAT_PARALLEL` (10.9A peak < 16A) ✓
- **120V regions (12A limit):** `HEAT_SEQUENTIAL` only - even brew alone draws 12.5A ≈ limit
- **120V regions (16A limit):** `HEAT_SEQUENTIAL` recommended (12.5A peak), `HEAT_PARALLEL` would trip breaker (20.8A peak!)

⚠️ **Remember:** These are PEAK currents. When the element is ON, it draws full current regardless of duty cycle!

**⚠️ Voltage Variation Consideration:**

- Power calculations assume nominal voltage (230V or 120V)
- Real-world grid voltage varies: ±10% is common (e.g., 207-253V at 230V nominal)
- Heater power varies with voltage squared: $P = V^2/R$
- Example: 1500W @ 230V becomes ~1633W @ 240V (UK/Australia)
- **Safety margin:** Environmental config should account for +10% voltage tolerance when calculating current limits

**Note:** Power ratings are machine-specific and must be configured in the machine configuration structure.

### Configuration Separation

Electrical configuration is split into two parts:

1. **Machine Configuration** - Hardware-specific, fixed (boiler power ratings) - **This document**
2. **Environmental Configuration** - Installation-specific, may vary (voltage, current limits) - **See [Environmental_Configuration.md](Environmental_Configuration.md)**

This separation allows the same machine to work in different electrical environments.

### Machine Electrical Configuration

```c
typedef struct {
    // ═══════════════════════════════════════════════════════════════
    // MACHINE ELECTRICAL SPECIFICATIONS (Hardware-specific, fixed)
    // ═══════════════════════════════════════════════════════════════

    // Heater power ratings (W) - from machine manufacturer specs
    // These vary by machine model (e.g., ECM Synchronika, Rancilio Silvia, etc.)
    uint16_t brew_heater_power;      // e.g., 1500W (ECM Synchronika), 1200W (other machine)
    uint16_t steam_heater_power;     // e.g., 1000W (ECM Synchronika), 1400W (other machine)

    // Additional machine-specific electrical specs (future expansion)
    // uint16_t pump_power;           // Pump motor power rating
    // uint16_t other_loads_power;    // Other electrical loads

} machine_electrical_t;
```

**Machine electrical specs are:**

- **Machine-specific** - Each machine model has different heater power ratings
- **Fixed for a given machine model** - ECM Synchronika always has 1500W/1000W, Rancilio Silvia has different ratings
- **Defined at compile-time** - Part of machine configuration header file
- **Never change** - Hardware specification that doesn't vary by installation
- **Used with environmental config** - Combined at runtime to calculate actual current draw

### Example Machine Configurations

```c
// ECM Synchronika (Dual Boiler)
static const machine_electrical_t MACHINE_ECM_SYNCHRONIKA_ELECTRICAL = {
    .brew_heater_power  = 1500,  // ECM Synchronika brew boiler (from manufacturer specs)
    .steam_heater_power = 1000,  // ECM Synchronika steam boiler (from manufacturer specs)
};

// Rancilio Silvia (Single Boiler) - Example for future support
static const machine_electrical_t MACHINE_RANCILIO_SILVIA_ELECTRICAL = {
    .brew_heater_power  = 1200,  // Single boiler, used for both brew and steam
    .steam_heater_power = 1200,  // Same heater, different setpoint
};

// Generic HX Machine - Example for future support
static const machine_electrical_t MACHINE_GENERIC_HX_ELECTRICAL = {
    .brew_heater_power  = 0,     // No brew boiler (heat exchanger)
    .steam_heater_power = 1400,  // Steam boiler only
};
```

**Note:** Each machine type has different heater power ratings. These are hardware specifications that don't change regardless of where the machine is installed.

**For environmental configuration examples (voltage, current limits), see [Environmental_Configuration.md](Environmental_Configuration.md).**

### Integration with Environmental Configuration

```c
// Machine configuration (hardware-specific, fixed per machine model)
typedef struct {
    machine_features_t      features;      // What hardware is present
    pin_config_t            pins;           // GPIO pin assignments
    safety_config_t         safety;         // Safety thresholds
    sensor_config_t         sensors;        // Sensor characteristics
    machine_electrical_t    electrical;     // Machine electrical specs (heater power ratings)
    defaults_config_t       defaults;       // Default operating parameters
} machine_config_t;
```

**Key Principles:**

- **Machine config** (this document):
  - Compiled into firmware, defines hardware capabilities
  - Different for each machine model (ECM Synchronika, Rancilio Silvia, etc.)
  - Contains machine-specific specs: heater power, sensors, pins, safety limits
  - Never changes for a given machine model
- **Environmental config** (see [Environmental_Configuration.md](Environmental_Configuration.md)):
  - Can be set at compile-time or runtime
  - Defines installation constraints (voltage, circuit capacity)
  - Same structure works with different machine types
  - Can be updated if machine is moved to different location
- **Runtime**:
  - System combines machine config + environmental config
  - Calculates actual operating limits (current draw, heating strategies)
  - Same environmental config works with different machine types

**For details on environmental configuration, automatic heating strategy selection, runtime monitoring, configuration storage, and safety considerations, see [Environmental_Configuration.md](Environmental_Configuration.md).**

### Example: ECM Synchronika Configuration

```c
// For Israel/Europe with 16A breaker
static const machine_config_t MACHINE_ECM_SYNCHRONIKA = {
    // ... other config ...

    .electrical = {
        // Machine electrical specs (hardware-specific, fixed)
        .brew_heater_power  = 1500,  // ECM Synchronika brew boiler (actual spec)
        .steam_heater_power = 1000,  // ECM Synchronika steam boiler (actual spec)
    },

    // Note: Environmental electrical config (voltage, current limits) is separate
    // and can be set at compile-time or runtime based on installation

    .defaults = {
        // ... other defaults ...

        // Heating strategy automatically selected based on electrical config
        .heating = {
            .strategy               = HEAT_SMART_STAGGER,  // Safe for 16A
            .max_combined_duty      = 150,                  // Limits to ~9A peak
            .stagger_priority       = 0,                   // Brew priority
        },
    },
};
```

---

### Heating Strategy Enum

```c
typedef enum {
    HEAT_BREW_ONLY       = 0,  // Only brew boiler
    HEAT_SEQUENTIAL      = 1,  // Brew first, then steam
    HEAT_PARALLEL        = 2,  // Both simultaneously
    HEAT_SMART_STAGGER   = 3,  // Staggered duty cycles
} heating_strategy_t;
```

### Strategy Parameters

```c
typedef struct {
    heating_strategy_t strategy;

    // For HEAT_SEQUENTIAL:
    // Start second boiler when first reaches this % of setpoint
    uint8_t sequential_threshold_pct;  // e.g., 90 = start at 90% of setpoint

    // For HEAT_SMART_STAGGER:
    // Maximum combined duty cycle (0-200, where 200 = both at 100%)
    uint8_t max_combined_duty;         // e.g., 150 = max 75% each if both active

    // Which boiler has priority in stagger mode
    uint8_t stagger_priority;          // 0=brew, 1=steam

} heating_config_t;
```

---

## Supported Machine Types

| Type ID | Enum                     | Description                         | Status             |
| ------- | ------------------------ | ----------------------------------- | ------------------ |
| 0x01    | `MACHINE_DUAL_BOILER`    | Dual boiler with independent PID    | ✅ Implemented     |
| 0x02    | `MACHINE_SINGLE_BOILER`  | Single boiler, brew/steam switching | ✅ Implemented     |
| 0x03    | `MACHINE_HEAT_EXCHANGER` | HX with steam boiler PID            | ✅ Implemented     |
| 0x04    | `MACHINE_THERMOBLOCK`    | Flow heater (no boiler)             | ❌ Not Implemented |

> **✅ IMPLEMENTATION STATUS:** Multi-machine support is fully implemented with **separate control files** for each machine type.

### Architecture: Compile-Time File Selection

Each machine type has its own control implementation file, selected at compile time:

```
src/pico/
├── include/
│   ├── control.h                 # Public API (same for all machines)
│   └── control_impl.h            # Internal interface for machine-specific code
├── src/
│   ├── control_common.c          # Shared: PID compute, output helpers, public API
│   ├── control_dual_boiler.c     # Dual boiler control (independent PIDs)
│   ├── control_single_boiler.c   # Single boiler control (mode switching)
│   └── control_heat_exchanger.c  # HX control (steam-only PID)
```

**Benefits:**

- **No runtime overhead** - Only compiles needed code
- **Smaller binary** - Machine-specific code only
- **Clean separation** - Each machine type in its own file
- **Easy to extend** - Add new machine types as new files

### Key Implementation Files

| File                           | Purpose                                                        |
| ------------------------------ | -------------------------------------------------------------- |
| `include/control_impl.h`       | Internal interface shared by machine-specific implementations  |
| `src/control_common.c`         | Shared PID, output helpers, public API wrappers                |
| `src/control_dual_boiler.c`    | Dual boiler: independent brew/steam PIDs, heating strategies   |
| `src/control_single_boiler.c`  | Single boiler: mode switching, steam timeout, setpoint ramping |
| `src/control_heat_exchanger.c` | HX: steam-only PID, passive brew water heating                 |
| `include/machine_config.h`     | Machine type definitions and feature flags                     |
| `src/machine_config.c`         | Runtime machine configuration access                           |
| `src/safety.c`                 | Machine-type aware safety checks                               |

### Heat Exchanger Control Modes

Traditional HX espresso machines (like those using **PRO ELIND ECO** control boards) often use a **pressurestat** - a mechanical pressure switch that controls the heater directly. Our firmware supports three control modes to handle different HX configurations:

| Mode                      | Description                           | Control      | Use Case                              |
| ------------------------- | ------------------------------------- | ------------ | ------------------------------------- |
| `HX_CONTROL_TEMPERATURE`  | PID based on steam NTC                | Active       | Modern retrofit with NTC sensor added |
| `HX_CONTROL_PRESSURE`     | PID based on pressure transducer      | Active       | Machines with pressure sensor         |
| `HX_CONTROL_PRESSURESTAT` | External pressurestat controls heater | Monitor only | Traditional machines (no rewiring)    |

#### Control Flow Diagrams

```
HX_CONTROL_TEMPERATURE (Modern Retrofit)
┌─────────────────────────────────────────────────────────────────┐
│  Steam NTC → Pico PID → SSR → Heater                            │
│  Group TC  → Display/Ready detection                            │
│  Pressure  → Display/Logging                                     │
└─────────────────────────────────────────────────────────────────┘

HX_CONTROL_PRESSURE (Pressure-Based PID)
┌─────────────────────────────────────────────────────────────────┐
│  Pressure Transducer → Pico PID → SSR → Heater                  │
│  Group TC  → Display/Ready detection                            │
│  Steam NTC → Display (if present)                               │
└─────────────────────────────────────────────────────────────────┘

HX_CONTROL_PRESSURESTAT (Monitor Only - Traditional Machines)
┌─────────────────────────────────────────────────────────────────┐
│  Pressurestat ──────────────────────→ Heater (direct wiring)    │
│                                                                  │
│  Our Controller (monitor only):                                 │
│  Pressure  → Display/Logging                                     │
│  Group TC  → Display/Ready detection                            │
│  Steam NTC → Display (if present)                               │
│                                                                  │
│  SSR NOT connected to heater in this mode                       │
└─────────────────────────────────────────────────────────────────┘
```

#### Pressurestat Mode Details

For traditional HX machines with mechanical pressurestat (like **PRO ELIND ECO**):

- **No rewiring required** - pressurestat remains in control of the heater
- Our controller provides **monitoring and display** only
- Read pressure, group temperature, steam temperature (if sensor present)
- Display "ready" indication based on group temperature
- Log data for analysis
- **SSR output is disabled** - heater wired through pressurestat

**⚠️ Safety Requirement:** In `HX_CONTROL_PRESSURESTAT` mode, the SSR GPIO pin must be explicitly driven **LOW** (not just PWM duty=0%) to ensure the pin is in a safe state. The firmware sets demand to 0.0f, which results in 0% PWM duty, but the GPIO should also be explicitly set LOW during initialization to prevent any floating state.

This mode allows adding smart monitoring to existing machines without modifying the safety-critical heater control circuit.

#### Configuration Examples

```c
// Traditional HX with pressurestat (e.g., PRO ELIND ECO board)
.mode_config.heat_exchanger = {
    .control_mode              = HX_CONTROL_PRESSURESTAT,
    .pressurestat_has_feedback = false,
    .target_group_temp         = 93.0f,
    .use_group_for_ready       = true,
    .group_ready_temp          = 88.0f,
}

// Modern HX retrofit with steam NTC sensor
.mode_config.heat_exchanger = {
    .control_mode              = HX_CONTROL_TEMPERATURE,
    .steam_setpoint            = 125.0f,
    .target_group_temp         = 93.0f,
    .use_group_for_ready       = true,
    .group_ready_temp          = 88.0f,
}

// HX with pressure-based control
.mode_config.heat_exchanger = {
    .control_mode              = HX_CONTROL_PRESSURE,
    .pressure_setpoint_bar     = 1.0f,
    .pressure_hysteresis_bar   = 0.1f,
    .target_group_temp         = 93.0f,
    .use_group_for_ready       = true,
    .group_ready_temp          = 88.0f,
}
```

### Machine Type Differences

The following table summarizes the key differences between machine types:

| Feature                | Dual Boiler  | Single Boiler | Heat Exchanger |
| ---------------------- | :----------: | :-----------: | :------------: |
| **Number of boilers**  |      2       |       1       |   1 (steam)    |
| **Brew heating**       | Active (PID) | Active (PID)  |  Passive (HX)  |
| **Steam heating**      | Active (PID) |  Mode switch  |  Active (PID)  |
| **Brew NTC sensor**    |      ✓       |       ✓       |       ✗        |
| **Steam NTC sensor**   |      ✓       |       ✗       |       ✓        |
| **Group thermocouple** |   Optional   |   Optional    |  **Required**  |
| **Number of SSRs**     |      2       |       1       |       1        |
| **Steam level probe**  |      ✓       |       ✗       |       ✓        |
| **Heating strategies** |    All 5     |   BREW_ONLY   |   BREW_ONLY    |
| **Mode switching**     |      No      |      Yes      |       No       |
| **Ready detection**    |   Brew NTC   |   Brew NTC    |    Group TC    |

### Module-by-Module Machine Type Handling

| Module               | Machine-Type Aware | How It Adapts                                    |
| -------------------- | :----------------: | ------------------------------------------------ |
| `control_*.c`        |         ✅         | Separate files per machine type (compile-time)   |
| `control_common.c`   |         ✅         | Restricts heating strategies for non-dual-boiler |
| `safety.c`           |         ✅         | Only checks sensors/SSRs that exist              |
| `state.c`            |         ✅         | Uses group_temp for HX ready detection           |
| `water_management.c` |         ✅         | Skips steam fill for single boiler               |
| `sensors.c`          |         ✅         | Checks `machine_has_*()` before reading sensors  |
| `main.c`             |         ✅         | Logs machine config, adapts status payload       |
| `protocol.c`         |         ✅         | Reports machine_type in boot payload             |

### Building for Different Machines

```bash
# Build single machine type (creates brewos_<type>.uf2)
cmake .. -DMACHINE_TYPE=DUAL_BOILER
make

# Build all machine types at once
cmake .. -DBUILD_ALL_MACHINES=ON
make
```

**Output binaries:**

- `brewos_dual_boiler.uf2`
- `brewos_single_boiler.uf2`
- `brewos_heat_exchanger.uf2`

See [Feature Status Table](Feature_Status_Table.md) for current implementation status.

---

## Machine-Type-Aware Behavior

### Control System (`control_*.c`)

Each machine type has its own control implementation file:

| File                       | Machine Type   | Control Logic                                     |
| -------------------------- | -------------- | ------------------------------------------------- |
| `control_dual_boiler.c`    | Dual Boiler    | Two independent PIDs, all heating strategies      |
| `control_single_boiler.c`  | Single Boiler  | One PID, mode switching (brew↔steam), auto-return |
| `control_heat_exchanger.c` | Heat Exchanger | Steam PID only, brew water is passive             |

**Heating Strategy Enforcement:**

```c
// control_common.c
bool control_set_heating_strategy(uint8_t strategy) {
    // Non-dual-boiler machines only support BREW_ONLY (single SSR)
    if (features->type != MACHINE_TYPE_DUAL_BOILER) {
        if (strategy != HEAT_BREW_ONLY) return false;
    }
    // ...
}
```

### State Machine (`state.c`)

The state machine adapts temperature checking based on machine type:

```c
// Get the correct temperature for "ready" detection
static float get_brew_temp_for_machine(float brew_temp, float group_temp) {
    if (machine_type == MACHINE_TYPE_HEAT_EXCHANGER) {
        return group_temp;  // HX has no brew NTC, use group TC
    }
    return brew_temp;  // Dual/Single boiler use brew NTC
}
```

### Safety System (`safety.c`)

Safety checks are machine-type aware to avoid false alarms:

| Check             | Dual Boiler | Single Boiler | Heat Exchanger |
| ----------------- | :---------: | :-----------: | :------------: |
| Brew NTC fault    |      ✓      |       ✓       | ✗ (no sensor)  |
| Steam NTC fault   |      ✓      | ✗ (no sensor) |       ✓        |
| Brew SSR monitor  |      ✓      |       ✓       |   ✗ (no SSR)   |
| Steam SSR monitor |      ✓      |  ✗ (no SSR)   |       ✓        |
| Steam level probe |      ✓      | ✗ (no probe)  |       ✓        |

### Water Management (`water_management.c`)

Steam boiler auto-fill is skipped for machines without a steam level probe:

```c
void water_management_update(void) {
    // Skip for single boiler (no steam level probe)
    if (!features->has_steam_level_probe) return;
    // ...
}
```

### Sensors (`sensors.c`)

Sensors use pin configuration to gracefully handle missing sensors:

- Returns `NAN` for unconfigured NTC sensors
- Returns `0.0f` for unconfigured pressure sensor
- Pin validation prevents reads on invalid GPIOs

---

## Configuration Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      CONFIGURATION HIERARCHY                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │  COMPILE-TIME (machine_xxx.h)                                        │   │
│   │  ─────────────────────────────                                       │   │
│   │  • Machine type enum                                                 │   │
│   │  • Pin mappings (GPIO numbers)                                       │   │
│   │  • Feature flags (sensors/outputs present)                           │   │
│   │  • Safety thresholds (max temperatures)                              │   │
│   │  • Sensor characteristics (NTC B-value, etc.)                        │   │
│   │                                                                       │   │
│   │  WHY COMPILE-TIME:                                                   │   │
│   │  • Compiler optimizes out unused code paths                          │   │
│   │  • Smaller binary                                                    │   │
│   │  • No runtime validation needed                                      │   │
│   │  • Easier to audit for safety                                        │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                         │
│                                    ▼                                         │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │  RUNTIME (flash storage)                                             │   │
│   │  ───────────────────────                                             │   │
│   │  • PID gains (Kp, Ki, Kd)                                            │   │
│   │  • Temperature setpoints                                             │   │
│   │  • Pre-infusion timing                                               │   │
│   │  • Calibration offsets                                               │   │
│   │  • User preferences                                                  │   │
│   │                                                                       │   │
│   │  WHY RUNTIME:                                                        │   │
│   │  • Tune without reflashing                                           │   │
│   │  • User adjustable                                                   │   │
│   │  • Machine-specific calibration                                      │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Data Structures

### machine_type_t

```c
typedef enum {
    MACHINE_DUAL_BOILER      = 0x01,
    MACHINE_SINGLE_BOILER    = 0x02,
    MACHINE_HEAT_EXCHANGER   = 0x03,
    MACHINE_THERMOBLOCK      = 0x04,
} machine_type_t;
```

### machine_features_t

```c
typedef struct {
    // Identity
    machine_type_t type;
    const char*    name;
    const char*    variant;         // e.g., "V1", "2024 model"

    // Boiler configuration
    uint8_t  num_boilers;           // 0, 1, or 2
    bool     has_brew_boiler;
    bool     has_steam_boiler;
    bool     is_heat_exchanger;

    // Temperature sensors
    bool     has_brew_ntc;
    bool     has_steam_ntc;
    bool     has_group_thermocouple;
    bool     has_inlet_temp;        // Water inlet temperature

    // Pressure & flow
    bool     has_pressure_sensor;
    bool     has_flow_meter;
    bool     has_scale;             // Integrated scale

    // Water system
    bool     has_reservoir_switch;
    bool     has_tank_level;
    bool     has_steam_level_probe;
    bool     has_auto_fill;         // Plumbed with solenoid fill
    bool     has_drain_solenoid;

    // Outputs
    bool     has_pump;
    bool     has_brew_solenoid;     // 3-way valve
    bool     has_water_led;
    bool     has_line_pressure_valve; // OPV bypass
    uint8_t  num_ssrs;              // 0, 1, or 2

    // Control modes
    bool     supports_preinfusion;
    bool     supports_pressure_profiling;
    bool     supports_flow_control;

} machine_features_t;
```

### pin_config_t

```c
// Use -1 for pins that don't exist on this machine
typedef struct {
    // Analog inputs (ADC)
    int8_t  adc_brew_ntc;
    int8_t  adc_steam_ntc;
    int8_t  adc_pressure;
    int8_t  adc_flow;               // If flow meter is analog
    int8_t  adc_inlet_temp;

    // SPI (thermocouple)
    int8_t  spi_miso;
    int8_t  spi_cs_thermocouple;
    int8_t  spi_sck;

    // Digital inputs
    int8_t  input_reservoir;
    int8_t  input_tank_level;
    int8_t  input_steam_level;
    int8_t  input_brew_switch;
    int8_t  input_steam_switch;     // For single boiler mode switch
    int8_t  input_flow_pulse;       // If flow meter is pulse output

    // Relay outputs
    int8_t  relay_pump;
    int8_t  relay_brew_solenoid;
    int8_t  relay_water_led;
    int8_t  relay_fill_solenoid;    // Auto-fill
    int8_t  relay_spare;

    // SSR outputs
    int8_t  ssr_brew;
    int8_t  ssr_steam;

    // User interface
    int8_t  led_status;
    int8_t  buzzer;

    // Communication
    int8_t  uart_esp32_tx;
    int8_t  uart_esp32_rx;
    int8_t  uart_pzem_tx;
    int8_t  uart_pzem_rx;
    int8_t  i2c_sda;
    int8_t  i2c_scl;

} pin_config_t;
```

### safety_config_t

```c
typedef struct {
    // Temperature limits (°C)
    float    brew_max_temp;
    float    steam_max_temp;
    float    group_max_temp;

    // Hysteresis for re-enable (°C below max)
    float    temp_hysteresis;

    // Timeout for SSR without temp change (seconds)
    uint16_t heater_timeout_s;

    // Maximum SSR duty cycle (%)
    uint8_t  max_ssr_duty;

    // Watchdog timeout (ms)
    uint16_t watchdog_timeout_ms;

} safety_config_t;
```

### sensor_config_t

```c
typedef struct {
    // NTC parameters (brew)
    float    brew_ntc_r25;          // Resistance at 25°C
    float    brew_ntc_b;            // B-value
    float    brew_ntc_r_series;     // Series resistor

    // NTC parameters (steam)
    float    steam_ntc_r25;
    float    steam_ntc_b;
    float    steam_ntc_r_series;

    // Pressure transducer
    float    pressure_min_v;        // Voltage at 0 bar
    float    pressure_max_v;        // Voltage at max bar
    float    pressure_max_bar;      // Max pressure reading
    float    pressure_divider;      // Voltage divider ratio

} sensor_config_t;
```

### defaults_config_t

All tunable parameters with sensible defaults per machine type.

```c
typedef struct {
    // ═══════════════════════════════════════════════════════════════
    // TEMPERATURE SETPOINTS
    // ═══════════════════════════════════════════════════════════════
    float    brew_setpoint;         // °C, default brew temperature
    float    steam_setpoint;        // °C, default steam temperature
    float    hot_water_setpoint;    // °C, hot water dispense temp (if applicable)

    // ═══════════════════════════════════════════════════════════════
    // TEMPERATURE OFFSETS (for display vs control)
    // ═══════════════════════════════════════════════════════════════
    float    brew_offset;           // Offset between boiler and group
    float    steam_offset;          // Calibration offset

    // ═══════════════════════════════════════════════════════════════
    // PID GAINS - BREW BOILER
    // ═══════════════════════════════════════════════════════════════
    float    brew_kp;
    float    brew_ki;
    float    brew_kd;

    // ═══════════════════════════════════════════════════════════════
    // PID GAINS - STEAM BOILER
    // ═══════════════════════════════════════════════════════════════
    float    steam_kp;
    float    steam_ki;
    float    steam_kd;

    // ═══════════════════════════════════════════════════════════════
    // HEATING STRATEGY
    // ═══════════════════════════════════════════════════════════════
    heating_config_t heating;

    // ═══════════════════════════════════════════════════════════════
    // PRE-INFUSION
    // ═══════════════════════════════════════════════════════════════
    bool     preinfusion_enabled;
    uint16_t preinfusion_time_ms;   // Pump ON time
    uint16_t preinfusion_pause_ms;  // Pause before full pressure
    uint8_t  preinfusion_pressure;  // Target pressure % (if flow control)

    // ═══════════════════════════════════════════════════════════════
    // BREW SETTINGS
    // ═══════════════════════════════════════════════════════════════
    uint16_t shot_time_target_ms;   // Target shot time (for display)
    uint16_t post_brew_purge_ms;    // Solenoid release delay

    // ═══════════════════════════════════════════════════════════════
    // STANDBY / ECO MODE
    // ═══════════════════════════════════════════════════════════════
    uint16_t standby_timeout_min;   // Minutes of inactivity → standby (0=disabled)
    float    standby_brew_temp;     // Reduced brew temp in standby
    float    standby_steam_temp;    // Reduced steam temp in standby (or 0=off)

    // ═══════════════════════════════════════════════════════════════
    // SINGLE BOILER SPECIFIC
    // ═══════════════════════════════════════════════════════════════
    float    sb_brew_setpoint;      // Brew mode setpoint
    float    sb_steam_setpoint;     // Steam mode setpoint
    uint16_t sb_mode_switch_delay_ms; // Delay when switching modes

} defaults_config_t;
```

### machine_config_t (Complete)

```c
typedef struct {
    machine_features_t      features;   // Hardware capabilities
    pin_config_t            pins;       // GPIO pin assignments
    safety_config_t         safety;     // Safety thresholds
    sensor_config_t         sensors;    // Sensor characteristics
    machine_electrical_t    electrical; // Machine electrical specs (heater power ratings)
    defaults_config_t       defaults;   // Default operating parameters
} machine_config_t;
```

**Important for Multi-Machine Support:**

- Each machine type (ECM Synchronika, Rancilio Silvia, etc.) has its own `machine_config_t`
- Machine-specific specs include: heater power ratings, pin mappings, sensor characteristics, safety limits
- Environmental configuration (voltage, current limits) is separate and works with any machine type
- The same environmental config can be used with different machine types - system automatically adapts

---

## Runtime Configuration

Parameters that can be changed at runtime (stored in flash):

```c
typedef struct {
    uint32_t magic;                 // 0xECM00001 (version marker)
    uint32_t version;               // Config structure version

    // ═══════════════════════════════════════════════════════════════
    // TEMPERATURE (override defaults)
    // ═══════════════════════════════════════════════════════════════
    float    brew_setpoint;
    float    steam_setpoint;
    float    brew_offset;
    float    steam_offset;

    // ═══════════════════════════════════════════════════════════════
    // PID GAINS (override defaults)
    // ═══════════════════════════════════════════════════════════════
    float    brew_kp, brew_ki, brew_kd;
    float    steam_kp, steam_ki, steam_kd;

    // ═══════════════════════════════════════════════════════════════
    // HEATING STRATEGY (override default)
    // ═══════════════════════════════════════════════════════════════
    heating_config_t heating;

    // ═══════════════════════════════════════════════════════════════
    // PRE-INFUSION (override defaults)
    // ═══════════════════════════════════════════════════════════════
    bool     preinfusion_enabled;
    uint16_t preinfusion_time_ms;
    uint16_t preinfusion_pause_ms;

    // ═══════════════════════════════════════════════════════════════
    // ECO MODE
    // ═══════════════════════════════════════════════════════════════
    uint16_t standby_timeout_min;
    float    standby_brew_temp;
    float    standby_steam_temp;

    // ═══════════════════════════════════════════════════════════════
    // FLAGS
    // ═══════════════════════════════════════════════════════════════
    uint8_t  flags;                 // Bitmask of enabled features

    // CRC32 for validation
    uint32_t crc;

} runtime_config_t;

// Runtime config flags
#define CONFIG_FLAG_USE_RUNTIME_TEMPS    (1 << 0)  // Use runtime temps vs defaults
#define CONFIG_FLAG_USE_RUNTIME_PID      (1 << 1)  // Use runtime PID vs defaults
#define CONFIG_FLAG_USE_RUNTIME_HEATING  (1 << 2)  // Use runtime heating strategy
#define CONFIG_FLAG_USE_RUNTIME_PREINF   (1 << 3)  // Use runtime pre-infusion
```

### Config Loading Priority

```c
// Get effective setpoint (runtime overrides default)
float get_brew_setpoint(void) {
    if (runtime_config.flags & CONFIG_FLAG_USE_RUNTIME_TEMPS) {
        return runtime_config.brew_setpoint;
    }
    return machine->defaults.brew_setpoint;
}

// Get effective heating strategy
heating_strategy_t get_heating_strategy(void) {
    if (runtime_config.flags & CONFIG_FLAG_USE_RUNTIME_HEATING) {
        return runtime_config.heating.strategy;
    }
    return machine->defaults.heating.strategy;
}
```

### Flash Storage & Wear Leveling

**⚠️ Important:** The current implementation writes configuration directly to a single flash sector without wear leveling. Each save operation erases and rewrites the entire 4KB sector.

**Flash Endurance Considerations:**

- Typical Pico flash: 10,000-100,000 erase/write cycles per sector
- Current implementation: Direct sector writes (no wear leveling)
- **Risk:** Frequent saves (e.g., after every PID auto-tune or mode change) could exhaust flash endurance

**Current Mitigation:**

- Configuration is only saved when explicitly changed (not on every update)
- Statistics module uses a save interval (every 10 brews) to reduce writes
- CRC32 validation detects corrupted data

**Future Enhancement (Recommended):**

- Implement wear leveling using a circular buffer across multiple sectors
- Or use a filesystem library (LittleFS) that handles wear leveling automatically
- Consider EEPROM or external flash for high-write scenarios

---

## Machine Definitions

### ECM Synchronika (Dual Boiler)

```c
// machines/ecm_synchronika.h

#ifndef MACHINE_ECM_SYNCHRONIKA_H
#define MACHINE_ECM_SYNCHRONIKA_H

#include "machine_config.h"

static const machine_config_t MACHINE_ECM_SYNCHRONIKA = {
    .features = {
        .type                   = MACHINE_DUAL_BOILER,
        .name                   = "ECM Synchronika",
        .variant                = "Custom Control Board v1",

        .num_boilers            = 2,
        .has_brew_boiler        = true,
        .has_steam_boiler       = true,
        .is_heat_exchanger      = false,

        .has_brew_ntc           = true,
        .has_steam_ntc          = true,
        .has_group_thermocouple = true,
        .has_inlet_temp         = false,

        .has_pressure_sensor    = true,
        .has_flow_meter         = false,
        .has_scale              = false,

        .has_reservoir_switch   = true,
        .has_tank_level         = true,
        .has_steam_level_probe  = true,
        .has_auto_fill          = false,
        .has_drain_solenoid     = false,

        .has_pump               = true,
        .has_brew_solenoid      = true,
        .has_water_led          = true,
        .has_line_pressure_valve = false,
        .num_ssrs               = 2,

        .supports_preinfusion   = true,
        .supports_pressure_profiling = false,
        .supports_flow_control  = false,
    },

    .pins = {
        // Analog inputs
        .adc_brew_ntc           = 26,
        .adc_steam_ntc          = 27,
        .adc_pressure           = 28,
        .adc_flow               = -1,
        .adc_inlet_temp         = -1,

        // SPI
        .spi_miso               = 16,
        .spi_cs_thermocouple    = 17,
        .spi_sck                = 18,

        // Digital inputs
        .input_reservoir        = 2,
        .input_tank_level       = 3,
        .input_steam_level      = 4,
        .input_brew_switch      = 5,
        .input_steam_switch     = -1,
        .input_flow_pulse       = -1,

        // Relay outputs
        .relay_pump             = 11,
        .relay_brew_solenoid    = 12,
        .relay_water_led        = 10,
        .relay_fill_solenoid    = -1,
        .relay_spare            = 20,

        // SSR outputs
        .ssr_brew               = 13,
        .ssr_steam              = 14,

        // UI
        .led_status             = 15,
        .buzzer                 = 19,

        // Communication
        .uart_esp32_tx          = 0,
        .uart_esp32_rx          = 1,
        .uart_pzem_tx           = 6,
        .uart_pzem_rx           = 7,
        .i2c_sda                = 8,
        .i2c_scl                = 9,
    },

    .safety = {
        .brew_max_temp          = 130.0f,
        .steam_max_temp         = 165.0f,
        .group_max_temp         = 110.0f,
        .temp_hysteresis        = 10.0f,
        .heater_timeout_s       = 60,
        .max_ssr_duty           = 95,
        .watchdog_timeout_ms    = 2000,
    },

    .sensors = {
        // NTC: 3.3kΩ @ 25°C, B=3950
        .brew_ntc_r25           = 3300.0f,
        .brew_ntc_b             = 3950.0f,
        .brew_ntc_r_series      = 3300.0f,

        .steam_ntc_r25          = 3300.0f,
        .steam_ntc_b            = 3950.0f,
        .steam_ntc_r_series     = 3300.0f,

        // YD4060: 0.5-4.5V, 0-16 bar, divider 0.6
        .pressure_min_v         = 0.5f,
        .pressure_max_v         = 4.5f,
        .pressure_max_bar       = 16.0f,
        .pressure_divider       = 0.6f,
    },

    .electrical = {
        .nominal_voltage           = 230,      // 230V (Europe/Israel)
        .max_current_draw           = 16.0f,   // 16A limit (typical for 16A breaker)
        .brew_heater_power          = 1500,     // ECM Synchronika brew boiler (actual spec)
        .steam_heater_power         = 1000,     // ECM Synchronika steam boiler (actual spec)
        .brew_heater_current        = 6.5f,    // Auto-calculated: 1500W / 230V
        .steam_heater_current       = 4.3f,    // Auto-calculated: 1000W / 230V
        .max_combined_current       = 15.2f,   // Auto-calculated: 16A * 0.95
    },

    .defaults = {
        // Temperature setpoints
        .brew_setpoint          = 93.0f,
        .steam_setpoint         = 145.0f,
        .hot_water_setpoint     = 85.0f,

        // Calibration offsets
        .brew_offset            = 0.0f,
        .steam_offset           = 0.0f,

        // PID gains - brew (tune for E61 thermal mass)
        .brew_kp                = 2.0f,
        .brew_ki                = 0.05f,
        .brew_kd                = 1.0f,

        // PID gains - steam
        .steam_kp               = 3.0f,
        .steam_ki               = 0.08f,
        .steam_kd               = 1.5f,

        // Heating strategy - parallel safe for 16A (10.9A total < 16A)
        .heating = {
            .strategy               = HEAT_PARALLEL,  // Both can run simultaneously at 230V/16A
            .sequential_threshold_pct = 85,   // For sequential mode (if needed)
            .max_combined_duty      = 150,    // For stagger mode (if needed)
            .stagger_priority       = 0,      // Brew priority
        },

        // Pre-infusion
        .preinfusion_enabled    = true,
        .preinfusion_time_ms    = 3000,
        .preinfusion_pause_ms   = 2000,
        .preinfusion_pressure   = 50,     // 50% if flow control available

        // Brew settings
        .shot_time_target_ms    = 28000,  // 28 second target
        .post_brew_purge_ms     = 2000,   // 2s solenoid delay

        // Standby / eco mode
        .standby_timeout_min    = 30,     // 30 min → standby
        .standby_brew_temp      = 60.0f,  // Keep warm
        .standby_steam_temp     = 0.0f,   // Steam off in standby

        // Single boiler (not applicable for dual)
        .sb_brew_setpoint       = 0.0f,
        .sb_steam_setpoint      = 0.0f,
        .sb_mode_switch_delay_ms = 0,
    },
};

#endif // MACHINE_ECM_SYNCHRONIKA_H
```

### Rancilio Silvia (Single Boiler)

```c
// machines/rancilio_silvia.h

static const machine_config_t MACHINE_RANCILIO_SILVIA = {
    .features = {
        .type                   = MACHINE_SINGLE_BOILER,
        .name                   = "Rancilio Silvia",
        .variant                = "v6 with PID",

        .num_boilers            = 1,
        .has_brew_boiler        = true,
        .has_steam_boiler       = false,  // Same boiler
        .is_heat_exchanger      = false,

        .has_brew_ntc           = true,
        .has_steam_ntc          = false,  // Only one sensor
        .has_group_thermocouple = true,
        .has_inlet_temp         = false,

        .has_pressure_sensor    = false,  // Optional add-on
        .has_flow_meter         = false,
        .has_scale              = false,

        .has_reservoir_switch   = true,
        .has_tank_level         = false,  // Simple float only
        .has_steam_level_probe  = false,  // No steam boiler
        .has_auto_fill          = false,
        .has_drain_solenoid     = false,

        .has_pump               = true,
        .has_brew_solenoid      = true,
        .has_water_led          = false,
        .has_line_pressure_valve = false,
        .num_ssrs               = 1,

        .supports_preinfusion   = true,
        .supports_pressure_profiling = false,
        .supports_flow_control  = false,
    },

    .pins = {
        .adc_brew_ntc           = 26,
        .adc_steam_ntc          = -1,     // Not present
        .adc_pressure           = -1,     // Not present
        // ... etc

        .ssr_brew               = 13,
        .ssr_steam              = -1,     // Same SSR, controlled by mode
        // ... etc
    },

    .safety = {
        .brew_max_temp          = 130.0f,
        .steam_max_temp         = 155.0f,  // Same boiler, different limit for steam mode
        .group_max_temp         = 105.0f,
        .temp_hysteresis        = 10.0f,
        .heater_timeout_s       = 60,
        .max_ssr_duty           = 95,
        .watchdog_timeout_ms    = 2000,
    },

    .sensors = {
        .brew_ntc_r25           = 100000.0f,  // Silvia uses 100K NTC
        .brew_ntc_b             = 3950.0f,
        .brew_ntc_r_series      = 100000.0f,
        // Steam uses same sensor
        .steam_ntc_r25          = 0,
        .steam_ntc_b            = 0,
        .steam_ntc_r_series     = 0,
        // No pressure sensor by default
        .pressure_min_v         = 0,
        .pressure_max_v         = 0,
        .pressure_max_bar       = 0,
        .pressure_divider       = 0,
    },

    .defaults = {
        // Single boiler setpoints (mode-switched)
        .brew_setpoint          = 0.0f,   // Use sb_brew_setpoint
        .steam_setpoint         = 0.0f,   // Use sb_steam_setpoint
        .hot_water_setpoint     = 0.0f,

        .brew_offset            = -10.0f, // Silvia runs hot, offset for group temp
        .steam_offset           = 0.0f,

        // PID gains - smaller boiler, faster response
        .brew_kp                = 4.0f,
        .brew_ki                = 0.1f,
        .brew_kd                = 2.0f,

        // Same PID for steam mode (same boiler)
        .steam_kp               = 4.0f,
        .steam_ki               = 0.1f,
        .steam_kd               = 2.0f,

        // Heating - single boiler, brew only by default
        .heating = {
            .strategy               = HEAT_BREW_ONLY,  // Steam is manual mode switch
            .sequential_threshold_pct = 100,
            .max_combined_duty      = 100,
            .stagger_priority       = 0,
        },

        // Pre-infusion
        .preinfusion_enabled    = false,  // No solenoid on basic Silvia
        .preinfusion_time_ms    = 0,
        .preinfusion_pause_ms   = 0,
        .preinfusion_pressure   = 0,

        // Brew settings
        .shot_time_target_ms    = 25000,
        .post_brew_purge_ms     = 0,      // No 3-way valve

        // Standby
        .standby_timeout_min    = 30,
        .standby_brew_temp      = 50.0f,
        .standby_steam_temp     = 0.0f,

        // SINGLE BOILER SPECIFIC - these are used instead of brew/steam setpoints
        .sb_brew_setpoint       = 93.0f,   // Brew mode temperature
        .sb_steam_setpoint      = 140.0f,  // Steam mode temperature
        .sb_mode_switch_delay_ms = 5000,   // 5s delay when switching modes
    },
};
```

### Generic Heat Exchanger

```c
// machines/generic_hx.h

static const machine_config_t MACHINE_GENERIC_HX = {
    .features = {
        .type                   = MACHINE_HEAT_EXCHANGER,
        .name                   = "Generic E61 HX",
        .variant                = "Standard",

        .num_boilers            = 1,
        .has_brew_boiler        = false,  // Passive HX
        .has_steam_boiler       = true,
        .is_heat_exchanger      = true,

        .has_brew_ntc           = false,  // No brew boiler sensor
        .has_steam_ntc          = true,
        .has_group_thermocouple = true,   // Important for HX!
        // ... etc

        .num_ssrs               = 1,      // Steam boiler only
    },

    // ... etc
};
```

---

## Build System Integration

### CMakeLists.txt

The build system supports building individual machine types or all types at once:

```cmake
# Build single machine type (default)
cmake .. -DMACHINE_TYPE=DUAL_BOILER
make

# Build all machine types
cmake .. -DBUILD_ALL_MACHINES=ON
make
```

**Key features:**

- Automatically selects the correct control implementation file
- Creates separate binaries for each machine type
- No unused code paths in the binary

### How It Works

```cmake
# Machine-specific control files
set(CONTROL_DUAL_BOILER src/control_dual_boiler.c)
set(CONTROL_SINGLE_BOILER src/control_single_boiler.c)
set(CONTROL_HEAT_EXCHANGER src/control_heat_exchanger.c)

# Function to create executable for a machine type
function(create_machine_executable MACHINE_TYPE_NAME)
    string(TOLOWER ${MACHINE_TYPE_NAME} MACHINE_TYPE_LOWER)
    set(TARGET_NAME "brewos_${MACHINE_TYPE_LOWER}")

    # Select control implementation
    if(MACHINE_TYPE_NAME STREQUAL "DUAL_BOILER")
        set(CONTROL_IMPL ${CONTROL_DUAL_BOILER})
    elseif(MACHINE_TYPE_NAME STREQUAL "SINGLE_BOILER")
        set(CONTROL_IMPL ${CONTROL_SINGLE_BOILER})
    elseif(MACHINE_TYPE_NAME STREQUAL "HEAT_EXCHANGER")
        set(CONTROL_IMPL ${CONTROL_HEAT_EXCHANGER})
    endif()

    add_executable(${TARGET_NAME}
        ${COMMON_SOURCES}
        ${CONTROL_IMPL}
    )

    target_compile_definitions(${TARGET_NAME} PRIVATE
        MACHINE_TYPE=${MACHINE_TYPE_NAME}
    )
endfunction()
```

### Machine Configuration Selection (machine_config.c)

```c
void machine_config_init(void) {
    // Select machine configuration at compile time
    #if defined(MACHINE_DUAL_BOILER)
        g_machine_config = &MACHINE_ECM_SYNCHRONIKA;
    #elif defined(MACHINE_SINGLE_BOILER)
        g_machine_config = &MACHINE_RANCILIO_SILVIA;
    #elif defined(MACHINE_HEAT_EXCHANGER)
        g_machine_config = &MACHINE_GENERIC_HX;
    #else
        #error "No machine type defined!"
    #endif
}
```

---

## Control Code Architecture

### Common vs Machine-Specific Code

The control system is split into shared and machine-specific implementations:

```c
// control_common.c - Shared by all machine types
void control_init(void) {
    pid_init(&g_brew_pid, DEFAULT_BREW_TEMP / 10.0f);
    pid_init(&g_steam_pid, DEFAULT_STEAM_TEMP / 10.0f);
    init_hardware_outputs();

    control_init_machine();  // Call machine-specific initialization
}

void control_update(void) {
    if (safety_is_safe_state()) { /* ensure outputs off */ }

    // Read sensors, compute dt
    // ...

    float brew_duty = 0.0f, steam_duty = 0.0f;
    control_update_machine(mode, brew_temp, steam_temp, group_temp, dt,
                          &brew_duty, &steam_duty);

    apply_hardware_outputs(brew_duty, steam_duty, pump);
}
```

### Machine-Specific Implementations

Each machine type implements these functions (from `control_impl.h`):

```c
// control_dual_boiler.c
void control_init_machine(void) {
    g_brew_pid.setpoint = DEFAULT_BREW_TEMP / 10.0f;
    g_steam_pid.setpoint = DEFAULT_STEAM_TEMP / 10.0f;
    g_heating_strategy = HEAT_SEQUENTIAL;
}

void control_update_machine(mode, brew_temp, steam_temp, group_temp, dt,
                           brew_duty, steam_duty) {
    float brew_demand = pid_compute(&g_brew_pid, brew_temp, dt);
    float steam_demand = pid_compute(&g_steam_pid, steam_temp, dt);
    apply_heating_strategy(brew_demand, steam_demand, ...);
}
```

```c
// control_single_boiler.c
void control_init_machine(void) {
    g_brew_pid.setpoint = sb_config->brew_setpoint;
    g_sb_mode = SB_MODE_BREW;
}

void control_update_machine(mode, brew_temp, ..., brew_duty, steam_duty) {
    handle_mode_switch(mode);  // Brew ↔ Steam mode switching
    *brew_duty = pid_compute(&g_brew_pid, brew_temp, dt);
    *steam_duty = 0.0f;  // Single boiler uses one SSR
}
```

```c
// control_heat_exchanger.c
void control_init_machine(void) {
    g_steam_pid.setpoint = hx_config->steam_setpoint;
}

void control_update_machine(mode, brew_temp, steam_temp, ..., brew_duty, steam_duty) {
    *brew_duty = 0.0f;  // No active brew heater (passive HX)
    *steam_duty = pid_compute(&g_steam_pid, steam_temp, dt);
}
```

### Compile-Time Optimization

```c
// interlocks.c - Compiler removes unused checks

bool check_interlocks(const machine_config_t* cfg) {

    // Steam level check (only if probe exists)
    if (cfg->features.has_steam_level_probe) {
        if (gpio_get(cfg->pins.input_steam_level)) {
            return false;  // Level low, interlock triggered
        }
    }

    // Tank level check (only if sensor exists)
    if (cfg->features.has_tank_level) {
        if (gpio_get(cfg->pins.input_tank_level)) {
            return false;
        }
    }

    // These branches are eliminated by compiler if feature is false
    // (since cfg is const and known at compile time)

    return true;
}
```

---

## Adding a New Machine

1. **Create control implementation:** `src/control_my_machine.c`

   - Implement `control_init_machine()` - machine-specific initialization
   - Implement `control_update_machine()` - machine-specific control loop
   - Implement `control_get_machine_mode()` / `control_is_machine_switching()`

2. **Add machine configuration:** In `src/machine_config.c`

   - Define `static const machine_config_t MACHINE_MY_MACHINE = {...}`
   - Add `#elif defined(MACHINE_MY_MACHINE)` block in `machine_config_init()`

3. **Update CMakeLists.txt:**

   ```cmake
   # Add to VALID_MACHINE_TYPES
   set(VALID_MACHINE_TYPES "DUAL_BOILER" "SINGLE_BOILER" "HEAT_EXCHANGER" "MY_MACHINE")

   # Add control file variable
   set(CONTROL_MY_MACHINE src/control_my_machine.c)

   # Add to create_machine_executable function
   elseif(MACHINE_TYPE_NAME STREQUAL "MY_MACHINE")
       set(CONTROL_IMPL ${CONTROL_MY_MACHINE})
   ```

4. **Build and test:**
   ```bash
   cmake .. -DMACHINE_TYPE=MY_MACHINE
   make
   # Output: brewos_my_machine.uf2
   ```

---

**Note:** See the "Runtime Configuration" section above (line 901) for the complete runtime configuration structure and implementation details.
