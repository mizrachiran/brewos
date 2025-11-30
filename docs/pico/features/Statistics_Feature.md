# Brew Statistics Feature

This document describes the brew statistics feature, which provides comprehensive tracking and analytics for brew cycles.

**See also:** [Feature Status Table](Feature_Status_Table.md) for implementation status.

---

## Overview

The statistics feature provides comprehensive tracking and analytics for brew cycles, separate from the cleaning counter. This enables users to view detailed statistics about their espresso machine usage.

**Key Distinction:**
- **Cleaning Counter**: Simple counter (≥15s brews) that resets after cleaning - used only for cleaning reminders
- **Statistics**: Comprehensive tracking (≥5s brews) with historical data, averages, and time-based analytics - never resets automatically

---

## Features

### Overall Statistics
- **Total Brews**: All-time count of brew cycles
- **Average Brew Time**: Average duration across all brews
- **Min/Max Brew Times**: Shortest and longest brew times
- **Total Brew Time**: Cumulative time spent brewing

### Time-Based Statistics
- **Daily**: Cups per day (rolling 24 hours), average brew time
- **Weekly**: Cups per week (rolling 7 days), average brew time
- **Monthly**: Cups per month (rolling 30 days), average brew time

### Historical Data
- **Brew History**: Last 100 brew entries with timestamps and durations
- **Chronological Tracking**: Entries stored with timestamps for time-based calculations

---

## API

### Initialization

```c
void statistics_init(void);
```

Called once at boot. Initializes statistics data structures.

### Record Brew

```c
bool statistics_record_brew(uint32_t duration_ms);
```

Records a brew cycle:
- Validates duration (5s - 2min)
- Updates overall statistics
- Adds to history (circular buffer)
- Recalculates time-based stats
- Returns `true` if recorded successfully

### Get Statistics

```c
bool statistics_get(statistics_t* stats);
uint32_t statistics_get_total_brews(void);
uint16_t statistics_get_avg_brew_time_ms(void);
bool statistics_get_daily(daily_stats_t* stats);
bool statistics_get_weekly(weekly_stats_t* stats);
bool statistics_get_monthly(monthly_stats_t* stats);
uint8_t statistics_get_history(brew_entry_t* entries, uint8_t max_entries);
```

Retrieve statistics:
- `statistics_get()`: Complete statistics structure
- `statistics_get_total_brews()`: Total brew count
- `statistics_get_avg_brew_time_ms()`: Average brew time
- `statistics_get_daily/weekly/monthly()`: Time-based statistics
- `statistics_get_history()`: Historical brew entries (newest first)

### Reset

```c
void statistics_reset_all(void);
```

Resets all statistics (clears all data). User-initiated action.

### Update

```c
void statistics_update_time_based(void);
```

Recalculates time-based statistics. Call periodically (e.g., every hour) to update daily/weekly/monthly stats as time passes.

---

## Data Structures

### brew_entry_t

```c
typedef struct __attribute__((packed)) {
    uint32_t timestamp;        // Milliseconds since boot
    uint16_t duration_ms;      // Brew duration in milliseconds
    uint8_t reserved[2];       // Reserved for future use
} brew_entry_t;  // 8 bytes
```

### daily_stats_t / weekly_stats_t / monthly_stats_t

```c
typedef struct __attribute__((packed)) {
    uint16_t count;            // Number of brews in time period
    uint32_t total_time_ms;    // Total brew time in time period
    uint16_t avg_time_ms;      // Average brew time in time period
} daily_stats_t;  // 8 bytes
```

### statistics_t

```c
typedef struct {
    // Overall statistics
    uint32_t total_brews;
    uint32_t total_brew_time_ms;
    uint16_t avg_brew_time_ms;
    uint16_t min_brew_time_ms;
    uint16_t max_brew_time_ms;
    
    // Time-based statistics
    daily_stats_t daily;
    weekly_stats_t weekly;
    monthly_stats_t monthly;
    
    // History (circular buffer)
    brew_entry_t history[STATS_MAX_HISTORY_ENTRIES];
    uint8_t history_count;
    uint8_t history_index;
    
    // Metadata
    uint32_t last_brew_timestamp;
    bool initialized;
} statistics_t;
```

---

## Configuration

| Parameter | Value | Description |
|-----------|-------|-------------|
| `STATS_MAX_HISTORY_ENTRIES` | 100 | Maximum number of brew entries to store |
| `STATS_MIN_BREW_TIME_MS` | 5000 | Minimum brew time to count (5 seconds) |
| `STATS_MAX_BREW_TIME_MS` | 120000 | Maximum brew time (2 minutes) |

---

## Time-Based Calculations

Statistics use **rolling windows** for time-based calculations:
- **Daily**: Last 24 hours from current time
- **Weekly**: Last 7 days from current time
- **Monthly**: Last 30 days from current time

When `statistics_update_time_based()` is called, it:
1. Iterates through history entries
2. Checks if each entry is within the time window
3. Recalculates counts and averages

**Note:** Currently uses "milliseconds since boot" for timestamps. If RTC is available, can be extended to use actual date/time.

---

## Integration with Brew Cycle

Statistics are automatically recorded when a brew cycle completes:

