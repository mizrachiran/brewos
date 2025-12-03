# Pico Firmware - Feature Status Table

**Last Updated:** December 2024  
**Status:** Phase 6 - Integration & Testing  
**Latest Update:** Comprehensive error handling implemented: error tracking, validation, and reporting for all components

---

## Feature Status Legend

- ✅ **Complete** - Fully implemented and tested
- ⚠️ **Partial** - Partially implemented, needs work
- ❌ **Not Implemented** - Not yet started
- 🔄 **In Progress** - Currently being worked on

---

## Core System Features

| Feature                        | Status | Module       | Notes                                                               |
| ------------------------------ | ------ | ------------ | ------------------------------------------------------------------- |
| **Hardware Abstraction Layer** | ✅     | `hardware.c` | Complete with simulation mode support                               |
| **Dual-Core Architecture**     | ✅     | `main.c`     | Core 0: Control, Core 1: Communication                              |
| **Watchdog Timer**             | ✅     | `safety.c`   | 2000ms timeout, properly fed                                        |
| **Boot Sequence**              | ✅     | `main.c`     | Initialization, safety checks, mode selection                       |
| **Error Handling**             | ✅     | Multiple     | Comprehensive error tracking, validation, and reporting implemented |

---

## Sensor Implementation

| Feature                                | Status | Module                     | Notes                                                              |
| -------------------------------------- | ------ | -------------------------- | ------------------------------------------------------------------ |
| **Brew Boiler NTC**                    | ✅     | `sensors.c`                | Steinhart-Hart conversion, moving average filter                   |
| **Steam Boiler NTC**                   | ✅     | `sensors.c`                | Steinhart-Hart conversion, moving average filter                   |
| **Group Head Thermocouple (MAX31855)** | ✅     | `sensors.c`                | SPI interface, fault detection, cold junction compensation         |
| **Pressure Transducer**                | ✅     | `sensors.c`                | ADC reading, voltage divider calculation, filtering                |
| **Water Level Sensors**                | ✅     | `sensors.c`                | Reservoir, tank, steam boiler level probes                         |
| **Brew Switch**                        | ✅     | `state.c`                  | Debounced, GPIO input                                              |
| **Emergency Stop**                     | ✅     | `safety.c`                 | Hardware interlock                                                 |
| **Water Mode Switch**                  | ✅     | `water_management.c`       | Physical switch (tank vs plumbed)                                  |
| **WEIGHT_STOP Signal**                 | ✅     | `state.c`                  | ESP32 signal for brew-by-weight                                    |
| **Sensor Fault Detection**             | ✅     | `sensors.c`                | NTC open/short, MAX31855 faults                                    |
| **Sensor Error Tracking**              | ✅     | `sensors.c`                | Consecutive failure counters, error thresholds, recovery detection |
| **Sensor Filtering**                   | ✅     | `sensor_utils.c`           | Moving average filter for all analog sensors                       |
| **Statistics Recording**               | ✅     | `state.c` + `statistics.c` | Automatically records brews for statistics                         |

---

## Safety System

| Feature                             | Status | Module     | Notes                                       |
| ----------------------------------- | ------ | ---------- | ------------------------------------------- |
| **Water Reservoir Interlock**       | ✅     | `safety.c` | Blocks heating/pump in water tank mode      |
| **Tank Level Interlock**            | ✅     | `safety.c` | Debounced, prevents operation if low        |
| **Steam Boiler Level Interlock**    | ✅     | `safety.c` | Prevents heating if empty                   |
| **Over-Temperature Protection**     | ✅     | `safety.c` | Brew, steam, group head with hysteresis     |
| **Sensor Fault Protection**         | ✅     | `safety.c` | Safe state on NTC/MAX31855 faults           |
| **SSR Monitoring**                  | ✅     | `safety.c` | Detects stuck-on SSRs (60s timeout)         |
| **SSR Duty Cycle Limit**            | ✅     | `safety.c` | 95% maximum duty cycle enforced             |
| **Safe State Implementation**       | ✅     | `safety.c` | All outputs OFF, LED/buzzer feedback        |
| **Safe State Recovery**             | ✅     | `safety.c` | Automatic recovery when conditions clear    |
| **ESP32 Heartbeat**                 | ✅     | `safety.c` | Timeout detection, safe state on disconnect |
| **Environmental Config Validation** | ✅     | `safety.c` | Machine disabled if env config invalid      |
| **Emergency Stop**                  | ✅     | `safety.c` | Hardware interlock, immediate safe state    |

---

## Control System

