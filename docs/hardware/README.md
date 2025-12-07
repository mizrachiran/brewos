# Hardware Documentation

Custom control PCB for ECM Synchronika dual-boiler espresso machine.

**Current Version:** 2.24.3 (December 2025)

---

## 📋 Document Index

### Main Specification

| Document | Description |
|----------|-------------|
| [**Specification.md**](Specification.md) | **Start here** - Overview and navigation to all sections |

### Detailed Specifications (`spec/` folder)

| Document | Description |
|----------|-------------|
| [Overview](spec/01-Overview.md) | System architecture, design goals, electrical specs |
| [GPIO Allocation](spec/02-GPIO-Allocation.md) | Complete Pico 2 pin mapping, RP2350 errata |
| [Power Supply](spec/03-Power-Supply.md) | AC/DC isolation, buck converter, ADC reference |
| [Outputs](spec/04-Outputs.md) | Relay drivers, SSR triggers, indicator LEDs |
| [Analog Inputs](spec/05-Analog-Inputs.md) | NTC thermistors, pressure transducer |
| [Connectors](spec/06-Connectors.md) | J1-J26 pinouts, wiring diagrams |
| [BOM](spec/07-BOM.md) | Complete bill of materials |
| [PCB Layout](spec/08-PCB-Layout.md) | Layout guidelines, trace widths, zones |
| [Safety](spec/09-Safety.md) | Protection circuits, compliance |

### Schematics & Reference

| Document | Description |
|----------|-------------|
| [Schematic Reference](schematics/Schematic_Reference.md) | Detailed ASCII circuit schematics |
| [Component Reference](schematics/Component_Reference_Guide.md) | Component numbering and cross-reference |
| [netlist.csv](schematics/netlist.csv) | Machine-readable net connections |

### Testing & Support

| Document | Description |
|----------|-------------|
| [Test Procedures](Test_Procedures.md) | Manufacturing test and validation |
| [ESP32 Wiring](ESP32_Wiring.md) | Display module integration |
| [CHANGELOG](CHANGELOG.md) | Version history and design changes |

---

## 🎯 Project Overview

### What This Board Does

Plug & play replacement for the factory GICAR control board and PID controller:

- **Temperature Control:** Dual PID for brew and steam boilers (NTC sensors)
- **Pressure Monitoring:** Real-time brew pressure via 0.5-4.5V transducer
- **Power Metering:** External module support (PZEM, JSY, Eastron)
- **Smart Features:** Brew-by-weight, shot timers, profiles via ESP32 display
- **Connectivity:** WiFi, MQTT, OTA updates via ESP32

### Key Design Principles

| Principle       | Implementation                                     |
| --------------- | -------------------------------------------------- |
| **Plug & Play** | Uses original machine connectors (6.3mm spades)    |
| **Reversible**  | No permanent modifications to machine              |
| **Safe**        | HV isolated from control circuits, proper creepage |
| **Universal**   | 100-240V AC, 50/60Hz worldwide                     |

---

## 🔌 Connector Quick Reference

### High Voltage (220V AC)

| Connector | Type                 | Function                            |
| --------- | -------------------- | ----------------------------------- |
| J1        | 6.3mm Spade          | Mains input (L, N, PE)              |
| J2-J4     | 6.3mm Spade          | Relay outputs (LED, Pump, Solenoid) |
| J24       | Screw Terminal 3-pos | Power meter HV (L fused, N, PE)     |

### Low Voltage (3.3V/5V)

| Connector | Type                  | Function                                           |
| --------- | --------------------- | -------------------------------------------------- |
| J26       | Screw Terminal 18-pos | All sensors & SSR control                          |
| J15       | JST-XH 8-pin          | ESP32 display + brew-by-weight + expansion (Pin 8) |
| J17       | JST-XH 6-pin          | Power meter UART/RS485                             |

---

## ⚙️ Configuration Jumpers

| Jumper  | Default | Function                                               |
| ------- | ------- | ------------------------------------------------------ |
| **JP1** | OPEN    | Brew NTC: OPEN=50kΩ (ECM), CLOSE=10kΩ (Rocket/Gaggia)  |
| **JP2** | OPEN    | Steam NTC: OPEN=50kΩ (ECM), CLOSE=10kΩ (Rocket/Gaggia) |
| **JP3** | OPEN    | Power meter voltage: OPEN=5V, CLOSE=3.3V               |
| **JP4** | 1-2     | Power meter type: 1-2=RS485, 2-3=TTL                   |

**See:** [Component Reference Guide](schematics/Component_Reference_Guide.md) for complete component listings.

---

## 🛒 What to Order

### PCB Components (on board)

See [BOM](spec/07-BOM.md) for complete bill of materials.

### User-Supplied (external)

| Item            | Specification                | Notes                          |
| --------------- | ---------------------------- | ------------------------------ |
| NTC Sensors     | 50kΩ @ 25°C (or 10kΩ)        | Machine-dependent, see JP2/JP3 |
| Pressure Sensor | **0.5-4.5V ratiometric**     | G1/4" or 1/8" NPT thread       |
| Power Meter     | PZEM-004T, JSY-MK-163T, etc. | External module with CT clamp  |
| SSRs            | 40A solid state relay × 2    | For brew and steam heaters     |

---

## 🔧 Machine Compatibility

### Tested

- ✅ ECM Synchronika (primary target)

### Should Work (same NTC config)

- ECM Mechanika, Profitec Pro 700/800

### Requires JP1/JP2 Change

- Rocket Appartamento/Giotto (10kΩ NTC)
- Rancilio Silvia (10kΩ NTC)
- Gaggia Classic (10kΩ NTC)

---

## 📁 Related Documentation

- **Firmware:** `docs/pico/` - Pico firmware and power metering integration
- **Web Interface:** `src/web/` - React dashboard and settings
- **ESP32 Display:** `src/esp32/` - Display module firmware

---

## ⚠️ Safety Warnings

1. **Mains Voltage:** This board handles 220V AC. Only qualified personnel should install.
2. **Isolation:** Maintain >6mm creepage between HV and LV circuits.
3. **Grounding:** PE must be connected to machine chassis via MH1 mounting hole.
4. **Fusing:** 10A fuse (F1) protects relay loads. Heaters use machine's existing fusing.

---

_For detailed specifications, start with [Specification.md](Specification.md)._
