# Microcontroller & GPIO Allocation

## RP2354 Microcontroller Specifications

| Parameter         | Value                                          |
| ----------------- | ---------------------------------------------- |
| MCU               | RP2354 Dual-core ARM Cortex-M33 @ 150MHz       |
| Flash             | **2MB stacked flash** (internal to chip)       |
| SRAM              | 520KB                                          |
| GPIO              | **30 multi-function pins** (GPIO 0-29)         |
| ADC               | 4 channels, 12-bit, 500 ksps                   |
| UART              | 2× peripherals                                 |
| SPI               | 2× peripherals                                 |
| I2C               | 2× peripherals                                 |
| PWM               | 12× slices (24 channels)                       |
| PIO               | 3× programmable I/O blocks (12 state machines) |
| Operating Voltage | 1.8V - 5.5V (via VSYS), 3.3V logic             |
| Temperature Range | -20°C to +85°C                                 |
| Security          | ARM TrustZone, signed boot, 8KB OTP            |
| Package           | **QFN-60** (RP2354A) or QFN-80 (RP2354B)       |

## Complete GPIO Allocation

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                        RP2354 MICROCONTROLLER GPIO MAP                          │
│                          (30 GPIOs Available, 26 Allocated)                     │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐  │
│  │  ACTIVE ANALOG INPUTS (ADC)                                              │  │
│  │  ├── GPIO26 (ADC0) ─── Brew Boiler NTC (via voltage divider)            │  │
│  │  ├── GPIO27 (ADC1) ─── Steam Boiler NTC (via voltage divider)           │  │
│  │  └── GPIO28 (ADC2) ─── Pressure Transducer (scaled 0.5-4.5V → 0.34-3.06V) │  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐  │
│  │  EXPANSION GPIOs (Available for Future Features)                        │  │
│  │  ├── GPIO16 ─── **NOW AVAILABLE** (disconnected from J15, traces to SWD)│  │
│  │  ├── GPIO17 ─── SPARE (SPI0 CS)                                         │  │
│  │  ├── GPIO18 ─── SPARE (SPI0 SCK)                                        │  │
│  │  ├── GPIO22 ─── **NOW AVAILABLE** (disconnected from J15, traces to SWD)│  │
│  │  ├── GPIO23 ─── **NOW AVAILABLE** (was internal to Pico module)        │  │
│  │  ├── GPIO24 ─── **NOW AVAILABLE** (was internal to Pico module)        │  │
│  │  ├── GPIO25 ─── **NOW AVAILABLE** (was internal to Pico module)        │  │
│  │  └── GPIO29 ─── **ADC3 - 5V_MONITOR** (ratiometric pressure compensation)│  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐  │
│  │  DIGITAL INPUTS (Switches & Sensors)                                     │  │
│  │  ├── GPIO2 ─── Water Reservoir Switch (internal pull-up, active low)   │  │
│  │  ├── GPIO3 ─── Tank Level Sensor (internal pull-up, active low)        │  │
│  │  ├── GPIO4 ─── Steam Boiler Level (TLV3201 comparator out, active low) │  │
│  │  └── GPIO5 ─── Brew Handle Switch (internal pull-up, active low)       │  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐  │
│  │  DIGITAL OUTPUTS (Relay & SSR Drivers)                                   │  │
│  │  ├── GPIO10 ─── Mains Indicator Lamp Relay (K1) + Green Indicator LED   │  │
│  │  ├── GPIO11 ─── Pump Relay (K2) + Green Indicator LED                   │  │
│  │  ├── GPIO12 ─── Solenoid Relay (K3) + Green Indicator LED               │  │
│  │  ├── GPIO13 ─── Brew SSR Trigger (SSR1) + Orange Indicator LED          │  │
│  │  └── GPIO14 ─── Steam SSR Trigger (SSR2) + Orange Indicator LED         │  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐  │
│  │  UART0 - ESP32 DISPLAY MODULE (8-pin JST-XH)                            │  │
│  │  ├── GPIO0 (UART0 TX) ─── ESP32 RX (33Ω series + TVS - R40, D_UART_TX) │  │
│  │  ├── GPIO1 (UART0 RX) ─── ESP32 TX (33Ω series + TVS - R41, D_UART_RX) │  │
│  │  ├── RP2354 RUN ◄──────── ESP32 (ESP32 resets MCU, USB D- repurposed) │  │
│  │  ├── SWDIO (Dedicated Pin) ↔── ESP32 TX2 (J15 Pin 6, SWD interface)    │  │
│  │  ├── GPIO21 (WEIGHT_STOP) ◄─ ESP32 (J15 Pin 7, 4.7kΩ pull-down, USB D+ repurposed)│  │
│  │  └── SWCLK (Dedicated Pin) ↔── ESP32 RX2 (J15 Pin 8, SWD interface)     │  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐  │
│  │  USER INTERFACE                                                          │  │
│  │  ├── GPIO15 ─── Status LED (Green, active high, 330Ω series)           │  │
│  │  └── GPIO19 ─── Buzzer (Passive piezo, PWM output, 100Ω series)        │  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐  │
│  │  POWER METERING (Universal External via UART/RS485)                     │  │
│  │  ├── GPIO6 ─── METER TX (UART to meter RX / RS485 DI)                  │  │
│  │  ├── GPIO7 ─── METER RX (UART from meter TX / RS485 RO)                │  │
│  │  └── GPIO20 ── RS485 DE/RE (Direction control for MAX3485)             │  │
│  │  Supports: PZEM-004T, JSY-MK-163T/194T, Eastron SDM, and more          │  │
│  │  No HV measurement circuitry on PCB - J24 provides pass-through to meter│  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐  │
│  │  ESP32 CONTROL SIGNALS (J15 Pins 5-8) - All have 4.7kΩ pull-downs      │  │
│  │  ├── PICO RUN ← ESP32 GPIO20 (J15 Pin 5, USB D- repurposed)           │  │
│  │  ├── SWDIO (Dedicated Pin) ↔ ESP32 TX2 (J15 Pin 6, SWD interface)     │  │
│  │  ├── GPIO21 ── WEIGHT_STOP ← ESP32 GPIO19 (J15 Pin 7, USB D+ repurposed) │  │
│  │  └── SWCLK (Dedicated Pin) ↔ ESP32 RX2 (J15 Pin 8, SWD interface)     │  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐  │
│  │  HARDWARE CONTROL (Direct to Pico pins, not GPIO)                       │  │
│  │  └── RUN Pin ─── Reset Button (SMD tactile, to GND)                    │  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
│  GPIO UTILIZATION: 22/30 used (GPIO8,9,16,22,23,24,25 available)              │
│  ✅ GPIO8,9: NOW AVAILABLE (I2C accessory port removed)                      │
│  ✅ GPIO16,22: NOW AVAILABLE (disconnected from J15, traces to SWD)          │
│  ✅ GPIO23-25: NOW AVAILABLE (previously internal to Pico module)            │
│  ✅ GPIO29: ADC3 - 5V_MONITOR (ratiometric pressure compensation)            │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

