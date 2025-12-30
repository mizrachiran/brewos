# Communication Protocol Specification

## Overview

Binary protocol for communication between the Pico (RP2350) and ESP32 display module.

**Protocol Version:** 1.1  
**Transport:** UART  
**Baud Rate:** 921600  
**Format:** 8N1  
**GPIO:** Pico TX=GPIO0, RX=GPIO1

---

## Design Principles

1. **Simple** - Single unified status message with all data
2. **Compact** - ~44 bytes per status update (vs ~250 for JSON)
3. **Fast** - 921600 baud, sub-millisecond packet transmission
4. **Reliable** - CRC16 integrity check, sequence numbers, automatic retry
5. **Atomic** - Complete machine state in every packet
6. **Robust** - Parser timeout, backpressure, protocol versioning (v1.1+)

---

## Packet Structure

All packets follow this format:

```
┌───────┬──────────┬────────┬─────┬─────────────────┬───────────┐
│ START │ MSG_TYPE │ LENGTH │ SEQ │     PAYLOAD     │   CRC16   │
│ 1byte │  1byte   │ 1byte  │1byte│   0-56 bytes    │  2bytes   │
│ 0xAA  │          │        │     │                 │           │
└───────┴──────────┴────────┴─────┴─────────────────┴───────────┘
```

| Field    | Size | Description                               |
| -------- | ---- | ----------------------------------------- |
| START    | 1    | Sync byte, always `0xAA`                  |
| MSG_TYPE | 1    | Message type identifier                   |
| LENGTH   | 1    | Payload length in bytes (0-56)            |
| SEQ      | 1    | Sequence number (0-255, wraps)            |
| PAYLOAD  | 0-56 | Message-specific data                     |
| CRC16    | 2    | CRC-16-CCITT over TYPE+LENGTH+SEQ+PAYLOAD |

**Maximum packet size:** 62 bytes (4 header + 56 payload + 2 CRC)

---

## CRC16 Calculation

CRC-16-CCITT (polynomial 0x1021, initial value 0xFFFF):

```c
uint16_t crc16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}
```

CRC is calculated over bytes: `[MSG_TYPE, LENGTH, SEQ, PAYLOAD...]`  
CRC is transmitted little-endian (low byte first).

---

## Message Types

### Pico → ESP32

| Type            | Value | Description                                               | Interval          |
| --------------- | ----- | --------------------------------------------------------- | ----------------- |
| MSG_STATUS      | 0x01  | **Unified status** (temps, pressure, power, state)        | 500ms             |
| MSG_ALARM       | 0x02  | Alarm event (immediate, safety-critical)                  | On event          |
| MSG_BOOT        | 0x03  | Boot complete notification                                | Once              |
| MSG_ACK         | 0x04  | Command acknowledgment                                    | On command        |
| MSG_CONFIG      | 0x05  | **Full configuration** (setpoints, PID, heating, etc.)    | On request/change |
| MSG_DEBUG       | 0x06  | Debug log message (see [Debugging.md](Debugging.md))      | On log event      |
| MSG_DEBUG_RESP  | 0x07  | Debug command response                                    | On debug command  |
| MSG_ENV_CONFIG  | 0x08  | **Environmental config** (voltage, current limits)        | On request/change |
| MSG_POWER_METER | 0x0B  | **Power meter reading** (voltage, current, power, energy) | 1 second          |
| MSG_HANDSHAKE   | 0x0C  | **Protocol handshake** (version negotiation)              | Once on boot      |
| MSG_NACK        | 0x0D  | **Negative ACK** (busy/backpressure signal)               | On overload       |

### ESP32 → Pico

| Type                         | Value | Description                                       |
| ---------------------------- | ----- | ------------------------------------------------- |
| MSG_CMD_SET_TEMP             | 0x10  | Set temperature setpoint                          |
| MSG_CMD_SET_PID              | 0x11  | Set PID parameters                                |
| MSG_CMD_BREW                 | 0x13  | Start/stop brew                                   |
| MSG_CMD_MODE                 | 0x14  | Change operating mode                             |
| MSG_CMD_CONFIG               | 0x15  | Set runtime configuration                         |
| MSG_CMD_GET_CONFIG           | 0x16  | **Request current configuration**                 |
| MSG_CMD_GET_ENV_CONFIG       | 0x17  | **Request environmental configuration**           |
| MSG_CMD_DEBUG                | 0x1D  | Debug commands (see [Debugging.md](Debugging.md)) |
| MSG_CMD_BOOTLOADER           | 0x1F  | Enter bootloader for OTA                          |
| MSG_CMD_POWER_METER_CONFIG   | 0x21  | Configure power meter (enable/disable)            |
| MSG_CMD_POWER_METER_DISCOVER | 0x22  | Start power meter auto-discovery                  |

### Bidirectional

| Type     | Value | Description           |
| -------- | ----- | --------------------- |
| MSG_PING | 0x00  | Heartbeat (echo back) |

---

## Payload Definitions

### MSG_STATUS (0x01) - Pico → ESP32

**Unified status message** sent every 500ms containing complete machine state including power data. This provides an atomic snapshot of all machine parameters in a single packet.

```c
typedef struct __attribute__((packed)) {
    int16_t brew_temp;          // Celsius * 10 (0.1C resolution)
    int16_t steam_temp;         // Celsius * 10
    int16_t group_temp;         // Celsius * 10 (if available)
    uint16_t pressure;          // Bar * 100 (0.01 bar resolution)
    int16_t brew_setpoint;      // Celsius * 10
    int16_t steam_setpoint;     // Celsius * 10
    uint8_t brew_output;        // 0-100% (SSR duty cycle)
    uint8_t steam_output;       // 0-100% (SSR duty cycle)
    uint8_t pump_output;        // 0-100% (pump duty)
    uint8_t state;              // Machine state (see STATE_* in protocol_defs.h)
    uint8_t flags;              // Status flags (see STATUS_FLAG_* in protocol_defs.h)
    uint8_t water_level;        // 0-100%
    uint16_t power_watts;       // Current power draw (estimated)
    uint32_t uptime_ms;         // Milliseconds since boot
} status_payload_t;  // 22 bytes

// Full packet: 4 (header) + 22 (payload) + 2 (CRC) = 28 bytes
// TX time @ 921600 baud: 0.30ms
```

