# PCB Silkscreen Recommendations

Based on connector specifications from `06-Connectors.md`.

---

## General Guidelines

- **Font:** Use minimum 0.8mm (32mil) height for legibility after manufacturing
- **Line Width:** 0.15mm (6mil) minimum for text, 0.2mm for symbols
- **Clearance:** Keep silkscreen 0.2mm away from pads and vias
- **Orientation:** Place labels to read left-to-right or bottom-to-top

---

## High-Voltage Section

### Safety Markings (CRITICAL)

Add high-voltage warning symbols and text:

- "⚡ DANGER: HIGH VOLTAGE ⚡" label
- "230VAC - DISCONNECT BEFORE SERVICING" text
- IEC 60417-6042 symbol (lightning bolt in triangle) near J1, J2-J4, J24
- **Warning near HV connectors:** "NO PE CONNECTION - HV SECTION FLOATING"
- Dashed line or hatched area boundary between HV and LV sections

---

## Connector Silkscreen Labels

### J1 - Mains Input (6.3mm Spade, 2-pin)

| Pin | Label | Notes           |
| --- | ----- | --------------- |
| 1   | **L** | Live, bold text |
| 2   | **N** | Neutral         |

**⚠️ CRITICAL:** PE (Protective Earth) pin **REMOVED**. HV section is floating - no Earth connection on PCB.

---

### J2-J4 - Relay Outputs (6.3mm Spade)

| Connector | Primary Label | Secondary Label | Current Rating |
| --------- | ------------- | --------------- | -------------- |
| J2        | K1            | LAMP            | 100mA          |
| J3        | K2            | PUMP            | 5A             |
| J4        | K3            | SOL             | 0.5A           |

---

### J26 - Unified Low-Voltage Terminal Block (18-pos)

Use dual-row labeling: pin numbers above, function labels below. Use compact labels for silkscreen space efficiency.

| Pin | Label | Function                   | Notes                                    |
| --- | ----- | -------------------------- | ---------------------------------------- |
| 1   | S1    | Water Reservoir Switch     | Digital input, active low (GPIO2)        |
| 2   | S1-G  | Water Reservoir GND        | Switch return                            |
| 3   | S2    | Tank Level Sensor          | Digital input, active low (GPIO3)        |
| 4   | S2-G  | Tank Level GND             | Sensor return                            |
| 5   | S3    | Steam Boiler Level Probe   | Single wire, ground return via J5 (SRif) |
| 6   | S4    | Brew Handle Switch         | Digital input, active low (GPIO5)        |
| 7   | S4-G  | Brew Handle GND            | Switch return                            |
| 8   | T1    | Brew NTC Signal            | To ADC0 (GPIO26)                         |
| 9   | T1-G  | Brew NTC GND               | Sensor return                            |
| 10  | T2    | Steam NTC Signal           | To ADC1 (GPIO27)                         |
| 11  | T2-G  | Steam NTC GND              | Sensor return                            |
| 12  | P-5V  | Pressure Transducer +5V    | Power for transducer                     |
| 13  | P-GND | Pressure Transducer GND    | Sensor return                            |
| 14  | P-SIG | Pressure Transducer Signal | 0.5-4.5V input (GPIO28)                  |
| 15  | SSR1+ | SSR1 Control +5V           | Brew SSR power                           |
| 16  | SSR1- | SSR1 Control -             | Brew SSR trigger                         |
| 17  | SSR2+ | SSR2 Control +5V           | Steam SSR power                          |
| 18  | SSR2- | SSR2 Control -             | Steam SSR trigger                        |

**Visual Reference (as printed on PCB):**

```
┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
│ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │ 8 │ 9 │10 │11 │12 │13 │14 │15 │16 │17 │18 │
│S1 │S1G│S2 │S2G│S3 │S4 │S4G│T1 │T1G│T2 │T2G│P5V│PGD│PSG│SR+│SR-│SR+│SR-│
└───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘
 │   │   │   │   │   │   │   │   │   │   │   │   │   │   │   │   │   │
 └─S1─┘   └─S2─┘  S3  └─S4─┘   └─T1──┘   └─T2──┘ └──Press──┘ └SSR1─┘ └SSR2─┘
```

