# Firmware Debugging Guide

## Overview

This guide covers debugging strategies for the ECM Pico firmware, from development through production.

**Debugging Methods:**
1. SWD Hardware Debugging (Picoprobe/J-Link)
2. USB Serial Printf
3. Protocol Debug Messages (via ESP32)
4. LED/Buzzer Indicators
5. Runtime Debug Commands

---

## 1. SWD Hardware Debugging

The most powerful debugging method - supports breakpoints, stepping, and memory inspection.

### Option A: Picoprobe (Recommended - Low Cost)

Use a second Raspberry Pi Pico as a debug probe.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           PICOPROBE SETUP                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   ┌─────────────┐                           ┌─────────────┐                 │
│   │   Target    │         SWD               │  Picoprobe  │                 │
│   │    Pico     │◄─────────────────────────►│   (Debug)   │                 │
│   │             │  SWCLK, SWDIO, GND        │             │                 │
│   │  ECM Board  │                           │  2nd Pico   │                 │
│   └─────────────┘                           └──────┬──────┘                 │
│                                                    │                         │
│                                                   USB                        │
│                                                    │                         │
│                                                    ▼                         │
│                                             ┌───────────┐                   │
│                                             │  Laptop   │                   │
│                                             │  OpenOCD  │                   │
│                                             │    GDB    │                   │
│                                             └───────────┘                   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

#### Wiring

| Picoprobe GPIO | Target Pico | Function |
|----------------|-------------|----------|
| GP2 | SWCLK (pad) | Debug clock |
| GP3 | SWDIO (pad) | Debug data |
| GND | GND | Ground |
| GP4 (optional) | UART TX | Serial passthrough |
| GP5 (optional) | UART RX | Serial passthrough |

#### Flash Picoprobe Firmware

```bash
# Download Picoprobe UF2
# https://github.com/raspberrypi/picoprobe/releases

# Hold BOOTSEL on debugger Pico, connect USB
# Copy picoprobe.uf2 to RPI-RP2 drive
```

#### Install OpenOCD

```bash
# macOS
brew install openocd

# Ubuntu/Debian
sudo apt install openocd

# Verify
openocd --version
```

#### Start Debug Session

**Terminal 1 - OpenOCD:**
```bash
openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg
```

**Terminal 2 - GDB:**
```bash
arm-none-eabi-gdb build/ecm_firmware.elf

(gdb) target remote localhost:3333
(gdb) load
(gdb) monitor reset init
(gdb) break main
(gdb) continue
```

#### VS Code Configuration

**.vscode/launch.json:**
```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Pico Debug (Picoprobe)",
            "type": "cortex-debug",
            "request": "launch",
            "cwd": "${workspaceFolder}",
            "executable": "${workspaceFolder}/build/ecm_firmware.elf",
            "servertype": "openocd",
            "configFiles": [
                "interface/cmsis-dap.cfg",
                "target/rp2040.cfg"
            ],
            "openOCDLaunchCommands": ["adapter speed 5000"],
            "svdFile": "${env:PICO_SDK_PATH}/src/rp2040/hardware_regs/rp2040.svd",
            "runToEntryPoint": "main",
            "preLaunchTask": "build"
        }
    ]
}
```

**.vscode/tasks.json:**
```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "build",
            "type": "shell",
            "command": "cd build && make -j4",
            "group": {
                "kind": "build",
                "isDefault": true
            }
        }
    ]
}
```

**Required VS Code Extensions:**
- Cortex-Debug
- C/C++ (Microsoft)

### Option B: J-Link

Professional debugger with faster performance.

```bash
# Start J-Link GDB Server
JLinkGDBServer -device RP2040_M0_0 -if SWD -speed 4000

# Connect GDB
arm-none-eabi-gdb build/ecm_firmware.elf
(gdb) target remote localhost:2331
```

---

## 2. USB Serial Printf

Simple and effective for development.

