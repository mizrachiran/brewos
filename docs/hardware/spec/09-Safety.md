# Safety & Protection

## Mains Input Protection

### Protection Components

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                         MAINS INPUT PROTECTION                                  │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    L (Live) ───┬───[F1 10A]───┬──────────────────────────► L_FUSED            │
│                │              │                                                 │
│           ┌────┴────┐    ┌────┴────┐                                           │
│           │   MOV   │    │   X2    │                                           │
│           │  275V   │    │  100nF  │                                           │
│           │  (RV1)  │    │  (C1)   │                                           │
│           └────┬────┘    └────┬────┘                                           │
│                │              │                                                 │
│    N (Neutral) ┴──────────────┴──────────────────────────► N                   │
│                                                                                 │
│    ⚠️ NOTE: PE (Protective Earth) connection REMOVED from PCB.                  │
│    HV section is now floating - no Earth connection to prevent L-to-Earth      │
│    shorts.                                                                      │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

### Component Specifications

| Component | Value              | Part Number              | Purpose                 |
| --------- | ------------------ | ------------------------ | ----------------------- |
| F1        | 10A 250V slow-blow | **Littelfuse 463** (SMD) | Main circuit protection |
| F2        | 2A 250V slow-blow  | **Littelfuse 463** (SMD) | HLK module protection   |
| RV1       | 275V AC MOV        | S14K275                  | Surge protection        |
| C1        | 100nF X2 275V AC   | -                        | EMI filter              |

### Fuse Coordination

```
Fuse Hierarchy (selectivity):
─────────────────────────────

F1 (10A) protects: Relay loads (pump, solenoid, lamp)
    └─► Trip current: ~15A (slow-blow)
    └─► Trip time: ~10s at 200% load

F2 (2A) protects: HLK-15M05C module only
    └─► Trip current: ~3A
    └─► Trip time: ~1s at 200% load
    └─► Faster than F1 → protects HLK without affecting relays

Machine MCB (16A): House circuit protection
    └─► Trips on dead short only
    └─► F1/F2 clear faults before MCB trips
```

---

## Creepage and Clearance Requirements

### IEC 60950-1 / IEC 62368-1 Compliance

| Boundary              | Type       | Creepage  | Clearance | Implementation         |
| --------------------- | ---------- | --------- | --------- | ---------------------- |
| Mains → 5V DC         | Reinforced | **6.0mm** | **4.0mm** | Milled slot + wide gap |
| Relay coil → contacts | Basic      | 3.0mm     | 2.5mm     | Internal relay design  |
| Phase → Neutral       | Functional | 2.5mm     | 2.5mm     | Trace spacing          |
| HV GND → LV GND       | Reinforced | 6.0mm     | 4.0mm     | Via HLK isolation      |

### PCB Implementation

- **Milled slot** (minimum 2mm wide, full PCB thickness) between HV and LV sections
- **No copper pour** crosses isolation boundary
- **Silkscreen warning** at HV/LV boundary

---

## ESD and Transient Protection

### Input Protection Summary

| Signal           | Protection     | Device                     | Notes                                                |
| ---------------- | -------------- | -------------------------- | ---------------------------------------------------- |
| GPIO2 (Water)    | ESD clamp      | PESD5V0S1BL (D10)          | SOD-323                                              |
| GPIO3 (Tank)     | ESD clamp      | PESD5V0S1BL (D11)          | SOD-323                                              |
| GPIO4 (Level)    | ESD clamp      | PESD5V0S1BL (D12)          | SOD-323                                              |
| GPIO5 (Brew)     | ESD clamp      | PESD5V0S1BL (D13)          | SOD-323                                              |
| ADC0 (Brew NTC)  | ESD clamp      | PESD5V0S1BL (D14)          | SOD-323                                              |
| ADC1 (Steam NTC) | ESD clamp      | PESD5V0S1BL (D15)          | SOD-323                                              |
| ADC2 (Pressure)  | Schottky + TVS | BAT54S (D16) + PESD3V3S1BL | Overvoltage protection (dual clamp, low-leakage TVS) |
| 5V Rail          | TVS            | SMBJ5.0A (D20)             | Surge protection                                     |
| RS485 A/B        | TVS            | SM712 (D21)                | Asymmetric (-7V/+12V)                                |
| Service TX/RX    | Zener clamp    | BZT52C3V3 (D23/D24)        | 5V TTL protection                                    |
| USB D+/D-        | ESD clamp      | PESD5V0S1BL (D_USB_DP/DM)  | USB ESD protection (J_USB)                           |

