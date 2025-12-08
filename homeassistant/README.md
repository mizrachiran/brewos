# BrewOS Home Assistant Integration

This folder contains Home Assistant integration components for BrewOS espresso machines.

## Contents

```
homeassistant/
├── custom_components/brewos/   # Native HA integration
├── lovelace/brewos-card.js     # Custom dashboard card
├── examples/
│   ├── automations.yaml        # Example automations
│   └── dashboard.yaml          # Example dashboard config
└── hacs.json                   # HACS metadata
```

## Integration Methods

### MQTT Auto-Discovery

BrewOS publishes Home Assistant MQTT discovery messages. When enabled, entities are automatically created in Home Assistant.

**Requirements:**

- MQTT broker accessible to both HA and BrewOS
- HA MQTT integration configured

**Configuration (BrewOS Settings → MQTT):**

- Enable MQTT
- Set broker address and credentials
- Enable "Home Assistant Discovery"

### Custom Lovelace Card

A dashboard card component displaying machine status, temperatures, and controls.

**Installation:**

1. Copy `lovelace/brewos-card.js` to your HA `config/www/` directory
2. Add the resource in HA (Settings → Dashboards → Resources):
   - URL: `/local/brewos-card.js`
   - Type: JavaScript Module
3. Add the card to a dashboard

**Card Configuration:**

```yaml
type: custom:brewos-card
entity_prefix: brewos # MQTT entity prefix
name: "My Machine" # Display name
show_name: true # Show/hide name
show_stats: true # Show/hide statistics
show_actions: true # Show/hide action buttons
compact: false # Compact layout mode
```

### Native Integration

A full Home Assistant custom component providing sensors, controls, and services.

**Installation:**

1. Copy `custom_components/brewos/` to your HA `config/custom_components/` directory
2. Restart Home Assistant
3. Add integration: Settings → Devices & Services → Add Integration → BrewOS
4. Configure the MQTT topic prefix

**Configuration Options:**
| Option | Description | Default |
|--------|-------------|---------|
| `topic_prefix` | MQTT topic prefix | `brewos` |
| `device_id` | Device identifier (for multi-device setups) | ``|
|`name`| Device display name |`BrewOS Espresso Machine` |

## Entity Reference

### Sensors

| Entity ID                      | Description              | Unit  |
| ------------------------------ | ------------------------ | ----- |
| `sensor.brewos_brew_temp`      | Brew boiler temperature  | °C    |
| `sensor.brewos_steam_temp`     | Steam boiler temperature | °C    |
| `sensor.brewos_brew_setpoint`  | Brew target temperature  | °C    |
| `sensor.brewos_steam_setpoint` | Steam target temperature | °C    |
| `sensor.brewos_pressure`       | Brew pressure            | bar   |
| `sensor.brewos_scale_weight`   | Scale weight             | g     |
| `sensor.brewos_flow_rate`      | Flow rate                | g/s   |
| `sensor.brewos_shot_duration`  | Active shot duration     | s     |
| `sensor.brewos_shot_weight`    | Active shot weight       | g     |
| `sensor.brewos_target_weight`  | BBW target weight        | g     |
| `sensor.brewos_shots_today`    | Daily shot count         | shots |
| `sensor.brewos_total_shots`    | Lifetime shot count      | shots |
| `sensor.brewos_kwh_today`      | Daily energy consumption | kWh   |
| `sensor.brewos_power`          | Current power draw       | W     |
| `sensor.brewos_voltage`        | Line voltage             | V     |
| `sensor.brewos_current`        | Current draw             | A     |
| `sensor.brewos_state`          | Machine state            | text  |

### Binary Sensors

| Entity ID                              | Description               | Device Class |
| -------------------------------------- | ------------------------- | ------------ |
| `binary_sensor.brewos_is_brewing`      | Brewing in progress       | running      |
| `binary_sensor.brewos_is_heating`      | Boilers heating           | heat         |
| `binary_sensor.brewos_ready`           | At target temperature     | -            |
| `binary_sensor.brewos_water_low`       | Water tank low            | problem      |
| `binary_sensor.brewos_alarm_active`    | Alarm condition           | problem      |
| `binary_sensor.brewos_pico_connected`  | Pico controller connected | connectivity |
| `binary_sensor.brewos_scale_connected` | Bluetooth scale connected | connectivity |

### Controls

| Entity ID                         | Type   | Description                   |
| --------------------------------- | ------ | ----------------------------- |
| `switch.brewos_power`             | Switch | Machine power on/off          |
| `button.brewos_start_brew`        | Button | Start brewing                 |
| `button.brewos_stop_brew`         | Button | Stop brewing                  |
| `button.brewos_tare_scale`        | Button | Tare scale                    |
| `button.brewos_enter_eco`         | Button | Enter eco mode                |
| `button.brewos_exit_eco`          | Button | Exit eco mode                 |
| `number.brewos_brew_temp_target`  | Number | Brew temperature (85-100°C)   |
| `number.brewos_steam_temp_target` | Number | Steam temperature (120-160°C) |
| `number.brewos_bbw_target`        | Number | Target weight (18-100g)       |
| `select.brewos_mode_select`       | Select | Mode: standby, on, eco        |
| `select.brewos_heating_strategy`  | Select | Heating strategy              |

## MQTT Topics

BrewOS uses the following MQTT topic structure:

```
{prefix}/{device_id}/status       # Machine status (JSON)
{prefix}/{device_id}/power        # Power meter readings (JSON)
{prefix}/{device_id}/statistics   # Shot/energy statistics (JSON)
{prefix}/{device_id}/command      # Command input
{prefix}/{device_id}/availability # Online/offline status
```

**Command Format:**

```json
{ "cmd": "command_name", "param": "value" }
```

**Available Commands:**
| Command | Parameters | Description |
|---------|------------|-------------|
| `set_mode` | `mode`: standby/on/eco, `strategy`: 0-3 (optional) | Set machine mode |
| `set_temp` | `boiler`: brew/steam, `temp`: float | Set temperature |
| `set_target_weight` | `weight`: float | Set BBW target |
| `set_heating_strategy` | `strategy`: 0-3 | Set heating strategy |
| `brew_start` | - | Start brewing |
| `brew_stop` | - | Stop brewing |
| `tare` | - | Tare scale |
| `enter_eco` | - | Enter eco mode |
| `exit_eco` | - | Exit eco mode |

**Heating Strategies:**
| Value | Name | Description |
|-------|------|-------------|
| 0 | `brew_only` | Heat brew boiler only |
| 1 | `sequential` | Heat brew first, then steam |
| 2 | `parallel` | Heat both boilers simultaneously |
| 3 | `smart_stagger` | Intelligent staggered heating |

## Examples

See `examples/` directory for:

- `automations.yaml` - Morning routines, auto power-off, notifications
- `dashboard.yaml` - Complete dashboard configuration

## Troubleshooting

**Entities not appearing:**

1. Verify MQTT broker connection in BrewOS settings
2. Check "Home Assistant Discovery" is enabled
3. Confirm HA MQTT integration is connected to the same broker
4. Use MQTT Explorer to verify topics are being published

**Stale or missing data:**

1. Check BrewOS device is powered and connected to WiFi
2. Verify MQTT broker is accessible
3. Check HA logs for connection errors

**Entity naming:**
Entity IDs follow the pattern `{domain}.brewos_{entity_key}`. The prefix can be customized in BrewOS MQTT settings.