**flags bitmask:**
| Bit | Name | Meaning when SET (1) |
|-----|------|---------------------|
| 0 | STATUS_FLAG_BREWING | Currently brewing |
| 1 | STATUS_FLAG_HEATING | Heaters active (output > 0%) |
| 2 | STATUS_FLAG_PUMP_ON | Pump running |
| 3 | STATUS_FLAG_WATER_LOW | Water level low |
| 4 | STATUS_FLAG_ALARM | Alarm condition active |
| 5-7 | Reserved | |

**Note:** Status flags are different from state enum:

- **State enum** (`state` field): Single operational state (IDLE, HEATING, READY, BREWING, etc.)
- **Status flags** (`flags` field): Multiple boolean conditions that can be true simultaneously
- Example: In `STATE_READY`, `STATUS_FLAG_HEATING` may be true (PID maintaining temp), but `STATE_HEATING` is false (not in initial heating phase)

**state enum:**
| Value | Name | Description |
|-------|------|-------------|
| 0 | STATE_INIT | Initializing |
| 1 | STATE_IDLE | Machine on but not heating (MODE_IDLE) |
| 2 | STATE_HEATING | Initial heating phase (cold to hot) |
| 3 | STATE_READY | At temperature, PID maintaining, ready to brew |
| 4 | STATE_BREWING | Brewing in progress |
| 5 | STATE_FAULT | Fault condition |
| 6 | STATE_SAFE | Safe state (all outputs off) |

**heating_strategy enum:**
| Value | Name | Description |
|-------|------|-------------|
| 0 | HEAT_BREW_ONLY | Only brew boiler heats |
| 1 | HEAT_SEQUENTIAL | Brew first, then steam |
| 2 | HEAT_PARALLEL | Both simultaneously |
| 3 | HEAT_SMART_STAGGER | Both with limited combined duty |

---

### MSG_ALARM (0x02) - Pico → ESP32

Sent immediately when alarm condition occurs or clears.

```c
typedef struct __attribute__((packed)) {
    uint16_t code;       // Alarm code (see table)
    uint8_t  severity;   // 0=info, 1=warning, 2=critical
    uint8_t  active;     // 1=alarm raised, 0=alarm cleared
} alarm_payload_t;  // 4 bytes
```

**Alarm codes:**
| Code | Severity | Description |
|------|----------|-------------|
| 0x0001 | CRITICAL | Watchdog timeout |
| 0x0002 | CRITICAL | Brew NTC open circuit |
| 0x0003 | CRITICAL | Brew NTC short circuit |
| 0x0004 | CRITICAL | Steam NTC open circuit |
| 0x0005 | CRITICAL | Steam NTC short circuit |
| 0x0006 | CRITICAL | Brew over-temperature |
| 0x0007 | CRITICAL | Steam over-temperature |
| 0x0008 | CRITICAL | Steam boiler level low |
| 0x0009 | CRITICAL | No water reservoir |
| 0x000A | CRITICAL | Tank level low |
| 0x0020 | WARNING | Thermocouple fault |
| 0x0021 | WARNING | PZEM communication timeout |
| 0x0022 | WARNING | ESP32 communication timeout |
| 0x0030 | INFO | Brew started |
| 0x0031 | INFO | Brew completed |

---

### MSG_BOOT (0x03) - Pico → ESP32

Sent once after successful initialization.

```c
typedef struct __attribute__((packed)) {
    uint8_t  version_major;      // Firmware version major
    uint8_t  version_minor;      // Firmware version minor
    uint8_t  version_patch;      // Firmware version patch
    uint8_t  machine_type;      // Machine config enum (see MACHINE_TYPE_*)
    uint8_t  pcb_type;          // PCB type (see PCB_TYPE_* in pcb_config.h)
    uint8_t  pcb_version_major;  // PCB version major
    uint8_t  pcb_version_minor;   // PCB version minor
    uint32_t reset_reason;       // Reset reason (future use)
} boot_payload_t;  // 10 bytes
```

**PCB Type Values:**
| Value | Name | Description |
|-------|------|-------------|
| 0 | `PCB_TYPE_UNKNOWN` | Unknown/not configured |
| 1 | `PCB_TYPE_ECM_V1` | ECM Synchronika V1 PCB |
| 2 | `PCB_TYPE_ECM_V2` | ECM Synchronika V2 PCB (future) |
| 255 | `PCB_TYPE_CUSTOM` | Custom PCB |

**PCB Version:**

- Major: Pinout or significant hardware changes
- Minor: Component changes, same pinout
- Patch: Bug fixes, same hardware

---

### MSG_ACK (0x04) - Pico → ESP32

Sent in response to commands.

```c
typedef struct __attribute__((packed)) {
    uint8_t  cmd_type;    // Original command type
    uint8_t  cmd_seq;     // Original command sequence
    uint8_t  result;      // ACK result code (see ACK_* constants)
    uint8_t  reserved;    // Reserved for future use
} ack_payload_t;  // 4 bytes
```

**ACK Result Codes** (from `protocol_defs.h`):

| Code | Constant              | Description                            |
| ---- | --------------------- | -------------------------------------- |
| 0x00 | `ACK_SUCCESS`         | Command executed successfully          |
| 0x01 | `ACK_ERROR_INVALID`   | Invalid command or parameters          |
| 0x02 | `ACK_ERROR_REJECTED`  | Command rejected (e.g., safety, state) |
| 0x03 | `ACK_ERROR_FAILED`    | Command failed (e.g., hardware error)  |
| 0x04 | `ACK_ERROR_TIMEOUT`   | Operation timed out                    |
| 0x05 | `ACK_ERROR_BUSY`      | System busy, try again later           |
| 0x06 | `ACK_ERROR_NOT_READY` | System not ready for this command      |