### Pressure ADC Protection Detail

```
                 R4 (5.6kΩ)
PRESS_SIG ────────┬──────────────────► GPIO28 (ADC2)
(0.5-4.5V)        │                         │
             ┌────┴────┐              ┌─────┴─────┐
             │  10kΩ   │              │  BAT54S   │
             │   R3    │              │   D16     │
             └────┬────┘              │ Schottky  │
                  │                   └─────┬─────┘
                 GND                       │
                                           │
                                    ┌──────┴──────┐
                                    │  BZT52C3V3  │
                                    │  D_PRESSURE │
                                    │   Zener     │
                                    └──────┬──────┘
                                           │
                                         +3.3V
```

**Dual-Clamp Protection Strategy:**

- **BAT54S (D16):** Fast Schottky clamp (low forward voltage, fast response)
- **PESD3V3S1BL (D_PRESSURE):** Low-leakage TVS clamp (3.3V breakdown, <1µA leakage at 2.8V)

**Protection Scenario:** If R3 fails open, full 5V appears at GPIO28.

- BAT54S clamps to 3.3V + 0.3V = 3.6V (fast response)
- PESD3V3S1BL provides hard clamp at 3.3V with minimal leakage at operating voltages (preserves measurement accuracy)
- Both clamps work in parallel for redundant protection

---

## Thermal Management

### Heat Sources

| Component   | Power Dissipation | Mitigation                            |
| ----------- | ----------------- | ------------------------------------- |
| HLK-15M05C  | ~3W at full load  | Adequate PCB area, natural convection |
| TPS563200   | ~25mW             | Minimal (>90% efficient)              |
| Relay coils | ~0.35W (K2)       | Continuous operation OK               |
| Transistors | <10mW each        | No heatsink needed                    |

### Design Considerations

1. **HLK module placement:** Near board edge for airflow
2. **No components under HLK:** Heat rises
3. **Thermal relief on pads:** Easier soldering, acceptable for non-power components
4. **Conformal coating:** Protects against moisture in hot/humid environment

---

## Conformal Coating Guidance

### Recommended Areas

| Area                            | Coating     | Notes                                 |
| ------------------------------- | ----------- | ------------------------------------- |
| HV section (relays, MOVs, fuse) | ✅ Yes      | Prevents tracking from contamination  |
| Level probe trace area          | ✅ Yes      | High-impedance, sensitive to moisture |
| LV analog section               | ✅ Yes      | Protects ADC inputs from drift        |
| Connectors (J15, J17, J26)      | ❌ Mask off | Must remain solderable                |
| Pico module socket              | ❌ Mask off | Module must be removable              |
| Relay contacts                  | ❌ Mask off | Internal, already sealed              |
| Test points (TP1-TP3)           | ❌ Mask off | Must remain accessible                |

**Coating Type:** Acrylic (MG Chemicals 419D) or silicone-based.
**Apply:** After all testing complete.

---

## Safety Compliance Notes

### Applicable Standards

| Standard              | Scope                          | Status               |
| --------------------- | ------------------------------ | -------------------- |
| IEC 60335-1           | Household appliances - General | Reference for safety |
| IEC 60950-1 / 62368-1 | IT equipment safety            | Creepage/clearance   |
| UL 94 V-0             | PCB flammability               | FR-4 material        |
| CE marking            | European conformity            | Design target        |

### Critical Safety Points

1. **Only relay-switched loads flow through control board** (pump, valves ~6A max)
2. **Heater power does NOT flow through PCB** (external SSRs connect directly to mains)
3. **Power metering HV does NOT flow through PCB** (external modules handle their own mains)
4. **Fused live bus** (10A) feeds relay COMs only
5. **6mm creepage/clearance** required between HV and LV sections
6. **All mounting holes (MH1-MH4) = NPTH** (isolated) - no PE connection on PCB
7. **J5 (SRif) provides single chassis reference** - connects PCB GND to chassis via dedicated wire

### MOV Safety (IEC 60335-1 §19.11.2)

MOVs are placed across **LOADS** (not across relay contacts):

- If MOV shorts → L-N short → fuse clears → safe
- If MOV was across contacts → actuator bypasses control → dangerous

### Safety Interlock Wiring Verification

**⚠️ CRITICAL REQUIREMENT:** SSR control outputs **MUST NOT** bypass the physical High-Limit Thermostats (165°C switches) on the boilers.

