# Power Metering Guide

## Overview

BrewOS supports monitoring your espresso machine's power consumption through two methods:

1. **Hardware Modules** - Connected directly to the Pico control board via J17 connector
2. **MQTT Smart Plugs** - Smart plugs like Shelly that publish power data over MQTT

You configure one source via the webapp at Settings → Machine → Power Metering.

---

## Supported Hardware Modules

These modules connect to your Pico control board via the **J17 connector** (6-pin JST-XH).

### PZEM-004T V3

**Most popular choice** - Widely available, affordable, accurate

| Specification | Value |
|--------------|-------|
| Interface | TTL UART (3.3V/5V compatible) |
| Baud Rate | 9600 |
| Protocol | Modbus RTU |
| Current Range | 0-100A (with included CT clamp) |
| Voltage Range | 80-260V AC |
| Price | ~$10-15 USD |
| Where to Buy | AliExpress, Amazon, local electronics stores |

**Wiring:**
- J17 Pin 2 (5V) → PZEM VCC (red)
- J17 Pin 3 (GND) → PZEM GND (black)
- J17 Pin 4 (RX) → PZEM TX (yellow)
- J17 Pin 5 (TX) → PZEM RX (white)
- J17 Pin 6 (DE/RE) → Leave unconnected

**High-Voltage Connections:**
- Connect mains L and N to PZEM screw terminals
- Clip CT clamp around the Live wire **only**
- Arrow on CT clamp should point toward the load

### JSY-MK-163T

Bi-directional metering (supports solar export measurement)

| Specification | Value |
|--------------|-------|
| Interface | TTL UART |
| Baud Rate | 4800 |
| Protocol | Modbus RTU |
| Current Range | 0-100A |
| Voltage Range | 80-280V AC |
| Bi-directional | Yes (import + export) |

**Same wiring as PZEM-004T** (except baud rate is auto-detected)

### JSY-MK-194T

Dual-channel metering (can monitor two circuits)

| Specification | Value |
|--------------|-------|
| Interface | TTL UART |
| Baud Rate | 4800 |
| Current Range | 0-200A (dual channel) |
| Features | Two independent CT inputs |

**Use for:** Monitoring both machine and grinder separately

### Eastron SDM120 / SDM230

Professional DIN-rail meters with high accuracy

| Specification | Value |
|--------------|-------|
| Interface | RS485 |
| Baud Rate | 2400 (SDM120) or 9600 (SDM230) |
| Protocol | Modbus RTU |
| Mounting | DIN rail |
| Accuracy | Class 1 (±1%) |
| Features | LCD display, pulse output |

**Requires RS485 connection:**
- J17 Pin 4 (RX) → RS485 A
- J17 Pin 5 (TX) → RS485 B  
- J17 Pin 6 (DE/RE) → MAX3485 direction control

---

## Supported MQTT Smart Plugs

These plugs are **already monitoring your machine's power**. BrewOS subscribes to their MQTT topics to get the data.

### Shelly Plug / Plug S

**Best for home automation enthusiasts**

**Configuration:**
1. Set up Shelly Plug with MQTT enabled
2. Note the status topic: `shellies/shelly-plug-XXXXXX/status`
3. In BrewOS: Settings → Machine → Power Metering
4. Select "MQTT Topic" → Enter topic → Format: "Shelly Plug"

**MQTT Topic Example:**
```
shellies/shelly-plug-ABC123/status
```

**Data Published:**
```json
{
  "meters": [{
    "power": 1234.5,
    "total": 12345
  }]
}
```

### Tasmota-Flashed Smart Plugs

Any smart plug running Tasmota firmware

**Configuration:**
1. Flash plug with Tasmota firmware
2. Enable MQTT in Tasmota console: `SetOption3 1`
3. Note the SENSOR topic: `tele/tasmota_XXXXXX/SENSOR`
4. In BrewOS: Enter topic → Format: "Tasmota"

**MQTT Topic Example:**
```
tele/tasmota_ABC123/SENSOR
```

**Data Published:**
```json
{
  "Time": "2024-01-01T00:00:00",
  "ENERGY": {
    "Power": 1234,
    "Voltage": 230,
    "Current": 5.36,
    "Total": 45.678
  }
}
```

### Generic MQTT

For custom setups or other smart plug brands

**Requirements:**
- Must publish JSON payload
- Must include at least `power` field
- Optional: `voltage`, `current`, `energy`, `frequency`, `power_factor`

