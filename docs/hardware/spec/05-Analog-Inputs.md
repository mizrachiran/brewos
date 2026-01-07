# Analog Input Circuits

## NTC Thermistor Interface

### Multi-Machine Compatibility (Jumper Selectable)

Different espresso machine brands use different NTC sensor values. **Solder jumpers JP1/JP2** allow switching between configurations.

| Machine Brand     | NTC @ 25°C | JP1 (Brew) | JP2 (Steam) | Effective R1 | Effective R2 |
| ----------------- | ---------- | ---------- | ----------- | ------------ | ------------ |
| **ECM, Profitec** | 50kΩ       | **OPEN**   | **OPEN**    | 3.3kΩ        | 1.2kΩ        |
| Rocket, Rancilio  | 10kΩ       | **CLOSED** | **CLOSED**  | ~1kΩ         | ~430Ω        |
| Gaggia            | 10kΩ       | **CLOSED** | **CLOSED**  | ~1kΩ         | ~430Ω        |
| Lelit (50kΩ)      | 50kΩ       | OPEN       | OPEN        | 3.3kΩ        | 1.2kΩ        |

**Default:** JP1/JP2 **OPEN** = ECM/Profitec (50kΩ NTC)

### Circuit Schematic

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                         NTC THERMISTOR INPUT CIRCUITS                           │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    BREW BOILER (GPIO26/ADC0)              STEAM BOILER (GPIO27/ADC1)           │
│    Target: 90-96°C                        Target: 125-150°C                    │
│                                                                                 │
│         ADC_VREF (3.0V)                        ADC_VREF (3.0V)                 │
│             │                                      │                           │
│        ┌────┴────┐                            ┌────┴────┐                      │
│        │  3.3kΩ  │  R1 (always populated)     │  1.2kΩ  │  R2 (always populated)
│        │  ±1%    │                            │  ±1%    │                      │
│        └────┬────┘                            └────┬────┘                      │
│             │                                      │                           │
│             ├────[1.5kΩ R1A]────[JP1]──┐          ├────[680Ω R2A]────[JP2]──┐ │
│             │                          │          │                          │ │
│             │  (JP1 adds R1A parallel) │          │  (JP2 adds R2A parallel) │ │
│             └──────────────────────────┘          └──────────────────────────┘ │
│             │                                      │                           │
│        ┌────┴────┐                            ┌────┴────┐                      │
│        │   NTC   │                            │   NTC   │                      │
│        │ 50k/10k │                            │ 50k/10k │                      │
│        └────┬────┘                            └────┬────┘                      │
│             │                                      │                           │
│        ┌────┴────┐                            ┌────┴────┐                      │
│        │  1kΩ R5 │  ADC protection            │  1kΩ R6 │  ADC protection      │
│        └────┬────┘                            └────┬────┘                      │
│             │                                      │                           │
│        ┌────┴────┐                            ┌────┴────┐                      │
│        │ 100nF   │  RC filter                 │ 100nF   │  RC filter           │
│        │  C8     │  (DC blocking)             │  C9     │  (DC blocking)       │
│        └────┬────┘                            └────┬────┘                      │
│             │                                      │                           │
│        ┌────┴────┐                            ┌────┴────┐                      │
│        │ 10kΩ    │  Mains hum filter          │ 10kΩ    │  Mains hum filter    │
│        │  R_HUM  │  (fc ≈ 1.6kHz)             │  R_HUM  │  (fc ≈ 1.6kHz)       │
│        └────┬────┘                            └────┬────┘                      │
│             │                                      │                           │
│        ┌────┴────┐                            ┌────┴────┐                      │
│        │ 10nF    │  Low-pass filter           │ 10nF    │  Low-pass filter     │
│        │  C_HUM  │  (50/60Hz rejection)        │  C_HUM  │  (50/60Hz rejection) │
│        └────┬────┘                            └────┬────┘                      │
│             │                                      │                           │
│            GND                           ──────► GPIO26/ADC0    GPIO27/ADC1    │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

**⚠️ CRITICAL: Mains Hum Filtering**

Sensor wires run parallel to high-voltage heater wires inside the machine chassis, picking up 50/60Hz mains hum that must be filtered before the ADC.