**Examples:**

- `MSG_CMD_SET_TEMP` with invalid payload → `ACK_ERROR_INVALID`
- `MSG_CMD_MODE` during active brew → `ACK_ERROR_REJECTED`
- `MSG_CMD_BOOTLOADER` timeout → `ACK_ERROR_TIMEOUT`
- `MSG_CMD_CLEANING_START` when already cleaning → `ACK_ERROR_REJECTED`

---

### MSG_CONFIG (0x05) - Pico → ESP32

**Full configuration** (setpoints, PID, heating strategy). Sent:

- Once after boot (after MSG_BOOT)
- In response to MSG_CMD_GET_CONFIG
- After any configuration change

```c
typedef struct __attribute__((packed)) {
    int16_t brew_setpoint;      // Celsius * 10
    int16_t steam_setpoint;     // Celsius * 10
    int16_t temp_offset;        // Celsius * 10 (calibration offset)
    uint16_t pid_kp;            // * 100 (e.g., 500 = 5.00)
    uint16_t pid_ki;            // * 100
    uint16_t pid_kd;            // * 100
    uint8_t heating_strategy;   // See HEAT_STRATEGY_* in protocol_defs.h
    uint8_t machine_type;       // See MACHINE_TYPE_* in protocol_defs.h
} config_payload_t;  // 15 bytes

// Full packet: 4 (header) + 15 (payload) + 2 (CRC) = 21 bytes
// Note: Environmental config is sent separately via MSG_ENV_CONFIG
```

**Configuration Sync Flow:**

```
┌────────────────┐                              ┌────────────────┐
│     Pico       │                              │     ESP32      │
└───────┬────────┘                              └───────┬────────┘
        │                                               │
        │  ══════════ BOOT SEQUENCE ══════════         │
        │                                               │
        │─────────── MSG_BOOT ────────────────────────►│
        │                                               │
        │─────────── MSG_CONFIG ──────────────────────►│  ESP32 now knows
        │                                               │  full configuration
        │                                               │
        │  ══════════ ESP32 RECONNECTS ══════════      │
        │                                               │
        │◄────────── MSG_CMD_GET_CONFIG ───────────────│
        │                                               │
        │─────────── MSG_CONFIG ──────────────────────►│
        │                                               │
        │  ══════════ SETTING CHANGED ══════════       │
        │                                               │
        │◄────────── MSG_CMD_SET_TEMP ─────────────────│
        │                                               │
        │─────────── MSG_ACK (ACK_SUCCESS) ────────────►│
        │                                               │
        │─────────── MSG_CONFIG ──────────────────────►│  Updated config
        │                                               │
```

---

### MSG_CMD_SET_TEMP (0x10) - ESP32 → Pico

Set temperature setpoint for a boiler.

```c
typedef struct __attribute__((packed)) {
    uint8_t  target;      // 0=brew, 1=steam
    int16_t  temperature; // Celsius * 10 (e.g., 930 = 93.0°C)
} cmd_set_temp_t;  // 3 bytes
```

---

### MSG_CMD_SET_PID (0x11) - ESP32 → Pico

Set PID parameters for a boiler.

```c
typedef struct __attribute__((packed)) {
    uint8_t  target;      // 0=brew, 1=steam
    uint16_t kp;          // * 100 (e.g., 500 = 5.00)
    uint16_t ki;          // * 100
    uint16_t kd;          // * 100
} cmd_set_pid_t;  // 7 bytes
```

---

### MSG_CMD_BREW (0x13) - ESP32 → Pico

Start or stop brew cycle.

```c
typedef struct __attribute__((packed)) {
    uint8_t  action;      // 0=stop, 1=start
} cmd_brew_t;  // 1 byte
```

---

### MSG_CMD_MODE (0x14) - ESP32 → Pico

Change machine operating mode (turns machine on/off).

```c
typedef struct __attribute__((packed)) {
    uint8_t  mode;        // machine_mode_t: 0=MODE_IDLE, 1=MODE_BREW, 2=MODE_STEAM
} cmd_mode_t;  // 1 byte
```

**Mode Values:**
| Value | Name | Description |
|-------|------|-------------|
| 0 | MODE_IDLE | Machine idle (on but not heating). Pico/ESP32 stay on. |
| 1 | MODE_BREW | Brew mode: heats brew boiler (for espresso) |
| 2 | MODE_STEAM | Steam mode: heats both boilers (steam for milk, brew for espresso) |

**Behavior:**

- `MODE_IDLE`: Machine idle, no heating. Remote control, schedules, web access continue.
- `MODE_BREW`: Machine turns on, heats brew boiler to brew temperature.
- `MODE_STEAM`: Machine turns on, heats both boilers sequentially (brew first, then steam).

**Note:** Mode change is rejected if machine is currently brewing.

---

### MSG_CMD_CONFIG (0x15) - ESP32 → Pico

Set runtime configuration parameters.

```c
typedef struct __attribute__((packed)) {
    uint8_t  config_type;     // Configuration category (see table)
    uint8_t  data[];          // Variable payload based on config_type
} cmd_config_t;

// config_type values
#define CONFIG_HEATING_STRATEGY  0x01   // Heating strategy (0-3)
#define CONFIG_PREINFUSION       0x02   // Pre-infusion settings
#define CONFIG_ENVIRONMENTAL     0x05   // Environmental electrical config (voltage, current limits)
```

#### CONFIG_HEATING_STRATEGY (0x01)

Set the heating strategy for dual-boiler machines.

```c
typedef struct __attribute__((packed)) {
    uint8_t strategy;    // 0=BREW_ONLY, 1=SEQUENTIAL, 2=PARALLEL, 3=SMART_STAGGER
} config_heating_strategy_t;  // 1 byte
```

