# BrewOS Documentation

## Overview

BrewOS is an open-source espresso machine controller with:
- **Pico (RP2040)** - Real-time machine control
- **ESP32-S3** - Connectivity, UI, and smart features

## Documentation Structure

```
docs/
├── pico/                  # Pico firmware docs
│   ├── README.md
│   ├── Architecture.md
│   ├── Implementation_Plan.md
│   └── features/          # Feature-specific docs
├── esp32/                 # ESP32 firmware docs
│   ├── README.md
│   ├── Implementation_Plan.md
│   └── integrations/      # Integration docs (MQTT, API)
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
- [Pico Quick Start](pico/Quick_Start.md)
- [ESP32 Implementation](esp32/Implementation_Plan.md)

### Architecture
- [Pico Architecture](pico/Architecture.md)
- [Communication Protocol](shared/Communication_Protocol.md)

### Integrations
- [MQTT / Home Assistant](esp32/integrations/MQTT.md)
- [Web API Reference](esp32/integrations/Web_API.md)

### Hardware
- [Hardware Specification](hardware/Specification.md)
- [ESP32 Display Wiring](hardware/ESP32_Display_Wiring.md)

## Feature Status

See [Feature Status Table](shared/Feature_Status_Table.md) for current implementation status.
