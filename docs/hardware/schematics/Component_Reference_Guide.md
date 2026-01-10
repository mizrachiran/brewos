# BrewOS Control Board - Component Reference Guide

**Document Purpose:** Standardized component numbering scheme and cross-reference  
**Revision:** 2.31  
**Date:** January 2026

---

## Component Numbering Scheme

### Integrated Circuits (U1-U9)

| Ref | Part Number    | Function                                       | Package  |
| --- | -------------- | ---------------------------------------------- | -------- |
| U1  | RP2354A        | RP2354 Microcontroller (Discrete chip, QFN-60) | QFN-60   |
| U2  | HLK-15M05C     | AC/DC isolated power supply (5V)               | Module   |
| U3  | TPS563200DDCR  | Synchronous buck converter (3.3V)              | SOT-23-6 |
| U4  | TLV74311PDBVR  | 1.1V LDO (DVDD core voltage, external)         | SOT-23-5 |
| U5  | LM4040DIM3-3.0 | Precision 3.0V voltage reference               | SOT-23-3 |
| U6  | OPA342UA       | Level probe oscillator (op-amp)                | SOIC-8   |
| U7  | TLV3201AIDBVR  | Level probe comparator                         | SOT-23-5 |
| U8  | MAX3485ESA+    | RS485 transceiver                              | SOIC-8   |
| U9  | OPA2342UA      | ADC reference buffer (dual op-amp)             | SOIC-8   |

---

### Relays (K1-K3)

| Ref | Part Number    | Function             | Rating        | Size     |
| --- | -------------- | -------------------- | ------------- | -------- |
| K1  | APAN3105       | Mains indicator lamp | 3A @ 250V AC  | Slim 5mm |
| K2  | G5LE-1A4-E DC5 | Pump motor           | 16A @ 250V AC | Standard |
| K3  | APAN3105       | Solenoid valve       | 3A @ 250V AC  | Slim 5mm |

---

### Transistors (Q1-Q6)

| Ref | Part Number | Function        | Type | Package |
| --- | ----------- | --------------- | ---- | ------- |
| Q1  | MMBT2222A   | K1 relay driver | NPN  | SOT-23  |
| Q2  | MMBT2222A   | K2 relay driver | NPN  | SOT-23  |
| Q3  | MMBT2222A   | K3 relay driver | NPN  | SOT-23  |
| Q4  | (reserved)  | -               | -    | -       |
| Q5  | MMBT2222A   | SSR1 driver     | NPN  | SOT-23  |
| Q6  | MMBT2222A   | SSR2 driver     | NPN  | SOT-23  |

---

### Diodes & Protection

| Ref        | Part Number | Function                                                         | Package |
| ---------- | ----------- | ---------------------------------------------------------------- | ------- |
| D1         | UF4007      | K1 flyback diode                                                 | DO-41   |
| D2         | UF4007      | K2 flyback diode                                                 | DO-41   |
| D3         | UF4007      | K3 flyback diode                                                 | DO-41   |
| D10        | PESD5V0S1BL | Water switch ESD protection                                      | SOD-323 |
| D11        | PESD5V0S1BL | Tank level ESD protection                                        | SOD-323 |
| D12        | PESD5V0S1BL | Steam level ESD protection                                       | SOD-323 |
| D13        | PESD5V0S1BL | Brew switch ESD protection                                       | SOD-323 |
| D14        | PESD5V0S1BL | Brew NTC ESD protection                                          | SOD-323 |
| D15        | PESD5V0S1BL | Steam NTC ESD protection                                         | SOD-323 |
| D16        | BAT54S      | Pressure ADC overvoltage clamp (fast Schottky)                   | SOT-23  |
| D_PRESSURE | PESD3V3S1BL | Pressure ADC hard clamp (3.3V TVS, low-leakage, parallel to D16) | SOD-323 |
| D20        | SMBJ5.0A    | 5V rail TVS protection                                           | SMB     |
| D21        | SM712       | RS485 A/B line surge protection                                  | SOT-23  |

---

### LEDs