| Feature                             | Status | Module                        | Notes                                                            |
| ----------------------------------- | ------ | ----------------------------- | ---------------------------------------------------------------- |
| **Control Validation**              | ✅     | `control_common.c`            | PID parameter validation, output value clamping, mode validation |
| **PID Control (Brew)**              | ✅     | `control_common.c`            | Kp, Ki, Kd tuning, anti-windup                                   |
| **PID Control (Steam)**             | ✅     | `control_common.c`            | Kp, Ki, Kd tuning, anti-windup                                   |
| **Derivative Filtering**            | ✅     | `control_common.c`            | Low-pass filter on derivative term                               |
| **Setpoint Ramping**                | ✅     | `control_common.c`            | Gradual setpoint changes                                         |
| **Anti-Windup**                     | ✅     | `control_common.c`            | Prevents integral accumulation                                   |
| **PWM SSR Control**                 | ✅     | `control_common.c`            | 25Hz PWM for brew and steam SSRs                                 |
| **Relay Control**                   | ✅     | `control_common.c`            | Pump and solenoid control                                        |
| **Heating Strategy: BREW_ONLY**     | ✅     | `control_common.c`            | Brew boiler only                                                 |
| **Heating Strategy: SEQUENTIAL**    | ✅     | `control_common.c`            | Brew first, then steam                                           |
| **Heating Strategy: PARALLEL**      | ✅     | `control_common.c`            | Both boilers simultaneously                                      |
| **Heating Strategy: SMART_STAGGER** | ✅     | `control_common.c`            | Intelligent staggering with current limiting                     |
| **Strategy Validation**             | ✅     | `control_common.c`            | Validates against electrical limits                              |
| **Current Limiting**                | ✅     | `control_common.c`            | Real-time current limiting for parallel strategies               |
| **Power Measurement**               | ✅     | `control_common.c` + `pzem.c` | Real-time from PZEM, fallback to theoretical estimation          |

---

## State Machine

| Feature                   | Status | Module                 | Notes                                   |
| ------------------------- | ------ | ---------------------- | --------------------------------------- |
| **STATE_INIT**            | ✅     | `state.c`              | Initialization state                    |
| **STATE_IDLE**            | ✅     | `state.c`              | Machine on but not heating (MODE_IDLE)  |
| **STATE_HEATING**         | ✅     | `state.c`              | Initial heating phase (cold to hot)     |
| **STATE_READY**           | ✅     | `state.c`              | At temperature, PID maintaining         |
| **STATE_BREWING**         | ✅     | `state.c`              | Brew cycle in progress                  |
| **STATE_FAULT**           | ✅     | `state.c`              | Fault condition detected                |
| **STATE_SAFE**            | ✅     | `state.c`              | Safe state (all outputs OFF)            |
| **STATE_ECO**             | ✅     | `state.c`              | Eco mode (reduced temperature)          |
| **State Transitions**     | ✅     | `state.c`              | All transitions with entry/exit actions |
| **MODE_IDLE**             | ✅     | `state.c`              | Machine idle, no heating                |
| **MODE_BREW**             | ✅     | `state.c`              | Brew mode (heats brew boiler)           |
| **MODE_STEAM**            | ✅     | `state.c`              | Steam mode (heats both boilers)         |
| **MODE_ECO**              | ✅     | `state.c`              | Eco mode with reduced setpoint          |
| **Mode Switching**        | ✅     | `state.c`              | Dynamic mode changes via ESP32          |
| **Eco Mode Auto-Timeout** | ✅     | `state.c`              | Automatic eco entry after idle timeout  |
| **Eco Mode Config**       | ✅     | `config_persistence.c` | Eco temp, timeout, enabled - persisted  |

---

## Brew Cycle

| Feature                   | Status | Module               | Notes                                       |
| ------------------------- | ------ | -------------------- | ------------------------------------------- |
| **Pre-Infusion**          | ✅     | `state.c`            | Configurable on/pause timing                |
| **Brew Phase**            | ✅     | `state.c`            | Full pressure brewing                       |
| **Post-Brew Delay**       | ✅     | `state.c`            | Solenoid delay after brew (2s)              |
| **Brew Switch Detection** | ✅     | `state.c`            | Debounced, starts/stops brew                |
| **WEIGHT_STOP Signal**    | ✅     | `state.c`            | Automatic brew stop (brew-by-weight)        |
| **Shot Timer**            | ✅     | `state.c`            | Tracks brew duration, sends start timestamp |
| **Brew Priority**         | ✅     | `water_management.c` | Blocks steam fill during brew               |

---

## Water Management

| Feature                              | Status | Module               | Notes                                                      |
| ------------------------------------ | ------ | -------------------- | ---------------------------------------------------------- |
| **Water Management Error Reporting** | ✅     | `water_management.c` | Fill timeout alarms, level probe monitoring, error logging |
| **Steam Boiler Auto-Fill**           | ✅     | `water_management.c` | State machine with fill cycle                              |
| **Fill Cycle State Machine**         | ✅     | `water_management.c` | IDLE → HEATER_OFF → ACTIVE → COMPLETE                      |
| **Brew Priority**                    | ✅     | `water_management.c` | Fill stops if brew starts                                  |
| **Water LED Control**                | ✅     | `water_management.c` | Indicates reservoir/tank status                            |
| **Water Tank Mode**                  | ✅     | `water_management.c` | Detects via physical switch                                |
| **Plumbed Mode**                     | ✅     | `water_management.c` | Detects via physical switch                                |
| **Reservoir Detection**              | ✅     | `water_management.c` | Checks reservoir presence                                  |
| **Fill Solenoid Control**            | ✅     | `water_management.c` | Opens/closes fill path                                     |
| **Pump Control (Fill)**              | ✅     | `water_management.c` | Activates pump during fill                                 |

