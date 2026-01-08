# Connector Specifications

## High-Voltage Terminals (6.3mm Spade)

High-current connections to original machine wiring use 6.3mm (0.25") spade terminals for plug & play compatibility.

### Mains Input (J1)

| Pin  | Function      | Terminal   | Wire Gauge | Notes                |
| ---- | ------------- | ---------- | ---------- | -------------------- |
| J1-L | Mains Live    | 6.3mm male | 14 AWG     | Fused, to relay COMs |
| J1-N | Mains Neutral | 6.3mm male | 14 AWG     | Common neutral bus   |

**⚠️ NOTE: PE (Protective Earth) pin REMOVED from J1.**
HV section is now floating - no Earth connection on PCB to prevent L-to-Earth shorts.

### Relay Outputs (J2-J4)

| Pin   | Function | Terminal   | Wire Gauge | Notes               |
| ----- | -------- | ---------- | ---------- | ------------------- |
| J2-NO | K1 N.O.  | 6.3mm male | 16 AWG     | Mains lamp (≤100mA) |
| J3-NO | K2 N.O.  | 6.3mm male | 14 AWG     | Pump (5A peak)      |
| J4-NO | K3 N.O.  | 6.3mm male | 16 AWG     | Solenoid (~0.5A)    |

**Note:** Relay COMs are connected internally to the fused live bus - no external COM terminals needed.

---

## Unified Low-Voltage Terminal Block (J26)

**ALL low-voltage connections are consolidated into a single 18-position screw terminal block.**

### J26 Pinout

```
┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
│ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │ 8 │ 9 │10 │11 │12 │13 │14 │15 │16 │17 │18 │
│S1 │S1G│S2 │S2G│S3 │S4 │S4G│T1 │T1G│T2 │T2G│P5V│PGD│PSG│SR+│SR-│SR+│SR-│
└───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘
 │   │   │   │   │   │   │   │   │   │   │   │   │   │   │   │   │   │
 └─S1─┘   └─S2─┘  S3  └─S4─┘   └─T1──┘   └─T2──┘ └──Press──┘ └SSR1─┘ └SSR2─┘
```

### Complete Pin Assignment

| Pin | Label | Function                   | Wire   | Signal    | Notes                     |
| --- | ----- | -------------------------- | ------ | --------- | ------------------------- |
| 1   | S1    | Water Reservoir Switch     | 22 AWG | GPIO2     | Digital input, active low |
| 2   | S1-G  | Water Reservoir GND        | 22 AWG | GND       | Switch return             |
| 3   | S2    | Tank Level Sensor          | 22 AWG | GPIO3     | Digital input, active low |
| 4   | S2-G  | Tank Level GND             | 22 AWG | GND       | Sensor return             |
| 5   | S3    | Steam Boiler Level Probe   | 22 AWG | PROBE     | Via OPA342/TLV3201→GPIO4  |
| 6   | S4    | Brew Handle Switch         | 22 AWG | GPIO5     | Digital input, active low |
| 7   | S4-G  | Brew Handle GND            | 22 AWG | GND       | Switch return             |
| 8   | T1    | Brew NTC Signal            | 22 AWG | ADC0      | To GPIO26 via divider     |
| 9   | T1-G  | Brew NTC GND               | 22 AWG | GND       | Sensor return             |
| 10  | T2    | Steam NTC Signal           | 22 AWG | ADC1      | To GPIO27 via divider     |
| 11  | T2-G  | Steam NTC GND              | 22 AWG | GND       | Sensor return             |
| 12  | P-5V  | Pressure Transducer +5V    | 22 AWG | +5V       | Power for transducer      |
| 13  | P-GND | Pressure Transducer GND    | 22 AWG | GND       | Sensor return             |
| 14  | P-SIG | Pressure Transducer Signal | 22 AWG | PRESS_SIG | 0.5-4.5V input            |
| 15  | SSR1+ | SSR1 Control +5V           | 22 AWG | +5V       | Brew SSR power            |
| 16  | SSR1- | SSR1 Control -             | 22 AWG | SSR1_NEG  | Brew SSR trigger          |
| 17  | SSR2+ | SSR2 Control +5V           | 22 AWG | +5V       | Steam SSR power           |
| 18  | SSR2- | SSR2 Control -             | 22 AWG | SSR2_NEG  | Steam SSR trigger         |

### Wiring Notes

- **SWITCHES (Pin 1-7):** N.O. switches connect between signal and adjacent GND pin
- **S3 (Pin 5):** Level probe single wire, ground return via boiler body → J5 (SRif) → PCB GND
- **NTCs (Pin 8-11):** 2-wire thermistors, polarity doesn't matter
- **PRESSURE (Pin 12-14):** 3-wire transducer: +5V (red), GND (black), Signal (yellow/white)
- **SSRs (Pin 15-18):** Connect to SSR DC input terminals (+5V to SSR+, SSR- to SSR DC-)

**Part Number:** Phoenix Contact MKDS 1/18-5.08 (18-position, 5.08mm pitch)

---

## ESP32 Display Module (J15)

8-pin JST-XH connector for external ESP32 display module. **Updated in v2.31 for SWD support.**

| Pin | Signal      | Direction        | Notes                                                         |
| --- | ----------- | ---------------- | ------------------------------------------------------------- |
| 1   | +5V         | Power            | ESP32 power                                                   |
| 2   | GND         | Power            | Ground                                                        |
| 3   | RP2354_TX   | RP2354→ESP32     | GPIO0 via 33Ω (R40, ESD/ringing protection) + TVS (D_UART_TX) |
| 4   | RP2354_RX   | ESP32→RP2354     | GPIO1 via 33Ω (R41, ESD/ringing protection) + TVS (D_UART_RX) |
| 5   | RP2354_RUN  | ESP32→RP2354     | Reset control                                                 |
| 6   | **SWDIO**   | **ESP32↔RP2354** | **RP2354 SWDIO Pin ↔ ESP32 TX2, 47Ω series (R_SWDIO) only**   |
| 7   | WEIGHT_STOP | ESP32→RP2354     | GPIO21, 4.7kΩ pull-down (R73, RP2350 E9 errata)               |
| 8   | **SWCLK**   | **ESP32↔RP2354** | **RP2354 SWCLK Pin ↔ ESP32 RX2, 47Ω series (R_SWCLK) only**   |

**SWD Interface (v2.31):**
Pins 6 and 8 connect to the **dedicated SWDIO and SWCLK physical pins** on the RP2354 (NOT GPIO 16/22). This enables:

- Factory flash of blank RP2354 chips via ESP32
- Hardware-level recovery if firmware is corrupted
- Remote debugging capability (GDB via ESP32)

**Important:** 
- GPIO 16 and 22 are now available for other uses since J15 traces connect to the dedicated SWD pins instead.
- **SWD lines use 47Ω series resistors only** - NO pull-down resistors needed. The RP2350 E9 errata affects GPIO Input Buffer circuitry, NOT the dedicated Debug Port interface.
- **GPIO inputs (e.g., GPIO21/WEIGHT_STOP) require 4.7kΩ pull-down resistors** for E9 errata mitigation, but SWD lines do not.

**ESD Protection:**

- **D_UART_TX, D_UART_RX:** ESDALC6V1 TVS diodes placed near J15 connector
- Protects RP2354 GPIOs from static discharge during display installation/cleaning
- Series resistors (R40, R41) reduce ringing on cable (10-20cm length)

**Part Number:** JST B8B-XH-A (8-pin, 2.54mm pitch)

**⚠️ CRITICAL: ESP32 RF Subsystem Isolation**

The ESP32-S3 module **MUST** use an external antenna connected via u.FL/IPEX connector. The ECM Synchronika's stainless steel chassis acts as a Faraday cage, blocking Wi-Fi/Bluetooth signals if an onboard PCB trace antenna is used.

**Requirements:**

- ESP32 module must have u.FL/IPEX antenna connector (NOT PCB trace antenna)
- External antenna must be routed outside the machine chassis
- Antenna cable length: 10-20cm recommended (keep as short as possible)
- Antenna type: 2.4GHz/5GHz dual-band compatible (Wi-Fi 6 capable)

**Failure Mode:** If onboard antenna is used, Wi-Fi range will be <1 meter and Bluetooth pairing will fail, rendering the display module non-functional for remote access.

---

## Service/Debug Port (J16)

4-pin header for debug access. **Shares UART0 with J15 (ESP32).**

| Pin | Signal     | Notes                                |
| --- | ---------- | ------------------------------------ |
| 1   | +3.3V      | 3.3V power                           |
| 2   | GND        | Ground                               |
| 3   | SERVICE_TX | GPIO0 via 33Ω (R42, shared with J15) |
| 4   | SERVICE_RX | GPIO1 via 33Ω (R43, shared with J15) |

**⚠️ Disconnect ESP32 cable when using service port for flashing!**

**Protection:**

- R42/R43: 33Ω series resistors for ESD/ringing protection (shared with J15)
- D23/D24: BZT52C3V3 Zener clamps for 5V TTL adapter overvoltage safety

---

## Power Meter Interface (J17)

6-pin JST-XH for external power meter (PZEM-004T, JSY-MK, Eastron SDM, etc.).

| Pin | Signal     | Notes                             |
| --- | ---------- | --------------------------------- |
| 1   | +3.3V      | Meter logic power                 |
| 2   | +5V        | Meter power (if needed)           |
| 3   | GND        | Ground                            |
| 4   | J17_RX     | From meter TX (via level shifter) |
| 5   | J17_TX     | To meter RX                       |
| 6   | RS485_DERE | Direction control (GPIO20)        |

**Jumper Configuration:**

- **JP3:** OPEN = 5V TTL meters, CLOSED = 3.3V meters
- **JP4:** 1-2 = RS485 differential, 2-3 = TTL UART

---

## I2C Accessory Port (J23)

4-pin header for I2C accessories (OLED display, EEPROM, etc.).

| Pin | Signal   | Notes                    |
| --- | -------- | ------------------------ |
| 1   | +3.3V    | 3.3V power               |
| 2   | GND      | Ground                   |
| 3   | I2C0_SDA | GPIO8 with 4.7kΩ pull-up |
| 4   | I2C0_SCL | GPIO9 with 4.7kΩ pull-up |

---

## External Power Meter HV (J24)

2-position screw terminal for power meter mains pass-through.

| Pin | Signal  | Notes                |
| --- | ------- | -------------------- |
| 1   | L_FUSED | Fused live (from F1) |
| 2   | N       | Neutral              |

**⚠️ NOTE: PE (Protective Earth) pin REMOVED from J24.**
HV section is now floating - no Earth connection on PCB to prevent L-to-Earth shorts.

**⚠️ HIGH VOLTAGE - Connect to power meter only. No CT clamp through PCB.**

---

## Chassis Reference (SRif) - J5

Single 6.3mm male spade terminal for chassis reference connection.

| Pin | Function                 | Terminal   | Wire Gauge | Notes                              |
| --- | ------------------------ | ---------- | ---------- | ---------------------------------- |
| J5  | Chassis Reference (SRif) | 6.3mm male | 18 AWG     | Connect to PCB GND via C-R network |

**Circuit Implementation:**

J5 connects to PCB GND through a parallel C-R network to prevent DC ground loops while allowing AC signal return:

- **R_SRif:** 1MΩ resistor (blocks DC loop currents)
- **C_SRif:** 100nF capacitor (allows 1kHz AC signal return, low impedance at AC)

**Wiring Instructions:**

> Connect J5 (SRif) to a solid chassis screw or the boiler mounting bolt using 18AWG Green/Yellow wire. This is required for Level Probe operation and Logic Grounding. The return path for the level probe signal is: AC_OUT (U6) → J26-5 → PROBE → WATER → BOILER BODY → J5 (SRif) → C_SRif (AC path) / R_SRif (DC blocking) → PCB GND.

**Safety Note:** The C-R network prevents hard ground loops between the earthed chassis and the PCB logic ground, while maintaining the AC return path required for level probe operation.

**Part Number:** Keystone 1285 (6.3mm male spade terminal)