#### CONFIG_PREINFUSION (0x02)

Set pre-infusion parameters.

```c
typedef struct __attribute__((packed)) {
    uint8_t  enabled;     // 0=disabled, 1=enabled
    uint16_t on_ms;       // Pump ON time in milliseconds
    uint16_t pause_ms;    // Soak/pause time in milliseconds
} config_preinfusion_t;  // 5 bytes
```

**Usage:**

- ESP32 sends `MSG_CMD_CONFIG` with `config_type = CONFIG_PREINFUSION`
- Pico updates pre-infusion settings and persists to flash
- Pico responds with `MSG_ACK`

#### CONFIG_ENVIRONMENTAL (0x05)

Set environmental electrical configuration (voltage, current limits). This is installation-specific and can be updated if the machine is moved to a different location.

```c
typedef struct __attribute__((packed)) {
    uint16_t nominal_voltage;    // 120, 230, 240, etc. (V)
    float    max_current_draw;   // 10.0, 16.0, etc. (A) - 4 bytes
} config_environmental_t;  // 6 bytes
```

**Usage:**

- ESP32 sends `MSG_CMD_CONFIG` with `config_type = CONFIG_ENVIRONMENTAL`
- Pico updates environmental config and recalculates electrical state
- Pico responds with `MSG_ACK` (with result code: ACK_SUCCESS, ACK_ERROR_INVALID, etc.)
- Pico may send updated `MSG_CONFIG` if environmental config affects heating strategy

**Example:**

```c
// ESP32 → Pico: Set 230V / 16A limit
cmd_config_t cmd = {
    .config_type = CONFIG_ENVIRONMENTAL,
    .data = {
        // config_environmental_t payload
        0xE6, 0x00,  // nominal_voltage = 230 (little-endian)
        0x00, 0x00, 0x80, 0x41  // max_current_draw = 16.0f (IEEE 754 float)
    }
};
```

**Note:** Environmental configuration is separate from machine configuration. Machine electrical specs (heater power ratings) are fixed per machine model and cannot be changed via protocol.

---

### MSG_CMD_GET_CONFIG (0x16) - ESP32 → Pico

**Request current configuration.** No payload - Pico responds with MSG_CONFIG.

```c
// No payload - just header with LENGTH=0
// Pico responds with MSG_CONFIG containing full configuration
```

Use cases:

- ESP32 just connected/reconnected
- ESP32 UI needs to refresh settings display
- Sync check after communication loss

---

### MSG_ENV_CONFIG (0x08) - Pico → ESP32

**Environmental configuration** (voltage, current limits). Sent:

- Once after boot (after MSG_BOOT and MSG_CONFIG)
- In response to MSG_CMD_GET_ENV_CONFIG
- After environmental configuration change (following MSG_ACK)

```c
typedef struct __attribute__((packed)) {
    uint16_t nominal_voltage;    // 120, 230, 240, etc. (V)
    float    max_current_draw;   // 10.0, 16.0, etc. (A) - 4 bytes
    // Calculated values (for information)
    float    brew_heater_current;    // Calculated: brew_heater_power / nominal_voltage
    float    steam_heater_current;   // Calculated: steam_heater_power / nominal_voltage
    float    max_combined_current;   // Calculated: max_current_draw * 0.95
} env_config_payload_t;  // 18 bytes

// Full packet: 4 (header) + 18 (payload) + 2 (CRC) = 24 bytes
```

**Configuration Flow:**

```
┌────────────────┐                              ┌────────────────┐
│     Pico       │                              │     ESP32      │
└───────┬────────┘                              └───────┬────────┘
        │                                               │
        │  ══════════ BOOT SEQUENCE ══════════         │
        │                                               │
        │─────────── MSG_BOOT ────────────────────────►│
        │─────────── MSG_CONFIG ──────────────────────►│
        │─────────── MSG_ENV_CONFIG ──────────────────►│  ESP32 now knows
        │                                               │  all configurations
        │  ══════════ ESP32 RECONNECTS ══════════      │
        │                                               │
        │◄────────── MSG_CMD_GET_ENV_CONFIG ──────────│
        │                                               │
        │─────────── MSG_ENV_CONFIG ──────────────────►│
        │                                               │
        │  ══════════ SETTING CHANGED ══════════       │
        │                                               │
        │◄────────── MSG_CMD_CONFIG (ENV) ──────────────│
        │                                               │
        │─────────── MSG_ACK (ACK_SUCCESS) ────────────►│
        │─────────── MSG_ENV_CONFIG ──────────────────►│  Updated config
        │                                               │
```

---

### MSG_CMD_GET_ENV_CONFIG (0x17) - ESP32 → Pico

**Request environmental configuration.** No payload - Pico responds with MSG_ENV_CONFIG.

```c
// No payload - just header with LENGTH=0
// Pico responds with MSG_ENV_CONFIG containing environmental config
```

Use cases:

- ESP32 just connected/reconnected
- ESP32 UI needs to display/verify voltage and current limits
- User wants to check current environmental settings

---

### MSG_POWER_METER (0x0B) - Pico → ESP32

**Power meter reading** from hardware modules (PZEM, JSY, Eastron). Sent every 1 second when power metering is enabled.

```c
typedef struct __attribute__((packed)) {
    float voltage;        // Volts (RMS) - 4 bytes
    float current;        // Amps (RMS) - 4 bytes
    float power;          // Watts (Active) - 4 bytes
    float energy_import;  // kWh (from grid) - 4 bytes
    float energy_export;  // kWh (to grid, for solar) - 4 bytes
    float frequency;      // Hz - 4 bytes
    float power_factor;   // 0.0 - 1.0 - 4 bytes
    uint32_t timestamp;   // milliseconds when read - 4 bytes
    uint8_t valid;        // 1 if reading is valid - 1 byte
    uint8_t padding[3];   // Padding to 4-byte alignment - 3 bytes
} power_meter_reading_t;  // 36 bytes

// Full packet: 4 (header) + 36 (payload) + 2 (CRC) = 42 bytes
```

