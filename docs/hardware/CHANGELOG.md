# Hardware Specification Changelog

## Revision History

| Rev      | Date         | Description                                                                               |
| -------- | ------------ | ----------------------------------------------------------------------------------------- |
| **2.31** | **Jan 2026** | **CURRENT** - RP2354 Discrete MCU Migration (Module → Chip, SWD Support, USB-C)           |
| 2.30     | Jan 06 2026  | SMD Transition (SMD Fuses, V-Chip Caps) & SRif Grounding                                  |
| 2.29     | Dec 22 2025  | SRif Grounding Architecture (Remove PE from PCB, Chassis Reference System)                |
| 2.28     | Dec 14 2025  | RP2350 Engineering Verification ECOs (5V tolerance, bulk cap, safety docs)                |
| 2.27     | Dec 14 2025  | Schematic diagram clarifications (VREF, Level Probe, 5V Protection), Netlist cleanup      |
| 2.26.1   | Dec 11 2025  | Improved SSR wiring diagrams, fixed J26 pin number inconsistencies                        |
| 2.25     | Dec 9 2025   | J15 Pin 6 SPARE1, removed SW2/R72 (BOOTSEL not available on Pico header)                  |
| 2.24.2   | Dec 7 2025   | 🔴 SAFETY FIXES: Wien gain corrected, MOV relocated, JP5 added                            |
| 2.24.1   | Dec 7 2025   | CRITICAL FIXES: Buck feedback, Wien gain (wrong value), VREF isolation (DO NOT FABRICATE) |
| 2.24     | Dec 2025     | Critical VREF buffer, TC protection, ratiometric ADC (DO NOT FABRICATE)                   |
| 2.23     | Dec 2025     | Design review action items (warnings, coating)                                            |
| 2.22     | Dec 2025     | Engineering review fixes (thermal, GPIO protection)                                       |
| 2.21.1   | Dec 2025     | Pico 2 compatibility fixes, power supply                                                  |
| 2.21     | Dec 2025     | External power metering, multi-machine NTC support                                        |
| 2.20     | Dec 2025     | Unified 22-pos screw terminal (J26)                                                       |
| 2.19     | Dec 2025     | Removed spare relay K4                                                                    |
| 2.17     | Nov 2025     | Brew-by-weight support (J15 8-pin)                                                        |
| 2.16     | Nov 2025     | Production-ready specification                                                            |

---

## v2.31 (January 2026) - RP2354 Discrete MCU Migration

**Architecture Shift: Module to Discrete Chip**

This revision migrates from the Raspberry Pi Pico 2 module to the discrete RP2354 microcontroller chip. This change eliminates the external flash requirement (2MB stacked flash included) and enables factory flash capability via SWD interface.

### 🔴 Critical Architecture Changes

#### 1. MCU Replacement: Pico 2 Module → RP2354 Discrete

| Component | Old (v2.30)                  | New (v2.31)          | Reason                               |
| --------- | ---------------------------- | -------------------- | ------------------------------------ |
| **U1**    | Raspberry Pi Pico 2 (SC0942) | **RP2354A (QFN-60)** | Discrete chip with 2MB stacked flash |
| **J20**   | 2×20 Header (Pico socket)    | **REMOVED**          | No longer needed (direct chip mount) |

**Benefits:**

- **2MB stacked flash** eliminates external QSPI flash chip
- **Lower BOM cost** (no module premium)
- **Factory flash capability** via SWD interface
- **Access to GPIO23, 24, 25, 29** previously internal to module

#### 2. New Components Required

| Component         | Qty | Value              | Purpose                                           |
| ----------------- | --- | ------------------ | ------------------------------------------------- |
| **Crystal**       | 1   | **12 MHz**         | Main clock source (required for USB/timing)       |
| **Load Caps**     | 2   | ~10-22pF           | Crystal load capacitors (check crystal datasheet) |
| **Resistor**      | 1   | 1kΩ                | Serial termination for crystal OUT (if needed)    |
| **Inductor**      | 1   | **2.2µH**          | For internal core voltage SMPS (VREG_OUT/DVDD)    |
| **Capacitors**    | ~6  | 100nF, 1µF, 10µF   | Decoupling for 3.3V (IOVDD) and 1.1V (DVDD) rails |
| **USB Connector** | 1   | **USB-C (16-pin)** | Essential for recovery and USB development        |
| **Resistors**     | 2   | 27Ω                | USB D+/D- series termination                      |
| **Resistors**     | 2   | 5.1kΩ              | USB-C CC1/CC2 pull-downs                          |

#### 3. Power Supply Changes

**RP2354 Power Architecture:**

- **VREG_IN:** Connect to +3.3V (from TPS563200)
- **VREG_OUT:** Connect to **DVDD** (Core voltage pins, 1.1V)
- **LX:** Connect **2.2µH inductor** to VREG_OUT/DVDD
- **IOVDD:** All GPIO pins require 3.3V (from TPS563200)
- **Decoupling:** Place 100nF caps close to every IOVDD pin; at least one 1µF/10µF on DVDD rail

**Internal Regulator:**
The RP2354 has an internal regulator that can run in LDO or SMPS mode. This design uses SMPS mode (via 2.2µH inductor) for efficiency, matching the Pico 2's approach.

#### 4. GPIO Mapping (Compatible)

**No GPIO allocation changes required.** The RP2354A (QFN-60) exposes GPIO 0-29 directly, and all existing netlist signals map directly to corresponding GPIO pins.

**GPIO Availability:**

- **GPIO16 & 22:** Now available (disconnected from J15, traces moved to dedicated SWD pins)
- **GPIO23 & 25:** Now available (previously internal to Pico module)
- **GPIO24:** Used for VBUS Detect (USB-C power detection via 10kΩ/20kΩ divider)
- **GPIO29:** Used for ADC3 - 5V_MONITOR (ratiometric pressure compensation, from v2.24)

GPIO23 and 25 can be routed to test points or an expansion header.

#### 5. USB & Boot Support

**USB-C Connector:**

- Route **USB_D+** and **USB_D-** to USB-C connector
- Enables manual flashing via ROM bootloader (PicoBoot) if ESP32 is down
- Essential for development and "Plan B" recovery

**BOOTSEL Button:**

- Add tactile button connecting **CS** pin (QSPI_SS) to Ground (via ~1k resistor)
- Note: On RP2354 with stacked flash, BOOTSEL functionality is handled via QSPI_SS line - holding flash CS low on boot enters ROM bootloader

#### 6. SWD Interface for Factory Flash (CRITICAL)

**Problem:** A fresh RP2354 has no firmware. It will boot into USB ROM Bootloader. The UART bootloader described in OTA updates relies on _application firmware_ being present. **UART OTA cannot flash a blank chip.**

**Solution: Implement SWD via ESP32**

The most robust approach is to wire the RP2354's **SWD (Serial Wire Debug)** interface to the ESP32. This allows the ESP32 to act as a hardware programmer (like a Picoprobe).

**J15 Connector Changes:**

| Pin | Old (v2.30)      | New (v2.31)    | Notes                                    |
| --- | ---------------- | -------------- | ---------------------------------------- |
| 5   | PICO_RUN (Reset) | **RP2354 RUN** | Unchanged                                |
| 6   | SPARE1 (GPIO16)  | **SWDIO**      | RP2354 SWDIO (dedicated pin) → J15 Pin 6 |
| 8   | SPARE2 (GPIO22)  | **SWCLK**      | RP2354 SWCLK (dedicated pin) → J15 Pin 8 |

**Important:** SWDIO and SWCLK are **dedicated physical pins** on the RP2354, NOT multiplexed on GPIO 16/22. GPIO 16 and 22 are now available for other uses since J15 traces connect to the dedicated SWD pins.

**Why SWD is Better:**

1. **Factory Flash:** ESP32 can bit-bang SWD protocol to flash blank RP2354
2. **Unbrickable:** If bad OTA corrupts software bootloader, UART OTA is dead. SWD works at hardware level and can recover anytime
3. **Debugging:** Enables remote debugging (GDB) on RP2354 via ESP32 if needed

**Software Implication:**
ESP32 firmware will need to support SWD flashing (porting a DAP implementation). If not implemented immediately, USB-C connector provides "Plan B" recovery.

**Recommendation:** Implement SWD wiring on PCB **now**. It costs nothing and enables factory flash capability.

#### 7. Reset Button

Keep `SW1` from BOM, but ensure it pulls the RP2354 `RUN` pin to ground (same as Pico 2).

#### 8. 5V Tolerance Protection

**CRITICAL:** The RP2354 GPIOs are **NOT 5V tolerant** (and fail safely only if IOVDD is powered). Ensure current limiting resistors (`R40`, `R41`, etc.) are kept in the new layout to protect the chip from 5V peripherals (relay drivers, external sensors).

### BOM Impact

| Change Type                           | Count   | Est. Cost Impact                   |
| ------------------------------------- | ------- | ---------------------------------- |
| MCU change (module → chip)            | 1       | **-$2-3** (module premium removed) |
| New components (crystal, caps, USB-C) | ~12     | **+$1.50**                         |
| Removed (J20 socket)                  | 1       | **-$0.50**                         |
| **Total BOM Δ**                       | **~13** | **~-$1.00 savings**                |

### Files Modified

| File                         | Changes                                                                             |
| ---------------------------- | ----------------------------------------------------------------------------------- |
| `spec/07-BOM.md`             | Replaced Pico 2 module with RP2354, added crystal, inductor, USB-C, decoupling caps |
| `spec/02-GPIO-Allocation.md` | Updated MCU spec, noted GPIO23-29 now available                                     |
| `spec/03-Power-Supply.md`    | Added RP2354 power architecture (VREG, DVDD, IOVDD)                                 |
| `spec/06-Connectors.md`      | Updated J15 for SWD, added USB-C connector, removed J20                             |
| `spec/08-PCB-Layout.md`      | Updated footprint requirements, USB-C placement                                     |
| `spec/01-Overview.md`        | Updated MCU specification to RP2354                                                 |
| `README.md`                  | Updated version and MCU reference                                                   |
| `CHANGELOG.md`               | This entry                                                                          |

