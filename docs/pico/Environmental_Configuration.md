# Environmental Configuration

## Overview

Environmental configuration defines installation-specific parameters that vary by location, independent of the machine hardware. This allows the same machine firmware to work in different electrical environments (different voltages, circuit capacities, etc.).

**Key Principle:**
- **Machine Configuration** = Hardware-specific, fixed per machine model (see [Machine_Configurations.md](Machine_Configurations.md))
- **Environmental Configuration** = Installation-specific, varies by location (this document)
- **Same environmental config works with different machine types** - System automatically adapts

---

## Electrical Environment

### Voltage Standards

| Region | Nominal Voltage | Common Range | Notes |
|--------|----------------|--------------|-------|
| **USA** | 120V | 110-125V | Standard residential |
| **Europe** | 230V | 220-240V | EU standard (harmonized) |
| **UK** | 230V | 220-240V | Same as EU (post-harmonization) |
| **Israel** | 230V | 220-240V | Same as Europe |
| **Australia** | 230V | 220-240V | Same as Europe |
| **Japan** | 100V | 100-110V | Unique standard |

### Circuit Breaker Ratings by Region

| Region | Common Ratings | Typical Use | Max Continuous Load (80% rule) |
|--------|---------------|-------------|-------------------------------|
| **Israel** | 10A, 16A | Residential | 10A: 1.84kW, 16A: 2.94kW @ 230V |
| **Europe** | 10A, 16A, 20A | Residential | 10A: 1.84kW, 16A: 2.94kW, 20A: 3.68kW @ 230V |
| **USA** | 15A, 20A | Residential | 15A: 1.44kW, 20A: 1.92kW @ 120V |
| **UK** | 13A (fused plug) | Residential | 13A: 2.99kW @ 230V (via BS 1363 plug) |

**Note:** The 80% rule (NEC/CE) means continuous loads should not exceed 80% of breaker rating.

### Current Limit Recommendations

The `max_current_draw` value should be set based on your actual circuit capacity:

- **10A**: Typical for 10A or 13A breakers (with safety margin)
- **12A**: Typical for 15A breakers (80% rule)
- **16A**: Typical for 16A or 20A breakers (with safety margin)
- **20A**: For dedicated circuits with higher capacity

---

## Environmental Electrical Configuration Structure

```c
typedef struct {
    // ═══════════════════════════════════════════════════════════════
    // ENVIRONMENTAL ELECTRICAL CONFIGURATION (Installation-specific)
    // ═══════════════════════════════════════════════════════════════
    
    // Nominal voltage (V) - from local electrical supply
    uint16_t nominal_voltage;        // 120, 230, 240, etc.
    
    // Maximum current draw limit (A) - from circuit breaker/installation
    // Set this to your circuit's safe limit (typically 10A or 16A)
    float    max_current_draw;       // e.g., 10.0f, 16.0f
    
    // Calculated values (computed from machine + environment)
    float    brew_heater_current;    // Calculated: machine.brew_heater_power / nominal_voltage
    float    steam_heater_current;   // Calculated: machine.steam_heater_power / nominal_voltage
    float    max_combined_current;   // Calculated: max_current_draw * 0.95 (5% safety margin)
    
} environmental_electrical_t;
```

**Environmental electrical config:**
- Varies by installation location
- Can be set at compile-time or runtime
- May need adjustment if machine is moved
- Set `max_current_draw` directly (e.g., 10A or 16A)
- Works with any machine type - system calculates heater currents from machine config

---

## Complete Environmental Configuration

```c
// Environmental configuration (installation-specific, may vary by location)
typedef struct {
    environmental_electrical_t electrical;  // Voltage, current limits (varies by installation)
    // Future: temperature units, timezone, locale, etc.
} environmental_config_t;
```

---

## Example Configurations

### Typical Configurations by Region

**Israel/Europe (230V):**
- **10A limit**: Common residential circuit
- **16A limit**: Common residential circuit (recommended for dual-boiler)

**USA (120V):**
- **12A limit**: Typical for 15A breaker (80% rule)
- **16A limit**: Typical for 20A breaker (80% rule)

### Code Examples

```c
// Example 1: Israel/Europe 230V with 10A limit
// Works with ECM Synchronika, Rancilio Silvia, or any machine
static const environmental_electrical_t ENV_230V_10A = {
    .nominal_voltage    = 230,
    .max_current_draw   = 10.0f,  // Direct limit: 10A
    // Calculated values computed at runtime based on machine config:
    // For ECM Synchronika (1500W/1000W): brew=6.5A, steam=4.3A, total=10.9A
    // For Rancilio Silvia (1200W): single=5.2A (1200W/230V)
    // max_combined_current = 10.0A * 0.95 = 9.5A
};

// Example 2: Israel/Europe 230V with 16A limit
static const environmental_electrical_t ENV_230V_16A = {
    .nominal_voltage    = 230,
    .max_current_draw   = 16.0f,  // Direct limit: 16A
};

// Example 3: USA 120V with 12A limit
static const environmental_electrical_t ENV_120V_12A = {
    .nominal_voltage    = 120,
    .max_current_draw   = 12.0f,  // Direct limit: 12A
};

// Example 4: USA 120V with 16A limit
static const environmental_electrical_t ENV_120V_16A = {
    .nominal_voltage    = 120,
    .max_current_draw   = 16.0f,  // Direct limit: 16A
};
```

**Key Point:** The same environmental configuration works with different machine types. The system automatically calculates the appropriate heating strategy based on:
- Machine's heater power ratings (from machine config - see [Machine_Configurations.md](Machine_Configurations.md))
- Environmental voltage and current limits (from environmental config)

