# ECM Synchronika Custom Control Board

## Production Design Specification v2.28

**Target:** Plug & play replacement for GICAR control board and PID controller  
**Status:** Draft specification - Ready for review and prototype

---

## Quick Navigation

| Section                                       | Description                                                  |
| --------------------------------------------- | ------------------------------------------------------------ |
| [Overview](spec/01-Overview.md)               | System architecture, design goals, electrical specifications |
| [GPIO Allocation](spec/02-GPIO-Allocation.md) | Complete Pico 2 pin mapping, RP2350 errata notes             |
| [Power Supply](spec/03-Power-Supply.md)       | AC/DC isolation, 3.3V buck converter, ADC reference          |
| [Outputs](spec/04-Outputs.md)                 | Relay drivers, SSR triggers, indicator LEDs                  |
| [Analog Inputs](spec/05-Analog-Inputs.md)     | NTC thermistors, pressure transducer, level probe            |
| [Connectors](spec/06-Connectors.md)           | J1-J26 pinouts, wiring diagrams                              |
| [BOM](spec/07-BOM.md)                         | Complete bill of materials                                   |
| [PCB Layout](spec/08-PCB-Layout.md)           | Layout guidelines, trace widths, zones                       |
| [Safety](spec/09-Safety.md)                   | Protection circuits, compliance, thermal                     |

---

## Related Documentation

| Document                                                       | Purpose                                         |
| -------------------------------------------------------------- | ----------------------------------------------- |
| [Schematic Reference](schematics/Schematic_Reference.md)       | Detailed circuit diagrams with ASCII schematics |
| [Component Reference](schematics/Component_Reference_Guide.md) | Component numbering and cross-reference         |
| [Netlist](schematics/netlist.csv)                              | Machine-readable net connections                |
| [Test Procedures](Test_Procedures.md)                          | Validation and commissioning tests              |
| [ESP32 Wiring](ESP32_Wiring.md)                                | Display module integration                      |
| [Changelog](CHANGELOG.md)                                      | Design revision history                         |

---

## Design Summary

### Key Specifications

| Parameter     | Value                             |
| ------------- | --------------------------------- |
| Input Voltage | 100-240V AC, 50/60Hz              |
| Output Rails  | 5V DC (3A), 3.3V DC (500mA)       |
| Isolation     | 3000V AC (reinforced)             |
| PCB Size      | **80mm × 80mm** (target), 2-layer |
| MCU           | Raspberry Pi Pico 2 (RP2350)      |

### Interfaces

| Interface       | Purpose                                            |
| --------------- | -------------------------------------------------- |
| 3× Relays       | Lamp (3A), Pump (16A), Solenoid (3A)               |
| 2× SSR Triggers | Brew heater, Steam heater (external 40A SSRs)      |
| 3× ADC          | Brew NTC, Steam NTC, Pressure transducer           |
| 4× Digital In   | Water switch, Tank level, Brew handle, Steam level |
| UART0           | ESP32 display module                               |
| UART1           | Power meter (TTL/RS485)                            |
| I2C             | Accessory expansion                                |

### Critical Design Points

1. **Only relay-switched loads flow through control board** (pump, valves ~6A max)
2. **Heater power does NOT flow through PCB** (external SSRs connect directly to mains)
3. **Power metering HV does NOT flow through PCB** (external modules handle their own mains)
4. **Fused live bus** (10A) feeds relay COMs only
5. **6mm creepage/clearance** required between HV and LV sections
6. **All connectors on bottom edge** (enclosure opens from bottom only to prevent water ingress)

---

## Connector Quick Reference

### J26 - Unified Low-Voltage Terminal (18-pos)

```
┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
│ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │ 8 │ 9 │10 │11 │12 │13 │14 │15 │16 │17 │18 │
│S1 │GND│S2 │GND│S3 │S4 │GND│T1 │GND│T2 │GND│5V │GND│PSG│5V │SR-│5V │SR-│
└───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘
 │       │       │   │       │       │       └─Press─┘ └SSR1─┘ └SSR2─┘
 Water   Tank   Lvl Brew    Brew    Steam
 Switch  Level  Prb Handle  NTC     NTC
```

### High-Voltage (6.3mm Spade)

| Terminal          | Function       |
| ----------------- | -------------- |
| J1-L, J1-N, J1-PE | Mains input    |
| J2-NO             | Indicator lamp |
| J3-NO             | Pump           |
| J4-NO             | Solenoid       |

---

## GPIO Summary

| GPIO  | Function         | Direction |
| ----- | ---------------- | --------- |
| 0-1   | UART0 (ESP32)    | TX/RX     |
| 2-5   | Switches         | Input     |
| 6-7   | UART1 (Meter)    | TX/RX     |
| 8-9   | I2C              | SDA/SCL   |
| 10-12 | Relay drivers    | Output    |
| 13-14 | SSR drivers      | Output    |
| 15    | Status LED       | Output    |
| 16-18 | **SPARE** (SPI0) | -         |
| 19    | Buzzer           | PWM       |
| 20    | RS485 DE/RE      | Output    |
| 21    | WEIGHT_STOP      | Input     |
| 22    | SPARE            | -         |
| 26-28 | ADC0-2           | Analog    |

---

## Revision History

See [CHANGELOG.md](CHANGELOG.md) for detailed version history.

| Version | Date     | Changes                                                        |
| ------- | -------- | -------------------------------------------------------------- |
| 2.28    | Dec 2025 | RP2350 Engineering Verification ECOs (5V tolerance, bulk cap)  |
| 2.27    | Dec 2025 | Schematic diagram clarifications, netlist cleanup              |
| 2.24.3  | Dec 2025 | Removed thermocouple, fixed F2 fuse, netlist validation        |

---

_For detailed circuit information, see the individual specification files in the `spec/` directory._