### Design Verdict

**Status:** Production Ready - Architecture Migration

This migration from module to discrete chip provides cost savings, factory flash capability, and access to previously internal GPIOs. The SWD interface enables robust recovery and factory programming. All existing GPIO allocations remain compatible.

**⚠️ IMPORTANT:** When fabricating boards with this revision:

- Verify U1 footprint is QFN-60 (RP2354A) or QFN-80 (RP2354B)
- Ensure 12MHz crystal + load caps are placed close to XIN/XOUT
- Verify 2.2µH inductor on VREG_OUT/DVDD path
- Place decoupling caps (100nF on all IOVDD, 1µF/10µF on DVDD)
- Route SWDIO/SWCLK to J15 Pins 6/8
- Include USB-C connector for recovery
- Add BOOTSEL button (CS to GND via 1kΩ)

---

## v2.30 (January 06, 2026) - SMD Transition

**Optimization for Manufacturing & Size**

- **Fuses:** Replaced THT 5x20mm glass fuses with Littelfuse 463 Series **SMD Nano²** fuses.
  - _Benefit:_ Reduces component height from ~15mm to ~4mm. Removes manual assembly step.
- **Capacitors:** Switched 5V Bulk Capacitor (C2) to **SMD V-Chip** Aluminum Electrolytic.
  - _Benefit:_ Streamlines assembly.
- **Safety:** SMD fuses selected specifically for **250VAC High Breaking Capacity** (500A) to prevent arc explosion faults.

---

## v2.29 (December 22, 2025) - SRif Grounding Architecture

**🔴 CRITICAL SAFETY DESIGN CHANGE: Eliminates L-to-Earth Short Risk**

This revision implements the "Gicar-style" **Chassis Reference (SRif)** grounding system to eliminate the risk of L-to-Earth shorts that could cause explosive arcing. The High Voltage Earth (PE) track is completely removed from the PCB input, significantly reducing the risk of catastrophic failures while maintaining safe and reliable level probe operation.

### Core Design Change

**Previous Architecture (v2.28):**

- PE (Protective Earth) connected to PCB via J1 and J24
- MH1 mounting hole was PTH (Plated Through Hole) serving as PE star ground point
- Level probe return path via internal PE connection

**New Architecture (v2.29):**

- **HV Section Floating:** No Earth connection on PCB High Voltage side
- **LV Section Grounded:** Logic Ground (GND) connects to chassis via dedicated J5 (SRif) wire
- **All Mounting Holes NPTH:** No random grounding through screws
- **Level Probe Return:** Explicit path via Boiler → Chassis → J5 (SRif) → PCB GND

### 🔴 Critical Hardware Changes

#### 1. Connector Modifications

| Connector | Old Specification           | New Specification                | Impact                             |
| --------- | --------------------------- | -------------------------------- | ---------------------------------- |
| **J1**    | 3-Pin Spade (L, N, PE)      | **2-Pin Spade (L, N only)**      | PE pin removed from mains input    |
| **J24**   | 3-Position Screw (L, N, PE) | **2-Position Screw (L, N only)** | PE pin removed from power meter HV |
| **J5**    | (Not present)               | **1-Pin 6.3mm Spade (SRif)**     | New chassis reference connector    |

**J5 (SRif) Wiring:**

- Connect J5 to PCB GND (Logic Ground)
- Wire to boiler/chassis mounting bolt using 18AWG Green/Yellow wire
- Required for level probe operation and logic grounding

#### 2. Mounting Holes

| Hole    | Old (v2.28)         | New (v2.29)         | Reason                                  |
| ------- | ------------------- | ------------------- | --------------------------------------- |
| **MH1** | PTH (PE star point) | **NPTH (isolated)** | Prevent random grounding through screws |
| MH2-MH4 | NPTH (isolated)     | NPTH (isolated)     | No change                               |

**Rationale:** We do NOT want the board grounding itself randomly through the screws. Grounding happens ONLY through the J5 SRif wire to prevent ground loops and ensure controlled current paths.

#### 3. PCB Layout Isolation Requirements

**Enhanced Keep-Out Zone:**

- Maintain strict **6mm (240 mil) Keep-Out Zone** between any High Voltage trace (L, N, Relay Contacts) and the Low Voltage Ground Pour
- **Delete all copper** within 6mm of the L/N tracks
- Verify no Ground plane is under the HLK module's AC pins

**Safety Benefit:**
If 'L' shorts to the PCB surface, it hits FR4 (fiberglass), not a Ground Plane. It cannot create a dead short or explode. The fuse will eventually blow if it finds a path, but the "Explosive Arc" risk is eliminated.

### Grounding Strategy Update

**New SRif Architecture:**

```
1. MACHINE CHASSIS is Earthed via the main power cord (Wall Plug)
2. PCB HV SIDE is Floating (No Earth connection)
3. PCB LV SIDE (GND) connects to CHASSIS via J5 (SRif)
```

**Level Probe Return Path:**

```
AC_OUT (U6) ──► J26-5 ──► PROBE ──► WATER ──► BOILER BODY
                                                       │
PCB GND ◄────── J5 (SRif) ◄────── WIRE ◄───────────┘
```

### Component Changes

| Component | Change                  | Notes                       |
| --------- | ----------------------- | --------------------------- |
| J1        | 3-pin → 2-pin connector | Remove PE terminal          |
| J24       | 3-pos → 2-pos terminal  | Remove PE terminal          |
| J5        | **NEW** - 6.3mm spade   | Chassis reference connector |
| MH1       | PTH → NPTH              | Isolated mounting hole      |

### BOM Impact

| Change Type        | Count | Est. Cost Impact            |
| ------------------ | ----- | --------------------------- |
| Connector changes  | 2     | ~$0 (same part, fewer pins) |
| New connector (J5) | 1     | ~$0.10                      |
| **Total BOM Δ**    | **3** | **+$0.10**                  |

### Files Modified

| File                                | Changes                                                                       |
| ----------------------------------- | ----------------------------------------------------------------------------- |
| `schematics/Schematic_Reference.md` | Removed PE from J1/J24, added J5 SRif, updated level probe return path        |
| `spec/06-Connectors.md`             | Updated J1/J24 to 2-pin, added J5 specification                               |
| `spec/08-PCB-Layout.md`             | Changed MH1 to NPTH, updated grounding strategy, added isolation requirements |
| `spec/09-Safety.md`                 | Replaced grounding hierarchy with SRif architecture, updated safety notes     |
| `spec/07-BOM.md`                    | Updated connector quantities and specifications                               |
| `spec/03-Power-Supply.md`           | Removed PE from mains input diagram                                           |
| `README.md`                         | Updated connector quick reference and safety warnings                         |
| `CHANGELOG.md`                      | This entry                                                                    |

### Design Rationale

**Problem Solved:**
The previous design brought Mains Earth (PE) onto the PCB High Voltage side, creating a risk of L-to-Earth shorts. If a component failure or contamination caused Live voltage to arc to the nearby PE trace/plane, it could create a dead short with explosive consequences.

**Solution:**
By floating the HV section and grounding only the LV section via a dedicated SRif wire, we:

1. Eliminate the risk of L-to-Earth shorts on the PCB
2. Maintain safe level probe operation through controlled return path
3. Prevent ground loops by ensuring single-point grounding
4. Improve safety margin for fault conditions

**Industry Standard:**
This approach follows the "Gicar-style" grounding architecture used in professional espresso machine control systems, where logic ground is referenced to chassis but HV remains floating.

### Design Verdict

**Status:** Production Ready - Critical Safety Improvement

This design change addresses a fundamental safety risk in the previous architecture. The SRif grounding system is a proven approach that eliminates the explosive arc risk while maintaining all functional requirements. All documentation has been updated to reflect the new architecture.

**⚠️ IMPORTANT:** When fabricating boards with this revision:

- Verify J1 and J24 are 2-pin/2-position connectors (not 3-pin)
- Ensure J5 (SRif) connector is included in BOM
- All mounting holes must be NPTH (Non-Plated Through Hole)
- Maintain 6mm Keep-Out Zone in PCB layout between HV and LV sections

---

## v2.28 (December 14, 2025) - RP2350 Engineering Verification ECOs

**Comprehensive engineering design verification of RP2350 implementation addressing critical 5V tolerance, power sequencing, and analog signal chain integrity.**

Based on detailed engineering analysis against RP2350 datasheet requirements and IEC safety standards.

### 🔴 Critical Engineering Change Orders (ECOs)

#### ECO-03: UART/GPIO 5V Tolerance Protection (CRITICAL)

| Component | Old Value | New Value | Reason                                                         |
| --------- | --------- | --------- | -------------------------------------------------------------- |
| R40       | 33Ω       | **1kΩ**   | ESP32 TX→Pico RX: Limits fault current during power sequencing |
| R41       | 33Ω       | **1kΩ**   | ESP32 RX←Pico TX: Limits fault current during power sequencing |
| R42       | 33Ω       | **1kΩ**   | Meter TX: 5V tolerance protection                              |
| R43       | 33Ω       | **1kΩ**   | Meter RX: 5V tolerance protection                              |

**Technical Background:**
The RP2350 "5V Tolerant" GPIO feature requires IOVDD (3.3V) to be present for tolerance to be active. If an external peripheral (ESP32 via separate USB power) initializes before the Pico's buck converter, a condition exists where V_GPIO=5V and IOVDD=0V. In this state, internal protection diodes are unpowered, causing forward-bias through parasitic diodes to the 3.3V rail, resulting in silicon latch-up or burnout.

**Solution:** 1kΩ series resistors limit fault current to <500µA during power sequencing anomalies, providing safe margin even when IOVDD is absent.

#### ECO-05: 3.3V Rail Bulk Capacitor (MAJOR)