### Enable in CMakeLists.txt

```cmake
# Enable USB serial output
pico_enable_stdio_usb(ecm_firmware 1)
pico_enable_stdio_uart(ecm_firmware 0)
```

### Debug Macros

```c
// debug.h

#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>
#include "pico/stdlib.h"

// Log levels
#define LOG_TRACE  0
#define LOG_DEBUG  1
#define LOG_INFO   2
#define LOG_WARN   3
#define LOG_ERROR  4
#define LOG_NONE   5

#ifndef LOG_LEVEL
    #ifdef DEBUG
        #define LOG_LEVEL LOG_DEBUG
    #else
        #define LOG_LEVEL LOG_WARN
    #endif
#endif

// Module IDs for filtering
#define MOD_MAIN     0x01
#define MOD_SAFETY   0x02
#define MOD_SENSORS  0x03
#define MOD_PID      0x04
#define MOD_CONTROL  0x05
#define MOD_COMMS    0x06
#define MOD_STATE    0x07

// Timestamp helper
#define TIMESTAMP() (to_ms_since_boot(get_absolute_time()))

// Log macros
#if LOG_LEVEL <= LOG_TRACE
    #define LOG_T(mod, fmt, ...) printf("[%8lu] T/%02X: " fmt "\n", TIMESTAMP(), mod, ##__VA_ARGS__)
#else
    #define LOG_T(mod, fmt, ...)
#endif

#if LOG_LEVEL <= LOG_DEBUG
    #define LOG_D(mod, fmt, ...) printf("[%8lu] D/%02X: " fmt "\n", TIMESTAMP(), mod, ##__VA_ARGS__)
#else
    #define LOG_D(mod, fmt, ...)
#endif

#if LOG_LEVEL <= LOG_INFO
    #define LOG_I(mod, fmt, ...) printf("[%8lu] I/%02X: " fmt "\n", TIMESTAMP(), mod, ##__VA_ARGS__)
#else
    #define LOG_I(mod, fmt, ...)
#endif

#if LOG_LEVEL <= LOG_WARN
    #define LOG_W(mod, fmt, ...) printf("[%8lu] W/%02X: " fmt "\n", TIMESTAMP(), mod, ##__VA_ARGS__)
#else
    #define LOG_W(mod, fmt, ...)
#endif

#if LOG_LEVEL <= LOG_ERROR
    #define LOG_E(mod, fmt, ...) printf("[%8lu] E/%02X: " fmt "\n", TIMESTAMP(), mod, ##__VA_ARGS__)
#else
    #define LOG_E(mod, fmt, ...)
#endif

#endif // DEBUG_H
```

### Usage

```c
#include "debug.h"

void read_sensors(void) {
    float brew_temp = read_ntc(ADC_BREW_NTC);
    float steam_temp = read_ntc(ADC_STEAM_NTC);
    
    LOG_D(MOD_SENSORS, "Brew=%.1f°C Steam=%.1f°C", brew_temp, steam_temp);
    
    if (brew_temp > MAX_BREW_TEMP) {
        LOG_E(MOD_SENSORS, "Brew over-temp! %.1f > %.1f", brew_temp, MAX_BREW_TEMP);
    }
}

void pid_compute(void) {
    LOG_T(MOD_PID, "PID: err=%.2f P=%.2f I=%.2f D=%.2f out=%.2f",
          error, p_term, i_term, d_term, output);
}
```

### View Output

```bash
# macOS - find device
ls /dev/tty.usbmodem*

# Connect
screen /dev/tty.usbmodem14201 115200

# Or use minicom
minicom -D /dev/tty.usbmodem14201 -b 115200

# Exit screen: Ctrl-A, then K, then Y
```

### Example Output