---

## Cleaning Mode

| Feature                          | Status | Module                                | Notes                                                  |
| -------------------------------- | ------ | ------------------------------------- | ------------------------------------------------------ |
| **Cleaning Counter**             | ✅     | `cleaning.c`                          | Simple counter (≥15s brews) - resets after cleaning    |
| **Cleaning Reminder**            | ✅     | `cleaning.c`                          | Configurable threshold (10-200 cycles, default: 100)   |
| **Cleaning Cycle**               | ✅     | `cleaning.c`                          | 10-second backflush cycle                              |
| **Temperature Requirement**      | ✅     | `cleaning.c`                          | Only allows cleaning when machine is hot (STATE_READY) |
| **Auto-Stop**                    | ✅     | `cleaning.c`                          | Auto-stops after 10 seconds                            |
| **Manual Stop**                  | ✅     | `cleaning.c`                          | Manual stop via brew switch                            |
| **State Machine Integration**    | ✅     | `state.c`                             | Cleaning mode handled separately from normal brew      |
| **Protocol Commands**            | ✅     | `main.c`                              | MSG_CMD_CLEANING_START/STOP/RESET/SET_THRESHOLD        |
| **Brew Switch Control**          | ✅     | `state.c`                             | Brew switch controls cleaning cycle when active        |
| **Cleaning Counter Persistence** | ✅     | `cleaning.c` + `config_persistence.c` | Brew count and threshold persist to flash              |
| **Status Payload Integration**   | ✅     | `protocol.c`                          | Brew count/reminder included in MSG_STATUS             |

---

## Statistics & Analytics

| Feature                       | Status | Module                  | Notes                                                                 |
| ----------------------------- | ------ | ----------------------- | --------------------------------------------------------------------- |
| **Overall Statistics**        | ✅     | `statistics.c`          | Total brews, average, min, max brew times                             |
| **Daily Statistics**          | ✅     | `statistics.c`          | Cups per day, average brew time (rolling 24h)                         |
| **Weekly Statistics**         | ✅     | `statistics.c`          | Cups per week, average brew time (rolling 7 days)                     |
| **Monthly Statistics**        | ✅     | `statistics.c`          | Cups per month, average brew time (rolling 30 days)                   |
| **Brew History**              | ✅     | `statistics.c`          | Last 100 brew entries with timestamps (circular buffer)               |
| **Automatic Recording**       | ✅     | `state.c`               | Records all brews ≥5 seconds automatically                            |
| **Time-Based Calculations**   | ✅     | `statistics.c`          | Rolling window calculations for daily/weekly/monthly                  |
| **Statistics Initialization** | ✅     | `main.c`                | Statistics module initialized at boot                                 |
| **Statistics Persistence**    | ✅     | `statistics.c`          | Flash persistence (saves every 10 brews, loads on boot)               |
| **Protocol Integration**      | ⚠️     | `protocol.c` + `main.c` | MSG_STATISTICS implemented, command handler pending                   |
| **RTC Support**               | ⚠️     | `statistics.c`          | Structure in place, uses boot time (RTC hardware integration pending) |

---

## Communication Protocol