## GPIO Summary Table

| GPIO   | Function                        | Direction | Type        | Pull          | Protection                                         |
| ------ | ------------------------------- | --------- | ----------- | ------------- | -------------------------------------------------- |
| 0      | UART0 TX → ESP32                | Output    | Digital     | None          | 1kΩ series + TVS (R40, D_UART_TX) - 5V tolerance protection |
| 1      | UART0 RX ← ESP32                | Input     | Digital     | None          | 1kΩ series + TVS (R41, D_UART_RX) - 5V tolerance protection |
| 2      | Water Reservoir Switch          | Input     | Digital     | Internal PU   | ESD clamp                                          |
| 3      | Tank Level Sensor               | Input     | Digital     | Internal PU   | ESD clamp                                          |
| 4      | Steam Boiler Level (Comparator) | Input     | Digital     | None          | TLV3201 output                                     |
| 5      | Brew Handle Switch              | Input     | Digital     | Internal PU   | ESD clamp                                          |
| 6      | Meter TX (UART1)                | Output    | Digital     | None          | 1kΩ series R44 (5V tolerance protection)           |
| 7      | Meter RX (UART1)                | Input     | Digital     | None          | 5V→3.3V divider (R45/R45A) + 33Ω R45B (ESD/ringing protection) |
| 8      | **Available**                   | **I/O**   | **Digital** | **None**      | **Unused GPIO**                                    |
| 9      | **Available**                   | **I/O**   | **Digital** | **None**      | **Unused GPIO**                                    |
| 10     | Relay K1 + LED                  | Output    | Digital     | None          | -                                                  |
| 11     | Relay K2 + LED                  | Output    | Digital     | None          | -                                                  |
| 12     | Relay K3 + LED                  | Output    | Digital     | None          | -                                                  |
| 13     | SSR1 Trigger + LED              | Output    | Digital     | None          | -                                                  |
| 14     | SSR2 Trigger + LED              | Output    | Digital     | None          | -                                                  |
| 15     | Status LED                      | Output    | Digital     | None          | -                                                  |
| 16     | **Available**                   | **I/O**   | **Digital** | **None**      | **Disconnected from J15 (traces moved to SWD)**    |
| 17     | SPI0 CS (Spare)                 | Output    | Digital     | None          | -                                                  |
| 18     | SPI0 SCK (Spare)                | Output    | Digital     | None          | -                                                  |
| 19     | Buzzer PWM                      | Output    | PWM         | None          | -                                                  |
| 20     | RS485 DE/RE                     | Output    | Digital     | Pull-down     | MAX3485 direction (TTL mode: NC)                   |
| 21     | WEIGHT_STOP (ESP32→RP2354)      | Input     | Digital     | Pull-down     | Brew-by-weight signal (J15 Pin 7)                  |
| 22     | **Available**                   | **I/O**   | **Digital** | **None**      | **Disconnected from J15 (traces moved to SWD)**    |
| **23** | **Available**                   | **I/O**   | **Digital** | **None**      | **Previously internal to Pico module**             |
| **24** | **Available**                   | **I/O**   | **Digital** | **None**      | **Previously internal to Pico module**             |
| **25** | **Available**                   | **I/O**   | **Digital** | **None**      | **Previously internal to Pico module**             |
| 26     | ADC0 - Brew NTC                 | Input     | Analog      | None          | RC filter                                          |
| 27     | ADC1 - Steam NTC                | Input     | Analog      | None          | RC filter                                          |
| 28     | ADC2 - Pressure                 | Input     | Analog      | None          | RC filter, divider                                 |
| **29** | **ADC3 - 5V_MONITOR**           | **Input** | **Analog**  | **None**      | **5V rail monitor (R91/R92 divider, ratiometric)** |

