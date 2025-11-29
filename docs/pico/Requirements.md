# ECM Synchronika Control Board - Pico Firmware Requirements

## Firmware Requirements Specification

**Document Purpose:** Define all functional, safety, and interface requirements for the RP2040 firmware  
**Target Platform:** Raspberry Pi Pico (RP2040)  
**Language:** C (Pico SDK)  
**Revision:** 1.0  
**Date:** November 2025  
**Related Documents:** 
- [Hardware Specification](../hardware/Specification.md)
- [Test Procedures](../hardware/Test_Procedures.md)
- [Architecture](Architecture.md)
- [Communication Protocol](Communication_Protocol.md)
- [Machine Configurations](Machine_Configurations.md)

---

# TABLE OF CONTENTS

1. [System Overview](#1-system-overview)
2. [Safety Requirements (CRITICAL)](#2-safety-requirements-critical)
3. [Hardware Interface Requirements](#3-hardware-interface-requirements)
4. [Functional Requirements](#4-functional-requirements)
5. [Communication Protocol](#5-communication-protocol)
6. [State Machine](#6-state-machine)
7. [PID Control Requirements](#7-pid-control-requirements)
8. [Error Handling](#8-error-handling)
9. [Startup Sequence](#9-startup-sequence)
10. [Module Architecture](#10-module-architecture)
11. [Implementation Phases](#11-implementation-phases)

---

# 1. System Overview

## 1.1 Firmware Purpose

The Pico firmware is responsible for:
- Real-time control of dual-boiler espresso machine
- Temperature regulation via PID control (brew and steam boilers)
- Safety interlock enforcement (highest priority)
- Sensor data acquisition and processing
- Communication with ESP32 display module
- Power monitoring via PZEM-004T

## 1.2 Architecture Philosophy

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           FIRMWARE ARCHITECTURE                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   ┌──────────────────────────────────────────────────────────────────────┐  │
│   │                     CORE 0 - REAL-TIME CONTROL                        │  │
│   │  ┌────────────┐  ┌────────────┐  ┌────────────┐  ┌────────────┐      │  │
│   │  │  Safety    │  │    PID     │  │   Sensor   │  │  Output    │      │  │
│   │  │ Interlocks │  │  Control   │  │  Reading   │  │  Drivers   │      │  │
│   │  │ (100ms)    │  │  (100ms)   │  │  (50ms)    │  │            │      │  │
│   │  └────────────┘  └────────────┘  └────────────┘  └────────────┘      │  │
│   └──────────────────────────────────────────────────────────────────────┘  │
│                                                                              │
│   ┌──────────────────────────────────────────────────────────────────────┐  │
│   │                     CORE 1 - COMMUNICATION                            │  │
│   │  ┌────────────┐  ┌────────────┐  ┌────────────┐  ┌────────────┐      │  │
│   │  │   ESP32    │  │   PZEM     │  │   State    │  │    OTA     │      │  │
│   │  │   UART     │  │   UART     │  │  Broadcast │  │  Handler   │      │  │
│   │  │  (async)   │  │  (100ms)   │  │            │  │            │      │  │
│   │  └────────────┘  └────────────┘  └────────────┘  └────────────┘      │  │
│   └──────────────────────────────────────────────────────────────────────┘  │
│                                                                              │
│   ┌──────────────────────────────────────────────────────────────────────┐  │
│   │                     SHARED STATE (Thread-Safe)                        │  │
│   │  ┌─────────────────────────────────────────────────────────────────┐ │  │
│   │  │  Temperatures │ Pressure │ Levels │ Outputs │ Alarms │ Config  │ │  │
│   │  └─────────────────────────────────────────────────────────────────┘ │  │
│   └──────────────────────────────────────────────────────────────────────┘  │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

# 2. Safety Requirements (CRITICAL)

```
⚠️⚠️⚠️  THESE REQUIREMENTS ARE NON-NEGOTIABLE  ⚠️⚠️⚠️

Failure to implement these correctly can result in:
• Fire hazard from dry-running heaters
• Equipment destruction
• Personal injury
```

## 2.1 Watchdog Timer

| ID | Requirement | Priority |
|----|-------------|----------|
| SAF-001 | Enable RP2040 watchdog timer immediately on boot | CRITICAL |
| SAF-002 | Watchdog timeout: 2000ms maximum | CRITICAL |
| SAF-003 | Feed watchdog only from main control loop after safety checks pass | CRITICAL |
| SAF-004 | On watchdog timeout: all outputs must go to safe state (OFF) | CRITICAL |

## 2.2 Water Level Interlocks

| ID | Requirement | Priority |
|----|-------------|----------|
| SAF-010 | If water reservoir switch (GPIO2) = HIGH: disable pump, disable heaters | CRITICAL |
| SAF-011 | If tank level sensor (GPIO3) = HIGH: disable pump, disable heaters | CRITICAL |
| SAF-012 | If steam boiler level probe (GPIO4) = HIGH: disable steam SSR immediately | CRITICAL |
| SAF-013 | Debounce water sensors: 3-5 consecutive readings (50-100ms total) | HIGH |
| SAF-014 | Log all interlock activations with timestamp | HIGH |

## 2.3 Over-Temperature Protection

| ID | Requirement | Priority |
|----|-------------|----------|
| SAF-020 | Brew boiler max temperature: 130°C → disable SSR1 | CRITICAL |
| SAF-021 | Steam boiler max temperature: 165°C → disable SSR2 | CRITICAL |
| SAF-022 | Thermocouple (brew head) max: 110°C → disable SSR1 + alert | HIGH |
| SAF-023 | If NTC reads open circuit (>150°C calculated): disable associated SSR | CRITICAL |
| SAF-024 | If NTC reads short circuit (<-20°C calculated): disable associated SSR | CRITICAL |
| SAF-025 | Implement hysteresis: re-enable 10°C below max threshold | MEDIUM |

## 2.4 Output Safety

| ID | Requirement | Priority |
|----|-------------|----------|
| SAF-030 | All outputs default to OFF at startup (GPIO set LOW before enabling as output) | CRITICAL |
| SAF-031 | Maximum SSR ON time without temperature change: 60s → disable + alert | HIGH |
| SAF-032 | SSR duty cycle limit: 95% maximum (prevents relay welding) | MEDIUM |
| SAF-033 | Pump cannot run without water reservoir present | CRITICAL |
| SAF-034 | Solenoid (3-way valve) only activates during brew cycle | MEDIUM |

## 2.5 Safe State Definition

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           SAFE STATE (ALL OUTPUTS)                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   On ANY critical error, watchdog timeout, or unrecoverable fault:          │
│                                                                              │
│   OUTPUT          │ SAFE STATE  │ GPIO     │ REASON                         │
│   ────────────────│─────────────│──────────│────────────────────────────    │
│   SSR1 (Brew)     │ OFF (LOW)   │ GPIO13   │ Prevent heater fire            │
│   SSR2 (Steam)    │ OFF (LOW)   │ GPIO14   │ Prevent heater fire            │
│   K1 (Water LED)  │ OFF (LOW)   │ GPIO10   │ Indicate fault                 │
│   K2 (Pump)       │ OFF (LOW)   │ GPIO11   │ Prevent dry running            │
│   K3 (Solenoid)   │ OFF (LOW)   │ GPIO12   │ Prevent stuck valve            │
│   K4 (Spare)      │ OFF (LOW)   │ GPIO20   │ Safe default                   │
│   Status LED      │ BLINK FAST  │ GPIO15   │ Indicate fault (2Hz)           │
│   Buzzer          │ ALERT TONE  │ GPIO19   │ 3 beeps, then silent           │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

# 3. Hardware Interface Requirements

## 3.1 GPIO Pin Mapping

```python
# Pin Definitions (Constants)
class Pins:
    # UART - ESP32 Display
    UART0_TX = 0        # To ESP32 RX
    UART0_RX = 1        # From ESP32 TX
    
    # Digital Inputs (Active LOW, Internal Pull-Up)
    WATER_RESERVOIR = 2 # Water tank present switch
    TANK_LEVEL = 3      # Tank level float switch
    STEAM_LEVEL = 4     # Steam boiler level (comparator output)
    BREW_SWITCH = 5     # Brew handle switch
    
    # UART - PZEM Power Meter
    PZEM_TX = 6         # To PZEM RX
    PZEM_RX = 7         # From PZEM TX
    
    # I2C Accessory Port
    I2C_SDA = 8
    I2C_SCL = 9
    
    # Relay Outputs (Active HIGH)
    RELAY_K1 = 10       # Water LED
    RELAY_K2 = 11       # Pump
    RELAY_K3 = 12       # Solenoid (3-way valve)
    
    # SSR Outputs (Active HIGH)
    SSR_BREW = 13       # Brew heater
    SSR_STEAM = 14      # Steam heater
    
    # User Interface
    STATUS_LED = 15     # Green status LED
    BUZZER = 19         # Passive piezo (PWM)
    
    # Spare Relay
    RELAY_K4 = 20       # Spare SPDT relay
    
    # SPI - MAX31855 Thermocouple
    SPI_MISO = 16       # MAX31855 DO
    SPI_CS = 17         # MAX31855 CS
    SPI_SCK = 18        # MAX31855 CLK
    
    # Analog Inputs (ADC)
    ADC_BREW_NTC = 26   # Brew boiler NTC
    ADC_STEAM_NTC = 27  # Steam boiler NTC
    ADC_PRESSURE = 28   # Pressure transducer
```

## 3.2 Sensor Specifications

### 3.2.1 NTC Thermistors (Brew & Steam)

| Parameter | Value |
|-----------|-------|
| Type | NTC 3.3kΩ @ 25°C |
| Series Resistor | 3.3kΩ 1% |
| B-Value | ~3950K (verify with sensor datasheet) |
| ADC Voltage at 25°C | ~1.65V |
| ADC Voltage at 0°C | ~2.45V |
| ADC Voltage at 90°C | ~0.37V |
| Sampling Rate | 20 Hz (50ms) |
| Filtering | Moving average (8 samples) |

```python
# NTC Temperature Calculation (Steinhart-Hart)
def ntc_to_temp(adc_value, vref=3.3, r_series=3300, r_ntc_25=3300, b_value=3950):
    """Convert ADC reading to temperature in Celsius"""
    voltage = adc_value * vref / 65535  # 16-bit ADC
    
    if voltage <= 0 or voltage >= vref:
        return None  # Open or short circuit
    
    r_ntc = r_series * voltage / (vref - voltage)
    
    # Steinhart-Hart simplified (B-parameter equation)
    t_kelvin = 1 / (1/298.15 + (1/b_value) * math.log(r_ntc / r_ntc_25))
    return t_kelvin - 273.15
```

### 3.2.2 MAX31855 Thermocouple

| Parameter | Value |
|-----------|-------|
| Type | K-Type thermocouple |
| Interface | SPI (Mode 0, 5MHz max) |
| Resolution | 0.25°C |
| Range | -200°C to +1350°C |
| Sampling Rate | 4 Hz (250ms) - sensor limit |
| Cold Junction | Internal compensation |

### 3.2.3 Pressure Transducer (YD4060)

| Parameter | Value |
|-----------|-------|
| Range | 0-16 bar |
| Output | 0.5V (0 bar) to 4.5V (16 bar) |
| Voltage Divider | 10kΩ / 15kΩ (ratio 0.6) |
| ADC Input Range | 0.3V - 2.7V |
| Resolution | 0.0047 bar per ADC count |
| Sampling Rate | 10 Hz (100ms) |

```python
# Pressure Calculation
def adc_to_pressure(adc_value, vref=3.3, range_bar=16):
    """Convert ADC reading to pressure in bar"""
    voltage = adc_value * vref / 65535
    
    # Reverse voltage divider: V_in = V_adc / 0.6
    v_transducer = voltage / 0.6
    
    # YD4060: 0.5V = 0 bar, 4.5V = 16 bar
    pressure = (v_transducer - 0.5) * range_bar / 4.0
    return max(0, pressure)  # Clamp to 0 minimum
```

## 3.3 Communication Interfaces

### 3.3.1 UART0 - ESP32 Display

| Parameter | Value |
|-----------|-------|
| Baud Rate | 921600 |
| Format | 8N1 |
| GPIO TX | 0 |
| GPIO RX | 1 |
| Protocol | JSON-based messages (see Section 5) |
| Timeout | 5s (consider ESP32 disconnected) |

### 3.3.2 UART1 - PZEM-004T Power Meter

| Parameter | Value |
|-----------|-------|
| Baud Rate | 9600 |
| Format | 8N1 |
| GPIO TX | 6 |
| GPIO RX | 7 |
| Protocol | Modbus-RTU |
| Address | 0xF8 (default) |
| Polling Rate | 1 Hz (1000ms) |

---

# 4. Functional Requirements

## 4.1 Temperature Control

| ID | Requirement | Priority |
|----|-------------|----------|
| FUNC-001 | Read brew boiler NTC at 20Hz, apply 8-sample moving average | HIGH |
| FUNC-002 | Read steam boiler NTC at 20Hz, apply 8-sample moving average | HIGH |
| FUNC-003 | Read MAX31855 thermocouple at 4Hz | HIGH |
| FUNC-004 | PID control loop for brew boiler at 10Hz (100ms) | HIGH |
| FUNC-005 | PID control loop for steam boiler at 10Hz (100ms) | HIGH |
| FUNC-006 | SSR output via PWM with 1-second period (zero-cross SSR) | HIGH |
| FUNC-007 | Configurable setpoints via ESP32 commands | MEDIUM |
| FUNC-008 | Default brew setpoint: per machine config (e.g., 93°C for ECM) | MEDIUM |
| FUNC-009 | Default steam setpoint: per machine config (e.g., 145°C for ECM) | MEDIUM |

## 4.2 Heating Strategy

| ID | Requirement | Priority |
|----|-------------|----------|
| FUNC-010 | Support configurable heating strategies | HIGH |
| FUNC-011 | HEAT_BREW_ONLY: only brew boiler heats | HIGH |
| FUNC-012 | HEAT_SEQUENTIAL: brew first, steam after brew reaches threshold | HIGH |
| FUNC-013 | HEAT_PARALLEL: both boilers heat simultaneously | MEDIUM |
| FUNC-014 | HEAT_STEAM_PRIORITY: steam first, then brew | MEDIUM |
| FUNC-015 | HEAT_SMART_STAGGER: both with limited combined duty cycle | MEDIUM |
| FUNC-016 | Default heating strategy: per machine config | MEDIUM |
| FUNC-017 | Heating strategy changeable at runtime via ESP32 command | MEDIUM |
| FUNC-018 | Save heating strategy to flash for persistence across reboots | MEDIUM |

## 4.3 Brew Cycle Control

| ID | Requirement | Priority |
|----|-------------|----------|
| FUNC-020 | Detect brew switch activation (GPIO5 LOW) | HIGH |
| FUNC-021 | On brew start: activate pump (K2) and solenoid (K3) | HIGH |
| FUNC-022 | On brew stop: deactivate pump, deactivate solenoid after configurable delay | HIGH |
| FUNC-023 | Log brew start/stop times | MEDIUM |
| FUNC-024 | Track shot duration, broadcast to ESP32 | MEDIUM |
| FUNC-025 | Pre-infusion support: configurable pump on/off pattern | LOW |

## 4.4 Water Management

| ID | Requirement | Priority |
|----|-------------|----------|
| FUNC-030 | Monitor water reservoir switch continuously | HIGH |
| FUNC-031 | Monitor tank level sensor continuously | HIGH |
| FUNC-032 | Water LED (K1) ON when reservoir present AND tank level OK | MEDIUM |
| FUNC-033 | Steam boiler level monitoring via comparator (GPIO4) | HIGH |
| FUNC-034 | Steam boiler auto-fill via fill solenoid and pump (see [Water_Management_Implementation.md](Water_Management_Implementation.md)) | HIGH |

## 4.5 Power Monitoring

| ID | Requirement | Priority |
|----|-------------|----------|
| FUNC-040 | Query PZEM-004T every 1000ms | MEDIUM |
| FUNC-041 | Read: Voltage, Current, Power, Energy, Frequency, PF | MEDIUM |
| FUNC-042 | Broadcast power data to ESP32 every 2000ms | MEDIUM |
| FUNC-043 | Detect power anomalies (voltage <90V or >260V) | LOW |

## 4.6 User Interface (On-Board)

| ID | Requirement | Priority |
|----|-------------|----------|
| FUNC-050 | Status LED patterns (see state machine) | MEDIUM |
| FUNC-051 | Buzzer: startup chirp (200Hz, 100ms) | LOW |
| FUNC-052 | Buzzer: brew complete tone (3 beeps) | LOW |
| FUNC-053 | Buzzer: error alarm (continuous until acknowledged) | HIGH |
| FUNC-054 | Relay indicator LEDs driven by GPIO (automatic with relay) | N/A |

---

# 5. Communication Protocol

> **See [Communication_Protocol.md](Communication_Protocol.md) for complete binary protocol specification.**

## 5.1 Protocol Overview

| Link | Transport | Baud | Format | Protocol |
|------|-----------|------|--------|----------|
| Pico ↔ ESP32 | UART0 | 921600 | 8N1 | Binary (unified status) |
| Pico ↔ PZEM | UART1 | 9600 | 8N1 | Modbus-RTU |

## 5.2 Message Types (Pico → ESP32)

| Message | Interval | Purpose |
|---------|----------|---------|
| MSG_STATUS (0x01) | 500ms | Unified status (temps, pressure, power, state) |
| MSG_ALARM (0x02) | On event | Alarm notifications (immediate, safety-critical) |
| MSG_BOOT (0x03) | Once | Boot complete |
| MSG_ACK (0x04) | On command | Command acknowledgment |

## 5.3 Message Types (ESP32 → Pico)

| Message | Purpose |
|---------|---------|
| MSG_CMD_SET_TEMP (0x10) | Set temperature setpoint |
| MSG_CMD_SET_PID (0x11) | Set PID parameters |
| MSG_CMD_BREW (0x13) | Start/stop brew |
| MSG_CMD_MODE (0x14) | Change operating mode |
| MSG_CMD_BOOTLOADER (0x1F) | Enter bootloader for OTA |

## 5.4 Protocol Requirements

| ID | Requirement | Priority |
|----|-------------|----------|
| COMM-001 | Binary protocol with CRC16 integrity check | HIGH |
| COMM-002 | State broadcast every 500ms ±50ms | HIGH |
| COMM-003 | Alarm messages sent immediately on event | HIGH |
| COMM-004 | Command acknowledgment within 100ms | MEDIUM |
| COMM-005 | Detect ESP32 disconnect after 5s of no communication | MEDIUM |
| COMM-006 | Continue standalone operation if ESP32 disconnected | HIGH |

---

# 6. State Machine

## 6.1 System States

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           SYSTEM STATE MACHINE                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│                              ┌─────────┐                                     │
│                              │  BOOT   │                                     │
│                              └────┬────┘                                     │
│                                   │                                          │
│                           Initialize HW                                      │
│                           Enable Watchdog                                    │
│                                   │                                          │
│                                   ▼                                          │
│                         ┌─────────────────┐                                  │
│                         │   SELF_TEST     │                                  │
│                         └────────┬────────┘                                  │
│                                  │                                           │
│               ┌──────────────────┼──────────────────┐                       │
│               │                  │                  │                        │
│               ▼                  ▼                  ▼                        │
│     ┌──────────────┐   ┌──────────────┐   ┌──────────────┐                  │
│     │  SAFE_MODE   │   │  HEATING     │   │   FAULT      │                  │
│     │ (No sensors) │   │  (Warm-up)   │   │  (Critical)  │                  │
│     └──────────────┘   └──────┬───────┘   └──────────────┘                  │
│                               │                    ▲                         │
│                       At temp │                    │ Critical error          │
│                               ▼                    │                         │
│                      ┌─────────────────┐           │                         │
│                      │     READY       │───────────┘                         │
│                      └────────┬────────┘                                     │
│                               │                                              │
│                    Brew switch│                                              │
│                               ▼                                              │
│                      ┌─────────────────┐                                     │
│                      │    BREWING      │                                     │
│                      └────────┬────────┘                                     │
│                               │                                              │
│                 Brew complete │                                              │
│                               ▼                                              │
│                      ┌─────────────────┐                                     │
│                      │   POST_BREW     │ ──► (2s solenoid delay)            │
│                      └────────┬────────┘                                     │
│                               │                                              │
│                               └──────────► READY                             │
│                                                                              │
│   Special States:                                                            │
│   ───────────────                                                            │
│   STANDBY  - Low power mode (heaters off, monitoring only)                  │
│   SERVICE  - Manual control mode (ESP32 connected in service mode)          │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

## 6.2 Status LED Patterns

| State | LED Pattern | Description |
|-------|-------------|-------------|
| BOOT | Solid ON | Initializing |
| SELF_TEST | Fast blink (4Hz) | Running diagnostics |
| HEATING | Slow pulse (1Hz) | Warming up |
| READY | Solid ON | At temperature, ready to brew |
| BREWING | Double blink | Active brew |
| POST_BREW | Slow blink (1Hz) | Post-brew delay |
| FAULT | Fast blink (2Hz) | Error condition |
| SAFE_MODE | Triple blink | Limited functionality |
| STANDBY | Off (brief flash every 5s) | Low power |

---

# 7. PID Control Requirements

## 7.1 PID Algorithm

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         PID CONTROL IMPLEMENTATION                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   Standard PID with anti-windup and derivative filtering:                   │
│                                                                              │
│   error = setpoint - temperature                                            │
│                                                                              │
│   P_term = Kp × error                                                        │
│   I_term = I_term + Ki × error × dt    (with clamping)                      │
│   D_term = Kd × (error - prev_error) / dt  (with low-pass filter)           │
│                                                                              │
│   output = P_term + I_term + D_term                                         │
│   output = clamp(output, 0.0, 1.0)     (duty cycle 0-100%)                  │
│                                                                              │
│   Anti-Windup:                                                               │
│   ─────────────                                                              │
│   If output is saturated (0 or 1), stop integrating in that direction.     │
│   This prevents integral term from growing unbounded during heat-up.        │
│                                                                              │
│   Derivative Filter:                                                         │
│   ──────────────────                                                         │
│   Apply first-order low-pass filter to derivative term:                     │
│   D_filtered = α × D_raw + (1-α) × D_prev,  where α = 0.1 to 0.3           │
│   This reduces noise amplification from temperature sensor noise.           │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

## 7.2 Default PID Parameters

| Boiler | Kp | Ki | Kd | Notes |
|--------|-----|-----|-----|-------|
| Brew | 2.0 | 0.05 | 1.0 | Start conservative, tune in machine |
| Steam | 3.0 | 0.08 | 1.5 | Higher gains due to larger thermal mass |

**Note:** These are starting points. PID tuning should be performed on the actual machine.

## 7.3 SSR Output

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| PWM Period | 1000ms | Matches zero-cross SSR cycle |
| Min Duty | 0% | Full off |
| Max Duty | 95% | Prevent thermal runaway, allow cooling |
| Resolution | 1% | 10ms increments |

---

# 8. Error Handling

## 8.1 Error Codes

| Code | Severity | Description | Action |
|------|----------|-------------|--------|
| E001 | CRITICAL | Watchdog timeout | Reset, enter FAULT |
| E002 | CRITICAL | Brew NTC open circuit | Disable SSR1, alert |
| E003 | CRITICAL | Brew NTC short circuit | Disable SSR1, alert |
| E004 | CRITICAL | Steam NTC open circuit | Disable SSR2, alert |
| E005 | CRITICAL | Steam NTC short circuit | Disable SSR2, alert |
| E006 | CRITICAL | Brew over-temperature | Disable SSR1, alert |
| E007 | CRITICAL | Steam over-temperature | Disable SSR2, alert |
| E008 | CRITICAL | Steam level low | Disable SSR2, alert |
| E009 | CRITICAL | No water reservoir | Disable pump & heaters |
| E010 | CRITICAL | Tank level low | Disable pump & heaters |
| E020 | WARNING | MAX31855 fault | Log, use NTC only |
| E021 | WARNING | PZEM communication timeout | Log, continue |
| E022 | WARNING | ESP32 communication timeout | Log, continue standalone |
| E030 | INFO | Brew started | Log |
| E031 | INFO | Brew completed | Log |

## 8.2 Error Recovery

| Severity | Recovery Procedure |
|----------|-------------------|
| CRITICAL | Enter FAULT state, disable outputs, require power cycle or explicit reset from ESP32 |
| WARNING | Log, continue operation with degraded capability |
| INFO | Log only |

---

# 9. Startup Sequence

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           STARTUP SEQUENCE                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   1. HARDWARE INITIALIZATION (before watchdog enabled)                      │
│      ────────────────────────────────────────────────                       │
│      a. Set all output GPIOs LOW, then configure as outputs                 │
│      b. Configure input GPIOs with pull-ups                                 │
│      c. Initialize ADC                                                      │
│      d. Initialize SPI (MAX31855)                                           │
│      e. Initialize UART0 (ESP32)                                            │
│      f. Initialize UART1 (PZEM)                                             │
│                                                                              │
│   2. ENABLE WATCHDOG (2000ms timeout)                                       │
│      ─────────────────────────────────                                      │
│      From this point, watchdog MUST be fed regularly!                       │
│                                                                              │
│   3. SELF-TEST                                                              │
│      ──────────                                                             │
│      a. Read all NTC sensors, verify in valid range                        │
│      b. Read MAX31855, check for faults                                     │
│      c. Read pressure sensor, verify 0 bar (not pressurized)               │
│      d. Read water level sensors                                            │
│      e. Test buzzer (brief chirp)                                           │
│      f. Flash status LED                                                    │
│                                                                              │
│   4. ENTER OPERATIONAL STATE                                                │
│      ───────────────────────────                                            │
│      If all sensors OK: HEATING state                                       │
│      If any sensor failed: SAFE_MODE or FAULT                              │
│                                                                              │
│   5. BROADCAST BOOT COMPLETE TO ESP32                                       │
│      ───────────────────────────────────                                    │
│      {"type": "boot", "data": {"version": "1.0.0", "state": "heating"}}    │
│                                                                              │
│   6. MAIN LOOP BEGINS                                                       │
│      ─────────────────                                                      │
│      • Core 0: Safety + Control loop                                        │
│      • Core 1: Communication loop                                           │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

# 10. Module Architecture

> **See [Architecture.md](Architecture.md) for detailed implementation including:**
> - Dual-core design (Core 0: control, Core 1: communication)
> - Complete file structure
> - Control loop implementation
> - State machine code
> - PID controller implementation
> - Build system configuration

## 10.1 Module Requirements

| Module | Responsibility | Priority |
|--------|----------------|----------|
| `safety/` | Watchdog, interlocks, safe state | CRITICAL |
| `sensors/` | NTC, thermocouple, pressure, levels | HIGH |
| `control/` | PID, boiler control, brew cycle | HIGH |
| `comms/` | ESP32 UART, PZEM, protocol | MEDIUM |
| `ui/` | LED patterns, buzzer | LOW |
| `state/` | State machine, shared data | HIGH |

## 10.2 Module Dependencies

```
┌─────────────────────────────────────────────────────────────────┐
│                     MODULE DEPENDENCY GRAPH                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│                        ┌──────────┐                             │
│                        │   main   │                             │
│                        └────┬─────┘                             │
│              ┌──────────────┼──────────────┐                    │
│              ▼              ▼              ▼                    │
│         ┌────────┐    ┌─────────┐    ┌─────────┐               │
│         │ safety │    │ control │    │  comms  │               │
│         └────┬───┘    └────┬────┘    └────┬────┘               │
│              │             │              │                     │
│              ▼             ▼              ▼                     │
│         ┌────────┐    ┌─────────┐    ┌─────────┐               │
│         │sensors │    │  state  │    │   ui    │               │
│         └────────┘    └─────────┘    └─────────┘               │
│                                                                  │
│   CRITICAL PATH: main → safety → sensors (checked every loop)  │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

# 11. Implementation Phases

## Phase 1: Safety Foundation (Week 1)
- [ ] Pin definitions and hardware initialization
- [ ] Watchdog timer implementation
- [ ] Safe state procedure
- [ ] All outputs default OFF

## Phase 2: Sensor Drivers (Week 2)
- [ ] NTC ADC reading with averaging
- [ ] Temperature conversion (Steinhart-Hart)
- [ ] MAX31855 SPI driver
- [ ] Pressure transducer reading
- [ ] Digital input debouncing

## Phase 3: Safety Interlocks (Week 2-3)
- [ ] Water reservoir interlock
- [ ] Tank level interlock
- [ ] Steam level interlock
- [ ] Over-temperature protection
- [ ] Sensor fault detection

## Phase 4: Basic Control (Week 3-4)
- [ ] PID controller class
- [ ] SSR PWM output
- [ ] Brew boiler control
- [ ] Steam boiler control
- [ ] State machine framework

## Phase 5: Brew Cycle (Week 4-5)
- [ ] Brew switch detection
- [ ] Pump/solenoid control
- [ ] Brew timer
- [ ] Post-brew delay

## Phase 6: Communication (Week 5-6)
- [ ] UART0 ESP32 protocol
- [ ] JSON message encoding
- [ ] Command processing
- [ ] State broadcast

## Phase 7: Power Monitoring (Week 6)
- [ ] PZEM-004T Modbus driver
- [ ] Power data reading
- [ ] Data broadcast

## Phase 8: User Interface (Week 7)
- [ ] Status LED patterns
- [ ] Buzzer tones
- [ ] Alarm handling

## Phase 9: Integration & Testing (Week 8+)
- [ ] Full system integration
- [ ] PID tuning
- [ ] Reliability testing
- [ ] Documentation

---

# Appendix A: NTC Lookup Table (Optional)

For faster temperature calculation, pre-compute lookup table:

| ADC Value (16-bit) | Voltage (V) | Resistance (Ω) | Temperature (°C) |
|--------------------|-------------|----------------|------------------|
| 48886 | 2.46 | 7618 | 0 |
| 45875 | 2.31 | 5638 | 10 |
| 42190 | 2.12 | 4229 | 20 |
| 37895 | 1.91 | 3171 | 30 |
| 33165 | 1.67 | 2367 | 40 |
| 28262 | 1.42 | 1753 | 50 |
| 23460 | 1.18 | 1286 | 60 |
| 19017 | 0.96 | 932 | 70 |
| 15106 | 0.76 | 669 | 80 |
| 11821 | 0.59 | 476 | 90 |
| 9147 | 0.46 | 337 | 100 |
| 7018 | 0.35 | 237 | 110 |
| 5363 | 0.27 | 166 | 120 |
| 4096 | 0.21 | 117 | 130 |

---

# Appendix B: PZEM-004T Modbus Commands

| Function | Register | Description |
|----------|----------|-------------|
| 0x04 | 0x0000-0x0001 | Voltage (0.1V units) |
| 0x04 | 0x0002-0x0003 | Current (0.001A units) |
| 0x04 | 0x0004-0x0005 | Power (0.1W units) |
| 0x04 | 0x0006-0x0007 | Energy (Wh units) |
| 0x04 | 0x0008 | Frequency (0.1Hz units) |
| 0x04 | 0x0009 | Power Factor (0.01 units) |
| 0x04 | 0x000A | Alarm status |

---

*End of Firmware Requirements Document*

