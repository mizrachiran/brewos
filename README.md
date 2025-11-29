<p align="center">
  <img src="docs/assets/brewos-logo.png" alt="BrewOS Logo" width="200">
</p>

<h1 align="center">BrewOS</h1>

<p align="center">
  <strong>Open-source firmware for espresso machine control</strong>
</p>

<p align="center">
  <a href="#features">Features</a> •
  <a href="#supported-machines">Supported Machines</a> •
  <a href="#quick-start">Quick Start</a> •
  <a href="#documentation">Documentation</a> •
  <a href="#contributing">Contributing</a>
</p>

<p align="center">
  <a href="https://github.com/mizrachiran/brewos/blob/main/LICENSE">
    <img src="https://img.shields.io/badge/license-Apache%202.0%20+%20Commons%20Clause-blue.svg" alt="License: Apache 2.0 + Commons Clause">
  </a>
  <a href="https://github.com/mizrachiran/brewos/releases">
    <img src="https://img.shields.io/github/v/release/mizrachiran/brewos?include_prereleases" alt="Release">
  </a>
  <img src="https://img.shields.io/badge/platform-RP2040%20%7C%20ESP32-brightgreen" alt="Platform">
  <img src="https://img.shields.io/badge/status-development-orange" alt="Status">
</p>

---

## What is BrewOS?

BrewOS is an open-source control system designed to replace factory controllers in espresso machines. It provides enhanced temperature control, real-time monitoring, and modern features while maintaining safety as the top priority.

**Why BrewOS?**
- 🎯 **Precise PID Control** - Sub-degree temperature stability for consistent shots
- 📱 **WiFi Connected** - Monitor and control via web interface
- 🔧 **Multi-Machine Support** - One firmware for dual boiler, single boiler, and HX machines
- 🛡️ **Safety First** - Hardware watchdogs, interlocks, and fail-safe design
- 📊 **Data Logging** - Track shots, temperatures, and machine statistics
- 🔄 **OTA Updates** - Update firmware wirelessly via web interface

---

## Features

### Temperature Control
- Dual independent PID loops for brew and steam boilers
- Configurable heating strategies (parallel, sequential, smart stagger)
- Group head temperature monitoring via thermocouple
- Pre-infusion support with configurable timing

### Connectivity
- Built-in WiFi access point for initial setup
- Web-based dashboard for monitoring and control
- Real-time WebSocket updates
- REST API for integration

### Safety
- Hardware watchdog timer (2-second timeout)
- Water level interlocks
- Over-temperature protection (165°C max)
- Automatic safe-state on any fault
- Extensive error logging

### Monitoring
- Real-time temperature graphs
- Pressure monitoring
- Shot timer with statistics
- Power consumption tracking (PZEM-004T)
- Brew counter with cleaning reminders

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              SYSTEM ARCHITECTURE                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌──────────────┐         ┌──────────────┐         ┌──────────────┐         │
│  │   SENSORS    │         │  PICO RP2040 │         │   ACTUATORS  │         │
│  │              │         │              │         │              │         │
│  │ • NTC ×2     │────────►│ • Safety     │────────►│ • SSR ×2     │         │
│  │ • Thermocouple│        │ • PID Control│         │ • Relay ×4   │         │
│  │ • Pressure   │         │ • State Mgmt │         │ • Buzzer     │         │
│  │ • Levels ×3  │         │              │         │ • LED        │         │
│  │ • Switches   │         └───────┬──────┘         └──────────────┘         │
│  └──────────────┘                 │                                          │
│                                   │                                          │
│  ┌──────────────┐                 │                                          │
│  │    PZEM      │────────────────►│                                          │
│  │  Power Meter │ UART            │ Binary UART (921600 baud)                │
│  └──────────────┘                 ▼                                          │
│                           ┌──────────────┐         ┌──────────────┐         │
│                           │    ESP32     │         │  External    │         │
│                           │   Display    │◄───────►│  (MQTT, App) │         │
│                           │   Module     │  WiFi   │              │         │
│                           └──────────────┘         └──────────────┘         │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Dual-Core Design:**
| Core | Responsibility | Timing |
|------|----------------|--------|
| **Core 0** | Safety, sensors, PID control, outputs | 100ms deterministic loop |
| **Core 1** | UART communication, protocol handling | Async, interrupt-driven |