```
[     123] I/01: ECM Firmware v1.0.0 starting
[     124] I/01: Machine: ECM Synchronika
[     125] D/02: Watchdog enabled, 2000ms timeout
[     130] D/03: ADC initialized
[     135] D/03: SPI initialized for MAX31855
[     200] I/01: Self-test starting
[     250] D/03: Brew NTC: 24.3°C (OK)
[     300] D/03: Steam NTC: 23.8°C (OK)
[     350] D/03: Thermocouple: 22.5°C (OK)
[     400] D/03: Pressure: 0.12 bar (OK)
[     450] I/01: Self-test PASSED
[     451] I/07: State: BOOT -> HEATING
[    1000] D/04: Brew PID: SP=93.0 PV=24.3 err=68.7 out=100%
[    1000] D/04: Steam PID: SP=145.0 PV=23.8 err=121.2 out=100%
```

---

## 3. Protocol Debug Messages

Debug remotely via ESP32 when USB is not accessible.

### Add MSG_DEBUG to Protocol

```c
// protocol.h
#define MSG_DEBUG  0x06

typedef struct __attribute__((packed)) {
    uint32_t timestamp;      // ms since boot
    uint8_t  level;          // LOG_TRACE to LOG_ERROR
    uint8_t  module;         // Module ID
    uint8_t  len;            // Message length
    char     message[48];    // Fixed max length
} debug_payload_t;  // 55 bytes max
```

### Send Debug via Protocol

```c
// debug_protocol.c

#include "protocol.h"
#include <stdarg.h>

static uint8_t protocol_log_level = LOG_INFO;

void set_protocol_log_level(uint8_t level) {
    protocol_log_level = level;
}

void debug_protocol(uint8_t level, uint8_t module, const char* fmt, ...) {
    if (level < protocol_log_level) return;
    
    debug_payload_t payload;
    payload.timestamp = to_ms_since_boot(get_absolute_time());
    payload.level = level;
    payload.module = module;
    
    va_list args;
    va_start(args, fmt);
    payload.len = vsnprintf(payload.message, sizeof(payload.message), fmt, args);
    va_end(args);
    
    send_packet(MSG_DEBUG, &payload, 7 + payload.len);
}

// Convenience macros
#define PLOG_D(mod, fmt, ...) debug_protocol(LOG_DEBUG, mod, fmt, ##__VA_ARGS__)
#define PLOG_I(mod, fmt, ...) debug_protocol(LOG_INFO, mod, fmt, ##__VA_ARGS__)
#define PLOG_W(mod, fmt, ...) debug_protocol(LOG_WARN, mod, fmt, ##__VA_ARGS__)
#define PLOG_E(mod, fmt, ...) debug_protocol(LOG_ERROR, mod, fmt, ##__VA_ARGS__)
```

### ESP32 Receives and Forwards

```cpp
// ESP32 side - forward to Serial, WebSocket, MQTT

void handle_debug_message(const debug_payload_t* dbg) {
    const char* levels[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR"};
    
    // To Serial
    Serial.printf("[%lu] %s/%02X: %s\n", 
                  dbg->timestamp, 
                  levels[dbg->level], 
                  dbg->module, 
                  dbg->message);
    
    // To WebSocket (browser console)
    if (wsClient.connected()) {
        wsClient.printf("{\"type\":\"log\",\"level\":\"%s\",\"msg\":\"%s\"}", 
                        levels[dbg->level], dbg->message);
    }
    
    // To MQTT (remote logging)
    mqtt.publish("ecm/debug", dbg->message);
}
```

---

## 4. Runtime Debug Commands

Add debug commands for interactive debugging.

### Protocol Extension