| Component | Old    | New                   | Reason                            |
| --------- | ------ | --------------------- | --------------------------------- |
| C5        | (none) | **47µF 10V X5R 1206** | 3.3V rail transient stabilization |

**Technical Background:**
The Pico 2 module has limited onboard capacitance. When driving external loads (ESP32 logic, sensor buffers), the rail can experience brownouts during:

- WiFi transmission bursts (if using Pico 2 W)
- Relay switching transients (back-EMF coupling)
- ESP32 current spikes (up to 500mA during WiFi TX)

**Total 3.3V Rail Capacitance:** 22µF + 22µF + 47µF = **91µF** (adequate for ~1A transient load)

### 🟡 Documentation Enhancements

#### 5V Tolerance Power Sequencing Warning (GPIO Spec)

Added comprehensive documentation on RP2350 5V tolerance constraints:

- Risk scenario analysis for parallel USB power sources
- Safe operating procedures for development and production
- Protection mechanism summary table

#### Hardware Interlock Design Notes (Safety Spec)

Added analysis of current software-controlled SSR design:

- Mitigation layers (watchdog, pull-downs, thermal fuse)
- Optional external hardware watchdog (TPL5010) for commercial certification
- Current design assessed as adequate for home/prosumer use

#### Zero-Crossing Detection Note (Safety Spec)

Documented current limitation for AC phase control:

- Slow PWM (time proportional) mode only
- Future ZCD implementation path for pressure profiling
- Component recommendations (H11AA1 optocoupler)

### ✅ Verified Design Elements

The engineering review confirmed these existing design choices as correct:

| Element                 | Implementation          | Verification                                               |
| ----------------------- | ----------------------- | ---------------------------------------------------------- |
| Pressure sensor divider | R4=5.6kΩ, R3=10kΩ       | ✅ Maps 0.5-4.5V → 0.32-2.88V (within 3.0V ADC ref)        |
| ADC overvoltage clamp   | BAT54S (D16)            | ✅ Clamps to 3.6V max on open-circuit fault                |
| SSR pull-downs          | R14/R15 4.7kΩ           | ✅ Ensures heaters OFF during MCU reset/boot               |
| Precision ADC reference | LM4040 + OPA2342 buffer | ✅ Robust design for high-current sensor loads             |
| Fluid level AC drive    | Wien bridge oscillator  | ✅ Superior to DC sensing, prevents electrode electrolysis |
| Flyback diodes          | UF4007 (D1-D3)          | ✅ Fast recovery prevents relay contact damage             |

### BOM Impact

| Change Type               | Count | Est. Cost Impact |
| ------------------------- | ----- | ---------------- |
| Value changes (resistors) | 4     | ~$0.10           |
| New component (C5)        | 1     | ~$0.15           |
| **Total BOM Δ**           | **5** | **+$0.25**       |

### Files Modified

| File                         | Changes                                                                      |
| ---------------------------- | ---------------------------------------------------------------------------- |
| `spec/02-GPIO-Allocation.md` | 5V tolerance section, UART series resistor values, power sequencing warnings |
| `spec/03-Power-Supply.md`    | 3.3V bulk capacitor (C5) requirement and circuit diagram                     |
| `spec/07-BOM.md`             | Updated R40-R43 values, added C5                                             |
| `spec/09-Safety.md`          | Power sequencing risks, hardware interlock analysis, ZCD design notes        |
| `CHANGELOG.md`               | This entry                                                                   |

### Design Verdict

**Status:** Production Ready with ECOs Applied

The RP2350-based BrewOS hardware demonstrates high design sophistication, particularly in the precision analog signal conditioning for temperature control. The identified ECOs address power sequencing edge cases that could cause field failures. With these changes applied, the design is qualified as a robust platform for commercial/prosumer espresso machine control.

**Acknowledgment:** Analysis based on comprehensive engineering design verification report reviewing RP2350 datasheet requirements, IEC 60335-1 safety standards, and best practices for embedded thermal control systems.

---

## v2.27 (December 14, 2025) - Schematic Diagram Improvements

**Comprehensive diagram updates for clarity and consistency. No functional hardware changes from v2.26.**

### Diagram Improvements (Schematic_Reference.md)

1.  **5V Rail Protection:**

    - Redesigned to clearly show FB2 in series and D20/C2 in parallel
    - Added clear signal flow and "Why" section

2.  **JP4 Mode Selection:**

    - Clarified 3-pad jumper topology (RS485 vs TTL)
    - Added distinct visual guides for both modes

3.  **Level Probe Circuit:**

    - Complete redesign of the 3-stage circuit (Wien Bridge, Interface, Comparator)
    - Fixed broken connection lines and incorrect resistor values in diagram
    - Added explicit signal flow explanation

4.  **ADC Voltage Reference:**

    - Clarified distinction between `REF_3V0` (LM4040) and `ADC_VREF` (Buffered)
    - Added unity-gain buffer schematic to show how high-current drive is achieved

5.  **GPIO Pull-downs (E9 Errata):**
    - Fixed diagrams for WEIGHT_STOP (GPIO21), SPARE1 (GPIO16), SPARE2 (GPIO22)
    - Corrected pull-down connection to GND (was ambiguously drawn)
    - Verified 4.7kΩ values match component list

### Documentation Fixes

- **Netlist Summary:** Renamed `+3.3V_ANALOG` to `ADC_VREF (3.0V)` for accuracy
- **Pin Mapping:** Corrected GPIO28 source in Netlist from J26-16 to J26-14 (matching J26 pinout)

---

## v2.26.1 (December 11, 2025) - SSR Documentation Improvements

**Improved external SSR relay wiring documentation for hardware engineer clarity**

### Documentation Changes

| File                     | Change                                            |
| ------------------------ | ------------------------------------------------- |
| `Schematic_Reference.md` | Added complete SSR system wiring diagram          |
| `Schematic_Reference.md` | Fixed J26 SSR pin numbers (15-18, not 17-20)      |
| `Schematic_Reference.md` | Fixed J26 pressure pin numbers (12-14, not 14-16) |
| `spec/04-Outputs.md`     | Corrected SSR trigger circuit diagram topology    |

### SSR Section Improvements (Schematic_Reference.md)

1. **Added Complete System Overview Diagram**

   - Shows control PCB, external SSR modules, and machine mains wiring
   - Clear separation between 5V DC control signals and 220V AC load paths
   - Visual representation of what connects where

2. **Added Quick Reference Wiring Table**

   | Function      | PCB Terminal | Wire To        | Signal Type  | Wire Gauge |
   | ------------- | ------------ | -------------- | ------------ | ---------- |
   | SSR1 Ctrl (+) | J26-15       | SSR1 DC (+)    | +5V DC       | 22-26 AWG  |
   | SSR1 Ctrl (-) | J26-16       | SSR1 DC (-)    | Switched GND | 22-26 AWG  |
   | SSR2 Ctrl (+) | J26-17       | SSR2 DC (+)    | +5V DC       | 22-26 AWG  |
   | SSR2 Ctrl (-) | J26-18       | SSR2 DC (-)    | Switched GND | 22-26 AWG  |
   | SSR Load (L)  | NOT ON PCB   | Machine Live   | 220V AC      | 14-16 AWG  |
   | SSR Load (T)  | NOT ON PCB   | Heater Element | 220V AC      | 14-16 AWG  |

3. **Added Key Points for Hardware Engineer**

   - J26 pins 15-18 carry ONLY 5V DC control signals (<20mA per channel)
   - SSR AC terminals connect to existing machine wiring, NOT to this PCB
   - External SSRs must be mounted on heatsink
   - SSR optocoupler provides 3kV isolation between control and load

4. **Improved PCB Detail Diagram**
   - Clear J26 terminal block layout for SSR pins
   - Operation state table (GPIO HIGH/LOW → SSR ON/OFF)
   - Visual separation between on-board components and external wiring

### Pin Number Corrections

| Section             | Old (Incorrect) | New (Correct) |
| ------------------- | --------------- | ------------- |
| SSR1 Control        | J26-17/18       | J26-15/16     |
| SSR2 Control        | J26-19/20       | J26-17/18     |
| Pressure Transducer | J26-14/15/16    | J26-12/13/14  |

### 04-Outputs.md Fix

**Problem:** SSR trigger circuit diagram showed LED and external SSR in series topology.

**Fix:** Corrected to parallel topology matching Schematic_Reference.md:

- Path 1: +5V → J26-15 (SSR+) → External SSR → J26-16 (SSR-) → Transistor → GND
- Path 2: +5V → R34 (330Ω) → LED → Transistor → GND

### No Hardware Changes

This revision contains **documentation improvements only**. No component values, footprints, or netlist changes.

---

## v2.26 (December 9, 2025) - Spare GPIO Pull-downs

**Both SPARE1 and SPARE2 now connect ESP32 to Pico with proper pull-down resistors**

### Changes

| Component          | Change                     | Reason                               |
| ------------------ | -------------------------- | ------------------------------------ |
| SPARE1 (J15 Pin 6) | ESP32 GPIO9 → Pico GPIO16  | Bidirectional spare I/O              |
| SPARE2 (J15 Pin 8) | ESP32 GPIO22 → Pico GPIO22 | Bidirectional spare I/O              |
| R74                | Added 4.7kΩ pull-down      | SPARE1 (GPIO16) RP2350 E9 mitigation |
| R75                | Added 4.7kΩ pull-down      | SPARE2 (GPIO22) RP2350 E9 mitigation |

### New Components

| Ref | Value | Package | Function                  |
| --- | ----- | ------- | ------------------------- |
| R74 | 4.7kΩ | 0805    | SPARE1 pull-down (GPIO16) |
| R75 | 4.7kΩ | 0805    | SPARE2 pull-down (GPIO22) |

### J15 Connector Pinout (Final)

