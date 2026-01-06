# Bill of Materials (BOM)

## Integrated Circuits

| Qty | Ref | Description           | Part Number    | Package  | Notes                  |
| --- | --- | --------------------- | -------------- | -------- | ---------------------- |
| 1   | U1  | Raspberry Pi Pico 2   | SC0942         | Module   | Or Pico 2 W (SC1632)   |
| 1   | U2  | AC/DC Converter 5V 3A | HLK-15M05C     | Module   | Isolated, 15W          |
| 1   | U3  | 3.3V Sync Buck        | TPS563200DDCR  | SOT-23-6 | 3A, >90% efficiency    |
| 1   | U5  | Precision Voltage Ref | LM4040DIM3-3.0 | SOT-23-3 | 3.0V shunt             |
| 1   | U6  | Op-Amp                | OPA342UA       | SOIC-8   | Level probe oscillator |
| 1   | U7  | Comparator            | TLV3201AIDBVR  | SOT-23-5 | Level probe detector   |
| 1   | U8  | RS485 Transceiver     | MAX3485ESA+    | SOIC-8   | For industrial meters  |
| 1   | U9  | Dual Op-Amp           | OPA2342UA      | SOIC-8   | VREF buffer + spare    |

## Transistors and Diodes

| Qty | Ref     | Description    | Part Number | Package | Notes                   |
| --- | ------- | -------------- | ----------- | ------- | ----------------------- |
| 5   | Q1-Q5   | NPN Transistor | MMBT2222A   | SOT-23  | 3 relay + 2 SSR drivers |
| 3   | D1-D3   | Fast Flyback   | UF4007      | DO-41   | 75ns recovery           |
| 6   | D10-D15 | ESD Protection | PESD5V0S1BL | SOD-323 | Sensor inputs           |
| 1   | D16     | Schottky Clamp | BAT54S      | SOT-23  | Pressure ADC protection |
| 1   | D20     | TVS Diode      | SMBJ5.0A    | SMB     | 5V rail protection      |
| 1   | D21     | RS485 TVS      | SM712       | SOT-23  | A/B line protection     |
| 2   | D23-D24 | Zener          | BZT52C3V3   | SOD-123 | Service port clamp      |

## Resistors

### Analog Signal Conditioning

| Qty | Ref   | Value | Tolerance | Package | Notes                        |
| --- | ----- | ----- | --------- | ------- | ---------------------------- |
| 1   | R1    | 3.3kΩ | 1%        | 0805    | Brew NTC pull-up             |
| 1   | R1A   | 1.5kΩ | 1%        | 0805    | Brew NTC parallel (via JP1)  |
| 1   | R2    | 1.2kΩ | 1%        | 0805    | Steam NTC pull-up            |
| 1   | R2A   | 680Ω  | 1%        | 0805    | Steam NTC parallel (via JP2) |
| 2   | R5-R6 | 1kΩ   | 1%        | 0805    | NTC ADC series protection    |
| 1   | R3    | 10kΩ  | 1%        | 0805    | Pressure divider (to GND)    |
| 1   | R4    | 5.6kΩ | 1%        | 0805    | Pressure divider (series)    |
| 1   | R7    | 1kΩ   | 1%        | 0805    | LM4040 bias                  |
| 1   | R8    | 47Ω   | 1%        | 0805    | ADC VREF buffer isolation    |

### Digital I/O

| Qty | Ref         | Value | Tolerance | Package | Notes                         |
| --- | ----------- | ----- | --------- | ------- | ----------------------------- |
| 3   | R16-R18     | 10kΩ  | 5%        | 0805    | Switch pull-ups               |
| 1   | R71         | 10kΩ  | 5%        | 0805    | Pico RUN pull-up              |
| 6   | R11-R15,R19 | 4.7kΩ | 5%        | 0805    | Driver pull-downs (RP2350 E9) |
| 1   | R73         | 4.7kΩ | 5%        | 0805    | WEIGHT_STOP pull-down         |
| 1   | R74         | 4.7kΩ | 5%        | 0805    | SPARE1 pull-down (GPIO16)     |
| 1   | R75         | 4.7kΩ | 5%        | 0805    | SPARE2 pull-down (GPIO22)     |

### Transistor Drivers

| Qty | Ref             | Value | Tolerance | Package | Notes                   |
| --- | --------------- | ----- | --------- | ------- | ----------------------- |
| 5   | R20-R22,R24-R25 | 470Ω  | 5%        | 0805    | Base resistors          |
| 3   | R30-R32         | 470Ω  | 5%        | 0805    | Relay LED current limit |
| 2   | R34-R35         | 330Ω  | 5%        | 0805    | SSR LED current limit   |

### Communication

