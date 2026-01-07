# Engineering Task List - PCB Design Finalization

**Target Machine:** ECM Synchronika  
**Revision:** v2.31+  
**Status:** Pre-Production Checklist

This document tracks the implementation and optimization tasks necessary to finalize the BrewOS hardware design for production.

---

## Required Changes List (Critical Actions)

*These items address safety, hardware longevity, and immediate failure risks. They must be implemented in the schematic before any PCB manufacturing.*

### ✅ 1. AC-Drive for Water Level Probe

**Status:** ✅ **IMPLEMENTED** (v2.22)

- **Implementation:** Wien bridge oscillator (U6) generates ~1.6kHz AC signal
- **AC Coupling:** C64 (1µF) provides DC blocking to prevent electrolysis
- **Detection:** TLV3201 comparator detects AC signal attenuation when water touches probe
- **Verification:** See `spec/05-Analog-Inputs.md` - Level Probe AC Coupling section

**No action required** - Already implemented and documented.

---

### ✅ 2. Protection for RP2354 5V Tolerance

**Status:** ✅ **IMPLEMENTED** (v2.28)

- **Implementation:** Series current-limiting resistors (R40-R43 = 1kΩ) on all GPIOs connected to 5V sensors
- **Schottky Clamping:** BAT54S (D16) on pressure sensor input
- **Pull-downs:** 4.7kΩ resistors (R11-R15, R73-R75) ensure defined GPIO states
- **Documentation:** See `spec/02-GPIO-Allocation.md` - RP2354 5V Tolerance section

**No action required** - Already implemented and documented.

---

### ⚠️ 3. Isolate ESP32 RF Subsystem

**Status:** ⚠️ **DOCUMENTATION ADDED** - Requires ESP32 module selection verification

- **Requirement:** ESP32 module **MUST** use external antenna via u.FL/IPEX connector
- **Rationale:** ECM Synchronika's stainless steel chassis acts as Faraday cage, blocking Wi-Fi/Bluetooth if onboard PCB trace antenna is used
- **Documentation:** Added to `spec/06-Connectors.md` - J15 ESP32 Display Module section

**Action Required:**
- [ ] Verify ESP32 module selection includes u.FL/IPEX connector (NOT PCB trace antenna)
- [ ] Update ESP32 module BOM with antenna connector requirement
- [ ] Add antenna cable specification (10-20cm, 2.4GHz/5GHz compatible)

---

### ✅ 4. Install Snubbers on Inductive Loads

**Status:** ✅ **IMPLEMENTED** (v2.22)

- **Implementation:** MOVs (RV2, RV3) across pump and solenoid loads
- **Component:** S10K275 varistors (275V AC, 10mm disc)
- **Placement:** Across load (Phase-N), NOT across relay contacts (safety requirement)
- **Documentation:** See `spec/04-Outputs.md` - MOV Surge Protection section

**No action required** - Already implemented and documented.

---

### ✅ 5. Enforce Safety Interlock Wiring

**Status:** ✅ **VERIFIED** - Documentation added

- **Verification:** SSR control outputs are LOW-VOLTAGE DC signals only - they do NOT bypass high-limit thermostats
- **Physical Interlock:** Machine's factory-installed high-limit thermostats remain in series with heater elements
- **Documentation:** Added to `spec/09-Safety.md` - Safety Interlock Wiring Verification section

**No action required** - Design verified safe.

---

## Engineering Task List (Implementation Steps)

### Phase 1: Component Selection & Schematic Design

#### ✅ High-Temp Capacitors

**Status:** ✅ **IMPLEMENTED** (v2.30)

- **C2 (5V bulk):** 470µF 6.3V, **105°C Aluminum Polymer** (Panasonic OS-CON or equivalent)
- **Rationale:** Arrhenius Law - capacitor lifespan halves for every 10°C rise. At 65°C ambient, 85°C-rated caps have <1 year life.
- **Documentation:** See `spec/03-Power-Supply.md` - Thermal Derating section

**No action required** - Already specified.

---

#### ✅ NTC Signal Conditioning

**Status:** ✅ **IMPLEMENTED** (v2.24)

- **Implementation:** Precision ADC reference (LM4040 + OPA2342 buffer) provides stable 3.0V reference
- **Pull-up Resistors:** Optimized for 50kΩ NTCs (R1=3.3kΩ for brew, R2=1.2kΩ for steam)
- **Multi-Machine Support:** JP1/JP2 jumpers allow 10kΩ NTC support (Rocket, Gaggia)
- **Documentation:** See `spec/05-Analog-Inputs.md` - NTC Thermistor Interface section

**No action required** - Already implemented.