| Pin | Signal      | ESP32 GPIO  | Pico GPIO  | Protection      |
| --- | ----------- | ----------- | ---------- | --------------- |
| 1   | 5V          | VIN         | VSYS       | -               |
| 2   | GND         | GND         | GND        | -               |
| 3   | TX          | GPIO17 (RX) | GPIO0 (TX) | 33Ω series      |
| 4   | RX          | GPIO18 (TX) | GPIO1 (RX) | 33Ω series      |
| 5   | RUN         | GPIO8       | RUN        | 10kΩ pull-up    |
| 6   | SPARE1      | GPIO9       | GPIO16     | 4.7kΩ pull-down |
| 7   | WEIGHT_STOP | GPIO10      | GPIO21     | 4.7kΩ pull-down |
| 8   | SPARE2      | GPIO22      | GPIO22     | 4.7kΩ pull-down |

---

## v2.25 (December 9, 2025) - J15 SPARE1 Pin

**J15 Pin 6 changed from BOOTSEL control to general-purpose GPIO (SPARE1)**

### Background

The Pico's BOOTSEL function is controlled by the QSPI_SS signal, which is NOT exposed on the 40-pin header. Remote BOOTSEL control from ESP32 was never functional without soldering to a test point on the Pico module.

### Changes

| Component | Change           | Reason                                    |
| --------- | ---------------- | ----------------------------------------- |
| J15 Pin 6 | BOOTSEL → SPARE1 | QSPI_SS not on Pico header                |
| SW2       | Removed          | Cannot function without physical Pico mod |
| R72       | Removed (DNP)    | No longer needed                          |
| J15 Pin 8 | SPARE → SPARE2   | Renamed for consistency                   |

### Component Removal

| Ref | Description                  | Reason                                                                                   |
| --- | ---------------------------- | ---------------------------------------------------------------------------------------- |
| SW2 | Tactile 6×6mm BOOTSEL button | BOOTSEL requires direct connection to Pico QSPI_SS (TP6), not available on 40-pin header |
| R72 | 10kΩ BOOTSEL pull-up         | No BOOTSEL signal on PCB                                                                 |

### J15 Connector Pinout (Updated)

| Pin | Signal      | Direction | Description                |
| --- | ----------- | --------- | -------------------------- |
| 1   | 5V          | →         | ESP32 VIN power            |
| 2   | GND         | -         | Common ground              |
| 3   | TX          | →         | Pico GPIO0 → ESP32 RX      |
| 4   | RX          | ←         | ESP32 TX → Pico GPIO1      |
| 5   | RUN         | ←         | ESP32 GPIO8 → Pico RUN     |
| 6   | SPARE1      | -         | Available (ESP32 GPIO9)    |
| 7   | WEIGHT_STOP | ←         | ESP32 GPIO10 → Pico GPIO21 |
| 8   | SPARE2      | -         | Available (Pico GPIO22)    |

### OTA/Recovery Strategy

- **Normal OTA:** ESP32 triggers Pico software bootloader via UART command
- **Recovery:** Physical access required - press BOOTSEL button on Pico module while resetting

---

## v2.24.3 (December 7, 2025) - ⚠️ PRODUCTION READINESS ECO

**Engineering Change Orders based on comprehensive production readiness review**

### Critical Issues Addressed

| Severity | Component         | Issue                    | Fix Applied                           | Impact                        |
| -------- | ----------------- | ------------------------ | ------------------------------------- | ----------------------------- |
| CRITICAL | F2                | Fuse undersized          | 500mA → **2A**                        | Prevents nuisance tripping    |
| CRITICAL | MH1/TC            | Thermocouple ground loop | Documentation + isolation requirement | Prevents MAX31855 faults      |
| HIGH     | R20-R22, R24-R25  | Relay saturation         | 1kΩ → **470Ω**                        | Guarantees relay pull-in      |
| MEDIUM   | R11-R15, R19, R73 | RP2350 errata E9         | 10kΩ → **4.7kΩ**                      | Prevents GPIO latch-up        |
| MEDIUM   | R93-R94           | RS485 failsafe (missing) | **Added** 20kΩ biasing resistors      | Idle bus noise immunity       |
| MEDIUM   | D23-D24           | Service port protection  | **Added** 3.3V zener clamps           | Protects from 5V TTL adapters |

### Component Value Changes

| Ref | v2.24.2 | v2.24.3       | Reason                                    |
| --- | ------- | ------------- | ----------------------------------------- |
| F2  | 500mA   | **2A**        | HLK module can source 3A; 500mA too tight |
| R11 | 10kΩ    | **4.7kΩ**     | RP2350 E9: 10kΩ marginal for 200µA leak   |
| R12 | 10kΩ    | **4.7kΩ**     | RP2350 E9: 10kΩ marginal for 200µA leak   |
| R13 | 10kΩ    | **4.7kΩ**     | RP2350 E9: 10kΩ marginal for 200µA leak   |
| R14 | 10kΩ    | **4.7kΩ**     | RP2350 E9: 10kΩ marginal for 200µA leak   |
| R15 | 10kΩ    | **4.7kΩ**     | RP2350 E9: 10kΩ marginal for 200µA leak   |
| R19 | 10kΩ    | **4.7kΩ**     | RP2350 E9: 10kΩ marginal for 200µA leak   |
| R20 | 1kΩ     | **470Ω**      | Forced β=30→14.5 for hard saturation      |
| R21 | 1kΩ     | **470Ω**      | Forced β=30→14.5 for hard saturation      |
| R22 | 1kΩ     | **470Ω**      | Forced β=30→14.5 for hard saturation      |
| R24 | 1kΩ     | **470Ω**      | Forced β=30→14.5 for hard saturation      |
| R25 | 1kΩ     | **470Ω**      | Forced β=30→14.5 for hard saturation      |
| R73 | 10kΩ    | **4.7kΩ**     | RP2350 E9: 10kΩ marginal for 200µA leak   |
| R93 | (none)  | **20kΩ**      | RS485 A line failsafe pull-up (NEW)       |
| R94 | (none)  | **20kΩ**      | RS485 B line failsafe pull-down (NEW)     |
| D23 | (none)  | **BZT52C3V3** | Service port TX 3.3V clamp (NEW)          |
| D24 | (none)  | **BZT52C3V3** | Service port RX 3.3V clamp (NEW)          |

### Technical Analysis

#### 1. Fuse F2 Undersizing (CRITICAL)

**Problem:** F2 rated at 500mA, but combined load approaches 1A during startup:

- ESP32 WiFi burst: 500mA
- Pico + peripherals: 300mA
- Relay coils (simultaneous): 200mA
- **Total: ~1A peak, ~700mA steady-state**

**Fix:** Increased to 2A slow-blow. HLK-15M05C can deliver 3A (15W), so 2A provides protection while allowing normal operation.

#### 2. Thermocouple Ground Loop (CRITICAL - PRODUCTION BLOCKER)

**Problem:** MH1 connects PCB GND to Protective Earth. ECM Synchronika uses grounded-junction thermocouples (TC tip welded to M6 sheath, which contacts boiler body, which is grounded to PE). This creates:

```
Boiler → PE → MH1 → PCB GND → MAX31855 T- pin → Thermocouple
```

MAX31855 T- pin is internally biased to VCC/2 and must NOT be shorted to ground. Result: "Short to GND" fault, NaN readings.

**Solutions:**

1. **Digital Isolation** (professional): Add ADuM1201 + isolated DC-DC for MAX31855 circuit
2. **Ungrounded TC Only** (operational restriction): Specify insulated-junction probes only
3. **Float Logic GND** (not recommended): Remove MH1-PE connection (safety/EMI risk)

**Action Taken:** Added critical warning in documentation. Recommend Solution 1 before production.

#### 3. Relay Driver Saturation (HIGH)

**Problem:** 1kΩ base resistors provide only 2.6mA base current from 3.3V GPIO:

- Coil current (K2): 80mA
- Forced β = 80mA / 2.6mA ≈ 30
- MMBT2222A specs require β ≤ 10 for hard saturation (<0.1V Vce(sat))

At β=30, transistor operates in linear region (Vce ≈ 0.3-0.6V), reducing relay coil voltage to ~4.5V. This marginally meets "Must Operate" spec (75% of 5V = 3.75V) but reduces reliability.

**Fix:** 470Ω provides 5.5mA base current → forced β ≈ 14.5 → hard saturation → Vce(sat) < 0.2V → relay sees 4.8V.

#### 4. RP2350 GPIO Latch-Up Errata E9 (MEDIUM)

**Problem:** RP2350 silicon erratum where GPIO inputs with internal pull-downs can latch at ~2.1V due to pad leakage current (120-200µA typ). A 10kΩ resistor with 200µA develops 2.0V, failing to hold logic low (threshold typically 0.3×Vcc = 0.99V).

**Fix:** 4.7kΩ resistors ensure even worst-case 200µA creates only 0.94V, recognized as valid logic low.

**Affected Pins:** R11-R15 (relay/SSR drivers), R19 (RS485 control), R73 (WEIGHT_STOP).

#### 5. RS485 Failsafe Biasing (MEDIUM)

**Problem:** Industrial RS485 best practice requires pull-up on A and pull-down on B to define idle bus state when no driver active. Without bias, floating lines can pick up noise (pump motors, SSR switching) and be misinterpreted as data.

**Fix:** Added R93 (20kΩ to +3.3V) on A line, R94 (20kΩ to GND) on B line.

#### 6. Service Port Input Protection (MEDIUM)

**Problem:** J16 debug port shares GPIO0/1 with ESP32. If technician connects 5V TTL USB-serial adapter, 5V directly applied to RP2350 pins (only 3.3V tolerant with active IOVDD). Series resistors R42/R43 (33Ω) provide virtually no overvoltage protection.

**Fix:** Added D23/D24 (BZT52C3V3) 3.3V zener clamps on TX/RX lines.

### Design Simplification

**Thermocouple Removed (v2.24.3):**

- Eliminated external K-type thermocouple support (U4, D22, C10, C40-C42)
- Freed GPIO16/17/18 (SPI pins) for future expansion
- J26 pins 12-13 now NC (not connected)
- **Rationale**: Boiler NTC sensors provide sufficient temperature control; thermocouple required $5-7 isolation circuit to avoid ground loop; pressure profiling more valuable than group temp monitoring