| Qty | Ref     | Value   | Tolerance | Package | Notes                                 |
| --- | ------- | ------- | --------- | ------- | ------------------------------------- |
| 4   | R40-R43 | **1kΩ** | 5%        | 0805    | UART series (5V tolerance protection) |
| 1   | R44     | 33Ω     | 5%        | 0805    | I2C series                            |
| 1   | R45     | 2.2kΩ   | 1%        | 0805    | J17 RX level shift (upper)            |
| 1   | R45A    | 3.3kΩ   | 1%        | 0805    | J17 RX level shift (lower)            |
| 1   | R45B    | 33Ω     | 5%        | 0805    | J17 RX series                         |
| 2   | R46-R47 | 4.7kΩ   | 5%        | 0805    | I2C pull-ups                          |
| 2   | R93-R94 | 20kΩ    | 5%        | 0805    | RS485 failsafe bias                   |

### User Interface

| Qty | Ref | Value | Tolerance | Package | Notes      |
| --- | --- | ----- | --------- | ------- | ---------- |
| 1   | R48 | 330Ω  | 5%        | 0805    | Status LED |
| 1   | R49 | 100Ω  | 5%        | 0805    | Buzzer     |

### Level Probe

| Qty | Ref     | Value | Tolerance | Package | Notes               |
| --- | ------- | ----- | --------- | ------- | ------------------- |
| 1   | R81     | 10kΩ  | 1%        | 0805    | Oscillator feedback |
| 1   | R82     | 4.7kΩ | 1%        | 0805    | Wien bridge gain    |
| 2   | R83-R84 | 10kΩ  | 1%        | 0805    | Wien bridge         |
| 1   | R85     | 100Ω  | 5%        | 0805    | Current limit       |
| 1   | R86     | 10kΩ  | 5%        | 0805    | AC bias             |
| 2   | R87-R88 | 100kΩ | 1%        | 0805    | Threshold divider   |
| 1   | R89     | 1MΩ   | 5%        | 0805    | Hysteresis          |

### Buck Converter

| Qty | Ref | Value | Tolerance | Package | Notes          |
| --- | --- | ----- | --------- | ------- | -------------- |
| 1   | R9  | 33kΩ  | 1%        | 0805    | Feedback upper |
| 1   | R10 | 10kΩ  | 1%        | 0805    | Feedback lower |

### 5V Monitoring

| Qty | Ref | Value | Tolerance | Package | Notes         |
| --- | --- | ----- | --------- | ------- | ------------- |
| 1   | R91 | 10kΩ  | 1%        | 0805    | Monitor upper |
| 1   | R92 | 5.6kΩ | 1%        | 0805    | Monitor lower |

## Capacitors

| Qty | Ref     | Value    | Voltage | Package  | Notes                                      |
| --- | ------- | -------- | ------- | -------- | ------------------------------------------ |
| 1   | C1      | 100nF X2 | 275V AC | Radial   | Mains EMI filter (**Compact 10mm pitch**)  |
| 1   | C2      | 470µF    | 6.3V    | **SMD**  | 5V bulk (SMD V-Chip 6.3mm or 8mm)          |
| 1   | C3      | 22µF     | 25V     | 1206     | Buck input (X5R)                           |
| 2   | C4,C4A  | 22µF     | 10V     | 1206     | Buck output (X5R)                          |
| 1   | **C5**  | **47µF** | **10V** | **1206** | **3.3V rail bulk (WiFi/relay transients)** |
| 1   | C7      | 22µF     | 10V     | 1206     | ADC ref bulk                               |
| 1   | C7A     | 100nF    | 25V     | 0805     | ADC ref HF decoupling                      |
| ~15 | Various | 100nF    | 25V     | 0805     | Decoupling                                 |
| 2   | C61-C62 | 10nF     | 50V     | 0805     | Wien bridge timing                         |
| 1   | C64     | 1µF      | 25V     | 0805     | Level probe AC coupling                    |

**⚠️ NOTE: Relay Snubber Capacitors (C25/C27) REMOVED**

- Previous design used 50V ceramic capacitors (C25/C27) as relay snubbers
- These were replaced with MOVs (RV2/RV3) in v2.21 for improved safety and reliability
- MOVs provide better arc suppression without the risk of capacitor failure
- See Electromechanical section for RV2/RV3 specifications

## Inductors

| Qty | Ref | Value       | Saturation | DCR    | Package | Notes                    |
| --- | --- | ----------- | ---------- | ------ | ------- | ------------------------ |
| 1   | L1  | 2.2µH       | 3A         | <100mΩ | 1210    | Buck inductor            |
| 1   | FB1 | 600Ω@100MHz | -          | -      | 0603    | Analog section isolation |

## Electromechanical

| Qty | Ref     | Description      | Part Number          | Notes                 |
| --- | ------- | ---------------- | -------------------- | --------------------- |
| 2   | K1,K3   | Relay 5V 3A      | Panasonic APAN3105   | Slim 5mm              |
| 1   | K2      | Relay 5V 16A     | Omron G5LE-1A4-E DC5 | **-E variant!**       |
| 1   | F1      | **Fuse 10A SMD** | **Littelfuse 463**   | **Nano² (10x3mm)**    |
| 1   | F2      | **Fuse 2A SMD**  | **Littelfuse 463**   | **Nano² (10x3mm)**    |
| 1   | RV1     | Varistor 275V    | S14K275              | 14mm, mains surge     |
| 2   | RV2-RV3 | Varistor 275V    | S10K275              | 10mm, arc suppression |
| 1   | SW1     | Tactile Switch   | EVQP7A01P            | 6×6mm SMD             |
| 1   | BZ1     | Passive Buzzer   | CEM-1203(42)         | 12mm                  |