---

#### ⚠️ NTC Beta Values

**Status:** ⚠️ **DOCUMENTATION NEEDED**

- **Requirement:** Confirm Beta (β) value of ECM P6037 stock probe (typically ~3435K or 3950K)
- **Purpose:** Ensure bias resistors are sized correctly for RP2354 ADC range
- **Current Design:** Assumes standard 3950K Beta value

**Action Required:**
- [ ] Measure ECM P6037 NTC Beta value (or obtain from ECM datasheet)
- [ ] Verify ADC range coverage with measured Beta value
- [ ] Update firmware calibration tables if Beta differs from assumed value

---

#### ✅ Power Supply Buck Converter

**Status:** ✅ **IMPLEMENTED** (v2.22)

- **Implementation:** TPS563200 synchronous buck converter (5V → 3.3V, >90% efficient)
- **Current Capacity:** 3A output (adequate for ~1.1A peak load with 3× headroom)
- **Thermal:** Minimal heat dissipation (~25mW vs 425mW for LDO)
- **Documentation:** See `spec/03-Power-Supply.md` - 3.3V Synchronous Buck Converter section

**No action required** - Already implemented.

---

### Phase 2: PCB Layout & Signal Integrity

#### ✅ Define Stack-up

**Status:** ✅ **DOCUMENTED**

- **Recommended:** 4-layer stack-up (Signal / Ground / Power / Signal)
- **Minimum Viable:** 2-layer acceptable but with performance trade-offs
- **Documentation:** See `spec/08-PCB-Layout.md` - Layer Stackup section

**No action required** - Already documented.

---

#### ✅ Implement Star Grounding

**Status:** ✅ **IMPLEMENTED** (v2.29)

- **Implementation:** SRif (Chassis Reference) architecture
- **Analog Ground (AGND):** Separate from Digital Ground (DGND)
- **Connection Point:** Single point near RP2354 ADC_GND
- **Documentation:** See `spec/08-PCB-Layout.md` - Grounding Strategy section

**No action required** - Already implemented.

---

#### ⚠️ SPI Bus Termination

**Status:** ⚠️ **NOT APPLICABLE** - No SPI bus between RP2354 and ESP32

- **Current Design:** RP2354 ↔ ESP32 communication via UART (GPIO0/1)
- **UART Protection:** Already implemented (33Ω series resistors + TVS diodes)
- **Note:** If SPI is added in future, add 22-33Ω series termination resistors

**No action required** - Design uses UART, not SPI.

---

#### ✅ Thermal Relief

**Status:** ✅ **DOCUMENTED**

- **Implementation:** Thermal vias under voltage regulators and ESP32 module
- **Purpose:** Dissipate heat into inner ground planes
- **Documentation:** See `spec/08-PCB-Layout.md` - Critical Layout Notes section

**No action required** - Already documented.

---

#### ⚠️ ADC Precision Reference

**Status:** ⚠️ **IMPLEMENTED** - Additional documentation added

- **Implementation:** LM4040 (3.0V) + OPA2342 buffer (U9A)
- **Rationale:** Wi-Fi transmission bursts cause ripple on 3.3V digital rail, making it unsuitable as ADC reference
- **Documentation:** See `spec/03-Power-Supply.md` - Precision ADC Voltage Reference section

**No action required** - Already implemented.

---

#### ⚠️ Mains Creepage Slots

**Status:** ⚠️ **DOCUMENTATION ADDED** - Requires PCB layout implementation

- **Requirement:** Milled slots (cutouts) in PCB between HV relay contacts and LV logic sections
- **Standard:** IPC-2221B compliance (>3mm clearance or physical slots)
- **Implementation:** 2mm wide milled slots, full PCB thickness
- **Documentation:** Added to `spec/08-PCB-Layout.md` - Creepage and Clearance section

**Action Required:**
- [ ] Add milled slot layer to PCB design software
- [ ] Verify slot placement between all HV components and LV section
- [ ] Confirm with PCB manufacturer that slots will be milled (not just routed)
- [ ] Visual inspection of Gerber files to verify slot presence

---

#### ⚠️ Mains Hum Filtering

**Status:** ⚠️ **DOCUMENTATION ADDED** - Requires schematic update

- **Requirement:** Low-pass RC filter (fc ≈ 1.6kHz) at ADC input for NTC sensors
- **Components:** R_HUM (10kΩ) + C_HUM (10nF) in series before ADC pin
- **Rationale:** Sensor wires run parallel to high-voltage heater wires, picking up 50/60Hz noise
- **Documentation:** Added to `spec/05-Analog-Inputs.md` - NTC Thermistor Interface section