---

## Supported Machines

BrewOS supports multiple espresso machine architectures through compile-time configuration:

| Machine Type | Status | Examples |
|--------------|--------|----------|
| **Dual Boiler** | ✅ Supported | ECM Synchronika, Profitec Pro 700, Lelit Bianca |
| **Single Boiler** | ✅ Supported | ECM Barista, Profitec Pro 300, Rancilio Silvia |
| **Heat Exchanger** | ✅ Supported | ECM Mechanika, Profitec Pro 500, E61 HX machines |
| **Thermoblock** | 🔮 Planned | - |

**👉 [Full Compatibility List](docs/Compatibility.md)** - See validated machines and expected compatible models.

### Reference Implementation

The **ECM Synchronika** serves as the reference implementation with complete schematics and documentation:

| Resource | Description |
|----------|-------------|
| [Schematic](schematics/ECM_Schematic_Reference.md) | Complete circuit diagrams |
| [Netlist](schematics/ECM_Netlist.csv) | Component list for PCB |
| [Wiring Guide](hardware/ESP32_Display_Wiring.md) | Connection details |
| [Compatibility](docs/Compatibility.md) | Validated & compatible machines |

---

## Quick Start

### Prerequisites

- [Pico SDK](https://github.com/raspberrypi/pico-sdk) (v1.5.0+)
- [ARM GCC Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
- [PlatformIO](https://platformio.org/) (for ESP32)
- CMake 3.13+

### Building

```bash
# Clone the repository
git clone https://github.com/mizrachiran/brewos.git
cd brewos

# Build Pico firmware (all machine types)
cd src/pico
mkdir build && cd build
cmake -DBUILD_ALL_MACHINES=ON ..
make -j4

# Output files:
# - brewos_dual_boiler.uf2
# - brewos_single_boiler.uf2
# - brewos_heat_exchanger.uf2
```

```bash
# Build ESP32 firmware
cd src/esp32
pio run

# Upload web UI
pio run -t uploadfs
```

### Flashing

**Pico (USB):**
1. Hold BOOTSEL button
2. Connect USB cable
3. Release button - Pico mounts as drive
4. Copy `brewos_*.uf2` to the drive

**Pico (OTA via ESP32):**
1. Connect to BrewOS-Setup WiFi
2. Open http://192.168.4.1
3. Upload firmware via web interface

See [SETUP.md](SETUP.md) for detailed instructions.

---

## Documentation

| Document | Description |
|----------|-------------|
| **Getting Started** | |
| [Setup Guide](SETUP.md) | Development environment setup |
| [Quick Start](docs/pico/Quick_Start.md) | Quick reference for common tasks |
| **Pico Firmware** | |
| [Architecture](docs/pico/Architecture.md) | Module structure, dual-core design |
| [Requirements](docs/pico/Requirements.md) | Functional and safety requirements |
| [Machine Configurations](docs/pico/Machine_Configurations.md) | Multi-machine support |
| **ESP32 Firmware** | |
| [Implementation Plan](docs/esp32/Implementation_Plan.md) | ESP32 development status |
| [MQTT Integration](docs/esp32/integrations/MQTT.md) | Home Assistant integration |
| [Web API](docs/esp32/integrations/Web_API.md) | REST API reference |
| **Shared** | |
| [Communication Protocol](docs/shared/Communication_Protocol.md) | Binary protocol Pico ↔ ESP32 |
| **Hardware** | |
| [Specification](hardware/Specification.md) | PCB design, component selection |
| [Test Procedures](hardware/Test_Procedures.md) | Hardware testing procedures |
| **Features** | |
| [Cleaning Mode](docs/pico/features/Cleaning_Mode_Implementation.md) | Backflush cycle |
| [Statistics](docs/pico/features/Statistics_Feature.md) | Shot tracking |
| [Water Management](docs/pico/features/Water_Management_Implementation.md) | Auto-fill system |

---

## Hardware Requirements

### Microcontrollers
- **Raspberry Pi Pico** (RP2040) - Real-time control
- **ESP32-S3** - WiFi, display, user interface

### Sensors
- 2× NTC thermistors (brew & steam boilers)
- 1× MAX31855 thermocouple (group head)
- 1× Pressure transducer (0-16 bar)
- 3× Water level probes

### Actuators
- 2× Solid State Relays (heater control)
- 4× Mechanical relays (pump, solenoid, etc.)
- 1× Piezo buzzer
- 1× Status LED

### Optional
- PZEM-004T power meter
- Flow meter
- Scale integration

---

## 🧪 Call for Testers

**Help us expand machine compatibility!** We're looking for espresso enthusiasts to test BrewOS on their machines.

### ✅ Validated

| Brand | Model | Notes |
|-------|-------|-------|
| ECM | **Synchronika** | Reference machine |

### 🔷 Same Platform (Need Validation)

These use the same GICAR board and should work - we need testers to confirm!

| Brand | Models |
|-------|--------|
| **ECM** | Barista, Technika, Technika Profi, Mechanika, Mechanika Profi, Mechanika V Slim, Controvento |
| **Profitec** | Pro 300, Pro 500, Pro 700 |

### 🔲 Other Brands (Wanted)

| Brand | Models |
|-------|--------|
| **Lelit** | MaraX, Bianca, Elizabeth |
| **Rocket** | Appartamento, Mozzafiato, R58, R Nine One |
| **Bezzera** | BZ10, BZ13, Duo, Matrix |
| **La Marzocco** | Linea Mini, GS3 |
| **Other** | Any E61 group head machine |

**👉 [Become a Tester](TESTERS.md)** - See how you can help!

---

## Safety Notice

```
⚠️  WARNING: MAINS VOLTAGE

This project involves 100-240V AC mains electricity.
Improper handling can result in death or serious injury.

• Only qualified individuals should work on mains circuits
• Always use isolation transformers during development
• Never work alone on energized equipment
• Disconnect power before making any changes
• Follow all local electrical codes and regulations
```

**Safety is not optional.** The firmware includes multiple safety layers, but hardware installation must be performed by qualified individuals. See [Safety Requirements](docs/pico/Requirements.md#2-safety-requirements-critical) for details.

---

## Contributing

We welcome contributions! Please read our guidelines before getting started:

1. **Read the docs** - Understand the [Architecture](docs/pico/Architecture.md) and [Requirements](docs/pico/Requirements.md)
2. **Check existing issues** - See what's being worked on
3. **Follow code style** - Match existing patterns
4. **Test thoroughly** - Especially safety-critical code
5. **Update documentation** - Keep docs in sync with changes

See [CONTRIBUTING.md](CONTRIBUTING.md) for detailed guidelines.

### Development Priorities

| Priority | Area | Description |
|----------|------|-------------|
| 🔴 Critical | Safety | Any safety improvements |
| 🟠 High | Stability | Bug fixes, reliability |
| 🟡 Medium | Features | New machine support, integrations |
| 🟢 Normal | Enhancements | UI improvements, optimizations |

---

## Roadmap

- [x] Dual boiler support (ECM Synchronika)
- [x] Single boiler support
- [x] Heat exchanger support
- [x] Web-based dashboard
- [x] OTA firmware updates
- [x] MQTT integration
- [x] Home Assistant auto-discovery
- [ ] BLE scale integration (brew by weight)
- [ ] Cloud connectivity
- [ ] Mobile app (React Native)
- [ ] Pressure profiling
- [ ] Flow control
- [ ] Thermoblock support

---

## Community

- **Discussions:** [GitHub Discussions](https://github.com/mizrachiran/brewos/discussions)
- **Issues:** [GitHub Issues](https://github.com/mizrachiran/brewos/issues)
- **Discord:** Coming soon

---

## License

This project is licensed under the **Apache License 2.0 with Commons Clause** - see the [LICENSE](LICENSE) file for details.

**What this means:**
- ✅ You can use, modify, and distribute the software for personal use
- ✅ You can use it for your own espresso machine
- ✅ You can contribute improvements back to the project
- ❌ You cannot sell the software or services based primarily on the software

---

## Acknowledgments

- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
- [ESP-IDF](https://github.com/espressif/esp-idf) & Arduino ESP32
- [PlatformIO](https://platformio.org/)
- The espresso enthusiast community

---

<p align="center">
  <sub>Built with ☕ by espresso enthusiasts, for espresso enthusiasts</sub>
</p>
