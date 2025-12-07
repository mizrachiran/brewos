# Power Metering - Pico Firmware Implementation

## Overview

The Pico firmware handles **hardware power meter modules** (PZEM, JSY, Eastron) connected via J17 (UART1 on GPIO6/7, RS485 direction on GPIO20).

The implementation uses a **data-driven architecture** with Modbus register maps, making it easy to add new meter types without code changes.

---

## Hardware Interface

| GPIO | Function | Notes |
|------|----------|-------|
| GPIO6 | UART1 TX | To meter RX |
| GPIO7 | UART1 RX | From meter TX |
| GPIO20 | RS485 DE/RE | Direction control (HIGH=TX, LOW=RX) |

**Connector:** J17 (JST-XH 6-pin) - See hardware specification Section 10.

---

## Supported Meters

| Module | Baud | Protocol | Interface | Slave Addr |
|--------|------|----------|-----------|------------|
| PZEM-004T V3 | 9600 | Modbus RTU | TTL UART | 0xF8 |
| JSY-MK-163T | 4800 | Modbus RTU | TTL UART | 0x01 |
| JSY-MK-194T | 4800 | Modbus RTU | TTL UART | 0x01 |
| Eastron SDM120 | 2400 | Modbus RTU | RS485 | 0x01 |
| Eastron SDM230 | 9600 | Modbus RTU | RS485 | 0x01 |

---

## Software Architecture

### Data-Driven Register Maps

Instead of writing separate code for each meter, we use configuration structs:

```c
typedef struct {
    const char* name;
    uint8_t slave_addr;
    uint32_t baud_rate;
    bool is_rs485;
    
    uint16_t voltage_reg;
    float voltage_scale;
    
    uint16_t current_reg;
    float current_scale;
    
    // ... other registers
    
    uint8_t function_code;
    uint8_t num_registers;
} modbus_register_map_t;
```

**Benefits:**
- Adding new meters = adding config entry
- No code duplication
- Easy to test (just verify map values)
- Maintainable and extensible

### Unified Data Model

All meters normalize to a common structure:

```c
typedef struct {
    float voltage;        // Volts (RMS)
    float current;        // Amps (RMS)
    float power;          // Watts (Active)
    float energy_import;  // kWh (from grid)
    float energy_export;  // kWh (to grid - for solar)
    float frequency;      // Hz
    float power_factor;   // 0.0 - 1.0
    uint32_t timestamp;   // When read (ms)
    bool valid;           // Reading successful
} power_meter_reading_t;
```

---

## Public API

### Initialization

```c
bool power_meter_init(const power_meter_config_t* config);
```

Initialize power meter driver. Pass `NULL` to load config from flash.

**Returns:** `true` if successful, `false` on error

### Update

```c
void power_meter_update(void);
```

Poll the meter and update internal state. **Call every 1 second** from main loop.

This function:
1. Sends Modbus read request
2. Waits for response (500ms timeout)
3. Verifies CRC
4. Parses data into `power_meter_reading_t`
5. Caches result internally

### Get Reading

```c
bool power_meter_get_reading(power_meter_reading_t* reading);
```

Retrieve last cached reading.

**Returns:** `true` if reading is valid and recent (<5s old)

### Auto-Detection

```c
bool power_meter_auto_detect(void);
```

Automatically identify connected meter type. Tries all known configurations:

1. PZEM-004T @ 9600 baud
2. JSY-MK-163T @ 4800 baud
3. JSY-MK-194T @ 4800 baud
4. Eastron SDM120 @ 2400 baud
5. Eastron SDM230 @ 9600 baud

**Detection Logic:**
- Send Modbus read request
- Wait 500ms for response
- Verify voltage reading is reasonable (50-300V)
- First valid response wins

**Returns:** `true` if meter detected

**Time:** ~2-3 seconds for full detection

### Status

```c
bool power_meter_is_connected(void);
const char* power_meter_get_name(void);
const char* power_meter_get_error(void);
```

Query connection status, meter name, and error messages.

