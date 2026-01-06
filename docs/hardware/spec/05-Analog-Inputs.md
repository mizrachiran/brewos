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
│        └────┬────┘                            └────┬────┘                      │
│             │                                      │                           │
│            GND                           ──────► GPIO26/ADC0    GPIO27/ADC1    │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

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

| ADC  | GPIO   | Signal          | Range      | Resolution        |
| ---- | ------ | --------------- | ---------- | ----------------- |
| ADC0 | GPIO26 | Brew NTC        | 0-3.0V     | ~31 counts/°C     |
| ADC1 | GPIO27 | Steam NTC       | 0-3.0V     | ~25 counts/°C     |
| ADC2 | GPIO28 | Pressure        | 0.32-2.88V | ~200 counts/bar   |
| ADC3 | GPIO29 | 5V_MONITOR      | 0-3.0V     | 5V rail monitor (ratiometric pressure compensation) |