| Ref  | Color  | Function              | Package |
| ---- | ------ | --------------------- | ------- |
| LED1 | Green  | K1 relay indicator    | 0805    |
| LED2 | Green  | K2 relay indicator    | 0805    |
| LED3 | Green  | K3 relay indicator    | 0805    |
| LED5 | Orange | SSR1 heater indicator | 0805    |
| LED6 | Orange | SSR2 heater indicator | 0805    |
| LED7 | Green  | System status         | 0805    |

---

### Resistors - Organized by Function

#### Sensor Input Networks (R1-R10)

| Ref         | Value | Tolerance | Function                                                            |
| ----------- | ----- | --------- | ------------------------------------------------------------------- |
| R1          | 3.3kΩ | 1%        | Brew NTC pull-up (50kΩ sensors)                                     |
| R1A         | 1.5kΩ | 1%        | Brew NTC parallel (10kΩ via JP1)                                    |
| R2          | 1.2kΩ | 1%        | Steam NTC pull-up (50kΩ sensors)                                    |
| R2A         | 680Ω  | 1%        | Steam NTC parallel (10kΩ via JP2)                                   |
| R3          | 10kΩ  | 1%        | Pressure transducer divider (lower)                                 |
| R4          | 5.6kΩ | 1%        | Pressure transducer divider (upper)                                 |
| R5          | 1kΩ   | 1%        | Brew NTC ADC series protection                                      |
| R6          | 1kΩ   | 1%        | Steam NTC ADC series protection                                     |
| R_HUM_BREW  | 10kΩ  | 5%        | Mains hum filter resistor (Brew NTC, fc≈1.6kHz, 50/60Hz rejection)  |
| R_HUM_STEAM | 10kΩ  | 5%        | Mains hum filter resistor (Steam NTC, fc≈1.6kHz, 50/60Hz rejection) |
| R7          | 1kΩ   | 1%        | LM4040 voltage reference bias                                       |
| R8          | 47Ω   | 1%        | ADC VREF buffer isolation                                           |
| R9          | 33kΩ  | 1%        | TPS563200 feedback divider (upper)                                  |
| R10         | 10kΩ  | 1%        | TPS563200 feedback divider (lower)                                  |

#### Digital I/O Pull-ups/Pull-downs (R11-R19)

| Ref | Value | Tolerance | Function              |
| --- | ----- | --------- | --------------------- |
| R11 | 4.7kΩ | 5%        | K1 driver pull-down   |
| R12 | 4.7kΩ | 5%        | K2 driver pull-down   |
| R13 | 4.7kΩ | 5%        | K3 driver pull-down   |
| R14 | 4.7kΩ | 5%        | SSR1 driver pull-down |
| R15 | 4.7kΩ | 5%        | SSR2 driver pull-down |
| R16 | 10kΩ  | 5%        | Water switch pull-up  |
| R17 | 10kΩ  | 5%        | Tank level pull-up    |
| R18 | 10kΩ  | 5%        | Brew handle pull-up   |
| R19 | 4.7kΩ | 5%        | RS485 DE/RE pull-down |

#### Transistor Base Resistors (R20-R25)

| Ref | Value | Tolerance | Function       |
| --- | ----- | --------- | -------------- |
| R20 | 470Ω  | 5%        | Q1 base (K1)   |
| R21 | 470Ω  | 5%        | Q2 base (K2)   |
| R22 | 470Ω  | 5%        | Q3 base (K3)   |
| R23 | (gap) | -         | Reserved       |
| R24 | 470Ω  | 5%        | Q5 base (SSR1) |
| R25 | 470Ω  | 5%        | Q6 base (SSR2) |

#### LED Current Limiters (R30-R49)

| Ref | Value | Tolerance | Function              |
| --- | ----- | --------- | --------------------- |
| R30 | 470Ω  | 5%        | LED1 (K1 indicator)   |
| R31 | 470Ω  | 5%        | LED2 (K2 indicator)   |
| R32 | 470Ω  | 5%        | LED3 (K3 indicator)   |
| R34 | 330Ω  | 5%        | LED5 (SSR1 indicator) |
| R35 | 330Ω  | 5%        | LED6 (SSR2 indicator) |
| R48 | 330Ω  | 5%        | LED7 (status LED)     |
| R49 | 100Ω  | 5%        | Buzzer series         |