**Verification Checklist:**

- [ ] **SSR wiring:** External SSRs connect to machine's existing heater wiring, which includes high-limit thermostats in series
- [ ] **No bypass:** PCB SSR control signals (J26 pins 15-18) are LOW-VOLTAGE DC control only - they do NOT carry mains power
- [ ] **Physical interlock:** Machine's factory-installed high-limit thermostats remain in series with heater elements
- [ ] **Failsafe:** If firmware crashes with SSR ON, high-limit thermostat will still open at 165°C, cutting power to heater

**Wiring Diagram Verification:**

```
Mains L ──► [High-Limit Thermostat] ──► [SSR AC Input] ──► [Heater Element] ──► N
                              │
                              │ (Physical switch, opens at 165°C)
                              │
PCB Control: J26-15/16 ──► [SSR DC Control] (5V DC, <20mA)
                              │
                              └─► SSR turns ON/OFF based on firmware control
```

**Safety Benefit:** This provides a **non-software failsafe** against thermal runaway if the firmware crashes in an "ON" state. The high-limit thermostat is a mechanical switch that operates independently of the control board.

---

## Grounding Strategy (SRif Architecture)

### New Grounding Approach

This design implements the "Gicar-style" **Chassis Reference (SRif)** grounding system to eliminate the risk of L-to-Earth shorts that could cause explosive arcing.

### Core Concept

Instead of bringing Mains Earth onto the PCB High Voltage side (where it risks arcing to Live), we:

1. **Float the HV Section:** Remove the Earth pin from the main power connector (J1, J24).
2. **Ground the LV Section:** Connect the Low Voltage Logic Ground (GND) to the machine chassis via a single dedicated wire (**J5 SRif**).
3. **Result:** The level probe circuit completes its path through: Boiler Metal → Chassis → SRif Wire (J5) → PCB GND.

### Grounding Hierarchy

```
NEW GROUNDING STRATEGY
──────────────────────

1. MACHINE CHASSIS is Earthed via the main power cord (Wall Plug).
2. PCB HV SIDE is Floating (No Earth connection).
3. PCB LV SIDE (GND) connects to CHASSIS via J5 (SRif).

Safety Benefit:
If 'L' shorts to the PCB surface, it hits FR4 (fiberglass), not a
Ground Plane. It cannot create a dead short or explode.
The fuse will eventually blow if it finds a path, but the "Explosive
Arc" risk is eliminated.
```

### Level Probe Return Path

The level probe signal return path is now explicitly via the SRif loop:

```
Signal Flow:
AC_OUT (U6) ──► J26-5 ──► PROBE ──► WATER ──► BOILER BODY
                                                       │
PCB GND ◄── C_SRif (AC) / R_SRif (DC block) ◄── J5 (SRif) ◄────── WIRE ◄───────────┘
```

### Implementation Requirements

- **J5 Connector:** Single 6.3mm male spade terminal connected to PCB GND via C-R network
- **C-R Network:** Parallel combination of R_SRif (1MΩ) and C_SRif (100nF)
  - C_SRif allows 1kHz AC signal return (low impedance at AC)
  - R_SRif blocks DC loop currents (high impedance at DC)
  - **⚠️ CRITICAL - C_SRif Voltage Rating:** This capacitor connects Logic Ground to Chassis Earth. In a fault condition (Live short to Chassis), this capacitor could see mains voltage transients before the breaker trips. **C_SRif MUST be rated for at least 250V AC (X2 Safety rating preferred) or 630V DC.** A standard 50V ceramic capacitor is a safety risk (could fail short, creating a ground loop).
- **Wiring:** 18AWG Green/Yellow wire from J5 to boiler/chassis mounting bolt
- **Mounting Holes:** All mounting holes (MH1-MH4) must be NPTH (Non-Plated Through Hole) to prevent random grounding
- **Isolation:** Maintain 6mm Keep-Out Zone between HV traces and LV Ground Pour

### Level Probe Fail-Safe Analysis

**⚠️ FAILURE MODE:** If the user forgets to connect the Green/Yellow SRif wire (J5) to the chassis, the level probe circuit opens (high impedance).

**Consequences:**

- **Steam Boiler Level Probe:** Reads "No Water" (high impedance = HIGH signal)

  - Autofill will attempt to run but will timeout (safe failure)
  - Heater will not turn on if firmware requires valid level reading (safe)
  - Ensure firmware handles "infinite fill" timeout gracefully (e.g., 30-second timeout with error indication)