## LEDs

| Qty | Ref       | Description     | Color  | Package |
| --- | --------- | --------------- | ------ | ------- |
| 3   | LED1-LED3 | Relay Indicator | Green  | 0805    |
| 2   | LED5-LED6 | SSR Indicator   | Orange | 0805    |
| 1   | LED7      | Status          | Green  | 0805    |

## Connectors

| Qty   | Ref     | Description           | Part Number            | Notes                                                  |
| ----- | ------- | --------------------- | ---------------------- | ------------------------------------------------------ |
| **2** | **J1**  | **6.3mm Spade**       | **Keystone 1285**      | **Mains input (L, N) - 2 discrete terminals required** |
| 3     | J2-J4   | 6.3mm Spade           | Keystone 1285          | Relay outputs                                          |
| 1     | J5      | 6.3mm Spade           | Keystone 1285          | Chassis Reference (SRif)                               |
| 1     | **J26** | Screw Terminal 18-pos | Phoenix MKDS 1/18-5.08 | All LV connections                                     |
| 1     | J15     | JST-XH 8-pin          | JST B8B-XH-A           | ESP32 module                                           |
| 1     | J16     | Header 4-pin          | 2.54mm pitch           | Service/debug                                          |
| 1     | J17     | JST-XH 6-pin          | JST B6B-XH-A           | Power meter                                            |
| 1     | J20     | Header 2×20           | 2.54mm pitch           | Pico socket                                            |
| 1     | J23     | Header 4-pin          | 2.54mm pitch           | I2C accessory                                          |
| 1     | J24     | Screw Terminal 2-pos  | 5.08mm pitch           | Power meter HV (L, N only - PE removed)                |

## Solder Jumpers

| Qty | Ref | Type  | Function              | Default     |
| --- | --- | ----- | --------------------- | ----------- |
| 1   | JP1 | 2-pad | Brew NTC selection    | OPEN (50kΩ) |
| 1   | JP2 | 2-pad | Steam NTC selection   | OPEN (50kΩ) |
| 1   | JP3 | 2-pad | Power meter RX level  | OPEN (5V)   |
| 1   | JP4 | 3-pad | Power meter interface | 1-2 (RS485) |

## Test Points

Test points are organized by function for systematic board bring-up and debugging.

### Power Rails

| Qty | Ref | Signal | Location        | Notes                                 |
| --- | --- | ------ | --------------- | ------------------------------------- |
| 1   | TP1 | GND    | Near Pico       | Ground reference for all measurements |
| 1   | TP2 | 5V     | Near HLK output | Main 5V rail verification             |
| 1   | TP3 | 3.3V   | Near Pico       | Logic rail verification               |
| 1   | TP4 | 5V_MON | Near R91/R92    | 5V monitor divider (~1.8V)            |

### Analog Signals

| Qty | Ref | Signal   | Location       | Notes                      |
| --- | --- | -------- | -------------- | -------------------------- |
| 1   | TP5 | ADC_VREF | Near U9 output | 3.0V reference calibration |
| 1   | TP6 | ADC0     | Near GPIO26    | Brew NTC signal            |
| 1   | TP7 | ADC1     | Near GPIO27    | Steam NTC signal           |
| 1   | TP8 | ADC2     | Near GPIO28    | Pressure transducer signal |

### Communication

| Qty | Ref  | Signal   | Location | Notes                            |
| --- | ---- | -------- | -------- | -------------------------------- |
| 1   | TP9  | UART0_TX | Near J15 | Debug/ESP32 TX (GPIO0)           |
| 1   | TP10 | UART0_RX | Near J15 | Debug/ESP32 RX (GPIO1)           |
| 1   | TP11 | RS485_DE | Near U8  | RS485 direction control (GPIO20) |

## Mounting Hardware

| Qty | Ref     | Description      | Notes                             |
| --- | ------- | ---------------- | --------------------------------- |
| 4   | MH1-MH4 | M3 Mounting Hole | NPTH, isolated (no PE connection) |

---

## User-Supplied Components (NOT included with PCB)

| Component            | Specification         | Purpose             |
| -------------------- | --------------------- | ------------------- |
| External SSRs (×2)   | 40A, 3-32V DC control | Brew/Steam heaters  |
| Pressure Transducer  | 0-16 bar, 0.5-4.5V    | Group head pressure |
| NTC Sensors (×2)     | 50kΩ @ 25°C (or 10kΩ) | Boiler temperatures |
| ESP32 Display Module | Custom PCB            | User interface      |
| Power Meter          | PZEM-004T or similar  | Power monitoring    |