#### Communication Interface Protection (R40-R41, R44)

| Ref  | Value | Tolerance | Function                                                         |
| ---- | ----- | --------- | ---------------------------------------------------------------- |
| R40  | 1kΩ   | 5%        | ESP32 UART TX series (5V tolerance protection) + TVS (D_UART_TX) |
| R41  | 1kΩ   | 5%        | ESP32 UART RX series (5V tolerance protection) + TVS (D_UART_RX) |
| R44  | 1kΩ   | 5%        | Power meter TX series (5V tolerance protection)                  |
| R45  | 2.2kΩ | 1%        | J17 RX level shift (upper)                                       |
| R45A | 3.3kΩ | 1%        | J17 RX level shift (lower)                                       |
| R45B | 33Ω   | 5%        | J17 RX series (post-divider)                                     |

#### Control Signal Pull-ups (R71, R73)

| Ref | Value | Tolerance | Function              |
| --- | ----- | --------- | --------------------- |
| R71 | 10kΩ  | 5%        | RP2354 RUN pull-up    |
| R73 | 4.7kΩ | 5%        | WEIGHT_STOP pull-down |
| R74 | 4.7kΩ | 5%        | SPARE1 pull-down      |
| R75 | 4.7kΩ | 5%        | SPARE2 pull-down      |

#### Level Probe Circuit (R81-R89)

| Ref | Value | Tolerance | Function                     |
| --- | ----- | --------- | ---------------------------- |
| R81 | 10kΩ  | 1%        | Wien bridge feedback         |
| R82 | 4.7kΩ | 1%        | Wien bridge gain (to GND)    |
| R83 | 10kΩ  | 1%        | Wien bridge RC network       |
| R84 | 10kΩ  | 1%        | Wien bridge RC network       |
| R85 | 100Ω  | 5%        | Probe current limit          |
| R86 | 10kΩ  | 5%        | AC sense bias                |
| R87 | 100kΩ | 1%        | Comparator reference (upper) |
| R88 | 100kΩ | 1%        | Comparator reference (lower) |
| R89 | 1MΩ   | 5%        | Comparator hysteresis        |

#### Ratiometric Compensation & RS485 Biasing (R91-R94)

| Ref     | Value | Tolerance | Function                                                                                               |
| ------- | ----- | --------- | ------------------------------------------------------------------------------------------------------ |
| R91     | 10kΩ  | 1%        | 5V monitor divider (upper)                                                                             |
| R92     | 5.6kΩ | 1%        | 5V monitor divider (lower)                                                                             |
| R93     | 20kΩ  | 5%        | RS485 A line failsafe pull-up                                                                          |
| R94     | 20kΩ  | 5%        | RS485 B line failsafe pull-down                                                                        |
| R_SWDIO | 47Ω   | 5%        | SWDIO series protection (J15-6)                                                                        |
| R_SWCLK | 22Ω   | 5%        | SWCLK series protection (J15-8, optimized)                                                             |
| R_XTAL  | 1kΩ   | 5%        | Crystal series resistor (recommended to prevent overdriving crystal, per Raspberry Pi recommendations) |

---

### Capacitors - Organized by Function

#### Power Supply Filtering (C1-C6)

| Ref | Value    | Voltage | Type                 | Function                                        |
| --- | -------- | ------- | -------------------- | ----------------------------------------------- |
| C1  | 100nF X2 | 275V AC | Radial               | Mains EMI filter                                |
| C2  | 470µF    | 6.3V    | **105°C Al-Polymer** | 5V bulk capacitor (e.g., Panasonic OS-CON)      |
| C3  | 22µF     | 25V     | Ceramic              | Buck converter input                            |
| C4  | 22µF     | 10V     | Ceramic              | Buck converter output                           |
| C4A | 22µF     | 10V     | Ceramic              | Buck output (parallel)                          |
| C5  | 47µF     | 10V     | Ceramic X5R          | 3.3V rail bulk (WiFi/relay transients - ECO-05) |
| C6  | 100nF    | 25V     | Ceramic              | 3.3V digital decoupling                         |

