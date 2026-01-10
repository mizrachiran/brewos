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

| Pin | Signal      | Direction        | Notes                                                               |
| --- | ----------- | ---------------- | ------------------------------------------------------------------- |
| 1   | +5V         | Power            | ESP32 power                                                         |
| 2   | GND         | Power            | Ground                                                              |
| 3   | RP2354_TX   | RP2354→ESP32     | GPIO0 via 1kΩ (R40, 5V tolerance protection) + TVS (D_UART_TX)      |
| 4   | RP2354_RX   | ESP32→RP2354     | GPIO1 via 1kΩ (R41, 5V tolerance protection) + TVS (D_UART_RX)      |
| 5   | RP2354_RUN  | ESP32→RP2354     | Reset control (via 1kΩ R_RUN_EXT - prevents short-circuit with SW1) |
| 6   | **SWDIO**   | **ESP32↔RP2354** | **RP2354 SWDIO Pin ↔ ESP32 TX2, 47Ω series (R_SWDIO) only**         |
| 7   | WEIGHT_STOP | ESP32→RP2354     | GPIO21, 4.7kΩ pull-down (R73, RP2350 E9 errata)                     |
| 8   | **SWCLK**   | **ESP32↔RP2354** | **RP2354 SWCLK Pin ↔ ESP32 RX2, 22Ω series (R_SWCLK) only**         |

**SWD Interface (v2.31):**
Pins 6 and 8 connect to the **dedicated SWDIO and SWCLK physical pins** on the RP2354 (NOT GPIO 16/22). This enables:

- Factory flash of blank RP2354 chips via ESP32
- Hardware-level recovery if firmware is corrupted
- Remote debugging capability (GDB via ESP32)

**Important:**

- GPIO 16 and 22 are now available for other uses since J15 traces connect to the dedicated SWD pins instead.
- **SWD lines use series resistors only** (R_SWDIO: 47Ω, R_SWCLK: 22Ω optimized) - NO pull-down resistors needed. The RP2350 E9 errata affects GPIO Input Buffer circuitry, NOT the dedicated Debug Port interface.
- **GPIO inputs (e.g., GPIO21/WEIGHT_STOP) require 4.7kΩ pull-down resistors** for E9 errata mitigation, but SWD lines do not.

**5V Tolerance & ESD Protection:**

- **D_UART_TX, D_UART_RX:** ESDALC6V1 TVS diodes placed near J15 connector
- Protects RP2354 GPIOs from static discharge during display installation/cleaning
- **Series resistors (R40, R41 = 1kΩ):** Critical for 5V tolerance protection
  - Limits fault current to <500µA during power sequencing anomalies
  - Required because RP2354 "5V tolerant" feature only works when IOVDD (3.3V) is present
  - If ESP32 powers up before PCB (IOVDD=0V), 1kΩ prevents latch-up/burnout

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

## USB-C Programming Port (J_USB)

USB-C connector for direct RP2354 programming and debugging via USB.

| Pin  | Signal | Notes                                                         |
| ---- | ------ | ------------------------------------------------------------- |
| VBUS | VBUS   | USB power (5V), connected to VSYS via D_VBUS (Schottky diode) |
| D+   | USB_DP | USB data positive, to RP2354 USB D+ pin                       |
| D-   | USB_DM | USB data negative, to RP2354 USB D- pin                       |
| GND  | GND    | USB ground                                                    |
| CC1  | CC1    | Configuration channel 1 (5.1kΩ pull-down)                     |
| CC2  | CC2    | Configuration channel 2 (5.1kΩ pull-down)                     |

**Features:**

- **Direct USB programming:** Connect USB-C cable to PC for firmware flashing via USB bootloader
- **USB CDC Serial:** Supports USB serial communication (if enabled in firmware)
- **Power input:** VBUS (5V) can power the board via VSYS (isolated from HLK module via D_VBUS)
- **CC resistors:** 5.1kΩ pull-downs on CC1/CC2 enable 5V power delivery

**⚠️ SAFETY WARNINGS:**

- **NEVER connect USB while mains-powered:** See [USB Debugging Safety](spec/09-Safety.md#usb-debugging-safety) for details
- **Floating ground risk:** USB shield is not hard-earthed (SRif architecture)
- **Power sequencing:** If using USB power, ensure IOVDD is present before applying signals to GPIOs
- **Physical safety:** USB port should be **covered or internal** (requiring panel removal to access) to discourage casual use while machine is mains-powered. This prevents ground loop hazards and floating ground risks.

**Protection:**

- **F_USB:** 1A PTC fuse protects against USB power faults (replaces undersized R_VBUS)
- **D_VBUS:** SS14 Schottky diode (1A rating) prevents backfeeding HLK module when USB is connected. **Critical:** Must be 1A rated for ESP32 load (355mA typical, 910mA peak)
- **D_USB_DP/D_USB_DM:** PESD5V0S1BL ESD protection diodes (**placed close to USB Connector J_USB** - shunts ESD spikes at entry point before they enter board, per Raspberry Pi best practices)
- **R_USB_DP/R_USB_DM:** 27Ω series termination resistors (**placed close to RP2354 USB pins** - required for USB impedance matching). Signal flow: Connector → ESD Diode → Trace → Termination Resistor → MCU.

**Part Number:** USB-C 2.0 connector (e.g., USB-C-016, Molex 47346-0001, or equivalent)

---

## Power Meter Interface (J17)

6-pin JST-XH for external power meter (PZEM-004T, JSY-MK, Eastron SDM, etc.).

| Pin | Signal     | Notes                                     |
| --- | ---------- | ----------------------------------------- |
| 1   | +3.3V      | Meter logic power                         |
| 2   | +5V        | Meter power (if needed)                   |
| 3   | GND        | Ground                                    |
| 4   | J17_RX     | From meter TX (via 5V→3.3V level shifter) |
| 5   | J17_TX     | To meter RX (via 1kΩ series protection)   |
| 6   | RS485_DERE | Direction control (GPIO20)                |

**Jumper Configuration:**

- **JP3:** OPEN = 5V TTL meters (uses voltage divider), CLOSED = 3.3V meters (bypasses divider)
- **JP4:** 1-2 = RS485 differential, 2-3 = TTL UART

**⚠️ CRITICAL - Level Shifter Protection:**

For 5V TTL meters (JP3 OPEN), J17 Pin 4 uses a **voltage divider** to scale 5V logic signals down to 3.3V before reaching GPIO7. This prevents damage to the RP2354, which is **NOT 5V tolerant** when IOVDD is unpowered.

**Divider Topology:**

```
J17 Pin 4 (5V input) ──► R45 (2.2kΩ) ──► J17_DIV ──► R45A (3.3kΩ) ──► GND
                                    │
                                    └──► R45B (33Ω) ──► GPIO7
```

- **Topology:** Input → R45 (upper, 2.2kΩ) → Divider Node → R45A (lower, 3.3kΩ) → GND
- **Output:** Divider Node → R45B (33Ω series) → GPIO7
- **5V Input:** V_out = 5V × R45A/(R45+R45A) = 5V × 3.3k/(2.2k+3.3k) = 3.0V ✓
- **3.3V Input:** When JP3 is CLOSED, the divider is bypassed for 3.3V logic meters
- **Series Protection:** R45B (33Ω) provides additional ESD/ringing protection after the divider

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
