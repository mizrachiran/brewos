# Microcontroller & GPIO Allocation

## Raspberry Pi Pico 2 Specifications

| Parameter         | Value                                          |
| ----------------- | ---------------------------------------------- |
| MCU               | RP2350 Dual-core ARM Cortex-M33 @ 150MHz       |
| Flash             | 4MB onboard QSPI                               |
| SRAM              | 520KB                                          |
| GPIO              | 26 multi-function pins                         |
| ADC               | 4 channels, 12-bit, 500 ksps                   |
| UART              | 2× peripherals                                 |
| SPI               | 2× peripherals                                 |
| I2C               | 2× peripherals                                 |
| PWM               | 12× slices (24 channels)                       |
| PIO               | 3× programmable I/O blocks (12 state machines) |
| Operating Voltage | 1.8V - 5.5V (via VSYS), 3.3V logic             |
| Temperature Range | -20°C to +85°C                                 |
| Security          | ARM TrustZone, signed boot, 8KB OTP            |

## Complete GPIO Allocation

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                        RASPBERRY PI PICO 2 GPIO MAP                             │
│                          (All 26 GPIOs Allocated)                               │
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
│  │  ├── GPIO17 ─── SPARE (SPI0 CS)                                         │  │
│  │  └── GPIO18 ─── SPARE (SPI0 SCK)                                        │  │
│  │  Note: GPIO16 is now SPARE1 (J15 Pin 6), see ESP32 CONTROL SIGNALS      │  │
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
│  │  ├── GPIO0 (UART0 TX) ─── ESP32 RX (1kΩ series protection - R40)       │  │
│  │  ├── GPIO1 (UART0 RX) ─── ESP32 TX (1kΩ series protection - R41)       │  │
│  │  ├── PICO RUN ◄──────── ESP32 GPIO8 (ESP32 resets Pico)                │  │
│  │  ├── GPIO16 (SPARE1) ↔── ESP32 GPIO9 (J15 Pin 6, 4.7kΩ pull-down)     │  │
│  │  ├── GPIO21 (WEIGHT_STOP) ◄─ ESP32 GPIO10 (J15 Pin 7, 4.7kΩ pull-down)│  │
│  │  └── GPIO22 (SPARE2) ↔── ESP32 GPIO22 (J15 Pin 8, 4.7kΩ pull-down)    │  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐  │
│  │  SERVICE/DEBUG PORT (4-pin header) - Shared with ESP32 on GPIO0/1       │  │
│  │  ├── GPIO0 (UART0 TX) ─── J15 (ESP32) + J16 (Service) - 1kΩ protected  │  │
│  │  └── GPIO1 (UART0 RX) ─── J15 (ESP32) + J16 (Service) - 1kΩ protected  │  │
│  │  ⚠️ Disconnect ESP32 cable when using service port for flashing         │  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐  │
│  │  I2C0 - ACCESSORY PORT (4-pin header)                                   │  │
│  │  ├── GPIO8 (I2C0 SDA) ─── Accessory data (4.7kΩ pull-up)               │  │
│  │  └── GPIO9 (I2C0 SCL) ─── Accessory clock (4.7kΩ pull-up)              │  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
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
│  │  ESP32 CONTROL SIGNALS (J15 Pins 6-8) - All have 4.7kΩ pull-downs      │  │
│  │  ├── GPIO16 ── SPARE1 ↔ ESP32 GPIO9 (J15 Pin 6)                       │  │
│  │  ├── GPIO21 ── WEIGHT_STOP ← ESP32 GPIO10 (J15 Pin 7)                 │  │
│  │  └── GPIO22 ── SPARE2 ↔ ESP32 GPIO22 (J15 Pin 8)                      │  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
│  ┌─────────────────────────────────────────────────────────────────────────┐  │
│  │  HARDWARE CONTROL (Direct to Pico pins, not GPIO)                       │  │
│  │  └── RUN Pin ─── Reset Button (SMD tactile, to GND)                    │  │
│  └─────────────────────────────────────────────────────────────────────────┘  │
│                                                                                 │
│  GPIO UTILIZATION: 25/26 available (GPIO22 spare for expansion)               │
│  ⚠️ GPIO23-25, GPIO29 are INTERNAL to Pico 2 module - NOT on header!         │
│                                                                                 │
└────────────────────────────────────────────────────────────────────────────────┘
```

## GPIO Summary Table

| GPIO | Function                        | Direction | Type    | Pull          | Protection                        |
| ---- | ------------------------------- | --------- | ------- | ------------- | --------------------------------- |
| 0    | UART0 TX → ESP32                | Output    | Digital | None          | 1kΩ series (R40)                  |
| 1    | UART0 RX ← ESP32                | Input     | Digital | None          | 1kΩ series (R41)                  |
| 2    | Water Reservoir Switch          | Input     | Digital | Internal PU   | ESD clamp                         |
| 3    | Tank Level Sensor               | Input     | Digital | Internal PU   | ESD clamp                         |
| 4    | Steam Boiler Level (Comparator) | Input     | Digital | None          | TLV3201 output                    |
| 5    | Brew Handle Switch              | Input     | Digital | Internal PU   | ESD clamp                         |
| 6    | Meter TX (UART1)                | Output    | Digital | None          | 1kΩ series R42 (5V tolerance)     |
| 7    | Meter RX (UART1)                | Input     | Digital | None          | 1kΩ series R43 (5V tolerance)     |
| 8    | I2C0 SDA (Accessory)            | I/O       | Digital | 4.7kΩ ext. PU | Accessory expansion               |
| 9    | I2C0 SCL (Accessory)            | Output    | Digital | 4.7kΩ ext. PU | Accessory expansion               |
| 10   | Relay K1 + LED                  | Output    | Digital | None          | -                                 |
| 11   | Relay K2 + LED                  | Output    | Digital | None          | -                                 |
| 12   | Relay K3 + LED                  | Output    | Digital | None          | -                                 |
| 13   | SSR1 Trigger + LED              | Output    | Digital | None          | -                                 |
| 14   | SSR2 Trigger + LED              | Output    | Digital | None          | -                                 |
| 15   | Status LED                      | Output    | Digital | None          | -                                 |
| 16   | SPARE1 ↔ ESP32 GPIO9            | I/O       | Digital | Pull-down     | J15 Pin 6, 4.7kΩ (R74)            |
| 17   | SPI0 CS (Spare)                 | Output    | Digital | None          | -                                 |
| 18   | SPI0 SCK (Spare)                | Output    | Digital | None          | -                                 |
| 19   | Buzzer PWM                      | Output    | PWM     | None          | -                                 |
| 20   | RS485 DE/RE                     | Output    | Digital | Pull-down     | MAX3485 direction (TTL mode: NC)  |
| 21   | WEIGHT_STOP (ESP32→Pico)        | Input     | Digital | Pull-down     | Brew-by-weight signal (J15 Pin 7) |
| 22   | SPARE2 ↔ ESP32 GPIO22           | I/O       | Digital | Pull-down     | J15 Pin 8, 4.7kΩ (R75)            |
| 26   | ADC0 - Brew NTC                 | Input     | Analog  | None          | RC filter                         |
| 27   | ADC1 - Steam NTC                | Input     | Analog  | None          | RC filter                         |
| 28   | ADC2 - Pressure                 | Input     | Analog  | None          | RC filter, divider                |

## RP2350 GPIO Considerations

### 5V Tolerance and Power Sequencing (CRITICAL)

The RP2350 datasheet highlights a critical constraint regarding "5V Tolerant" GPIOs: **IOVDD (3.3V) must be present for the tolerance to be active.**

**⚠️ RISK SCENARIO:**
If the BrewOS controller is powered via USB (VBUS), but an external peripheral (like the ESP32 module if self-powered from a separate USB cable) is powered from a different source that initializes faster than the Pico's onboard buck converter, a condition exists where V_GPIO = 5V and IOVDD = 0V.

**Failure Mechanism:**
In this state, the internal protection diodes are unpowered. The 5V input will forward-bias the parasitic diode to the 3.3V rail, effectively trying to power the entire Pico board through the GPIO pin. This usually results in immediate silicon latch-up or burnout.

**Protection Implemented:**
Series current-limiting resistors (1kΩ) are mandatory on all GPIO lines that may interface with 5V-capable peripherals:

| GPIO | Signal   | Resistor | Purpose                                                |
| ---- | -------- | -------- | ------------------------------------------------------ |
| 0    | UART0 TX | R40 1kΩ  | Limits fault current to <500µA during power sequencing |
| 1    | UART0 RX | R41 1kΩ  | Limits fault current to <500µA during power sequencing |
| 6    | Meter TX | R42 1kΩ  | Protection for external meter interface                |
| 7    | Meter RX | R43 1kΩ  | Protection for external meter interface                |

**Safe Operating Procedure:**

1. Always power the control PCB BEFORE connecting USB to ESP32 module
2. Never hot-plug peripherals while system is running
3. Use the integrated 5V supply (J15 Pin 1) for ESP32, not separate USB power

### Internal GPIOs (NOT available on Pico 2 header)

| GPIO   | Internal Function | Notes                              |
| ------ | ----------------- | ---------------------------------- |
| GPIO23 | SMPS Power Save   | Controls regulator efficiency mode |
| GPIO24 | VBUS Detect       | High when USB connected            |
| GPIO25 | Onboard LED       | Green LED on Pico 2 module         |
| GPIO29 | VSYS/3 ADC        | Internal voltage monitoring        |

**⚠️ These GPIOs are NOT exposed on the 40-pin header - do not attempt to use them!**

### RP2350 E9 Errata (GPIO Input Latching)

The RP2350 has a documented errata (E9) where GPIO inputs can latch in a high state under certain conditions. **Mitigation is already implemented in this design:**

| Input GPIO | Function          | Protection                                      | Notes                   |
| ---------- | ----------------- | ----------------------------------------------- | ----------------------- |
| GPIO2-5    | Switches          | Internal pull-up + external pull-down (R10-R13) | Ensures defined state   |
| GPIO16     | SPARE1            | 4.7kΩ pull-down (R74)                           | ESP32↔Pico spare I/O    |
| GPIO20-21  | RS485/WEIGHT_STOP | 4.7kΩ pull-down (R73)                           | Prevents false triggers |
| GPIO22     | SPARE2            | 4.7kΩ pull-down (R75)                           | ESP32↔Pico spare I/O    |

**All digital inputs have either internal pull-ups OR external pull-down resistors**, ensuring they cannot float and trigger the E9 errata condition.

### RP2350 ADC E9 Errata (ADC Leakage Current)

The RP2350 A2 stepping has a documented hardware issue (Erratum E9) affecting ADC-capable GPIO pins (GPIO 26-29).

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

1. **Silicon Stepping:** Verify RP2350 is B0 stepping or later (may resolve E9)
2. **Firmware Calibration:** Implement temperature-dependent offset compensation
3. **External ADC (future revision):** Use dedicated ADC IC with voltage follower buffers
4. **Lower Source Impedance:** Consider 1kΩ series + external buffer for critical channels

**Current Design Status:**
The RC filter capacitors (C8, C9, C11) suppress AC noise but do NOT mitigate DC leakage. For ±0.5°C espresso extraction accuracy, firmware calibration against a reference thermometer is **required** during commissioning.