```c
#define MSG_CMD_DEBUG  0x1D

typedef struct __attribute__((packed)) {
    uint8_t  cmd;       // Debug command
    uint8_t  param1;    // Command parameter 1
    uint8_t  param2;    // Command parameter 2
    uint8_t  param3;    // Command parameter 3
} cmd_debug_t;

// Debug commands
#define DBG_SET_LOG_LEVEL     0x01  // Set protocol log level
#define DBG_DUMP_STATE        0x02  // Trigger full state dump
#define DBG_DUMP_TIMING       0x03  // Dump loop timing stats
#define DBG_READ_ADC_RAW      0x04  // Read raw ADC value
#define DBG_READ_GPIO         0x05  // Read GPIO states
#define DBG_SET_OUTPUT        0x06  // Force output state (dangerous!)
#define DBG_TRIGGER_WATCHDOG  0x07  // Test watchdog reset
#define DBG_MEMORY_STATS      0x08  // Report memory usage
#define DBG_RESET_STATS       0x09  // Reset all counters
```

### Debug Response

```c
#define MSG_DEBUG_RESP  0x07

typedef struct __attribute__((packed)) {
    uint8_t  cmd;           // Original command
    uint8_t  status;        // 0=OK, 1=error
    uint8_t  len;           // Data length
    uint8_t  data[52];      // Response data
} debug_resp_payload_t;
```

### Implementation

```c
void handle_debug_command(const cmd_debug_t* cmd) {
    debug_resp_payload_t resp;
    resp.cmd = cmd->cmd;
    resp.status = 0;
    
    switch (cmd->cmd) {
        case DBG_SET_LOG_LEVEL:
            set_protocol_log_level(cmd->param1);
            resp.len = 0;
            break;
            
        case DBG_DUMP_TIMING: {
            timing_stats_t* stats = get_timing_stats();
            resp.len = snprintf((char*)resp.data, sizeof(resp.data),
                "loops=%lu avg=%luus max=%luus",
                stats->count, stats->avg_us, stats->max_us);
            break;
        }
        
        case DBG_READ_ADC_RAW: {
            uint16_t raw = adc_read_raw(cmd->param1);
            resp.len = snprintf((char*)resp.data, sizeof(resp.data),
                "ADC%d=%u (%.3fV)", cmd->param1, raw, raw * 3.3f / 4096);
            break;
        }
        
        case DBG_READ_GPIO: {
            uint32_t all_gpio = gpio_get_all();
            memcpy(resp.data, &all_gpio, 4);
            resp.len = 4;
            break;
        }
        
        case DBG_MEMORY_STATS: {
            extern char __StackLimit, __bss_end__;
            uint32_t heap_used = (uint32_t)&__bss_end__ - 0x20000000;
            uint32_t stack_free = (uint32_t)&__StackLimit - (uint32_t)__get_MSP();
            resp.len = snprintf((char*)resp.data, sizeof(resp.data),
                "heap=%lu stack_free=%lu", heap_used, stack_free);
            break;
        }
        
        default:
            resp.status = 1;  // Unknown command
            resp.len = 0;
    }
    
    send_packet(MSG_DEBUG_RESP, &resp, 3 + resp.len);
}
```

---

## 5. LED Debug Codes

Visual debugging without any tools.

### Error Code Blink Patterns

```c
// debug_led.h

// Error codes displayed as blink patterns
// Long blink = 1, Short blink = 0
// Example: Error 0x05 = binary 101 = long-short-long

void debug_blink_error(uint8_t error_code);
void debug_blink_byte(uint8_t value);
void debug_blink_pattern(const char* pattern);  // e.g., "LSL" = long-short-long
```

### Implementation

```c
// debug_led.c

#define LONG_MS   400
#define SHORT_MS  150
#define GAP_MS    200
#define PAUSE_MS  1000

void debug_blink_error(uint8_t error_code) {
    // Blink error code in binary, MSB first
    for (int i = 7; i >= 0; i--) {
        gpio_put(PIN_STATUS_LED, 1);
        
        if (error_code & (1 << i)) {
            sleep_ms(LONG_MS);   // 1 = long
        } else {
            sleep_ms(SHORT_MS);  // 0 = short
        }
        
        gpio_put(PIN_STATUS_LED, 0);
        sleep_ms(GAP_MS);
    }
    sleep_ms(PAUSE_MS);
}

// In fault handler
void enter_fault_state(uint8_t error_code) {
    // All outputs safe
    enter_safe_state();
    
    // Blink error code forever
    while (1) {
        debug_blink_error(error_code);
        watchdog_update();  // Keep alive to show pattern
    }
}
```

