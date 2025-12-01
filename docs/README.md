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
├── Compatibility.md       # Validated machines list
├── pico/                  # Pico firmware docs
│   ├── README.md
│   ├── Architecture.md
│   ├── Implementation_Plan.md
│   └── features/          # Feature-specific docs
├── esp32/                 # ESP32 firmware docs
│   ├── README.md
│   ├── Implementation_Plan.md
│   └── integrations/      # Integration docs (MQTT, BLE, API)
├── web/                   # Web interface docs
│   ├── README.md
│   └── WebSocket_Protocol.md
├── cloud/                 # Cloud service docs
│   ├── README.md
│   ├── Deployment.md
│   └── ESP32_Integration.md
├── shared/                # Shared documentation
│   ├── Communication_Protocol.md
│   └── Feature_Status_Table.md
└── hardware/              # Hardware documentation
    ├── Specification.md
    ├── ESP32_Display_Wiring.md
    └── Test_Procedures.md
```

## Quick Links

### Getting Started
- [Machine Compatibility](Compatibility.md) ⭐ *Validated machines*
- [ESP32 Implementation](esp32/Implementation_Plan.md)

### Architecture
- [System Architecture](Architecture.md) ⭐ *Full system overview with cloud*
- [Pico Architecture](pico/Architecture.md)
- [ESP32 State Management](esp32/State_Management.md) - Settings, stats, shot history
- [Communication Protocol](shared/Communication_Protocol.md)

### Web & Cloud
- [Web Interface](web/README.md) - React dashboard development
- [Progressive Web App (PWA)](web/PWA.md) - PWA features and offline support
- [Push Notifications](web/Push_Notifications.md) - Push notification setup and usage
- [WebSocket Protocol](web/WebSocket_Protocol.md) - Message format reference
- [Cloud Service](cloud/README.md) - Remote access architecture
- [Cloud Push Notifications](cloud/Push_Notifications.md) - Cloud push notification implementation
- [Cloud Deployment](cloud/Deployment.md) - Deployment guides
- [ESP32 Cloud Integration](cloud/ESP32_Integration.md) - Connect ESP32 to cloud

### Integrations
- [MQTT / Home Assistant](esp32/integrations/MQTT.md)
- [BLE Scales](esp32/integrations/BLE_Scales.md)
- [Web API Reference](esp32/integrations/Web_API.md)

### Hardware
- [Hardware Specification](hardware/Specification.md)
- [ESP32 Display Wiring](hardware/ESP32_Display_Wiring.md)

## Feature Status

See [Feature Status Table](shared/Feature_Status_Table.md) for current implementation status.

---

## 🧪 Call for Testers

We're looking for help testing BrewOS on more machines!

**✅ Validated:** ECM Synchronika (reference machine)

**🔷 Same Platform (need testers):** ECM (Barista, Technika, Mechanika, Controvento) • Profitec (Pro 300, 500, 700)

**🔲 Wanted:** Lelit, Rocket, Bezzera, La Marzocco, and any E61 machine

👉 See [TESTERS.md](../TESTERS.md) to learn how you can help!