#### ADC Reference & Sensor Filters (C7-C13)

| Ref         | Value | Voltage | Type    | Function                                                             |
| ----------- | ----- | ------- | ------- | -------------------------------------------------------------------- |
| C7          | 22µF  | 10V     | Ceramic | ADC VREF bulk                                                        |
| C7A         | 100nF | 25V     | Ceramic | ADC VREF HF decoupling                                               |
| C8          | 100nF | 25V     | Ceramic | Brew NTC filter (DC blocking)                                        |
| C9          | 100nF | 25V     | Ceramic | Steam NTC filter (DC blocking)                                       |
| C_HUM_BREW  | 10nF  | 25V     | Ceramic | Mains hum filter capacitor (Brew NTC, fc≈1.6kHz, 50/60Hz rejection)  |
| C_HUM_STEAM | 10nF  | 25V     | Ceramic | Mains hum filter capacitor (Steam NTC, fc≈1.6kHz, 50/60Hz rejection) |
| C11         | 100nF | 25V     | Ceramic | Pressure transducer filter                                           |
| C13         | 100µF | 10V     | Radial  | ESP32 bulk capacitor                                                 |

#### Level Probe Circuit (C60-C65)

| Ref | Value | Voltage | Type    | Function               |
| --- | ----- | ------- | ------- | ---------------------- |
| C60 | 100nF | 25V     | Ceramic | OPA342 VCC decoupling  |
| C61 | 10nF  | 50V     | Ceramic | Wien bridge timing     |
| C62 | 10nF  | 50V     | Ceramic | Wien bridge timing     |
| C63 | 100nF | 25V     | Ceramic | TLV3201 VCC decoupling |
| C64 | 1µF   | 25V     | Ceramic | AC coupling to probe   |
| C65 | 100nF | 25V     | Ceramic | AC sense filter        |

#### Communication & Monitoring (C70, C80, C81)

| Ref     | Value | Voltage | Type    | Function                                                                          |
| ------- | ----- | ------- | ------- | --------------------------------------------------------------------------------- |
| C70     | 100nF | 25V     | Ceramic | MAX3485 VCC decoupling                                                            |
| C80     | 100nF | 25V     | Ceramic | OPA2342 VCC decoupling                                                            |
| C81     | 100nF | 25V     | Ceramic | 5V monitor filter                                                                 |
| C_XTAL  | 22pF  | 50V     | 0603    | Crystal load capacitors (×2, calculated: C_L = 2×(15pF-3pF) = 24pF, use 22pF std) |
| C_IOVDD | 100nF | 25V     | Ceramic | RP2354 IOVDD decoupling (×6, one per pin)                                         |
| C_DVDD  | 10µF  | 10V     | 1206    | RP2354 DVDD bulk (external LDO output)                                            |
| C_DVDD2 | 100nF | 25V     | 0805    | RP2354 DVDD decoupling (external LDO output)                                      |

---

### Solder Jumpers (JP1-JP4)

| Ref | Type  | Default | Function                       |
| --- | ----- | ------- | ------------------------------ |
| JP1 | 2-pad | OPEN    | Brew NTC selection (50k/10k)   |
| JP2 | 2-pad | OPEN    | Steam NTC selection (50k/10k)  |
| JP3 | 2-pad | OPEN    | J17 RX voltage level (5V/3.3V) |
| JP4 | 3-pad | 1-2     | GPIO7 source (RS485 or TTL)    |

---

### Connectors (J1-J26)

#### High Voltage Connectors (6.3mm Spade Terminals)

| Ref | Type        | Function                             |
| --- | ----------- | ------------------------------------ |
| J1  | 2-pos spade | Mains input (L, N only - PE removed) |
| J2  | 2-pos spade | K1 output (indicator lamp)           |
| J3  | 2-pos spade | K2 output (pump)                     |
| J4  | 2-pos spade | K3 output (solenoid)                 |

