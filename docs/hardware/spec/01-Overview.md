# System Overview

## Project Description

This specification defines a custom control PCB to replace the factory GICAR control board and PID controller in an ECM Synchronika dual-boiler espresso machine. The design must be:

- **Plug & Play**: Direct replacement using existing machine wiring
- **Reversible**: Original equipment can be reinstalled without modification
- **Universal Voltage**: Support 100-240V AC, 50/60Hz worldwide operation
- **Production Ready**: Suitable for small-batch or volume manufacturing

## Design Goals

| Requirement         | Description                                        |
| ------------------- | -------------------------------------------------- |
| Temperature Control | Dual PID control for brew and steam boilers        |
| Pressure Monitoring | Real-time pressure display and profiling           |
| Safety Interlocks   | Water level, over-temperature, watchdog protection |
| Connectivity        | ESP32 display module for WiFi, MQTT, Web API       |
| Power Monitoring    | Total machine power consumption metering           |
| User Feedback       | Status LEDs, buzzer for alerts                     |
| Serviceability      | Accessible test points                             |

## System Architecture

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
│   ┌─────────────────┐     │  │       ▼          ║   │   RP2354     │   │   │    │
│   │  POWER METER    │     │  │    5V Rail ──────╫──►│  Microcontroller │   │    │
│   │  (External)     │◄────│  │       │          ║   │   (QFN-60)   │   │   │    │
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

## Electrical Specifications

### Input Power

| Parameter           | Specification                                                                   |
| ------------------- | ------------------------------------------------------------------------------- |
| Input Voltage       | 100-240V AC ±10%                                                                |
| Frequency           | 50/60 Hz                                                                        |
| Maximum Current     | 16A (total machine load through relays)                                         |
| Power Factor        | >0.9 (machine dependent)                                                        |
| Inrush Current      | Limited by machine's existing protection                                        |
| Transient Tolerance | ±1kV differential, ±2kV common-mode (IEC 61000-4-5), protected by MOV (S14K275) |

### Output Power Rails

| Rail    | Voltage  | Current Capacity | Source                | Purpose                                                      |
| ------- | -------- | ---------------- | --------------------- | ------------------------------------------------------------ |
| 5V DC   | 5.0V ±5% | **3A minimum**   | Isolated AC/DC module | RP2354 VSYS, relays, ESP32, SSR drivers                      |
| 3.3V DC | 3.3V ±3% | 500mA minimum    | Buck from 5V          | RP2354 IOVDD, sensors, logic                                 |
| 1.1V DC | 1.1V ±3% | 100mA minimum    | External LDO (U4)     | RP2354 DVDD (core voltage, external LDO for noise reduction) |

### Isolation Requirements

| Boundary              | Isolation Type | Requirement                            |
| --------------------- | -------------- | -------------------------------------- |
| Mains → 5V DC         | Reinforced     | 3000V AC for 1 minute                  |
| Relay Contacts → Coil | Basic          | 2500V AC                               |
| Power Meter → Logic   | Functional     | Via opto-isolated UART in meter module |

### Environmental

| Parameter             | Specification                 |
| --------------------- | ----------------------------- |
| Operating Temperature | 0°C to +50°C                  |
| Storage Temperature   | -20°C to +70°C                |
| Humidity              | 20% to 90% RH, non-condensing |
| Altitude              | Up to 2000m                   |

## Component Summary

### Inputs (Sensors & Switches)

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

- **NTC:** Default configured for 50kΩ @ 25°C. Use JP2/JP3 solder jumpers to switch to 10kΩ (see [Analog Inputs](04-Analog-Inputs.md)).
- **Pressure:** 0.5-4.5V ratiometric only. Cannot use 4-20mA current loop sensors.

### Outputs (Actuators)

| ID   | Component              | Load Rating         | Control             | Connection       |
| ---- | ---------------------- | ------------------- | ------------------- | ---------------- |
| K1   | Mains Indicator Lamp   | 100-240V AC, ≤100mA | Onboard Relay (3A)  | J2 (6.3mm spade) |
| K2   | Pump Motor             | 100-240V AC, 65W    | Onboard Relay (16A) | J3 (6.3mm spade) |
| K3   | Solenoid Valve (3-way) | 100-240V AC, 15W    | Onboard Relay (3A)  | J4 (6.3mm spade) |
| SSR1 | Brew Boiler Heater     | 100-240V AC, 1400W  | External SSR 40A    | J26 Pin 15-16    |
| SSR2 | Steam Boiler Heater    | 100-240V AC, 1400W  | External SSR 40A    | J26 Pin 17-18    |

### Communication Interfaces

| Interface | Type       | Purpose                               | Connector |
| --------- | ---------- | ------------------------------------- | --------- |
| ESP32     | UART0      | Display module, WiFi, user interface  | J15       |
| Meter     | UART/RS485 | External power meter communication    | J17       |
| USB       | USB 2.0    | Programming, debugging, serial logging | J_USB     |

### User Interface (Onboard)

| Component    | Type           | Function              |
| ------------ | -------------- | --------------------- |
| Status LED   | Green 0805 SMD | System status, errors |
| Buzzer       | Passive Piezo  | Audible alerts        |
| Reset Button | Tactile 6x6mm  | System reset          |
| Boot Button  | Tactile 6x6mm  | Bootloader entry      |