### BOM Impact

| Change Type        | Count  | Est. Cost Impact   |
| ------------------ | ------ | ------------------ |
| Value changes only | 13     | $0 (same parts)    |
| New components     | 4      | ~$0.50             |
| Removed components | 5      | **-$3.50**         |
| **Total BOM Δ**    | **12** | **-$3.00 savings** |

### Production Status

**DO NOT FABRICATE v2.24.2 without these fixes applied.**

- **F2 fix:** Mandatory (prevents field failures)
- **Relay driver fix:** Mandatory (reliability)
- **RP2350 errata fix:** Strongly recommended (silicon-dependent)
- **Thermocouple isolation:** Critical for grounded TC machines (ECM Synchronika)
- **RS485/Service port:** Recommended for production robustness

**Next Steps:**

1. Update schematic with component value changes
2. Implement thermocouple isolation circuit (ADuM1201 + B0505S)
3. Generate new Gerbers from v2.24.3 design files
4. Order prototype with all ECOs applied
5. Validate fixes before production run

---

## v2.24.2 (December 7, 2025) - 🔴 SAFETY COMPLIANCE FIXES

**Corrects calculation error and safety violations in v2.24.1**

### Critical Corrections to v2.24.1

| Issue              | v2.24.1 (Incorrect)   | v2.24.2 (Corrected)   | Impact                                 |
| ------------------ | --------------------- | --------------------- | -------------------------------------- |
| **Wien gain**      | R91A = 5.1kΩ (A=2.96) | R91A = 4.7kΩ (A=3.13) | Loop gain 0.987→1.043 (NOW oscillates) |
| **MOV placement**  | Across relay contacts | Across load (Phase-N) | IEC 60335-1 compliance                 |
| **GPIO7 conflict** | Firmware control only | JP5 hardware jumper   | Prevents bus contention                |

### Component Changes from v2.24.1

| Component | Change                     | Reason                                       |
| --------- | -------------------------- | -------------------------------------------- |
| **R91A**  | 5.1kΩ → **4.7kΩ**          | Barkhausen criterion: A×β must be >1, not ~1 |
| **RV2**   | K2_NO↔K2_COM → **J3_NO↔N** | MOV short must blow fuse, not bypass relay   |
| **RV3**   | K3_NO↔K3_COM → **J4_NO↔N** | Same safety logic as RV2                     |
| **JP5**   | (new) 3-pad jumper         | Hardware GPIO7 source selection (RS485/TTL)  |

### Technical Analysis

#### Wien Bridge Calculation Error (v2.24.1)

**The Mistake in v2.24.1:**

```
R91A = 5.1kΩ
A_CL = 1 + (10k/5.1k) = 2.96
Loop gain = A_CL × β = 2.96 × (1/3) = 0.987 < 1 ✗

Barkhausen criterion NOT met → oscillation decays to zero!
```

**Corrected in v2.24.2:**

```
R91A = 4.7kΩ (standard E24 value)
A_CL = 1 + (10k/4.7k) = 3.13
Loop gain = 3.13 × (1/3) = 1.043 > 1 ✓

Provides 4.3% margin above unity for robust oscillation startup.
```

#### MOV Safety Violation (v2.24.1)

**The Flaw:**
v2.24.1 claimed "Water tank switch (GPIO2) would prevent pump operation if MOV shorts."

**The Reality:**

- GPIO2 is just a sensor input to the MCU (not a power interrupt)
- If RV2 shorts across relay contacts: Live → MOV short → Pump (relay bypassed)
- **Firmware has ZERO control** - it's a hardware short circuit
- Violates IEC 60335-1 §19.11.2 single-fault safety requirement

**The Fix:**

```
Old: RV2 from K2_NO to K2_COM (across relay contacts)
New: RV2 from J3_NO to Neutral (across pump load)

If MOV shorts now: Phase → Neutral short → F1 (10A fuse) blows → Safe
```

Arc suppression still effective (MOV absorbs inductive kickback energy).

#### GPIO7 Hardware Protection (v2.24.2)

**The Risk:** v2.24.1 relied on firmware tri-stating MAX3485 in TTL mode via GPIO20.

**Problem:** If firmware crashes or GPIO20 floats during reset, BOTH drivers active on GPIO7 = bus contention.

**Solution:** JP5 3-pad solder jumper physically selects source:

```
     U8_RO (MAX3485)          J17_DIV (External Meter)
          │                          │
          1 ◄──── 2 (center) ────► 3
                  │
               GPIO7

JP5 Setting:
• Pads 1-2: RS485 mode (U8 RO → GPIO7)
• Pads 2-3: TTL mode (J17_DIV → GPIO7)
```

Hardware isolation eliminates firmware dependency for safety.

---

## v2.24.1 (December 7, 2025) - 🔴 EMERGENCY CRITICAL FIXES (SUPERSEDED)

**DO NOT FABRICATE v2.24 - Three Critical Circuit Errors Identified**

External peer review identified showstopper issues that would cause complete board failure:

### Critical Components Added

| Ref   | Value    | Purpose                   | Failure Mode if Missing                |
| ----- | -------- | ------------------------- | -------------------------------------- |
| R_FB1 | 33kΩ 1%  | TPS563200 feedback upper  | Buck outputs 5V → destroys 3.3V domain |
| R_FB2 | 10kΩ 1%  | TPS563200 feedback lower  | Same - CRITICAL system failure         |
| R91A  | 5.1kΩ 1% | Wien bridge gain resistor | No oscillation → level probe dead      |
| R_ISO | 47Ω 1%   | VREF buffer isolation     | Op-amp oscillates → noisy readings     |

### Components Updated

| Component | v2.24     | v2.24.1    | Reason                   |
| --------- | --------- | ---------- | ------------------------ |
| C2        | 100µF 16V | 470µF 6.3V | Per HLK-15M05C spec      |
| R1A       | (missing) | 1.5kΩ 1%   | Brew NTC parallel (JP2)  |
| R2A       | (missing) | 680Ω 1%    | Steam NTC parallel (JP3) |

### Issue Details

**1. TPS563200 Missing Feedback Divider**

- D-CAP2 buck converter requires external divider on FB pin to set output voltage
- v2.24 showed FB→VOUT direct connection (incorrect for 3.3V output)
- Without R_FB1/R_FB2: Output voltage indeterminate, likely 4-5V
- **Impact:** First power-up would immediately destroy RP2350 MCU

**2. Wien Bridge Missing Gain Resistor**

- Oscillator requires A_CL ≈ 3 for sustained oscillation (Barkhausen criterion)
- R91 (feedback) present, but R91A (to GND) was missing
- Without R91A: Unity gain (A=1), no oscillation
- **Impact:** Level probe failure → boiler overfill → safety hazard

**3. ADC Reference Buffer Capacitive Load**

- OPA2342 cannot drive 22µF directly (causes instability)
- Missing isolation resistor between op-amp output and C7
- **Impact:** Ringing on ADC_VREF → unstable temperature control

### Review Process

- External technical review conducted December 7, 2025
- Reviewer identified issues through circuit analysis and datasheet verification
- Design team validated claims and implemented corrections same day
- See `docs/hardware/schematics/REVIEW_RESPONSE.md` for full analysis

**Files Modified:**

- `netlist.csv` - Added 4 critical components, corrected 3 values
- `Schematic_Reference.md` - Updated circuit diagrams
- `Specification.md` - Updated schematics, BOM, added warning header
- `CHANGELOG.md` - This entry

---

## v2.24 (December 2025) - ⚠️ DO NOT FABRICATE

**Independent Engineering Review - Critical Analog Fixes**

This revision addresses critical issues identified in an independent engineering design review that would have caused system failure in production.

### 🔴 Critical Engineering Change Orders (ECOs)

#### 1. VREF Buffer Op-Amp (SYSTEM-CRITICAL FIX)

| Component    | Old (v2.23)          | New (v2.24)                | Reason                                       |
| ------------ | -------------------- | -------------------------- | -------------------------------------------- |
| **U9**       | (none)               | OPA2342UA (dual op-amp)    | Buffers ADC_VREF for high-current loads      |
| **C80**      | (none)               | 100nF 25V ceramic          | U9 VCC decoupling                            |
| **R7**       | To ADC_VREF directly | To U9A input (high-Z)      | LM4040 now sees only ~60µA load              |
| **ADC_VREF** | Direct from LM4040   | From U9A output (buffered) | Can now drive 10mA+ without voltage collapse |

**The Problem (Why This Was Critical):**

The LM4040 shunt reference with R7=1kΩ provides only **300µA** of bias current:
$$I_{available} = \frac{3.3V - 3.0V}{1k\Omega} = 0.3mA$$

But the NTC sensor dividers demand **~4mA** at operating temperature:

- Brew NTC at 93°C: R_NTC ≈ 3.5kΩ → I = 3.0V / (3.3kΩ + 3.5kΩ) ≈ 441µA
- Steam NTC at 135°C: R_NTC ≈ 1kΩ → I = 3.0V / (1.2kΩ + 1kΩ) ≈ **1.36mA**
- Total: **~1.8mA minimum**, often higher

**Result without fix:** ADC_VREF collapses to ~0.5-1.0V, causing complete temperature sensing failure. The PID would either emergency shutdown (thinking boiler overheated) or overheat the boiler dangerously.

**The Solution:**

```
    +3.3V
       │
    ┌──┴──┐
    │ 1kΩ │  R7 (unchanged)
    └──┬──┘
       │
       ├───────────────────────────► LM4040_VREF (3.0V, unloaded)
       │                                    │
    ┌──┴──┐                          ┌──────┴──────┐
    │LM4040│                         │    U9A      │
    │ 3.0V │  U5                     │   OPA2342   │
    └──┬──┘                          │  (+) ───────┤
       │                             │      Buffer │
      GND                            │  (-) ◄──┬───┤
                                     │         │   │
                                     └────┬────┘   │
                                          │        │
                                          └────────┴──────► ADC_VREF (3.0V, buffered)
                                                                   │
                                                           ┌───────┴───────┐
                                                           │               │
                                                          R1             R2
                                                       (3.3kΩ)        (1.2kΩ)
                                                           │               │
                                                         ADC0           ADC1
                                                      (Brew NTC)    (Steam NTC)
```