| Feature                            | Status | Module                 | Notes                                                                                                                                                                                                                                               |
| ---------------------------------- | ------ | ---------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---- | --- | --- | ------- | --- |
| **UART Protocol**                  | ✅     | `protocol.c`           | Binary protocol with CRC16                                                                                                                                                                                                                          |
| **Packet Structure**               | ✅     | `protocol.c`           | SYNC                                                                                                                                                                                                                                                | TYPE | LEN | SEQ | PAYLOAD | CRC |
| **MSG_BOOT**                       | ✅     | `protocol.c`           | Boot message with version info                                                                                                                                                                                                                      |
| **MSG_STATUS**                     | ✅     | `protocol.c`           | Status updates (250ms interval)                                                                                                                                                                                                                     |
| **MSG_ALARM**                      | ✅     | `protocol.c`           | Alarm notifications                                                                                                                                                                                                                                 |
| **MSG_CMD_MODE**                   | ✅     | `main.c`               | Mode selection (IDLE/BREW/STEAM)                                                                                                                                                                                                                    |
| **MSG_CMD_SET_TEMP**               | ✅     | `main.c`               | Temperature setpoint changes                                                                                                                                                                                                                        |
| **MSG_CMD_SET_PID**                | ✅     | `main.c`               | PID parameter changes                                                                                                                                                                                                                               |
| **MSG_CMD_CONFIG**                 | ✅     | `main.c`               | Environmental configuration                                                                                                                                                                                                                         |
| **MSG_CMD_START_BREW**             | ✅     | `main.c`               | Start brew command                                                                                                                                                                                                                                  |
| **MSG_CMD_STOP_BREW**              | ✅     | `main.c`               | Stop brew command                                                                                                                                                                                                                                   |
| **MSG_CMD_CLEANING_START**         | ✅     | `main.c`               | Start cleaning cycle                                                                                                                                                                                                                                |
| **MSG_CMD_CLEANING_STOP**          | ✅     | `main.c`               | Stop cleaning cycle                                                                                                                                                                                                                                 |
| **MSG_CMD_CLEANING_RESET**         | ✅     | `main.c`               | Reset brew counter                                                                                                                                                                                                                                  |
| **MSG_CMD_CLEANING_SET_THRESHOLD** | ✅     | `main.c`               | Set cleaning reminder threshold                                                                                                                                                                                                                     |
| **MSG_CMD_GET_STATISTICS**         | ✅     | `main.c`               | Request statistics                                                                                                                                                                                                                                  |
| **MSG_CMD_SET_ECO**                | ✅     | `main.c`               | Set eco mode configuration                                                                                                                                                                                                                          |
| **MSG_STATISTICS**                 | ✅     | `protocol.c`           | Statistics response                                                                                                                                                                                                                                 |
| **MSG_CONFIG**                     | ✅     | `protocol.c`           | Configuration response                                                                                                                                                                                                                              |
| **MSG_ENV_CONFIG**                 | ✅     | `protocol.c`           | Environmental config response                                                                                                                                                                                                                       |
| **MSG_DEBUG**                      | ✅     | `protocol.c`           | Debug message forwarding                                                                                                                                                                                                                            |
| **MSG_CMD_BOOTLOADER**             | ✅     | `main.c`               | Bootloader entry command (fully implemented)                                                                                                                                                                                                        |
| **Heartbeat/Ping**                 | ✅     | `protocol.c`           | Connection monitoring                                                                                                                                                                                                                               |
| **Error Handling**                 | ✅     | `protocol.c`           | CRC errors, timeouts, error tracking, packet validation                                                                                                                                                                                             |
| **ACK Result Codes**               | ✅     | `protocol.c`, `main.c` | Success/invalid/rejected/failed/timeout/busy/not_ready codes                                                                                                                                                                                        |
| **Protocol Error Tracking**        | ✅     | `protocol.c`           | CRC error counter, packet error counter, error reset function                                                                                                                                                                                       |
| **OTA Firmware Update (ESP32)**    | ✅     | ESP32 `web_server.cpp` | File upload to LittleFS, MSG_CMD_BOOTLOADER command, chunked firmware streaming over UART, progress reporting via WebSocket                                                                                                                         |
| **OTA Firmware Update (Pico)**     | ✅     | `bootloader.c`         | Serial bootloader fully implemented: receives firmware chunks over UART, verifies checksums, writes to flash, handles page/sector alignment, reboots on success. No hardware pin manipulation required (hardware bootloader entry is fallback only) |

---

## Machine Type Support

| Feature                   | Status | Module                     | Notes                                                            |
| ------------------------- | ------ | -------------------------- | ---------------------------------------------------------------- |
| **Machine Config System** | ✅     | `machine_config.c`         | `machine_config_t`, `machine_features_t`, compile-time selection |
| **Dual Boiler**           | ✅     | `control_dual_boiler.c`    | Independent brew/steam PIDs, all heating strategies              |
| **Single Boiler**         | ✅     | `control_single_boiler.c`  | Mode switching (brew ↔ steam setpoint), auto-return timeout      |
| **Heat Exchanger**        | ✅     | `control_heat_exchanger.c` | Three control modes: Temperature PID, Pressure PID, Pressurestat |
| **Thermoblock**           | ❌     | N/A                        | Future: requires flow heater control                             |

### Heat Exchanger Control Modes

| Mode                      | Status | Description                                          |
| ------------------------- | ------ | ---------------------------------------------------- |
| `HX_CONTROL_TEMPERATURE`  | ✅     | PID based on steam NTC (modern retrofit)             |
| `HX_CONTROL_PRESSURE`     | ✅     | PID based on pressure transducer                     |
| `HX_CONTROL_PRESSURESTAT` | ✅     | Monitor only - external pressurestat controls heater |

### Control Architecture (Compile-Time File Selection)

Each machine type has its own control implementation:

```
src/pico/src/
├── control_common.c          # Shared: PID, outputs, public API
├── control_dual_boiler.c     # Dual boiler control logic
├── control_single_boiler.c   # Single boiler with mode switching
└── control_heat_exchanger.c  # HX steam-only control
```