---

### J15 - ESP32 Display Module (8-pin JST-XH)

| Pin | Label     | Function             | Notes                                      |
| --- | --------- | -------------------- | ------------------------------------------ |
| 1   | 5V        | Power                | ESP32 power supply                         |
| 2   | G         | Ground               | Common ground                              |
| 3   | TX        | RP2354 TX → ESP32 RX | GPIO0, protected (1kΩ R40 + TVS D_UART_TX) |
| 4   | RX        | ESP32 TX → RP2354 RX | GPIO1, protected (1kΩ R41 + TVS D_UART_RX) |
| 5   | RUN       | RP2354 Reset Control | ESP32 → RP2354 RUN pin (via 1kΩ R_RUN_EXT) |
| 6   | **SWDIO** | **SWD Data I/O**     | **RP2354 SWDIO ↔ ESP32 TX2 (47Ω R_SWDIO)** |
| 7   | WGT       | Weight Stop Signal   | ESP32 → RP2354 GPIO21 (4.7kΩ R73 pull-down)|
| 8   | **SWCLK** | **SWD Clock**        | **RP2354 SWCLK ↔ ESP32 RX2 (22Ω R_SWCLK)** |

**Pin 1 indicator:** Use filled square pad or dot

**⚠️ IMPORTANT:** Pins 6 and 8 are SWD interface (Serial Wire Debug) for factory flash and recovery. GPIO 16 and 22 are now available for other uses.

**Note on 1kΩ series resistors (R40, R41):** These provide 5V tolerance protection by limiting fault current to <500µA during power sequencing anomalies. Required because RP2354 "5V tolerant" feature only works when IOVDD (3.3V) is present.

---

### J17 - Power Meter Interface (6-pin JST-XH)

| Pin | Label |
| --- | ----- |
| 1   | 3V3   |
| 2   | 5V    |
| 3   | G     |
| 4   | RX    |
| 5   | TX    |
| 6   | DE    |

---


### J5 - Chassis Reference (SRif) - 6.3mm Spade

| Pin | Label | Notes                                         |
| --- | ----- | --------------------------------------------- |
| J5  | SRif  | Chassis Reference (connect to boiler/chassis) |

**Silkscreen Labeling:**

- Label as "J5" or "SRif" near connector
- Add note: "Connect to chassis/boiler bolt (18AWG Green/Yellow wire)"
- Use ground symbol (⏚) or "CHASSIS" label
- **CRITICAL:** This is NOT Protective Earth - it's a chassis reference for level probe return path

**Wiring Note:** This connector provides the return path for the level probe signal. Connect to a solid chassis screw or boiler mounting bolt using 18AWG Green/Yellow wire.

---

### J24 - External Power Meter HV (2-pos Screw Terminal)

| Pin | Label | Notes                |
| --- | ----- | -------------------- |
| 1   | L     | Fused live (from F1) |
| 2   | N     | Neutral              |

**⚠️ CRITICAL:** PE (Protective Earth) pin **REMOVED**. HV section is floating - no Earth connection on PCB.

**Safety Markings:** Add high-voltage warning symbol (⚡) and "230VAC" label near connector.

---

### J_USB - USB-C Programming Port

| Label | Notes                                           |
| ----- | ----------------------------------------------- |
| USB   | USB-C connector for programming                 |

**Silkscreen Markings:**

- Label as "J_USB" or "USB" near connector
- Add warning symbol: "⚠️ MAINS OFF"
- Consider adding: "DISCONNECT MAINS BEFORE USE"

**⚠️ SAFETY:** USB port should be **covered or internal** (requiring panel removal) to discourage casual use while machine is mains-powered. This prevents ground loop hazards.

---

## Solder Jumpers