The op-amp buffer:

- Presents ~pA input bias to LM4040 (maintains precision reference accuracy)
- Drives the ~4mA sensor load from its output stage (powered from 3.3V rail)
- Unity-gain follower: Vout = Vin (no gain error)

**U9B (second half of OPA2342) is available for future use** (e.g., pressure sensor buffer if needed)

#### 2. Pressure Sensor Ratiometric Compensation (ADC Channel)

| Component | Old (v2.23)       | New (v2.24)       | Reason                                         |
| --------- | ----------------- | ----------------- | ---------------------------------------------- |
| **R100**  | (none)            | 10kΩ 1% 0805      | 5V monitor upper divider                       |
| **R101**  | (none)            | 5.6kΩ 1% 0805     | 5V monitor lower divider (same ratio as R3/R4) |
| **C81**   | (none)            | 100nF 25V ceramic | 5V monitor filter                              |
| **ADC3**  | (unused internal) | 5V_MONITOR        | Firmware ratiometric correction                |

**The Problem:**

Pressure transducers are **ratiometric** - their output is proportional to their supply voltage:
$$V_{out} = P \times k \times V_{supply}$$

If the 5V rail (from HLK-15M05C) drifts by 5% under load, pressure readings drift by 5%. In a 10-bar system, that's **0.5 bar error** - significant for pressure profiling.

**The Solution:**

Route the 5V rail through an identical voltage divider to a spare ADC input. Firmware calculates:
$$P_{actual} = \frac{ADC_{pressure}}{ADC_{5V\_monitor}} \times k$$

This cancels out any 5V supply drift.

```
    +5V Rail
       │
    ┌──┴──┐
    │ 10kΩ │  R100
    │  1%  │
    └──┬──┘
       │
       ├──────────[100nF C81]──────► GND
       │
       └──────────────────────────► ADC3/GPIO29 (5V_MONITOR)
       │
    ┌──┴──┐
    │5.6kΩ│  R101
    │ 1%  │
    └──┬──┘
       │
      GND

    Divider ratio: 5.6k / (10k + 5.6k) = 0.359
    At 5.0V: ADC reads 1.795V (safe for 3.0V reference)
    At 5.5V: ADC reads 1.974V (still within range)
```

**Note:** GPIO29/ADC3 is internal to Pico 2 but accessible via software. Firmware must configure it for ADC use.

### 🟡 Reliability Enhancements

#### 3. Thermocouple ESD Protection

| Component | Old (v2.23) | New (v2.24)  | Reason                       |
| --------- | ----------- | ------------ | ---------------------------- |
| **D22**   | (none)      | TPD2E001DRLR | TC_POS/TC_NEG ESD protection |

**The Problem:**

Thermocouple inputs (TC_POS, TC_NEG on J26 pins 12-13) are the most ESD-vulnerable points in the system:

- Long wires act as antennas
- Connector is user-accessible during installation
- No protection between connector and MAX31855

A static discharge could destroy U4 (MAX31855) - an expensive failure requiring board rework.

**The Solution:**

Add TPD2E001DRLR dual-line ESD protection diode near J26:

- Bidirectional TVS clamp
- Ultra-low capacitance (<0.5pF) - won't affect µV-level TC signals
- ±15kV ESD protection (HBM)

```
    J26-12 (TC+) ───┬───────────────────────────► TC_POS (to MAX31855)
                    │
               ┌────┴────┐
               │ TPD2E001│
               │   D22   │  Bidirectional ESD clamp
               └────┬────┘
                    │
    J26-13 (TC-) ───┴───────────────────────────► TC_NEG (to MAX31855)
```

#### 4. Thermocouple Common-Mode Filter

| Component | Old (v2.23) | New (v2.24)     | Reason                |
| --------- | ----------- | --------------- | --------------------- |
| **C41**   | (none)      | 1nF 50V ceramic | TC+ common-mode shunt |
| **C42**   | (none)      | 1nF 50V ceramic | TC- common-mode shunt |

**The Problem:**

C40 (10nF across TC+/TC-) filters **differential** noise, but **common-mode** interference from chassis coupling passes through. In industrial environments with motor-driven pumps, common-mode noise can corrupt MAX31855 readings.

**The Solution:**

Add small capacitors from each TC line to GND:

- 1nF is small enough to not affect thermocouple response time
- Shunts common-mode RF/EMI to ground
- Standard practice in industrial thermocouple interfaces

```
                    C40 (10nF differential)
                   ┌────────┐
    TC_POS ────────┤        ├────────► MAX31855 T+
                   │        │
            ┌──────┘        └──────┐
            │                      │
       ┌────┴────┐            ┌────┴────┐
       │  C41    │            │  C42    │
       │  1nF    │            │  1nF    │
       │  CM     │            │  CM     │
       └────┬────┘            └────┬────┘
            │                      │
           ─┴─                    ─┴─
           GND                    GND

    TC_NEG ──────────────────────────────────► MAX31855 T-
```

#### 5. J17 Level Shifter 3.3V Bypass Jumper

| Component | Old (v2.23) | New (v2.24)                  | Reason                                 |
| --------- | ----------- | ---------------------------- | -------------------------------------- |
| **JP4**   | (none)      | Solder jumper (default OPEN) | Bypass 5V→3.3V divider for 3.3V meters |

**The Problem:**

The resistive divider (R45/R45A) converts 5V signals to 3.0V - perfect for 5V meters. But for **3.3V meters**:
$$V_{out} = 3.3V \times \frac{3.3k\Omega}{2.2k\Omega + 3.3k\Omega} = 3.3V \times 0.6 = 1.98V$$

This is **below** the RP2350's V_IH threshold (2.145V), causing communication failures with 3.3V logic meters.

**The Solution:**

Add JP4 solder jumper to bypass the voltage divider when using 3.3V meters:

```
    J17 Pin 4 (RX from meter)
         │
         ├────────────────────────────────────┐
         │                                    │
    ┌────┴────┐                          ┌────┴────┐
    │  2.2kΩ  │  R45                     │   JP4   │  Solder jumper
    └────┬────┘                          │ (Bypass)│  Default: OPEN
         │                               └────┬────┘
         ├───────────────────────────────────┘
         │
    ┌────┴────┐
    │  3.3kΩ  │  R45A
    └────┬────┘
         │
        GND
         │
    ┌────┴────┐
    │   33Ω   │  R45B (series protection)
    └────┬────┘
         │
         └────────────────────────────────────► GPIO7 (METER_RX)
```

| Meter Type | JP4 Setting | Result                        |
| ---------- | ----------- | ----------------------------- |
| 5V TTL     | OPEN        | 5V → 3.0V via divider (safe)  |
| 3.3V       | CLOSED      | 3.3V → 3.3V bypassed (direct) |

**⚠️ WARNING:** Closing JP4 with a 5V meter connected will damage GPIO7!

### 📋 Component Summary (v2.24 Changes)

| Category       | Added                          | Changed | Removed |
| -------------- | ------------------------------ | ------- | ------- |
| **ICs**        | U9 (OPA2342UA dual buffer)     | -       | -       |
| **Diodes**     | D22 (TPD2E001DRLR ESD)         | -       | -       |
| **Resistors**  | R100, R101 (5V monitor)        | -       | -       |
| **Capacitors** | C41, C42 (1nF TC CM), C80, C81 | -       | -       |
| **Jumpers**    | JP4 (J17 3.3V bypass)          | -       | -       |

### Design Verdict

**Status:** Approved for production

The v2.24 changes fix a **critical analog design flaw** (VREF buffer) that would have caused complete system failure in the field. The additional protections (TC ESD, CM filter, ratiometric compensation) improve reliability and measurement accuracy.

**Total component cost impact:** ~$2.50 (OPA2342: $1.50, TPD2E001: $0.50, passives: $0.50)

---

## v2.23 (December 2025)

**Design Review Action Items - Documentation & Manufacturing**

This revision addresses recommendations from an external design review, focusing on documentation clarity, manufacturing guidance, and grounding awareness.

### 📝 Documentation Additions

#### 1. Non-Isolated Power Meter Warning (Section 10.2)

Added detailed warning about grounding implications when using non-isolated TTL power meters (PZEM-004T, JSY series):

- Board is Class I (Earthed) due to PE-GND bond at MH1 for level probe sensing
- Non-isolated meters create PE-N bond through meter's internal connection
- Clarified this is a ground quality issue, NOT a safety hazard (board is already earthed)
- Recommended RS485 industrial meters (Eastron SDM) for cleaner grounding

#### 2. X2 Capacitor Bleed Resistor (Section 11.1)

Added optional 1MΩ bleed resistor footprint (DNP) across X2 capacitor (C1):

- Discharges capacitor when machine unplugged
- Required for IEC 60950 certification compliance
- τ = 0.1s, fully discharged in <1 second

#### 3. Conformal Coating Guidance (Section 16.4)

Expanded from single line to detailed table specifying:

- Areas to coat: HV section, level probe traces, LV analog section
- Areas to mask: All connectors, Pico socket, relay contacts, test points
- Recommended coating types: Acrylic (MG Chemicals 419D) or silicone

#### 4. Pico Module Retention Note (Section 16.4)

Added guidance to apply RTV silicone at module corners after testing to prevent vibration-induced creep from pump operation.

### 🧪 Test Procedure Additions (Test_Procedures.md)

#### AC Oscillator Level Probe Validation (Section 3.4)

Added Phase 2/3 test procedure for the production Wien-bridge oscillator circuit:

