# Pico Firmware Documentation

The Raspberry Pi Pico handles real-time machine control for the BrewOS coffee machine controller.

## Quick Links

| Document | Description |
|----------|-------------|
| [Architecture](Architecture.md) | System architecture and design |
| [Implementation Plan](Implementation_Plan.md) | Development roadmap |
| [Quick Start](Quick_Start.md) | Getting started guide |
| [Debugging](Debugging.md) | Debug and troubleshooting |
| [Requirements](Requirements.md) | Functional requirements |

## Configuration

| Document | Description |
|----------|-------------|
| [Machine Configurations](Machine_Configurations.md) | Machine type settings |
| [Environmental Configuration](Environmental_Configuration.md) | Voltage/current config |
| [Versioning](Versioning.md) | Version management |

## Features

| Document | Description |
|----------|-------------|
| [Cleaning Mode](features/Cleaning_Mode_Implementation.md) | Backflush cleaning |
| [Water Management](features/Water_Management_Implementation.md) | Water level/tank |
| [Statistics](features/Statistics_Feature.md) | Usage statistics |
| [Shot Timer](features/Shot_Timer_Display.md) | Brew timer display |
| [Error Handling](features/Error_Handling.md) | Alarms and faults |

## Hardware

- **MCU:** Raspberry Pi Pico (RP2040)
- **Cores:** Dual-core ARM Cortex-M0+ @ 133 MHz
- **RAM:** 264 KB
- **Flash:** 2 MB

## Building

```bash
cd src/pico
mkdir build && cd build
cmake ..
make
```

## Machine Types Supported

- Single Boiler
- Heat Exchanger
- Dual Boiler

## Folder Structure

```
docs/pico/
├── README.md              # This file
├── Architecture.md
├── Implementation_Plan.md
├── Quick_Start.md
├── Debugging.md
├── Requirements.md
├── Machine_Configurations.md
├── Environmental_Configuration.md
├── Versioning.md
└── features/
    ├── Cleaning_Mode_Implementation.md
    ├── Water_Management_Implementation.md
    ├── Statistics_Feature.md
    ├── Shot_Timer_Display.md
    └── Error_Handling.md
```