### Build Commands

```bash
# Build single machine type
cmake .. -DMACHINE_TYPE=DUAL_BOILER
make
# Output: ecm_pico_dual_boiler.uf2

# Build ALL machine types at once
cmake .. -DBUILD_ALL_MACHINES=ON
make
# Output: ecm_pico_dual_boiler.uf2, ecm_pico_single_boiler.uf2, ecm_pico_heat_exchanger.uf2
```

### Machine-Type Aware Components

| Component              | File                 | Adapts to Machine Type                                      |
| ---------------------- | -------------------- | ----------------------------------------------------------- |
| **Control System**     | `control_*.c`        | ✅ Separate files per machine type (compile-time selection) |
| **Heating Strategies** | `control_common.c`   | ✅ Only BREW_ONLY for non-dual-boiler                       |
| **Safety System**      | `safety.c`           | ✅ Only checks sensors/SSRs that exist                      |
| **State Machine**      | `state.c`            | ✅ Uses group_temp for HX ready detection                   |
| **Water Management**   | `water_management.c` | ✅ Skips steam auto-fill for single boiler                  |
| **Sensor Reading**     | `sensors.c`          | ✅ Checks machine features before reading sensors           |
| **Status Payload**     | `main.c`             | ✅ Reports appropriate temps per machine type               |
| **Protocol**           | `protocol.c`         | ✅ Reports machine_type in boot payload                     |
| **Main Init**          | `main.c`             | ✅ Logs machine config at boot                              |

**Key Differences by Machine Type:**

| Feature            | Dual Boiler | Single Boiler | Heat Exchanger |
| ------------------ | :---------: | :-----------: | :------------: |
| Brew NTC           |      ✓      |       ✓       |       ✗        |
| Steam NTC          |      ✓      |       ✗       |       ✓        |
| Group TC           |  Optional   |   Optional    |  **Required**  |
| SSRs               |      2      |       1       |       1        |
| Heating strategies |    All 5    |   BREW_ONLY   |   BREW_ONLY    |
| Ready detection    |  brew_temp  |   brew_temp   |   group_temp   |

> **Note:** See [Machine_Configurations.md](Machine_Configurations.md) for detailed machine configuration documentation.

---

## Configuration & Persistence

| Feature                          | Status | Module                   | Notes                                                   |
| -------------------------------- | ------ | ------------------------ | ------------------------------------------------------- |
| **Environmental Config**         | ✅     | `environmental_config.c` | Voltage, max current draw                               |
| **Machine Electrical Config**    | ✅     | `machine_electrical.h`   | Heater power, current limits                            |
| **PCB Configuration**            | ✅     | `pcb_config.c`           | Pin mappings, hardware version                          |
| **Configuration Persistence**    | ✅     | `config_persistence.c`   | Flash storage for settings                              |
| **Default Values**               | ✅     | `config_persistence.c`   | Defaults for all config except environmental            |
| **Setup Mode**                   | ✅     | `config_persistence.c`   | Machine disabled if env config not set                  |
| **CRC32 Validation**             | ✅     | `config_persistence.c`   | Data integrity checks                                   |
| **Magic Number**                 | ✅     | `config_persistence.c`   | Flash data validation                                   |
| **Version Control**              | ✅     | `config_persistence.c`   | Config version tracking                                 |
| **PID Settings Persistence**     | ✅     | `config_persistence.c`   | Kp, Ki, Kd for brew and steam                           |
| **Setpoint Persistence**         | ✅     | `config_persistence.c`   | Brew and steam setpoints                                |
| **Heating Strategy Persistence** | ✅     | `config_persistence.c`   | Selected heating strategy                               |
| **Pre-Infusion Persistence**     | ✅     | `config_persistence.c`   | Pre-infusion settings                                   |
| **Statistics Persistence**       | ✅     | `statistics.c`           | Flash persistence (saves every 10 brews, loads on boot) |

---

## Power Monitoring

| Feature                         | Status | Module             | Notes                                                    |
| ------------------------------- | ------ | ------------------ | -------------------------------------------------------- |
| **PZEM-004T Integration**       | ✅     | `pzem.c`           | Modbus RTU protocol, reads at 20Hz in sensor cycle       |
| **Real Power Measurement**      | ✅     | `pzem.c`           | Voltage, current, power, energy, frequency, power factor |
| **Power Estimation (Fallback)** | ✅     | `control_common.c` | Theoretical calculation when PZEM unavailable            |
| **Current Monitoring**          | ✅     | `pzem.c`           | Real-time current from PZEM, fallback to estimation      |
| **Power Limit Enforcement**     | ✅     | `control_common.c` | Validates heating strategies against limits              |
| **Modbus RTU Protocol**         | ✅     | `pzem.c`           | CRC16 validation, error handling, timeout management     |

---

## Testing & Validation