#### Low Voltage Connectors

| Ref | Type           | Pins | Function                                      |
| --- | -------------- | ---- | --------------------------------------------- |
| J15 | JST-XH 2.54mm  | 8    | ESP32 display + brew-by-weight                |
| J17 | JST-XH 2.54mm  | 6    | Power meter interface (LV)                    |
| J24 | Screw terminal | 2    | Power meter HV input (L, N only - PE removed) |
| J26 | Screw terminal | 18   | Unified LV terminal (sensors, SSRs)           |

---

### Mechanical Components

#### Fuses (F1-F2)

| Ref | Rating   | Type                       | Function              |
| --- | -------- | -------------------------- | --------------------- |
| F1  | 10A 250V | 5×20mm                     | Main mains protection |
| F2  | 2A 250V  | SMD Nano² (Littelfuse 463) | HLK module protection |

#### Varistors / MOVs (RV1-RV3)

| Ref | Rating | Size | Location         | Function              |
| --- | ------ | ---- | ---------------- | --------------------- |
| RV1 | 275V   | 14mm | Mains input      | Surge protection      |
| RV2 | 275V   | 10mm | J3-NO to Neutral | Pump arc suppression  |
| RV3 | 275V   | 10mm | J4-NO to Neutral | Solenoid arc suppress |

#### Switches

| Ref | Type          | Function               |
| --- | ------------- | ---------------------- |
| SW1 | Tactile 6×6mm | Reset button (RUN pin) |

#### Other Components

| Ref | Part          | Function                                                   |
| --- | ------------- | ---------------------------------------------------------- | ---------------------------- |
| BZ1 | Passive 12mm  | Piezo buzzer                                               |
| FB1 | 600Ω @ 100MHz | ADC reference isolation                                    |
| L1  | 2.2µH 3A      | Buck converter inductor                                    |
| L2  | **REMOVED**   | **No longer needed (external LDO replaces internal SMPS)** |
| Y1  | 12 MHz        | Crystal (HC-49 or SMD)                                     | Main clock source for RP2354 |

---

### Test Points (TP1-TP3)

| Ref | Signal     | Voltage | Function                         |
| --- | ---------- | ------- | -------------------------------- |
| TP1 | GPIO20     | 3.3V    | RS485 direction control monitor  |
| TP2 | ADC_VREF   | 3.0V    | Precision reference verification |
| TP3 | 5V_MONITOR | ~1.8V   | Ratiometric calibration point    |

---

### Mounting Holes (MH1-MH4)

| Ref | Size | Type | Connection | Function                             |
| --- | ---- | ---- | ---------- | ------------------------------------ |
| MH1 | M3   | NPTH | Isolated   | No PE connection (SRif architecture) |
| MH2 | M3   | NPTH | Isolated   | Mechanical only                      |
| MH3 | M3   | NPTH | Isolated   | Mechanical only                      |
| MH4 | M3   | NPTH | Isolated   | Mechanical only                      |

---

## Numbering Gaps & Reserved Designators

The following reference designators are intentionally skipped or reserved:

| Range        | Status    | Reason                                   |
| ------------ | --------- | ---------------------------------------- |
| Q4           | Reserved  | SSR drivers use Q5/Q6 (legacy numbering) |
| R23          | Reserved  | Gap in base resistor sequence            |
| R33, R36-R39 | Reserved  | Gap in LED resistor sequence             |
| R50-R70      | Available | Reserved for future expansion            |
| R90          | Reserved  | Level probe section starts at R81        |
| R93-R100     | Available | Reserved for future sensors              |
| LED4         | Reserved  | Gap in LED sequence (legacy)             |

---

## Design Organization Principles

1. **Sequential numbering** within functional blocks
2. **Gaps preserved** for future expansion within each block
3. **Related components** use adjacent reference designators
4. **Consistent patterns**: R1x=pull-ups, R2x=base resistors, R3x=LED resistors
5. **No renumbering** of established references to maintain schematic stability

---

**Document Version:** 1.1  
**Last Updated:** January 2026 (v2.31 - RP2354 migration)
