# Pico Firmware Documentation

The Raspberry Pi Pico handles real-time machine control for the BrewOS coffee machine controller.

## Quick Links

| Document                                      | Description                    |
| --------------------------------------------- | ------------------------------ |
| [Architecture](Architecture.md)               | System architecture and design |
| [Implementation Plan](Implementation_Plan.md) | Development roadmap            |
| [Debugging](Debugging.md)                     | Debug and troubleshooting      |
| [Requirements](Requirements.md)               | Functional requirements        |

## Configuration

| Document                                                      | Description                      |
| ------------------------------------------------------------- | -------------------------------- |
| [Machine Configurations](Machine_Configurations.md)           | Machine type settings            |
| [Environmental Configuration](Environmental_Configuration.md) | Voltage/current config           |
| [Power Metering](Power_Metering.md)                           | Hardware power meter integration |
| [Versioning](Versioning.md)                                   | Version management               |

## Features

| Document                                         | Description        |
| ------------------------------------------------ | ------------------ |
| [Cleaning Mode](features/Cleaning_Mode.md)       | Backflush cleaning |
| [Water Management](features/Water_Management.md) | Water level/tank   |
| [Shot Timer](features/Shot_Timer.md)             | Brew timer display |
| [Error Handling](features/Error_Handling.md)     | Alarms and faults  |

> **Note:** Statistics tracking is handled by ESP32. See [ESP32 Statistics](../esp32/features/Statistics.md).

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
├── README.md                    # This file
├── Architecture.md              # System architecture
├── Implementation_Plan.md       # Development roadmap
├── Debugging.md                 # Debug strategies
├── Requirements.md              # Functional requirements
├── Machine_Configurations.md    # Machine type settings
├── Environmental_Configuration.md # Voltage/current config
├── Power_Metering.md            # Hardware power meter driver
├── Versioning.md                # Version management
└── features/
    ├── Cleaning_Mode.md         # Backflush cleaning
    ├── Water_Management.md      # Water level/tank
    ├── Shot_Timer.md            # Brew timer display
    └── Error_Handling.md        # Alarms and faults
```