- Oscilloscope verification of 160Hz AC output
- Tap water vs distilled water testing
- Sensitivity adjustment guidance (R95 value)
- Pass criteria for production validation

---

## v2.22 (December 2025)

**Engineering Design Review Fixes - Thermal & GPIO Protection**

This revision addresses critical issues identified in an independent engineering design qualification and reliability analysis of the RP2350-based control system.

### 🔴 Critical Engineering Change Orders (ECOs)

#### 1. Power Regulator: LDO → Synchronous Buck Converter

| Component | Old (v2.21)     | New (v2.22)      | Reason                                                       |
| --------- | --------------- | ---------------- | ------------------------------------------------------------ |
| **U3**    | AP2112K-3.3 LDO | TPS563200DDCR    | LDO thermal risk in hot environment                          |
| **L1**    | (none)          | 2.2µH inductor   | Required for buck topology (2.2µH per TI datasheet for 3.3V) |
| **C3**    | 100µF Alum      | 22µF Ceramic X5R | Buck input cap                                               |
| **C4**    | 47µF Ceramic    | 22µF Ceramic X5R | Buck output cap                                              |
| **C4A**   | (none)          | 22µF Ceramic X5R | Parallel output cap for low ripple                           |

**Problem:** SOT-23-5 LDO dissipates P = (5V - 3.3V) × I = 1.7W × I. At 250mA load in 60°C ambient, junction temperature exceeds 125°C maximum rating, causing thermal shutdown.

**Solution:** Synchronous buck converter at >90% efficiency reduces heat by 10×. Same footprint complexity, vastly improved thermal reliability.

#### 1a. Pico Internal Regulator Disabled (3V3_EN = GND)

| Component | Old (v2.21)                       | New (v2.22)                   | Reason                             |
| --------- | --------------------------------- | ----------------------------- | ---------------------------------- |
| **U1**    | 3V3 = +3.3V (parallel regulators) | 3V3 = +3.3V, **3V3_EN = GND** | Disables Pico's internal regulator |

**Problem:** The Pico 2 has an internal RT6150B buck-boost regulator that outputs 3.3V on Pin 36. Connecting the external TPS563200 output to this pin creates a "hard parallel" condition - two closed-loop regulators fighting for control. This causes feedback contention, potential reverse current damage, and instability.

**Solution:** Connect Pico Pin 37 (3V3_EN) to GND. This disables the internal regulator, allowing the external TPS563200 to safely power the entire 3.3V domain via the Pico's 3V3 pin.

#### 1b. Fusing Hierarchy: Secondary Fuse for HLK Module

| Component | Old (v2.21) | New (v2.22)               | Reason                                        |
| --------- | ----------- | ------------------------- | --------------------------------------------- |
| **F2**    | (none)      | 500mA 250V slow-blow fuse | Protects HLK module independently from relays |

**Problem:** The main 10A fuse (F1) protects relay-switched loads (pump, solenoid). If the HLK-15M05C AC/DC module fails (e.g., shorted primary winding), the 10A fuse may not clear fast enough to prevent PCB damage.

**Solution:** Add F2 (500mA) in series with HLK module input, branching from L_FUSED after F1. This "fusing hierarchy" ensures HLK faults blow only F2, protecting traces and preventing main fuse replacement for logic supply faults.

#### 2. PZEM Interface: 5V→3.3V Level Shifting

| Component | Old (v2.21) | New (v2.22)            | Reason                         |
| --------- | ----------- | ---------------------- | ------------------------------ |
| **R45**   | 33Ω series  | 2.2kΩ (upper divider)  | RP2350 GPIO is NOT 5V tolerant |
| **R45A**  | (none)      | 3.3kΩ (lower divider)  | Creates voltage divider        |
| **R45B**  | (none)      | 33Ω (series after div) | ESD protection after divider   |

**Problem:** PZEM-004T outputs 5V TTL signals. RP2350 internal protection diodes conduct at ~3.6V, stressing silicon and causing long-term GPIO degradation or latch-up.

**Solution:** Resistive divider: V_out = 5V × 3.3k / (2.2k + 3.3k) = 3.0V (safe for 3.3V GPIO).

#### 3. Relay K1 Function Clarification

| Aspect          | Old (v2.21)        | New (v2.22)                    |
| --------------- | ------------------ | ------------------------------ |
| **Description** | "Water LED relay"  | "Mains indicator lamp relay"   |
| **Load Type**   | Ambiguous          | Mains indicator lamp (~100mA)  |
| **Relay**       | APAN3105 (3A slim) | APAN3105 (3A slim) - unchanged |

**Clarification:** K1 switches a mains-powered indicator lamp on the machine front panel (~100mA load). The 3A slim relay (APAN3105) is appropriate for this low-current application. The relay is required because the indicator is mains-level, but the current draw is minimal.

### 🟡 Reliability Enhancements

#### 4. Fast Flyback Diodes

| Component | Old (v2.21) | New (v2.22) | Reason                                   |
| --------- | ----------- | ----------- | ---------------------------------------- |
| **D1-D3** | 1N4007      | UF4007      | Fast recovery (75ns) for snappy contacts |

**Improvement:** Standard 1N4007 has slow reverse recovery, causing relay coil current to decay slowly ("lazy" contact opening). UF4007 fast recovery diode dissipates stored energy faster, resulting in snappier relay contact separation and reduced arcing on mains-side contacts.

#### 5. Precision ADC Voltage Reference

| Component | Old (v2.21)       | New (v2.22)              | Reason                         |
| --------- | ----------------- | ------------------------ | ------------------------------ |
| **U5**    | (none)            | LM4040DIM3-3.0           | Precision 3.0V shunt reference |
| **R7**    | (none)            | 1kΩ 1% (bias)            | Sets reference operating point |
| **C7**    | 22µF on 3.3V_A    | 22µF on ADC_VREF         | Bulk cap for reference         |
| **C7A**   | (none)            | 100nF (HF decoupling)    | Reference noise filtering      |
| **FB1**   | 3.3V → 3.3V_A     | 3.3V → ADC_VREF (via R7) | Isolates reference from noise  |
| **R1,R2** | Pull-up to 3.3V_A | Pull-up to ADC_VREF      | Uses precision reference       |
| **TP2**   | (none)            | ADC_VREF test point      | Calibration verification       |

**Improvement:** Using the 3.3V rail as ADC reference couples regulator thermal drift into temperature measurements. The LM4040 provides a stable 3.0V ±0.5% reference with 100ppm/°C tempco, significantly improving temperature measurement accuracy and stability.

#### 6. Pressure Sensor Divider: Prevent ADC Saturation

| Component | Old (v2.21) | New (v2.22) | Reason                                      |
| --------- | ----------- | ----------- | ------------------------------------------- |
| **R4**    | 4.7kΩ       | 5.6kΩ       | Prevents ADC saturation with 3.0V reference |

**Problem:** With the 3.0V ADC reference (U5), the old 4.7kΩ divider produced V_max = 4.5V × 0.68 = 3.06V, exceeding the 3.0V reference and causing ADC saturation above ~15.6 bar.

**Solution:** Change R4 to 5.6kΩ. New ratio = 0.641, V_max = 2.88V (120mV headroom below reference). Full 0-16 bar range now linear with no clipping.

### 📋 Documentation Additions

#### RP2350 ADC E9 Erratum Documentation

Added comprehensive documentation of the RP2350 A2 stepping ADC leakage current issue (Erratum E9):

- **Issue:** Parasitic leakage on ADC pins (GPIO 26-29) when voltage >2.0V or high-impedance
- **Impact:** Can cause temperature errors >1°C on NTC channels at high temperatures
- **Mitigations documented:** Silicon stepping verification, firmware calibration, future external ADC option
- **Recommendation:** Firmware calibration against reference thermometer required for ±0.5°C accuracy

#### Thermocouple Ground Loop Warning

Added critical documentation about the MAX31855 ground loop trap with grounded junction thermocouples:

- **Problem:** Espresso machine thermocouples are often "grounded junction" - the TC junction is welded to the stainless steel probe sheath. When the sheath is screwed into the boiler (bonded to PE), a ground loop forms through MH1 → PCB GND → MAX31855 GND → T- → boiler.
- **Consequence:** MAX31855 internal T- bias is overridden, causing "Short to GND" fault (D2 bit) or massive common-mode noise on the µV-level thermocouple signal, rendering PID control unstable.
- **🔴 REQUIREMENT:** **UNGROUNDED (INSULATED) thermocouples ONLY**. The junction must be electrically isolated from the sheath by MgO insulation.
- **Diagnostic:** If MAX31855 reports "Short to GND" fault, the thermocouple is likely grounded junction - replace with ungrounded type.
- **Alternative (not recommended):** Galvanic isolation (ISO7741 + isolated DC/DC) adds ~$15 and PCB complexity.

#### 7. Pump Relay: High-Capacity Variant Required

| Component | Old (v2.21)  | New (v2.22)        | Reason                             |
| --------- | ------------ | ------------------ | ---------------------------------- |
| **K2**    | G5LE-1A4 DC5 | G5LE-1A4-**E** DC5 | Standard variant is only 10A rated |

**Problem:** The netlist specified K2 as "G5LE-1A4 DC5" with description "16A pump relay". However, the standard Omron G5LE-1A4 is rated for only **10A** (resistive) and less for inductive loads. The "-E" suffix denotes the high-capacity 16A variant.

**Risk:** Using an undersized relay for the pump's inductive load (with inrush current) can cause contact welding, leading to a "runaway pump" failure mode where the pump runs continuously.

**Solution:** Specify G5LE-1A4**-E** DC5 to ensure 16A contact rating matches the specification.

#### 8. RS485 TVS Surge Protection Added

| Component | Old (v2.21) | New (v2.22) | Reason                          |
| --------- | ----------- | ----------- | ------------------------------- |
| **D21**   | (none)      | SM712       | RS485 A/B line surge protection |

