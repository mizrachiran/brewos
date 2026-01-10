# BrewOS Control Board - Test Procedures

## Pre-Manufacturing Validation & Prototype Testing Guide

**Document Purpose:** Step-by-step test procedures for validating the control board design  
**Revision:** 1.0  
**Date:** November 2025  
**Related Document:** ECM_Control_Board_Specification_v2.md

---

# TABLE OF CONTENTS

1. [Safety Warnings](#1-safety-warnings)
2. [Required Equipment](#2-required-equipment)
3. [Phase 1: Breadboard Testing](#3-phase-1-breadboard-testing-before-pcb-order)
4. [Phase 2: Prototype Board Testing](#4-phase-2-prototype-board-testing)
5. [Phase 3: Machine Integration Testing](#5-phase-3-machine-integration-testing)
6. [Risk Mitigation Checklist](#6-risk-mitigation-checklist)
7. [Test Results Log](#7-test-results-log)

---

# 1. Safety Warnings

```
┌────────────────────────────────────────────────────────────────────────────────┐
│  ⚠️⚠️⚠️  CRITICAL SAFETY WARNINGS  ⚠️⚠️⚠️                                      │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│  THIS BOARD OPERATES AT MAINS VOLTAGE (100-240V AC)                           │
│  LETHAL VOLTAGES ARE PRESENT WHEN POWERED                                      │
│                                                                                 │
│  MANDATORY SAFETY RULES:                                                       │
│  ────────────────────────                                                      │
│  1. NEVER work on mains circuits alone - have someone nearby                  │
│  2. ALWAYS use an isolation transformer for initial power-up                  │
│  3. WAIT 30+ seconds after power-off before touching (capacitor discharge)    │
│  4. USE one-hand rule when probing live circuits                              │
│  5. VERIFY power is OFF with multimeter before touching                       │
│  6. KEEP fire extinguisher (CO2 or dry powder) within reach                   │
│  7. WEAR safety glasses during high-power tests                               │
│  8. WORK on non-conductive surface (rubber mat)                               │
│  9. REMOVE metal jewelry (rings, watches, bracelets)                          │
│  10. KNOW location of circuit breaker/emergency shutoff                       │
│                                                                                 │
│  IF IN DOUBT, DO NOT PROCEED - CONSULT A QUALIFIED ELECTRICIAN                │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

---

# 2. Required Equipment

## 2.1 Essential Test Equipment

| Equipment                  | Purpose                      | Minimum Specs          | Recommended          |
| -------------------------- | ---------------------------- | ---------------------- | -------------------- |
| Digital Multimeter         | Voltage, current, resistance | CAT III 600V rated     | Fluke 117 or similar |
| Isolation Transformer      | Safe mains testing           | 500VA, 1:1 ratio       | With current limiter |
| Variable DC Power Supply   | Circuit testing              | 0-30V, 0-3A            | Dual channel         |
| Oscilloscope               | Signal integrity             | 50MHz, 2 channel       | 100MHz+ with probes  |
| Soldering Station          | Assembly                     | Temperature controlled | 60W+                 |
| Small Flathead Screwdriver | Screw terminal wiring        | 2.5mm blade            | For Phoenix MKDS     |

## 2.2 Recommended Additional Equipment

| Equipment                       | Purpose                  | Notes                        |
| ------------------------------- | ------------------------ | ---------------------------- |
| Thermal Camera / IR Thermometer | Hot spot detection       | Non-contact temp measurement |
| Current Clamp                   | Mains current            | AC clamp meter, 0-20A        |
| ESR Meter                       | Capacitor testing        | For electrolytic caps        |
| LCR Meter                       | Component verification   | Resistance, capacitance      |
| USB Logic Analyzer              | Digital signal debugging | 8+ channels, 24MHz+          |

## 2.3 Test Components & Fixtures

| Item                             | Quantity | Purpose               |
| -------------------------------- | -------- | --------------------- |
| 3.3kΩ 1% resistor                | 5        | NTC simulation        |
| 10kΩ 1% resistor                 | 5        | Various tests         |
| Test leads with clips            | 10       | Connections           |
| Breadboard (830 point)           | 2        | Circuit prototyping   |
| Jumper wires                     | 50       | Connections           |
| LED (any color)                  | 5        | Output indication     |
| 1kW resistive load (heater/lamp) | 1        | High current testing  |
| Thermometer (digital)            | 1        | Reference temperature |
| Ice + insulated container        | -        | 0°C reference         |
| Electric kettle / hot water      | -        | High temp reference   |

## 2.4 Safety Equipment

| Item                               | Purpose                |
| ---------------------------------- | ---------------------- |
| Fire extinguisher (CO2/dry powder) | Electrical fire safety |
| Safety glasses                     | Eye protection         |
| Insulated gloves (Class 0)         | Optional, for HV work  |
| Rubber mat                         | Insulated work surface |
| First aid kit                      | Emergency              |

---

# 3. Phase 1: Breadboard Testing (Before PCB Order)

**Objective:** Validate all circuit designs work correctly before committing to PCB fabrication.

---

## 3.1 NTC Thermistor Circuit Test

### Purpose

Verify the voltage divider produces correct output for temperature measurement with **50kΩ @ 25°C NTC sensors** (ECM/Profitec default). The production board uses a **3.0V precision reference** (LM4040).

> **Note:** ECM Synchronika uses 50kΩ NTC sensors. The pull-up resistor values are optimized per channel:
>
> - **Brew boiler:** 3.3kΩ pull-up (optimized for 90-96°C range)
> - **Steam boiler:** 1.2kΩ pull-up (optimized for 125-150°C range)

### Circuit to Build (Brew Boiler Example)

```
        3.0V (from bench supply - matches ADC_VREF)
          │
     ┌────┴────┐
     │  3.3kΩ  │  R_pullup (use 1% tolerance)
     │   1%    │
     └────┬────┘
          │
          ├────────────► V_out (measure here)
          │
     ┌────┴────┐
     │  NTC    │  ECM 50kΩ @ 25°C NTC sensor
     │  50kΩ   │  (or 50kΩ 1% resistor to simulate 25°C)
     └────┬────┘
          │
         GND
```

### Test Procedure

| Step | Action                                            | Expected Result        | ✓   |
| ---- | ------------------------------------------------- | ---------------------- | --- |
| 1    | Set bench supply to exactly 3.000V                | Display shows 3.00V    |     |
| 2    | Connect 50kΩ 1% resistor as "NTC"                 | Simulates 25°C         |     |
| 3    | Measure V_out                                     | 2.81V ±0.05V           |     |
| 4    | Replace with actual NTC at room temp              | Should match room temp |     |
| 5    | Place NTC in ice water (0°C)                      | V_out ≈ 2.94V          |     |
| 6    | Place NTC in boiling water (~100°C)               | V_out ≈ 1.55V          |     |
| 7    | Connect 4.3kΩ resistor (simulates 93°C brew temp) | V_out ≈ 1.70V          |     |
| 8    | Record actual readings below                      |                        |     |

### Results Log

| Condition          | Expected V_out | Measured V_out | Expected Temp | Calculated Temp | Error    |
| ------------------ | -------------- | -------------- | ------------- | --------------- | -------- |
| 50kΩ resistor      | 2.81V          | \_\_\_V        | 25°C          | N/A             | N/A      |
| 4.3kΩ resistor     | 1.70V          | \_\_\_V        | 93°C          | N/A             | N/A      |
| Room temp (actual) | ~2.81V         | \_\_\_V        | ~25°C         | \_\_\_°C        | \_\_\_°C |
| Ice water          | 2.94V          | \_\_\_V        | 0°C           | \_\_\_°C        | \_\_\_°C |
| Boiling water      | 1.55V          | \_\_\_V        | ~100°C        | \_\_\_°C        | \_\_\_°C |

### Pass Criteria

- [ ] All readings within ±10% of expected voltage
- [ ] Temperature calculation within ±3°C of reference thermometer
- [ ] No erratic readings or noise

### Troubleshooting

| Issue             | Possible Cause        | Solution                          |
| ----------------- | --------------------- | --------------------------------- |
| V_out always 3.0V | NTC open/disconnected | Check NTC continuity              |
| V_out always 0V   | NTC shorted           | Check for shorts                  |
| Noisy readings    | Poor connections      | Use shorter wires, add 100nF cap  |
| Wrong temperature | Incorrect B-value     | Recalculate with sensor datasheet |

### Steam Boiler NTC Test (Optional)

For steam boiler testing, use **1.2kΩ pull-up** instead of 3.3kΩ:

| Condition      | Pull-up | Expected V_out | Simulated Temp |
| -------------- | ------- | -------------- | -------------- |
| 50kΩ resistor  | 1.2kΩ   | 2.93V          | 25°C           |
| 1.4kΩ resistor | 1.2kΩ   | 1.62V          | 135°C          |
| 1.0kΩ resistor | 1.2kΩ   | 1.36V          | 150°C          |

---

## 3.2 Relay Driver Circuit Test

### Purpose

Verify the transistor driver can reliably switch relay coils from a 3.3V GPIO signal.

### Circuit to Build

```
                                    +5V (bench supply)
                                      │
                 ┌────────────────────┴────────────────────┐
                 │                                         │
            ┌────┴────┐                               ┌────┴────┐
            │  RELAY  │                               │  470Ω   │  R_led
            │  COIL   │                               └────┬────┘
            │   5V    │                                    │
            └────┬────┘                               ┌────┴────┐
                 │                                    │   LED   │
            ┌────┴────┐                               │  Green  │
            │ UF4007  │ ← Fast flyback diode               └────┬────┘
            └────┬────┘                                    │
                 │                                         │
                 └─────────────────────┬───────────────────┤
                                       │                   │
                                  ┌────┴────┐              │
                                  │    C    │              │
                                  │ 2N2222  │              │
    3.3V GPIO ─────────[1kΩ]─────►│    B    │              │
    (or bench supply)             │    E    ├──────────────┘
                      │           └────┬────┘
                 ┌────┴────┐           │
                 │  10kΩ   │          ─┴─
                 │Pull-down│          GND
                 └────┬────┘
                     ─┴─
                     GND
```

**LED current:** (5V - 2.0V) / 470Ω = 6.4mA (clearly visible)

### Test Procedure

| Step | Action                                  | Expected Result           | ✓   |
| ---- | --------------------------------------- | ------------------------- | --- |
| 1    | Build circuit on breadboard             | No shorts                 |     |
| 2    | Set 5V supply, verify voltage           | 5.0V ±0.25V               |     |
| 3    | Leave GPIO input disconnected           | Relay OFF, LED OFF        |     |
| 4    | Connect GPIO input to GND               | Relay OFF, LED OFF        |     |
| 5    | Connect GPIO input to 3.3V              | Relay clicks ON, LED ON   |     |
| 6    | Measure voltage at transistor collector | <0.5V (saturated)         |     |
| 7    | Measure relay coil current              | 70-85mA                   |     |
| 8    | Remove 3.3V from GPIO                   | Relay clicks OFF, LED OFF |     |
| 9    | Rapid on/off cycling (10x)              | Reliable switching        |     |
| 10   | Repeat for all 3 relay types            | All pass                  |     |

### Results Log

| Relay         | Coil Current | Collector Voltage (ON) | Switching Reliable | Pass |
| ------------- | ------------ | ---------------------- | ------------------ | ---- |
| K1 (10A SPST) | \_\_\_mA     | \_\_\_V                | Yes / No           |      |
| K2 (16A SPST) | \_\_\_mA     | \_\_\_V                | Yes / No           |      |
| K3 (10A SPST) | \_\_\_mA     | \_\_\_V                | Yes / No           |      |

### Pass Criteria

- [ ] All relays switch reliably from 3.3V signal
- [ ] Coil current within datasheet specs (typically 70-85mA for 5V relays)
- [ ] Collector voltage < 0.5V when ON (transistor saturated)
- [ ] No chattering or bouncing
- [ ] Pull-down keeps relay OFF when GPIO floating

---

## 3.3 SSR Driver Circuit Test

### Purpose

Verify the SSR trigger circuit provides adequate voltage (~4.8V) for KS15 D-24Z25-LQ SSR (4-32V input).

### Circuit to Build

**IMPORTANT:** SSR has internal current limiting - NO series resistor needed!

```
                                    +5V
                                      │
               ┌──────────────────────┴──────────────────────┐
               │                                             │
               │                                        ┌────┴────┐
               │                                        │  1kΩ    │  R_led
               │                                        └────┬────┘
               │                                             │
               ▼                                        ┌────┴────┐
    ┌──────────────────┐                                │   LED   │  Indicator
    │  SSR Input (+)   │                                │ Orange  │
    └────────┬─────────┘                                └────┬────┘
             │                                               │
             │    ┌───────────────────┐                      │
             │    │   EXTERNAL SSR    │                      │
             │    │   (internal LED   │                      │
             │    │    + resistor)    │                      │
             │    │  KS15 D-24Z25-LQ  │                      │
             │    └───────────────────┘                      │
             │                                               │
    ┌────────┴─────────┐                                     │
    │  SSR Input (-)   ├─────────────────────────────────────┤
    └────────┬─────────┘                                ┌────┴────┐
             │                                          │    C    │
             │                                          │ 2N2222  │
             └─────────────────────────────────────────►│    E    │
                                                        └────┬────┘
                                                             │
    3.3V GPIO ─────────────[1kΩ R_base]─────────────────────►B
                                 │                           │
                            ┌────┴────┐                     ─┴─
                            │  10kΩ   │ Pull-down           GND
                            └────┬────┘
                                ─┴─
                                GND
```

**Operation:**

- GPIO LOW → Transistor OFF → SSR- floating → SSR OFF (no current path)
- GPIO HIGH → Transistor ON → SSR- pulled to GND → SSR ON
- Voltage at SSR = 5V - Vce(sat) = 5V - 0.2V = **4.8V** (exceeds 4V minimum ✓)

### Test Procedure

| Step | Action                              | Expected Result                            | ✓   |
| ---- | ----------------------------------- | ------------------------------------------ | --- |
| 1    | Build circuit without SSR connected |                                            |     |
| 2    | GPIO = GND                          | Transistor OFF, SSR- floating, ~5V at SSR+ |     |
| 3    | GPIO = 3.3V                         | Transistor ON, SSR+ to SSR- = ~4.8V        |     |
| 4    | Connect actual SSR (DC side only)   |                                            |     |
| 5    | GPIO = GND                          | SSR LED OFF                                |     |
| 6    | GPIO = 3.3V                         | SSR LED ON                                 |     |
| 7    | Measure current into SSR            | 8-10mA                                     |     |
| 8    | Check indicator LED                 | LED illuminates                            |     |

### Results Log

| Measurement                                     | Expected | Measured | Pass |
| ----------------------------------------------- | -------- | -------- | ---- |
| SSR+ voltage (from 5V rail)                     | 5.0V     | \_\_\_V  |      |
| Voltage SSR+ to SSR- (GPIO HIGH, SSR connected) | 4.5-4.8V | \_\_\_V  |      |
| Transistor Vce(sat) when ON                     | <0.3V    | \_\_\_V  |      |
| Current into SSR                                | 7-15mA   | \_\_\_mA |      |
| SSR internal LED illuminates                    | Yes      | Yes / No |      |
| Indicator LED illuminates                       | Yes      | Yes / No |      |

### Pass Criteria

- [ ] SSR turns ON reliably when GPIO HIGH (~4.8V across SSR input)
- [ ] SSR turns OFF reliably when GPIO LOW (SSR- floating)
- [ ] Voltage at SSR ≥ 4.0V (meets KS15 D-24Z25-LQ minimum)
- [ ] SSR input current 7-15mA (handled by SSR internal limiting)
- [ ] No excessive heating of transistor or SSR

---

## 3.4 Steam Boiler Level Probe Circuit Test

### Purpose

Verify the level probe sensing concept works correctly and requires PE-GND connection.

```
┌────────────────────────────────────────────────────────────────────────────────┐
│  ⚠️  IMPORTANT: DC vs AC SENSING                                              │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│  This breadboard test uses a SIMPLIFIED DC CIRCUIT for concept validation.     │
│                                                                                 │
│  The PRODUCTION DESIGN uses an AC OSCILLATOR circuit (OPA342 Wien-bridge       │
│  + TLV3201 comparator) operating at ~1.6kHz to PREVENT PROBE CORROSION.        │
│                                                                                 │
│  DC sensing causes electrolysis, which corrodes the probe within months.       │
│  The AC design extends probe life to 5-10+ years.                              │
│                                                                                 │
│  This DC test is ONLY for validating:                                          │
│  • Basic conductivity detection concept                                        │
│  • PE-GND connection requirement                                               │
│  • Threshold resistances for water detection                                   │
│                                                                                 │
│  DO NOT use this DC circuit in production! See Phase 2/3 for AC circuit test. │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

### Simplified DC Circuit for Concept Validation

```
        3.3V
          │
     ┌────┴────┐
     │  47kΩ   │  Higher value limits current
     └────┬────┘
          │
          ├──────────────────────► GPIO (measure voltage here)
          │
     ┌────┴────┐     ┌────┴────┐
     │  10kΩ   │     │  100nF  │
     └────┬────┘     └────┬────┘
          │               │
          ├───────────────┤
          │               │
          ▼               │
     "PROBE" ─────────────┤
     (wire)               │
                         ─┴─
                         GND (connected to PE in real machine)
```

### Test Procedure (DC Concept Validation Only)

| Step | Action                                             | Expected Result                | ✓   |
| ---- | -------------------------------------------------- | ------------------------------ | --- |
| 1    | Build circuit on breadboard                        |                                |     |
| 2    | Leave "PROBE" wire disconnected (floating)         | GPIO reads HIGH (3.3V)         |     |
| 3    | Touch "PROBE" wire to GND                          | GPIO reads LOW (0V)            |     |
| 4    | Release "PROBE" wire                               | GPIO returns to HIGH           |     |
| 5    | Simulate water: 1kΩ resistor between PROBE and GND | GPIO reads LOW                 |     |
| 6    | Simulate partial contact: 10kΩ to GND              | GPIO reads LOW (threshold)     |     |
| 7    | Simulate high resistance: 100kΩ to GND             | GPIO reads HIGH (no detection) |     |

### Results Log

| Condition              | Expected GPIO | Measured Voltage | Pass |
| ---------------------- | ------------- | ---------------- | ---- |
| Probe open (air)       | HIGH (3.3V)   | \_\_\_V          |      |
| Probe to GND direct    | LOW (0V)      | \_\_\_V          |      |
| Probe via 1kΩ to GND   | LOW           | \_\_\_V          |      |
| Probe via 10kΩ to GND  | LOW           | \_\_\_V          |      |
| Probe via 100kΩ to GND | HIGH          | \_\_\_V          |      |

### Pass Criteria (DC Concept Test)

- [ ] Open probe reads HIGH (no water detected)
- [ ] Grounded probe reads LOW (water detected)
- [ ] Detection threshold appropriate for water conductivity
- [ ] No oscillation or noise on signal

### Critical Notes

```
⚠️  This circuit REQUIRES PE (Protective Earth) to be connected to signal GND.
    In the actual machine, the boiler body is connected to PE.
    Without PE-GND connection, this circuit will NOT work!

⚠️  DO NOT USE THIS DC CIRCUIT IN PRODUCTION!
    The production AC oscillator circuit (OPA342 + TLV3201) is tested in Phase 2/3.
```

### Phase 2/3: AC Oscillator Circuit Validation (On Assembled PCB)

The production design uses a Wien-bridge oscillator (OPA342) + comparator (TLV3201) for corrosion-free AC sensing. This cannot be easily breadboarded - test on assembled PCB:

| Step | Action                                                  | Expected Result                  | ✓   |
| ---- | ------------------------------------------------------- | -------------------------------- | --- |
| 1    | Power PCB, probe disconnected                           | GPIO4 reads HIGH (water low)     |     |
| 2    | Connect oscilloscope to AC_OUT (TP if available)        | ~160Hz sine wave, ~1.5Vpp        |     |
| 3    | Short probe terminal to GND                             | GPIO4 reads LOW (water OK)       |     |
| 4    | Dip probe in tap water (metal container grounded to PE) | GPIO4 reads LOW                  |     |
| 5    | Remove probe from water                                 | GPIO4 returns HIGH within 1s     |     |
| 6    | Dip probe in distilled/RO water                         | May read HIGH (low conductivity) |     |
| 7    | Add pinch of salt to water, retry                       | GPIO4 reads LOW                  |     |

**Sensitivity Adjustment (if needed):**

- If probe doesn't detect water reliably: Reduce R95 (10kΩ → 4.7kΩ)
- If probe triggers on steam/condensation: Increase R95 or adjust R98 hysteresis
- Oscillator amplitude too low: Check OPA342 power, R91-R93 values

**Pass Criteria for Production:**

- [ ] Probe in air = GPIO4 HIGH (fail-safe: heater OFF)
- [ ] Probe in tap water = GPIO4 LOW (heater enabled)
- [ ] No oscillation/chatter at water surface level
- [ ] Response time < 500ms in both directions

---

## 3.5 Pressure Transducer Circuit Test (YD4060)

### Purpose

Verify the voltage divider correctly scales the YD4060's 0.5-4.5V output to **0.32-2.88V** ADC range (stays below 3.0V reference).

**Transducer:** YD4060 (0-16 bar, 0.5-4.5V output, 5VDC supply)

### Circuit to Build

```
    Variable Voltage Source               To ADC
    (0.5V to 4.5V)                       (GPIO28)
          │                                  │
          │                                  │
     ┌────┴────┐                             │
     │  5.6kΩ  │  R4 (series resistor)       │
     │   1%    │                             │
     └────┬────┘                             │
          │                                  │
          ├──────────────────────────────────┤
          │                                  │
     ┌────┴────┐                        ┌────┴────┐
     │  10kΩ   │  R3 (to GND)           │  100nF  │
     │   1%    │                        │         │
     └────┬────┘                        └────┬────┘
          │                                  │
         GND                                GND
```

**Divider Ratio:** R3 / (R3 + R4) = 10k / (10k + 5.6k) = **0.641**

### Test Procedure

| Step | Action                           | Expected V_out | ✓   |
| ---- | -------------------------------- | -------------- | --- |
| 1    | Set input to 0.50V (0 bar)       | 0.32V          |     |
| 2    | Set input to 1.00V               | 0.64V          |     |
| 3    | Set input to 2.50V (8 bar)       | 1.60V          |     |
| 4    | Set input to 4.00V               | 2.56V          |     |
| 5    | Set input to 4.50V (16 bar)      | 2.88V          |     |
| 6    | Verify output never exceeds 3.0V | <3.0V always   |     |

### Results Log

| Input Voltage | Expected Output | Measured Output | Ratio | Pass |
| ------------- | --------------- | --------------- | ----- | ---- |
| 0.50V         | 0.32V           | \_\_\_V         | 0.641 |      |
| 1.00V         | 0.64V           | \_\_\_V         | 0.641 |      |
| 2.50V         | 1.60V           | \_\_\_V         | 0.641 |      |
| 4.00V         | 2.56V           | \_\_\_V         | 0.641 |      |
| 4.50V         | 2.88V           | \_\_\_V         | 0.641 |      |

### Pass Criteria

- [ ] Output ratio is **0.641 ±2%**
- [ ] Output never exceeds 3.0V (safe for RP2354 ADC with 3.0V reference)
- [ ] Stable readings (no noise/drift)

---

## 3.6 ESP32 Interface Test

### Purpose

Verify UART communication between RP2354 and ESP32 display module, and test the **brew-by-weight (WEIGHT_STOP)** signal.

> **Important Architecture Note:** In the production design:
>
> - **ESP32 controls RP2354** (not the other way around) for OTA firmware updates
> - ESP32 controls RP2354's **RUN** pin (J15 Pin 5) via GPIO20
> - **SWDIO** (J15 Pin 6): ESP32 TX2 ↔ RP2354 SWDIO (dedicated pin, 47Ω series)
> - **WEIGHT_STOP** (J15 Pin 7): ESP32 GPIO19 → RP2354 GPIO21 (4.7kΩ pull-down)
> - **SWCLK** (J15 Pin 8): ESP32 RX2 ↔ RP2354 SWCLK (dedicated pin, 22Ω series)

### Required

- RP2354 microcontroller (discrete chip or development board)
- ESP32 display module (or any ESP32 dev board)
- Resistors per schematic (33Ω series on UART lines)

### J15 Connector Wiring (8-pin JST-XH)

| J15 Pin | Function    | RP2354 Connection | ESP32 Connection | Protection      |
| ------- | ----------- | --------------- | ---------------- | --------------- |
| 1       | 5V Power    | VSYS (5V)       | VIN              | -               |
| 2       | Ground      | GND             | GND              | -               |
| 3       | TX          | GPIO0           | GPIO44 (RX)      | 33Ω series + TVS |
| 4       | RX          | GPIO1           | GPIO43 (TX)      | 33Ω series + TVS |
| 5       | RUN         | RUN pin         | GPIO20           | 10kΩ pull-up    |
| 6       | SWDIO       | SWDIO (dedicated) | TX2              | 47Ω series      |
| 7       | WEIGHT_STOP | GPIO21          | GPIO19           | 4.7kΩ pull-down |
| 8       | SWCLK       | SWCLK (dedicated) | RX2               | 22Ω series      |

### Test Firmware - RP2354 Side (MicroPython)

```python
from machine import UART, Pin
import time

# Initialize UART
uart = UART(0, baudrate=115200, tx=Pin(0), rx=Pin(1))

# WEIGHT_STOP signal (input from ESP32 for brew-by-weight)
weight_stop = Pin(21, Pin.IN, Pin.PULL_DOWN)

def uart_test():
    """Test UART communication"""
    print("Sending test message...")
    uart.write("Hello from RP2354!\n")
    time.sleep_ms(100)

    if uart.any():
        response = uart.read()
        print(f"Received: {response}")
    else:
        print("No response received")

def weight_stop_test():
    """Monitor WEIGHT_STOP signal from ESP32"""
    print("Monitoring WEIGHT_STOP (GPIO21)...")
    print("ESP32 should pulse this HIGH when target weight is reached")
    print("Press Ctrl+C to stop")
    last_state = weight_stop.value()
    while True:
        state = weight_stop.value()
        if state != last_state:
            print(f"WEIGHT_STOP: {'HIGH - STOP BREW!' if state else 'LOW'}")
            last_state = state
        time.sleep_ms(10)

# Main
print("ESP32 Interface Test")
print("-" * 40)
print("Commands: uart, weight, quit")
print("")
print("NOTE: ESP32 controls RP2354 RUN pin via GPIO20")

while True:
    cmd = input("> ").strip().lower()
    if cmd == "uart":
        uart_test()
    elif cmd == "weight":
        try:
            weight_stop_test()
        except KeyboardInterrupt:
            print("\nStopped monitoring")
    elif cmd == "quit":
        break
```

### Test Procedure

| Step | Action                               | Expected Result                 | ✓   |
| ---- | ------------------------------------ | ------------------------------- | --- |
| 1    | Connect ESP32 to J15, power on       | Both devices boot               |     |
| 2    | Type "uart" - send test message      | Data transmitted to ESP32       |     |
| 3    | Verify ESP32 receives message        | Message received on ESP32 RX    |     |
| 4    | ESP32 sends response                 | RP2354 receives response        |     |
| 5    | Type "weight" to monitor WEIGHT_STOP | Shows "LOW" initially           |     |
| 6    | ESP32 sets WEIGHT_STOP HIGH          | RP2354 prints "HIGH - STOP BREW!" |     |
| 7    | ESP32 releases WEIGHT_STOP           | RP2354 prints "LOW"             |     |
| 8    | ESP32 pulses J15 Pin 5 (RUN) LOW     | RP2354 resets                   |     |

### Results Log

| Test                        | Result      | Notes |
| --------------------------- | ----------- | ----- |
| UART TX (RP2354 → ESP32)      | Pass / Fail |       |
| UART RX (ESP32 → RP2354)      | Pass / Fail |       |
| WEIGHT_STOP signal (GPIO21)   | Pass / Fail |       |
| ESP32 can reset RP2354 (RUN)  | Pass / Fail |       |

### Pass Criteria

- [ ] UART communication works bidirectionally
- [ ] No data corruption at 115200 baud
- [ ] WEIGHT_STOP signal (GPIO21) reads correctly
- [ ] ESP32 can reset RP2354 via RUN pin

---

# 4. Phase 2: Prototype Board Testing

**Objective:** Validate the assembled PCB before connecting to machine.

---

## 4.1 Visual Inspection Checklist

| Item                                 | Check                       | Pass |
| ------------------------------------ | --------------------------- | ---- |
| **Solder Quality**                   |                             |      |
| No solder bridges between IC pins    | Inspect under magnification |      |
| No cold solder joints (dull, grainy) | All joints shiny            |      |
| All SMD components placed correctly  | No tombstoning              |      |
| **Component Orientation**            |                             |      |
| RP2354 module (if socketed)          | Pin 1 aligned               |      |
| HLW8012 IC                           | Pin 1 aligned               |      |
| All diodes                           | Cathode band correct        |      |
| Electrolytic capacitors              | Polarity correct            |      |
| LEDs                                 | Polarity correct            |      |
| **High Voltage Section**             |                             |      |
| Isolation slot present               | 2-3mm wide, clean           |      |
| HV traces properly routed            | No crossing to LV           |      |
| Creepage distances                   | ≥6mm HV to LV               |      |
| **Mechanical**                       |                             |      |
| Mounting holes clear                 | No traces blocked           |      |
| Connector alignment                  | Straight, accessible        |      |
| No PCB damage                        | No cracks, delamination     |      |

---

## 4.2 Pre-Power Electrical Tests

### Continuity Tests (Power OFF, No Components Powered)

| Test                      | Probe Points    | Expected        | Measured | Pass |
| ------------------------- | --------------- | --------------- | -------- | ---- |
| 5V to GND (short check)   | VSYS pad to GND | >10kΩ           | \_\_\_Ω  |      |
| 3.3V to GND (short check) | 3V3 pad to GND  | >10kΩ           | \_\_\_Ω  |      |
| L to N (mains short)      | J1-L to J1-N    | Open            | \_\_\_Ω  |      |
| L to GND (isolation)      | J1-L to GND     | Open (>1MΩ)     | \_\_\_Ω  |      |
| N to GND (isolation)      | J1-N to GND     | Open (>1MΩ)     | \_\_\_Ω  |      |
| PE to GND (connected)     | J1-PE to GND    | <1Ω             | \_\_\_Ω  |      |
| Fuse holder               | Across F1       | <0.5Ω (fuse in) | \_\_\_Ω  |      |

### Component Installation Verification

| Component             | Test Method             | Expected         | Pass |
| --------------------- | ----------------------- | ---------------- | ---- |
| All relay coils       | Measure coil resistance | 50-80Ω           |      |
| All LEDs              | Diode test              | 1.8-3.2V forward |      |
| All flyback diodes    | Diode test              | ~0.6V forward    |      |
| Shunt resistor R60    | Low-ohm mode            | 1mΩ ±10%         |      |
| NTC divider resistors | Resistance              | 3.3kΩ ±1%        |      |

---

## 4.3 Low Voltage Power-Up Test

### Setup

1. **DO NOT CONNECT MAINS**
2. Connect external 5V supply to VSYS (via test points or USB)
3. Current-limit supply to 500mA initially

### Test Sequence

| Step | Action                    | Measure               | Expected          | Actual   | Pass |
| ---- | ------------------------- | --------------------- | ----------------- | -------- | ---- |
| 1    | Apply 5V @ 500mA limit    | Supply current        | <100mA            | \_\_\_mA |      |
| 2    | Verify 5V rail            | Voltage at VSYS       | 5.0V ±5%          | \_\_\_V  |      |
| 3    | Verify 3.3V buck output   | Voltage at 3V3        | 3.3V ±3%          | \_\_\_V  |      |
| 3a   | Verify ADC reference      | Voltage at TP2        | 3.0V ±0.5%        | \_\_\_V  |      |
| 4    | Check for hot components  | Touch test (careful!) | All cool          |          |      |
| 5    | Connect RP2354 module     | Observe power LED      | Module powers on   |          |      |
| 6    | Verify power-on           | Measure 3.3V rail     | 3.3V ±3%          |          |      |
| 7    | Remove USB, power from 5V | 5V current draw       | ~50-100mA         | \_\_\_mA |      |

---

## 4.4 GPIO and Peripheral Test

### Test Firmware

Upload a comprehensive test firmware to exercise all GPIOs:

```python
# gpio_test.py - Upload to RP2354
from machine import Pin, ADC, PWM, SPI, UART
import time

# Define all outputs (directly controlled by RP2354)
outputs = {
    10: "K1_RELAY",      # Mains indicator lamp relay
    11: "K2_RELAY",      # Pump relay
    12: "K3_RELAY",      # Solenoid relay
    13: "SSR1",          # Brew heater SSR trigger
    14: "SSR2",          # Steam heater SSR trigger
    15: "STATUS_LED",    # Green status LED
    19: "BUZZER",        # Passive piezo buzzer (PWM)
}

# Define all inputs
inputs = {
    2: "WATER_SW",       # Water reservoir switch (active low)
    3: "TANK_LVL",       # Tank level sensor (active low)
    4: "STEAM_LVL",      # Steam boiler level (from TLV3201 comparator)
    5: "BREW_SW",        # Brew handle switch (active low)
    21: "WEIGHT_STOP",   # Brew-by-weight signal from ESP32 (active high)
}

# ADC channels (3.0V reference from LM4040)
ADC_VREF = 3.0  # Precision reference voltage
adcs = {
    26: "BREW_NTC",
    27: "STEAM_NTC",
    28: "PRESSURE",
}

def test_outputs():
    """Toggle each output"""
    print("\n=== OUTPUT TEST ===")
    for gpio, name in outputs.items():
        pin = Pin(gpio, Pin.OUT)
        print(f"GPIO{gpio} ({name}): ON", end="")
        pin.value(1)
        time.sleep(0.5)
        print(" -> OFF")
        pin.value(0)
        time.sleep(0.2)

def test_inputs():
    """Read all inputs"""
    print("\n=== INPUT TEST ===")
    for gpio, name in inputs.items():
        # WEIGHT_STOP uses pull-down, others use pull-up
        if gpio == 21:
            pin = Pin(gpio, Pin.IN, Pin.PULL_DOWN)
            state = "HIGH (active)" if pin.value() else "LOW (idle)"
        else:
            pin = Pin(gpio, Pin.IN, Pin.PULL_UP)
            state = "HIGH (open)" if pin.value() else "LOW (active)"
        print(f"GPIO{gpio} ({name}): {state}")

def test_adc():
    """Read all ADC channels"""
    print("\n=== ADC TEST ===")
    print(f"(Reference voltage: {ADC_VREF}V)")
    for gpio, name in adcs.items():
        adc = ADC(gpio)
        raw = adc.read_u16()
        voltage = raw * ADC_VREF / 65535
        print(f"GPIO{gpio} ({name}): {raw} ({voltage:.3f}V)")

def test_buzzer():
    """Play test tone"""
    print("\n=== BUZZER TEST ===")
    pwm = PWM(Pin(19))
    for freq in [1000, 2000, 3000]:
        print(f"  {freq}Hz", end="... ")
        pwm.freq(freq)
        pwm.duty_u16(32768)
        time.sleep(0.3)
        print("done")
    pwm.duty_u16(0)
    pwm.deinit()

def main():
    print("=" * 50)
    print("ECM Control Board - GPIO Test")
    print("=" * 50)
    print("Note: J15 pins 6-8 are SWDIO/WEIGHT_STOP/SWCLK (SWD interface + brew-by-weight)")

    while True:
        print("\nOptions:")
        print("  1 - Test all outputs (relays, SSRs, LEDs)")
        print("  2 - Read all inputs (switches, WEIGHT_STOP)")
        print("  3 - Read all ADC channels")
        print("  4 - Buzzer test")
        print("  5 - Run all tests")
        print("  q - Quit")

        cmd = input("\nSelect: ").strip()

        if cmd == "1":
            test_outputs()
        elif cmd == "2":
            test_inputs()
        elif cmd == "3":
            test_adc()
        elif cmd == "4":
            test_buzzer()
        elif cmd == "5":
            test_outputs()
            test_inputs()
            test_adc()
            test_buzzer()
        elif cmd.lower() == "q":
            break

if __name__ == "__main__":
    main()
```

### Output Test Results

| GPIO | Function       | Click/LED Observed | Pass |
| ---- | -------------- | ------------------ | ---- |
| 10   | K1 Relay + LED | Yes / No           |      |
| 11   | K2 Relay + LED | Yes / No           |      |
| 12   | K3 Relay + LED | Yes / No           |      |
| 13   | SSR1 LED       | Yes / No           |      |
| 14   | SSR2 LED       | Yes / No           |      |
| 15   | Status LED     | Yes / No           |      |
| 19   | Buzzer         | Audible / No       |      |

### Input Test Results

| GPIO | Function     | Pull      | Open State | Active State | Pass |
| ---- | ------------ | --------- | ---------- | ------------ | ---- |
| 2    | Water Switch | Pull-up   | HIGH       | LOW          |      |
| 3    | Tank Level   | Pull-up   | HIGH       | LOW          |      |
| 4    | Steam Level  | None      | HIGH       | LOW          |      |
| 5    | Brew Switch  | Pull-up   | HIGH       | LOW          |      |
| 21   | WEIGHT_STOP  | Pull-down | LOW        | HIGH         |      |

### ADC Test Results

> **Note:** ADC uses 3.0V precision reference (LM4040). Verify at TP2.

| GPIO | Function  | No Input  | With 50kΩ NTC @ 25°C | Pass |
| ---- | --------- | --------- | -------------------- | ---- |
| 26   | Brew NTC  | ~2.9V     | ~2.81V               |      |
| 27   | Steam NTC | ~2.9V     | ~2.93V (1.2kΩ PU)    |      |
| 28   | Pressure  | 0.32V min | (varies with input)  |      |

---

## 4.5 Mains Power-Up Test

```
⚠️⚠️⚠️  DANGER - LETHAL VOLTAGES  ⚠️⚠️⚠️

ONLY proceed if:
- You have appropriate electrical safety training
- An isolation transformer is available
- Another person is present
- Fire extinguisher is within reach
```

### Setup

1. Disconnect all loads (relays disconnected from machine)
2. Connect isolation transformer (if available)
3. Connect power via switch-mode power strip with switch
4. Have multimeter ready to measure

### Test Sequence

| Step | Action                   | Measure          | Expected         | Actual  | Pass |
| ---- | ------------------------ | ---------------- | ---------------- | ------- | ---- |
| 1    | Apply mains (via switch) | None             | No sparks/smoke  |         |      |
| 2    | Wait 5 seconds           | Observe          | No heating       |         |      |
| 3    | Measure 5V rail          | Voltage          | 5.0V ±5%         | \_\_\_V |      |
| 4    | Measure idle current     | Clamp meter      | <0.1A            | \_\_\_A |      |
| 5    | Energize one relay (K1)  | Current increase | +~0.08A          | \_\_\_A |      |
| 6    | Energize all 3 relays    | Total current    | ~0.3A            | \_\_\_A |      |
| 7    | Run for 5 minutes        | Temperature      | All cool         |         |      |
| 8    | Power off, wait 30s      |                  | Caps discharged  |         |      |
| 9    | Touch test (careful!)    | Temperature      | Slightly warm OK |         |      |

---

## 4.6 High Current Test

### Setup

- Connect a 1kW resistive load (e.g., heater, incandescent bulbs) through relay K2
- Use clamp meter to measure current
- Have thermal camera or IR thermometer ready

### Test Sequence (⚠️ MAINS VOLTAGE)

| Step | Action                      | Measure        | Expected        | Actual   | Pass |
| ---- | --------------------------- | -------------- | --------------- | -------- | ---- |
| 1    | Energize K2 relay           | Load turns on  | Load operates   |          |      |
| 2    | Measure load current        | Clamp meter    | 4-5A (1kW load) | \_\_\_A  |      |
| 3    | Run for 10 minutes          |                | Continuous      |          |      |
| 4    | Measure relay temperature   | IR thermometer | <60°C           | \_\_\_°C |      |
| 5    | Measure shunt temperature   | IR thermometer | <50°C           | \_\_\_°C |      |
| 6    | Measure fuse holder temp    | IR thermometer | <50°C           | \_\_\_°C |      |
| 7    | Measure spade terminal temp | IR thermometer | <50°C           | \_\_\_°C |      |
| 8    | De-energize K2, power off   |                |                 |          |      |

### Pass Criteria

- [ ] No component exceeds 70°C
- [ ] No discoloration of PCB
- [ ] No burning smell
- [ ] Relay contacts don't weld

---

# 5. Phase 3: Machine Integration Testing

**Objective:** Validate full system operation in the actual espresso machine.

---

## 5.1 Pre-Connection Checklist

| Item                                             | Verification               | Pass |
| ------------------------------------------------ | -------------------------- | ---- |
| Machine unplugged                                | Verified                   |      |
| Original control board removed                   | Documented wire positions  |      |
| All machine wires labeled                        | Photos taken               |      |
| New board PE connected                           | Continuity to chassis      |      |
| **LV Wiring:** Wires stripped 6mm or ferrules    | Visual (J26 - all 18 pins) |      |
| **HV Wiring:** Spade connectors crimped properly | Tug test (J1, J2, J3, J4)  |      |

---

## 5.2 Sensor Connection Test (Machine OFF)

Connect all sensors but do NOT connect mains loads. **All sensors connect to unified J26 terminal block.**

| Sensor            | J26 Pin(s) | Expected Reading     | Actual    | Pass |
| ----------------- | ---------- | -------------------- | --------- | ---- |
| Water Reservoir   | 1, 2       | HIGH (tank removed)  |           |      |
| Tank Level        | 3, 4       | (depends on float)   |           |      |
| Steam Level Probe | 5          | (depends on water)\* |           |      |
| Brew Handle       | 6, 7       | HIGH (handle down)   |           |      |
| Brew NTC          | 8, 9       | Room temp (~25°C)    | \_\_\_°C  |      |
| Steam NTC         | 10, 11     | Room temp (~25°C)    | \_\_\_°C  |      |
| Pressure          | 14, 15, 16 | 0 bar                | \_\_\_bar |      |
| SSR1 Control      | 17, 18     | 5V output            |           |      |
| SSR2 Control      | 19, 20     | 5V output            |           |      |
| Spare GND         | 21, 22     | 0V (GND)             |           |      |

> **J26 Terminal Block:** Phoenix MKDS 1/18-5.08 (18-position, 5.08mm pitch).

> **Note for CT Clamp:** CT clamp connects directly to the external power meter module (not via this PCB). Verify CT clamp wiring per meter module datasheet.

> **Note for Steam Level Probe:** Single-wire connection on J26 Pin 5. Return path is via boiler chassis (PE/Earth connection required).

---

## 5.3 Actuator Connection Test (Machine OFF, Mains Disconnected from Loads)

| Actuator      | Connector     | Test Method   | Expected    | Pass |
| ------------- | ------------- | ------------- | ----------- | ---- |
| K1 - LED      | J2 (spade)    | Energize K1   | Click heard |      |
| K2 - Pump     | J3 (spade)    | Energize K2   | Click heard |      |
| K3 - Solenoid | J4 (spade)    | Energize K3   | Click heard |      |
| SSR1 - Brew   | J26 Pin 15-16 | Energize SSR1 | SSR LED on  |      |
| SSR2 - Steam  | J26 Pin 17-18 | Energize SSR2 | SSR LED on  |      |

---

## 5.4 Full Power Integration Test

### Sequence

| Step | Action                   | Verify                       | Pass |
| ---- | ------------------------ | ---------------------------- | ---- |
| 1    | Connect all loads        | Wiring correct               |      |
| 2    | Apply mains power        | No issues                    |      |
| 3    | Pump test (K2)           | Pump runs, water flows       |      |
| 4    | Solenoid test (K3)       | Valve clicks                 |      |
| 5    | Brew heater test (SSR1)  | Element heats (monitor temp) |      |
| 6    | Steam heater test (SSR2) | Element heats (monitor temp) |      |
| 7    | Safety interlock test    | Remove water → heaters OFF   |      |
| 8    | Temperature control test | PID holds setpoint           |      |
| 9    | Full brew cycle          | All systems operational      |      |

---

## 5.5 Safety Interlock Tests

| Test                    | Procedure          | Expected Result       | Actual | Pass |
| ----------------------- | ------------------ | --------------------- | ------ | ---- |
| Water reservoir removed | Remove tank        | Heaters disabled      |        |      |
| Tank level low          | Lower float        | Heaters disabled      |        |      |
| Steam boiler dry        | Simulate low level | Steam heater disabled |        |      |
| Over-temperature        | (test in firmware) | Heaters disabled      |        |      |
| Watchdog timeout        | (test in firmware) | System resets         |        |      |

---

# 6. Risk Mitigation Checklist

## High Voltage Safety

| Risk                   | Mitigation                 | Verified |
| ---------------------- | -------------------------- | -------- |
| Insufficient creepage  | Measure: ≥6mm HV to LV     |          |
| Trace width inadequate | Verify: ≥3mm for 16A paths |          |
| Poor isolation         | Slot present between HV/LV |          |
| Fuse undersized        | Verify: 20A rating         |          |
| No surge protection    | MOV installed              |          |
| Contact arcing         | Flyback diodes installed   |          |

## Thermal

| Risk              | Mitigation              | Verified |
| ----------------- | ----------------------- | -------- |
| Shunt overheating | 5W rating (20× margin)  |          |
| Relay coil heat   | Spacing adequate        |          |
| AC/DC module heat | Ventilation space       |          |
| Buck regulator    | Minimal heat (>90% eff) |          |

## Signal Integrity

| Risk            | Mitigation                     | Verified |
| --------------- | ------------------------------ | -------- |
| ADC noise       | Analog ground isolation        |          |
| UART corruption | Series resistors installed     |          |
| ESD damage      | ESD clamps on external signals |          |

---

# 7. Test Results Log

## Summary

| Phase                    | Date           | Tester     | Result      |
| ------------------------ | -------------- | ---------- | ----------- |
| 1.1 NTC Test             | **_/_**/\_\_\_ | **\_\_\_** | Pass / Fail |
| 1.2 Relay Driver         | **_/_**/\_\_\_ | **\_\_\_** | Pass / Fail |
| 1.3 SSR Driver           | **_/_**/\_\_\_ | **\_\_\_** | Pass / Fail |
| 1.4 Level Probe          | **_/_**/\_\_\_ | **\_\_\_** | Pass / Fail |
| 1.5 Pressure             | **_/_**/\_\_\_ | **\_\_\_** | Pass / Fail |
| 1.7 ESP32 Interface      | **_/_**/\_\_\_ | **\_\_\_** | Pass / Fail |
| 2.1 Visual Inspection    | **_/_**/\_\_\_ | **\_\_\_** | Pass / Fail |
| 2.2 Pre-Power Tests      | **_/_**/\_\_\_ | **\_\_\_** | Pass / Fail |
| 2.3 Low Voltage Power-Up | **_/_**/\_\_\_ | **\_\_\_** | Pass / Fail |
| 2.4 GPIO Test            | **_/_**/\_\_\_ | **\_\_\_** | Pass / Fail |
| 2.5 Mains Power-Up       | **_/_**/\_\_\_ | **\_\_\_** | Pass / Fail |
| 2.6 High Current Test    | **_/_**/\_\_\_ | **\_\_\_** | Pass / Fail |
| 3.1-3.5 Integration      | **_/_**/\_\_\_ | **\_\_\_** | Pass / Fail |

## Issues Found

| Issue # | Description | Severity     | Resolution | Status      |
| ------- | ----------- | ------------ | ---------- | ----------- |
| 1       |             | High/Med/Low |            | Open/Closed |
| 2       |             | High/Med/Low |            | Open/Closed |
| 3       |             | High/Med/Low |            | Open/Closed |

## Sign-Off

| Role     | Name | Signature | Date |
| -------- | ---- | --------- | ---- |
| Tester   |      |           |      |
| Reviewer |      |           |      |

---

_End of Test Procedures Document_
