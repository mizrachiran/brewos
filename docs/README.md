# BrewOS Documentation

## Overview

BrewOS is an open-source espresso machine controller with:

- **Pico (RP2040)** - Real-time machine control
- **ESP32-S3** - Connectivity, UI, and smart features
- **Web Interface** - Modern React dashboard
- **Cloud Service** - Remote access from anywhere

## Documentation Structure

```
docs/
├── Architecture.md        # System overview with cloud
├── Compatibility.md       # Validated machines list
├── pico/                  # Pico firmware (real-time control)
│   ├── README.md          # Pico overview
│   ├── Architecture.md
│   ├── Implementation_Plan.md
│   ├── Machine_Configurations.md
│   ├── Power_Metering.md  # Hardware power meter driver
│   ├── Debugging.md
│   └── features/          # Cleaning, water, shot timer, errors
├── esp32/                 # ESP32 firmware (connectivity & UI)
│   ├── README.md          # ESP32 overview
│   ├── Implementation_Plan.md
│   ├── UI_Design.md
│   ├── Simulator.md
│   ├── State_Management.md
│   ├── features/          # Schedules, eco mode, statistics
│   └── integrations/      # MQTT, BLE scales, brew-by-weight, API
├── web/                   # Web interface (React dashboard)
│   ├── README.md          # Web development guide
│   ├── PWA.md
│   ├── Push_Notifications.md
│   ├── Power_Metering.md  # Power monitoring user guide
│   └── WebSocket_Protocol.md
├── cloud/                 # Cloud service (remote access)
│   ├── README.md          # Cloud architecture
│   ├── Deployment.md
│   ├── ESP32_Integration.md
│   ├── Pairing_and_Sharing.md
│   └── Push_Notifications.md
├── shared/                # Shared documentation
│   ├── Communication_Protocol.md
│   ├── Feature_Status_Table.md
│   └── API_Versioning.md
└── hardware/              # Hardware documentation
    ├── README.md          # Hardware overview
    ├── Specification.md   # Complete PCB spec
    ├── ESP32_Wiring.md
    ├── Test_Procedures.md
    ├── CHANGELOG.md
    └── schematics/        # Schematics and component reference

homeassistant/             # Home Assistant integration
├── README.md              # Integration documentation
├── custom_components/     # Native HA integration
├── lovelace/              # Custom dashboard card
└── examples/              # Automations and dashboard configs
```

## Quick Links

### Getting Started

- [Machine Compatibility](Compatibility.md) ⭐ _Validated machines_
- [System Architecture](Architecture.md) ⭐ _Full system overview with cloud_

### Firmware

#### Pico (Real-Time Control)

- [Pico Overview](pico/README.md) ⭐ _Start here for Pico_
- [Pico Architecture](pico/Architecture.md) - System design
- [Implementation Plan](pico/Implementation_Plan.md) - Development roadmap
- [Machine Configurations](pico/Machine_Configurations.md) - Machine type settings
- [Debugging](pico/Debugging.md) - Debug and troubleshooting

#### ESP32 (Connectivity & UI)

- [ESP32 Overview](esp32/README.md) ⭐ _Start here for ESP32_
- [Implementation Plan](esp32/Implementation_Plan.md) - Development status
- [UI Design](esp32/UI_Design.md) - Display screens and navigation
- [UI Simulator](esp32/Simulator.md) - Desktop preview tool
- [State Management](esp32/State_Management.md) - Settings, stats, shot history

### Web & Cloud

- [Web Interface](web/README.md) ⭐ _React dashboard development_
- [Progressive Web App (PWA)](web/PWA.md) - PWA features and offline support
- [Push Notifications](web/Push_Notifications.md) - Push notification setup and usage
- [WebSocket Protocol](web/WebSocket_Protocol.md) - Message format reference
- [Cloud Service](cloud/README.md) ⭐ _Remote access architecture_
- [Cloud Deployment](cloud/Deployment.md) - Deployment guides
- [Cloud Push Notifications](cloud/Push_Notifications.md) - Push notification implementation
- [ESP32 Cloud Integration](cloud/ESP32_Integration.md) - Connect ESP32 to cloud
- [Pairing & Sharing](cloud/Pairing_and_Sharing.md) - Device pairing and sharing

### Integrations

- [Home Assistant](../homeassistant/README.md) ⭐ _Custom card, native integration, automations_
- [MQTT](esp32/integrations/MQTT.md) - MQTT setup and auto-discovery
- [BLE Scales](esp32/integrations/BLE_Scales.md) - Bluetooth scale integration
- [Brew-by-Weight](esp32/integrations/Brew_By_Weight.md) - Auto-stop at target weight
- [Power Metering](web/Power_Metering.md) - Power monitoring (hardware modules + MQTT smart plugs)
- [Web API Reference](esp32/integrations/Web_API.md) - HTTP endpoints and WebSocket
- [Notifications](esp32/integrations/Notifications.md) - Push reminders and alerts

### Hardware

- [Hardware Overview](hardware/README.md) ⭐ _Start here for hardware_
- [PCB Specification](hardware/Specification.md) - Complete PCB design
- [Schematic Reference](hardware/schematics/Schematic_Reference.md) - Detailed circuits
- [Component Reference](hardware/schematics/Component_Reference_Guide.md) - Component guide
- [ESP32 Wiring Guide](hardware/ESP32_Wiring.md) - Display module wiring
- [Test Procedures](hardware/Test_Procedures.md) - Manufacturing tests

### Shared

- [Communication Protocol](shared/Communication_Protocol.md) - Pico ↔ ESP32 protocol
- [Feature Status Table](shared/Feature_Status_Table.md) - Implementation status
- [API Versioning](shared/API_Versioning.md) - Version management

## Feature Status

See [Feature Status Table](shared/Feature_Status_Table.md) for current implementation status.

---

## 🧪 Call for Testers

We're looking for help testing BrewOS on more machines!

**✅ Validated:** ECM Synchronika (reference machine)

**🔷 Same Platform (need testers):** ECM (Barista, Technika, Mechanika, Controvento) • Profitec (Pro 300, 500, 700)

**🔲 Wanted:** Lelit, Rocket, Bezzera, La Marzocco, and any E61 machine

👉 See [TESTERS.md](../TESTERS.md) to learn how you can help!