**Filter Design:**

- **R_HUM:** 10kΩ series resistor (limits current, forms RC filter with C_HUM)
- **C_HUM:** 10nF ceramic capacitor to GND (low-pass filter)
- **Cutoff frequency:** fc = 1/(2π × R × C) = 1/(2π × 10kΩ × 10nF) ≈ **1.6kHz**
- **Attenuation at 50Hz:** ~32× (30dB) - sufficient to reject mains hum
- **Attenuation at 60Hz:** ~27× (28dB) - sufficient to reject mains hum

**Placement:** R_HUM and C_HUM must be placed immediately before the ADC input pin, after the existing 100nF filter capacitor (C8/C9).

### Why Different Pull-up Resistors?

Maximum ADC sensitivity occurs when R_pullup ≈ R_NTC at target temperature:

| Sensor | Target Temp | R_NTC at Target | Optimal Pull-up |
| ------ | ----------- | --------------- | --------------- |
| Brew   | 93°C        | ~4.3kΩ          | 3.3kΩ           |
| Steam  | 135°C       | ~1.4kΩ          | 1.2kΩ           |

### ADC Resolution

**Brew NTC (3.3kΩ pull-up):**

| Temp | R_NTC | Vout  | ADC Count | Resolution |
| ---- | ----- | ----- | --------- | ---------- |
| 90°C | 4.7kΩ | 1.75V | 2389      | -          |
| 93°C | 4.3kΩ | 1.70V | 2315      | 31/°C      |
| 96°C | 3.9kΩ | 1.62V | 2217      | 33/°C      |

**Steam NTC (1.2kΩ pull-up):**

| Temp  | R_NTC  | Vout  | ADC Count | Resolution |
| ----- | ------ | ----- | --------- | ---------- |
| 130°C | 1.6kΩ  | 1.71V | 2341      | -          |
| 135°C | 1.4kΩ  | 1.62V | 2203      | 28/°C      |
| 140°C | 1.25kΩ | 1.52V | 2086      | 23/°C      |

---

## Pressure Transducer Interface

**⚠️ SENSOR RESTRICTION:** Circuit designed for **0.5-4.5V ratiometric output ONLY**.

- ✅ 3-wire sensors (5V, GND, Signal) like YD4060
- ❌ 4-20mA current loop sensors (require current sense resistor)

### Circuit Schematic

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                        PRESSURE TRANSDUCER INTERFACE                            │
│                         (0.5-4.5V Ratiometric Output)                          │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    J26 CONNECTOR                                                                │
│    ─────────────                                                                │
│    Pin 12 (+5V) ──────────────────────────────────► Transducer VCC (Red)       │
│    Pin 13 (GND) ──────────────────────────────────► Transducer GND (Black)     │
│    Pin 14 (SIG) ◄─────────────────────────────────  Transducer OUT (Yellow)    │
│                                                                                 │
│                                                                                 │
│    VOLTAGE DIVIDER (Scales 0.5-4.5V → 0.34-3.06V for 3.3V ADC)                │
│    ─────────────────────────────────────────────────────────────               │
│                                                                                 │
│    PRESS_SIG ────┬────────────────────────────────────────────────────────┐    │
│    (0.5-4.5V)    │                                                        │    │
│                  │                                                        │    │
│             ┌────┴────┐                                                   │    │
│             │  5.6kΩ  │  R4 (series resistor)                            │    │
│             │   1%    │                                                   │    │
│             └────┬────┘                                                   │    │
│                  │                                                        │    │
│                  ├────────────────────────────────────────► GPIO28 (ADC2)│    │
│                  │                                                        │    │
│             ┌────┴────┐     ┌────────┐     ┌────────┐                    │    │
│             │  10kΩ   │     │ 100nF  │     │ BAT54S │                    │    │
│             │   1%    │     │ C11    │     │ D16    │ Overvoltage clamp  │    │
│             │   R3    │     └───┬────┘     │ Schottky│                    │    │
│             └────┬────┘         │          └────┬────┘                    │    │
│                  │              │               │                         │    │
│                 GND            GND            +3.3V                       │    │
│                                                                                 │
│    MATH:                                                                       │
│    ─────                                                                       │
│    V_ADC = V_sensor × R3 / (R3 + R4) = V_sensor × 10k / 15.6k = 0.641 × V     │
│                                                                                 │
│    At 0.5V input (0 bar):  V_ADC = 0.321V                                     │
│    At 4.5V input (16 bar): V_ADC = 2.88V  (within 3.0V ADC_VREF)             │
│                                                                                 │
│    PROTECTION:                                                                 │
│    ───────────                                                                 │
│    D16 (BAT54S): Dual Schottky clamps ADC input to 3.3V+0.3V = 3.6V max      │
│    This protects against R3 open-circuit fault (full 5V at GPIO28)           │
│                                                                                 │
│    ⚠️ CRITICAL: Additional Zener Clamp Required                               │
│    ────────────────────────────────────────────────────────────────────────── │
│    D_PRESSURE (BZT52C3V3): 3.3V Zener diode must be added in parallel to      │
│    BAT54S at GPIO28 input. Pressure sensors are ratiometric 5V devices. If    │
│    the voltage divider fails or ground lifts, it can dump 5V directly into   │
│    the ADC pin. The Zener provides hard clamping to 3.3V, ensuring ADC       │
│    protection even under fault conditions.                                    │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