Label all solder jumpers with designator, function, and position indicators.

### JP1 - Brew NTC Selection (2-pad)

| Position | Label | Function                   |
| -------- | ----- | -------------------------- |
| OPEN     | 50kΩ  | ECM/Profitec NTC (default) |
| CLOSED   | 10kΩ  | Rocket/Gaggia NTC          |

**Silkscreen:**

```
JP1
[ ]─[ ]  OPEN = 50kΩ
[█]─[█]  CLOSED = 10kΩ
```

### JP2 - Steam NTC Selection (2-pad)

| Position | Label | Function                   |
| -------- | ----- | -------------------------- |
| OPEN     | 50kΩ  | ECM/Profitec NTC (default) |
| CLOSED   | 10kΩ  | Rocket/Gaggia NTC          |

**Silkscreen:**

```
JP2
[ ]─[ ]  OPEN = 50kΩ
[█]─[█]  CLOSED = 10kΩ
```

### JP3 - Power Meter RX Level (2-pad)

| Position | Label | Function                |
| -------- | ----- | ----------------------- |
| OPEN     | 5V    | 5V TTL meters (default) |
| CLOSED   | 3.3V  | 3.3V logic meters       |

**Silkscreen:**

```
JP3
[ ]─[ ]  OPEN = 5V
[█]─[█]  CLOSED = 3.3V
```

### JP4 - Power Meter Interface (3-pad)

| Position | Label | Function                     |
| -------- | ----- | ---------------------------- |
| 1-2      | RS485 | RS485 differential (default) |
| 2-3      | TTL   | TTL UART                     |

**Silkscreen:**

```
JP4
[1]─[2]─[3]
│   │   │
RS485 TTL
(default)
```

**Position indicators:** Label pads 1, 2, 3 clearly. Show default position (1-2).

### JP5 - GPIO7 Input Source Selection (3-pad)

| Position | Label | Function                       |
| -------- | ----- | ------------------------------ |
| 1-2      | RS485 | MAX3485 RO → GPIO7 (default)   |
| 2-3      | TTL   | J17_DIV (External) → GPIO7     |

**Silkscreen:**

```
JP5
[1]─[2]─[3]
│   │   │
RS485 TTL
(default)
```

**Purpose:** Hardware isolation for GPIO7 input source. Prevents bus contention if firmware crashes - physically selects either RS485 receiver or external TTL meter input.

**⚠️ CRITICAL:** Must match JP4 setting. If JP4 is set to RS485 mode, JP5 must also be 1-2. If JP4 is set to TTL mode, JP5 must be 2-3.

---

## Additional Silkscreen Elements

### Board Identification Block

Add text block with:

- "BrewOS Controller v2.31"
- "brewos.io"
- "RP2354 MCU"

### Test Points

Label critical test points for debugging:

| Designator | Label     | Function                         | Expected Value | Notes                        |
| ---------- | --------- | -------------------------------- | -------------- | ---------------------------- |
| TP1        | GND       | Ground reference                 | 0V             | Near RP2354                  |
| TP2        | 5V        | Main power rail                  | 5.00V ±5%      | Near HLK output              |
| TP3        | 3.3V      | Logic power rail                 | 3.30V ±3%      | Near TPS563200 output        |
| TP4        | 1V1       | Core voltage (DVDD)              | 1.10V ±3%      | Near external LDO (U4)       |
| TP5        | ADC_VREF  | ADC Reference (3.0V buffered)    | 3.00V ±0.5%    | Critical for calibration     |
| TP6        | 5V_MON    | 5V Monitor (ratiometric)         | ~1.79V         | Near R100/R101 divider       |
| TP7        | ADC0      | Brew NTC Signal (GPIO26)         | 0.5-2.5V       | Brew temp ADC                |
| TP8        | ADC1      | Steam NTC Signal (GPIO27)        | 0.5-2.5V       | Steam temp ADC               |
| TP9        | ADC2      | Pressure Signal (GPIO28)         | 0.32-2.88V     | Pressure ADC                 |
| TP10       | UART0_TX  | RP2354 TX (GPIO0)                | 3.3V idle      | Near J15                     |
| TP11       | UART0_RX  | RP2354 RX (GPIO1)                | 3.3V idle      | Near J15                     |
| TP12       | RS485_DE  | RS485 Direction Control (GPIO20) | 0V/3.3V        | Near U8 (MAX3485)            |