**Example topic:**
```
home/espresso_machine/power
```

**Example payload:**
```json
{
  "power": 1200,
  "voltage": 230,
  "current": 5.2
}
```

---

## Configuration Guide

### Hardware Module Setup

1. **Physical Connection**
   - Connect module to J17 connector on Pico board
   - Wire mains L/N to module's HV terminals
   - Clip CT clamp around Live wire

2. **Webapp Configuration**
   - Navigate to Settings → Machine → Power Metering
   - Click "Configure Power Metering"
   - Select "Hardware Module (UART/RS485)"
   - Select "Auto-detect" for meter type
   - Click "Auto-Detect Meter"
   - Wait 10-15 seconds for detection
   - Click "Save Configuration"

3. **Verification**
   - Status should show "Connected"
   - Current readings (voltage, current, power) should display
   - Check MQTT topic `brewos/{device-id}/power` if MQTT enabled

### MQTT Smart Plug Setup

1. **Prerequisites**
   - MQTT broker running (e.g., Mosquitto)
   - Smart plug configured to publish to broker
   - BrewOS MQTT configured (Settings → Network → MQTT)

2. **Webapp Configuration**
   - Navigate to Settings → Machine → Power Metering
   - Click "Configure Power Metering"
   - Select "MQTT Topic (Smart Plug)"
   - Enter the plug's MQTT topic
   - Select format (Shelly, Tasmota, or auto-detect)
   - Click "Save Configuration"

3. **Verification**
   - Status should show "Connected" within 10 seconds
   - Readings should match those from the smart plug

---

## Auto-Detection

For hardware modules, BrewOS can automatically identify the connected meter:

1. Click "Auto-Detect Meter" button
2. System tries each known meter type:
   - PZEM-004T @ 9600 baud
   - JSY-MK-163T @ 4800 baud
   - JSY-MK-194T @ 4800 baud
   - Eastron SDM120 @ 2400 baud
   - Eastron SDM230 @ 9600 baud

3. First valid response is accepted
4. Configuration is saved automatically

**Detection Time:** 10-15 seconds (tries all 5 meter types)

---

## Troubleshooting

### Hardware Module Not Detected

**Check wiring:**
- Verify J17 connector is firmly seated
- Check TX/RX are not swapped
- Verify 5V power is connected
- For RS485: Check A/B polarity

**Check module:**
- Power LED on module should be lit
- Try manually selecting meter type (bypass auto-detect)
- Verify meter's Modbus address matches expected (usually 0x01 or 0xF8)

**Check GPIO configuration:**
- GPIO6/7 must not be used elsewhere
- GPIO20 (RS485 DE/RE) must be available for RS485 meters

### MQTT Source Not Connecting

**Check MQTT:**
- Verify MQTT is enabled (Settings → Network → MQTT)
- Verify broker is reachable
- Check topic spelling (case-sensitive)
- Use MQTT Explorer to verify plug is publishing

**Check topic:**
- Shelly: `shellies/{device-id}/status`
- Tasmota: `tele/{device-id}/SENSOR`
- Verify JSON contains expected fields

### Readings Look Wrong

**Voltage:**
- Should match your mains supply (110V, 220V, 230V, 240V)
- If off by 10x, check scaling factor
- PZEM shows 0.1V resolution (e.g., 2300 = 230.0V)

**Current:**
- Should be 3-6A when heating
- Should be 0.5-2A when idle
- If too high, CT clamp may be around multiple wires

**Energy:**
- Should increment gradually
- PZEM: Wh (watts × hours)
- JSY: kWh (kilowatt-hours)
- Check if meter was reset recently

### Connection Drops

**Hardware:**
- Check connector is fully seated
- Verify no loose wires
- Try lower baud rate (less susceptible to noise)
- Keep J17 cable short (<50cm recommended)

**MQTT:**
- Check WiFi signal strength
- Verify broker is stable
- Check smart plug hasn't gone to sleep
- 10-second timeout triggers disconnection

---

## Data Published to MQTT

When MQTT is enabled, power meter data is published to:

**Topic:** `brewos/{device-id}/power`

**Payload:**
```json
{
  "voltage": 230.5,
  "current": 5.2,
  "power": 1198,
  "energy_import": 45.3,
  "energy_export": 0.0,
  "frequency": 50.0,
  "power_factor": 0.98
}
```