### Pressure Calculation

```
ADC_raw = adc_read()
V_adc = ADC_raw × 3.0V / 4095
V_sensor = V_adc × 15.6kΩ / 10kΩ = V_adc × 1.56
Pressure_bar = (V_sensor - 0.5V) × 16 bar / 4.0V
```

### Recommended Transducer

| Parameter | YD4060-016B-G8       |
| --------- | -------------------- |
| Range     | 0-16 bar (0-232 PSI) |
| Output    | 0.5-4.5V ratiometric |
| Supply    | 5V DC                |
| Thread    | G1/8" (fits E61)     |
| Accuracy  | ±0.5% FS             |

---

## ADC Reference Design

See [Power Supply](03-Power-Supply.md#precision-adc-voltage-reference-buffered) for the buffered 3.0V reference circuit.

### ADC Channel Summary

| ADC  | GPIO   | Signal     | Range      | Resolution                                          |
| ---- | ------ | ---------- | ---------- | --------------------------------------------------- |
| ADC0 | GPIO26 | Brew NTC   | 0-3.0V     | ~31 counts/°C                                       |
| ADC1 | GPIO27 | Steam NTC  | 0-3.0V     | ~25 counts/°C                                       |
| ADC2 | GPIO28 | Pressure   | 0.32-2.88V | ~200 counts/bar                                     |
| ADC3 | GPIO29 | 5V_MONITOR | 0-3.0V     | 5V rail monitor (ratiometric pressure compensation) |

---

## Level Probe AC Coupling Verification Checklist

The steam boiler level probe uses AC excitation to prevent electrolysis and probe corrosion. Verify the following implementation:

**Circuit Verification:**

- [ ] **AC Excitation:** Probe is driven by capacitor-coupled clock signal (C64 = 1µF AC coupling)
- [ ] **DC Blocking:** C64 provides adequate DC blocking (prevents electrolysis)
- [ ] **Input Protection:** Series resistor R85 (100Ω) limits current if probe shorts to high-voltage heater element
- [ ] **SRif Routing:** J5 (SRif) connector pin connects to PCB GND via C-R network (R_SRif = 1MΩ, C_SRif = 100nF in parallel)
- [ ] **AC Return Path:** C_SRif allows 1kHz AC signal return (low impedance at AC)
- [ ] **DC Loop Prevention:** R_SRif blocks DC loop currents (high impedance at DC)

**Firmware Requirements:**

- [ ] **Software Filter:** Debounce/integration filter in firmware (1-2 second integration time)
- [ ] **Rationale:** Water in a boiling boiler is turbulent; signal will oscillate rapidly
- [ ] **Prevents:** Fill solenoid chattering from rapid level signal changes

**Expected Behavior:**

- When water touches probe: AC signal is attenuated (water = resistive load) → comparator output LOW
- When no water: AC signal passes with minimal attenuation → comparator output HIGH
- Signal frequency: ~1.6kHz (Wien bridge oscillator, U6)