| Feature                            | Status | Module       | Notes                                                     |
| ---------------------------------- | ------ | ------------ | --------------------------------------------------------- |
| **Simulation Mode**                | ✅     | `hardware.c` | Full hardware simulation                                  |
| **Unit Testing**                   | ❌     | N/A          | Test framework not implemented                            |
| **Integration Testing**            | ⚠️     | N/A          | Basic testing done, full hardware testing pending         |
| **Hardware Testing**               | ⚠️     | N/A          | Full system test with real hardware pending               |
| **Temperature Control Validation** | ⚠️     | N/A          | Accuracy validation pending                               |
| **Brew Cycle Timing Validation**   | ⚠️     | N/A          | Timing validation pending                                 |
| **Safety System Load Testing**     | ⚠️     | N/A          | Safety system under load testing pending                  |
| **Performance Testing**            | ⚠️     | N/A          | Control loop timing analysis pending                      |
| **Safety Testing**                 | ⚠️     | N/A          | Individual interlocks tested, full system testing pending |

---

## Documentation

| Feature                       | Status | Location                                 | Notes                                         |
| ----------------------------- | ------ | ---------------------------------------- | --------------------------------------------- |
| **Pico Architecture**         | ✅     | `docs/pico/Architecture.md`              | Complete system architecture                  |
| **Pico Requirements**         | ✅     | `docs/pico/Requirements.md`              | Full requirements spec                        |
| **Communication Protocol**    | ✅     | `docs/shared/Communication_Protocol.md`  | Complete protocol spec                        |
| **Pico Implementation Plan**  | ✅     | `docs/pico/Implementation_Plan.md`       | Pico development status                       |
| **ESP32 Implementation Plan** | ✅     | `docs/esp32/Implementation_Plan.md`      | ESP32 development status                      |
| **MQTT Integration**          | ✅     | `docs/esp32/integrations/MQTT.md`        | Home Assistant integration                    |
| **Web API**                   | ✅     | `docs/esp32/integrations/Web_API.md`     | REST API documentation                        |
| **Water Management Docs**     | ✅     | `docs/pico/features/Water_Management.md` | Water system documentation                    |
| **Shot Timer Guide**          | ✅     | `docs/pico/features/Shot_Timer.md`       | Shot timer implementation                     |
| **Cleaning Mode Docs**        | ✅     | `docs/pico/features/Cleaning_Mode.md`    | Cleaning mode documentation                   |
| **Statistics Docs**           | ✅     | `docs/pico/features/Statistics.md`       | Statistics and analytics documentation        |
| **Error Handling Docs**       | ✅     | `docs/pico/features/Error_Handling.md`   | Error handling and recovery documentation     |
| **Setup Guide**               | ✅     | `SETUP.md`                               | Development environment setup and OTA updates |
| **Bootloader Implementation** | ✅     | `src/pico/src/bootloader.c`              | Serial bootloader for OTA firmware updates    |
| **Code Comments**             | ✅     | All source files                         | Inline documentation                          |
| **API Documentation**         | ✅     | Header files                             | Function documentation                        |
| **Test Procedures**           | ⚠️     | N/A                                      | Hardware test procedures pending              |

---

## Remaining Work Summary

### High Priority (Phase 6)

1. **Hardware Integration Testing** ⚠️

   - Full system test with real hardware
   - Temperature control accuracy validation
   - Brew cycle timing validation
   - Safety system under load testing

2. **Performance Optimization** ⚠️

   - Control loop timing analysis
   - Communication latency optimization
   - Memory usage optimization
   - CPU load balancing verification
   - Performance benchmarking and profiling

3. **Test Procedures** ⚠️
   - Hardware test procedures documentation
   - Integration test procedures
   - Safety system test procedures

### Medium Priority (Optional Enhancements)

1. ✅ **PZEM-004T Integration** (COMPLETE)

   - ✅ UART driver for PZEM-004T (Modbus RTU)
   - ✅ Real-time power monitoring (voltage, current, power, energy, frequency, PF)
   - ✅ Integrated into sensor reading cycle (20Hz)
   - ✅ Automatic fallback to theoretical estimation if unavailable

2. ✅ **Statistics Feature** (MOSTLY COMPLETE)

   - ✅ Overall statistics (total, average, min, max)
   - ✅ Time-based statistics (daily, weekly, monthly)
   - ✅ Historical data (last 100 brews in RAM, last 50 persisted to flash)
   - ✅ Automatic recording on brew completion
   - ✅ Flash persistence (saves every 10 brews, loads on boot)
   - ⚠️ Protocol integration: MSG_STATISTICS implemented, command handler pending
   - ⚠️ RTC support (structure in place, uses boot time - RTC hardware integration pending)
   - See `statistics.c` and [Statistics.md](../pico/features/Statistics.md) for implementation

3. **Enhanced Error Recovery** ✅ (Error tracking and reporting implemented, retry logic still pending)

   - Automatic retry logic for sensor failures
   - Improved recovery procedures
   - Extended error scenarios testing

