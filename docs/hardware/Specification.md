# ECM Synchronika Custom Control Board

## Production Design Specification v2.0

**Document Purpose:** Complete technical specification for PCB design and manufacturing  
**Target:** Plug & play replacement for GICAR control board and PID controller  
**Revision:** 2.24.2  
**Date:** December 2025  
**Status:** Draft specification - Ready for review and prototype

---

**📋 Revision History:** See [`CHANGELOG.md`](CHANGELOG.md) for design evolution and peer review fixes.

---

# TABLE OF CONTENTS

1. [System Overview](#1-system-overview)
2. [Electrical Specifications](#2-electrical-specifications)
3. [Component Summary](#3-component-summary)
4. [Microcontroller & GPIO Allocation](#4-microcontroller--gpio-allocation)
5. [Power Supply Design](#5-power-supply-design)
6. [Relay & SSR Output Circuits](#6-relay--ssr-output-circuits)
7. [Sensor Input Circuits](#7-sensor-input-circuits)
8. [Communication Interfaces](#8-communication-interfaces)
9. [User Interface Components](#9-user-interface-components)
10. [Power Metering Circuit](#10-power-metering-circuit)
11. [Safety & Protection](#11-safety--protection)
12. [PCB Design Requirements](#12-pcb-design-requirements)
13. [Connector Specifications](#13-connector-specifications)
14. [Bill of Materials](#14-bill-of-materials)
15. [Testing & Validation](#15-testing--validation)
16. [Deliverables](#16-deliverables)

---

# 1. System Overview

## 1.1 Project Description

This specification defines a custom control PCB to replace the factory GICAR control board and PID controller in an ECM Synchronika dual-boiler espresso machine. The design must be:

- **Plug & Play**: Direct replacement using existing machine wiring
- **Reversible**: Original equipment can be reinstalled without modification
- **Universal Voltage**: Support 100-240V AC, 50/60Hz worldwide operation
- **Production Ready**: Suitable for small-batch or volume manufacturing

## 1.2 Design Goals

| Requirement         | Description                                        |
| ------------------- | -------------------------------------------------- |
| Temperature Control | Dual PID control for brew and steam boilers        |
| Pressure Monitoring | Real-time pressure display and profiling           |
| Safety Interlocks   | Water level, over-temperature, watchdog protection |
| Connectivity        | ESP32 display module for WiFi, MQTT, Web API       |
| Power Monitoring    | Total machine power consumption metering           |
| User Feedback       | Status LEDs, buzzer for alerts                     |
| Serviceability      | Debug port, accessible test points                 |

## 1.3 System Architecture

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                        ECM SYNCHRONIKA CONTROL SYSTEM                           │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│   ┌─────────────────┐     ┌─────────────────────────────────────────────────┐   │
│   │   MAINS INPUT   │     │              CONTROL PCB                        │   │
│   │   100-240V AC   │     │  ┌─────────────────────────────────────────┐   │    │
│   │   50/60 Hz      │────►│  │  ISOLATED AC/DC         LOW VOLTAGE     │   │    │
│   │                 │     │  │  POWER SUPPLY    ║      SECTION         │   │    │
│   └─────────────────┘     │  │  (HLK-15M05C)    ║                      │   │    │
│                           │  │       │          ║   ┌──────────────┐   │   │    │
│   ┌─────────────────┐     │  │       ▼          ║   │ Raspberry Pi │   │   │    │
│   │  POWER METER    │     │  │    5V Rail ──────╫──►│    Pico      │   │   │    │
│   │  (External)     │◄────│  │       │          ║   │   RP2350     │   │   │    │
│   │  + CT Clamp     │     │  │       ▼          ║   └──────┬───────┘   │   │    │
│   └─────────────────┘     │  │   3.3V Rail ─────╫──────────┘           │   │    │
│                           │  │                  ║                      │   │    │
│   ┌─────────────────┐     │  │   RELAY DRIVERS  ║   SENSOR INPUTS      │   │    │
│   │   RELAYS (3x)   │◄────│  │   + INDICATOR    ║   + PROTECTION       │   │  │
│   │   K1: Indicator │     │  │   LEDs           ║                      │   │  │
│   │   K2: Pump      │     │  │                  ║                      │   │  │
│   │   K3: Solenoid  │     │  ╠══════════════════╬══════════════════════╣   │  │
│   └─────────────────┘     │  │  HIGH VOLTAGE    ║    LOW VOLTAGE       │   │  │
│                           │  │  SECTION         ║    SECTION           │   │  │
│                           │  │  (ISOLATED)      ║    (SAFE)            │   │  │
│   ┌─────────────────┐     │  └─────────────────────────────────────────┘   │  │
│   │  EXTERNAL SSRs  │◄────│                                                 │  │
│   │  Brew Heater    │     │  ┌─────────────────────────────────────────┐   │  │
│   │  Steam Heater   │     │  │  CONNECTORS                             │   │  │
│   └─────────────────┘     │  │  • 6.3mm Spade terminals (machine)     │   │  │
│                           │  │  • JST-XH 8-pin (ESP32 display+weight) │   │  │
│   ┌─────────────────┐     │  │  • 2.54mm headers (service/debug)      │   │  │
│   │  ESP32 DISPLAY  │◄────│  │  • Screw terminals (sensors)           │   │  │
│   │  MODULE         │     │  └─────────────────────────────────────────┘   │  │
│   │  (External)     │     │                                                 │  │
│   └─────────────────┘     └─────────────────────────────────────────────────┘  │
│                                                                                  │
│   ┌─────────────────┐     ┌─────────────────────────────────────────────────┐  │
│   │  MACHINE LOADS  │     │  SENSORS                                        │  │
│   │  • Pump Motor   │     │  • NTC Thermistors (Brew/Steam boilers)        │  │
│   │  • 3-Way Valve  │     │  • (future expansion)                          │  │
│   │  • Brew Heater  │     │  • Pressure Transducer (0.5-4.5V)              │  │
│   │  • Steam Heater │     │  • Water Level Switches                         │  │
│   │  • Mains Lamp   │     │  • Steam Boiler Level Probe                     │  │
│   └─────────────────┘     └─────────────────────────────────────────────────┘  │
│                                                                                  │
└─────────────────────────────────────────────────────────────────────────────────┘
```

---

# 2. Electrical Specifications

## 2.1 Input Power

| Parameter       | Specification                            |
| --------------- | ---------------------------------------- |
| Input Voltage   | 100-240V AC ±10%                         |
| Frequency       | 50/60 Hz                                 |
| Maximum Current | 16A (total machine load through relays)  |
| Power Factor    | >0.9 (machine dependent)                 |
| Inrush Current  | Limited by machine's existing protection |

## 2.2 Output Power Rails

| Rail    | Voltage  | Current Capacity | Source                | Purpose                          |
| ------- | -------- | ---------------- | --------------------- | -------------------------------- |
| 5V DC   | 5.0V ±5% | **3A minimum**   | Isolated AC/DC module | Pico, relays, ESP32, SSR drivers |
| 3.3V DC | 3.3V ±3% | 500mA minimum    | LDO from 5V           | Sensors, logic                   |

## 2.3 Isolation Requirements

| Boundary              | Isolation Type | Requirement                            |
| --------------------- | -------------- | -------------------------------------- |
| Mains → 5V DC         | Reinforced     | 3000V AC for 1 minute                  |
| Relay Contacts → Coil | Basic          | 2500V AC                               |
| Power Meter → Logic   | Functional     | Via opto-isolated UART in meter module |

## 2.4 Environmental

| Parameter             | Specification                 |
| --------------------- | ----------------------------- |
| Operating Temperature | 0°C to +50°C                  |
| Storage Temperature   | -20°C to +70°C                |
| Humidity              | 20% to 90% RH, non-condensing |
| Altitude              | Up to 2000m                   |

---

# 3. Component Summary

## 3.1 Inputs (Sensors & Switches)

| ID  | Component                    | Type                  | Signal              | Connection    |
| --- | ---------------------------- | --------------------- | ------------------- | ------------- |
| S1  | Water Reservoir Switch       | SPST N.O.             | Digital, Active Low | J26 Pin 1-2   |
| S2  | Tank Level Sensor            | 2-wire Magnetic Float | Digital, Active Low | J26 Pin 3-4   |
| S3  | Steam Boiler Level Probe     | Conductivity Probe    | Digital/Analog      | J26 Pin 5     |
| S4  | Brew Handle Switch           | SPST N.O./N.C.        | Digital, Active Low | J26 Pin 6-7   |
| T1  | Brew Boiler Temp             | NTC 50kΩ @ 25°C       | Analog (ADC)        | J26 Pin 8-9   |
| T2  | Steam Boiler Temp            | NTC 50kΩ @ 25°C       | Analog (ADC)        | J26 Pin 10-11 |
| P1  | Pressure Transducer (YD4060) | 0.5-4.5V, 0-16 bar    | Analog (ADC)        | J26 Pin 14-16 |

**⚠️ SENSOR COMPATIBILITY NOTES:**

- **NTC:** Default configured for 50kΩ @ 25°C. Use JP2/JP3 solder jumpers to switch to 10kΩ (see Section 7.1).
- **Pressure:** 0.5-4.5V ratiometric only. Cannot use 4-20mA current loop sensors.

## 3.2 Outputs (Actuators)

| ID   | Component              | Load Rating         | Control             | Connection       |
| ---- | ---------------------- | ------------------- | ------------------- | ---------------- |
| K1   | Mains Indicator Lamp   | 100-240V AC, ≤100mA | Onboard Relay (3A)  | J2 (6.3mm spade) |
| K2   | Pump Motor             | 100-240V AC, 65W    | Onboard Relay (16A) | J3 (6.3mm spade) |
| K3   | Solenoid Valve (3-way) | 100-240V AC, 15W    | Onboard Relay (3A)  | J4 (6.3mm spade) |
| SSR1 | Brew Boiler Heater     | 100-240V AC, 1400W  | External SSR 40A    | J26 Pin 17-18    |
| SSR2 | Steam Boiler Heater    | 100-240V AC, 1400W  | External SSR 40A    | J26 Pin 19-20    |

## 3.3 Communication Interfaces

| Interface        | Purpose                                  | Connector                  |
| ---------------- | ---------------------------------------- | -------------------------- |
| ESP32 Display    | Main UI, WiFi, MQTT, OTA, Brew-by-Weight | JST-XH 8-pin (J15)         |
| Service/Debug    | Firmware debug, emergency access         | 2.54mm 4-pin header        |
| Power Meter (LV) | UART/RS485 communication to meter        | JST-XH 6-pin (J17)         |
| Power Meter (HV) | Mains power for external meter module    | Screw terminal 3-pos (J24) |

## 3.4 User Interface (Onboard)

| Component          | Purpose                               |
| ------------------ | ------------------------------------- |
| Status LED (Green) | System state indication               |
| 6× Indicator LEDs  | Relay/SSR status (4 Green + 2 Orange) |
| Buzzer (Passive)   | Audio alerts (PWM tones)              |
| Reset Button (SMD) | Hardware reset                        |
| Boot Button (SMD)  | Bootloader mode entry                 |

---

# 4. Microcontroller & GPIO Allocation

## 4.1 Raspberry Pi Pico 2 Specifications

| Parameter         | Value                                          |
| ----------------- | ---------------------------------------------- |
| MCU               | RP2350 Dual-core ARM Cortex-M33 @ 150MHz       |
| Flash             | 4MB onboard QSPI                               |
| SRAM              | 520KB                                          |
| GPIO              | 26 multi-function pins                         |
| ADC               | 4 channels, 12-bit, 500 ksps                   |
| UART              | 2× peripherals                                 |
| SPI               | 2× peripherals                                 |
| I2C               | 2× peripherals                                 |
| PWM               | 12× slices (24 channels)                       |
| PIO               | 3× programmable I/O blocks (12 state machines) |
| Operating Voltage | 1.8V - 5.5V (via VSYS), 3.3V logic             |
| Temperature Range | -20°C to +85°C                                 |
| Security          | ARM TrustZone, signed boot, 8KB OTP            |

## 4.2 Complete GPIO Allocation

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                        RASPBERRY PI PICO 2 GPIO MAP                             │
│                          (All 26 GPIOs Allocated)                               │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐  │
│  │  ACTIVE ANALOG INPUTS (ADC)                                              │  │
│  │  ├── GPIO26 (ADC0) ─── Brew Boiler NTC (via voltage divider)            │  │
│  │  ├── GPIO27 (ADC1) ─── Steam Boiler NTC (via voltage divider)           │  │
│  │  └── GPIO28 (ADC2) ─── Pressure Transducer (scaled 0.5-4.5V → 0.34-3.06V) │  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐  │
│  │  EXPANSION GPIOs (Available for Future Features)                        │  │
│  │  ├── GPIO16 ─── SPARE (SPI0 MISO)                                       │  │
│  │  ├── GPIO17 ─── SPARE (SPI0 CS)                                         │  │
│  │  └── GPIO18 ─── SPARE (SPI0 SCK)                                        │  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐  │
│  │  DIGITAL INPUTS (Switches & Sensors)                                     │  │
│  │  ├── GPIO2 ─── Water Reservoir Switch (internal pull-up, active low)   │  │
│  │  ├── GPIO3 ─── Tank Level Sensor (internal pull-up, active low)        │  │
│  │  ├── GPIO4 ─── Steam Boiler Level (TLV3201 comparator out, active low) │  │
│  │  └── GPIO5 ─── Brew Handle Switch (internal pull-up, active low)       │  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐  │
│  │  DIGITAL OUTPUTS (Relay & SSR Drivers)                                   │  │
│  │  ├── GPIO10 ─── Mains Indicator Lamp Relay (K1) + Green Indicator LED   │  │
│  │  ├── GPIO11 ─── Pump Relay (K2) + Green Indicator LED                   │  │
│  │  ├── GPIO12 ─── Solenoid Relay (K3) + Green Indicator LED               │  │
│  │  ├── GPIO13 ─── Brew SSR Trigger (SSR1) + Orange Indicator LED          │  │
│  │  └── GPIO14 ─── Steam SSR Trigger (SSR2) + Orange Indicator LED         │  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐  │
│  │  UART0 - ESP32 DISPLAY MODULE (8-pin JST-XH)                            │  │
│  │  ├── GPIO0 (UART0 TX) ─── ESP32 RX (33Ω series protection)             │  │
│  │  ├── GPIO1 (UART0 RX) ─── ESP32 TX (33Ω series protection)             │  │
│  │  ├── PICO RUN ◄──────── ESP32 GPIO (ESP32 resets Pico)                 │  │
│  │  └── PICO BOOTSEL ◄──── ESP32 GPIO (ESP32 controls bootloader entry)   │  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐  │
│  │  SERVICE/DEBUG PORT (4-pin header) - Shared with ESP32 on GPIO0/1       │  │
│  │  ├── GPIO0 (UART0 TX) ─── J15 (ESP32) + J16 (Service) - 33Ω protected  │  │
│  │  └── GPIO1 (UART0 RX) ─── J15 (ESP32) + J16 (Service) - 33Ω protected  │  │
│  │  ⚠️ Disconnect ESP32 cable when using service port for flashing         │  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐  │
│  │  I2C0 - ACCESSORY PORT (4-pin header)                                   │  │
│  │  ├── GPIO8 (I2C0 SDA) ─── Accessory data (4.7kΩ pull-up)               │  │
│  │  └── GPIO9 (I2C0 SCL) ─── Accessory clock (4.7kΩ pull-up)              │  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐  │
│  │  USER INTERFACE                                                          │  │
│  │  ├── GPIO15 ─── Status LED (Green, active high, 330Ω series)           │  │
│  │  └── GPIO19 ─── Buzzer (Passive piezo, PWM output, 100Ω series)        │  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐  │
│  │  POWER METERING (Universal External via UART/RS485)                     │  │
│  │  ├── GPIO6 ─── METER TX (UART to meter RX / RS485 DI)                  │  │
│  │  ├── GPIO7 ─── METER RX (UART from meter TX / RS485 RO)                │  │
│  │  └── GPIO20 ── RS485 DE/RE (Direction control for MAX3485)             │  │
│  │  Supports: PZEM-004T, JSY-MK-163T/194T, Eastron SDM, and more          │  │
│  │  No HV measurement circuitry on PCB - J24 provides pass-through to meter│  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐  │
│  │  ESP32 CONTROL SIGNALS (J15 Pins 7-8)                                   │  │
│  │  ├── GPIO21 ── WEIGHT_STOP (ESP32→Pico signal for brew-by-weight)     │  │
│  │  └── GPIO22 ── SPARE/EXPANSION (Available for future use)              │  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐  │
│  │  HARDWARE CONTROL (Direct to Pico pins, not GPIO)                       │  │
│  │  ├── RUN Pin ─── Reset Button (SMD tactile, to GND)                    │  │
│  │  └── BOOTSEL ─── Boot Button (SMD tactile, directly to QSPI_SS)        │  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
│  GPIO UTILIZATION: 25/26 available (GPIO22 spare for expansion)               │
│  ⚠️ GPIO23-25, GPIO29 are INTERNAL to Pico 2 module - NOT on header!         │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

## 4.3 GPIO Summary Table

| GPIO | Function                        | Direction | Type    | Pull          | Protection                        |
| ---- | ------------------------------- | --------- | ------- | ------------- | --------------------------------- |
| 0    | UART0 TX → ESP32                | Output    | Digital | None          | 33Ω series                        |
| 1    | UART0 RX ← ESP32                | Input     | Digital | None          | 33Ω series                        |
| 2    | Water Reservoir Switch          | Input     | Digital | Internal PU   | ESD clamp                         |
| 3    | Tank Level Sensor               | Input     | Digital | Internal PU   | ESD clamp                         |
| 4    | Steam Boiler Level (Comparator) | Input     | Digital | None          | TLV3201 output                    |
| 5    | Brew Handle Switch              | Input     | Digital | Internal PU   | ESD clamp                         |
| 6    | Meter TX (UART1)                | Output    | Digital | None          | 33Ω series (to meter RX/RS485 DI) |
| 7    | Meter RX (UART1)                | Input     | Digital | None          | 33Ω series (from meter TX/RS485)  |
| 8    | I2C0 SDA (Accessory)            | I/O       | Digital | 4.7kΩ ext. PU | Accessory expansion               |
| 9    | I2C0 SCL (Accessory)            | Output    | Digital | 4.7kΩ ext. PU | Accessory expansion               |
| 10   | Relay K1 + LED                  | Output    | Digital | None          | -                                 |
| 11   | Relay K2 + LED                  | Output    | Digital | None          | -                                 |
| 12   | Relay K3 + LED                  | Output    | Digital | None          | -                                 |
| 13   | SSR1 Trigger + LED              | Output    | Digital | None          | -                                 |
| 14   | SSR2 Trigger + LED              | Output    | Digital | None          | -                                 |
| 15   | Status LED                      | Output    | Digital | None          | -                                 |
| 16   | SPI0 MISO                       | Input     | Digital | None          | -                                 |
| 17   | SPI0 CS                         | Output    | Digital | None          | -                                 |
| 18   | SPI0 SCK                        | Output    | Digital | None          | -                                 |
| 19   | Buzzer PWM                      | Output    | PWM     | None          | -                                 |
| 20   | RS485 DE/RE                     | Output    | Digital | Pull-down     | MAX3485 direction (TTL mode: NC)  |
| 21   | WEIGHT_STOP (ESP32→Pico)        | Input     | Digital | Pull-down     | Brew-by-weight signal (J15 Pin 7) |
| 22   | SPARE/EXPANSION                 | I/O       | Digital | None          | Available via J15 Pin 8           |
| 26   | ADC0 - Brew NTC                 | Input     | Analog  | None          | RC filter                         |
| 27   | ADC1 - Steam NTC                | Input     | Analog  | None          | RC filter                         |
| 28   | ADC2 - Pressure                 | Input     | Analog  | None          | RC filter, divider                |

### ⚠️ RP2350 GPIO Considerations

**Internal GPIOs (NOT available on Pico 2 header):**

| GPIO   | Internal Function | Notes                              |
| ------ | ----------------- | ---------------------------------- |
| GPIO23 | SMPS Power Save   | Controls regulator efficiency mode |
| GPIO24 | VBUS Detect       | High when USB connected            |
| GPIO25 | Onboard LED       | Green LED on Pico 2 module         |
| GPIO29 | VSYS/3 ADC        | Internal voltage monitoring        |

**⚠️ These GPIOs are NOT exposed on the 40-pin header - do not attempt to use them!**

### RP2350 E9 Errata (GPIO Input Latching)

The RP2350 has a documented errata (E9) where GPIO inputs can latch in a high state under certain conditions. **Mitigation is already implemented in this design:**

| Input GPIO | Function          | Protection                                      | Notes                   |
| ---------- | ----------------- | ----------------------------------------------- | ----------------------- |
| GPIO2-5    | Switches          | Internal pull-up + external pull-down (R10-R13) | Ensures defined state   |
| GPIO20-21  | RS485/WEIGHT_STOP | 10kΩ pull-down                                  | Prevents false triggers |

**All digital inputs have either internal pull-ups OR external pull-down resistors**, ensuring they cannot float and trigger the E9 errata condition.

### ⚠️ RP2350 ADC E9 Errata (ADC Leakage Current)

The RP2350 A2 stepping has a documented hardware issue (Erratum E9) affecting ADC-capable GPIO pins (GPIO 26-29).

**Mechanism:**
When voltage on an ADC pin exceeds ~2.0V or when the pin is in a high-impedance state, a parasitic leakage path activates within the pad driver circuitry. This leakage is non-linear and temperature-dependent.

**Impact on This Design:**

| ADC Pin | Function  | Source Impedance | Risk Level | Notes                            |
| ------- | --------- | ---------------- | ---------- | -------------------------------- |
| ADC0    | Brew NTC  | ~3.3kΩ (R1)      | Medium     | Leakage causes temperature error |
| ADC1    | Steam NTC | ~1.2kΩ (R2)      | Lower      | Lower impedance reduces impact   |
| ADC2    | Pressure  | ~3.8kΩ (R4‖R3)   | Medium     | Leakage causes pressure offset   |

**Quantitative Impact:**
For NTC circuits with R1=3.3kΩ, a few microamps of leakage induces several millivolts of offset. At brewing temperatures (93°C) where dR/dT is reduced, this can translate to >1°C temperature error.

**Mitigations (choose based on accuracy requirements):**

1. **Silicon Stepping:** Verify RP2350 is B0 stepping or later (may resolve E9)
2. **Firmware Calibration:** Implement temperature-dependent offset compensation
3. **External ADC (future revision):** Use dedicated ADC IC with voltage follower buffers
4. **Lower Source Impedance:** Consider 1kΩ series + external buffer for critical channels

**Current Design Status:**
The RC filter capacitors (C8, C9, C11) suppress AC noise but do NOT mitigate DC leakage. For ±0.5°C espresso extraction accuracy, firmware calibration against a reference thermometer is **required** during commissioning.

---

# 5. Power Supply Design

## 5.1 AC/DC Isolated Power Module

Use an integrated isolated AC/DC converter module for safety and simplicity.

### Power Budget Analysis

| Consumer            | Typical    | Peak       | Notes                      |
| ------------------- | ---------- | ---------- | -------------------------- |
| Raspberry Pi Pico 2 | 50mA       | 100mA      | Via VSYS                   |
| Relay coils (×3)    | 80mA       | 150mA      | K2:70mA, K1/K3:40mA each   |
| SSR drivers (×2)    | 10mA       | 20mA       | Transistor current         |
| ESP32 module        | 150mA      | 500mA      | **WiFi TX spikes!**        |
| Indicator LEDs (×6) | 30mA       | 60mA       | 3 relay + 2 SSR + 1 status |
| Buzzer              | 5mA        | 30mA       | When active                |
| 3.3V Buck load      | 30mA       | 100mA      | Sensors, margin            |
| **TOTAL**           | **~355mA** | **~910mA** |                            |

**Minimum: 1.5A, Selected: Hi-Link HLK-15M05C (3A/15W)** - 3× headroom over 1A peak

### AC/DC Module Selection

| Parameter          | **HLK-15M05C**  | Mean Well IRM-20-5 | HLK-5M05 (1A)    |
| ------------------ | --------------- | ------------------ | ---------------- |
| Output Voltage     | 5V DC ±2%       | 5V DC ±5%          | 5V DC ±2%        |
| **Output Current** | **3A (15W)**    | 4A (20W)           | 1A (5W)          |
| Input Voltage      | 100-240V AC     | 85-264V AC         | 100-240V AC      |
| Isolation          | 3000V AC        | 3000V AC           | 3000V AC         |
| Efficiency         | 82%             | 87%                | 80%              |
| **Package**        | **48×28×18mm**  | 52×27×24mm         | 34×20×15mm       |
| Safety             | CE              | UL, CE, CB         | CE               |
| **Recommendation** | **Best choice** | Alternative        | **Insufficient** |

**Selected: Hi-Link HLK-15M05C** (3A/15W, adequate for ~1.1A peak load with 3× headroom)

### Power Module Schematic

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                           AC/DC POWER SUPPLY SECTION                            │
│                        ⚠️  HIGH VOLTAGE - MAINS CONNECTED ⚠️                     │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│                                                                                 │
│    MAINS INPUT                    ISOLATED MODULE                    OUTPUT    │
│    ───────────                    ───────────────                    ──────    │
│                                                                                 │
│    L (Live) ────┬────[F1]────┬────────────────────────────────┐               │
│                 │   (10A)    │                                │               │
│            ┌────┴────┐  ┌────┴────┐     ┌─────────────────┐  │               │
│            │  MOV    │  │   X2    │     │                 │  │               │
│            │ 275V    │  │  100nF  │     │   HLK-15M05C    │  │               │
│            │ (RV1)   │  │  (C1)   │     │   or similar    │  │               │
│            └────┬────┘  └────┬────┘     │                 │  │               │
│                 │            │          │  L ─────────────┼──┘               │
│                 │            │          │                 │                   │
│    N (Neutral) ─┴────────────┴──────────┤  N ─────────────┼──┐               │
│                                         │                 │  │               │
│                                         │      ═══════    │  │    ┌────────┐ │
│                                         │   (Isolation)   │  │    │  5V    │ │
│                                         │      ═══════    │  │    │  Rail  │ │
│                                         │                 │  │    │        │ │
│                                         │  +5V ───────────┼──┼───►│ +5V DC │ │
│    PE (Earth) ─────[To Chassis]         │                 │  │    │        │ │
│                                         │  GND ───────────┼──┴───►│  GND   │ │
│                                         │                 │       │        │ │
│                                         └─────────────────┘       └────────┘ │
│                                                                                 │
│    Component Details:                                                          │
│    ─────────────────                                                           │
│    F1: Fuse, 10A 250V, 5x20mm glass, slow-blow (relay-switched loads only)   │
│    F2: Fuse, 2A 250V, 5x20mm, slow-blow (HLK module protection)               │
│    RV1: MOV/Varistor, 275V AC, 14mm disc (surge protection)                   │
│    C1: X2 safety capacitor, 100nF 275V AC (EMI filter)                        │
│                                                                                 │
│    Optional EMI Filter (for CE compliance):                                    │
│    ─────────────────────────────────────────                                   │
│    Add common-mode choke (CMC) between mains input and HLK module             │
│                                                                                 │
│         L ──┬──[CMC]──┬── L (to module)                                       │
│             │    ║    │                                                        │
│         N ──┴────╫────┴── N (to module)                                       │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

## 5.2 Secondary Power Rails

### 5V to 3.3V Synchronous Buck Converter

Uses a synchronous buck converter for high efficiency and minimal heat dissipation,
critical for reliable operation inside hot espresso machine enclosures.

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                      3.3V SYNCHRONOUS BUCK CONVERTER                            │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│      5V Rail                                                     3.3V Rail     │
│         │                                                            │         │
│         │                                                            │         │
│    ┌────┴────┐     ┌────────────────────────────┐              ┌────┴────┐    │
│    │  22µF   │     │      TPS563200DDCR         │              │  22µF   │    │
│    │  25V    │     │   Synchronous Buck 3A      │              │  10V    │    │
│    │ Ceramic │     │                            │              │ Ceramic │    │
│    │  (C3)   │     │  VIN              VOUT ────┼──[L1 2.2µH]──┤  (C4)   │    │
│    └────┬────┘     │                            │              │         │    │
│         │      ┌──►│  EN               SW ──────┘              └────┬────┘    │
│         │      │   │                            │                    │         │
│         │      │   │  FB ◄──────────────────────┼────────────────────┼────┐   │
│         │      │   │                            │              ┌────┴────┐│   │
│         │      │   │         GND                │              │  22µF   ││   │
│         │      │   └──────────┬─────────────────┘              │  10V    ││   │
│         │      │              │                                │ (C4A)   ││   │
│         └──────┘              │                                └────┬────┘│   │
│                               │                                     │     │   │
│        GND                   GND                                   GND    │   │
│                                                                            │   │
│    Feedback Network (sets 3.3V output):                                   │   │
│    ────────────────────────────────────                                   │   │
│                           ┌────┴────┐                                     │   │
│                           │  33kΩ   │  R_FB1                              │   │
│                           │   1%    │                                     │   │
│                           └────┬────┘                                     │   │
│                                │                                          │   │
│                           FB ──┴──────────────────────────────────────────┘   │
│                                │                                              │
│                           ┌────┴────┐                                         │
│                           │  10kΩ   │  R_FB2                                  │
│                           │   1%    │                                         │
│                           └────┬────┘                                         │
│                                │                                              │
│                               GND                                             │
│                                                                                 │
│    Regulator Selection:                                                        │
│    ───────────────────                                                         │
│    Selected: TPS563200DDCR (SOT-23-6)                                         │
│      - Topology: Synchronous Buck (integrated FETs)                           │
│      - Efficiency: >90% (vs ~34% for LDO)                                     │
│      - Max output: 3A continuous                                               │
│      - Switching freq: 650kHz (allows small inductor)                         │
│      - Thermal: Minimal self-heating even at full load                        │
│                                                                                 │
│    ✅ WHY BUCK INSTEAD OF LDO?                                                │
│    ──────────────────────────────                                              │
│    LDO Problem: P_dissipated = (5V - 3.3V) × I_load = 1.7V × I                │
│    At 250mA: 0.425W dissipation → SOT-23-5 junction temp exceeds limits       │
│    in hot espresso machine environment (60°C ambient + 106°C rise = 166°C)   │
│                                                                                 │
│    Buck Solution: >90% efficient, heat dissipation reduced by 10x             │
│    Same footprint as SOT-23-5 LDO, minimal BOM change                         │
│                                                                                 │
│    External Components:                                                        │
│    ───────────────────                                                         │
│    L1:    2.2µH, 3A saturation, DCR <100mΩ (Murata LQH32CN2R2M23 or equiv)   │
│           ⚠️ 2.2µH per TI datasheet for 3.3V output - NOT 4.7µH!            │
│           D-CAP2 topology requires adequate ripple for stable operation.      │
│    C3:    22µF 25V X5R Ceramic, 1206 (input)                                  │
│    C4:    22µF 10V X5R Ceramic, 1206 (output)                                 │
│    C4A:   22µF 10V X5R Ceramic, 1206 (output, parallel for ripple)           │
│    R_FB1: 33kΩ 1% 0805 (feedback upper resistor, FB to VOUT/3.3V)            │
│    R_FB2: 10kΩ 1% 0805 (feedback lower resistor, FB to GND)                  │
│                                                                                │
│    Output Voltage Calculation:                                                │
│    ──────────────────────────                                                 │
│    V_OUT = 0.768V × (1 + R_FB1/R_FB2)                                        │
│    V_OUT = 0.768V × (1 + 33kΩ/10kΩ) = 0.768V × 4.3 = 3.30V ✓                │
│                                                                                 │
│    Pico Internal Regulator Configuration:                                    │
│    ─────────────────────────────────────────────                              │
│    The Pico 2's Pin 37 (3V3_EN) is connected to GND, which DISABLES the      │
│    internal RT6150B buck-boost regulator. This allows the external            │
│    TPS563200 to power the ENTIRE 3.3V domain via Pico Pin 36 (3V3).          │
│                                                                                 │
│    WHY? Connecting two regulator outputs in parallel ("hard parallel")        │
│    causes feedback loop contention and potential reverse current damage.      │
│    The Pico's 3V3 pin is NOT 5V-tolerant as an input when the internal       │
│    regulator is active. Disabling via 3V3_EN=GND is the correct approach.    │
│                                                                                 │
│    Load Budget (3.3V Rail - Entire System):                                    │
│    ─────────────────────────────────────────                                   │
│    RP2350 (Pico 2): ~50mA typical, 100mA peak (with PIO active)              │
│    NTC dividers: ~1mA brew (3.3kΩ), ~3mA steam (1.2kΩ)                       │
│    MAX3485 RS485: ~1mA                                                         │
│    TLV3201 comparator: ~1mA                                                    │
│    OPA342 op-amp: ~1mA                                                         │
│    LM4040 reference: ~2mA                                                      │
│    I2C pullups, misc: ~5mA                                                     │
│    Total: ~70mA typical, 250mA peak → TPS563200 3A has ample margin          │
│                                                                                 │
│    ESP32: Power from 5V rail (J15 Pin 1) - has its own LDO.                  │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

### Precision ADC Voltage Reference (Buffered)

**Purpose:** Provides stable, low-drift reference for NTC temperature measurements.
Using 3.3V rail directly as ADC reference couples LDO/buck thermal drift into readings.

**Design Note:** The LM4040 is buffered by an op-amp to prevent voltage collapse under load.

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                  PRECISION ADC VOLTAGE REFERENCE (BUFFERED)                     │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│      3.3V Rail                                                                  │
│         │                                                                       │
│    ┌────┴────┐                                                                  │
│    │   1kΩ   │  R7 - Bias resistor                                             │
│    │   1%    │  I_bias = (3.3V - 3.0V) / 1kΩ = 0.3mA                          │
│    └────┬────┘  (min 60µA for LM4040, only drives buffer input ~pA)           │
│         │                                                                       │
│         ├────────────────────────► LM4040_VREF (3.0V, unloaded)               │
│         │                                 │                                     │
│    ┌────┴────┐                     ┌──────┴──────┐                             │
│    │ LM4040  │                     │    U9A      │  OPA2342UA (half)           │
│    │  3.0V   │  U5                 │   Buffer    │                             │
│    │ Shunt   │                     │  (+) ───────┤  Unity-gain follower        │
│    │  Ref    │                     │             │  Vout = Vin                 │
│    └────┬────┘                     │  (-) ◄──┬───┤  (feedback)                 │
│         │                          │         │   │                             │
│        GND                         └────┬────┘   │                             │
│                                         │        │                             │
│                                         └────────┴─────────► ADC_VREF (3.0V)  │
│                                                                   │            │
│                                                              ┌────┴────┐       │
│                                                              │  22µF   │ C7    │
│                                                              └────┬────┘       │
│                                                              ┌────┴────┐       │
│                                                              │  100nF  │ C7A   │
│                                                              └────┬────┘       │
│                                                                   │            │
│                                                                  GND           │
│                                                                                 │
│    Buffer Design Rationale:                                                   │
│    ────────────────────────────────────────                                    │
│    Without buffer, R7 provides only 0.3mA to share between LM4040 and loads.  │
│                                                                                 │
│    NTC Load Current (at operating temperatures):                               │
│    • Brew NTC at 93°C: R_NTC ≈ 3.5kΩ → I = 3.0V/(3.3kΩ+3.5kΩ) ≈ 441µA        │
│    • Steam NTC at 135°C: R_NTC ≈ 1kΩ → I = 3.0V/(1.2kΩ+1kΩ) ≈ 1.36mA         │
│    • Total: ~1.8mA (6× more than available 0.3mA!)                            │
│                                                                                 │
│    Without buffer: ADC_VREF collapses to ~0.5V → SYSTEM FAILURE               │
│    With buffer: Op-amp drives load from 3.3V rail → ADC_VREF stable at 3.0V   │
│                                                                                 │
│    Buffer Selection:                                                           │
│    ─────────────────                                                           │
│    U9: OPA2342UA (SOIC-8) - Dual rail-to-rail op-amp                          │
│      - U9A: VREF buffer (unity gain follower)                                 │
│      - U9B: Available for future use (spare)                                  │
│      - Gain bandwidth: 1MHz (adequate for DC reference)                       │
│      - Input bias: ~2pA (negligible load on LM4040)                           │
│      - Output: Can drive >10mA (plenty for sensor networks)                   │
│                                                                                 │
│    Reference Selection:                                                        │
│    ───────────────────                                                         │
│    U5: LM4040DIM3-3.0 (SOT-23-3)                                              │
│      - Voltage: 3.000V ±0.5% (A-grade)                                        │
│      - Tempco: 100ppm/°C max                                                  │
│      - I_bias: 60µA to 15mA (we use 300µA, well within range)                 │
│                                                                                 │
│    Connection to NTC Pull-ups:                                                 │
│    ──────────────────────────                                                  │
│    R1 (Brew NTC) and R2 (Steam NTC) connect to ADC_VREF (buffered output)    │
│    This eliminates supply voltage variations from temperature readings        │
│                                                                                 │
│    Test Point: TP2 provides access to ADC_VREF for calibration verification  │
│                                                                                 │
│    Component Values:                                                           │
│    ─────────────────                                                           │
│    U5:    LM4040DIM3-3.0, 3.0V shunt ref, SOT-23-3                            │
│    U9:    OPA2342UA, dual RRIO op-amp, SOIC-8                                 │
│    R7:    1kΩ 1%, 0805 (bias resistor)                                        │
│    R_ISO: 47Ω 1%, 0805 (buffer output isolation - prevents oscillation!)     │
│    C7:    22µF 10V X5R Ceramic, 1206 (bulk)                                   │
│    C7A:   100nF 25V Ceramic, 0805 (HF decoupling)                             │
│    C80:   100nF 25V Ceramic, 0805 (U9 VCC decoupling)                         │
│                                                                                │
│    R_ISO (47Ω) isolates the op-amp from the 22µF load, preventing oscillation│
│    while maintaining DC accuracy.                                             │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

## 5.3 Power Distribution and Filtering

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                          POWER DISTRIBUTION NETWORK                             │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│                                                                                 │
│    From HLK-15M05C                                                             │
│         │                                                                       │
│         │    ┌─────────────────────────────────────────────────────────────┐  │
│         │    │                    5V DISTRIBUTION                          │  │
│         ▼    │                                                              │  │
│    ┌────────┐│    ┌──────┐        ┌──────┐        ┌──────┐                │  │
│    │ 470µF  ├┼───►│ Pico │───────►│Relays│───────►│ SSR  │                │  │
│    │  6.3V  ││    │ VSYS │        │Driver│        │Driver│                │  │
│    │Polymer ││    └───┬──┘        └──────┘        └──────┘                │  │
│    └────┬───┘│        │                                                    │  │
│         │    │   ┌────┴────┐      ┌─────────────────────────────────────┐ │  │
│         │    │   │  100nF  │      │ Place 100nF ceramic at each load   │ │  │
│         │    │   │ Ceramic │      │ Decoupling capacitors              │ │  │
│         │    │   └────┬────┘      └─────────────────────────────────────┘ │  │
│         │    │        │                                                    │  │
│        GND   │       GND                                                   │  │
│         │    └─────────────────────────────────────────────────────────────┘  │
│         │                                                                       │
│         │    ┌─────────────────────────────────────────────────────────────┐  │
│         │    │                 3.3V ANALOG SECTION                          │  │
│         ▼    │      (Isolated from digital noise)                          │  │
│    ┌────────┐│                                                              │  │
│    │ LDO    ││    3.3V_A (Analog)                                          │  │
│    │3.3V    │├────────┬───────────────────────────────────────────────────┐│  │
│    └────┬───┘│        │                                                    ││  │
│         │    │   ┌────┴────┐     ┌────────┐     ┌────────┐               ││  │
│         │    │   │ Ferrite │     │ 22µF   │     │ 100nF  │               ││  │
│         │    │   │  Bead   │     │ Tant.  │     │Ceramic │               ││  │
│         │    │   │ 600Ω    │     │        │     │        │               ││  │
│         │    │   └────┬────┘     └───┬────┘     └───┬────┘               ││  │
│         │    │        │              │              │                     ││  │
│         │    │        └──────────────┴──────────────┴───────► ADC_VREF   ││  │
│         │    │                                                            ││  │
│         │    │   NTC Thermistors ─────────────────────► ADC0, ADC1       ││  │
│         │    │   Pressure Transducer ─────────────────► ADC2             ││  │
│         │    │   (SPI reserved for future expansion)                      ││  │
│         │    │                                                            ││  │
│        GND   │                                                            ││  │
│         │    └────────────────────────────────────────────────────────────┘│  │
│         │                                                                       │
│         │    ┌─────────────────────────────────────────────────────────────┐  │
│         │    │                 3.3V DIGITAL SECTION                         │  │
│         ▼    │                                                              │  │
│    ┌────────┐│    3.3V_D (Digital)                                         │  │
│    │ LDO    ││────────┬───────────────────────────────────────────────────┐│  │
│    │ Same   ││   ┌────┴────┐                                               ││  │
│    └────┬───┘│   │ 100nF   │   Digital I/O, LEDs, etc.                    ││  │
│         │    │   │ Ceramic │                                               ││  │
│         │    │   └────┬────┘                                               ││  │
│        GND   │       GND                                                   ││  │
│              └─────────────────────────────────────────────────────────────┘│  │
│                                                                                 │
│    Note: 3.3V_A and 3.3V_D can share the same LDO, but use a ferrite bead    │
│    or small inductor to isolate the analog section from digital switching     │
│    noise. Connect grounds at a single point near the Pico ADC_GND pin.        │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

## 5.4 Decoupling Capacitor Placement

| Location                | Capacitor | Type             | Notes                    |
| ----------------------- | --------- | ---------------- | ------------------------ |
| 5V rail main            | 470µF     | Polymer (6.3V)   | Near HLK-15M05C output   |
| 5V at Pico VSYS         | 100nF     | Ceramic (0805)   | Adjacent to pin          |
| 5V at each relay driver | 100nF     | Ceramic (0805)   | Suppress switching noise |
| 3.3V LDO output (U3)    | 47µF      | Tantalum/Ceramic | For sensors, low ESR     |
| 3.3V Pico output (3V3)  | 100nF     | Ceramic (0805)   | Pico internal rail only  |
| 3.3V at each ADC input  | 100nF     | Ceramic (0603)   | Filter network           |
| AGND/DGND star point    | 10µF      | Ceramic          | Optional, reduces noise  |

---

# 6. Relay & SSR Output Circuits

## 6.1 Relay Driver Circuit

All relays use identical driver circuits with integrated indicator LEDs.

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                           RELAY DRIVER CIRCUIT                                  │
│                    (Identical for K1, K2, K3)                                  │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│                                5V Rail                                          │
│                                  │                                              │
│              ┌───────────────────┴───────────────────┐                         │
│              │                                       │                         │
│         ┌────┴────┐                             ┌────┴────┐                    │
│         │  Relay  │                             │  470Ω   │ ← R30+n           │
│         │  Coil   │                             │  (LED)  │   LED resistor    │
│         │  5V DC  │                             └────┬────┘                    │
│         │         │                                  │                         │
│         └────┬────┘                             ┌────┴────┐                    │
│              │                                  │   LED   │                    │
│         ┌────┴────┐                             │  Green  │  ← Indicator      │
│         │  1N4007 │  ← Flyback diode            │  0805   │                    │
│         │   (D)   │    Catches coil spike       └────┬────┘                    │
│         └────┬────┘                                  │                         │
│              │                                       │                         │
│              └───────────────────┬───────────────────┤                         │
│                                  │                   │                         │
│                             ┌────┴────┐              │                         │
│                             │    C    │              │                         │
│                             │ MMBT2222│              │                         │
│                             │  (Q)    │              │                         │
│     GPIO ─────────[1kΩ]────►│    B    │              │                         │
│    (10-14)        R20+n     │    E    ├──────────────┘                         │
│              │              └────┬────┘                                        │
│         ┌────┴────┐              │                                             │
│         │  10kΩ   │             GND                                            │
│         │R10+n    │  ← Pull-down (relay OFF at boot)                          │
│         └────┬────┘                                                            │
│              │                                                                  │
│             GND                                                                │
│                                                                                 │
│    OPERATION:                                                                  │
│    ──────────                                                                  │
│    GPIO LOW  → Transistor OFF → Relay OFF, LED OFF                            │
│    GPIO HIGH → Transistor ON  → Relay ON, LED ON                              │
│                                                                                 │
│    Relay coil current:                                                       │
│    • K2 (G5LE-1A4-E): 5V / ~70Ω = ~70mA                                     │
│    • K1/K3 (APAN3105): 5V / ~125Ω = ~40mA                                   │
│    LED current: (5V - 2.0V) / 470Ω = 6.4mA (bright indicator)                │
│                                                                                 │
│    Component Specifications:                                                   │
│    ─────────────────────────                                                   │
│    Q: MMBT2222A (SOT-23) - NPN transistor, Ic(max)=600mA, Vce(sat)<0.4V       │
│    UF4007: Fast flyback diode, 1A, 1000V, 75ns recovery (DO-41)               │
│           Fast recovery type for snappier relay contact opening               │
│    LED: Green 0805, Vf=2.0V, If=6mA                                           │
│    R20+n: 470Ω 5% 0805 (base resistor)                                        │
│    R30+n: 470Ω 5% 0805 (LED resistor - brighter than 1kΩ)                     │
│    R10+n: 10kΩ 5% 0805 (pull-down)                                            │
│                                                                                 │
│    RELAY SELECTION (Optimized by load):                                       │
│    ────────────────────────────────────                                       │
│    K1 (Mains Indicator): Panasonic APAN3105 - 3A @ 250V AC, 5V coil, slim 5mm│
│              Switches mains indicator lamp on machine front panel            │
│              (~100mA load). 3A relay provides ample margin.                  │
│    K2 (Pump): Omron G5LE-1A4-E DC5 - 16A @ 250V AC, 5V coil, standard size  │
│              ⚠️ MUST use -E (high capacity) variant for 16A rating!          │
│              Standard G5LE-1A4 is only 10A - insufficient for pump inrush    │
│    K3 (Solenoid): Panasonic APAN3105 - 3A @ 250V AC, 5V coil, slim 5mm       │
│              Solenoid ~15W (~0.5A) - 3A rating is plenty                      │
│                                                                                 │
│    SPACE SAVINGS: K1+K3 slim relays save ~16mm PCB width                      │
│                                                                                 │
│    MOV Surge Protection (Inductive Loads):                                   │
│    ─────────────────────────────────────────────────────                      │
│    The Pump (Ulka) and Solenoid Valves are INDUCTIVE loads. When the relay   │
│    contacts open, they generate high-voltage arcs that cause:                 │
│    • Premature relay contact welding/pitting (critical with slim relays!)    │
│    • EMI spikes that can reset the RP2350 or freeze I2C/SPI                  │
│    • Even with opto-isolation, RF noise couples into PCB traces              │
│                                                                                 │
│    SOLUTION: MOV (Varistor) across relay CONTACTS (HV side)                  │
│    ─────────────────────────────────────────────────────────                 │
│                                                                                │
│         Relay Contact (NO)                                                    │
│              │                                                                │
│              ├───────────┐                                                    │
│              │           │                                                    │
│         To  Load     ┌───┴───┐                                               │
│        (Pump/Valve)  │  MOV  │  RV2/RV3 (275V AC Varistor)                   │
│                      │ 275V  │  10mm disc, ~4mm thick                        │
│                      └───┬───┘                                               │
│                          │                                                    │
│                   Relay COM (or Load return)                                  │
│                                                                                │
│    MOV vs RC Snubber:                                                        │
│    ──────────────────                                                        │
│    • MOV is ~70% smaller than X2 capacitor + resistor                       │
│    • No resistor needed - simpler BOM, fewer components                     │
│    • Faster clamping - MOV responds in nanoseconds                          │
│    • Standard protection for espresso machine relays                        │
│                                                                                │
│    MOV Component Specifications:                                             │
│    ─────────────────────────────                                             │
│    Part: S10K275 or equivalent (275V AC, 10mm disc)                         │
│    Footprint: Disc_D10mm_W5.0mm_P7.50mm                                      │
│    Clamping Voltage: ~455V @ 1mA                                            │
│    Surge Current: 2500A (8/20µs)                                            │
│                                                                                │
│    MOV Placement on PCB:                                                     │
│    ──────────────────────────────────────────                                │
│    • RV2: From J3-NO (pump output terminal) to Neutral bus                   │
│    • RV3: From J4-NO (solenoid output terminal) to Neutral bus               │
│    • Place MOVs on LOAD side, not across relay contacts                      │
│    • HV side - maintain >6mm clearance to LV section                        │
│                                                                                │
│    Which Relays Need MOVs?                                                   │
│    ───────────────────────                                                   │
│    • K2 (Pump) - MANDATORY - RV2 from J3-NO to N (across load)              │
│    • K3 (Solenoid) - MANDATORY - RV3 from J4-NO to N (across load)          │
│    • K1 (Mains Lamp) - NOT NEEDED (resistive load, no inductive kickback)   │
│    • SSRs (heaters) - NOT NEEDED (resistive load)                           │
│                                                                                │
│    ⚠️  IEC 60335-1 SAFETY COMPLIANCE:                                        │
│    ─────────────────────────────────────                                     │
│    MOVs across relay CONTACTS create single-fault dangerous condition.       │
│    If MOV shorts → actuator bypasses control logic → violates §19.11.2.     │
│    MOVs across LOAD allow safe failure: L-N short → fuse clears → safe.     │
│                                                                                │
└────────────────────────────────────────────────────────────────────────────────┘
```

## 6.2 SSR Trigger Circuit

External SSRs require 4-30V DC trigger input. Pico's 3.3V GPIO cannot drive SSR directly.
Solution: NPN transistor as low-side switch provides full 5V to SSR.

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                           SSR TRIGGER CIRCUIT                                   │
│                    (For SSR1 Brew Heater, SSR2 Steam Heater)                   │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    IMPORTANT: SSR has internal current limiting. No series resistor needed!   │
│    SSR input spec: 4-30V DC, ~10mA (check your SSR datasheet)                 │
│                                                                                 │
│    ⚠️  BEST PRACTICE: Indicator LED is on LOGIC SIDE (GPIO-driven)           │
│    This ensures known current to SSR and avoids current splitting issues.     │
│                                                                                 │
│                                5V Rail                                          │
│                                  │                                              │
│                                  │                                              │
│                                  ▼                                              │
│                        ┌──────────────────┐                                    │
│                        │  To External     │                                    │
│                        │  SSR Input (+)   │                                    │
│                        │                  │                                    │
│                        │ (J26 Pin 17/19)  │                                    │
│                        └────────┬─────────┘                                    │
│                                 │                                              │
│                                 │              ┌───────────────────┐           │
│                                 │              │                   │           │
│                                 │              │  External SSR     │           │
│                                 │              │  (has internal    │           │
│                                 │              │   LED + resistor) │           │
│                                 │              │                   │           │
│                                 │              └───────────────────┘           │
│                                 │                                              │
│                        ┌────────┴─────────┐                                    │
│                        │  To External     │                                    │
│                        │  SSR Input (-)   │                                    │
│                        │                  │                                    │
│                        │ (J26 Pin 18/20)  │                         ┌────────┐ │
│                        └────────┬─────────┘                         │   C    │ │
│                                 │                                   │MMBT2222│ │
│                                 │                                   │ Q5/Q6  │ │
│                                 └──────────────────────────────────►│   E    │ │
│                                                                     └───┬────┘ │
│                                                                         │      │
│    GPIO ────────[1kΩ R24/25]───┬────────────────────────────────────►│B      │
│    (13/14)                      │                                      │      │
│                                 │                                     ─┴─     │
│                            ┌────┴────┐                                GND     │
│                            │  10kΩ   │                                        │
│                            │ R14/15  │                                        │
│                            │Pull-down│                                        │
│                            └────┬────┘                                        │
│                                ─┴─                                            │
│                                GND                                            │
│                                                                                 │
│    INDICATOR LED (Logic Side - separate from SSR drive):                     │
│    ──────────────────────────────────────────────────────                    │
│                                                                                 │
│    GPIO ─────────────[1kΩ]─────┬────────► (shared with transistor base)     │
│    (13/14)                      │                                             │
│                            ┌────┴────┐                                        │
│                            │  330Ω   │  ← LED resistor (separate)            │
│                            │  R34/35 │                                        │
│                            └────┬────┘                                        │
│                                 │                                             │
│                            ┌────┴────┐                                        │
│                            │   LED   │                                        │
│                            │ Orange  │  ← Heater Active Indicator            │
│                            │  0805   │     (logic-side, not SSR-side)        │
│                            └────┬────┘                                        │
│                                 │                                             │
│                                GND                                            │
│                                                                                 │
│    OPERATION:                                                                  │
│    ───────────                                                                 │
│    GPIO LOW  → Transistor OFF → SSR- floating  → SSR OFF (no current path)   │
│              → LED OFF (on logic side)                                        │
│    GPIO HIGH → Transistor ON  → SSR- to GND    → SSR ON  (~5V across SSR)    │
│              → LED ON (on logic side)                                         │
│                                                                                 │
│    Voltage at SSR = 5V - Vce(sat) = 5V - 0.2V = 4.8V  ✓ (exceeds 4V min)     │
│                                                                                 │
│    WHY LED ON LOGIC SIDE?                                                     │
│    ───────────────────────                                                    │
│    • Ensures known, stable current to SSR (not split with LED)               │
│    • SSR current: 5V / R_internal ≈ 10mA (determined by SSR only)           │
│    • LED current: (3.3V - 2.0V) / 330Ω ≈ 4mA (separate path)                │
│    • No current splitting = predictable SSR triggering                        │
│    • Easier troubleshooting (LED and SSR independently testable)             │
│                                                                                 │
│    SSR Specifications (External, user-supplied):                              │
│    ─────────────────────────────────────────────                              │
│    Model: KS15 D-24Z25-LQ (or equivalent)                                     │
│      - Input: 4-32V DC (we provide ~4.8V ✓)                                   │
│      - Input current: ~10mA (handled by SSR internal limiting)                │
│      - Output: 24-280V AC @ 25A (machine is 220-240V)                         │
│      - Zero-cross switching (reduces EMI)                                      │
│                                                                                 │
│    ⚠️  SSRs MUST be mounted on adequate heatsink!                             │
│    ⚠️  Dissipation: ~1W per amp at full load                                  │
│                                                                                 │
│    Connector: J26 Pin 17-18 (SSR1), Pin 19-20 (SSR2)                          │
│                                                                                 │
│    Component Values:                                                           │
│    ─────────────────                                                           │
│    Q5-Q6:   MMBT2222A (SOT-23), Vce(sat) < 0.3V @ 100mA                       │
│    R24-25:  470Ω 5% 0805 (base drive, ~6mA for hard saturation)               │
│    R14-15:  4.7kΩ 5% 0805 (pull-down per RP2350 errata E9)                    │
│    R34-35:  330Ω 5% 0805 (indicator LED, ~4mA - brighter)                     │
│    LED5-6:  Orange 0805, Vf~2.0V                                              │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

## 6.3 Relay Contact Wiring

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                          RELAY OUTPUT CONNECTIONS                               │
│                    (6.3mm Spade Terminal Connections)                          │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    Each relay has 3 contacts: COM (Common), NO (Normally Open), NC (Closed)   │
│    This design uses only COM and NO for each relay.                           │
│                                                                                 │
│    K1 - MAINS INDICATOR LAMP                                                   │
│    ─────────────────────────                                                   │
│         COM ──[6.3mm]──► To machine LED power (typically 12V or 24V)          │
│         NO  ──[6.3mm]──► To LED (+)                                           │
│         NC  ── Not connected                                                   │
│                                                                                 │
│    K2 - PUMP MOTOR (Use 16A relay!)                                           │
│    ────────────────────────────────                                           │
│         COM ──[6.3mm]──► From mains Live (via machine wiring)                 │
│         NO  ──[6.3mm]──► To pump motor                                        │
│         NC  ── Not connected                                                   │
│                                                                                 │
│    K3 - SOLENOID VALVE (3-Way)                                                │
│    ───────────────────────────                                                 │
│         COM ──[6.3mm]──► From mains Live (via machine wiring)                 │
│         NO  ──[6.3mm]──► To solenoid coil                                     │
│         NC  ── Not connected                                                   │
│                                                                                 │
│                                                                                 │
│    ⚠️  HIGH VOLTAGE WARNING:                                                  │
│    ─────────────────────────                                                   │
│    Relay contacts switch mains voltage (100-240V AC).                         │
│    Maintain 6mm minimum creepage between contacts and control circuits.       │
│    Use PCB slots if necessary to achieve required clearance.                  │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

---

# 7. Sensor Input Circuits

## 7.1 NTC Thermistor Interface

### Multi-Machine Compatibility (Jumper Selectable)

Different espresso machine brands use different NTC sensor values. **Solder jumpers JP2/JP3** allow switching between configurations **without soldering resistors**.

| Machine Brand     | NTC @ 25°C | JP2 (Brew) | JP3 (Steam) | Effective R1 | Effective R2 |
| ----------------- | ---------- | ---------- | ----------- | ------------ | ------------ |
| **ECM, Profitec** | 50kΩ       | **OPEN**   | **OPEN**    | 3.3kΩ        | 1.2kΩ        |
| Rocket, Rancilio  | 10kΩ       | **CLOSED** | **CLOSED**  | ~1kΩ         | ~430Ω        |
| Gaggia            | 10kΩ       | **CLOSED** | **CLOSED**  | ~1kΩ         | ~430Ω        |
| Lelit (50kΩ)      | 50kΩ       | OPEN       | OPEN        | 3.3kΩ        | 1.2kΩ        |

**Default:** JP2/JP3 **OPEN** = ECM/Profitec (50kΩ NTC)

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                         NTC THERMISTOR INPUT CIRCUITS                           │
│          ⚠️  EACH SENSOR OPTIMIZED FOR ITS TARGET TEMPERATURE RANGE            │
│          ⚠️  JP2/JP3 SOLDER JUMPERS SELECT NTC TYPE (NO SOLDERING RESISTORS)   │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    BREW BOILER (GPIO26/ADC0)              STEAM BOILER (GPIO27/ADC1)           │
│    Target: 90-96°C                        Target: 125-150°C                    │
│                                                                                 │
│         3.3V (Analog)                          3.3V (Analog)                   │
│             │                                      │                           │
│        ┌────┴────┐                            ┌────┴────┐                      │
│        │  3.3kΩ  │  R1 (always populated)     │  1.2kΩ  │  R2 (always populated)│
│        │  ±1%    │                            │  ±1%    │                      │
│        └────┬────┘                            └────┬────┘                      │
│             │                                      │                           │
│             ├────[1.5kΩ R1A]────[JP2]──┐          ├────[680Ω R2A]────[JP3]──┐ │
│             │                          │          │                          │ │
│             │  (JP2 adds R1A parallel) │          │  (JP3 adds R2A parallel) │ │
│             │                          │          │                          │ │
│             └──────────────────────────┘          └──────────────────────────┘ │
│             │                                      │                           │
│    NTC     ┌┴─────┬────────┐          NTC        ┌┴─────┬────────┐            │
│   10k/50k  │      │        │         10k/50k     │      │        │            │
│    @25°C ──┤   ┌──┴──┐  ┌──┴──┐       @25°C ─────┤   ┌──┴──┐  ┌──┴──┐         │
│            │   │ 1kΩ │  │100nF│                  │   │ 1kΩ │  │100nF│         │
│            │   │ R5  │  │     │                  │   │ R6  │  │     │         │
│            │   └──┬──┘  └──┬──┘                  │   └──┬──┘  └──┬──┘         │
│            │      ▼        │                     │      ▼        │            │
│            │   ADC0        │                     │   ADC1        │            │
│            │               │                     │               │            │
│            └───────────────┴─► GND               └───────────────┴─► GND      │
│                                                                                 │
│    JUMPER CONFIGURATION:                                                       │
│    ─────────────────────                                                       │
│                                                                                 │
│    JP2 (Brew):   ═══[OPEN]═══     JP3 (Steam):  ═══[OPEN]═══                  │
│                  Solder to close                 Solder to close               │
│                                                                                 │
│    ECM/Profitec (50kΩ NTC):  JP2 OPEN,  JP3 OPEN  → R1=3.3kΩ, R2=1.2kΩ       │
│    Rocket/Gaggia (10kΩ NTC): JP2 CLOSE, JP3 CLOSE → R1≈1kΩ,  R2≈430Ω         │
│                                                                                 │
│    MATH:                                                                       │
│    • JP2 CLOSED: 3.3kΩ || 1.5kΩ = (3.3×1.5)/(3.3+1.5) = 1.03kΩ ✓            │
│    • JP3 CLOSED: 1.2kΩ || 680Ω = (1.2×0.68)/(1.2+0.68) = 434Ω ✓             │
│                                                                                 │
│    ═══════════════════════════════════════════════════════════════════════    │
│                                                                                 │
│    WHY DIFFERENT PULL-UP RESISTORS?                                           │
│    ─────────────────────────────────                                          │
│    Maximum ADC sensitivity occurs when R_pullup ≈ R_NTC at target temp.       │
│    • Brew at 93°C: NTC ≈ 4.3kΩ → optimal pull-up ~3.3kΩ                      │
│    • Steam at 135°C: NTC ≈ 1.4kΩ → optimal pull-up ~1.2kΩ                    │
│                                                                                 │
│    Using the same resistor for both would compromise one or the other.        │
│                                                                                 │
│    ═══════════════════════════════════════════════════════════════════════    │
│                                                                                 │
│    BREW NTC with 3.3kΩ PULL-UP (R1) - Optimized for 90-100°C:                 │
│    ─────────────────────────────────────────────────────────                  │
│    │  Temp  │ R_NTC  │  Vout   │ ADC Count │ ADC % │ Δ/°C  │                 │
│    │──────────────────────────────────────────────────────────│                 │
│    │  25°C  │ 50.0kΩ │  3.10V  │   3843    │  94%  │   -   │  ← Room temp   │
│    │  85°C  │  5.5kΩ │  2.06V  │   2555    │  62%  │   -   │                 │
│    │  90°C  │  4.7kΩ │  1.94V  │   2408    │  59%  │  29   │  ← TARGET      │
│    │  93°C  │  4.3kΩ │  1.87V  │   2315    │  57%  │  31   │  ← TARGET      │
│    │  96°C  │  3.9kΩ │  1.79V  │   2217    │  54%  │  33   │  ← TARGET      │
│    │ 100°C  │  3.5kΩ │  1.70V  │   2104    │  51%  │  28   │                 │
│                                                                                 │
│    Resolution at brew temps (90-96°C): ~31 ADC counts/°C → 0.032°C           │
│    Voltage swing centered near 55% ADC = excellent usable range               │
│                                                                                 │
│    ═══════════════════════════════════════════════════════════════════════    │
│                                                                                 │
│    STEAM NTC with 1.2kΩ PULL-UP (R2) - Optimized for 125-150°C:               │
│    ─────────────────────────────────────────────────────────                  │
│    │  Temp  │ R_NTC  │  Vout   │ ADC Count │ ADC % │ Δ/°C  │                 │
│    │──────────────────────────────────────────────────────────│                 │
│    │  25°C  │ 50.0kΩ │  3.22V  │   3996    │  98%  │   -   │  ← Room temp   │
│    │ 125°C  │  1.8kΩ │  1.98V  │   2455    │  60%  │   -   │                 │
│    │ 130°C  │  1.6kΩ │  1.89V  │   2341    │  57%  │  23   │                 │
│    │ 135°C  │  1.4kΩ │  1.78V  │   2203    │  54%  │  28   │  ← TARGET      │
│    │ 140°C  │  1.25kΩ│  1.68V  │   2086    │  51%  │  23   │  ← TARGET      │
│    │ 145°C  │  1.1kΩ │  1.58V  │   1957    │  48%  │  26   │                 │
│    │ 150°C  │  1.0kΩ │  1.50V  │   1861    │  45%  │  19   │  ← MAX TEMP    │
│                                                                                 │
│    Resolution at steam temps (130-145°C): ~25 ADC counts/°C → 0.04°C         │
│                                                                                 │
│    ═══════════════════════════════════════════════════════════════════════    │
│                                                                                 │
│    COMPONENT VALUES:                                                           │
│    ─────────────────                                                           │
│    R1:   3.3kΩ ±1% 0805 (Brew pull-up, always populated)                     │
│    R1A:  1.5kΩ ±1% 0805 (Brew parallel, enabled by JP2)                      │
│    R2:   1.2kΩ ±1% 0805 (Steam pull-up, always populated)                    │
│    R2A:  680Ω ±1% 0805 (Steam parallel, enabled by JP3)                      │
│    R5:   1kΩ ±5% 0805 (ADC series protection, brew)                          │
│    R6:   1kΩ ±5% 0805 (ADC series protection, steam)                         │
│    JP2:  Solder jumper (OPEN = 50kΩ NTC, CLOSED = 10kΩ NTC)                  │
│    JP3:  Solder jumper (OPEN = 50kΩ NTC, CLOSED = 10kΩ NTC)                  │
│                                                                                 │
│    ⚠️ DEFAULT: JP2/JP3 OPEN (ECM/Profitec 50kΩ NTC)                          │
│    ⚠️ FOR ROCKET/GAGGIA: Close JP2 and JP3 with solder bridge               │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

## 7.2 Pressure Transducer Interface

**⚠️ SENSOR RESTRICTION:** Circuit designed for **0.5-4.5V ratiometric output ONLY**.

- ✅ 3-wire sensors (5V, GND, Signal) like YD4060 or automotive pressure sensors
- ✅ 0.5V offset allows broken wire detection (0.0V = disconnected, 0.5V = 0 bar)
- ❌ 4-20mA current loop sensors (require different circuit with shunt resistor)
- ❌ 0-5V sensors without offset (no broken wire detection)

**Recommended Thread:** G1/4" or 1/8" NPT (verify machine's hydraulic fitting)

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                     PRESSURE TRANSDUCER INPUT CIRCUIT                           │
│                       (0.5-4.5V Ratiometric Output)                            │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    Pressure Transducer                                                         │
│    (3-wire: Vcc, GND, Signal)                                                 │
│                                                                                 │
│         5V ─────────────────────────────────────┬────► Transducer Vcc         │
│                                                 │      (Red wire)              │
│                                                 │                              │
│        GND ─────────────────────────────────────┼────► Transducer GND         │
│                                                 │      (Black wire)            │
│                                                 │                              │
│    Transducer Signal ───────────────────────────┘      (Yellow/White wire)    │
│    (0.5V - 4.5V)                                                               │
│         │                                                                      │
│         │       Voltage Divider (5V → 3.3V range)                             │
│         │                                                                      │
│    ┌────┴────┐                                                                 │
│    │  5.6kΩ  │  ← R4: Series resistor, 1% precision                           │
│    │  ±1%    │    (5.6kΩ prevents ADC saturation with 3.0V reference)         │
│    └────┬────┘                                                                 │
│         │                                                                      │
│         ├───────────────────────────┬───────────────────────► GPIO28 (ADC2)   │
│         │                           │                                          │
│         │                      ┌────┴────┐                                    │
│         │                      │  D16    │  ← CLAMPING DIODE (BAT54S)         │
│         │                      │ Schottky│    Protects ADC if divider fails   │
│         │                      │ to 3.3V │    Cathode → 3.3V_Analog           │
│         │                      └────┬────┘                                    │
│         │                           │                                          │
│         │                       3.3V_A                                        │
│         │                                                                      │
│    ┌────┴────┐                 ┌────┴────┐                                    │
│    │  10kΩ   │                 │  100nF  │  ← RC filter                       │
│    │  ±1%    │  ← R3: To GND   │ Ceramic │    fc ≈ 100 Hz                     │
│    └────┬────┘                 └────┬────┘                                    │
│         │                           │                                          │
│        GND                         GND                                         │
│                                                                                 │
│    Voltage Divider Calculation (OPTIMIZED for 3.0V ADC Reference):            │
│    ───────────────────────────────────────────────────────────────            │
│    Ratio = R3 / (R3 + R4) = 10k / (10k + 5.6k) = 0.641                        │
│                                                                                 │
│    Input 0.5V → Output: 0.5V × 0.641 = 0.32V → ADC: 437                       │
│    Input 2.5V → Output: 2.5V × 0.641 = 1.60V → ADC: 2185 (midscale)          │
│    Input 4.5V → Output: 4.5V × 0.641 = 2.88V → ADC: 3940                      │
│                                                                                 │
│    ADC utilization: 85% of 3.0V reference (full 16 bar range linear)         │
│    Headroom: 120mV below 3.0V reference - NO SATURATION                       │
│                                                                                 │
│    ⚠️ WHY 5.6kΩ (not 4.7kΩ)?                                                 │
│    With 4.7kΩ: V_max = 4.5V × 0.68 = 3.06V > 3.0V reference → SATURATES!     │
│    With 5.6kΩ: V_max = 4.5V × 0.641 = 2.88V < 3.0V reference → LINEAR        │
│                                                                                 │
│    ⚠️ CLAMPING DIODE (D16 - BAT54S):                                          │
│    ─────────────────────────────────                                          │
│    CRITICAL PROTECTION: If R3 (10kΩ to GND) fails OPEN, full 5V from         │
│    transducer appears at GPIO28, exceeding RP2350's 3.6V maximum!             │
│    The Schottky diode clamps overvoltage to 3.3V + 0.3V = 3.6V (safe).       │
│                                                                                 │
│    Connector: 3-pin screw terminal (5V, GND, Signal)                          │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

### Ratiometric Pressure Compensation

**Purpose:** Eliminates pressure reading errors caused by 5V supply voltage drift.

Pressure transducers are **ratiometric** - their output is proportional to their supply voltage.
If the 5V rail from the HLK-15M05C varies by 5% under load, pressure readings drift by 5%.
In a 10-bar system, that's a **0.5 bar error** - significant for pressure profiling.

```
┌────────────────────────────────────────────────────────────────────────────────┐
│              5V SUPPLY MONITOR FOR RATIOMETRIC COMPENSATION                     │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    +5V Rail (from HLK-15M05C)                                                  │
│         │                                                                       │
│         ├────────────────────────────────► Pressure transducer Vcc             │
│         │                                                                       │
│    ┌────┴────┐                                                                  │
│    │  10kΩ   │  R100 (upper divider)                                           │
│    │   1%    │                                                                  │
│    └────┬────┘                                                                  │
│         │                                                                       │
│         ├──────────────────────────────────► ADC3/GPIO29 (5V_MONITOR)          │
│         │                                    (Pico internal - firmware config) │
│    ┌────┴────┐                          ┌────┴────┐                            │
│    │  5.6kΩ  │  R101 (lower divider)    │  100nF  │ C81 (filter)              │
│    │   1%    │                          │  25V    │                            │
│    └────┬────┘                          └────┬────┘                            │
│         │                                    │                                  │
│        GND                                  GND                                 │
│                                                                                 │
│    DIVIDER CALCULATION:                                                        │
│    Ratio = 5.6k / (10k + 5.6k) = 0.359 (SAME ratio as pressure divider)       │
│                                                                                 │
│    At 5.0V nominal: ADC reads 1.795V                                          │
│    At 5.5V (+10%):  ADC reads 1.974V (still within 3.0V reference)            │
│    At 4.5V (-10%):  ADC reads 1.615V                                          │
│                                                                                 │
│    FIRMWARE RATIOMETRIC CALCULATION:                                           │
│    ─────────────────────────────────                                           │
│    P_corrected = (ADC_pressure / ADC_5V_monitor) × k                          │
│                                                                                 │
│    This formula CANCELS OUT any 5V supply drift:                              │
│    - If 5V drops 5%, both ADC readings drop 5%                                │
│    - The ratio stays constant → pressure reading stays accurate               │
│                                                                                 │
│    Component Values:                                                           │
│    ─────────────────                                                           │
│    R100: 10kΩ 1%, 0805 (matches R3 in pressure divider)                       │
│    R101: 5.6kΩ 1%, 0805 (matches R4 in pressure divider)                      │
│    C81:  100nF 25V Ceramic, 0805 (filter, matches C11)                        │
│                                                                                 │
│    ⚠️ GPIO29/ADC3 ACCESS:                                                      │
│    GPIO29 is internal to the Pico 2 module (not on 40-pin header).            │
│    Firmware must configure GPIO29 for ADC input mode.                         │
│    Connection: Route from 5V_DIV pad near pressure divider to GPIO29 via      │
│    Pico socket pin (Pico internal connection handles the rest).               │
│                                                                                 │
│    ALTERNATIVE (if GPIO29 unavailable):                                        │
│    Sacrifice GPIO22 (SPARE on J15-8) and route 5V monitor there instead.      │
│    GPIO22 is not ADC-capable, so use external ADC or calibration table.       │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

### Selected Transducer: YD4060

| Parameter             | Specification                                          |
| --------------------- | ------------------------------------------------------ |
| Model                 | YD4060 Series                                          |
| **Pressure Range**    | **0-1.6 MPa (0-16 bar)** ⚠️ Order correct range!       |
| Output Signal         | 0.5-4.5V ratiometric                                   |
| Supply Voltage        | 5VDC                                                   |
| Operating Current     | ≤3mA                                                   |
| Accuracy              | ±1.0% FS                                               |
| Operating Temperature | -40°C to +105°C                                        |
| Response Time         | ≤3ms                                                   |
| Thread                | 1/8" NPT (use G1/8 or M6 adapter for espresso machine) |
| Housing               | 304 Stainless Steel, IP65                              |
| CE Certified          | Yes                                                    |

**Wiring (3-wire cable):**

| Wire Color   | Function          | Connect To |
| ------------ | ----------------- | ---------- |
| Red          | Vcc (+5V)         | J26 Pin 14 |
| Black        | GND               | J26 Pin 15 |
| Yellow/White | Signal (0.5-4.5V) | J26 Pin 16 |

**Pressure to Voltage/ADC Mapping (0-16 bar range, 5.6kΩ/10kΩ divider, 3.0V reference):**

| Pressure             | Voltage Out | After Divider | ADC Count |
| -------------------- | ----------- | ------------- | --------- |
| 0 bar                | 0.5V        | 0.32V         | 437       |
| 4 bar                | 1.5V        | 0.96V         | 1311      |
| 8 bar                | 2.5V        | 1.60V         | 2185      |
| 9 bar (typical brew) | 2.75V       | 1.76V         | 2404      |
| 12 bar               | 3.5V        | 2.24V         | 3059      |
| 16 bar               | 4.5V        | 2.88V         | 3940      |

**Resolution:** 16 bar / (3940 - 437) = **0.0046 bar per ADC count** (~0.07 PSI)

**Firmware Conversion:**

```python
def adc_to_pressure(adc_count, range_bar=16):
    """Convert ADC reading to pressure in bar"""
    ADC_MIN = 437   # 0 bar (0.5V input, after 0.641 divider, 3.0V ref)
    ADC_MAX = 3940  # 16 bar (4.5V input, after 0.641 divider, 3.0V ref)

    # Clamp to valid range
    adc_count = max(ADC_MIN, min(ADC_MAX, adc_count))

    # Linear interpolation
    pressure = (adc_count - ADC_MIN) / (ADC_MAX - ADC_MIN) * range_bar
    return pressure
```

⚠️ **Important:** When ordering, specify:

- Pressure range: **0-1.6 MPa (16 bar)** - NOT 0-60 MPa
- Output: **0.5-4.5V**
- Supply: **5VDC**
- Thread: **G1/8** or request M6 adapter

## 7.4 Digital Switch Inputs

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                         DIGITAL SWITCH INPUT CIRCUIT                            │
│              (Water Reservoir, Tank Level, Brew Handle Switches)               │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│                          3.3V                                                  │
│                           │                                                     │
│                      ┌────┴────┐                                               │
│                      │  10kΩ   │  ← External pull-up (or use Pico internal)   │
│                      │         │                                               │
│                      └────┬────┘                                               │
│                           │                                                     │
│    From Switch            ├─────────────────────────────────► GPIO (2,3,5)    │
│    (6.3mm spade)         │                                                     │
│         │                 │                                                     │
│         │            ┌────┴────┐                                               │
│    ┌────┴────┐       │  ESD    │  ← ESD protection                            │
│    │  Switch │       │ Clamp   │    PESD5V0S1BL or similar                    │
│    │   N.O.  │       │ Bi-dir  │                                               │
│    └────┬────┘       └────┬────┘                                               │
│         │                 │                                                     │
│         └─────────────────┴───────────────────────────────────► GND           │
│                                                                                 │
│    Logic:                                                                      │
│    ───────                                                                     │
│    Switch OPEN   → GPIO reads HIGH (3.3V) → Switch inactive                   │
│    Switch CLOSED → GPIO reads LOW  (0V)   → Switch active                     │
│                                                                                 │
│    Debouncing: Implement in software (50-100ms debounce time)                 │
│                                                                                 │
│    Optional hardware debounce (add if EMI is problematic):                    │
│    ───────────────────────────────────────────────────────                    │
│         │                                                                      │
│         ├───[10kΩ]───┬───► GPIO                                               │
│         │            │                                                         │
│    ┌────┴────┐  ┌────┴────┐                                                   │
│    │ Switch  │  │  100nF  │  ← RC time constant ~1ms                          │
│    └────┬────┘  └────┬────┘                                                   │
│         │            │                                                         │
│        GND          GND                                                        │
│                                                                                 │
│    Connector: 6.3mm spade terminal (2 per switch: Signal, GND)               │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

## 7.5 Steam Boiler Level Probe (OPA342 + TLV3201 AC Sensing)

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                       STEAM BOILER LEVEL PROBE CIRCUIT                          │
│                    (AC Sensing with OPA342 + TLV3201)                          │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    Design Note: AC excitation prevents electrolysis and probe                │
│    corrosion for long probe life.                                              │
│                                                                                 │
│    This design uses commonly available, modern components:                     │
│    • OPA342 (or OPA207): Rail-to-rail op-amp for AC oscillator generation     │
│    • TLV3201: Precision comparator for level detection with hysteresis        │
│    • All components readily available from major distributors                  │
│                                                                                 │
│    ═══════════════════════════════════════════════════════════════════════    │
│                                                                                 │
│                         +3.3V                     +3.3V                        │
│                           │                         │                          │
│                      ┌────┴────┐               ┌────┴────┐                     │
│                      │  100nF  │ C60           │  100nF  │ C63                 │
│                      └────┬────┘               └────┬────┘                     │
│                           │                         │                          │
│    ┌──────────────────────┴─────────────────────────┴───────────────────────┐  │
│    │                                                                         │  │
│    │    STAGE 1: WIEN BRIDGE OSCILLATOR (~1kHz)                             │  │
│    │    ───────────────────────────────────────                              │  │
│    │                                                                         │  │
│    │                      +3.3V                                              │  │
│    │                        │                                                │  │
│    │                   ┌────┴────┐                                           │  │
│    │              VCC──┤         │                                           │  │
│    │                   │ OPA342  │ U6                                        │  │
│    │    ┌────[R91]─────┤-  (A)   ├──┬────────────────────► AC_OUT            │  │
│    │    │    10kΩ      │         │  │                      (to probe)        │  │
│    │    │         ┌────┤+        │  │                                        │  │
│    │    │         │    │   GND   │  │                                        │  │
│    │    │         │    └────┬────┘  │                                        │  │
│    │    │         │         │       │                                        │  │
│    │    │         │        ─┴─      │                                        │  │
│    │    │         │        GND      │                                        │  │
│    │    │         │                 │                                        │  │
│    │    │    ┌────┴────┐       ┌────┴────┐                                   │  │
│    │    │    │   10kΩ  │       │   10kΩ  │                                   │  │
│    │    │    │   R83   │       │   R84   │                                   │  │
│    │    │    └────┬────┘       └────┬────┘                                   │  │
│    │    │         │                 │                                        │  │
│    │    │         ├─────────────────┤                                        │  │
│    │    │         │                 │                                        │  │
│    │    │    ┌────┴────┐       ┌────┴────┐                                   │  │
│    │    │    │   10nF  │       │   10nF  │                                   │  │
│    │    │    │   C61   │       │   C62   │                                   │  │
│    │    │    └────┬────┘       └────┬────┘                                   │  │
│    │    │         │                 │                                        │  │
│    │    │        ─┴─               ─┴─                                       │  │
│    │    │        GND               GND                                       │  │
│    │    │                                                                    │  │
│    │    └───────────────────────────────────────────────────────────────┐    │  │
│    │                                                                     │    │  │
│    │    Oscillator frequency: f = 1/(2π × R × C) ≈ 1.6 kHz              │    │  │
│    │    (with 10kΩ and 10nF - optimized for probe longevity)            │    │  │
│    │                                                                     │    │  │
│    │    ⚠️ WHY ~1.6 kHz (NOT 160 Hz)?                                   │    │  │
│    │    ─────────────────────────────                                    │    │  │
│    │    Lower frequencies (< 500 Hz) allow electrochemical reactions    │    │  │
│    │    (electrolysis) during each AC half-cycle, corroding the probe.  │    │  │
│    │    Industry standard for conductivity sensors: 1-10 kHz.           │    │  │
│    │    At 1.6 kHz, probe life extends from months to 5-10+ years.      │    │  │
│    │                                                                     │    │  │
│    └─────────────────────────────────────────────────────────────────────┘    │  │
│                                                                                 │
│    ┌─────────────────────────────────────────────────────────────────────────┐│
│    │                                                                          ││
│    │    STAGE 2: PROBE & SIGNAL CONDITIONING                                 ││
│    │    ─────────────────────────────────────────                            ││
│    │                                                                          ││
│    │    AC_OUT ───[100Ω R85]───┬────────────────► J26 Pin 5 (Level Probe)    ││
│    │           (current limit) │                  Screw terminal             ││
│    │                           │                       │                     ││
│    │                      ┌────┴────┐             ┌────┴────┐                ││
│    │                      │  1µF    │             │  Probe  │                ││
│    │                      │  C64    │             │   ~~~   │                ││
│    │                      │ (AC     │             │  Water  │                ││
│    │                      │coupling)│             │  Level  │                ││
│    │                      └────┬────┘             └────┬────┘                ││
│    │                           │                       │                     ││
│    │                           │                  Boiler Body                ││
│    │                           │                  (Ground via PE)            ││
│    │                           │                      ─┴─                    ││
│    │                           ▼                      GND                    ││
│    │                       AC_SENSE                                          ││
│    │                           │                                             ││
│    │    When water present: AC signal coupled through water conductivity    ││
│    │    When dry: No AC signal returned (open circuit)                      ││
│    │                                                                          ││
│    └──────────────────────────────────────────────────────────────────────────┘│
│                                                                                 │
│    ┌─────────────────────────────────────────────────────────────────────────┐│
│    │                                                                          ││
│    │    STAGE 3: RECTIFIER & COMPARATOR                                      ││
│    │    ───────────────────────────────                                      ││
│    │                                                                          ││
│    │    AC_SENSE ────────────────┬─────────────────────┐                     ││
│    │                             │                     │                     ││
│    │                        ┌────┴────┐           ┌────┴────┐                ││
│    │                        │   10kΩ  │           │  100nF  │                ││
│    │                        │   R95   │           │   C65   │                ││
│    │                        │ (Bias)  │           │(Filter) │                ││
│    │                        └────┬────┘           └────┬────┘                ││
│    │                             │                     │                     ││
│    │                             ├─────────────────────┤                     ││
│    │                             │                     │                     ││
│    │                             │                    ─┴─                    ││
│    │                             │                    GND                    ││
│    │                             │                                           ││
│    │                             ▼                                           ││
│    │                         DC_LEVEL                                        ││
│    │                             │                                           ││
│    │    ┌────────────────────────┴───────────────────────────────────────┐  ││
│    │    │                                                                 │  ││
│    │    │                          +3.3V                                  │  ││
│    │    │                            │                                    │  ││
│    │    │                       ┌────┴────┐                               │  ││
│    │    │                  VCC──┤         │                               │  ││
│    │    │                       │TLV3201  │ U7                            │  ││
│    │    │    DC_LEVEL ──────────┤+        ├──────────────────► GPIO4      │  ││
│    │    │                       │   (Cmp) │                   (Digital)   │  ││
│    │    │    VREF ──────────────┤-        │                               │  ││
│    │    │    (from divider)     │   GND   │                               │  ││
│    │    │                       └────┬────┘                               │  ││
│    │    │                            │                                    │  ││
│    │    │                           ─┴─                                   │  ││
│    │    │                           GND                                   │  ││
│    │    │                                                                 │  ││
│    │    │    REFERENCE VOLTAGE DIVIDER (sets threshold):                  │  ││
│    │    │                                                                 │  ││
│    │    │    +3.3V ────[100kΩ R96]───┬───[100kΩ R97]──── GND              │  ││
│    │    │                            │                                    │  ││
│    │    │                            └───► VREF (~1.65V)                  │  ││
│    │    │                                                                 │  ││
│    │    │    HYSTERESIS (via positive feedback):                          │  ││
│    │    │    GPIO4 ────[1MΩ R98]────► + input of TLV3201                  │  ││
│    │    │    (provides ~50mV hysteresis for clean switching)              │  ││
│    │    │                                                                 │  ││
│    │    └─────────────────────────────────────────────────────────────────┘  ││
│    │                                                                          ││
│    └──────────────────────────────────────────────────────────────────────────┘│
│                                                                                 │
│    ═══════════════════════════════════════════════════════════════════════    │
│                                                                                 │
│    OPERATION:                                                                  │
│    ───────────                                                                 │
│    1. OPA342 generates ~1.6kHz AC oscillation (Wien bridge)                   │
│    2. AC signal excites probe through C64 (DC blocking capacitor)            │
│    3. If water present: AC signal conducts through water to boiler (GND)     │
│    4. Return AC signal is rectified/filtered, raising DC_LEVEL               │
│    5. TLV3201 compares DC_LEVEL to VREF threshold                            │
│    6. Output goes LOW when water detected (DC_LEVEL > VREF)                  │
│    7. Hysteresis resistor R98 prevents output chatter                        │
│                                                                                 │
│    Logic:                                                                      │
│    ───────                                                                     │
│    Water below probe → GPIO4 reads HIGH → Refill needed (UNSAFE for heater)  │
│    Water at/above probe → GPIO4 reads LOW → Level OK                         │
│                                                                                 │
│    Benefits of OPA342 + TLV3201 Design:                                       │
│    ─────────────────────────────────────                                      │
│    • Uses commonly available, modern components                              │
│    • AC excitation prevents electrolysis and probe corrosion                 │
│    • All logic on PCB - no external modules                                  │
│    • Low power consumption (<1mA total)                                      │
│    • Adjustable threshold via R96/R97 divider                                │
│    • Clean hysteresis via R98                                                │
│    • Works with any conductive liquid                                        │
│                                                                                │
│    ⚠️  OSCILLATOR WAVEFORM NOTE (For Testing):                               │
│    ─────────────────────────────────────────────                              │
│    A classic Wien Bridge requires AGC (non-linear feedback) to produce a     │
│    clean sine wave. Without AGC, the op-amp saturates to the supply rails,   │
│    producing a clipped sine or square wave. THIS IS ACCEPTABLE for water     │
│    conductivity sensing - we only need AC excitation, not a pure sine.       │
│                                                                                │
│    During testing: If the oscillator outputs a ~160Hz rail-to-rail square    │
│    wave, the circuit is working correctly. Do NOT attempt to tune for a      │
│    perfect sine wave - it is unnecessary and may destabilize the circuit.    │
│                                                                                 │
│    Component Values:                                                          │
│    ──────────────────                                                         │
│    U6:  OPA342UA (SOIC-8) or OPA2342UA (dual, use one section)              │
│         Alt: OPA207 (lower noise, higher precision)                          │
│    U7:  TLV3201AIDBVR (SOT-23-5) - rail-to-rail comparator                  │
│    R81: 10kΩ 1% 0805 (oscillator feedback)                                   │
│    R82: 4.7kΩ 1% 0805 (Wien bridge gain)                                     │
│    R83: 10kΩ 1% 0805 (Wien bridge)                                           │
│    R84: 10kΩ 1% 0805 (Wien bridge)                                           │
│    R85: 100Ω 5% 0805 (probe current limit)                                   │
│    R86: 10kΩ 5% 0805 (AC bias)                                               │
│    R87: 100kΩ 1% 0805 (reference divider upper)                              │
│    R88: 100kΩ 1% 0805 (reference divider lower)                              │
│    R89: 1MΩ 5% 0805 (comparator hysteresis)                                  │
│    R97: 100kΩ 1% 0805 (reference divider)                                    │
│    R98: 1MΩ 5% 0805 (hysteresis)                                             │
│    C60: 100nF 25V ceramic 0805 (OPA342 VCC decoupling)                       │
│    C61: 10nF 50V ceramic 0805 (Wien bridge timing - 1.6kHz for probe life)   │
│    C62: 10nF 50V ceramic 0805 (Wien bridge timing - 1.6kHz for probe life)   │
│    C63: 100nF 25V ceramic 0805 (TLV3201 VCC decoupling)                      │
│    C64: 1µF 25V ceramic 0805 (AC coupling to probe)                          │
│    C65: 100nF 25V ceramic 0805 (sense filter)                                │
│                                                                                 │
│    Threshold Adjustment:                                                      │
│    ─────────────────────                                                      │
│    • Adjust R96/R97 ratio to change detection threshold                      │
│    • For more sensitive detection: increase R97 (lower VREF)                 │
│    • For less sensitive detection: decrease R97 (higher VREF)                │
│    • Typical tap water works well with 1:1 ratio (VREF = 1.65V)              │
│                                                                                 │
│    Firmware Implementation:                                                   │
│    ─────────────────────────                                                  │
│    • Configure GPIO4 as digital input (no PWM needed!)                       │
│    • Read GPIO4 state directly                                               │
│    • Implement simple debouncing (3-5 consecutive readings)                  │
│                                                                                 │
│    Safety Interlock:                                                          │
│    GPIO4 HIGH indicates low water. Firmware should disable steam heater       │
│    to prevent dry-fire condition.                                             │
│                                                                                 │
│    Connector: 6.3mm spade terminal (single wire - probe only)                │
│    Ground return is through boiler body via machine chassis (PE)              │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

---

# 8. Communication Interfaces

## 8.1 ESP32 Display Module Interface (UART0)

**8-pin JST-XH connector** for external ESP32-based display module.

- ESP32 has full control over Pico: RUN (reset) and BOOTSEL (bootloader entry) for OTA
- **NEW: Brew-by-Weight support** via WEIGHT_STOP signal (Pin 7)
- **SPARE pin** (Pin 8) reserved for future expansion

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                       ESP32 DISPLAY MODULE INTERFACE                            │
│                         (J15 - JST-XH 8-Pin 2.54mm)                            │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    Pin Assignment:                                                             │
│    ───────────────                                                             │
│    ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐                          │
│    │  1  │  2  │  3  │  4  │  5  │  6  │  7  │  8  │                          │
│    │ 5V  │ GND │ TX  │ RX  │ RUN │BOOT │WGHT │SPARE│                          │
│    └──┬──┴──┬──┴──┬──┴──┬──┴──┬──┴──┬──┴──┬──┴──┬──┘                          │
│       │     │     │     │     │     │     │     │                              │
│       │     │     │     │     │     │     │     └──► GPIO22 (Future use)      │
│       │     │     │     │     │     │     └────────► GPIO21 (WEIGHT_STOP)     │
│       │     │     │     │     │     └──────────────► Pico BOOTSEL (bootloader)│
│       │     │     │     │     └────────────────────► Pico RUN pin (reset)     │
│       │     │     │     └──────────────────────────► GPIO1 (UART0 RX←ESP TX)  │
│       │     │     └────────────────────────────────► GPIO0 (UART0 TX→ESP RX)  │
│       │     └──────────────────────────────────────► Ground                   │
│       └────────────────────────────────────────────► 5V Power to ESP32        │
│                                                                                 │
│    ════════════════════════════════════════════════════════════════════════   │
│    ✅ ESP32 CONTROLS PICO FOR OTA FIRMWARE UPDATES                            │
│    ════════════════════════════════════════════════════════════════════════   │
│    • ESP32 updates ITSELF via WiFi OTA (standard ESP-IDF)                     │
│    • ESP32 updates PICO via UART + RUN/BOOTSEL control                        │
│    • Pico has no WiFi - relies on ESP32 as update gateway                     │
│    ════════════════════════════════════════════════════════════════════════   │
│                                                                                 │
│    UART Communication Circuit:                                                 │
│    ───────────────────────────                                                 │
│                                                                                 │
│    GPIO0 (TX) ────[33Ω]────┬────────────────────────► J15 Pin 3 (TX→ESP RX)  │
│                            │                                                   │
│                       ┌────┴────┐                                             │
│                       │  100pF  │  ← Optional HF filter                       │
│                       └────┬────┘                                             │
│                           GND                                                  │
│                                                                                 │
│    GPIO1 (RX) ◄───[33Ω]────┬────────────────────────── J15 Pin 4 (RX←ESP TX) │
│                            │                                                   │
│                       ┌────┴────┐                                             │
│                       │  100pF  │                                             │
│                       └────┬────┘                                             │
│                           GND                                                  │
│                                                                                 │
│    ────────────────────────────────────────────────────────────────────────   │
│    ESP32 → PICO RUN (Pin 5): ESP32 resets Pico                                │
│    ────────────────────────────────────────────────────────────────────────   │
│                                                                                 │
│                          3.3V                                                  │
│                           │                                                     │
│                      ┌────┴────┐                                               │
│                      │  10kΩ   │  ← Pull-up (Pico has internal, but add ext.) │
│                      └────┬────┘                                               │
│                           │                                                     │
│    Pico RUN pin ◄─────────┴────────────────────────── J15 Pin 5 (RUN)         │
│                                                                                 │
│    ESP32 GPIO: Open-drain output. Pull LOW to reset, release to run.         │
│                                                                                 │
│    ────────────────────────────────────────────────────────────────────────   │
│    ESP32 → PICO BOOTSEL (Pin 6): ESP32 controls bootloader entry              │
│    ────────────────────────────────────────────────────────────────────────   │
│                                                                                 │
│                          3.3V                                                  │
│                           │                                                     │
│                      ┌────┴────┐                                               │
│                      │  10kΩ   │  ← Pull-up (BOOTSEL normally high)           │
│                      └────┬────┘                                               │
│                           │                                                     │
│    Pico BOOTSEL ◄─────────┴────────────────────────── J15 Pin 6 (BOOT)        │
│                                                                                 │
│    ESP32 GPIO: Open-drain output. Hold LOW during reset to enter bootloader. │
│                                                                                 │
│    ════════════════════════════════════════════════════════════════════════   │
│    ESP32 OTA UPDATE SEQUENCE (Pico firmware update via WiFi):                 │
│    ════════════════════════════════════════════════════════════════════════   │
│                                                                                 │
│    METHOD 1: Serial Bootloader (✅ IMPLEMENTED)                                │
│    ──────────────────────────────────────────                                  │
│    1. ESP32 downloads new Pico firmware via WiFi                              │
│    2. ESP32 sends MSG_CMD_BOOTLOADER (0x1F) command via UART                  │
│    3. Pico serial bootloader acknowledges, enters bootloader mode             │
│    4. ESP32 streams firmware binary via UART (921600 baud)                    │
│    5. Pico bootloader receives chunks, verifies checksums, writes to flash     │
│    6. Pico resets automatically after successful update                       │
│    7. Pico boots with new firmware                                            │
│                                                                                 │
│    METHOD 2: Hardware Bootloader Entry (recovery)                             │
│    ───────────────────────────────────────────────                             │
│    1. ESP32 holds BOOTSEL (Pin 6) LOW                                         │
│    2. ESP32 pulses RUN (Pin 5) LOW then releases                              │
│    3. Pico enters USB bootloader mode (BOOTSEL held during reset)             │
│    4. ESP32 releases BOOTSEL                                                  │
│    5. Pico appears as USB mass storage OR custom UART bootloader activates    │
│    6. ESP32 streams firmware via UART                                         │
│                                                                                 │
│    RECOVERY (if Pico firmware is completely corrupted):                       │
│    ─────────────────────────────────────────────────────                       │
│    • Method 1 (serial bootloader) is fully implemented and preferred          │
│    • Method 2 (hardware bootloader entry) available as fallback/recovery      │
│    • OR use J16 service port with USB-UART adapter + BOOTSEL button          │
│                                                                                 │
│    ════════════════════════════════════════════════════════════════════════   │
│    BREW-BY-WEIGHT SIGNAL (Pin 7 - WEIGHT_STOP → GPIO21)                       │
│    ════════════════════════════════════════════════════════════════════════   │
│                                                                                 │
│    Purpose: Allows ESP32 (connected to Bluetooth scale) to signal Pico        │
│             to stop the brew when target weight is reached.                    │
│                                                                                 │
│                          3.3V                                                  │
│                           │                                                     │
│                      ┌────┴────┐                                               │
│                      │  10kΩ   │  ← Pull-down (normally LOW)                   │
│                      └────┬────┘                                               │
│                           │                                                     │
│    GPIO21 ◄───────────────┴────────────────────────── J15 Pin 7 (WEIGHT_STOP) │
│                                                                                 │
│    Logic:                                                                       │
│    ───────                                                                      │
│    • ESP32 monitors Bluetooth scale (e.g., Acaia, Decent, Felicita)           │
│    • When target weight reached → ESP32 sets Pin 7 HIGH                        │
│    • Pico reads GPIO21 HIGH → Immediately stops pump (K2)                     │
│    • ESP32 releases Pin 7 → Returns LOW (ready for next brew)                 │
│                                                                                 │
│    Timing: ESP32 should hold HIGH for at least 100ms for reliable detection.  │
│    Alternative: Pico can also poll scale weight via UART and make decision.   │
│                                                                                 │
│    ────────────────────────────────────────────────────────────────────────   │
│    SPARE PIN (Pin 8 - GPIO22)                                                  │
│    ────────────────────────────────────────────────────────────────────────   │
│                                                                                 │
│    GPIO22 ◄───────────────────────────────────────── J15 Pin 8 (SPARE)        │
│                                                                                 │
│    Reserved for future expansion. No pull-up/down by default.                 │
│    Suggested uses: Additional sensor input, flow sensor, etc.                 │
│                                                                                 │
│    ════════════════════════════════════════════════════════════════════════   │
│                                                                                 │
│    Power Notes:                                                                │
│    ────────────                                                                │
│    ESP32 + display draws 300-500mA. Power from 5V (Pin 1).                    │
│    ESP32 modules have onboard 3.3V LDO. Do not power from Pico's 3.3V.       │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

## 8.2 Service/Debug Port (UART0 - Shared with ESP32)

4-pin header for development, debugging, and emergency firmware flashing.
**Shares GPIO0/1 with ESP32 display connector (J15).**

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                           SERVICE/DEBUG PORT                                    │
│                (J16 - 2.54mm 4-Pin Header, SHARED with ESP32)                  │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    ⚠️  IMPORTANT: J16 shares GPIO0/1 with J15 (ESP32 Display)                 │
│    ─────────────────────────────────────────────────────────                   │
│    • DISCONNECT ESP32 CABLE (J15) before using service port                   │
│    • Both connectors cannot be used simultaneously                            │
│                                                                                 │
│    Pin Assignment:                                                             │
│    ───────────────                                                             │
│    ┌─────┬─────┬─────┬─────┐                                                  │
│    │  1  │  2  │  3  │  4  │                                                  │
│    │ 3V3 │ GND │ TX  │ RX  │                                                  │
│    └──┬──┴──┬──┴──┬──┴──┬──┘                                                  │
│       │     │     │     │                                                      │
│       │     │     │     └──────────────► GPIO1 (UART0 RX ← External TX)       │
│       │     │     └────────────────────► GPIO0 (UART0 TX → External RX)       │
│       │     └──────────────────────────► Ground                               │
│       └────────────────────────────────► 3.3V (reference only, <100mA)        │
│                                                                                 │
│    Shared UART0 Wiring:                                                        │
│    ────────────────────                                                        │
│                          ┌───────────────► J15 Pin (ESP32 RX)                 │
│    GPIO0 (TX) ──[33Ω]───┤                                                     │
│                          └───────────────► J16 Pin 3 (Service TX)             │
│                                                                                 │
│                          ┌───────────────► J15 Pin (ESP32 TX)                 │
│    GPIO1 (RX) ◄─[33Ω]───┤                                                     │
│                          └───────────────► J16 Pin 4 (Service RX)             │
│                                                                                 │
│    Use Cases:                                                                  │
│    ───────────                                                                 │
│    1. Emergency firmware flashing (UART bootloader on GPIO0/1)                │
│    2. Debug console when ESP32 disconnected                                   │
│    3. Production testing and calibration                                      │
│    4. Recovery if USB port is damaged                                         │
│                                                                                 │
│    ESP32 OTA Firmware Updates:                                                 │
│    ───────────────────────────                                                 │
│    The ESP32 display can update Pico firmware over-the-air:                   │
│    1. ESP32 downloads new firmware via WiFi                                   │
│    2. ESP32 puts Pico into bootloader mode (via RUN pin or BOOTSEL)          │
│    3. ESP32 sends firmware to Pico via UART0 (GPIO0/1)                        │
│    4. Pico reboots with new firmware                                          │
│    This enables remote updates without physical access!                        │
│                                                                                 │
│    Default Configuration: 921600 baud, 8N1 (normal communication)            │
│    USB Serial Debug: 115200 baud, 8N1                                        │
│                                                                                 │
│    Silkscreen Labels:                                                          │
│    ──────────────────                                                          │
│    J16: "SERVICE" with pin labels "3V3 GND TX RX"                             │
│    Warning: "⚠️ DISCONNECT DISPLAY BEFORE USE"                                │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

## 8.3 I2C Accessory Port

4-pin header for I2C accessories (sensors, displays, expansion modules).

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                           I2C ACCESSORY PORT                                    │
│                         (J23 - 2.54mm 4-Pin Header)                            │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    Pin Assignment:                                                             │
│    ───────────────                                                             │
│    ┌─────┬─────┬─────┬─────┐                                                  │
│    │  1  │  2  │  3  │  4  │                                                  │
│    │ 3V3 │ GND │ SDA │ SCL │                                                  │
│    └──┬──┴──┬──┴──┬──┴──┬──┘                                                  │
│       │     │     │     │                                                      │
│       │     │     │     └──────────────► GPIO9 (I2C0 SCL)                     │
│       │     │     └────────────────────► GPIO8 (I2C0 SDA)                     │
│       │     └──────────────────────────► Ground                               │
│       └────────────────────────────────► 3.3V (max 100mA for accessories)     │
│                                                                                 │
│    Circuit:                                                                    │
│    ─────────                                                                   │
│                     3.3V                                                       │
│                      │                                                          │
│               ┌──────┼──────┐                                                  │
│               │      │      │                                                  │
│            [4.7kΩ] [4.7kΩ]  │                                                  │
│               │      │      │                                                  │
│    GPIO8 ─────┴──────│──────┼───────────► J23 Pin 3 (SDA)                     │
│    GPIO9 ────────────┴──────┼───────────► J23 Pin 4 (SCL)                     │
│                             │                                                  │
│                            GND                                                 │
│                                                                                 │
│    Compatible Accessories:                                                     │
│    ───────────────────────                                                     │
│    • OLED displays (SSD1306, SH1106)                                          │
│    • Additional temperature sensors (TMP117, MCP9808)                         │
│    • Pressure sensors (BMP280, BME280)                                        │
│    • Real-time clock (DS3231)                                                 │
│    • EEPROM for settings backup (24LC256)                                     │
│    • I2C GPIO expanders (PCF8574, MCP23017)                                   │
│                                                                                 │
│    I2C Configuration:                                                          │
│    ──────────────────                                                          │
│    • Bus: I2C0                                                                 │
│    • Default speed: 100kHz (standard mode)                                    │
│    • Max speed: 400kHz (fast mode)                                            │
│    • Pull-ups: 4.7kΩ on-board                                                 │
│                                                                                 │
│    Silkscreen Label: "I2C" with pin labels "3V3 GND SDA SCL"                  │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

---

# 9. User Interface Components

## 9.1 Reset and Boot Buttons

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                        SMD TACTILE BUTTONS                                      │
│                      (Reset and Bootloader Entry)                              │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    RESET BUTTON (SW1)                                                          │
│    ──────────────────                                                          │
│                                                                                 │
│                          3.3V                                                  │
│                           │                                                     │
│                      ┌────┴────┐                                               │
│                      │  10kΩ   │  ← Pull-up (Pico has internal on RUN)        │
│                      └────┬────┘                                               │
│                           │                                                     │
│                           ├────────────────────────► Pico RUN Pin             │
│                           │                                                     │
│                      ┌────┴────┐                                               │
│                      │   SW1   │  ← SMD Tactile 6x6mm                         │
│                      │ (RESET) │    Panasonic EVQP7A01P or similar            │
│                      └────┬────┘                                               │
│                           │                                                     │
│                          GND                                                   │
│                                                                                 │
│    Operation: Press to reset Pico. Release to run.                            │
│                                                                                 │
│    ─────────────────────────────────────────────────────────────────────────  │
│                                                                                 │
│    BOOT BUTTON (SW2)                                                           │
│    ─────────────────                                                           │
│                                                                                 │
│    Connect in parallel with Pico's onboard BOOTSEL button.                    │
│    Access via TP6 test pad on Pico module, or solder to BOOTSEL net.         │
│                                                                                 │
│                      ┌─────────┐                                               │
│                      │   SW2   │  ← SMD Tactile 6x6mm                         │
│                      │ (BOOT)  │                                               │
│                      └────┬────┘                                               │
│                           │                                                     │
│            BOOTSEL ───────┴────────────────────────► GND                      │
│            (From Pico TP6)                                                     │
│                                                                                 │
│    Operation:                                                                  │
│    1. Hold BOOT button                                                        │
│    2. Press and release RESET button                                          │
│    3. Release BOOT button                                                     │
│    4. Pico appears as USB mass storage device                                 │
│    5. Drag-drop UF2 firmware file to flash                                    │
│                                                                                 │
│    Button: Panasonic EVQP7A01P (6x6x4.3mm, 2.55N force)                       │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

## 9.2 Status LED

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                            STATUS LED (Green)                                   │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    GPIO15 ────────[330Ω]────────┬────────────────────────────────────────────│
│                                 │                                              │
│                            ┌────┴────┐                                         │
│                            │   LED   │  ← Green 0805 SMD                      │
│                            │  D1     │    Vf=2.0V, If=10mA                    │
│                            └────┬────┘                                         │
│                                 │                                              │
│                                GND                                             │
│                                                                                 │
│    Current: (3.3V - 2.0V) / 330Ω ≈ 3.9mA (clearly visible)                   │
│                                                                                 │
│    NOTE: Green LED chosen over blue because:                                  │
│    - Blue LEDs have Vf=3.0-3.4V, leaving only 0-0.3V margin with 3.3V supply │
│    - Green LEDs have Vf=1.8-2.2V, providing reliable ~4mA current            │
│    - Green also matches relay indicator LEDs for consistency                  │
│                                                                                 │
│    Firmware States (Example):                                                  │
│    ──────────────────────────                                                  │
│    • Solid ON: System ready, at temperature                                   │
│    • Slow blink (1Hz): Heating up                                             │
│    • Fast blink (4Hz): Error condition                                        │
│    • Double blink: Communication active                                        │
│    • Off: Standby/sleep mode                                                  │
│                                                                                 │
│    Placement: Near board edge, visible when installed                         │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

## 9.3 Buzzer

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                          BUZZER (Passive Piezo)                                 │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    Using PASSIVE buzzer for variable tones via PWM:                           │
│                                                                                 │
│                       ┌───────────────┐                                        │
│    GPIO19 ───[100Ω]───┤    Passive    │                                        │
│      (PWM)            │    Buzzer     │  ← CUI CEM-1203(42) or similar        │
│                       │    (BZ1)      │    12mm, 3-5V, 85dB @ 10cm            │
│                       └───────┬───────┘                                        │
│                               │                                                │
│                              GND                                               │
│                                                                                 │
│    PWM Frequencies for Alerts:                                                 │
│    ───────────────────────────                                                 │
│    • 2000 Hz: Ready beep (short)                                              │
│    • 2500 Hz: Temperature reached                                              │
│    • 1500 Hz: Shot complete                                                    │
│    • 1000 Hz: Warning (low water)                                             │
│    • 500 Hz: Error                                                             │
│    • Melody: Startup jingle                                                   │
│                                                                                 │
│    Buzzer: CUI CEM-1203(42), 12mm passive piezo, 3-5V, 85dB @ 10cm           │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

---

# 10. Power Metering Circuit

## 10.1 Universal External Power Meter Interface

**⚠️ KEY DESIGN: NO HV MEASUREMENT CIRCUITRY ON PCB - EXTERNAL MODULES HANDLE METERING**

The control PCB provides a universal interface for connecting external power metering modules. The external module handles its own high-voltage connections (mains L/N and CT clamp), while the control PCB provides only low-voltage communication and power.

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                    UNIVERSAL POWER METER INTERFACE                              │
│                    (External Module - OFF-BOARD Design)                         │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    DESIGN PHILOSOPHY:                                                          │
│    ──────────────────                                                          │
│    • HIGH VOLTAGE stays entirely on external metering module                   │
│    • Control PCB provides ONLY low-voltage communication interface             │
│    • Supports TTL UART (direct) or RS485 (differential) via single connector  │
│    • User wires mains (L/N) directly to external module's screw terminals     │
│    • CT clamp connects directly to external module (not routed through PCB)   │
│                                                                                 │
│    ═══════════════════════════════════════════════════════════════════════    │
│                                                                                 │
│    WIRING TOPOLOGY:                                                            │
│    ────────────────                                                            │
│                                                                                 │
│    ┌──────────────────────┐          ┌──────────────────────────────────────┐ │
│    │    CONTROL PCB       │          │    EXTERNAL POWER METER              │ │
│    │                      │          │    (PZEM, JSY, Eastron, etc.)        │ │
│    │   ┌───────────┐      │          │                                      │ │
│    │   │  J17      │      │          │   ┌─────────────┐  ┌─────────────┐   │ │
│    │   │  JST-XH   │      │  4-wire  │   │   LV UART   │  │   HV MAINS  │   │ │
│    │   │  6-pin    │──────┼──────────┼───│   (JST)     │  │   (Screw)   │   │ │
│    │   │           │      │          │   │ 5V,GND,TX,RX│  │   L, N, CT  │   │ │
│    │   └───────────┘      │          │   └─────────────┘  └──────┬──────┘   │ │
│    │                      │          │                           │          │ │
│    │   GPIO6 → TX ────────┼──────────┼───► RX                    │          │ │
│    │   GPIO7 ← RX ────────┼──────────┼───◄ TX                    │          │ │
│    │   GPIO20 → DE/RE ────┼──────────┼───► (RS485 only)          │          │ │
│    │                      │          │                    ┌──────┴──────┐   │ │
│    └──────────────────────┘          │                    │    CT       │   │ │
│                                      │                    │   Clamp     │   │ │
│                                      │                    │    ~~~      │   │ │
│    MACHINE MAINS ────────────────────┼──────────────────► │   (Live)    │   │ │
│       L ─────────────────────────────┼───► L Terminal     └─────────────┘   │ │
│       N ─────────────────────────────┼───► N Terminal                       │ │
│                                      └──────────────────────────────────────┘ │
│                                                                                 │
│    ⚠️ NO HV SENSING/MEASUREMENT CIRCUITRY ON PCB - EXTERNAL MODULE HANDLES IT │
│    ⚠️ J24 PROVIDES L/N/PE PASS-THROUGH TO METER IN EXISTING HV ZONE           │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

## 10.2 Supported Power Metering Modules

| Module           | Default Baud | Protocol   | Physical Layer | Notes                    |
| ---------------- | ------------ | ---------- | -------------- | ------------------------ |
| **PZEM-004T V3** | 9600         | Modbus RTU | TTL (Logic)    | Most common, 100A CT     |
| **JSY-MK-163T**  | 4800         | Modbus RTU | TTL (Logic)    | Bi-directional, compact  |
| **JSY-MK-194T**  | 4800         | Modbus RTU | TTL (Logic)    | Dual-channel metering    |
| **Eastron SDM**  | 2400 or 9600 | Modbus RTU | RS485          | DIN-rail, industrial     |
| **ATM90E32**     | -            | SPI        | SPI            | High precision (via I2C) |
| **BL0937**       | -            | Pulse/CF   | GPIO           | Budget option            |

```
┌────────────────────────────────────────────────────────────────────────────────┐
│  ⚠️  NON-ISOLATED POWER METERS (PZEM-004T, JSY, etc.) - GROUNDING WARNING      │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│  BACKGROUND:                                                                   │
│  This control board intentionally bonds PE (Earth) to GND at MH1 to provide   │
│  a return path for the steam boiler water level probe. This is NOT a SELV     │
│  (Safety Extra-Low Voltage) design - it is Class I (Earthed).                 │
│                                                                                 │
│  THE ISSUE WITH NON-ISOLATED METERS:                                          │
│  ───────────────────────────────────                                          │
│  TTL power meters like PZEM-004T internally connect their UART GND to mains   │
│  Neutral (N). Connecting such a meter to J17 creates:                         │
│                                                                                 │
│      Control GND ──── MH1 ──── PE (Earth) ──── PZEM GND ──── Mains Neutral    │
│                                                                                 │
│  This creates a PE-N bond through the PZEM, which should only exist at the    │
│  electrical service panel.                                                     │
│                                                                                 │
│  CONSEQUENCES:                                                                 │
│  ─────────────                                                                 │
│  • Any N-PE voltage difference causes current flow through board GND traces   │
│  • Ground loop noise may affect ADC readings (temperature, pressure)          │
│  • Violates electrical code in some jurisdictions (multiple PE-N bonds)       │
│  • NOT a shock hazard (board is already at earth potential via MH1)          │
│                                                                                 │
│  RECOMMENDATIONS:                                                              │
│  ────────────────                                                              │
│  1. PREFERRED: Use RS485 industrial meters (Eastron SDM series) - typically   │
│     have isolated RS485 outputs and avoid this issue entirely                 │
│                                                                                 │
│  2. ACCEPTABLE: Use PZEM/JSY with awareness of ground loop. If ADC noise      │
│     becomes problematic, add opto-isolator between J17 and meter module       │
│                                                                                 │
│  3. ALTERNATIVE: Omit power metering entirely if not needed                   │
│                                                                                 │
│  ⚠️  The PZEM/JSY will WORK correctly - this is a grounding quality issue,   │
│      not a functionality or safety issue given the existing PE-GND bond.      │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

## 10.3 J17 Universal Power Meter Connector (JST-XH 6-pin)

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                    J17 - POWER METER INTERFACE CONNECTOR                        │
│                          JST-XH 6-pin (2.54mm pitch)                           │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    PINOUT:                                                                     │
│    ┌─────┬─────┬─────┬─────┬─────┬─────┐                                      │
│    │  1  │  2  │  3  │  4  │  5  │  6  │                                      │
│    │ 3V3 │ 5V  │ GND │ RX  │ TX  │DE/RE│                                      │
│    └─────┴─────┴─────┴─────┴─────┴─────┘                                      │
│                                                                                 │
│    PIN DEFINITIONS:                                                            │
│    │ Pin │ Label │ Direction │ Signal    │ Notes                            │ │
│    │─────│───────│───────────│───────────│──────────────────────────────────│ │
│    │  1  │ 3V3   │ Power Out │ +3.3V     │ For 3.3V logic modules           │ │
│    │  2  │ 5V    │ Power Out │ +5V       │ For 5V modules (PZEM, JSY)       │ │
│    │  3  │ GND   │ Ground    │ GND       │ System ground                    │ │
│    │  4  │ RX    │ Input     │ GPIO7     │ Data from meter (via level shift)│ │
│    │  5  │ TX    │ Output    │ GPIO6     │ Data to meter (via 33Ω)          │ │
│    │  6  │ DE/RE │ Output    │ GPIO20    │ RS485 direction (TTL: NC)        │ │
│                                                                                 │
│    5V to 3.3V Level Shifting (J17 RX Line):                                  │
│    ─────────────────────────────────────────────────────                       │
│    Some power meters (PZEM, JSY, etc.) output 5V TTL. RP2350 is NOT 5V tolerant!
│    Without level shifting, 5V signals will damage GPIO7 over time.           │
│                                                                                 │
│    SOLUTION: Resistive voltage divider with bypass jumper                     │
│                                                                                 │
│         J17 Pin 4 (RX from meter)                                             │
│              │                                                                 │
│              ├──────────────────────────────────────┐                         │
│              │                                      │                         │
│         ┌────┴────┐                            ┌────┴────┐                    │
│         │  2.2kΩ  │  R45 (upper divider)       │   JP4   │ Solder jumper      │
│         │   1%    │                            │ (Bypass)│ Default: OPEN      │
│         └────┬────┘                            └────┬────┘                    │
│              │                                      │                         │
│              ├──────────────────────────────────────┘                         │
│              │                                                                 │
│              ├──────────[33Ω R45B]───────────► GPIO7 (METER_RX)              │
│              │                                                                 │
│         ┌────┴────┐                                                           │
│         │  3.3kΩ  │  R45A (lower divider)                                     │
│         │   1%    │                                                           │
│         └────┬────┘                                                           │
│              │                                                                 │
│             GND                                                                │
│                                                                                 │
│    JP4 JUMPER SETTINGS:                                                       │
│    ───────────────────────────                                                 │
│    │ Meter Type │ JP4 Setting │ Result                                      │ │
│    │────────────│─────────────│─────────────────────────────────────────────│ │
│    │ 5V TTL     │ OPEN        │ 5V → 3.0V via divider (safe for GPIO7)      │ │
│    │ 3.3V Logic │ CLOSED      │ 3.3V → 3.3V bypassed (no attenuation)       │ │
│                                                                                 │
│    ⚠️ WARNING: Closing JP4 with a 5V meter connected WILL DAMAGE GPIO7!      │
│                                                                                 │
│    DIVIDER CALCULATION (JP4 OPEN):                                             │
│    V_out = V_in × R_lower / (R_upper + R_lower)                               │
│    V_out = 5V × 3.3kΩ / (2.2kΩ + 3.3kΩ) = 5V × 0.6 = 3.0V  ✓               │
│                                                                                 │
│    THE 3.3V METER PROBLEM (Why JP4 exists):                                   │
│    ─────────────────────────────────────────                                   │
│    3.3V meters through divider: 3.3V × 0.6 = 1.98V                            │
│    RP2350 V_IH threshold: 0.65 × 3.3V = 2.145V                                │
│    1.98V < 2.145V = COMMUNICATION FAILURE with 3.3V meters!                   │
│    JP4 bypasses divider, allowing 3.3V signals to pass directly.              │
│                                                                                 │
│    TTL UART MODE (PZEM, JSY - 5V):                                             │
│    ─────────────────────────────────                                           │
│    • JP4 = OPEN (default)                                                      │
│    • Connect Pin 2 (5V), Pin 3 (GND), Pin 4 (RX), Pin 5 (TX)                  │
│    • Leave Pin 6 (DE/RE) unconnected                                          │
│    • RX line automatically level-shifted via on-board resistor divider       │
│                                                                                 │
│    TTL UART MODE (Modern 3.3V meters):                                         │
│    ─────────────────────────────────────                                       │
│    • JP4 = CLOSED (solder bridge)                                             │
│    • Connect Pin 1 (3.3V), Pin 3 (GND), Pin 4 (RX), Pin 5 (TX)               │
│    • Leave Pin 6 (DE/RE) unconnected                                          │
│                                                                                 │
│    RS485 MODE (Eastron, Industrial):                                          │
│    ──────────────────────────────────                                          │
│    • Uses on-board MAX3485 transceiver                                        │
│    • Pin 4/5 become RS485 A/B differential pair                               │
│    • Pin 6 controls transceiver direction                                     │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

| Pin | Label | Direction | TTL Mode       | RS485 Mode            |
| --- | ----- | --------- | -------------- | --------------------- |
| 1   | 3V3   | Power Out | 3.3V for logic | 3.3V for logic        |
| 2   | 5V    | Power Out | 5V for module  | 5V for module         |
| 3   | GND   | Ground    | System ground  | System ground         |
| 4   | RX    | Input     | Meter TX→GPIO7 | RS485 A (via MAX3485) |
| 5   | TX    | Output    | GPIO6→Meter RX | RS485 B (via MAX3485) |
| 6   | DE/RE | Output    | Not connected  | GPIO20→MAX3485 DE/RE  |

## 10.4 RS485 Transceiver Circuit (Optional - For Industrial Meters)

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                    RS485 TRANSCEIVER CIRCUIT (MAX3485)                          │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│                                      +3.3V                                     │
│                                        │                                       │
│                                   ┌────┴────┐                                  │
│                                   │  100nF  │  C70 (Decoupling)                │
│                                   └────┬────┘                                  │
│                                        │                                       │
│                                   VCC──┤                                       │
│                                        │                                       │
│                                        │  U8: MAX3485                          │
│                              ┌─────────┴─────────┐                             │
│    GPIO6 (TX) ──────────────►│ DI            A   │◄────────────► J17 Pin 4     │
│                              │                   │               (RS485-A)     │
│    GPIO7 (RX) ◄──────────────│ RO            B   │◄────────────► J17 Pin 5     │
│                              │                   │               (RS485-B)     │
│    GPIO20 (DE/RE) ──────────►│ DE/RE         GND │                             │
│                              │                   │                             │
│                              └─────────┬─────────┘                             │
│                                        │                                       │
│                                       ─┴─                                      │
│                                       GND                                      │
│                                                                                 │
│    TERMINATION RESISTOR (Jumper Selectable):                                  │
│    ─────────────────────────────────────────                                   │
│                                                                                 │
│    J17 Pin 4 (A) ────┬────[120Ω R99]────┬──── J17 Pin 5 (B)                   │
│    Component Values:                                                           │
│    ─────────────────                                                           │
│    U8:  MAX3485 or SP3485 (3.3V, SOT-23-8 or SOIC-8)                          │
│    C70: 100nF 25V Ceramic, 0805                                               │
│                                                                                 │
│    OPERATION:                                                                  │
│    ──────────                                                                  │
│    DE/RE = HIGH: Transmit mode (DI → A/B)                                     │
│    DE/RE = LOW:  Receive mode (A/B → RO)                                      │
│                                                                                 │
│    Firmware handles DE/RE timing automatically during Modbus transactions.    │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

## 10.5 Electrical Interface Summary

**J17 Connector (JST-XH 6-pin):**

| Pin | Signal | Type      | Voltage  | Notes                          |
| --- | ------ | --------- | -------- | ------------------------------ |
| 1   | 3.3V   | Power Out | 3.3V DC  | For 3.3V logic meters          |
| 2   | 5V     | Power Out | 5.0V DC  | For PZEM, JSY modules          |
| 3   | GND    | Ground    | 0V       | System ground                  |
| 4   | RX     | Input     | 3.3V TTL | UART RX from meter TX          |
| 5   | TX     | Output    | 3.3V TTL | UART TX to meter RX            |
| 6   | DE/RE  | Output    | 3.3V TTL | RS485 direction (Eastron only) |

**Electrical Characteristics:**

- **Power capacity:** 500mA @ 5V, 100mA @ 3.3V
- **UART levels:** 3.3V TTL (5V tolerant via level shifter on RX)
- **Communication:** Modbus RTU protocol (9600/4800/2400 baud)
- **Protection:** 33Ω series resistors on TX/RX for ESD/overcurrent

**Compatible Modules:**

- TTL UART: PZEM-004T, JSY-MK-163T/194T (use pins 2,3,4,5 - leave pin 6 unconnected)
- RS485: Eastron SDM120/230 (use all pins, GPIO20 controls transceiver direction)

**For firmware integration details, see:** `docs/pico/Power_Metering.md`

---

# 11. Safety & Protection

## 11.1 Mains Input Protection

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                        MAINS INPUT PROTECTION                                   │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    COMPLETE MAINS INPUT CIRCUIT WITH FUSING HIERARCHY:                        │
│    ───────────────────────────────────────────────────                        │
│                                                                                 │
│    L (Live)  ─────┬─────────────────────────────────────────────────────────  │
│                   │                                                            │
│              ┌────┴────┐                                                       │
│              │  F1     │  ← Fuse: 10A slow-blow, 5x20mm glass                 │
│              │  10A    │    (relay-switched loads only - pump, valves)        │
│              │ 250V    │                                                       │
│              └────┬────┘                                                       │
│                   │ (L_FUSED)                                                  │
│                   ├───────────────────────► To K1, K2, K3 relay COMs          │
│                   │                                                            │
│              ┌────┴────┐                                                       │
│              │  F2     │  ← Fuse: 2A slow-blow, 5x20mm glass                  │
│              │  2A     │    (HLK AC/DC module only - fusing hierarchy)        │
│              │ 250V    │                                                       │
│              └────┬────┘                                                       │
│                   │ (L_HLK)                                                    │
│                   │                                                            │
│              ┌────┴────┐     ┌─────────┐                                      │
│              │  RV1    │     │   C1    │     │  R_BLEED │                     │
│              │  MOV    │     │   X2    │     │   1MΩ    │  ← Optional bleed   │
│              │  275V   ├─────┤  100nF  ├─────┤  0.5W    │    resistor (DNP)   │
│              │  14mm   │     │  275V   │     │  (opt)   │                     │
│              └────┬────┘     └────┬────┘     └────┬─────┘                     │
│                   │               │               │ (to HLK L input)          │
│    N (Neutral) ───┴───────────────┴───────────────┴────────────────────────  │
│                                                                                │
│    Optional EMI Filter (for CE compliance):                                   │
│    ────────────────────────────────────────                                   │
│                                                                                │
│         L ──┬──[CMC]──┬─── L (filtered)                                       │
│             │    ║    │                                                        │
│         N ──┴────╫────┴─── N (filtered)                                       │
│                  ║                                                             │
│              Common-mode choke                                                 │
│              (e.g., Würth 744272102, 10mH)                                    │
│                                                                                │
│    PE (Earth) ────────────────────────────────────────────────────────────    │
│                   │                                                            │
│                   └──► To chassis ground point (if metal enclosure)           │
│                        PE connects to signal GND at single point (see 12.5)   │
│                                                                                │
│    Component Specifications:                                                   │
│    ─────────────────────────                                                   │
│    F1:  Fuse, 10A/250V, 5x20mm, slow-blow (time-lag)                         │
│         Fuse: Littelfuse 0218010.MXP or equivalent                            │
│         Holder: Littelfuse 01000056Z (PCB mount clips with cover)            │
│                                                                                │
│    F2:  Fuse, 2A/250V, 5x20mm, slow-blow (HLK module only)                   │
│         Fuse: Littelfuse 0218002.MXP or equivalent                            │
│         Holder: Littelfuse 01000056Z (PCB mount clips with cover)            │
│         Purpose: Fusing hierarchy - protects HLK module and its PCB traces   │
│         independently from relay loads. If HLK fails, only F2 blows.         │
│                 Alt: Schurter 0031.8201 (enclosed PCB mount)                  │
│                                                                                │
│    R_BLEED: 1MΩ 0.5W resistor across C1 (OPTIONAL - DNP by default)          │
│         Purpose: Discharges X2 cap when machine unplugged (safety)           │
│         At 100nF, discharge τ = 0.1s, fully discharged in <1 second          │
│         Populate if required for safety certification (IEC 60950)            │
│                                                                                │
│    ⚠️  FUSE HOLDER LAYOUT NOTES:                                              │
│    • Holder is ~27mm long - verify clearance to HLK-15M05C and AC terminals  │
│    • Orient PARALLEL to board edge for easy fuse access when installed       │
│    • Minimum 4mm clearance from high-voltage components                       │
│    • Verify physical fit in CAD before committing layout                      │
│                                                                                │
│    ✅ FUSE SIZING SIMPLIFIED (External Metering Design):                      │
│    ─────────────────────────────────────────────────────                      │
│    With external power meter, heater current does NOT flow through the PCB.  │
│    The on-board fuse only protects relay-switched loads:                     │
│    • K2 (Pump): ~5A peak (Ulka EP5)                                          │
│    • K1 (LED): ≤100mA                                                        │
│    • K3 (Solenoid): ~0.5A                                                    │
│    Total relay-switched: ~6A maximum                                          │
│                                                                                │
│    10A fuse provides adequate margin for relay-switched loads.               │
│    Standard 5×20mm fuse holders (rated 10A) are now acceptable.              │
│                                                                                │
│    RV1: MOV/Varistor, 275V AC, 14mm disc, 4500A surge                        │
│         Part: Littelfuse V275LA20AP or Epcos S14K275                          │
│         Provides surge protection for lightning/transients                    │
│                                                                                │
│    C1:  X2 capacitor, 100nF, 275V AC, safety rated                           │
│         Part: Vishay MKP X2 or TDK B32922C series                            │
│         Provides EMI filtering for common-mode noise                          │
│                                                                                │
└────────────────────────────────────────────────────────────────────────────────┘
```

## 11.2 Creepage and Clearance Requirements

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                    CREEPAGE AND CLEARANCE REQUIREMENTS                          │
│             (Per IEC 60950-1 / IEC 62368-1 for PCB design reference)           │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    DEFINITIONS:                                                                │
│    ─────────────                                                               │
│    Creepage: Shortest path along a surface between conductors                 │
│    Clearance: Shortest path through air between conductors                    │
│                                                                                 │
│    For 240V RMS mains (peak 340V), pollution degree 2:                        │
│                                                                                 │
│    │ Insulation Type     │ Creepage  │ Clearance │ Application              │ │
│    │─────────────────────│───────────│───────────│──────────────────────────│ │
│    │ Basic Insulation    │ 2.5mm     │ 2.0mm     │ Within mains section     │ │
│    │ Reinforced (double) │ 6.0mm     │ 4.0mm     │ Mains to low-voltage     │ │
│    │                     │           │           │ (required for safety)    │ │
│                                                                                 │
│    DESIGN REQUIREMENTS:                                                        │
│    ────────────────────                                                        │
│                                                                                 │
│    1. AC Input to 5V DC Rail: ≥6mm creepage, ≥4mm clearance                  │
│       (achieved by HLK-15M05C internal isolation + PCB layout)                │
│                                                                                 │
│    2. Relay Contacts to Coil: ≥6mm creepage, ≥4mm clearance                  │
│       (check relay datasheet - most meet this internally)                     │
│       PCB traces must also maintain this separation                           │
│                                                                                 │
│    3. Power Meter Mains to Pico: ≥6mm creepage, ≥4mm clearance              │
│       Use PCB slots if necessary                                              │
│                                                                                 │
│    4. Between Live and Neutral: ≥2.5mm creepage/clearance                    │
│                                                                                 │
│    PCB SLOT USAGE:                                                            │
│    ────────────────                                                           │
│                                                                                │
│            MAINS SIDE          │          LOW VOLTAGE SIDE                    │
│    ┌──────────────────────┐    │    ┌──────────────────────┐                 │
│    │                      │    │    │                      │                 │
│    │   Relay Contacts     │    │    │      Pico            │                 │
│    │   AC/DC Module       │    │    │      Sensors         │                 │
│    │   Power Meter        │ ═══╪═══ │      Logic           │                 │
│    │                      │    │    │                      │                 │
│    └──────────────────────┘   SLOT  └──────────────────────┘                 │
│                                │                                              │
│                           ≥2mm wide routed slot in PCB                        │
│                           Extends creepage path                               │
│                                                                                │
│    Slot dimensions: 2-3mm wide, bridged only by approved components          │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

## 11.3 ESD and Transient Protection

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                      ESD AND TRANSIENT PROTECTION                               │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    External-facing signals require ESD protection:                            │
│                                                                                 │
│    SENSOR INPUTS (GPIO2-5, ADC0-2):                                           │
│    ─────────────────────────────────                                          │
│                                                                                 │
│    External         ESD Clamp                  To Pico GPIO                   │
│    Signal ────┬────[PESD5V0S1BL]────┬─────────────────►                       │
│               │                     │                                          │
│               │                     │                                          │
│              GND                   GND                                         │
│                                                                                 │
│    ESD Clamp Options:                                                         │
│    • PESD5V0S1BL (SOD-323): Single-line, 5V, <1pF                            │
│    • TPD1E10B06 (SOD-882): 10kV HBM rating                                   │
│    • PRTR5V0U2X (SOT-143): Dual-line                                         │
│                                                                                 │
│    Place ESD clamps as close to connector as possible.                        │
│                                                                                 │
│    UART LINES (GPIO0-1, GPIO6-7):                                             │
│    ─────────────────────────────────                                          │
│    33Ω series resistors (R40-R44) limit current during ESD clamping events.  │
│    This works with the ESD diode's clamping action to protect GPIO pins.     │
│                                                                                 │
│    ⚠️ ESD CLAMPING VOLTAGE WARNING:                                           │
│    ─────────────────────────────────                                          │
│    PESD5V0S1BL clamping voltage is ~14V at peak pulse current, but RP2350    │
│    absolute max is 3.6V. The series resistors are CRITICAL - they limit      │
│    current to the GPIO during the clamping transient, allowing the internal  │
│    protection diodes to share the load without destructive current flow.     │
│    For exposed connectors, use 33Ω-100Ω series resistors on ALL signal lines.│
│                                                                                 │
│    RS485 INTERFACE (U8 MAX3485, J17):                                         │
│    ──────────────────────────────────                                          │
│    Industrial environments (kitchens) have severe EMI from motors, heaters.  │
│                                                                                 │
│                       ┌───────────────┐                                       │
│    J17 RS485_A ───┬───┤     SM712     ├───┬─── MAX3485 A                     │
│                   │   │  Asymmetric   │   │                                   │
│                   │   │  TVS Diode    │   │                                   │
│    J17 RS485_B ───┼───┤   (-7V/+12V)  ├───┼─── MAX3485 B                     │
│                   │   └───────┬───────┘   │                                   │
│                   │           │           │                                   │
│                  GND         GND         GND                                  │
│                                                                                 │
│    D21 (SM712): Asymmetric TVS designed for RS485/RS422                      │
│    • Clamps to -7V / +12V (matches RS485 common-mode range)                  │
│    • Protects against lightning-induced surges and motor switching noise     │
│    • Place close to J17 connector                                             │
│                                                                                 │
│    R19 (4.7kΩ pull-down on GPIO20/DE): Ensures transceiver defaults to       │
│    receive mode during boot, preventing bus contention on startup.            │
│                                                                                 │
│    5V RAIL TRANSIENT SUPPRESSION:                                             │
│    ───────────────────────────────                                            │
│                                                                                 │
│    From HLK-15M05C ──┬────[Ferrite Bead]────┬──► 5V to circuits              │
│                      │                       │                                 │
│                 ┌────┴────┐            ┌────┴────┐                            │
│                 │  TVS    │            │  100µF  │                            │
│                 │  5.6V   │            │  16V    │                            │
│                 │ (SMBJ5.0A)           │         │                            │
│                 └────┬────┘            └────┬────┘                            │
│                      │                      │                                  │
│                     GND                    GND                                 │
│                                                                                 │
│    TVS diode absorbs relay coil flyback spikes that pass the 1N4007.         │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

## 11.4 Thermal Management

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                          THERMAL MANAGEMENT                                     │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    HEAT SOURCES ON PCB:                                                        │
│    ────────────────────                                                        │
│                                                                                 │
│    │ Component          │ Power Dissipation │ Management                    │ │
│    │────────────────────│───────────────────│───────────────────────────────│ │
│    │ HLK-15M05C (AC/DC) │ ~2.7W @ 1A load   │ Place away from heat-sensitive│ │
│    │ LDO Regulator      │ 0.3W @ 200mA      │ Use AP2112K for lower drop    │ │
│    │ Relay Coils (×3)   │ 0.2-0.4W each     │ Normal, space apart           │ │
│    │ Transistor drivers │ <0.1W each        │ Negligible                    │ │
│                                                                                 │
│    GUIDELINES:                                                                 │
│    ───────────                                                                 │
│    1. HLK-15M05C: Leave 5mm clearance on all sides for airflow               │
│    2. Place electrolytic capacitors away from heat sources (reduces aging)   │
│    3. LDO: Use thermal vias under SOT-223 tab (5× 0.3mm vias to GND plane)  │
│    4. Relays: Space 10mm apart if possible                                    │
│    5. Copper pours: Use exposed copper for heat spreading where possible     │
│                                                                                 │
│    VENTILATION:                                                               │
│    ────────────                                                               │
│    If enclosed, ensure enclosure has ventilation holes near HLK-15M05C       │
│    and relay section. Natural convection is usually sufficient.              │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

---

# 12. PCB Design Requirements

## 12.1 Board Specifications

| Parameter               | Specification                                               |
| ----------------------- | ----------------------------------------------------------- |
| **Dimensions**          | **130mm × 80mm**                                            |
| Enclosure Mounting Area | 150mm × 100mm (leaves room for terminals + enclosure walls) |
| **Layers**              | **2-layer**                                                 |
| Copper Weight           | 2oz (70µm) both layers for high-current traces              |
| Board Thickness         | 1.6mm                                                       |
| Material                | FR-4, Tg 130°C minimum, UL 94V-0 flammability rating        |

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                         ENCLOSURE MOUNTING LAYOUT                               │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    Enclosure Mounting Area: 150mm × 100mm                                      │
│    ┌─────────────────────────────────────────────────────────────────────────┐ │
│    │                                                                         │ │
│    │    ┌─────────────────────────────────────────────────────────────────┐ │ │
│    │    │                                                                 │ │ │
│    │    │                      PCB: 130mm × 80mm                         │ │ │
│    │    │                                                                 │ │ │
│    │    │   ┌───┐                                              ┌───┐     │ │ │
│    │    │   │M3 │  ← Mounting holes (4×)                       │M3 │     │ │ │
│    │    │   └───┘                                              └───┘     │ │ │
│    │    │                                                                 │ │ │
│    │    │                                                                 │ │ │
│    │    │   ┌───┐                                              ┌───┐     │ │ │
│    │    │   │M3 │                                              │M3 │     │ │ │
│    │    │   └───┘                                              └───┘     │ │ │
│    │    │                                                                 │ │ │
│    │    └───────────────────────────┬─────────────────────────────────────┘ │ │
│    │                                │                                       │ │
│    │    ←─── 10mm ───→              ▼                     ←─── 10mm ───→    │ │
│    │                        Terminal clearance                              │ │
│    │                           (~10mm)                                      │ │
│    └─────────────────────────────────────────────────────────────────────────┘ │
│                                                                                 │
│    Clearances:                                                                 │
│    • 10mm on each side for enclosure walls                                    │
│    • 10mm below PCB for terminal/wire clearance                               │
│    • Mounting holes: 3.2mm diameter for M3 screws                             │
│    • Hole placement: 5mm from board edges                                     │
│                                                                                 │
│    ⚠️  MECHANICAL FIT CONSIDERATIONS (ECM Synchronika Specifics):             │
│    ─────────────────────────────────────────────────────────────              │
│    • ECM Synchronika: GICAR box is low in chassis, near bottom plate         │
│    • Space is tight but relatively cool (away from boilers)                   │
│    • CRITICAL: Ensure Raspberry Pi Pico 2 USB port is ACCESSIBLE              │
│                                                                                │
│    Pico Orientation Recommendations:                                          │
│    ─────────────────────────────────                                          │
│    • Orient Pico so USB edge faces "Service" side of machine                 │
│    • If board mounted flat, vertical USB port may be blocked by cables       │
│    • Consider right-angle USB adapter or cable for space-constrained installs│
│    • Add access cutout in enclosure aligned with Pico USB port               │
│    • Alternative: Use USB breakout cable to enclosure-mounted USB jack       │
│                                                                                │
│    Thermal Considerations:                                                    │
│    ──────────────────────                                                     │
│    • Keep board away from boilers and steam paths                            │
│    • Max operating temp: 50°C ambient (derate HLK-15M05C above this)         │
│    • Add ventilation holes in enclosure if mounting near warm components     │
│                                                                                │
└────────────────────────────────────────────────────────────────────────────────┘
```

| Surface Finish | ENIG preferred (HASL acceptable) |
| Solder Mask | Both sides, green or black |
| Silkscreen | Both sides, white |
| Minimum Trace/Space | 0.2mm / 0.2mm (8 mil) |
| Minimum Via | 0.3mm drill, 0.6mm pad |

## 12.2 Trace Width Requirements

| Signal/Power        | Current | Width (2oz Cu)     | Notes                      |
| ------------------- | ------- | ------------------ | -------------------------- |
| Mains Live/Neutral  | 6A peak | 1.5mm (60 mil)     | Relay-switched loads only  |
| Relay K2 (Pump)     | 5A peak | 1.5mm (60 mil)     | Ulka pump                  |
| Relay K1 (LED)      | 100mA   | 0.5mm (20 mil)     | Mains indicator lamp       |
| Relay K3 (Solenoid) | 0.5A    | 1.0mm (40 mil)     | 3-way solenoid valve       |
| 5V power rail       | 1A      | 1.0mm (40 mil)     | Main distribution          |
| 5V to Pico VSYS     | 500mA   | 0.5mm (20 mil)     |                            |
| 5V to J15 (ESP32)   | 500mA   | 1.0mm (40 mil)     | WiFi TX spikes need margin |
| 3.3V power rail     | 500mA   | 0.5mm (20 mil)     |                            |
| Relay coil drive    | 80mA    | 0.3mm (12 mil)     |                            |
| Signal traces       | <10mA   | 0.25mm (10 mil)    | GPIO, UART, SPI            |
| Ground returns      | -       | Match signal width | Use ground plane           |

### ✅ SIMPLIFIED PCB DESIGN (External Power Metering)

**With external power metering, heater current (12A+) does NOT flow through the control PCB.**

The PCB only handles relay-switched loads:

- **K2 (Pump):** 5A peak (Ulka EP5)
- **K1 (LED):** ≤100mA
- **K3 (Solenoid):** ~0.5A
- **Total maximum:** ~6A

**Benefits of External Metering Design:**

- ✅ No 16A shunt resistor required
- ✅ No solder mask openings on high-current traces needed
- ✅ Standard thermal relief is acceptable
- ✅ Standard terminal blocks are sufficient
- ✅ 10A fuse with standard holder
- ✅ Simpler PCB layout
- ✅ No fire risk from high-current paths

### Standard PCB Practices Apply

For the relay-switched loads (max ~6A):

1. Use 1.5mm trace width for pump relay (K2) traces
2. Standard solder mask is fine
3. Thermal relief can be used on all pads
4. Standard 6.3mm spade terminals rated for 10A are acceptable

## 12.3 Layer Stackup (2-Layer)

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                   2-LAYER PCB STACKUP (130mm × 80mm)                            │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    ┌────────────────────────────────────────────────────────────────────┐     │
│    │  TOP COPPER (2oz / 70µm) - Component Side                          │     │
│    │  ─────────────────────────────────────────────                     │     │
│    │  • SMD components (Pico, ICs, passives)                            │     │
│    │  • High-current mains traces (3mm+ width)                          │     │
│    │  • Relay and connector pads                                        │     │
│    │  • Signal routing (LV section)                                     │     │
│    │  • Isolation slot along HV/LV boundary                             │     │
│    ├────────────────────────────────────────────────────────────────────┤     │
│    │  FR-4 CORE (1.4mm)                                                 │     │
│    │  - Tg ≥ 130°C                                                      │     │
│    │  - UL 94V-0 rated                                                  │     │
│    ├────────────────────────────────────────────────────────────────────┤     │
│    │  BOTTOM COPPER (2oz / 70µm) - Solder Side                          │     │
│    │  ─────────────────────────────────────────────                     │     │
│    │  • Ground plane (maximize coverage in LV section)                  │     │
│    │  • Signal routing for crossovers (keep short)                      │     │
│    │  • Through-hole component leads                                    │     │
│    │  • Star ground connection point near Pico                          │     │
│    │  • NO high-voltage traces on bottom layer                          │     │
│    └────────────────────────────────────────────────────────────────────┘     │
│                                                                                 │
│    Total Thickness: 1.6mm (standard)                                          │
│                                                                                 │
│    2-Layer Routing Strategy:                                                   │
│    ─────────────────────────                                                   │
│    • Top: Components, all HV traces, horizontal LV signal routing             │
│    • Bottom: Ground plane (priority), vertical LV routing, power polygons     │
│    • Keep ground plane continuous under Pico and analog section               │
│    • Use thermal relief on high-current ground connections                    │
│    • Route all mains traces ONLY on top layer                                 │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

## 12.4 Critical Layout Notes

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                         PCB LAYOUT REQUIREMENTS                                │
│                    (Review these BEFORE starting layout)                       │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    1. POWER METER UART TRACES (GPIO6/7) - PIO Driven                           │
│    ─────────────────────────────────────────────────────                       │
│    GPIO6/7 (Power Meter UART) will be driven by RP2350 PIO (Programmable I/O),│
│    not hardware UART. This means:                                              │
│                                                                                 │
│    ✅ NO differential pair routing required                                    │
│    ✅ NO matched trace lengths required                                        │
│    ✅ Just keep traces SHORT (< 5cm recommended)                               │
│    ✅ Include 33Ω series resistors (R44, R45) for protection                   │
│                                                                                 │
│    PIO handles timing in software, so standard routing rules for              │
│    high-speed differential pairs do NOT apply here.                            │
│                                                                                 │
│    ─────────────────────────────────────────────────────────────────────────   │
│                                                                                 │
│    2. STAR GROUND AT MH1 ONLY                                                  │
│    ──────────────────────────────                                              │
│    Grounding: Only mounting hole MH1 connects to GND plane (star point).     │
│                                                                                 │
│    │ Hole │ Type │ Connection │ Purpose                    │                  │
│    │──────│──────│────────────│────────────────────────────│                  │
│    │ MH1  │ PTH  │ GND plane  │ Star ground / PE bond      │                  │
│    │ MH2  │ NPTH │ ISOLATED   │ Mechanical only            │                  │
│    │ MH3  │ NPTH │ ISOLATED   │ Mechanical only            │                  │
│    │ MH4  │ NPTH │ ISOLATED   │ Mechanical only            │                  │
│                                                                                 │
│    WHY: If all mounting holes connect to GND, current will flow through       │
│    the metal chassis via multiple paths, creating GROUND LOOPS.               │
│    This causes EMI, noise, and unreliable ground reference.                   │
│                                                                                 │
│    Mark MH1 with "⏚" symbol on silkscreen.                                    │
│                                                                                 │
│    ⚠️ USB GROUND LOOP WARNING:                                                │
│    When Pico USB is connected to an earthed PC while machine mains is on,     │
│    a ground loop exists: PC Earth → USB GND → Pico GND → MH1 → Machine PE.    │
│    This can cause noise on ADC readings or, in worst case, damage if there    │
│    is a significant voltage difference between PC earth and machine PE.       │
│    Recommendation: Use USB only for development with machine unpowered,       │
│    or use a USB isolator for production debugging.                            │
│                                                                                 │
│    ─────────────────────────────────────────────────────────────────────────   │
│                                                                                 │
│    3. LEVEL PROBE GUARD RING (High-Impedance Trace Protection)                │
│    ────────────────────────────────────────────────────────────               │
│    The trace from Level Probe (J26 Pin 5) to OPA342 input is:                 │
│    • High-impedance (MΩ range)                                                │
│    • Very sensitive to noise pickup                                           │
│    • Susceptible to 50/60Hz mains hum                                         │
│                                                                                 │
│    REQUIRED: Surround this trace with a GUARD RING                            │
│                                                                                 │
│    Layout:                                                                     │
│    ┌─────────────────────────────────────────────────────────────────────┐    │
│    │  GND ─────────────────────────────────────────────────────────── GND│    │
│    │   │                                                               │ │    │
│    │   │   J26-5 (Probe) ─────────────────────────────► OPA342 IN+    │ │    │
│    │   │                                                               │ │    │
│    │  GND ─────────────────────────────────────────────────────────── GND│    │
│    └─────────────────────────────────────────────────────────────────────┘    │
│                                                                                 │
│    Implementation:                                                             │
│    • Route GND traces on BOTH sides of the probe signal trace                 │
│    • Connect guard traces to GND plane with vias every 5mm                    │
│    • Keep probe trace as SHORT as possible (< 2cm ideal)                      │
│    • Place OPA342 physically CLOSE to J26 screw terminal                      │
│    • Avoid routing probe trace near relay coils or mains traces               │
│                                                                                 │
│    This prevents 50Hz mains hum from coupling into the AC level sensing.      │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

## 12.5 PCB Layout Zones

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                    PCB LAYOUT ZONES (130mm × 80mm)                              │
│                      (Board orientation: landscape)                            │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    ┌──────────────────────────────────────────────────────────────────────┐   │
│    │                                                                      │   │
│    │  ┌──────────────────────────────┐ ║ ┌──────────────────────────────┐│   │
│    │  │        HIGH VOLTAGE          │ ║ │       LOW VOLTAGE            ││   │
│    │  │                              │ ║ │                              ││   │
│    │  │  ┌────────────┐  ┌────────┐  │ ║ │  ┌─────────────────────────┐ ││   │
│    │  │  │ AC Input   │  │HLK-15M│  │ ║ │  │   RASPBERRY PI PICO 2  │ ││   │
│    │  │  │ L N PE     │  │ AC/DC  │  │ ║ │  │   (Module or socket)   │ ││   │
│    │  │  │ Fuse, MOV  │  │        │  │ ║ │  │                        │ ││   │
│    │  │  └────────────┘  └────────┘  │ ║ │  └─────────────────────────┘ ││   │
│    │  │                              │ S │                              ││   │
│    │  │  ┌──────┐┌──────┐┌──────┐   │ L │  ┌─────────┐  ┌──────────┐   ││   │
│    │  │  │ K1   ││ K2   ││ K3   │   │ O │  │ LDO 3.3V│  │ (spare)  │   ││   │
│    │  │  │Relay ││Relay ││Relay │   │ T │  │         │  │          │   ││   │
│    │  │  └──────┘└──────┘└──────┘   │   │  └─────────┘  └──────────┘   ││   │
│    │  │                              │ ║ │                             ││   │
│    │  │  ┌────────────────────────┐  │ ║ │  ┌─────────────────────────┐ ││   │
│    │  │  │ Level Probe Circuit    │  │ ║ │  │   Sensor Input Section │ ││   │
│    │  │  │ (OPA342 + TLV3201)     │  │ ║ │  │   NTC, Pressure, etc.  │ ││   │
│    │  │  └────────────────────────┘  │ ║ │  └─────────────────────────┘ ││   │
│    │  │                              │ ║ │                              ││   │
│    │  └──────────────────────────────┘ ║ └──────────────────────────────┘│   │
│    │                                   ║                                  │   │
│    │  ─────────────────────────────────╫──────────────────────────────────│   │
│    │   HV CONNECTORS (6.3mm spades)    ║   LV CONNECTORS (screw terms)   │   │
│    │   K1 K2 K3 SSR1 SSR2              ║   NTC PRES TC SW1 SW2 SW3 SW4  │   │
│    │                                   ║   ESP32(JST) SERVICE(HDR)        │   │
│    │                                   ║                                  │   │
│    └──────────────────────────────────────────────────────────────────────┘   │
│                                                                                 │
│    KEY:                                                                        │
│    ════  Silkscreen boundary line between HV and LV zones                     │
│    SLOT  PCB routed slot for isolation (2-3mm wide)                           │
│                                                                                 │
│    Mounting Holes: 4× M3 (3.2mm) near corners, ≥5mm from board edge           │
│                                                                                │
│    ⚠️  MOUNTING HOLE GROUNDING (Critical for ground loop prevention):         │
│    ─────────────────────────────────────────────────────────────────          │
│    MH1 (bottom-left): PTH, connected to GND plane → PE STAR POINT             │
│    MH2 (bottom-right): NPTH or isolated pad → NOT connected to GND            │
│    MH3 (top-left): NPTH or isolated pad → NOT connected to GND                │
│    MH4 (top-right): NPTH or isolated pad → NOT connected to GND               │
│                                                                                │
│    Mark MH1 with "⏚" silkscreen symbol for PE bonding screw location.        │
│    This prevents multiple ground loops through metal chassis/screws.          │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

## 12.6 Grounding Strategy

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                          GROUNDING STRATEGY                                     │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    ⚠️  IMPORTANT: PE and Signal GND are CONNECTED at a single point.          │
│    This matches the existing ECM machine grounding and is required for the     │
│    steam boiler level probe sensing circuit to work properly.                  │
│                                                                                 │
│    GROUND HIERARCHY:                                                           │
│    ──────────────────                                                          │
│                                                                                 │
│    PE (Protective Earth) ───► J1-PE terminal                                   │
│           │                                                                    │
│           │    ┌────────────────────────────────────────────────────┐         │
│           │    │  SINGLE POINT PE-GND CONNECTION                    │         │
│           └────┤  (at mounting hole pad or designated star point)   │         │
│                │  This provides reference for level probe sensing.  │         │
│                └────────────────────┬───────────────────────────────┘         │
│                                     │                                          │
│    ┌────────────────────────────────┴───────────────────────────────────┐     │
│    │                        SIGNAL GROUND (0V)                           │     │
│    │                    (From isolated AC/DC module secondary)           │     │
│    └──────┬───────────────────────────────────────────────────────────────┘     │
│           │                                                                    │
│           ├───────────────────────────────────────────────────────────────┐   │
│           │                                                               │   │
│    ┌──────┴──────┐                                              ┌─────────┴──┐│
│    │ DIGITAL GND │                                              │ ANALOG GND ││
│    │             │                                              │            ││
│    │ - Pico GND  │                                              │ - ADC GND  ││
│    │ - Relays    │    Star ground connection point              │ - NTC GND  ││
│    │ - LEDs      │◄──────────────────────────────────────────── │ - Pressure ││
│    │ - Buzzer    │    (single point near Pico ADC_GND pin)      │            ││
│    │ - UART      │                                              │            ││
│    └─────────────┘                                              └────────────┘│
│                                                                                │
│    WHY PE-GND CONNECTION IS REQUIRED:                                         │
│    ──────────────────────────────────                                         │
│    • Steam boiler level probe uses boiler body (PE) as ground reference      │
│    • Without PE-GND connection, probe circuit has no return path             │
│    • Original ECM machine has shared PE/GND - this maintains compatibility   │
│    • AC/DC module still provides isolation from mains L/N (safety)           │
│                                                                                │
│    ⚠️  USB GROUND LOOP WARNING:                                               │
│    ─────────────────────────────                                              │
│    With PE bonded to signal GND, connecting USB to a grounded PC while       │
│    mains is active creates a ground loop. If there is any voltage potential  │
│    difference between machine PE and PC ground, current can flow through     │
│    the USB shield, causing:                                                   │
│    • Erratic behavior or data corruption                                      │
│    • Potential damage to USB ports                                            │
│    • Safety hazard if isolation fails                                         │
│                                                                                │
│    Recommended silkscreen near USB/service header:                            │
│    "⚠️ DISCONNECT MAINS BEFORE USB/UART DEBUGGING"                           │
│                                                                                │
│    For development with mains active, use a USB isolator (e.g., Adafruit     │
│    USB Isolator) to break the ground loop.                                    │
│                                                                                │
│    IMPLEMENTATION:                                                            │
│    ────────────────                                                           │
│    1. Use ground plane on bottom layer (as continuous as possible)           │
│    2. Connect PE to signal GND at MH1 ONLY (see Section 12.4 zone layout)    │
│       - MH1: PTH with GND connection (PE star point)                         │
│       - MH2, MH3, MH4: NPTH or isolated pads (NO GND connection)             │
│    3. Route analog ground traces directly to Pico ADC_GND pin               │
│    4. Add ferrite bead between digital and analog sections if noise issue    │
│    5. Use star grounding: all critical grounds meet at one point             │
│                                                                                │
│    ⚠️  PCB MANUFACTURING NOTE:                                               │
│    By default, manufacturers plate all holes and connect to ground pour.      │
│    EXPLICITLY specify MH2-MH4 as NPTH (Non-Plated Through Hole) or           │
│    request isolated pads with no connection to ground plane.                  │
│                                                                                │
│    TEST POINTS:                                                               │
│    ────────────                                                               │
│    Add test pads for: GND, 5V, 3.3V, 3.3V_ANALOG, each ADC input             │
│                                                                                │
└────────────────────────────────────────────────────────────────────────────────┘
```

## 12.7 Silkscreen Requirements

| Marking                              | Location                        | Purpose                                |
| ------------------------------------ | ------------------------------- | -------------------------------------- |
| ⚠️ HIGH VOLTAGE                      | Near AC input and relays        | Safety warning                         |
| ⚠️ DISCONNECT MAINS BEFORE USB DEBUG | Near USB/service header         | Ground loop warning                    |
| Dashed boundary line                 | Between HV and LV zones         | Visual separation                      |
| L, N, ⏚                              | Mains input terminal            | Wire identification                    |
| ⏚ (PE symbol)                        | At MH1 mounting hole            | PE star point / bonding screw location |
| K1, K2, K3                           | Relay terminals                 | Function identification                |
| SSR1+, SSR1-, etc.                   | SSR connectors                  | Polarity marking                       |
| Pin numbers                          | All connectors                  | Wiring reference                       |
| R1, C1, U1, etc.                     | All components                  | Assembly reference                     |
| Version, date                        | Board corner                    | Revision tracking                      |
| Polarity marks                       | Electrolytic caps, diodes, LEDs | Assembly guidance                      |
| Pin 1 indicator                      | ICs, Pico socket                | Orientation                            |

---

# 13. Connector Specifications

## 13.1 6.3mm Spade Terminals (High-Power Machine Connections)

High-current connections to original machine wiring use 6.3mm (0.25") spade terminals for plug & play compatibility.

### Power Metering Wiring

Relay-switched loads (pump, valves) are fused and distributed via internal bus. Power metering is handled by external meter module with CT clamp (no high current through PCB).

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                    POWER METERING - MAINS DISTRIBUTION                           │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│   FROM MAINS          ON PCB                             TO MACHINE LOADS       │
│   ──────────        ────────────                        ─────────────────       │
│                                                                                  │
│                    ┌─────────┐   ┌─────────┐                                    │
│   L_IN ───────────►│  FUSE   ├──────────────────► L_FUSED (internal bus)       │
│   (J1-L)           │  (F1)   │   │ (1mΩ)   │         │                          │
│                    └─────────┘   └─────────┘         ├──► Relay K1 COM          │
│                                                      ├──► Relay K2 COM          │
│                                                      ├──► Relay K3 COM          │
│                                                      └──► J24-L (Power Meter)   │
│                                                                                  │
│   N_IN ──────────────────────────────────────────────┬──► N (to all loads)      │
│   (J1-N)                                             └──► J24-N (Power Meter)   │
│                                                                                  │
│   PE ────────────────────────────────────────────────────► J24-PE (Power Meter) │
│   (J1-PE)                                                  (optional, for DIN)  │
│                                                                                  │
│   ⚠️ SSR heater power: Mains → SSR → Heater (via existing machine wiring)       │
│      NOT through this PCB! PCB provides 5V control signals via J26 Pin 17-20.   │
│                                                                                  │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### High-Power Terminal Assignments (6.3mm Spade)

| Designator                                                   | Function            | Terminal Type | Wire Gauge | Notes                            |
| ------------------------------------------------------------ | ------------------- | ------------- | ---------- | -------------------------------- |
| **Mains Input**                                              |
| J1-L                                                         | Mains Live Input    | 6.3mm male    | 14 AWG     | Fused, to relay COMs             |
| J1-N                                                         | Mains Neutral Input | 6.3mm male    | 14 AWG     | Common neutral bus               |
| J1-PE                                                        | Protective Earth    | 6.3mm male    | 14 AWG     | To chassis                       |
| **220V AC Relay Outputs (All COMs internal to L_FUSED bus)** |
| J2-NO                                                        | Relay K1 N.O.       | 6.3mm male    | 16 AWG     | Mains lamp output (≤100mA, 220V) |
| J3-NO                                                        | Relay K2 N.O.       | 6.3mm male    | 14 AWG     | Pump output (5A peak, 220V)      |
| J4-NO                                                        | Relay K3 N.O.       | 6.3mm male    | 16 AWG     | Solenoid output (~0.5A, 220V)    |

**Note:** Relay K1, K2, K3 COMs are connected internally to the fused live bus - no external COM terminals needed.

**Spade Terminal Part Numbers:**

- PCB Mount: Keystone 1285 or TE 63951-1 (6.3mm blade)
- Use vertical or right-angle spade terminals depending on enclosure

## 13.1a Unified Low-Voltage Screw Terminal Block (J26 - 18 Position)

**ALL low-voltage connections are consolidated into a single 18-position screw terminal block.**

**⚠️ J26 is for LOW VOLTAGE ONLY! 220V AC relay outputs (K1, K2, K3) use 6.3mm spade terminals.**

**⚠️ CT Clamp connections REMOVED - now handled directly by external power meter module.**

```
┌──────────────────────────────────────────────────────────────────────────────────────────────────────┐
│                    UNIFIED LOW-VOLTAGE SCREW TERMINAL BLOCK (J26 - 18 Position)                      │
│                              Phoenix MKDS 1/18-5.08 (5.08mm pitch)                                   │
│                                    ⚠️ LOW VOLTAGE ONLY ⚠️                                            │
├──────────────────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                                      │
│  SECTION A: SWITCHES     SECTION B: ANALOG SENSORS              SECTION C: SSR OUTPUTS       SPARE  │
│  ────────────────────    ─────────────────────────────────────  ───────────────────────────  ─────  │
│                                                                                                      │
│  ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐                          │
│  │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │ 8 │ 9 │10 │11 │12 │13 │14 │15 │16 │17 │18 │                          │
│  │S1 │S1G│S2 │S2G│S3 │S4 │S4G│T1 │T1G│T2 │T2G│P5V│PGD│PSG│SR+│SR-│SR+│SR-│                          │
│  └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘                          │
│   │   │   │   │   │   │   │   │   │   │   │   │   │   │   │   │   │                                  │
│   └─S1─┘   └─S2─┘  S3  └─S4─┘   └─T1──┘   └─T2──┘ └──Pressure──┘ └SSR1─┘ └SSR2─┘                    │
│   Water    Tank   Lvl  Brew     Brew     Steam       Transducer   Brew    Steam                    │
│   Res.     Level  Prb  Handle   NTC      NTC         (YD4060)     Heater  Heater                   │
│                                                                                                      │
├──────────────────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                                      │
│  COMPLETE PIN ASSIGNMENT TABLE:                                                                      │
│  ══════════════════════════════                                                                      │
│                                                                                                      │
│  │ Pin │ Label │ Function                    │ Wire   │ Signal      │ Notes                       │ │
│  │─────│───────│─────────────────────────────│────────│─────────────│─────────────────────────────│ │
│  │  1  │ S1    │ Water Reservoir Switch      │ 22 AWG │ GPIO2       │ Digital input, active low   │ │
│  │  2  │ S1-G  │ Water Reservoir GND         │ 22 AWG │ GND         │ Switch return               │ │
│  │  3  │ S2    │ Tank Level Sensor           │ 22 AWG │ GPIO3       │ Digital input, active low   │ │
│  │  4  │ S2-G  │ Tank Level GND              │ 22 AWG │ GND         │ Sensor return               │ │
│  │  5  │ S3    │ Steam Boiler Level Probe    │ 22 AWG │ PROBE       │ Via OPA342/TLV3201→GPIO4    │ │
│  │  6  │ S4    │ Brew Handle Switch          │ 22 AWG │ GPIO5       │ Digital input, active low   │ │
│  │  7  │ S4-G  │ Brew Handle GND             │ 22 AWG │ GND         │ Switch return               │ │
│  │  8  │ T1    │ Brew NTC Signal             │ 22 AWG │ ADC0        │ To GPIO26 via divider       │ │
│  │  9  │ T1-G  │ Brew NTC GND                │ 22 AWG │ GND         │ Sensor return               │ │
│  │ 10  │ T2    │ Steam NTC Signal            │ 22 AWG │ ADC1        │ To GPIO27 via divider       │ │
│  │ 11  │ T2-G  │ Steam NTC GND               │ 22 AWG │ GND         │ Sensor return               │ │
│  │ 12  │ P-5V  │ Pressure Transducer +5V     │ 22 AWG │ +5V         │ Power for YD4060            │ │
│  │ 13  │ P-GND │ Pressure Transducer GND     │ 22 AWG │ GND         │ Sensor return               │ │
│  │ 14  │ P-SIG │ Pressure Transducer Signal  │ 22 AWG │ PRESS_SIG   │ 0.5-4.5V to ADC divider     │ │
│  │ 15  │ SSR1+ │ SSR1 Control +5V            │ 22 AWG │ +5V         │ Brew heater SSR power       │ │
│  │ 16  │ SSR1- │ SSR1 Control -              │ 22 AWG │ SSR1_NEG    │ Brew heater SSR trigger     │ │
│  │ 17  │ SSR2+ │ SSR2 Control +5V            │ 22 AWG │ +5V         │ Steam heater SSR power      │ │
│  │ 18  │ SSR2- │ SSR2 Control -              │ 22 AWG │ SSR2_NEG    │ Steam heater SSR trigger    │ │
│                                                                                                      │
│  WIRING NOTES:                                                                                       │
│  ─────────────                                                                                       │
│  • SWITCHES (Pin 1-7): N.O. switches connect between signal and adjacent GND pin                    │
│  • S3 (Pin 5): Level probe single wire, ground return via boiler body (PE connection)               │
│  • NTCs (Pin 8-11): 2-wire thermistors, polarity doesn't matter                                     │
│  • PRESSURE (Pin 12-14): 3-wire transducer: +5V (red), GND (black), Signal (yellow/white)           │
│  • SSRs (Pin 15-18): Connect to SSR DC input terminals (+5V to SSR+, SSR- to SSR DC-)               │
│                                                                                                      │
│  ⚠️ CT CLAMP: Connect directly to external power meter module (not through this PCB)                │
│                                                                                                      │
└──────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

**Screw Terminal Part Number:**

- Phoenix Contact MKDS 1/18-5.08 (18-position, 5.08mm pitch)
- Alternative: 2× Phoenix MKDS 1/9-5.08 ganged together

## 13.2 Pin Headers and JST Connectors

| Designator | Function            | Type                 | Pitch  | Notes                                     |
| ---------- | ------------------- | -------------------- | ------ | ----------------------------------------- |
| J15        | ESP32 Display       | JST-XH 8-pin         | 2.54mm | Keyed, locking (incl. WEIGHT_STOP, SPARE) |
| J16        | Service/Debug       | Pin header 4-pin     | 2.54mm | 3V3, GND, TX, RX (shared GPIO0/1)         |
| J17        | Power Meter (Univ.) | JST-XH 6-pin         | 2.54mm | 3V3, 5V, GND, RX, TX, DE/RE (TTL/RS485)   |
| J20        | Pico Socket         | 2×20 female header   | 2.54mm | Or solder direct                          |
| J23        | I2C Accessory       | Pin header 4-pin     | 2.54mm | 3V3, GND, SDA, SCL (GPIO8/9)              |
| J24        | Power Meter HV      | Screw terminal 3-pos | 5.08mm | L (fused), N, PE for external meter       |

### J24 External Power Meter HV Terminals (L, N, PE)

3-position screw terminal for easy wiring of external power meter modules (PZEM, JSY, Eastron, etc.).

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                    J24 - POWER METER HV SCREW TERMINAL (3-pos)                  │
│                          ⚠️ HIGH VOLTAGE - 220V AC ⚠️                           │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    ┌───┬───┬───┐                                                               │
│    │ L │ N │ PE│    Phoenix MKDS 1/3-5.08 (5.08mm pitch)                       │
│    └───┴───┴───┘    Or equivalent HV-rated screw terminal                      │
│      │   │   │                                                                  │
│      │   │   └─────► Protective Earth (from J1-PE bus)                         │
│      │   └─────────► Neutral (from J1-N bus)                                   │
│      └─────────────► Live FUSED (from internal L_FUSED bus, after F1)          │
│                                                                                 │
│    WIRING TO EXTERNAL POWER METER:                                             │
│    ───────────────────────────────                                             │
│                                                                                 │
│    ┌──────────────┐                    ┌─────────────────────────┐             │
│    │  J24 (PCB)   │      User wire     │  External Power Meter   │             │
│    │              │                    │  (PZEM, JSY, Eastron)   │             │
│    │  L  ─────────┼────────────────────┼──► L (Live input)       │             │
│    │  N  ─────────┼────────────────────┼──► N (Neutral input)    │             │
│    │  PE ─────────┼────────────────────┼──► PE/GND (if needed)   │             │
│    └──────────────┘                    │                         │             │
│                                        │  CT clamp clips around  │             │
│                                        │  machine's Live wire    │             │
│                                        └─────────────────────────┘             │
│                                                                                 │
│    PE USAGE:                                                                   │
│    ─────────                                                                   │
│    • PZEM-004T: PE not required (isolated plastic case)                       │
│    • JSY-MK-163T: PE not required (isolated plastic case)                     │
│    • Eastron SDM (DIN rail): Connect PE for metal enclosure grounding         │
│    • Leave unconnected if meter doesn't need it                               │
│                                                                                 │
│    ⚠️ SAFETY NOTES:                                                            │
│    ────────────────                                                            │
│    • L is FUSED (via F1) - protected from overcurrent                         │
│    • Maintain >6mm creepage from LV circuits                                   │
│    • Use 16 AWG or heavier wire for L and N                                   │
│    • Route HV wires away from LV signal wires                                 │
│                                                                                 │
│    📋 PCB PLACEMENT NOTE:                                                      │
│    ──────────────────────                                                      │
│    J24 is located in the EXISTING HV ZONE of the PCB, adjacent to the relay   │
│    contacts and L_FUSED bus that already carry mains voltage. This is NOT a   │
│    new HV section - it reuses the same traces and clearances used for K1-K3.  │
│    No additional HV measurement circuitry exists on the PCB; the external     │
│    meter module performs all voltage/current sensing internally.               │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

### Expansion via J15 Pin 8 (GPIO22)

GPIO22 is available on **J15 Pin 8 (SPARE)** for future expansion:

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                    EXPANSION VIA J15 PIN 8 (GPIO22)                             │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    GPIO22 is available on J15 (ESP32 connector) Pin 8 for expansion:          │
│                                                                                 │
│    ┌──────────────────────────────────────────────────────────────────┐       │
│    │  J15 (ESP32 8-pin JST-XH)                                        │       │
│    │  ┌───┬───┬───┬───┬───┬───┬───┬───┐                              │       │
│    │  │5V │GND│TX │RX │RUN│BOOT│WGHT│SPARE│                           │       │
│    │  └───┴───┴───┴───┴───┴───┴───┴──┬──┘                            │       │
│    │                                  │                               │       │
│    │                          GPIO22 ─┘  (Available for expansion)   │       │
│    └──────────────────────────────────────────────────────────────────┘       │
│                                                                                 │
│    ⚠️ RP2350 GPIO is NOT 5V tolerant! Max input = 3.6V.                       │
│       For 5V sensors, use voltage divider or level shifter.                   │
│                                                                                 │
│    NOTE: GPIO23-25 and GPIO29 are used internally by the Pico 2 module        │
│    and are NOT available on the 40-pin header.                                │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

### Complete SSR Wiring (Each SSR has 2 connections to the board)

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                    EXTERNAL SSR WIRING DIAGRAM                                   │
│                  (SSR1 for Brew Heater shown - SSR2 identical)                  │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│   YOUR CONTROL PCB                          EXTERNAL SSR + MACHINE WIRING        │
│   ─────────────────                         ─────────────────────────────        │
│                                                                                  │
│   ┌─────────────────┐                      ┌─────────────────┐                  │
│   │                 │    5V CONTROL        │   SSR-40DA      │                  │
│   │  ┌───────────┐  │    (LOW VOLTAGE)     │   (or similar)  │                  │
│   │  │ J26:19-20 │  │    ───────────       │                 │                  │
│   │  │  SSR1+  ──┼──┼───────────────────►──┼── DC+ (3-32V)   │                  │
│   │  │  SSR1-  ──┼──┼───────────────────►──┼── DC- (control) │                  │
│   │  └───────────┘  │                      │                 │    ┌─────────┐  │
│   │                 │                      │                 │    │  BREW   │  │
│   │  ┌───────────┐  │                      │   AC Load 1   ──┼───►│ HEATER  │  │
│   │  │ J26:17-18 │  │                      │                 │    │ 1400W   │  │
│   │  │  SSR2+  ──┼──┼───► (to SSR2)        │   AC Load 2   ◄─┼────┤         │  │
│   │  │  SSR2-  ──┼──┼───► (to SSR2)        │                 │    └─────────┘  │
│   │  └───────────┘  │                      └────────┬────────┘         ▲       │
│   │                 │                               │                  │       │
│   └─────────────────┘                               │                  │       │
│                                                     │                  │       │
│   ⚠️  MAINS POWER TO SSRs (NOT through PCB!)       │                  │       │
│   ─────────────────────────────────────────         │                  │       │
│                                                     │                  │       │
│   Machine's existing wiring:                        ▼                  │       │
│   ┌───────────────────────────────────────────────────────────────────┐│       │
│   │                                                                   ││       │
│   │   MAINS L ────────────────────────────────────► SSR AC IN (L1)   ││       │
│   │   (from wall)                                                     ││       │
│   │                                                                   ││       │
│   │   MAINS N ◄────────────────────────────────────────────────────────┘       │
│   │   (from wall)                                                              │
│   │                                                                            │
│   └────────────────────────────────────────────────────────────────────────────┘
│                                                                                  │
│   ✅ KEY DESIGN PRINCIPLE:                                                       │
│   ─────────────────────────                                                      │
│   • PCB provides ONLY low-voltage control signals (5V DC) to SSRs               │
│   • Mains power to SSRs uses EXISTING machine wiring (not through PCB)          │
│   • NO high current (heater loads) flows through the control PCB                │
│   • 10A fuse protects relay loads only (pump, valves)                           │
│                                                                                  │
│   CONNECTIONS PER SSR:                                                           │
│   ─────────────────────                                                          │
│   │ Connection      │ Source           │ Type       │ Voltage │                 │
│   │─────────────────│──────────────────│────────────│─────────│                 │
│   │ Control DC+     │ J26 Pin 15/17    │ Screw term │ 5V DC   │                 │
│   │ Control DC-     │ J26 Pin 16/18    │ Screw term │ Trigger │                 │
│   │ Mains Live In   │ Machine wiring   │ Existing   │ 220V AC │ ← NOT from PCB │
│   │ Load Output     │ SSR AC terminal  │ Existing   │ 220V AC │                 │
│   │ Neutral         │ Machine wiring   │ Existing   │ 220V AC │                 │
│                                                                                  │
└─────────────────────────────────────────────────────────────────────────────────┘
```

**JST Part Numbers:**

- JST B8B-XH-A (PCB header, 8-pin)
- JST XHP-6 (housing) with SXH-001T-P0.6 contacts (cable side)

---

# 14. Bill of Materials (BOM)

## 14.1 Integrated Circuits

| Qty | Ref | Description              | Part Number    | Package  | Notes                                      |
| --- | --- | ------------------------ | -------------- | -------- | ------------------------------------------ |
| 1   | U1  | Raspberry Pi Pico 2      | SC0942         | Module   | Or Pico 2 W (SC1632) for onboard WiFi      |
| 1   | U2  | AC/DC Converter 5V 3A    | HLK-15M05C     | Module   | Isolated, 15W/3A (alt: Mean Well IRM-20-5) |
| 1   | U3  | 3.3V Sync Buck Converter | TPS563200DDCR  | SOT-23-6 | 3A, >90% efficiency                        |
| 1   | U5  | Precision Voltage Ref    | LM4040DIM3-3.0 | SOT-23-3 | 3.0V shunt reference for ADC               |
| 1   | U6  | Rail-to-Rail Op-Amp      | OPA342UA       | SOIC-8   | Level probe oscillator (alt: OPA207)       |
| 1   | U7  | Precision Comparator     | TLV3201AIDBVR  | SOT-23-5 | Level probe detector                       |
| 1   | U8  | RS485 Transceiver        | MAX3485ESA+    | SOIC-8   | For industrial meters (alt: SP3485EN-L/TR) |
| 1   | U9  | Dual Rail-to-Rail Op-Amp | OPA2342UA      | SOIC-8   | VREF buffer + spare                        |

## 14.2 Transistors and Diodes

| Qty | Ref     | Description        | Part Number | Package | Notes                                         |
| --- | ------- | ------------------ | ----------- | ------- | --------------------------------------------- |
| 5   | Q1-Q5   | NPN Transistor     | MMBT2222A   | SOT-23  | Relay (3) + SSR (2) drivers                   |
| 3   | D1-D3   | Fast Flyback Diode | UF4007      | DO-41   | Fast recovery (75ns) for snappy relay opening |
| 6   | D10-D15 | ESD Protection     | PESD5V0S1BL | SOD-323 | Sensor inputs                                 |
| 1   | D16     | Schottky Clamp     | BAT54S      | SOT-23  | Pressure ADC overvoltage                      |
| 1   | D20     | TVS Diode          | SMBJ5.0A    | SMB     | 5V rail protection                            |
| 1   | D21     | RS485 TVS          | SM712       | SOT-23  | RS485 A/B line surge protection (-7V/+12V)    |

## 14.3 Passive Components - Resistors

| Qty | Ref             | Value | Tolerance | Package | Notes                                                         |
| --- | --------------- | ----- | --------- | ------- | ------------------------------------------------------------- |
| 1   | R1              | 3.3kΩ | 1%        | 0805    | Brew NTC pull-up (always populated)                           |
| 1   | R1A             | 1.5kΩ | 1%        | 0805    | Brew NTC parallel (via JP2, for 10kΩ NTC)                     |
| 1   | R2              | 1.2kΩ | 1%        | 0805    | Steam NTC pull-up (always populated)                          |
| 1   | R2A             | 680Ω  | 1%        | 0805    | Steam NTC parallel (via JP3, for 10kΩ NTC)                    |
| 2   | R5-R6           | 1kΩ   | 1%        | 0805    | NTC ADC series protection                                     |
| 1   | R3              | 10kΩ  | 1%        | 0805    | Pressure divider (to GND)                                     |
| 1   | R4              | 5.6kΩ | 1%        | 0805    | Pressure divider (series) - prevents saturation with 3.0V ref |
| 5   | R16-R18         | 10kΩ  | 5%        | 0805    | Switch pull-ups (Water, Tank, Brew)                           |
| 2   | R71-R72         | 10kΩ  | 5%        | 0805    | Pico RUN/BOOTSEL pull-ups                                     |
| 6   | R11-R15,R19     | 4.7kΩ | 5%        | 0805    | Driver pull-downs (RP2350 errata E9)                          |
| 1   | R73             | 4.7kΩ | 5%        | 0805    | WEIGHT_STOP pull-down (RP2350 errata E9)                      |
| 5   | R20-R22,R24-R25 | 470Ω  | 5%        | 0805    | Transistor base (3 relay + 2 SSR)                             |
| 3   | R30-R32         | 470Ω  | 5%        | 0805    | Relay Indicator LEDs (K1, K2, K3)                             |
| 2   | R34-R35         | 330Ω  | 5%        | 0805    | SSR Indicator LEDs (logic-side)                               |
| 4   | R40-R43         | 33Ω   | 5%        | 0805    | UART series (ESP32/Service)                                   |
| 1   | R44             | 33Ω   | 5%        | 0805    | J17 TX series (power meter)                                   |
| 1   | R45             | 2.2kΩ | 1%        | 0805    | J17 RX 5V→3.3V level shifter (upper divider)                  |
| 1   | R45A            | 3.3kΩ | 1%        | 0805    | J17 RX 5V→3.3V level shifter (lower divider)                  |
| 1   | R45B            | 33Ω   | 5%        | 0805    | J17 RX series (after divider)                                 |
| 1   | R7              | 1kΩ   | 1%        | 0805    | LM4040 voltage reference bias resistor                        |
| 1   | R_ISO           | 47Ω   | 1%        | 0805    | ADC VREF buffer isolation (U9A output stability)              |
| 2   | R_FB1           | 33kΩ  | 1%        | 0805    | TPS563200 feedback upper (sets 3.3V output)                   |
| 1   | R_FB2           | 10kΩ  | 1%        | 0805    | TPS563200 feedback lower (to GND)                             |
| 2   | R46-R47         | 4.7kΩ | 5%        | 0805    | I2C pull-ups (SDA, SCL)                                       |
| 1   | R48             | 330Ω  | 5%        | 0805    | Status LED                                                    |
| 1   | R49             | 100Ω  | 5%        | 0805    | Buzzer                                                        |
| 2   | R71-R72         | 10kΩ  | 5%        | 0805    | Pico RUN/BOOTSEL pull-ups (J15 Pin 5/6)                       |
| 1   | R73             | 4.7kΩ | 5%        | 0805    | WEIGHT_STOP pull-down (RP2350 errata E9)                      |
| 1   | R81             | 10kΩ  | 1%        | 0805    | Level probe oscillator feedback                               |
| 1   | R82             | 4.7kΩ | 1%        | 0805    | Wien bridge gain resistor (A_CL=3.13, ensures oscillation)    |
| 2   | R83-R84         | 10kΩ  | 1%        | 0805    | Level probe Wien bridge                                       |
| 1   | R85             | 100Ω  | 5%        | 0805    | Level probe current limit                                     |
| 1   | R86             | 10kΩ  | 5%        | 0805    | Level probe AC bias                                           |
| 2   | R87-R88         | 100kΩ | 1%        | 0805    | Level probe threshold divider                                 |
| 1   | R89             | 1MΩ   | 5%        | 0805    | Level probe comparator hysteresis                             |
| 1   | R91             | 10kΩ  | 1%        | 0805    | 5V monitor divider (upper)                                    |
| 1   | R92             | 5.6kΩ | 1%        | 0805    | 5V monitor divider (lower)                                    |
| 1   | R93             | 20kΩ  | 5%        | 0805    | RS485 failsafe bias (A line pull-up)                          |
| 1   | R94             | 20kΩ  | 5%        | 0805    | RS485 failsafe bias (B line pull-down)                        |
| 1   | R98             | 1MΩ   | 5%        | 0805    | Level probe hysteresis                                        |
| 1   | R100            | 10kΩ  | 1%        | 0805    | 5V monitor upper divider (ratiometric pressure compensation)  |
| 1   | R101            | 5.6kΩ | 1%        | 0805    | 5V monitor lower divider (ratiometric pressure compensation)  |

## 14.3a Solder Jumpers

| Qty | Ref | Type  | Function                  | Default | Notes                                       |
| --- | --- | ----- | ------------------------- | ------- | ------------------------------------------- |
| 1   | JP1 | 2-pad | Brew NTC selection        | OPEN    | OPEN=50kΩ (ECM), CLOSE=10kΩ (Rocket/Gaggia) |
| 1   | JP2 | 2-pad | Steam NTC selection       | OPEN    | OPEN=50kΩ (ECM), CLOSE=10kΩ (Rocket/Gaggia) |
| 1   | JP3 | 2-pad | Power meter RX level      | OPEN    | OPEN=5V meters, CLOSE=3.3V meters           |
| 1   | JP4 | 3-pad | Power meter interface sel | 1-2     | 1-2=RS485, 2-3=TTL                          |

**Solder Jumper Implementation:**

**Implementation:**

- **2-pad jumpers (JP1-JP3):** Two pads with ~0.5mm gap. Apply solder blob to bridge.
- **3-pad jumper (JP4):** Three pads in a row. Bridge center pad to left or right.

**Configuration Guide:**

- JP1/JP2: Set based on installed NTC sensor type (50kΩ or 10kΩ)
- JP3: Set based on power meter logic level (5V or 3.3V)
- JP4: Set based on power meter interface (RS485 or TTL UART)

## 14.4 Passive Components - Capacitors

| Qty | Ref     | Value    | Voltage | Package      | Notes                                                        |
| --- | ------- | -------- | ------- | ------------ | ------------------------------------------------------------ |
| 1   | C1      | 100nF X2 | 275V AC | Radial       | Mains EMI filter                                             |
| 1   | C2      | 470µF    | 6.3V    | Radial 6.3mm | 5V bulk, **Polymer** (low ESR, long life in hot environment) |
| 1   | C3      | 22µF     | 25V     | 1206 Ceramic | Buck converter input cap (X5R)                               |
| 2   | C4,C4A  | 22µF     | 10V     | 1206 Ceramic | Buck converter output caps (X5R, parallel)                   |
| 1   | C5      | 100nF    | 25V     | 0805 Ceramic | 3.3V decoupling                                              |
| 1   | C7      | 22µF     | 10V     | 1206 Ceramic | ADC reference bulk cap                                       |
| 1   | C7A     | 100nF    | 25V     | 0805 Ceramic | ADC reference HF decoupling                                  |
| 12  | C10-C21 | 100nF    | 25V     | 0805         | Decoupling (general)                                         |
| 1   | C60     | 100nF    | 25V     | 0805         | OPA342 VCC decoupling                                        |
| 2   | C61-C62 | 10nF     | 50V     | 0805         | Level probe Wien bridge timing (~1.6kHz for probe longevity) |
| 1   | C63     | 100nF    | 25V     | 0805         | TLV3201 VCC decoupling                                       |
| 1   | C64     | 1µF      | 25V     | 0805         | Level probe AC coupling                                      |
| 1   | C65     | 100nF    | 25V     | 0805         | Level probe sense filter                                     |
| 4   | C30-C33 | 100pF    | 50V     | 0603         | UART/ADC filter                                              |
| 1   | C70     | 100nF    | 25V     | 0805         | RS485 transceiver (U8) decoupling                            |
| 1   | C80     | 100nF    | 25V     | 0805         | OPA2342 (U9) VCC decoupling                                  |
| 1   | C81     | 100nF    | 25V     | 0805         | 5V monitor filter (ratiometric pressure compensation)        |

## 14.4a Inductors

| Qty | Ref | Value | Saturation | DCR    | Package | Notes                                                                         |
| --- | --- | ----- | ---------- | ------ | ------- | ----------------------------------------------------------------------------- |
| 1   | L1  | 2.2µH | 3A min     | <100mΩ | 1210    | Buck inductor (Murata LQH32CN2R2M23) - 2.2µH per TPS563200 datasheet for 3.3V |

## 14.5 Electromechanical

| Qty | Ref     | Description          | Part Number             | Notes                                      |
| --- | ------- | -------------------- | ----------------------- | ------------------------------------------ |
| 2   | K1,K3   | Relay 5V 3A SPST-NO  | Panasonic APAN3105      | Slim 5mm, K1=Indicator lamp, K3=Solenoid   |
| 1   | K2      | Relay 5V 16A SPST-NO | Omron G5LE-1A4-E DC5    | Standard size, Pump motor (**-E** = 16A!)  |
| 1   | F1      | Fuse 10A + Holder    | 0218010.MXP + 01000056Z | 5×20mm slow, PCB mount with cover (relays) |
| 1   | F2      | Fuse 2A + Holder     | 0218002.MXP + 01000056Z | 5×20mm slow, HLK module fusing hierarchy   |
| 1   | RV1     | Varistor 275V        | S14K275                 | 14mm disc, mains surge protection          |
| 2   | RV2-RV3 | Varistor 275V        | S10K275                 | 10mm disc, K2/K3 arc suppression           |
| 2   | SW1-SW2 | Tactile Switch       | EVQP7A01P               | SMD 6×6mm                                  |
| 1   | BZ1     | Passive Buzzer       | CEM-1203(42)            | 12mm                                       |

## 14.6 LEDs

| Qty | Ref       | Description             | Color  | Package |
| --- | --------- | ----------------------- | ------ | ------- |
| 3   | LED1-LED3 | Relay Indicator (K1-K3) | Green  | 0805    |
| 2   | LED5-LED6 | SSR Indicator           | Orange | 0805    |
| 1   | LED7      | Status                  | Green  | 0805    |

## 14.7 Connectors

| Qty | Ref     | Description               | Part Number                | Notes                                          |
| --- | ------- | ------------------------- | -------------------------- | ---------------------------------------------- |
| 6   | J1-J4   | 6.3mm Spade Terminal      | Keystone 1285              | Mains (L,N,PE) + 220V relay outputs (K1,K2,K3) |
| 1   | **J26** | **Screw Terminal 18-pos** | **Phoenix MKDS 1/18-5.08** | **ALL LV connections - see 13.1a**             |
| 1   | J15     | JST-XH 8-pin Header       | B8B-XH-A                   | ESP32 display + brew-by-weight                 |
| 1   | J16     | Pin Header 1×4 2.54mm     | -                          | Service/debug (shared with J15)                |
| 1   | J17     | JST-XH 6-pin Header       | B6B-XH-A                   | Power Meter: 3V3, 5V, GND, RX, TX, DE/RE       |
| 1   | J20     | Female Header 2×20        | -                          | Pico socket                                    |
| 1   | J23     | Pin Header 1×4 2.54mm     | -                          | I2C accessory port                             |
| 1   | J24     | Screw Terminal 3-pos      | Phoenix MKDS 1/3-5.08      | Power Meter HV: L (fused), N, PE               |

## 14.8 User-Supplied Components (NOT included with PCB)

The following components are **NOT** included with the PCB and must be sourced by the user:

| Component                   | Notes                                              |
| --------------------------- | -------------------------------------------------- |
| Raspberry Pi Pico 2         | SC0942 or Pico 2 W (SC1632) for onboard WiFi       |
| JST-XH 8-pin Cable 50cm     | For ESP32 display connection (J15)                 |
| JST-XH 6-pin Cable 30-50cm  | For power meter connection (J17)                   |
| ESP32 Display Module        | User purchases separately                          |
| SSR Relays                  | Already exist on machine                           |
| External Power Meter Module | PZEM-004T, JSY-MK-163T, Eastron SDM, or compatible |

## 14.9 External Sensors BOM (User-Supplied)

**⚠️ SENSOR COMPATIBILITY: The PCB is designed for specific sensor types. Order accordingly.**

| Sensor              | Type Required                    | Constraint                              | Recommended Part                     |
| ------------------- | -------------------------------- | --------------------------------------- | ------------------------------------ |
| Brew/Steam NTC      | **50kΩ @ 25°C** (B25/85 ≈ 3950K) | R1/R2 optimized for 50kΩ (see 7.1)      | ECM OEM sensor or Semitec 503ET-4    |
| Pressure Transducer | **0.5-4.5V ratiometric**, 3-wire | Circuit is voltage divider (not 4-20mA) | YD4060 (0-16 bar), G1/4" or 1/8" NPT |

### NTC Sensor Selection by Machine Brand (Jumper Selectable)

| Machine Brand     | NTC Value @ 25°C | JP2 (Brew) | JP3 (Steam) | Effective R   |
| ----------------- | ---------------- | ---------- | ----------- | ------------- |
| **ECM, Profitec** | 50kΩ             | **OPEN**   | **OPEN**    | 3.3kΩ / 1.2kΩ |
| Rocket, Rancilio  | 10kΩ             | **CLOSE**  | **CLOSE**   | ~1kΩ / ~430Ω  |
| Gaggia Classic    | 10kΩ             | **CLOSE**  | **CLOSE**   | ~1kΩ / ~430Ω  |
| Lelit (50kΩ)      | 50kΩ             | OPEN       | OPEN        | 3.3kΩ / 1.2kΩ |

**Configuration:** Use **solder jumpers JP2/JP3** to switch between NTC types - no resistor swapping required.

### Pressure Transducer Specifications

| Parameter          | Requirement                | Notes                                    |
| ------------------ | -------------------------- | ---------------------------------------- |
| Output Type        | **0.5-4.5V ratiometric**   | NOT 4-20mA (different circuit required)  |
| Supply Voltage     | 5V DC                      | Powered from PCB                         |
| Range              | 0-16 bar (0-232 psi)       | Suitable for espresso (9-12 bar brew)    |
| Thread             | G1/4" or 1/8" NPT          | Must match machine's hydraulic T-fitting |
| Broken Wire Detect | 0.5V = 0 bar, 0.0V = fault | Built into circuit design                |

**EXTERNAL POWER METER INSTALLATION:**

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                   EXTERNAL POWER METER SETUP                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   SUPPORTED MODULES (Off-Board, user mounts separately):                    │
│   ────────────────────────────────────────────────────                       │
│   • PZEM-004T-100A (TTL UART, 9600 baud, with CT clamp)                     │
│   • JSY-MK-163T (TTL UART, 4800 baud, bi-directional)                       │
│   • JSY-MK-194T (TTL UART, 4800 baud, dual-channel)                         │
│   • Eastron SDM120/230 (RS485, DIN rail mount)                              │
│   • Other Modbus-RTU compatible energy meters                               │
│                                                                              │
│   CONNECTION TO CONTROL PCB:                                                 │
│   ──────────────────────────                                                 │
│   ┌─────────────────┐          ┌─────────────────────────┐                  │
│   │  Control PCB    │   JST    │  External Power Meter   │                  │
│   │                 │  Cable   │  (PZEM, JSY, etc.)      │                  │
│   │  ┌───────────┐  │ (4-wire) │                         │                  │
│   │  │    J17    │──┼─────────►│  LV UART/RS485 input    │                  │
│   │  │  JST-XH   │  │          │  5V, GND, TX, RX        │                  │
│   │  │   6-pin   │  │          │                         │                  │
│   │  └───────────┘  │          │  HV terminals (L, N, CT)│◄── User wires   │
│   └─────────────────┘          │  directly to mains      │    mains here   │
│                                └─────────────────────────┘                  │
│                                                                              │
│   ⚠️ NO HV MEASUREMENT CIRCUITRY on control PCB - meter handles sensing     │
│   ⚠️ J24 routes L/N/PE to meter; CT clamp wires directly to meter module    │
│   ⚠️ CT clamp clips around machine's Live wire, connects to meter           │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Cable Notes:**

- JST-XH 8-pin (J15): 5V, GND, TX, RX, RUN, BOOT, WEIGHT_STOP, SPARE - 50cm for ESP32
- JST-XH 6-pin (J17): 3V3, 5V, GND, RX, TX, DE/RE - 30-50cm for power meter

---

# 15. Testing & Validation

**For detailed test procedures, see:** `docs/hardware/Test_Procedures.md`

## 15.1 Test Points Required on PCB

| TP# | Signal   | Location        | Purpose          |
| --- | -------- | --------------- | ---------------- |
| TP1 | GND      | Near Pico       | Ground reference |
| TP2 | 5V       | Near LDO input  | Power rail check |
| TP3 | 3.3V     | Near Pico       | Power rail check |
| TP4 | 3.3V_A   | Near ADC        | Analog reference |
| TP5 | ADC0     | Near Pico       | Brew NTC signal  |
| TP6 | ADC1     | Near Pico       | Steam NTC signal |
| TP7 | ADC2     | Near Pico       | Pressure signal  |
| TP8 | UART0_TX | Near ESP32 conn | Debug probe      |
| TP9 | UART0_RX | Near ESP32 conn | Debug probe      |

---

# 16. Deliverables

## 16.1 Design Documentation

| Document         | Format       | Description                                     |
| ---------------- | ------------ | ----------------------------------------------- |
| Schematic        | PDF + Native | Complete circuit schematic with all connections |
| BOM              | Excel/CSV    | All components with Manufacturer Part Numbers   |
| PCB Layout       | Gerber X2    | Manufacturing files for board fabrication       |
| Drill File       | Excellon     | NC drill data                                   |
| Pick & Place     | CSV          | Component positions for SMT assembly            |
| Assembly Drawing | PDF          | Component placement, polarity, notes            |
| 3D Model         | STEP         | Board with components for enclosure design      |

## 16.2 Gerber File List

| Layer              | Filename         | Description            |
| ------------------ | ---------------- | ---------------------- |
| Top Copper         | \*-F_Cu.gtl      | Front copper layer     |
| Bottom Copper      | \*-B_Cu.gbl      | Back copper layer      |
| Top Solder Mask    | \*-F_Mask.gts    | Front solder mask      |
| Bottom Solder Mask | \*-B_Mask.gbs    | Back solder mask       |
| Top Silkscreen     | \*-F_SilkS.gto   | Front legend           |
| Bottom Silkscreen  | \*-B_SilkS.gbo   | Back legend            |
| Top Paste          | \*-F_Paste.gtp   | SMD stencil (optional) |
| Board Outline      | \*-Edge_Cuts.gm1 | PCB boundary           |
| Drill File         | \*.drl           | Plated through-holes   |
| Drill Map          | \*-drl_map.pdf   | Drill visualization    |

## 16.3 PCB Fabrication Specifications

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                       PCB FABRICATION NOTES                                     │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│    Material:           FR-4 TG130 (or higher)                                  │
│    Layers:             2                                                        │
│    Dimensions:         130mm × 80mm                                            │
│    Thickness:          1.6mm ±10%                                              │
│    Copper Weight:      2oz (70µm) both layers                                  │
│    Min Trace/Space:    0.2mm / 0.2mm (8mil/8mil)                              │
│    Min Drill:          0.3mm                                                   │
│    Mounting Holes:     4× M3 (3.2mm), 5mm from edges                          │
│    Surface Finish:     ENIG (preferred) or HASL Lead-Free                     │
│    Solder Mask:        Green (both sides)                                      │
│    Silkscreen:         White (both sides)                                      │
│    UL Marking:         Required (UL 94V-0)                                     │
│    IPC Class:          Class 2 minimum                                         │
│    Routed Slots:       Yes (isolation slots as marked)                         │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

## 16.4 Assembly Notes

1. **SMT components first**, then through-hole
2. **Pico 2 module**: Solder directly or use socket (socketed preferred for prototype)
3. **HLK-15M05C**: Solder last, check orientation (L, N marking)
4. **Relays**: Check coil polarity if polarized
5. **Electrolytic capacitors**: Observe polarity markings
6. **ESD handling**: Use proper ESD precautions for Pico and ICs
7. **Conformal coating**: See detailed guidance below
8. **Pico retention**: After final testing, apply small dab of RTV silicone at module corners to prevent vibration-induced creep from pump operation

### Conformal Coating Guidance (Recommended for Production)

The espresso machine environment (heat, humidity, coffee vapor) benefits from conformal coating:

| Area                            | Coating        | Notes                                         |
| ------------------------------- | -------------- | --------------------------------------------- |
| HV section (relays, MOVs, fuse) | ✅ Recommended | Prevents tracking from moisture/contamination |
| Level probe trace area          | ✅ Recommended | High-impedance, sensitive to moisture         |
| LV analog section               | ✅ Recommended | Protects ADC inputs from drift                |
| Connectors (J15, J17, J26)      | ❌ Mask off    | Must remain solderable/conductive             |
| Pico module socket              | ❌ Mask off    | Module must be removable                      |
| Relay contacts                  | ❌ Mask off    | Internal, already sealed                      |
| Test points (TP1-TP6)           | ❌ Mask off    | Must remain accessible                        |

**Coating type:** Acrylic (MG Chemicals 419D) or silicone-based. Apply after all testing complete.

## 16.5 Firmware Interface Summary

**For firmware implementation details, see:** `docs/pico/` directory

Hardware interfaces the firmware must support:

- UART0 (GPIO0/1): ESP32 communication @ 115200 baud
- UART1 (GPIO6/7): Power meter @ configurable baud (9600/4800/2400)
- SPI0 (GPIO16-18): Reserved for future expansion
- ADC (GPIO26-28): NTC thermistors, pressure transducer
- GPIO20: RS485 DE/RE direction control

---

# 17. Design Summary

## 17.1 Key Design Decisions

| Item                 | Decision                                                |
| -------------------- | ------------------------------------------------------- |
| Power Metering       | External modules: J17 (LV comm) + J24 (HV power L/N/PE) |
| Pressure Transducers | J26 Pin 14-16 (0.5-4.5V amplified output)               |
| Pico Mounting        | Socket (2×20 female header) for easy replacement        |
| Level Probe          | OPA342 + TLV3201 on-board AC sensing                    |

## 17.2 Critical Design Points

1. **Only relay-switched loads flow through control board** (pump, valves ~6A max)
2. **Heater power does NOT flow through PCB** (external SSRs connect directly to mains)
3. **Power metering HV does NOT flow through PCB** (external modules handle their own mains)
4. **Fused live bus** (10A) feeds relay COMs only
5. **6mm creepage/clearance** required between HV and LV sections
6. **MH1 = PE star point** (PTH), MH2-4 = NPTH (isolated)

## 17.3 EDA Tool Requirements

**Preferred:** KiCad 7.0+ (open source)

Deliver: Native project files, PDF schematic, Gerber files (Section 16.2), STEP 3D model

---

# Appendix A: Reference Documents

**Safety Standards:** PCB creepage/clearance per IEC 60950-1 / IEC 62368-1. Full machine compliance to IEC 60335-x (household appliances) is at system level.

**Key Datasheets:** Raspberry Pi Pico 2, RP2350, OPA342, TLV3201, MAX3485

**Machine Reference:** ECM Synchronika Service Manual, Parts Diagram

---

**Revision History:** See `CHANGELOG.md`

---

_End of Document_