- **Reservoir Sensor (GPIO2):** Independent switch, not affected by SRif failure
  - If "No Water" prevents heater operation, this is safe (prevents dry-fire)

**Firmware Requirements:**

- Implement timeout for autofill operation (e.g., 30 seconds max)
- Detect persistent "No Water" condition and indicate error to user
- Do not allow heater operation if level probe is invalid/unconnected
- Log SRif connection status during diagnostics

**Installation Checklist:**

- [ ] Verify SRif wire (J5) is connected to boiler/chassis mounting bolt
- [ ] Test level probe operation: fill boiler manually, verify GPIO4 reads LOW when water touches probe
- [ ] Verify autofill timeout works correctly (disconnect SRif wire, attempt fill, verify timeout)

---

## RP2354 5V Tolerance and Power Sequencing

### Critical Design Constraint

The RP2354 "5V Tolerant" GPIO feature has a critical caveat documented in the datasheet:

**⚠️ IOVDD (3.3V) must be present for the 5V tolerance to be active.**

### Power Sequencing Risk Analysis

| Scenario                             | Risk Level  | Consequence                        |
| ------------------------------------ | ----------- | ---------------------------------- |
| Normal operation (PCB powered first) | ✅ Safe     | IOVDD active, 5V tolerance engaged |
| ESP32 USB powered before PCB         | 🔴 Critical | IOVDD=0V, GPIO latch-up risk       |
| Hot-plugging peripherals             | 🟡 Medium   | Transient violation possible       |
| Power glitch/brownout                | 🟡 Medium   | Brief IOVDD dropout                |

### Protection Mechanisms Implemented

| Protection                   | Component             | Purpose                                                                          |
| ---------------------------- | --------------------- | -------------------------------------------------------------------------------- |
| Series resistors (1kΩ) + TVS | R40-R43, D_UART_TX/RX | 5V tolerance protection (limits fault current to <500µA during power sequencing) |
| Pull-down resistors (4.7kΩ)  | R11-R15, R73-R75      | Ensures defined GPIO states at boot                                              |
| ESD clamps                   | D10-D15               | Additional transient protection                                                  |
| Schottky clamp (BAT54S)      | D16                   | ADC overvoltage protection                                                       |

### Safe Operating Procedures

1. **Always power the control PCB before external peripherals**
2. **Use integrated 5V supply (J15 Pin 1) for ESP32** - never separate USB
3. **Never hot-plug connectors while system is running**
4. **During development**, power PCB via J15 from bench supply before connecting USB

---

## Hardware Interlock Considerations

### Current Design (Software-Controlled)

The current design relies entirely on software control for SSR heater switching:

```
GPIO13/14 → Transistor → SSR Trigger → Heater Element
```

**Risk:** If the MCU crashes with the heater SSR ON, the boiler could overheat.

### Mitigation Layers (Current Implementation)

| Layer | Protection                        | Notes                                    |
| ----- | --------------------------------- | ---------------------------------------- |
| 1     | RP2354 internal watchdog          | 8-second timeout, resets MCU             |
| 2     | SSR pull-down resistors (R14/R15) | SSR OFF when GPIO tristated during reset |
| 3     | Machine thermal fuse              | Factory-installed on boiler              |
| 4     | Pressure relief valve             | Factory-installed mechanical safety      |

### Future Enhancement: Hardware Watchdog (Optional)

For higher safety assurance (commercial/certification), consider adding an external "dead man switch":

| Component  | Part Number                              | Function                            |
| ---------- | ---------------------------------------- | ----------------------------------- |
| TPL5010    | Texas Instruments                        | External watchdog timer             |
| Connection | TPL5010 DONE ← MCU GPIO, WAKE → Pico RUN | Resets MCU if not reset by firmware |

**Implementation:**

- Firmware must toggle the watchdog input every few seconds
- If MCU hangs, TPL5010 pulls RUN low → system reset → SSRs turn OFF

**Note:** This is NOT implemented in the current design. The internal RP2354 watchdog combined with SSR pull-downs provides adequate protection for home/prosumer use.

---

## Missing Zero-Crossing Detection (Design Note)

### Current Limitation

For **phase-angle control** (dimming) of the AC pump or heater, a Zero-Cross Detector (ZCD) is required. This is NOT currently implemented.

### Impact

Without ZCD, the SSRs can only operate in "Slow PWM" (Time Proportional) mode:

- Example: 1 second period, 50% duty = 0.5s ON, 0.5s OFF
- This is adequate for thermal inertia (heaters) but NOT for pressure profiling

### For Pressure Profiling (Future Revision)

If AC pump phase control is required:

| Component       | Part Number                         | Function                    |
| --------------- | ----------------------------------- | --------------------------- |
| Optocoupler ZCD | H11AA1                              | Detects AC zero-cross       |
| Connection      | AC_L/AC_N → H11AA1 → GPIO interrupt | Sync PWM to mains frequency |

**Note:** The current HLK-15M05C power supply provides isolated 5V, allowing safe interface with AC via optocoupler.

---

## Reliability & MTBF Estimate

### Component MTBF Analysis

| Component      | Estimated MTBF (hours) | Failure Mode                 | Mitigation                        |
| -------------- | ---------------------- | ---------------------------- | --------------------------------- |
| HLK-15M05C     | 100,000                | Electrolytic capacitor aging | Derated operation, thermal mgmt   |
| Relays (K1-K3) | 500,000 (mechanical)   | Contact wear, coil burnout   | Relay drivers with flyback diodes |
| RP2354 MCU     | 1,000,000+             | Overvoltage, ESD damage      | ESD protection on all GPIO        |
| MOV (RV1)      | Degrades with surges   | Open after multiple surges   | Replaceable, visual inspection    |
| Fuses (F1, F2) | N/A (consumable)       | Normal operation on fault    | Spare fuses provided              |

### System MTBF Estimate

**Estimated System MTBF:** ~50,000 hours (5.7 years) under normal operating conditions

**Assumptions:**

- Operating temperature: 25°C average ambient (derated for higher temps)
- Duty cycle: 2-3 hours/day typical espresso machine usage
- Surge events: <10 per year (typical residential)
- No exposure to extreme humidity or contaminants

**Factors Affecting MTBF:**

- Ambient temperature (MTBF halves for every 10°C above 25°C)
- Voltage surge frequency (MOV degradation)
- Relay switching cycles (pump/valve actuation frequency)

**Recommended Service Interval:** Inspect MOV condition and relay contacts every 3 years or 15,000 operating hours.

---

## Test Points

Test points enable systematic board verification during commissioning and field debugging.

### Power Rail Verification

| TP  | Signal | Purpose           | Expected Value | Tolerance        |
| --- | ------ | ----------------- | -------------- | ---------------- |
| TP1 | GND    | Ground reference  | 0V             | —                |
| TP2 | 5V     | Main power rail   | 5.00V          | ±5% (4.75–5.25V) |
| TP3 | 3.3V   | Logic power rail  | 3.30V          | ±3% (3.20–3.40V) |
| TP4 | 5V_MON | 5V divider output | 1.79V          | ±2%              |

### Analog Signal Verification

| TP  | Signal   | Purpose               | Expected Value | Notes                         |
| --- | -------- | --------------------- | -------------- | ----------------------------- |
| TP5 | ADC_VREF | Reference calibration | 3.00V ±0.5%    | Critical for NTC accuracy     |
| TP6 | ADC0     | Brew NTC signal       | 0.5–2.5V       | Varies with temperature       |
| TP7 | ADC1     | Steam NTC signal      | 0.5–2.5V       | Varies with temperature       |
| TP8 | ADC2     | Pressure signal       | 0.32–2.88V     | 0 bar = 0.32V, 16 bar = 2.88V |

### Communication Verification

| TP   | Signal   | Purpose             | Expected Value      | Notes                 |
| ---- | -------- | ------------------- | ------------------- | --------------------- |
| TP9  | UART0_TX | Serial debug output | 3.3V idle           | Scope for TX activity |
| TP10 | UART0_RX | Serial debug input  | 3.3V idle           | Scope for RX activity |
| TP11 | RS485_DE | Direction control   | 0V (RX) / 3.3V (TX) | Verify bus direction  |

### Commissioning Checklist

1. **Power-up sequence:** Verify TP2 (5V) → TP3 (3.3V) → TP5 (ADC_VREF)
2. **Analog calibration:** Measure TP5 with 4.5-digit DMM, record for firmware offset
3. **Sensor verification:** Compare TP6/TP7 readings against reference thermometer
4. **Communication test:** Verify UART activity on TP9/TP10 during ESP32 handshake
5. **RS485 verification:** Monitor TP11 during meter polling (should toggle)
