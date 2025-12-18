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
- Dashed line or hatched area boundary between HV and LV sections

---

## Connector Silkscreen Labels

### J1 - Mains Input (6.3mm Spade)

| Pin | Label  | Notes                                 |
| --- | ------ | ------------------------------------- |
| 1   | **L**  | Live, bold text                       |
| 2   | **N**  | Neutral                               |
| 3   | **PE** | Protective earth, use ground symbol ⏚ |

---

### J2-J4 - Relay Outputs (6.3mm Spade)

| Connector | Primary Label | Secondary Label | Current Rating |
| --------- | ------------- | --------------- | -------------- |
| J2        | K1            | LAMP            | 100mA          |
| J3        | K2            | PUMP            | 5A             |
| J4        | K3            | SOL             | 0.5A           |

---

### J26 - Unified Low-Voltage Terminal Block (18-pos)

Use dual-row labeling: pin numbers above, function labels below.

| Pin | Label          | Function                   | Notes                      |
| --- | -------------- | -------------------------- | -------------------------- |
| 1   | WATER_SW       | Water Reservoir Switch     | Digital input, active low  |
| 2   | WATER_SW_GND   | Water Switch Ground        | Switch return              |
| 3   | TANK_SW        | Tank Level Sensor          | Digital input, active low  |
| 4   | TANK_SW_GND    | Tank Switch Ground         | Sensor return              |
| 5   | STEAM_PROBE    | Steam Boiler Level Probe   | Single wire, ground via PE |
| 6   | BREW_SW        | Brew Handle Switch         | Digital input, active low  |
| 7   | BREW_SW_GND    | Brew Switch Ground         | Switch return              |
| 8   | BREW_TEMP      | Brew NTC Signal            | To ADC0                    |
| 9   | BREW_TEMP_GND  | Brew Temp Ground           | Sensor return              |
| 10  | STEAM_TEMP     | Steam NTC Signal           | To ADC1                    |
| 11  | STEAM_TEMP_GND | Steam Temp Ground          | Sensor return              |
| 12  | PRESS_5V       | Pressure Transducer Power  | Power supply               |
| 13  | PRESS_GND      | Pressure Transducer Ground | Sensor return              |
| 14  | PRESS_SIG      | Pressure Transducer Signal | 0.5-4.5V input             |
| 15  | BREW_SSR_5V    | Brew SSR Control Power     | Brew heater SSR power      |
| 16  | BREW_SSR_GND   | Brew SSR Control Ground    | Brew SSR trigger           |
| 17  | STEAM_SSR_5V   | Steam SSR Control Power    | Steam heater SSR power     |
| 18  | STEAM_SSR_GND  | Steam SSR Control Ground   | Steam SSR trigger          |

---

### J15 - ESP32 Display Module (8-pin JST-XH)

| Pin | Label | Function           |
| --- | ----- | ------------------ |
| 1   | 5V    | Power              |
| 2   | G     | Ground             |
| 3   | TX    | To ESP32 RX        |
| 4   | RX    | From ESP32 TX      |
| 5   | RST   | Pico reset control |
| 6   | SP1   | Spare 1            |
| 7   | WGT   | Weight stop signal |
| 8   | SP2   | Spare 2            |

**Pin 1 indicator:** Use filled square pad or dot

---

### J16 - Service/Debug Port (4-pin)

| Pin | Label |
| --- | ----- |
| 1   | 3V3   |
| 2   | G     |
| 3   | TX    |
| 4   | RX    |

**Warning label:** Add "⚠ UNPLUG J15 TO FLASH" near connector

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

### J23 - I2C Accessory Port (4-pin)

| Pin | Label |
| --- | ----- |
| 1   | 3V3   |
| 2   | G     |
| 3   | SDA   |
| 4   | SCL   |

---

### J24 - External Power Meter HV (3-pos Screw Terminal)

| Pin | Label |
| --- | ----- |
| 1   | L     |
| 2   | N     |
| 3   | PE    |

**CRITICAL:** Add high-voltage warning symbol (⚡) and "230VAC" label. Use ground symbol (⏚) on PE pin.

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

---

## Additional Silkscreen Elements

### Board Identification Block

Add text block with:

- "BrewOS Controller v1.1"
- "brewos.io"

### Test Points

Label critical test points for debugging:

| Designator | Label | Function       |
| ---------- | ----- | -------------- |
| TP1        | 3V3   | 3.3V rail      |
| TP2        | 5V    | 5V rail        |
| TP3        | GND   | Ground         |
| TP4        | VBUS  | USB voltage    |
| TP5        | BREW  | Brew temp ADC  |
| TP6        | STEAM | Steam temp ADC |
| TP7        | PRESS | Pressure ADC   |

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
- Use dashed line or hatched pattern as boundary
- Minimum 6mm creepage distance (label "CREEPAGE" in gap area)

**Low Voltage Zone:** J15, J16, J17, J23, J26

- Add "LOW VOLTAGE ZONE (5V/3.3V)" label

---

## Reference Designator Summary

| Designator | Type   | Pins | Primary Function     |
| ---------- | ------ | ---- | -------------------- |
| J1         | Spade  | 3    | Mains input          |
| J2         | Spade  | 1    | Relay K1 (Lamp)      |
| J3         | Spade  | 1    | Relay K2 (Pump)      |
| J4         | Spade  | 1    | Relay K3 (Solenoid)  |
| J15        | JST-XH | 8    | ESP32 display        |
| J16        | Header | 4    | Service/Debug UART   |
| J17        | JST-XH | 6    | Power meter          |
| J23        | Header | 4    | I2C accessories      |
| J24        | Screw  | 3    | Meter HV passthrough |
| J26        | Screw  | 18   | Unified LV terminal  |

---

## Manufacturing Notes

1. **Silkscreen color:** White on green or black solder mask for best contrast
2. **Minimum feature size:** 0.15mm line width, 0.8mm text height
3. **Layer:** Top silkscreen for component side, bottom for wiring diagrams if needed
4. **Avoid:** Silkscreen over pads, vias, or exposed copper
5. **Pin 1 markers:** Use both filled pad and silkscreen indicator for consistency