### Error Code Reference

| Code | Binary | Pattern | Error |
|------|--------|---------|-------|
| 0x01 | 00000001 | SSSSSSSL | Watchdog timeout |
| 0x02 | 00000010 | SSSSSSLS | Brew NTC open |
| 0x03 | 00000011 | SSSSSSLL | Brew NTC short |
| 0x04 | 00000100 | SSSSSLS | Steam NTC open |
| 0x05 | 00000101 | SSSSSLSL | Steam NTC short |
| 0x06 | 00000110 | SSSSSLLS | Brew over-temp |
| 0x07 | 00000111 | SSSSSLL | Steam over-temp |
| 0x08 | 00001000 | SSSSLS | Steam level low |
| 0x09 | 00001001 | SSSSLSSL | No reservoir |
| 0x0A | 00001010 | SSSSLSLS | Tank level low |

(L = Long, S = Short)

---

## 6. Timing Analysis

Measure control loop timing to catch performance issues.

### Timing Instrumentation

```c
// timing.h

typedef struct {
    uint32_t count;
    uint32_t total_us;
    uint32_t min_us;
    uint32_t max_us;
    uint32_t last_us;
} timing_stats_t;

void timing_start(timing_stats_t* stats);
void timing_stop(timing_stats_t* stats);
void timing_reset(timing_stats_t* stats);
float timing_avg_us(const timing_stats_t* stats);
```

### Implementation

```c
// timing.c

static uint32_t timer_start;

void timing_start(timing_stats_t* stats) {
    timer_start = time_us_32();
}

void timing_stop(timing_stats_t* stats) {
    uint32_t elapsed = time_us_32() - timer_start;
    
    stats->count++;
    stats->total_us += elapsed;
    stats->last_us = elapsed;
    
    if (elapsed < stats->min_us || stats->min_us == 0) {
        stats->min_us = elapsed;
    }
    if (elapsed > stats->max_us) {
        stats->max_us = elapsed;
    }
}

float timing_avg_us(const timing_stats_t* stats) {
    if (stats->count == 0) return 0;
    return (float)stats->total_us / stats->count;
}
```

### Usage in Control Loop

```c
static timing_stats_t loop_timing;
static timing_stats_t sensor_timing;
static timing_stats_t pid_timing;

void control_loop(void) {
    timing_start(&loop_timing);
    
    // Safety
    check_interlocks();
    
    // Sensors
    timing_start(&sensor_timing);
    read_all_sensors();
    timing_stop(&sensor_timing);
    
    // PID
    timing_start(&pid_timing);
    compute_pid();
    timing_stop(&pid_timing);
    
    // ...
    
    timing_stop(&loop_timing);
    
    // Report every 10 seconds
    if (loop_timing.count % 100 == 0) {
        LOG_I(MOD_MAIN, "Loop: avg=%.0fus max=%luus", 
              timing_avg_us(&loop_timing), loop_timing.max_us);
        LOG_D(MOD_MAIN, "  Sensors: avg=%.0fus max=%luus",
              timing_avg_us(&sensor_timing), sensor_timing.max_us);
        LOG_D(MOD_MAIN, "  PID: avg=%.0fus max=%luus",
              timing_avg_us(&pid_timing), pid_timing.max_us);
    }
}
```

---

## 7. Build Configurations

### CMakeLists.txt