**Notes:**

- Only sent when power meter is enabled and connected
- `valid` flag is 0 if meter communication failed
- `energy_export` is 0 for non-bidirectional meters
- Pico handles hardware meters via UART1 (GPIO6/7) using Modbus protocol
- Supported meters: PZEM-004T, JSY-MK-163T/194T, Eastron SDM120/230

---

### MSG_CMD_POWER_METER_CONFIG (0x21) - ESP32 → Pico

Enable or disable power metering.

```c
typedef struct __attribute__((packed)) {
    uint8_t enabled;  // 0 = disable, 1 = enable
} cmd_power_meter_config_t;  // 1 byte
```

**Behavior:**

- When enabled: Pico initializes power meter on UART1 and starts sending MSG_POWER_METER
- When disabled: Pico stops polling meter and closes UART1
- Pico responds with MSG_ACK (success or error)

---

### MSG_CMD_POWER_METER_DISCOVER (0x22) - ESP32 → Pico

Start auto-discovery of connected power meter.

```c
// No payload - LENGTH = 0
```

**Behavior:**

- Pico tries each known meter type (PZEM @ 9600, JSY @ 4800, Eastron @ 2400/9600)
- First valid response is saved to configuration
- Takes ~10-15 seconds (tries 5 meter types with timeouts)
- Pico sends MSG_ACK when complete
- If meter found: Pico starts sending MSG_POWER_METER readings
- If not found: Pico sends MSG_ACK with error code

**Discovery Process:**

1. Try PZEM-004T @ 9600 baud, addr 0xF8
2. Try JSY-MK-163T @ 4800 baud, addr 0x01
3. Try JSY-MK-194T @ 4800 baud, addr 0x01
4. Try Eastron SDM120 @ 2400 baud, addr 0x01
5. Try Eastron SDM230 @ 9600 baud, addr 0x01

Each attempt: Send Modbus read request, wait 500ms, verify voltage reading is 50-300V.

---

### MSG_CMD_BOOTLOADER (0x1F) - ESP32 → Pico

Enter bootloader for OTA update.

**Payload:** Optional 4-byte magic header (0xFFFFFFFF) to indicate firmware streaming will follow.

**Response:** Pico should enter bootloader mode and prepare to receive firmware.

**Implementation:**