4. **Cleaning Mode Enhancements** ⚠️
   - ✅ Brew counter persistence to flash (COMPLETE)
   - ⚠️ Status payload integration (brew count/reminder in MSG_STATUS) - pending

### Low Priority (Future Enhancements)

1. **Unit Testing Framework** ❌

   - Test framework setup
   - Unit tests for critical functions
   - Automated testing

2. **Thermoblock Support** ❌

   - Flow heater control (no boiler)
   - Flow-based temperature control

3. **Advanced Features** ❌
   - Additional heating strategies
   - Advanced PID tuning algorithms
   - Predictive temperature control

---

## ESP32 Features

| Feature                    | Status | Module                    | Notes                                |
| -------------------------- | ------ | ------------------------- | ------------------------------------ |
| **WiFi Manager**           | ✅     | `wifi_manager.cpp`        | AP mode setup, STA mode connection   |
| **Web Server**             | ✅     | `web_server.cpp`          | REST API, WebSocket, LittleFS        |
| **MQTT Client**            | ✅     | `mqtt_client.cpp`         | Home Assistant integration           |
| **BLE Scale Manager**      | ✅     | `scale/scale_manager.cpp` | Acaia, Felicita, Lunar scales        |
| **Brew-by-Weight**         | ✅     | `brew_by_weight.cpp`      | Automatic weight-based stopping      |
| **Display/UI**             | ✅     | `display/`, `ui/`         | LVGL-based round display             |
| **State Manager**          | ✅     | `state/state_manager.cpp` | Settings, stats, history persistence |
| **Schedules & Automation** | ✅     | `state/state_manager.cpp` | Time-based on/off, auto power-off    |
| **Eco Mode**               | ✅     | Multiple                  | Reduced temp after idle, auto-wake   |
| **Cloud Integration**      | ✅     | `cloud/`                  | Remote monitoring and control        |
| **OTA Updates**            | ✅     | `web_server.cpp`          | ESP32 and Pico firmware updates      |
| **First Run Wizard**       | ✅     | Web UI                    | Guided setup flow                    |

### Schedule & Auto Power-Off

| Feature                 | Status | Notes                                     |
| ----------------------- | ------ | ----------------------------------------- |
| **Multiple Schedules**  | ✅     | Up to 10 schedules, persisted to LittleFS |
| **Day Selection**       | ✅     | Weekdays, weekends, specific days         |
| **Turn On/Off Actions** | ✅     | Each schedule can turn machine on or off  |
| **Heating Strategy**    | ✅     | Configurable strategy for ON schedules    |
| **Auto Power-Off**      | ✅     | Idle timeout (5-480 minutes)              |
| **Activity Detection**  | ✅     | Brewing, encoder, commands reset timer    |
| **Web UI**              | ✅     | `/schedules` page for configuration       |
| **REST API**            | ✅     | `/api/schedules/*` endpoints              |
| **WebSocket Commands**  | ✅     | Real-time schedule management             |

---

## Statistics

- **Total Features:** ~165
- **Completed:** ~150 (91%)
- **Partial:** ~8 (5%)
- **Not Implemented:** ~7 (4%)

---

## Next Steps

1. **Immediate:** Hardware integration testing
2. **Short Term:** Performance validation and optimization (error handling enhancements completed)
3. **Medium Term:** Test procedures documentation
4. **Optional:** Status payload updates (brew count/reminder in MSG_STATUS), MSG_CMD_GET_STATISTICS handler, enhanced error recovery, RTC hardware integration

---

**Note:** The firmware is functionally complete for **dual boiler, single boiler, and heat exchanger machines**. All major features including PZEM power monitoring, cleaning mode (with brew counter persistence), statistics (with persistence), OTA firmware updates (serial bootloader), comprehensive error handling, and multi-machine support are implemented. Remaining work focuses on testing, validation, and minor enhancements.

**Machine Type Support:** The firmware supports three machine types with **separate control implementations** (compile-time file selection):

- **Dual Boiler** (`control_dual_boiler.c`) - Independent brew/steam PIDs, all heating strategies
- **Single Boiler** (`control_single_boiler.c`) - Mode switching with configurable timeouts
- **Heat Exchanger** (`control_heat_exchanger.c`) - Three modes: Temperature PID, Pressure PID, or Pressurestat (monitor only)

Build all types with `cmake .. -DBUILD_ALL_MACHINES=ON` or a single type with `-DMACHINE_TYPE=<TYPE>`.

---

## Future Roadmap

Features planned for future releases, organized by priority and complexity.

### Brewing Enhancements