**Problem:** The RS485 interface (MAX3485, J17) extends off-board to external power meters. Industrial environments (kitchens) have severe EMI from motors, heaters, and potential lightning-induced surges on data lines.

**Solution:** Add SM712 asymmetric TVS diode on RS485 A/B lines:

- Clamps to -7V / +12V (matches RS485 common-mode voltage range)
- Protects against motor switching noise and nearby lightning
- Place close to J17 connector

**Existing good practice noted:** R19 (10kΩ pull-down on GPIO20/DE) ensures transceiver defaults to receive mode during boot, preventing bus contention.

#### 9. Level Probe Oscillator Frequency: Electrolysis Prevention

| Component   | Old (v2.21) | New (v2.22) | Reason                                   |
| ----------- | ----------- | ----------- | ---------------------------------------- |
| **C61-C62** | 100nF       | 10nF        | Increases frequency from 160Hz to 1.6kHz |

**Problem:** The Wien bridge oscillator at 160Hz allows significant electrochemical reactions (electrolysis) during each AC half-cycle. Low frequencies give ions time to migrate to the electrode surface and complete Faradaic reactions, corroding the level probe. At 160Hz, probe life may be only months.

**Solution:** Change C61/C62 from 100nF to 10nF:

- f = 1/(2π × 10kΩ × 10nF) ≈ **1.6 kHz**
- Industry standard for conductivity sensors: 1-10 kHz
- At 1.6kHz, probe life extends to **5-10+ years**

**Electrochemistry background:**

- Faradaic reactions require activation energy and time
- Higher frequency = shorter half-periods = less time for electrolysis
- Double-layer capacitance at electrode/water interface is better bypassed at kHz frequencies

### Component Summary (v2.22 Changes)

| Category        | Added               | Changed                                       | Removed |
| --------------- | ------------------- | --------------------------------------------- | ------- |
| **ICs**         | U5 (LM4040DIM3-3.0) | U3 (AP2112K → TPS563200)                      | -       |
| **Inductors**   | L1 (2.2µH)          | -                                             | -       |
| **Resistors**   | R7, R45A, R45B      | R45 (33Ω → 2.2kΩ), R4 (4.7k → 5.6k)           | -       |
| **Capacitors**  | C4A, C7A            | C3, C4, C7, C61-C62 (100nF → 10nF for 1.6kHz) | -       |
| **Diodes**      | D21 (SM712 RS485)   | D1-D3 (1N4007 → UF4007)                       | -       |
| **Relays**      | -                   | K1 clarified, K2 → G5LE-1A4-E (16A)           | -       |
| **Test Points** | TP2 (ADC_VREF)      | -                                             | -       |
| **Fuses**       | F2 (500mA HLK)      | -                                             | -       |
| **Pico Config** | -                   | 3V3_EN = GND (internal reg disabled)          | -       |

### Design Verdict

**Status:** Approved for production (pending verification of changes)

The engineering review identified this design as fundamentally sound with excellent practices in several areas (Wien bridge level sensor, snubber networks). With the implementation of these thermal and logic-level protections, the design is qualified as a high-reliability platform suitable for commercial/prosumer applications.

---

## v2.21 (December 2025)

**Universal External Power Metering & Multi-Machine Support**

### MCU Upgrade: Raspberry Pi Pico 2 (RP2350)

| Change          | Description                                          |
| --------------- | ---------------------------------------------------- |
| **MCU**         | Upgraded from RP2040 to **RP2350** (Pico 2)          |
| **Part Number** | SC0942 (Pico 2) or SC1632 (Pico 2 W with WiFi)       |
| **Performance** | Dual Cortex-M33 @ 150MHz (vs M0+ @ 133MHz)           |
| **Memory**      | 520KB SRAM, 4MB Flash (vs 264KB/2MB)                 |
| **PIO**         | 12 state machines in 3 blocks (vs 8 in 2 blocks)     |
| **ADC**         | 4 channels (vs 3) - extra channel available          |
| **E9 Errata**   | Pull-down resistors on inputs mitigate GPIO latching |

**Note:** Same form factor and pinout - drop-in upgrade from original Pico.

### Power Metering (External Modules)

| Change       | Description                                                 |
| ------------ | ----------------------------------------------------------- |
| **REMOVED**  | Embedded PZEM-004T daughterboard                            |
| **J17 (LV)** | JST-XH 6-pin for UART/RS485 communication                   |
| **J24 (HV)** | Screw terminal 3-pos (L fused, N, PE) for easy meter wiring |
| **U8**       | MAX3485 RS485 transceiver with JP1 termination jumper       |
| **J26**      | Reduced 24→22 pos (CT clamp pins removed)                   |

**Supported meters:** PZEM-004T, JSY-MK-163T/194T, Eastron SDM, and other Modbus meters

### Multi-Machine NTC Support (Jumper Selectable)

| Jumper Config     | Machine Type   | NTC  | Effective R1 | Effective R2 |
| ----------------- | -------------- | ---- | ------------ | ------------ |
| JP2/JP3 **OPEN**  | ECM, Profitec  | 50kΩ | 3.3kΩ        | 1.2kΩ        |
| JP2/JP3 **CLOSE** | Rocket, Gaggia | 10kΩ | ~1kΩ         | ~430Ω        |

**New components:** R1A (1.5kΩ), R2A (680Ω) parallel resistors via solder jumpers

### Expansion & Documentation

- **GPIO22 Expansion:** Available on J15 Pin 8 (SPARE) for future use (flow meter, etc.)
- **J25 REMOVED:** GPIO23 is internal to Pico 2 module (SMPS Power Save) - not on header
- **Section 14.3a:** Solder Jumpers (JP1, JP2, JP3)
- **Section 14.9:** External Sensors BOM with ordering specs
- **Sensor restrictions documented:** Type-K thermocouple only, 0.5-4.5V pressure only
- **C2 upgraded:** 470µF 6.3V Polymer capacitor (low ESR, long life in hot environment)
  - C3 removed - single bulk cap sufficient with HLK internal filtering
- **K1/K3 downsized:** Panasonic APAN3105 (3A, slim 5mm) replaces HF46F (10A)
  - K2 (pump) stays Omron G5LE-1A4 DC5 (16A) for motor inrush
  - Saves ~16mm PCB width
- **Snubbers → MOVs:** Replaced bulky X2 caps (C50-C51) + resistors (R80-R81) with MOVs (RV2-RV3)
  - S10K275 varistors (10mm disc) - ~70% smaller than RC snubbers
  - Critical for slim relay contact protection

### Fixes & Clarifications (v2.21.1)

| Item              | Issue                          | Fix                                                                   |
| ----------------- | ------------------------------ | --------------------------------------------------------------------- |
| **Power Supply**  | HLK-5M05 (1A) insufficient     | Changed to **HLK-15M05C** (3A/15W, 48×28×18mm)                        |
| **GPIO23**        | Not available on Pico 2 header | J25 expansion header **removed** (use GPIO22 via J15 Pin 8)           |
| **3.3V Rail**     | Unclear power architecture     | Clarified: External LDO (U3) for sensors only, Pico has internal 3.3V |
| **Diagram**       | "RELAYS (4x)" incorrect        | Fixed to **"RELAYS (3x)"** (K1, K2, K3)                               |
| **SSR Pins**      | J26 Pin 19-22 referenced       | Fixed: **SSR1=Pin 17-18, SSR2=Pin 19-20**                             |
| **Power Budget**  | Relay current incorrect        | Updated: 80mA typical, 150mA peak (K2:70mA, K1/K3:40mA)               |
| **Total Current** | Old values                     | Updated: **~355mA typical, ~910mA peak** (3A gives 3× headroom)       |

### New Features (v2.21.1)

| Item             | Description                                                                    |
| ---------------- | ------------------------------------------------------------------------------ |
| **ADC Clamping** | Added **D16 (BAT54S)** Schottky diode to protect pressure ADC from overvoltage |

**ADC Protection:** Prevents RP2350 damage if pressure transducer voltage divider fails.

### Pico 2 GPIO Clarifications

**Internal GPIOs (NOT on Pico 2 40-pin header):**

| GPIO   | Internal Function | Notes                    |
| ------ | ----------------- | ------------------------ |
| GPIO23 | SMPS Power Save   | Cannot use for J25       |
| GPIO24 | VBUS Detect       | USB connection sense     |
| GPIO25 | Onboard LED       | Green LED on module      |
| GPIO29 | VSYS/3 ADC        | Internal voltage monitor |

**Available on header:** GPIO 0-22, 26-28 (26 pins total)

---

## v2.20 (December 2025)

**Unified Low-Voltage Terminal Block**

- **J26 unified screw terminal (22-pos):** ALL low-voltage connections consolidated
- **Includes:** Switches (S1-S4), NTCs (T1-T2), Thermocouple, Pressure, SSRs
- **Eliminates:** J10, J11, J12, J13, J18, J19 (all merged into J26)
- **6.3mm spades retained ONLY for 220V AC:** Mains input and relay outputs

---

## v2.19 (December 2025)

**Simplified Relay Configuration**

- Removed spare relay K4 and associated components
- GPIO20 made available as test point TP1

---

## v2.17 (November 2025)

**Brew-by-Weight Support**

- J15 ESP32 connector expanded from 6-pin to 8-pin JST-XH
- GPIO21: WEIGHT_STOP signal (ESP32 → Pico)
- GPIO22: SPARE for future expansion
- Enables Bluetooth scale integration via ESP32

---

## v2.16 (November 2025)

**Production-Ready Specification**

- Power supply: HLK-15M05C (5V 3A/15W, 48×28×18mm)
- Level probe: OPA342 + TLV3201 AC sensing circuit
- Snubbers: MANDATORY for K2 (Pump) and K3 (Solenoid)
- NTC pull-ups: Optimized for 50kΩ NTCs (R1=3.3kΩ, R2=1.2kΩ)
- Mounting: MH1=PE star point (PTH), MH2-4=NPTH (isolated)
- SSR control: 5V trigger signals only, mains via existing machine wiring