```cmake
# Build type
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type")
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo")

# Debug configuration
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug build - full debugging enabled")
    add_compile_definitions(DEBUG=1)
    add_compile_definitions(LOG_LEVEL=0)  # TRACE
    add_compile_options(-g3 -O0)
    add_compile_options(-Wall -Wextra -Wpedantic)
    
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    message(STATUS "Release with debug info")
    add_compile_definitions(DEBUG=1)
    add_compile_definitions(LOG_LEVEL=2)  # INFO
    add_compile_options(-g -O2)
    
else()
    message(STATUS "Release build - minimal debugging")
    add_compile_definitions(DEBUG=0)
    add_compile_definitions(LOG_LEVEL=3)  # WARN only
    add_compile_options(-O2)
endif()
```

### Build Commands

```bash
# Debug build (full debugging)
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j4

# Release with debug info
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make -j4

# Release (production)
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4
```

---

## 8. Debug Checklist

### Development Setup

- [ ] Picoprobe wired and firmware flashed
- [ ] OpenOCD installed and tested
- [ ] VS Code with Cortex-Debug extension
- [ ] Debug build configuration in CMake
- [ ] USB serial output working

### Before Testing

- [ ] Log level appropriate for test
- [ ] Timing instrumentation enabled
- [ ] Watchdog disabled or extended (for breakpoints)
- [ ] Safe test environment (isolation transformer, fire extinguisher)

### During Debug Session

- [ ] Monitor USB serial output
- [ ] Check ESP32 forwarded logs
- [ ] Watch for timing warnings
- [ ] Observe LED patterns

### Common Issues

| Symptom | Possible Cause | Debug Approach |
|---------|----------------|----------------|
| No USB serial output | USB not initialized | Check `pico_enable_stdio_usb` |
| Garbled output | Wrong baud rate | Verify 115200 in terminal |
| Pico resets during debug | Watchdog firing | Disable watchdog or extend timeout |
| Breakpoint not hit | Optimization | Use `-O0` in debug build |
| Variables "optimized out" | Compiler optimization | Add `volatile` or use `-O0` |
| SWD not connecting | Wiring issue | Check SWCLK/SWDIO connections |

---

## 9. Remote Debugging (Production)

When the machine is assembled and USB is inaccessible:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      REMOTE DEBUGGING ARCHITECTURE                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   ┌─────────┐     UART      ┌─────────┐     WiFi     ┌─────────────────┐   │
│   │  Pico   │──────────────►│  ESP32  │─────────────►│  Browser/MQTT   │   │
│   │         │  MSG_DEBUG    │         │  WebSocket   │                 │   │
│   └─────────┘               └─────────┘              └─────────────────┘   │
│                                                                              │
│   Debug Flow:                                                               │
│   1. Pico sends MSG_DEBUG packets                                          │
│   2. ESP32 receives and forwards via WebSocket                             │
│   3. Browser displays real-time log console                                │
│   4. Or ESP32 publishes to MQTT for remote monitoring                      │
│                                                                              │
│   Debug Commands (via ESP32 UI):                                            │
│   • Set log level                                                           │
│   • Request state dump                                                      │
│   • Read raw sensor values                                                  │
│   • Check timing statistics                                                 │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Quick Reference

### Log Levels

| Level | Value | Use |
|-------|-------|-----|
| TRACE | 0 | Detailed flow tracing |
| DEBUG | 1 | Development debugging |
| INFO | 2 | Normal operational info |
| WARN | 3 | Warnings (non-critical) |
| ERROR | 4 | Errors (critical) |

### Module IDs

| ID | Module |
|----|--------|
| 0x01 | Main |
| 0x02 | Safety |
| 0x03 | Sensors |
| 0x04 | PID |
| 0x05 | Control |
| 0x06 | Comms |
| 0x07 | State |

### Debug Message Types

| Type | Value | Direction |
|------|-------|-----------|
| MSG_DEBUG | 0x06 | Pico → ESP32 |
| MSG_DEBUG_RESP | 0x07 | Pico → ESP32 |
| MSG_CMD_DEBUG | 0x1D | ESP32 → Pico |