---

## Integration with Main Firmware

### In main.c

```c
#include "power_meter.h"

void main(void) {
    // Initialize power meter
    power_meter_init(NULL);  // Load from flash
    
    while (1) {
        // Update every 1 second
        if (time_since_last_update >= 1000) {
            power_meter_update();
            
            // Get reading and send to ESP32
            power_meter_reading_t reading;
            if (power_meter_get_reading(&reading)) {
                protocol_send_power_meter(&reading);
            }
        }
    }
}
```

### Protocol Messages

**From ESP32 to Pico:**

`MSG_CMD_POWER_METER_CONFIG` (0x21):
```c
// Payload: [enabled:1]
// 0 = disable, 1 = enable
```

`MSG_CMD_POWER_METER_DISCOVER` (0x22):
```c
// Payload: empty
// Triggers auto-detection
```

**From Pico to ESP32:**

`MSG_POWER_METER` (0x0B):
```c
// Payload: power_meter_reading_t struct (36 bytes)
// Sent every 1 second when enabled
```

---

## Modbus Protocol Details

### Request Format

```
┌───────┬───────┬─────────┬─────────┬──────────┬──────────┬────────┐
│ Addr  │ Func  │ Reg Hi  │ Reg Lo  │ Count Hi │ Count Lo │  CRC   │
│  1B   │  1B   │   1B    │   1B    │    1B    │    1B    │  2B    │
└───────┴───────┴─────────┴─────────┴──────────┴──────────┴────────┘
```

**Example (PZEM read 10 registers):**
```
F8 04 00 00 00 0A [CRC]
│  │  │     │
│  │  │     └─ Count: 10 registers
│  │  └─────── Start: 0x0000
│  └────────── Function: 0x04 (read input regs)
└───────────── Slave: 0xF8
```

### Response Format

```
┌───────┬───────┬──────────┬─────────┬────────┐
│ Addr  │ Func  │ Byte Cnt │  Data   │  CRC   │
│  1B   │  1B   │    1B    │  N×2B   │  2B    │
└───────┴───────┴──────────┴─────────┴────────┘
```

**Example (PZEM voltage=230.5V, current=5.2A):**
```
F8 04 14 09 01 14 50 ... [CRC]
│  │  │  │     │
│  │  │  │     └─ Current: 0x1450 = 5200 → 5.2A
│  │  │  └─────── Voltage: 0x0901 = 2305 → 230.5V
│  │  └─────────── Byte count: 20 bytes (10 regs)
│  └────────────── Function: 0x04
└───────────────── Slave: 0xF8
```

### CRC-16 Calculation

Modbus uses CRC-16 with polynomial 0xA001 (reversed 0x8005):

```c
uint16_t modbus_crc16(const uint8_t* buffer, uint16_t length) {
    uint16_t crc = 0xFFFF;
    
    for (uint16_t i = 0; i < length; i++) {
        crc ^= buffer[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;  // Little-endian
}
```

---

## Register Map Examples

### PZEM-004T V3

| Register | Name | Raw Value | Scaling | Result |
|----------|------|-----------|---------|--------|
| 0x0000 | Voltage | 2305 | × 0.1 | 230.5 V |
| 0x0001 | Current | 5200 | × 0.001 | 5.2 A |
| 0x0002 | Power | 1196 | × 1.0 | 1196 W |
| 0x0003 | Energy | 12345 (32-bit) | × 1.0 / 1000 | 12.345 kWh |
| 0x0004 | Frequency | 500 | × 0.1 | 50.0 Hz |
| 0x0005 | Power Factor | 98 | × 0.01 | 0.98 |

### JSY-MK-163T

| Register | Name | Raw Value | Scaling | Result |
|----------|------|-----------|---------|--------|
| 0x0048 | Voltage | 2305000 | × 0.0001 | 230.5 V |
| 0x0049 | Current | 52000 | × 0.0001 | 5.2 A |
| 0x004A | Power | 11960000 | × 0.0001 | 1196 W |
| 0x004B | Energy | 12345000 (32-bit) | × 0.001 / 1000 | 12.345 kWh |
| 0x0057 | Frequency | 5000 | × 0.01 | 50.0 Hz |
| 0x0056 | Power Factor | 980 | × 0.001 | 0.98 |