**Action Required:**
- [ ] Add R_HUM (10kΩ) and C_HUM (10nF) to schematic for both ADC0 and ADC1
- [ ] Place components immediately before ADC input pin (after existing 100nF filter)
- [ ] Update netlist.csv with new component references
- [ ] Verify filter cutoff frequency: fc = 1/(2π × 10kΩ × 10nF) ≈ 1.6kHz

---

#### ⚠️ Pressure Sensor ADC Protection

**Status:** ⚠️ **PARTIALLY IMPLEMENTED** - Additional Zener clamp required

- **Current:** BAT54S (D16) Schottky clamp already implemented
- **Additional:** BZT52C3V3 (3.3V Zener) must be added in parallel
- **Rationale:** Pressure sensors are ratiometric 5V devices. If divider fails or ground lifts, 5V can dump into ADC pin
- **Documentation:** Added to `spec/05-Analog-Inputs.md` - Pressure Transducer Interface section

**Action Required:**
- [ ] Add BZT52C3V3 (D_PRESSURE) to schematic, parallel to BAT54S at GPIO28
- [ ] Update netlist.csv with new component reference
- [ ] Verify dual-clamp protection: Schottky (fast) + Zener (hard clamp)

---

### Phase 3: Firmware/Hardware Integration

#### ✅ Map GPIO Pins

**Status:** ✅ **COMPLETE**

- **Documentation:** Complete GPIO allocation in `spec/02-GPIO-Allocation.md`
- **5V-Tolerant Pins:** Documented and protected
- **ADC Pins:** Protected with RC filters and clamping diodes

**No action required** - Already documented.

---

#### ⚠️ Configure Watchdog

**Status:** ⚠️ **SOFTWARE IMPLEMENTATION** - Hardware design complete

- **Current:** RP2354 internal watchdog (8-second timeout)
- **Optional:** External hardware watchdog (TPL5010) documented but not implemented
- **Documentation:** See `spec/09-Safety.md` - Hardware Interlock Considerations section

**Action Required:**
- [ ] Verify firmware implements RP2354 watchdog kick routine
- [ ] Test watchdog recovery behavior during firmware hang
- [ ] Consider external watchdog (TPL5010) for commercial certification

---

#### ⚠️ Calibration Routine

**Status:** ⚠️ **SOFTWARE IMPLEMENTATION** - Hardware design complete

- **Requirement:** Calibration routine for NTC sensors to account for voltage offset from Op-Amp buffer
- **Current Design:** OPA2342 buffer introduces minimal offset (<1mV typical)
- **Documentation:** See `spec/05-Analog-Inputs.md` - ADC Reference Design section

**Action Required:**
- [ ] Implement firmware calibration routine using reference thermometer
- [ ] Account for RP2354 ADC E9 errata (leakage current) in calibration
- [ ] Document calibration procedure for end users

---

## Summary

### ✅ Completed Items (No Action Required)

1. AC-Drive for Water Level Probe
2. Protection for RP2354 5V Tolerance
3. Snubbers on Inductive Loads (MOVs)
4. Safety Interlock Wiring Verification
5. High-Temp Capacitors
6. NTC Signal Conditioning
7. Power Supply Buck Converter
8. Define Stack-up
9. Implement Star Grounding
10. Thermal Relief
11. ADC Precision Reference
12. Map GPIO Pins

### ⚠️ Action Required Items

1. **ESP32 RF Subsystem:** Verify module selection includes u.FL/IPEX connector
2. **NTC Beta Values:** Measure ECM P6037 Beta value and verify ADC range
3. **Mains Creepage Slots:** Add milled slots to PCB layout
4. **Mains Hum Filtering:** Add R_HUM + C_HUM to schematic
5. **Pressure Sensor ADC Protection:** Add BZT52C3V3 Zener clamp
6. **Watchdog Configuration:** Verify firmware implementation
7. **Calibration Routine:** Implement firmware calibration

---

## Next Steps

1. **Schematic Updates:**
   - Add mains hum filtering (R_HUM, C_HUM) to ADC inputs
   - Add pressure sensor Zener clamp (BZT52C3V3)

2. **PCB Layout Updates:**
   - Add milled slots between HV and LV sections
   - Verify creepage/clearance compliance

3. **Component Verification:**
   - Verify ESP32 module includes u.FL/IPEX connector
   - Measure ECM P6037 NTC Beta value

4. **Firmware Integration:**
   - Implement watchdog kick routine
   - Implement calibration routine

5. **Documentation:**
   - Update BOM with new components
   - Update schematic images
   - Update test procedures

---

**Last Updated:** January 2026  
**Next Review:** Before PCB fabrication order