**Publish Interval:** 5 seconds  
**Retained:** Yes  
**QoS:** 0

---

## Home Assistant Integration

When Home Assistant auto-discovery is enabled, the following sensors are created:

| Sensor | Entity ID | Unit | Device Class |
|--------|-----------|------|--------------|
| Voltage | `sensor.brewos_voltage` | V | voltage |
| Current | `sensor.brewos_current` | A | current |
| Power | `sensor.brewos_power` | W | power |
| Energy Import | `sensor.brewos_energy_import` | kWh | energy |
| Energy Export | `sensor.brewos_energy_export` | kWh | energy |
| Frequency | `sensor.brewos_frequency` | Hz | frequency |
| Power Factor | `sensor.brewos_power_factor` | - | power_factor |

**Example Home Assistant Automation:**
```yaml
automation:
  - alias: "Espresso Machine Energy Alert"
    trigger:
      - platform: numeric_state
        entity_id: sensor.brewos_power
        above: 2000
    action:
      - service: notify.mobile_app
        data:
          message: "Espresso machine drawing {{ states('sensor.brewos_power') }}W!"
```

---

## Technical Details

### Communication Architecture

```
Hardware Modules (PZEM, JSY, Eastron)
    ↓ UART1/RS485 (GPIO6/7/20)
Raspberry Pi Pico 2
    ↓ UART0 (GPIO0/1 via MSG_POWER_METER)
ESP32-S3
    ↓ WebSocket / MQTT
Webapp / Home Assistant

MQTT Smart Plugs (Shelly, Tasmota)
    ↓ WiFi → MQTT Broker
ESP32-S3 (subscribes to topic)
    ↓ WebSocket
Webapp
```

### Why This Design?

- **Pico** handles hardware meters because it has direct UART1 access
- **ESP32** handles MQTT sources because it has WiFi
- **Single source** to avoid conflicting data from multiple meters
- **Unified data model** normalizes all sources to common format

### Data Flow

1. **Hardware source:**
   - Pico polls meter every 1 second via Modbus
   - Pico sends MSG_POWER_METER to ESP32 with reading
   - ESP32 broadcasts to webapp via WebSocket
   - ESP32 publishes to MQTT every 5 seconds

2. **MQTT source:**
   - ESP32 subscribes to plug's MQTT topic
   - ESP32 parses incoming JSON
   - ESP32 broadcasts to webapp via WebSocket
   - Data is already on MQTT (no republishing needed)

---

## Firmware Implementation Notes

### Pico Firmware

**Files:**
- `src/pico/include/power_meter.h` - Public API
- `src/pico/src/power_meter.c` - Modbus driver implementation

**Key Functions:**
- `power_meter_init()` - Initialize with config
- `power_meter_update()` - Poll meter (call every 1s)
- `power_meter_get_reading()` - Get cached reading
- `power_meter_auto_detect()` - Try all known meters

**Protocol Commands:**
- `MSG_CMD_POWER_METER_CONFIG` (0x21) - Enable/disable metering
- `MSG_CMD_POWER_METER_DISCOVER` (0x22) - Start auto-discovery

**Protocol Responses:**
- `MSG_POWER_METER` (0x0B) - Power meter reading (sent every 1s when enabled)

### ESP32 Firmware

**Files:**
- `src/esp32/include/power_meter/power_meter.h` - Base interface
- `src/esp32/include/power_meter/mqtt_power_meter.h` - MQTT driver
- `src/esp32/include/power_meter/power_meter_manager.h` - Manager
- `src/esp32/src/power_meter/mqtt_power_meter.cpp` - MQTT implementation
- `src/esp32/src/power_meter/power_meter_manager.cpp` - Manager implementation

**Key Classes:**
- `PowerMeterManager` - Manages active source and readings
- `MQTTPowerMeter` - Subscribes to MQTT topics, parses data
- `IPowerMeter` - Abstract base class

---

## Adding a New Hardware Module

If you have a Modbus-based power meter not in the list:

1. **Find the datasheet** - You need:
   - Modbus slave address
   - Baud rate
   - Register map (which register has voltage, current, etc.)
   - Scaling factors

2. **Add to register map** in `src/pico/src/power_meter.c`:

```c
{
    .name = "Your Meter Name",
    .slave_addr = 0x01,
    .baud_rate = 9600,
    .is_rs485 = false,
    .voltage_reg = 0x0000,
    .voltage_scale = 0.1f,
    // ... other registers
    .function_code = MODBUS_FC_READ_INPUT_REGS,
    .num_registers = 10
}
```

