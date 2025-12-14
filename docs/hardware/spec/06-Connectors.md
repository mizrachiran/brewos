# Connector Specifications

## High-Voltage Terminals (6.3mm Spade)

High-current connections to original machine wiring use 6.3mm (0.25") spade terminals for plug & play compatibility.

### Mains Input (J1)

| Pin   | Function         | Terminal   | Wire Gauge | Notes                |
| ----- | ---------------- | ---------- | ---------- | -------------------- |
| J1-L  | Mains Live       | 6.3mm male | 14 AWG     | Fused, to relay COMs |
| J1-N  | Mains Neutral    | 6.3mm male | 14 AWG     | Common neutral bus   |
| J1-PE | Protective Earth | 6.3mm male | 14 AWG     | To chassis           |

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
- **S3 (Pin 5):** Level probe single wire, ground return via boiler body (PE connection)
- **NTCs (Pin 8-11):** 2-wire thermistors, polarity doesn't matter
- **PRESSURE (Pin 12-14):** 3-wire transducer: +5V (red), GND (black), Signal (yellow/white)
- **SSRs (Pin 15-18):** Connect to SSR DC input terminals (+5V to SSR+, SSR- to SSR DC-)

**Part Number:** Phoenix Contact MKDS 1/18-5.08 (18-position, 5.08mm pitch)

---

## ESP32 Display Module (J15)

8-pin JST-XH connector for external ESP32 display module.

| Pin | Signal      | Direction  | Notes                                        |
| --- | ----------- | ---------- | -------------------------------------------- |
| 1   | +5V         | Power      | ESP32 power                                  |
| 2   | GND         | Power      | Ground                                       |
| 3   | PICO_TX     | Pico→ESP32 | GPIO0 via 1kΩ (R40, 5V tolerance - ECO-03)   |
| 4   | PICO_RX     | ESP32→Pico | GPIO1 via 1kΩ (R41, 5V tolerance - ECO-03)   |
| 5   | PICO_RUN    | ESP32→Pico | Reset control (ESP32 GPIO8)                  |
| 6   | SPARE1      | ESP32↔Pico | GPIO16 ↔ ESP32 GPIO9, 4.7kΩ pull-down (R74)  |
| 7   | WEIGHT_STOP | ESP32→Pico | GPIO21, 4.7kΩ pull-down (R73)                |
| 8   | SPARE2      | ESP32↔Pico | GPIO22 ↔ ESP32 GPIO22, 4.7kΩ pull-down (R75) |

**Part Number:** JST B8B-XH-A (8-pin, 2.54mm pitch)

---

## Service/Debug Port (J16)

4-pin header for debug access. **Shares UART0 with J15 (ESP32).**

| Pin | Signal     | Notes                                |
| --- | ---------- | ------------------------------------ |
| 1   | +3.3V      | 3.3V power                           |
| 2   | GND        | Ground                               |
| 3   | SERVICE_TX | GPIO0 via 1kΩ (R42, shared with J15) |
| 4   | SERVICE_RX | GPIO1 via 1kΩ (R43, shared with J15) |

**⚠️ Disconnect ESP32 cable when using service port for flashing!**

**Protection:**

- R42/R43: 1kΩ series resistors for RP2350 5V tolerance protection (ECO-03)
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

3-position screw terminal for power meter mains pass-through.

| Pin | Signal  | Notes                |
| --- | ------- | -------------------- |
| 1   | L_FUSED | Fused live (from F1) |
| 2   | N       | Neutral              |
| 3   | PE      | Protective Earth     |

**⚠️ HIGH VOLTAGE - Connect to power meter only. No CT clamp through PCB.**

---

## Pico Socket (J20)

2×20 female header (2.54mm pitch) for Raspberry Pi Pico 2 module.

**Mounting Options:**

- Direct solder (production)
- Socket for easy replacement (prototype/debug)

After final testing, apply small dab of RTV silicone at module corners to prevent vibration-induced creep.