| Feature                    | Priority | Complexity | Description                                                                 |
| -------------------------- | -------- | ---------- | --------------------------------------------------------------------------- |
| **Shot Profiling**         | High     | High       | Pressure/flow profiles during extraction (pre-infusion ramp, peak, decline) |
| **Pressure Profiling**     | High     | High       | Variable pressure curves: flat, declining, bloom, custom profiles           |
| **Flow Profiling**         | High     | High       | Target flow rate curves during extraction                                   |
| **Gear Pump Support**      | High     | Medium     | Variable speed pump control for flow/pressure profiling                     |
| **Flow Meter Integration** | High     | Medium     | Gicar-style flow meters for precise volumetric dosing                       |
| **Recipe Management**      | Medium   | Medium     | Save/recall shot recipes (temp, pressure profile, dose, yield)              |
| **Shot Replication**       | Medium   | High       | Replay successful shots with same parameters                                |
| **Auto-Purge**             | Low      | Low        | Automatic group head flush before/after shots                               |

### Hardware Support

| Feature                        | Priority | Complexity | Description                                                             |
| ------------------------------ | -------- | ---------- | ----------------------------------------------------------------------- |
| **Gear Pump Control**          | High     | Medium     | PWM/analog control for variable-speed gear pumps (Fluid-o-Tech, Procon) |
| **Dimmer/Triac Control**       | Medium   | Medium     | Vibratory pump speed control via phase-angle dimming                    |
| **Flow Meter Driver**          | High     | Low        | Pulse counting for Hall-effect flow meters                              |
| **Grinder Integration**        | Medium   | Medium     | Start/stop grinder, single-dose workflow                                |
| **Thermoblock Support**        | Low      | High       | Flow heater control for on-demand machines                              |
| **Additional Scale Protocols** | Low      | Low        | Hiroia Jimmy, Brewista, generic HX711 scales                            |
| **Rotary Pump Control**        | Medium   | Medium     | Bypass valve or VFD control for rotary pumps                            |

### Smart Features

| Feature                | Priority | Complexity | Description                                                          |
| ---------------------- | -------- | ---------- | -------------------------------------------------------------------- |
| **PID Auto-Tune**      | Medium   | High       | Automatic PID parameter optimization (Ziegler-Nichols, relay method) |
| **Shot Analytics**     | Medium   | Medium     | Extraction analysis, channeling detection, shot scoring              |
| **User Profiles**      | Medium   | Medium     | Per-user temperature/recipe preferences                              |
| **Voice Control**      | Low      | Medium     | Alexa/Google Home/HomeKit integration                                |
| **Predictive Heating** | Low      | High       | ML-based pre-heating based on usage patterns                         |
| **Data Export**        | Medium   | Low        | CSV/JSON export of shot history for external analysis                |

### Advanced Temperature Control

| Feature                       | Priority | Complexity | Description                                          |
| ----------------------------- | -------- | ---------- | ---------------------------------------------------- |
| **Temperature Profiling**     | Medium   | Medium     | Variable brew temp during extraction                 |
| **Group Head PID**            | Medium   | Medium     | Active group head temperature control (heated group) |
| **Temperature Surfing**       | Low      | Low        | HX flush timing recommendations                      |
| **Thermal Stability Scoring** | Low      | Medium     | Rate temperature stability before brewing            |

### Connectivity & Integration

| Feature                | Priority | Complexity | Description                               |
| ---------------------- | -------- | ---------- | ----------------------------------------- |
| **Apple HomeKit**      | Medium   | Medium     | Native HomeKit integration via HAP        |
| **Matter Support**     | Low      | High       | Matter/Thread smart home protocol         |
| **Bluetooth Remote**   | Low      | Low        | Simple BLE remote for basic control       |
| **Multi-Machine Sync** | Low      | High       | Coordinate multiple machines (café setup) |

### Maintenance & Diagnostics

| Feature                    | Priority | Complexity | Description                                  |
| -------------------------- | -------- | ---------- | -------------------------------------------- |
| **Predictive Maintenance** | Medium   | High       | Component wear tracking, service predictions |
| **Diagnostic Mode**        | Medium   | Medium     | Comprehensive self-test and reporting        |
| **Remote Diagnostics**     | Low      | Medium     | Cloud-based troubleshooting support          |
| **Component Hour Meters**  | Low      | Low        | Track runtime for pump, heaters, solenoids   |

---

### Implementation Notes

**Gear Pump Support:**

- Requires PWM output (GPIO + MOSFET driver) or 0-10V analog output
- Flow meter feedback for closed-loop control
- Soft start/stop ramps to prevent pressure spikes
- Compatible pumps: Fluid-o-Tech MG200-series, Procon

**Shot Profiling:**

- Requires either gear pump OR pressure transducer feedback with vibratory pump
- Profile storage in flash (10-20 profiles)
- Real-time profile interpolation in control loop
- WebSocket streaming for live profile visualization

**Flow Meter Integration:**

- Hall-effect sensors: pulse counting via GPIO interrupt
- Typical: 0.5-2.0 mL/pulse calibration
- Integration with brew-by-volume (alternative to brew-by-weight)