**Note:** JSY has much higher resolution (4 decimal places vs PZEM's 1-3)

---

## Error Handling

### Timeout Detection

If no response within 500ms, the request fails and `last_error` is set.

**Connection timeout:** 5 seconds without successful read → `is_connected() = false`

### Invalid Readings

Readings are validated:
- Voltage must be 50-300V (reasonable mains range)
- CRC must match
- Response length must be sufficient
- Slave address must match expected

Invalid readings are rejected and not cached.

### Error Messages

Retrieved via `power_meter_get_error()`:
- `"No response from meter"` - Timeout
- `"Invalid response"` - CRC mismatch
- `"Parse error"` - Malformed packet
- `"Auto-detection failed"` - No meter found
- `"Invalid meter index"` - Config error

---

## Testing

### Unit Tests

**File:** `src/pico/test/test_power_meter.c`

**Test Coverage:**
- ✅ Modbus CRC-16 calculation (known test vectors)
- ✅ Data extraction (uint16, uint32, big-endian)
- ✅ Register map validation (all 5 meter types)
- ✅ Response parsing (PZEM, JSY formats)
- ✅ Response verification (CRC, slave addr, function code)
- ✅ Connection status and timeout detection
- ✅ Error handling (NULL pointers, invalid data)
- ✅ Realistic scenarios (brewing, idle power levels)
- ✅ Energy conversion (Wh → kWh)
- ✅ Voltage range validation (110V, 220V, 240V)

**Run Tests:**
```bash
cd src/pico/test
mkdir -p build && cd build
cmake ..
make
./brewos_tests
```

**Expected Output:**
```
Running Power Meter Tests...
test_power_meter.c:49 tests passed
══════════════════════════════════════════
✅ ALL TESTS PASSED ✅
══════════════════════════════════════════
```

### Hardware Testing

See `docs/hardware/Test_Procedures.md` for J17 connector testing.

---

## Performance Characteristics

| Metric | Value |
|--------|-------|
| Poll Interval | 1000ms (1 second) |
| Modbus Request Time | 8 bytes @ 9600 baud = ~8ms |
| Response Time | 25 bytes @ 9600 baud = ~26ms |
| Total Transaction | ~50-100ms |
| CPU Usage | <0.5% (1ms per second) |
| Flash Usage | ~4KB (code + maps) |
| RAM Usage | ~200 bytes |

---

## Adding a New Meter Type

### Step 1: Obtain Datasheet

You need to know:
- Modbus slave address (default)
- Baud rate
- Parity (usually None for meters)
- Function code (0x03 or 0x04)
- Register map (addresses for voltage, current, power, etc.)
- Scaling factors (e.g., 0.1 for PZEM voltage)

### Step 2: Add Register Map

Edit `src/pico/src/power_meter.c`:

```c
static const modbus_register_map_t METER_MAPS[] = {
    // ... existing meters ...
    
    // Your new meter
    {
        .name = "YourMeter Model123",
        .slave_addr = 0x01,
        .baud_rate = 9600,
        .is_rs485 = false,
        
        .voltage_reg = 0x0000,
        .voltage_scale = 0.1f,
        
        .current_reg = 0x0001,
        .current_scale = 0.01f,
        
        .power_reg = 0x0002,
        .power_scale = 1.0f,
        
        .energy_reg = 0x0003,
        .energy_scale = 1.0f,
        .energy_is_32bit = true,
        
        .frequency_reg = 0x0004,
        .frequency_scale = 0.1f,
        
        .pf_reg = 0x0005,
        .pf_scale = 0.001f,
        
        .function_code = MODBUS_FC_READ_INPUT_REGS,
        .num_registers = 10
    }
};
```

### Step 3: Rebuild and Test

```bash
cd src/pico
mkdir -p build && cd build
cmake ..
make
# Flash to Pico
```

Auto-detection will now try your meter automatically.

---

## Debugging

### Enable Debug Output

In `power_meter.c`, `printf()` statements show:
- Auto-detection progress
- Modbus request/response bytes
- Parsed values
- Error conditions

### Common Issues

**No response:**
- Check TX/RX wiring (swap if needed)
- Verify baud rate matches meter
- Check meter is powered
- Try manual meter selection instead of auto-detect

**Wrong values:**
- Check scaling factor in register map
- Verify register addresses match datasheet
- Check byte order (Modbus is big-endian)

**CRC errors:**
- Noise on UART lines (add ferrite bead)
- Baud rate mismatch
- Cable too long (keep J17 cable <50cm)

---

## Memory Map

```
┌─────────────────────────────────────┐
│ FLASH (4KB)                         │
├─────────────────────────────────────┤
│ • Register maps (5 × ~80 bytes)     │
│ • Modbus protocol code              │
│ • CRC calculation                   │
│ • Parser functions                  │
└─────────────────────────────────────┘

┌─────────────────────────────────────┐
│ RAM (200 bytes)                     │
├─────────────────────────────────────┤
│ • last_reading (40 bytes)           │
│ • current_config (16 bytes)         │
│ • last_error (64 bytes)             │
│ • Response buffer (128 bytes max)   │
│ • UART RX FIFO (hardware, 32 bytes) │
└─────────────────────────────────────┘
```

---

## Future Enhancements

### Potential Additions

1. **More Modbus meters:**
   - Schneider PowerLogic
   - ABB B-series
   - YHDC meters

2. **SPI meters:**
   - ATM90E32 (3-phase, high precision)
   - ADE7953 (2-channel)

3. **Multi-channel:**
   - Report both channels of JSY-MK-194T
   - Separate machine/grinder tracking

4. **Advanced features:**
   - Harmonic analysis (if meter supports)
   - Peak demand tracking
   - Time-of-use energy buckets

---

## Protocol Specification

### MSG_POWER_METER (0x0B)

**Direction:** Pico → ESP32  
**Frequency:** Every 1 second (when enabled)  
**Payload:** `power_meter_reading_t` struct (36 bytes)

```c
struct {
    float voltage;        // 4 bytes
    float current;        // 4 bytes
    float power;          // 4 bytes
    float energy_import;  // 4 bytes
    float energy_export;  // 4 bytes
    float frequency;      // 4 bytes
    float power_factor;   // 4 bytes
    uint32_t timestamp;   // 4 bytes
    bool valid;           // 1 byte + 3 padding = 4 bytes
}; // Total: 36 bytes
```

### MSG_CMD_POWER_METER_CONFIG (0x21)

**Direction:** ESP32 → Pico  
**Payload:** 1 byte

```
[enabled:1]
0 = Disable power metering
1 = Enable power metering
```

### MSG_CMD_POWER_METER_DISCOVER (0x22)

**Direction:** ESP32 → Pico  
**Payload:** Empty

Triggers auto-detection. Pico will:
1. Try all known meter types
2. Stop at first valid response
3. Save configuration
4. Start sending MSG_POWER_METER messages

---

## Code Organization

```
src/pico/
├── include/
│   └── power_meter.h          # Public API
├── src/
│   └── power_meter.c          # Implementation (register maps, Modbus)
└── test/
    └── test_power_meter.c     # Unit tests (49 tests)
```

**Lines of Code:**
- `power_meter.h`: ~80 lines
- `power_meter.c`: ~450 lines
- `test_power_meter.c`: ~370 lines
- **Total:** ~900 lines

---

## See Also

- [Power Metering User Guide](../web/Power_Metering.md) - Configuration and troubleshooting
- [Hardware Specification](../hardware/Specification.md#10-power-metering-circuit) - J17 pinout
- [Communication Protocol](../shared/Communication_Protocol.md) - Pico-ESP32 messages
