# Schedules & Auto Power-Off Feature

## Overview

The Schedules feature allows users to automate their espresso machine by:
1. **Time-based Schedules**: Turn the machine on or off at specific times on selected days
2. **Auto Power-Off**: Automatically turn off the machine after a period of inactivity

This feature is managed entirely by the ESP32, with configuration persisted to LittleFS.

## Features

### Multiple Schedules
- Up to 10 schedules
- Each schedule can turn the machine ON or OFF
- Configurable days of the week (weekdays, weekends, specific days)
- Configurable time (hour:minute)
- Optional heating strategy selection for ON schedules
- Optional friendly name for each schedule

### Auto Power-Off
- Automatically turn off machine after idle period
- Configurable timeout (5-480 minutes)
- Activity detection: brewing, encoder interaction, commands
- Won't trigger during active brewing

## Data Structures

### ScheduleEntry

```cpp
struct ScheduleEntry {
    uint8_t id;                    // Unique ID (1-10, 0 = unused)
    bool enabled;                  // Schedule is active
    uint8_t days;                  // Day of week bitmask
    uint8_t hour;                  // Hour (0-23)
    uint8_t minute;                // Minute (0-59)
    ScheduleAction action;         // ACTION_TURN_ON or ACTION_TURN_OFF
    HeatingStrategy strategy;      // Heating strategy for ON actions
    char name[24];                 // User-friendly name
};
```

### Day of Week Bitmask

| Bit | Day |
|-----|-----|
| 0x01 | Sunday |
| 0x02 | Monday |
| 0x04 | Tuesday |
| 0x08 | Wednesday |
| 0x10 | Thursday |
| 0x20 | Friday |
| 0x40 | Saturday |

**Presets:**
- `WEEKDAYS` = 0x3E (Mon-Fri)
- `WEEKENDS` = 0x41 (Sat-Sun)
- `EVERY_DAY` = 0x7F (All days)

### Heating Strategies

| Value | Strategy | Description |
|-------|----------|-------------|
| 0 | BREW_ONLY | Heat only brew boiler |
| 1 | SEQUENTIAL | Brew first, then steam (default) |
| 2 | PARALLEL | Heat both simultaneously |
| 3 | STEAM_PRIORITY | Steam first, then brew |
| 4 | SMART_STAGGER | Power-aware intelligent heating |

## API Endpoints

### REST API

#### Get All Schedules
```
GET /api/schedules
```

Response:
```json
{
  "schedules": [
    {
      "id": 1,
      "enabled": true,
      "days": 62,
      "hour": 7,
      "minute": 0,
      "action": "on",
      "strategy": 1,
      "name": "Morning Brew"
    }
  ],
  "autoPowerOffEnabled": true,
  "autoPowerOffMinutes": 60
}
```

#### Add Schedule
```
POST /api/schedules
Content-Type: application/json

{
  "enabled": true,
  "days": 62,
  "hour": 7,
  "minute": 0,
  "action": "on",
  "strategy": 1,
  "name": "Morning Brew"
}
```

Response:
```json
{
  "status": "ok",
  "id": 1
}
```

#### Update Schedule
```
POST /api/schedules/update
Content-Type: application/json

{
  "id": 1,
  "enabled": true,
  "days": 127,
  "hour": 6,
  "minute": 30,
  "action": "on",
  "strategy": 2,
  "name": "Early Bird"
}
```

#### Delete Schedule
```
POST /api/schedules/delete
Content-Type: application/json

{
  "id": 1
}
```

#### Toggle Schedule Enable/Disable
```
POST /api/schedules/toggle
Content-Type: application/json

{
  "id": 1,
  "enabled": false
}
```

#### Get Auto Power-Off Settings
```
GET /api/schedules/auto-off
```

Response:
```json
{
  "enabled": true,
  "minutes": 60
}
```

#### Set Auto Power-Off Settings
```
POST /api/schedules/auto-off
Content-Type: application/json

{
  "enabled": true,
  "minutes": 90
}
```

### WebSocket Commands

Commands are sent via WebSocket with type "command":

