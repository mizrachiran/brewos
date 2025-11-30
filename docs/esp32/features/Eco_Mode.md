# Eco Mode Feature

## Overview

Eco Mode is a power-saving feature that automatically reduces the brew boiler temperature when the machine is idle for an extended period. This significantly reduces power consumption while keeping the machine in a semi-ready state for quick wake-up.

## How It Works

### Automatic Eco Mode

1. **Idle Timer**: When the machine is in the READY state (at temperature, not brewing), an idle timer starts counting
2. **Timeout**: After the configured timeout (default: 30 minutes), the machine automatically enters Eco Mode
3. **Reduced Temperature**: The brew boiler setpoint is lowered to the eco temperature (default: 80°C)
4. **Wake on Activity**: Any user activity (mode change, brew, button press) immediately exits Eco Mode

### State Flow

```
READY → [idle timeout] → ECO → [user activity] → HEATING → READY
```

### Temperature Management

| Mode | Brew Temp | Steam Temp |
|------|-----------|------------|
| Normal (READY) | ~93°C | ~145°C |
| Eco Mode | ~80°C | Unchanged |

The lower eco temperature (~80°C) significantly reduces heat loss and power consumption while remaining warm enough for quick recovery to brewing temperature.

## Configuration

### Parameters

| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| `enabled` | `true` | bool | Enable/disable auto-eco timeout |
| `eco_brew_temp` | 80.0°C | 50-90°C | Target brew temperature in eco mode |
| `timeout_minutes` | 30 | 0-480 | Minutes of idle before entering eco (0=disabled) |

### Web UI

Configure eco mode from **Settings → Eco Mode**:

- **Eco Brew Temp**: Temperature to maintain during eco mode
- **Auto-Eco After**: Idle time before entering eco mode

### API Commands

#### Set Eco Configuration

Via WebSocket or MQTT:
```json
{
  "type": "command",
  "cmd": "set_eco",
  "enabled": true,
  "brewTemp": 80.0,
  "timeout": 30
}
```

#### Manually Enter Eco Mode
```json
{
  "type": "command",
  "cmd": "enter_eco"
}
```

#### Exit Eco Mode (Wake Up)
```json
{
  "type": "command",
  "cmd": "exit_eco"
}
```

#### Via Machine Mode
```json
{
  "type": "command",
  "cmd": "set_mode",
  "mode": "eco"
}
```

## Protocol

### Message Format

**MSG_CMD_SET_ECO (0x1E)**

| Byte | Field | Description |
|------|-------|-------------|
| 0 | enabled | 0=disabled, 1=enabled |
| 1-2 | eco_brew_temp | Temperature × 10 (big-endian, °C) |
| 3-4 | timeout_minutes | Timeout in minutes (big-endian) |

**Single-byte command:**
| Value | Action |
|-------|--------|
| 0 | Exit eco mode |
| 1 | Enter eco mode |

### Status Reporting

The machine state in the status payload reflects eco mode:
- `state = 7` (STATE_ECO) when in eco mode

## Implementation Details

### Pico Firmware

The Pico firmware manages eco mode state:

1. **State Machine**: New STATE_ECO state added
2. **Idle Timer**: Tracks time since last user activity
3. **Setpoint Management**: Saves/restores normal setpoint on eco entry/exit
4. **Persistence**: Eco configuration saved to flash

### ESP32 Firmware

The ESP32 handles:
- WebSocket command forwarding to Pico
- MQTT command integration
- Status broadcast including eco state

### Web Application

The web app provides:
- Eco mode settings UI in Settings page
- Commands: `set_eco`, `enter_eco`, `exit_eco`

**Files:**
- `src/web/src/pages/Settings.tsx` - Eco mode configuration form

## Power Savings

Estimated power reduction in eco mode:

| Scenario | Normal | Eco Mode | Savings |
|----------|--------|----------|---------|
| Idle power (brew only) | ~150W avg | ~50W avg | ~67% |
| Daily cost (8hr idle) | ~$0.30 | ~$0.10 | ~$0.20/day |

*Values are approximate and depend on ambient temperature, machine insulation, and local electricity rates.*

## Best Practices

1. **Set appropriate timeout**: 30 minutes is a good balance between responsiveness and savings
2. **Consider usage patterns**: If you brew every hour, increase timeout to 60+ minutes
3. **Morning routine**: Wake the machine 5-10 minutes before brewing for optimal temperature
4. **Enable auto-eco**: Don't rely on manual eco mode - let the timer handle it

## Troubleshooting

### Eco mode not activating

- Check if eco mode is enabled in settings
- Verify timeout is not set to 0 (disabled)
- Machine must be in READY state (not IDLE or HEATING)
- Ensure no ongoing brewing activity

### Slow recovery from eco mode

- Recovery time depends on:
  - Temperature difference (normal - eco)
  - Heater power
  - Ambient temperature
- Typical recovery: 2-5 minutes

### Settings not persisting

- Eco config is saved to flash when changed
- Flash write requires ~100ms - don't power off immediately after changing settings

## Related Documentation

- [State Management](../State_Management.md)
- [Communication Protocol](../../shared/Communication_Protocol.md)
- [WebSocket Protocol](../../web/WebSocket_Protocol.md)