## RP2354 GPIO Considerations

### 5V Tolerance and Power Sequencing (CRITICAL)

The RP2354 datasheet highlights a critical constraint regarding "5V Tolerant" GPIOs: **IOVDD (3.3V) must be present for the tolerance to be active.**

**⚠️ RISK SCENARIO:**
If the BrewOS controller is powered via USB (VBUS), but an external peripheral (like the ESP32 module if self-powered from a separate USB cable) is powered from a different source that initializes faster than the Pico's onboard buck converter, a condition exists where V_GPIO = 5V and IOVDD = 0V.

**Failure Mechanism:**
In this state, the internal protection diodes are unpowered. The 5V input will forward-bias the parasitic diode to the 3.3V rail, effectively trying to power the entire Pico board through the GPIO pin. This usually results in immediate silicon latch-up or burnout.

**Protection Implemented:**
Series resistors (1kΩ) and TVS diodes are used on UART lines for 5V tolerance protection and ESD protection:

| GPIO | Signal   | Resistor | TVS Diode | Purpose                                                |
| ---- | -------- | -------- | --------- | ------------------------------------------------------ |
| 0    | UART0 TX | R40 1kΩ  | D_UART_TX | 5V tolerance protection, limits fault current to <500µA during power sequencing |
| 1    | UART0 RX | R41 1kΩ  | D_UART_RX | 5V tolerance protection, limits fault current to <500µA during power sequencing |
| 6    | Meter TX | R44 1kΩ  | -         | 5V tolerance protection for external meter interface  |
| 7    | Meter RX | R45/R45A divider + R45B 33Ω | - | 5V→3.3V level shifter (2.2kΩ/3.3kΩ) + series protection |

**⚠️ CRITICAL:** The RP2354 "5V Tolerant" GPIO feature requires IOVDD (3.3V) to be present. If an external peripheral (ESP32 via separate USB power) initializes before the Pico's buck converter, a condition exists where V_GPIO=5V and IOVDD=0V. In this state, internal protection diodes are unpowered, causing forward-bias through parasitic diodes to the 3.3V rail, resulting in silicon latch-up or burnout. The 1kΩ series resistors limit fault current to <500µA, providing safe margin even when IOVDD is absent.

**⚠️ IMPORTANT:** The RP2354 GPIOs are **NOT 5V tolerant** (and fail safely only if IOVDD is powered). All current limiting resistors must be kept in the layout to protect the chip from 5V peripherals.

**Safe Operating Procedure:**

1. Always power the control PCB BEFORE connecting USB to ESP32 module
2. Never hot-plug peripherals while system is running
3. Use the integrated 5V supply (J15 Pin 1) for ESP32, not separate USB power

### Previously Internal GPIOs (NOW AVAILABLE on RP2354)

With the migration to discrete RP2354 chip, these GPIOs are now accessible:

| GPIO   | Previous Function (Pico Module) | New Status (RP2354)                                       |
| ------ | ------------------------------- | --------------------------------------------------------- |
| GPIO23 | SMPS Power Save (internal)      | **Available** - Can be used for expansion                 |
| GPIO24 | VBUS Detect (internal)          | **Available** - Can be used for expansion                 |
| GPIO25 | Onboard LED (internal)          | **Available** - Can be used for expansion                 |
| GPIO29 | VSYS/3 ADC (internal)           | **ADC3 - 5V_MONITOR** - Ratiometric pressure compensation |

**✅ These GPIOs are now accessible on the RP2354 QFN package and can be routed to test points or expansion headers.**