- ESP32 sends this command via normal protocol
- **Serial bootloader (recommended):** Pico enters bootloader mode and waits for firmware over UART
- **Hardware bootloader entry (fallback):** ESP32 can also use hardware bootloader entry (BOOTSEL + RUN pins)
- ESP32 streams firmware using bootloader protocol (see [Firmware Streaming Protocol](#firmware-streaming-protocol-bootloader-mode) section below)

**Implementation:**

- ESP32 side: Supports both serial and hardware bootloader entry
- Pico side: Serial bootloader receives firmware over UART, writes to flash, verifies checksums

See [Feature Status Table](Feature_Status_Table.md) for current implementation status.

```c
// No payload - LENGTH = 0
```

---

### MSG_PING (0x00) - Bidirectional

Heartbeat. Receiver echoes back the same packet.

```c
typedef struct __attribute__((packed)) {
    uint32_t timestamp;   // Sender's uptime_ms
} ping_payload_t;  // 4 bytes
```

---

## Timing

| Message         | Direction    | Interval                   | Priority |
| --------------- | ------------ | -------------------------- | -------- |
| MSG_STATUS      | Pico → ESP32 | 500ms                      | Normal   |
| MSG_POWER_METER | Pico → ESP32 | 1000ms (when enabled)      | Normal   |
| MSG_CONFIG      | Pico → ESP32 | On boot + on change        | Normal   |
| MSG_ALARM       | Pico → ESP32 | Immediate                  | High     |
| MSG_PING        | Either       | 5000ms (timeout detection) | Low      |
| Commands        | ESP32 → Pico | On demand                  | Normal   |

### Boot Sequence

```
Pico boots → MSG_BOOT → MSG_CONFIG → MSG_ENV_CONFIG → MSG_STATUS (repeating)
                                                     → MSG_POWER_METER (repeating, if enabled)
```

### After Config Change

```
ESP32 sends MSG_CMD_* → Pico sends MSG_ACK (with result code) → Pico sends MSG_CONFIG (if applicable)
```

**ACK Result Codes:**

- `ACK_SUCCESS` (0x00) - Command executed successfully
- `ACK_ERROR_INVALID` (0x01) - Invalid command or parameters
- `ACK_ERROR_REJECTED` (0x02) - Command rejected (e.g., safety, state)
- `ACK_ERROR_FAILED` (0x03) - Command failed (e.g., hardware error)
- `ACK_ERROR_TIMEOUT` (0x04) - Operation timed out
- `ACK_ERROR_BUSY` (0x05) - System busy, try again later
- `ACK_ERROR_NOT_READY` (0x06) - System not ready for this command

---

## Protocol v1.1 Features

### Overview

Protocol version 1.1 adds enterprise-grade robustness features:

- **Parser Timeout:** 500ms watchdog prevents stuck state machine
- **Sequence Validation:** Detects duplicate/out-of-order packets
- **Automatic Retry:** Up to 3 retries with 1-second ACK timeout
- **Protocol Handshake:** Version negotiation and capability exchange
- **Backpressure:** NACK signaling when command queue is full
- **Enhanced Diagnostics:** 15+ tracked metrics for monitoring

### Handshake Flow

The protocol handshake occurs at startup to negotiate capabilities and versions:

```
ESP32                                    Pico
  |                                        |
  |  MSG_HANDSHAKE (request)              |
  |  Payload: [proto_ver_major,           |
  |            proto_ver_minor,           |
  |            capabilities_flags,        |
  |            max_packet_size_hi,        |
  |            max_packet_size_lo]        |
  |--------------------------------------->|
  |                                        |
  |                 MSG_HANDSHAKE (reply) |
  |                  Payload: [1, 1, 0,   |
  |                            3, 0xE8,   |
  |                            0x03]      |
  |<---------------------------------------|
  |                                        |
  | (Protocol ready - normal operation)   |
```

**Handshake Payload (6 bytes):**

```c
typedef struct __attribute__((packed)) {
    uint8_t proto_ver_major;        // Protocol major version (1)
    uint8_t proto_ver_minor;        // Protocol minor version (1)
    uint8_t capabilities_flags;     // Reserved for future use
    uint8_t reserved;               // Reserved
    uint16_t max_packet_size;       // Maximum packet size (1000 bytes)
} handshake_payload_t;
```

**Capabilities Flags (future):**

- Bit 0: Supports MSG_NACK
- Bit 1: Supports retry mechanism
- Bit 2-7: Reserved

### Retry Mechanism

Commands are automatically retried if no ACK is received within 1 second:

```
ESP32                                    Pico
  |                                        |
  |  MSG_CMD_SET_TEMP (seq=42)            |
  |--------------------------------------->|
  |                                        |
  |  (waiting for ACK, timeout=1000ms)    |
  |                                        |
  |  ... 1000ms timeout ...               |
  |                                        |
  |  MSG_CMD_SET_TEMP (seq=42, retry 1)   |
  |--------------------------------------->|
  |                                        |
  |                        MSG_ACK (seq=42)|
  |<---------------------------------------|
  |                                        |
  | (Command succeeded on retry)          |
```

**Retry Parameters:**

- `PROTOCOL_RETRY_COUNT`: 3 maximum retries
- `PROTOCOL_ACK_TIMEOUT_MS`: 1000ms wait time
- `PROTOCOL_MAX_PENDING_CMDS`: 4 simultaneous commands

**Retry Logic:**

1. Send command with sequence number
2. Add to pending command array with timestamp
3. Wait for ACK with matching sequence number
4. If timeout expires, retry same sequence number
5. After 3 retries, mark command as failed

### Backpressure (NACK)

When the Pico command queue is full (≥3 pending commands), it sends MSG_NACK:

```
ESP32                                    Pico
  |                                        |
  |  MSG_CMD_BREW                         |
  |--------------------------------------->|
  |                                        |
  |  (Pico has 3 pending commands)        |
  |                                        |
  |                      MSG_NACK (BUSY)  |
  |  Payload: [cmd_type=0x13,             |
  |            cmd_seq=45,                |
  |            result=ACK_ERROR_BUSY]     |
  |<---------------------------------------|
  |                                        |
  | (ESP32 waits before retrying)         |
```

**NACK Payload (4 bytes):**

```c
typedef struct __attribute__((packed)) {
    uint8_t cmd_type;          // Original command type
    uint8_t cmd_seq;           // Original sequence number
    uint8_t result;            // Error code (ACK_ERROR_BUSY = 0x03)
    uint8_t reserved;          // Reserved
} nack_payload_t;
```

**Backpressure Threshold:**

- `PROTOCOL_BACKPRESSURE_THRESHOLD`: 3 pending commands
- Prevents command queue overflow
- ESP32 should implement exponential backoff

### Parser Timeout

The parser state machine has a 500ms watchdog to prevent stuck states:

```c
#define PROTOCOL_PARSER_TIMEOUT_MS 500

// In protocol_process_byte():
if (g_parser.state != PARSER_STATE_IDLE) {
    if (time_since_last_byte > PROTOCOL_PARSER_TIMEOUT_MS) {
        // Reset parser, log timeout error
        g_stats.parser_timeouts++;
        protocol_reset_parser();
    }
}
```

**Timeout Scenarios:**

- Incomplete packet received
- Communication interrupted mid-packet
- Corrupted packet length field

### Sequence Validation

Each packet has an 8-bit sequence number that increments:

```c
// Validate sequence number
if (seq == g_last_seq) {
    g_stats.duplicate_packets++;
    // Discard duplicate
} else if (seq < g_last_seq && g_last_seq - seq > 128) {
    // Sequence wrapped around (valid)
    g_last_seq = seq;
} else if (seq < g_last_seq) {
    g_stats.out_of_order_packets++;
    // Discard out-of-order
} else {
    g_last_seq = seq;
    // Process packet
}
```

**Sequence Rules:**

- Wraps at 255 → 0
- Duplicate detection prevents replay attacks
- Out-of-order detection catches timing issues

### Enhanced Diagnostics

Protocol statistics available via `protocol_get_stats()`:

```c
typedef struct {
    // Packet counters
    uint32_t packets_received;      // Total valid packets
    uint32_t packets_sent;          // Total packets transmitted
    uint32_t bytes_received;        // Total bytes processed
    uint32_t bytes_sent;            // Total bytes transmitted

    // Error counters
    uint16_t crc_errors;            // CRC checksum failures
    uint16_t packet_errors;         // Malformed packets
    uint16_t parser_timeouts;       // Parser watchdog triggers
    uint16_t duplicate_packets;     // Duplicate sequence numbers
    uint16_t out_of_order_packets;  // Out-of-order packets

    // Command tracking
    uint8_t pending_commands;       // Current pending count
    uint16_t retry_attempts;        // Total retry count
    uint16_t ack_timeouts;          // ACK timeout count
    uint16_t nack_received;         // NACK (backpressure) count

    // Protocol state
    bool handshake_complete;        // Version negotiation done
    uint8_t protocol_version;       // Negotiated protocol version
} protocol_stats_t;
```

**Diagnostic Use Cases:**

- Monitor `crc_errors` for EMI/wiring issues
- Track `retry_attempts` for reliability metrics
- Check `pending_commands` for flow control
- Verify `handshake_complete` on connection
- Alert on high `parser_timeouts` rate

---

## Error Handling

### Protocol Error Tracking

The protocol implementation tracks errors for diagnostics:

- **CRC Errors:** `protocol_get_crc_errors()` - Returns count of CRC mismatches
- **Packet Errors:** `protocol_get_packet_errors()` - Returns count of malformed packets
- **Error Reset:** `protocol_reset_error_counters()` - Clears error counters

### Receive Errors

| Condition                   | Action                                   | Error Tracking                             |
| --------------------------- | ---------------------------------------- | ------------------------------------------ |
| Invalid start byte          | Discard, wait for 0xAA                   | No tracking (expected)                     |
| CRC mismatch                | Discard packet, increment `g_crc_errors` | Logged with expected vs received CRC       |
| Invalid packet length       | Reset state, increment `g_packet_errors` | Logged with invalid length value           |
| Buffer overflow             | Reset state, increment `g_packet_errors` | Logged                                     |
| Unknown message type        | Discard, no ACK sent                     | No tracking (expected for future commands) |
| Timeout (incomplete packet) | Reset parser state after 100ms           | No tracking (handled by state machine)     |

**Error Reporting:**

- CRC errors are logged with both received and expected CRC values
- Packet errors include context (invalid length, buffer overflow)
- Error counts are available via protocol functions for diagnostics

### Communication Loss

| Condition                | Pico Action         | ESP32 Action        |
| ------------------------ | ------------------- | ------------------- |
| No MSG_STATE for 2s      | N/A                 | Display "Comm Lost" |
| No commands for 30s      | Continue standalone | N/A                 |
| No PING response for 10s | Log warning         | Attempt reconnect   |

---

## Implementation Notes

### Byte Order

All multi-byte values are **little-endian** (least significant byte first).

### Packed Structures

Use `__attribute__((packed))` (GCC) or `#pragma pack(1)` to prevent padding.

### Buffer Sizing

- TX buffer: 64 bytes minimum
- RX buffer: 64 bytes minimum
- Ring buffer recommended for RX

### Thread Safety

On Pico with dual-core:

- Core 0 writes state to shared buffer
- Core 1 reads and transmits
- Use mutex or lock-free queue

---

## Example: Status Packet

Hex dump of a typical MSG_STATUS packet:

```
AA 01 26 2F                     # Header: START, TYPE=0x01, LEN=38, SEQ=47

# Temperatures (6 bytes)
A4 03                           # temp_brew = 932 (93.2°C)
AE 05                           # temp_steam = 1454 (145.4°C)
8F 03                           # temp_group = 911 (91.1°C)

# Pressure (2 bytes)
84 03                           # pressure = 900 (9.00 bar)

# Setpoints (4 bytes)
A2 03                           # setpoint_brew = 930 (93.0°C)
AA 05                           # setpoint_steam = 1450 (145.0°C)

# Outputs & Sensors (4 bytes)
2D                              # ssr_brew_duty = 45%
14                              # ssr_steam_duty = 20%
04                              # relays = 0b00000100 (water LED on)
07                              # levels = 0b00000111 (all OK)

# Machine State (6 bytes)
03                              # state = STATE_READY
01                              # heating_strategy = HEAT_SEQUENTIAL
06                              # flags = 0b00000110 (brew+steam ready)
00                              # errors = 0 (no errors)
00 00                           # brew_time_ms = 0

# Power Data (12 bytes)
01 09                           # voltage = 2305 (230.5V)
6C 02                           # current = 620 (6.20A)
92 05                           # power_w = 1426W
D2 04 00 00                     # energy_wh = 1234 Wh
32                              # frequency = 50 Hz
62                              # power_factor = 98 (0.98)

# System (4 bytes)
40 42 0F 00                     # uptime_ms = 1000000

# CRC (2 bytes)
XX XX                           # CRC16
```

**Total: 44 bytes** (4 header + 38 payload + 2 CRC)
**TX time @ 921600 baud: 0.48ms**

---

## Firmware Streaming Protocol (Bootloader Mode)

When Pico is in bootloader mode (after receiving `MSG_CMD_BOOTLOADER`), ESP32 streams firmware using a simple chunked protocol over UART.

### Protocol Format

Each firmware chunk follows this format:

```
┌──────────┬──────────────┬──────────┬──────────┬──────────┐
│  MAGIC   │  CHUNK_NUM   │   SIZE   │   DATA   │ CHECKSUM │
│ 2 bytes  │   4 bytes    │ 2 bytes  │ variable │  1 byte  │
│ 0x55 0xAA│  little-end  │ little-  │  0-256   │   XOR    │
│          │              │  endian  │  bytes   │          │
└──────────┴──────────────┴──────────┴──────────┴──────────┘
```

| Field     | Size     | Description                                     |
| --------- | -------- | ----------------------------------------------- |
| MAGIC     | 2        | Magic bytes `0x55 0xAA` to identify chunk start |
| CHUNK_NUM | 4        | Chunk number (little-endian, starting from 0)   |
| SIZE      | 2        | Data size in bytes (little-endian, 0-256)       |
| DATA      | variable | Firmware data (0-256 bytes)                     |
| CHECKSUM  | 1        | XOR checksum of all data bytes                  |

### End Marker

After all chunks are sent, ESP32 sends an end marker:

- Chunk with chunk number `0xFFFFFFFF` and 2-byte data `0xAA 0x55` (reversed magic)

### Streaming Process

1. **Initialization:**

   - ESP32 sends `MSG_CMD_BOOTLOADER` via normal protocol
   - **Serial bootloader (implemented):** Pico receives command, sends acknowledgment, and enters bootloader mode
   - **Hardware bootloader entry (fallback):** ESP32 can also trigger hardware bootloader entry (BOOTSEL + RUN pins)
   - Pico waits for firmware chunks over UART

2. **Firmware Streaming:**

   - ESP32 reads firmware file from LittleFS
   - Splits into chunks of up to 256 bytes
   - Sends each chunk with header, data, and checksum
   - Reports progress every 10% via WebSocket

3. **Completion:**
   - ESP32 sends end marker
   - ESP32 waits 1 second
   - ESP32 resets Pico (pulse RUN pin)
   - Pico boots with new firmware

### Error Handling

- **Chunk transmission failure:** ESP32 logs error and stops streaming
- **Checksum mismatch:** Pico bootloader rejects chunk, sends error code (0xFF + BOOTLOADER_ERROR_CHECKSUM), returns error
- **Timeout:** Pico bootloader sends error code (0xFF + BOOTLOADER_ERROR_TIMEOUT) on timeout
- **Invalid chunk:** Pico bootloader validates chunk size, chunk number, and flash bounds before processing
- **Flash errors:** Pico bootloader validates flash offset alignment and bounds, sends error codes for erase/write failures
- **Error reporting:** All bootloader errors are reported to ESP32 via error codes (0xFF + error_code) before returning
- **Timeout:** If no progress for 30 seconds, abort update
- **File corruption:** ESP32 verifies file size before streaming

### Implementation Status

**ESP32 Side:**

- File upload with progress tracking
- Bootloader entry sequence
- Firmware streaming protocol
- Progress reporting via WebSocket

**Pico Side:**

- Serial bootloader (receives firmware entirely over UART)
- Handles `MSG_CMD_BOOTLOADER` command
- Accepts firmware chunks in streaming format
- Writes firmware to flash (page-aligned, sector-erased)
- Verifies checksums (XOR per chunk)
- Handles errors gracefully (timeouts, checksum failures, flash errors)
- **Advantage:** No hardware pin control needed, works entirely over UART
- **Location:** `src/pico/src/bootloader.c` and `src/pico/include/bootloader.h`

### Notes

- **Serial bootloader (implemented):** Pico enters bootloader mode via `MSG_CMD_BOOTLOADER` command, receives firmware over UART, no hardware intervention needed
- **Hardware bootloader entry (fallback):** ESP32 implementation also supports hardware bootloader entry (BOOTSEL + RUN pins) as a fallback method
- Streaming uses same UART as normal communication (921600 baud)
- Chunk size is limited to 256 bytes to prevent UART buffer overflow
- Progress updates sent every 10% to minimize WebSocket traffic
- Bootloader writes firmware to reserved flash region (last 512KB)
- Flash writes are page-aligned (256 bytes) and sectors are erased as needed (4KB)
- Bootloader resets device after successful firmware reception

---

## Backward Compatibility & Update Strategy

### Protocol Versioning

The protocol uses a version number (`PROTOCOL_VERSION`) defined in `protocol_defs.h` to track protocol compatibility. Currently set to version 1.

**Version Components:**

- **Protocol Version** (`PROTOCOL_VERSION`): Defines the binary protocol format
- **Firmware Version** (in `MSG_BOOT`): Tracks individual device firmware versions

### Breaking Changes Policy

**Breaking changes** are defined as:

- Changes to packet structure (field order, size, or types)
- Removal or modification of existing message types
- Changes to message type values
- Changes to CRC calculation
- Changes to packet header format

**Non-breaking changes** include:

- Adding new message types (using unused type IDs)
- Adding optional fields to existing messages (with version checks)
- Extending reserved fields

### Update Strategy: Coordinated Update

For breaking protocol changes, both devices **must be updated together** using a coordinated update sequence. This ensures no compatibility issues during the transition.

#### Update Sequence

```
┌─────────────────────────────────────────────────────────────────┐
│         COORDINATED UPDATE SEQUENCE (Breaking Changes)          │
└─────────────────────────────────────────────────────────────────┘

1. PREPARATION
   ───────────
   • ESP32 downloads new Pico firmware (.uf2) via WiFi
   • ESP32 downloads new ESP32 firmware via WiFi
   • Both firmware files stored in ESP32 flash

2. PICO UPDATE
   ───────────
   • ESP32 sends MSG_CMD_BOOTLOADER (0x1F) to Pico
   • Pico enters bootloader mode
   • ESP32 streams new Pico firmware via UART
   • Pico writes firmware to flash
   • ESP32 resets Pico (pulse RUN pin)
   • Pico boots with new firmware

3. ESP32 UPDATE
   ───────────
   • ESP32 initiates self-update (OTA)
   • ESP32 reboots with new firmware
   • ESP32 reconnects to WiFi

4. VERIFICATION
   ────────────
   • Pico sends MSG_BOOT with new version
   • ESP32 verifies protocol compatibility
   • Both devices communicate with new protocol
   • System fully operational
```

#### Implementation Notes

**During Update:**

- Pico may be temporarily unavailable (during bootloader mode)
- ESP32 web UI should show "Update in progress" status
- Machine should be in safe state (heaters off) before starting update

**Error Recovery:**

- If Pico update fails: ESP32 can retry or enter recovery mode
- If ESP32 update fails: Device will reboot to previous firmware (if OTA rollback enabled)
- If both fail: Manual recovery via USB (Pico) or serial (ESP32)

**Version Checking:**

- ESP32 should parse `MSG_BOOT` to read Pico firmware version
- ESP32 should check `PROTOCOL_VERSION` compatibility
- Mismatched versions should trigger update sequence or show warning

### Non-Breaking Changes

For non-breaking changes (new features, bug fixes):

- Devices can be updated independently
- Older firmware continues to work with newer firmware
- New message types are ignored by older firmware (graceful degradation)

### Update Recommendations

1. **Always update both devices** when protocol version changes
2. **Test updates** in development before deploying
3. **Maintain changelog** documenting protocol version changes
4. **Increment protocol version** (`PROTOCOL_VERSION`) for breaking changes
5. **Document migration path** for major protocol updates

### Version Management

For details on versioning, protocol version tracking, and release management, see [Versioning.md](Versioning.md).

---