---

## Integration with Machine Configuration

The system combines machine configuration and environmental configuration at runtime:

```c
// Runtime electrical state (computed from machine + environment)
typedef struct {
    // From machine config
    uint16_t brew_heater_power;
    uint16_t steam_heater_power;
    
    // From environmental config
    uint16_t nominal_voltage;
    float    max_current_draw;
    
    // Calculated values
    float    brew_heater_current;
    float    steam_heater_current;
    float    max_combined_current;
    
} electrical_state_t;

// Initialize from machine + environment
void init_electrical_state(electrical_state_t* state, 
                          const machine_electrical_t* machine,
                          const environmental_electrical_t* env) {
    state->brew_heater_power = machine->brew_heater_power;
    state->steam_heater_power = machine->steam_heater_power;
    state->nominal_voltage = env->nominal_voltage;
    state->max_current_draw = env->max_current_draw;
    
    // Calculate currents
    state->brew_heater_current = (float)machine->brew_heater_power / env->nominal_voltage;
    state->steam_heater_current = (float)machine->steam_heater_power / env->nominal_voltage;
    state->max_combined_current = env->max_current_draw * 0.95f;  // 5% safety margin
}
```

**How It Works:**
1. **Machine config** provides: Heater power ratings (e.g., 1500W brew, 1000W steam)
2. **Environmental config** provides: Voltage (e.g., 230V), current limit (e.g., 16A)
3. **System calculates**: Heater currents, combined limits, appropriate heating strategy
4. **Result**: Same environmental config works with different machine types

---

## Automatic Heating Strategy Selection

The firmware automatically selects the appropriate heating strategy based on combined electrical state:

```c
heating_strategy_t select_heating_strategy(const electrical_state_t* elec) {
    float total_current = elec->brew_heater_current + elec->steam_heater_current;
    
    // If both heaters at 100% exceed max current, need to limit
    if (total_current > elec->max_current_draw) {
        // Check if stagger can work (both at ~75% duty)
        if (total_current * 0.75 <= elec->max_combined_current) {
            return HEAT_SMART_STAGGER;  // Can stagger at ~75% each
        } else {
            return HEAT_SEQUENTIAL;     // Must heat sequentially
        }
    } else {
        return HEAT_PARALLEL;           // Can run both simultaneously
    }
}
```

See [Machine_Configurations.md](Machine_Configurations.md#heating-strategies) for details on heating strategies.

---

## Runtime Monitoring

Environmental configuration can be set at compile-time or runtime, and actual voltage/current can be monitored via PZEM power monitor:

```c
// Verify actual voltage matches environmental configuration
void verify_electrical_state(const electrical_state_t* state) {
    float measured_voltage = pzem_get_voltage();
    float nominal = state->nominal_voltage;
    
    // Allow ±10% tolerance
    if (measured_voltage < nominal * 0.9 || measured_voltage > nominal * 1.1) {
        log_warning("Voltage mismatch: configured=%.0fV, measured=%.1fV", 
                    nominal, measured_voltage);
    }
    
    // Monitor current draw against environmental limit
    float measured_current = pzem_get_current();
    if (measured_current > state->max_current_draw) {
        log_error("Current draw exceeds limit: %.2fA > %.2fA", 
                  measured_current, state->max_current_draw);
        // Reduce heating or enter safe mode
    }
    
    // Warn if approaching limit (90% threshold)
    if (measured_current > state->max_current_draw * 0.9) {
        log_warning("Current draw approaching limit: %.2fA / %.2fA", 
                    measured_current, state->max_current_draw);
    }
}
```

---

## Configuration Storage

Environmental configuration can be stored in flash (like runtime config) to persist across reboots:

```c
// Environmental config stored in flash
typedef struct {
    uint32_t magic;                 // 0xECM_ENV01
    uint32_t version;               // Config version
    
    uint16_t nominal_voltage;       // 120, 230, 240, etc.
    float    max_current_draw;      // 10.0, 16.0, etc.
    
    uint32_t crc;                   // CRC32 for validation
} environmental_config_flash_t;
```

This allows users to:
- Configure voltage/current limits via ESP32 web UI
- Save configuration to flash
- Machine adapts to different electrical installations
- Update configuration if machine is moved to different location

---

## Safety Considerations

1. **Set `max_current_draw` conservatively** - Use a value below your circuit breaker rating
   - For 10A breaker: Set to 10A (or 8A for extra safety)
   - For 15A breaker: Set to 12A (80% rule)
   - For 16A breaker: Set to 16A (or 12.8A for 80% rule)
   - For 20A breaker: Set to 16A (80% rule)
2. **Monitor actual current** - Use PZEM to verify the machine stays within limits
3. **Default to conservative strategy** - If in doubt, use HEAT_SEQUENTIAL
4. **Document the configuration** - Label the machine with voltage and current limit
5. **Consider other loads** - If other devices share the circuit, reduce `max_current_draw` accordingly

---

## Future Expansion

Environmental configuration can be extended to include:

- **Temperature units** - Celsius vs Fahrenheit
- **Timezone** - For scheduling and logging
- **Locale** - Language, date formats
- **Regulatory compliance** - Different safety standards by region
- **Power management** - Peak demand limits, time-of-use rates

---

## Related Documentation

- [Machine_Configurations.md](Machine_Configurations.md) - Machine-specific hardware configuration
- [Architecture.md](Architecture.md) - System architecture and control algorithms
- [Communication_Protocol.md](Communication_Protocol.md) - Protocol for ESP32 communication