**Notes:**

- Power rail test points (TP1-TP4) should be clearly labeled near power connectors for easy access during testing
- TP5 (ADC_VREF) is critical for temperature calibration - verify 3.00V before commissioning
- TP6 (5V_MON) used for ratiometric pressure compensation - verify ratio matches R100/R101 divider

### Polarity Indicators

For power-carrying connectors, use:

- **+** symbol for positive terminals
- **−** or **G** for ground/negative
- Small **diode symbol** near reverse-polarity-sensitive inputs

### Fiducials

Label fiducial markers: FID1, FID2, FID3

---

## Voltage Zone Demarcation

Create clear visual separation between high and low voltage areas:

**High Voltage Zone:** J1, J2, J3, J4, J24

- Add "⚡ HIGH VOLTAGE ZONE ⚡" label
- Add warning: "NO PE CONNECTION - HV SECTION FLOATING"
- Use dashed line or hatched pattern as boundary
- Minimum 6mm creepage distance (label "CREEPAGE" or "MILLED SLOT" in gap area)
- **Milled slot** must be visible on silkscreen (physical cutout between HV and LV sections)

**Low Voltage Zone:** J5, J15, J17, J26, J_USB

- Add "LOW VOLTAGE ZONE (5V/3.3V)" label
- Add note near J5: "SRif - Chassis Reference (NOT Protective Earth)"
- Add warning near J_USB: "⚠️ MAINS OFF BEFORE USB"

---

## Reference Designator Summary

| Designator | Type      | Pins  | Primary Function             | Notes                              |
| ---------- | --------- | ----- | ---------------------------- | ---------------------------------- |
| J1         | Spade     | **2** | Mains input (L, N)           | **PE removed - HV floating**       |
| J2         | Spade     | 1     | Relay K1 (Lamp)              | Mains indicator lamp               |
| J3         | Spade     | 1     | Relay K2 (Pump)              | Vibration pump                     |
| J4         | Spade     | 1     | Relay K3 (Solenoid)          | Solenoid valve                     |
| **J5**     | **Spade** | **1** | **Chassis Reference (SRif)** | **Connect to chassis/boiler bolt** |
| J15        | JST-XH    | 8     | ESP32 display + SWD          | SWD on Pins 6/8 (factory flash)    |
| J17        | JST-XH    | 6     | Power meter interface        | TTL/RS485 selectable (JP4/JP5)     |
| J24        | Screw     | **2** | Meter HV passthrough (L, N)  | **PE removed - HV floating**       |
| J26        | Screw     | 18    | Unified LV terminal          | All sensors + SSR control          |
| J_USB      | USB-C     | -     | USB programming port         | ⚠️ Disconnect mains before use     |

**Removed Connectors (v2.31):**

| Designator | Previous Function | Reason for Removal                           |
| ---------- | ----------------- | -------------------------------------------- |
| J16        | Service/Debug     | Redundant - UART0 accessible via J15         |
| J20        | Pico 2 Socket     | Removed - using discrete RP2354 chip         |
| J23        | I2C Accessory     | Unused - GPIO8/9 available for future expansion |

---

## Manufacturing Notes

1. **Silkscreen color:** White on green or black solder mask for best contrast
2. **Minimum feature size:** 0.15mm line width, 0.8mm text height
3. **Layer:** Top silkscreen for component side, bottom for wiring diagrams if needed
4. **Avoid:** Silkscreen over pads, vias, or exposed copper
5. **Pin 1 markers:** Use both filled pad and silkscreen indicator for consistency