```json
{
  "type": "command",
  "cmd": "add_schedule",
  "enabled": true,
  "days": 62,
  "hour": 7,
  "minute": 0,
  "action": "on",
  "strategy": 1,
  "name": "Morning Brew"
}
```

Available commands:
- `add_schedule` - Add new schedule
- `update_schedule` - Update existing (requires `id`)
- `delete_schedule` - Delete schedule (requires `id`)
- `toggle_schedule` - Enable/disable (requires `id`, `enabled`)
- `set_auto_off` - Set auto power-off (`enabled`, `minutes`)
- `get_schedules` - Request schedules (broadcasts via state)

### Persistence

Schedules are stored in `/schedules.json` on LittleFS:

```json
{
  "schedules": [...],
  "autoPowerOffEnabled": true,
  "autoPowerOffMinutes": 60
}
```

### Schedule Execution

The `StateManager::checkSchedules()` method is called in the main loop:

1. Checks every 10 seconds (SCHEDULE_CHECK_INTERVAL)
2. Waits for NTP time sync (skips if time < 1000000)
3. Compares current time/day against each enabled schedule
4. Prevents duplicate triggers within the same minute
5. Calls the schedule callback when a match is found

### Idle Tracking

For auto power-off:

1. `_lastActivityTime` tracks last user activity
2. Activity is detected on:
   - Brewing start/stop
   - Encoder button press
   - Encoder rotation
3. `isIdleTimeout()` checks if idle duration exceeds configured timeout
4. Auto power-off only triggers when machine is ON and not brewing

### Schedule Callback

When a schedule triggers, the callback in `main.cpp`:

1. Sends `MSG_CMD_MODE` to Pico (0x01 for ON, 0x00 for OFF)
2. For ON actions, sends `MSG_CMD_CONFIG` with heating strategy
3. Shows notification on display

## Usage Examples

### Morning Coffee Schedule

Turn on machine at 7:00 AM on weekdays with sequential heating:

```json
{
  "enabled": true,
  "days": 62,
  "hour": 7,
  "minute": 0,
  "action": "on",
  "strategy": 1,
  "name": "Morning Coffee"
}
```

### Evening Shutdown

Turn off machine at 10:00 PM every day:

```json
{
  "enabled": true,
  "days": 127,
  "hour": 22,
  "minute": 0,
  "action": "off",
  "name": "Evening Shutdown"
}
```

### Weekend Brunch

Turn on at 9:30 AM on weekends with parallel heating:

```json
{
  "enabled": true,
  "days": 65,
  "hour": 9,
  "minute": 30,
  "action": "on",
  "strategy": 2,
  "name": "Weekend Brunch"
}
```

## Web UI

The Schedules page (`/schedules`) provides:

1. **Schedule List**
   - View all configured schedules
   - Toggle enable/disable
   - Edit or delete schedules
   - Visual indicators for action type (ON/OFF)

2. **Schedule Form**
   - Name (optional)
   - Action selection (Turn On / Turn Off)
   - Time picker (hour:minute)
   - Day selector with presets (Weekdays, Weekends, Every day)
   - Heating strategy selector (for ON actions)

3. **Auto Power-Off Settings**
   - Enable/disable toggle
   - Timeout configuration (5-480 minutes)

## Troubleshooting

### Schedule not triggering

1. **Check NTP sync**: Schedule requires valid time. Ensure WiFi is connected and NTP has synced.
2. **Verify enabled**: Check that the schedule is enabled.
3. **Check days**: Ensure current day matches the schedule's day bitmask.
4. **Check time**: Schedules trigger at the exact minute; ensure time is correct.

### Auto power-off not working

1. **Check enabled**: Ensure auto power-off is enabled.
2. **Check machine state**: Only triggers when machine is ON (not STANDBY).
3. **Check activity**: Any brewing or user interaction resets the idle timer.
4. **Check timeout**: Ensure timeout is > 0 minutes.

### Schedules not persisting

1. Check LittleFS is mounted
2. Verify free space on flash
3. Check for file corruption in `/schedules.json`

## Related Documentation

- [State Management](../State_Management.md)
- [Eco Mode](./Eco_Mode.md)
- [WebSocket Protocol](../../web/WebSocket_Protocol.md)