### RP2354 E9 Errata (GPIO Input Latching)

The RP2354 has a documented errata (E9) where GPIO inputs can latch in a high state under certain conditions. **Mitigation is already implemented in this design:**

| Input GPIO | Function          | Protection                                      | Notes                   |
| ---------- | ----------------- | ----------------------------------------------- | ----------------------- |
| GPIO2-5    | Switches          | Internal pull-up + external pull-down (R10-R13) | Ensures defined state   |
| GPIO20-21  | RS485/WEIGHT_STOP | 4.7kΩ pull-down (R19/R73)                       | Prevents false triggers |
| GPIO16,22  | Available         | None                                            | Disconnected from J15   |

**All digital inputs have either internal pull-ups OR external pull-down resistors**, ensuring they cannot float and trigger the E9 errata condition.

### RP2354 ADC E9 Errata (ADC Leakage Current)

The RP2354 A2 stepping has a documented hardware issue (Erratum E9) affecting ADC-capable GPIO pins (GPIO 26-29).

**Mechanism:**
When voltage on an ADC pin exceeds ~2.0V or when the pin is in a high-impedance state, a parasitic leakage path activates within the pad driver circuitry. This leakage is non-linear and temperature-dependent.

**Impact on This Design:**

| ADC Pin | Function  | Source Impedance | Risk Level | Notes                            |
| ------- | --------- | ---------------- | ---------- | -------------------------------- |
| ADC0    | Brew NTC  | ~3.3kΩ (R1)      | Medium     | Leakage causes temperature error |
| ADC1    | Steam NTC | ~1.2kΩ (R2)      | Lower      | Lower impedance reduces impact   |
| ADC2    | Pressure  | ~3.8kΩ (R4‖R3)   | Medium     | Leakage causes pressure offset   |

**Quantitative Impact:**
For NTC circuits with R1=3.3kΩ, a few microamps of leakage induces several millivolts of offset. At brewing temperatures (93°C) where dR/dT is reduced, this can translate to >1°C temperature error.

**Mitigations (choose based on accuracy requirements):**

1. **Silicon Stepping:** Verify RP2354 is B0 stepping or later (may resolve E9)
2. **Firmware Calibration:** Implement temperature-dependent offset compensation
3. **External ADC (future revision):** Use dedicated ADC IC with voltage follower buffers
4. **Lower Source Impedance:** Consider 33Ω series + external buffer for critical channels

**Current Design Status:**
The RC filter capacitors (C8, C9, C11) suppress AC noise but do NOT mitigate DC leakage. For ±0.5°C espresso extraction accuracy, firmware calibration against a reference thermometer is **required** during commissioning.

### SWD Interface (Serial Wire Debug)

**New in v2.31:** The RP2354's SWD interface is routed to J15 connector for factory flash capability:

| Signal    | RP2354 Pin            | J15 Pin | ESP32 Signal  | Purpose       |
| --------- | --------------------- | ------- | ------------- | ------------- |
| **SWDIO** | **SWDIO (Dedicated)** | Pin 6   | **TX2**       | SWD data I/O  |
| **SWCLK** | **SWCLK (Dedicated)** | Pin 8   | **RX2**       | SWD clock     |
| **RUN**   | RUN                   | Pin 5   | Reset control | Reset control |

**Important Notes:**

- **SWDIO and SWCLK are dedicated physical pins** on the RP2354, NOT multiplexed on GPIO 16/22
- **GPIO 16 and 22 are now available** since J15 traces connect to the dedicated SWD pins instead
- **Series protection resistors** are required on SWDIO/SWCLK lines: R_SWDIO (47Ω) for SWDIO, R_SWCLK (22Ω optimized) for SWCLK

**Benefits:**

- **Factory Flash:** ESP32 can bit-bang SWD protocol to flash blank RP2354
- **Unbrickable Recovery:** Hardware-level access even if firmware is corrupted
- **Remote Debugging:** Enables GDB debugging via ESP32 if needed

**Note:** ESP32 firmware MUST support SWD flashing (DAP implementation) as this is the **only** method to program a blank chip.

## ESP32 USB Pin Repurposing

**Note:** ESP32-S3 USB data pins (D+ and D-) are repurposed as GPIO pins:

- **GPIO19 (USB D+)**: Used for WEIGHT_STOP signal (ESP32 → Pico GPIO21 via J15 Pin 7)
- **GPIO20 (USB D-)**: Used for Pico RUN pin control (ESP32 → Pico RUN via J15 Pin 5)

USB CDC (Serial over USB) is disabled to free these pins for GPIO functions. USB bootloader functionality remains available (separate from USB CDC).

For details on re-enabling USB CDC, see `docs/development/USB_CDC_Re-enable.md`.