3. **Rebuild and flash** Pico firmware

4. Auto-detection will now try your meter

---

## Adding a New MQTT Format

For smart plugs with custom JSON formats:

1. **Examine the MQTT payload** using MQTT Explorer

2. **Add parser** to `src/esp32/src/power_meter/mqtt_power_meter.cpp`:

```cpp
bool MQTTPowerMeter::parseYourFormat(JsonDocument& doc) {
    if (doc.containsKey("your_key")) {
        _lastReading.power = doc["your_key"]["power"].as<float>();
        _lastReading.voltage = doc["your_key"]["voltage"].as<float>();
        // ...
        return true;
    }
    return false;
}
```

3. **Add to tryAutoParse()** for automatic detection

4. **Rebuild and flash** ESP32 firmware

---

## Safety Notes

### High Voltage Safety

**⚠️ WARNING:** Power meters connect directly to mains voltage (100-240V AC)

- **Turn off power** before wiring
- Use appropriate wire gauge (14 AWG minimum for 15A circuits)
- Verify correct polarity (L = Live/Hot, N = Neutral)
- CT clamp must clip around **only one wire** (Live)
- Ensure all connections are tight
- Use proper enclosures (IP20 minimum)
- Consult a licensed electrician if unsure

### Current Measurement

- **CT clamp direction matters** - arrow points toward load
- Clamp around **Live wire only**, not both L+N
- 100A CT clamps are oversized for espresso machines but provide headroom
- Never open a CT clamp while powered (can generate dangerous voltages)

### Installation Location

- Mount power meter module away from boilers (heat)
- Keep away from moisture
- Ensure adequate ventilation for heat dissipation
- Secure all wiring to prevent strain on J17 connector

---

## Performance Impact

### Hardware Modules

- **Pico overhead:** ~1ms per poll (negligible)
- **Modbus latency:** 50-100ms per query
- **Update rate:** 1 reading/second (adequate for PID control)
- **Flash usage:** ~4KB for driver

### MQTT Sources

- **ESP32 overhead:** Minimal (callback-driven)
- **Network latency:** 100-500ms depending on WiFi/broker
- **Update rate:** Depends on smart plug (typically 1-5 seconds)
- **No impact on Pico** (ESP32 only)

---

## Comparison: Hardware vs MQTT

| Feature | Hardware Module | MQTT Smart Plug |
|---------|----------------|-----------------|
| **Accuracy** | ±1-2% | ±2-3% |
| **Latency** | 1 second | 1-5 seconds |
| **Cost** | $10-50 | $15-30 |
| **Installation** | Requires J17 wiring | Plug-and-play |
| **Isolation** | External module isolates | Already isolated |
| **Portability** | Permanent install | Easy to move |
| **Data** | Voltage, current, power, energy, PF, frequency | Varies by plug |
| **Reliability** | Very high | Depends on WiFi |

**Recommendation:**
- **DIY builders:** Hardware module (integrated, professional)
- **Home automation users:** MQTT smart plug (easy, already have one)
- **Maximum accuracy:** Eastron SDM series (DIN rail meters)

---

## FAQ

**Q: Can I use both hardware and MQTT simultaneously?**  
A: No. You must choose one source. The system uses mutually exclusive configuration to avoid conflicting readings.

**Q: Will this work with 4-20mA current loop meters?**  
A: No. The J17 interface is designed for voltage output (Modbus UART/RS485). Current loop meters require different hardware.

**Q: Can I monitor the grinder too?**  
A: Use JSY-MK-194T with dual CT clamps - one for machine, one for grinder. Firmware currently reports only one channel, but could be extended.

**Q: Does this replace the existing PZEM driver in pico/src/pzem.c?**  
A: Yes. The new `power_meter.c` uses data-driven register maps and supports multiple meter types, making the old `pzem.c` obsolete.

**Q: What if my meter uses a different Modbus address?**  
A: Most meters allow address configuration. Check the datasheet for DIP switches or serial commands to change the address to match (0x01 or 0xF8 are standard).

---

## See Also

- [Hardware Specification](../hardware/Specification.md#10-power-metering-circuit) - J17 connector pinout
- [MQTT Integration](../esp32/integrations/MQTT.md) - MQTT setup guide
- [Pico Power Metering](../pico/Power_Metering.md) - Firmware implementation details