```c
// In state.c, state_exit_action(STATE_BREWING)
uint32_t brew_duration = g_brew_stop_time - g_brew_start_time;

// Record for statistics (all brews >= 5 seconds)
statistics_record_brew(brew_duration);

// Record for cleaning counter (only brews >= 15 seconds)
cleaning_record_brew_cycle(brew_duration);
```

**Key Differences:**
- **Statistics**: Records all brews ≥5 seconds (comprehensive tracking)
- **Cleaning Counter**: Records only brews ≥15 seconds (for cleaning reminders)

---

## Communication Protocol

### Protocol Integration

Statistics are exposed via communication protocol:

#### MSG_CMD_GET_STATISTICS (0x1C) - ESP32 → Pico

Request complete statistics.

```c
// No payload - just header with LENGTH=0
// Pico responds with MSG_STATISTICS containing statistics_payload_t
```

**Status:** Message type defined but handler not yet implemented in `main.c`.

#### MSG_STATISTICS (0x09) - Pico → ESP32

Statistics response.

```c
typedef struct __attribute__((packed)) {
    uint32_t total_brews;
    uint16_t avg_brew_time_ms;
    uint16_t min_brew_time_ms;
    uint16_t max_brew_time_ms;
    daily_stats_t daily;
    weekly_stats_t weekly;
    monthly_stats_t monthly;
    // History sent separately or truncated
} statistics_payload_t;
```

---

## Persistence

Statistics are persisted to flash and survive reboots:

1. **Periodic Saves**: Saves every 10 brews (configurable via `STATS_SAVE_INTERVAL`)
2. **Summary Data**: Stores overall stats and recent history (last 50 entries)
3. **Time-Based Stats**: Recalculated on boot from persisted history
4. **Flash Sector**: Uses dedicated flash sector (second-to-last sector, config uses last)

**Flash Storage Details:**
- Uses `persisted_statistics_t` structure (smaller than full `statistics_t`)
- Stores last 50 history entries (reduced from 100 to fit in flash)
- CRC32 validation for data integrity
- Magic number and version for format validation

**Automatic Operations:**
- `statistics_init()`: Loads persisted data from flash on boot
- `statistics_record_brew()`: Triggers save every 10 brews
- Flash operations handled safely with interrupt disable

---

## Usage Example

### ESP32 Web UI

```javascript
// Request statistics
sendCommand(MSG_CMD_GET_STATISTICS);

// Display statistics
function displayStatistics(stats) {
    // Overall stats
    document.getElementById('total-brews').textContent = stats.total_brews;
    document.getElementById('avg-time').textContent = formatTime(stats.avg_brew_time_ms);
    document.getElementById('min-time').textContent = formatTime(stats.min_brew_time_ms);
    document.getElementById('max-time').textContent = formatTime(stats.max_brew_time_ms);
    
    // Daily stats
    document.getElementById('cups-today').textContent = stats.daily.count;
    document.getElementById('avg-today').textContent = formatTime(stats.daily.avg_time_ms);
    
    // Weekly stats
    document.getElementById('cups-week').textContent = stats.weekly.count;
    document.getElementById('avg-week').textContent = formatTime(stats.weekly.avg_time_ms);
    
    // Monthly stats
    document.getElementById('cups-month').textContent = stats.monthly.count;
    document.getElementById('avg-month').textContent = formatTime(stats.monthly.avg_time_ms);
}
```

---

## Design Notes

### Timestamps

Currently uses "milliseconds since boot" for timestamps. If RTC hardware is available, the system can be extended to use actual date/time. The `statistics_update_time_based()` function should be called periodically (e.g., every hour) to keep rolling window calculations accurate.

---

## Potential Enhancements

1. **Advanced Analytics**:
   - Brew time distribution (histogram)
   - Peak usage times
   - Trends over time

2. **Export/Import**:
   - Export statistics to JSON/CSV
   - Import historical data
   - Backup/restore functionality

3. **Visualizations**:
   - Charts for brew times
   - Usage graphs (daily/weekly/monthly)
   - Timeline view

4. **Filtering**:
   - Filter by date range
   - Filter by brew time range
   - Custom time periods

---

## Testing

### Manual Testing

1. **Record Brews**:
   - Pull multiple shots (various durations)
   - Verify statistics update correctly
   - Check min/max/average calculations

2. **Time-Based Stats**:
   - Record brews over multiple days
   - Call `statistics_update_time_based()`
   - Verify daily/weekly/monthly counts

3. **History**:
   - Record >100 brews
   - Verify circular buffer wraps correctly
   - Check history retrieval (newest first)

4. **Reset**:
   - Call `statistics_reset_all()`
   - Verify all statistics cleared
   - Verify history cleared

---

## Summary

The statistics feature provides:
- Overall statistics (total brews, average/min/max brew times)
- Time-based statistics (daily, weekly, monthly rolling windows)
- Historical data (last 100 brews in RAM, last 50 in flash)
- Automatic recording on brew completion (brews ≥5 seconds)
- Flash persistence (saves every 10 brews, loads on boot)
- Protocol messages for requesting and receiving statistics

**Key Design Decision:** Statistics track all brews ≥5 seconds for detailed analytics, while the cleaning counter only tracks brews ≥15 seconds for cleaning reminders. Statistics never reset automatically (unlike the cleaning counter which resets after cleaning).

