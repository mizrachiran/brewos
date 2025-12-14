# ECM Synchronika Control Board - Schematic Reference

## Detailed Circuit Schematics for PCB Design

**Board Size:** 130mm × 80mm (2-layer, 2oz copper)  
**Enclosure:** 150mm × 100mm mounting area  
**Revision:** v2.24.3  
**Date:** December 2025

---

## Key Design Features

1. Universal external power metering interface (J17 LV + J24 HV)
2. Multi-machine NTC compatibility via JP1/JP2 jumpers (50kΩ or 10kΩ)
3. RS485 and TTL UART support via on-board MAX3485 transceiver
4. Unified 18-position low-voltage terminal block (J26)
5. OPA342 + TLV3201 AC-excited level probe (prevents electrolysis)
6. Buffered precision ADC reference (LM4040 + OPA2342)
7. HLK-15M05C isolated power supply (5V 3A)
8. TPS563200 synchronous buck converter (3.3V 3A, >90% efficient)
9. MOV arc suppression on inductive loads (across load, not contacts)
10. Mounting: MH1=PE star point (PTH), MH2-4=NPTH (isolated)

---

# Sheet 1: Power Supply

## 1.1 Mains Input & Protection

```
                                    MAINS INPUT & PROTECTION
    ════════════════════════════════════════════════════════════════════════════

    J1 (Mains Input)
    ┌─────────────┐
    │ L  (Live)   ├────┬──────────────────────────────────────────────────────────►
    │             │    │
    │ N  (Neutral)├──┐ │    F1                RV1              C1
    │             │  │ │  ┌─────┐          ┌──────┐        ┌──────┐
    │ PE (Earth)  ├─┐│ └──┤ 10A ├────┬─────┤ MOV  ├────┬───┤ 100nF├───┬─────► L_FUSED
    └─────────────┘ ││    │250V │    │     │ 275V │    │   │ X2   │   │
                    ││    │T/Lag│    │     │ 14mm │    │   │275VAC│   │
        To Chassis  ││    └─────┘    │     └──┬───┘    │   └──┬───┘   │
        Ground      ││               │        │        │      │       │
        (if metal   │└───────────────┼────────┴────────┴──────┴───────┴─────► N_FUSED
        enclosure)  │                │
                    │                │
                   ─┴─              ─┴─
                   GND              GND
                   (PE)            (Mains)

    Component Values:
    ─────────────────
    F1:  Littelfuse 0218010.MXP, 10A 250V, 5x20mm, Slow-blow (relay loads only ~6A)
    F2:  Littelfuse 0218002.MXP, 2A 250V, 5x20mm, Slow-blow
    RV1: Epcos S14K275 or Littelfuse V275LA20AP, 275VAC, 14mm disc
    C1:  TDK B32922C3104M, 100nF, 275VAC, X2 Safety Rated
```

## 1.2 AC/DC Isolated Power Supply

```
                                AC/DC ISOLATED CONVERTER
    ════════════════════════════════════════════════════════════════════════════

                                    U2: Hi-Link HLK-15M05C
                                    (3A, compact 16mm height)
                              ┌──────────────────────────┐
                              │                          │
    L_FUSED ──────────────────┤ L               +Vout    ├───────┬──────────► +5V
                              │     ╔═══════╗            │       │
                              │     ║ 3kV   ║            │   C2  │
                              │     ║ISOLATE║            │ ┌─────┴─────┐
    N_FUSED ──────────────────┤ N   ╚═══════╝    -Vout   │ │   470µF   │
                              │                          ├─┤   6.3V    │
                              │                          │ │  Polymer  │
                              └──────────────────────────┘ └─────┬─────┘
                                                                 │
                                                                ─┴─
                                                                GND
                                                              (Secondary)

    NOTES:
    ══════
    • Primary side (L, N) is MAINS VOLTAGE - maintain 6mm clearance to secondary
    • Secondary side (-Vout) becomes system GND
    • GND is ISOLATED from mains PE (earth)
    • HLK-15M05C is 48×28×18mm - verify clearance to fuse holder

    Component Values:
    ─────────────────
    U2:  Hi-Link HLK-15M05C (5V 3A/15W) - adequate for ~1.1A peak load
         Alt: Mean Well IRM-20-5 (5V 4A) if more headroom needed
    C2:  470µF 6.3V Polymer (low ESR, long life in hot environment)
```

## 1.3 5V to 3.3V Synchronous Buck Converter

Uses a synchronous buck converter for high efficiency (>90%) and minimal heat dissipation,
critical for reliable operation inside hot espresso machine enclosures.

```
                            3.3V SYNCHRONOUS BUCK CONVERTER
    ════════════════════════════════════════════════════════════════════════════

                            U3: TPS563200DDCR
                           ┌──────────────────────┐
                           │                      │
    +5V ─────┬─────────────┤ VIN            SW    ├────┬────[L1 2.2µH]────┬──► +3.3V
             │             │                      │    │                   │
         C3  │      ┌──────┤ EN              FB   ├────┼───────────────────┼──┐
       ┌─────┴─────┐│      │                      │    │                   │  │
       │   22µF    ││      │                      │    │              ┌────┴────┐
       │   25V     ││      │       GND            │    │              │  22µF   │
       │ Ceramic   ││      └──────────┬───────────┘    │              │  10V    │
       │  (X5R)    ││                 │                │              │  (C4)   │
       └─────┬─────┘│                ─┴─               │              └────┬────┘
             │      │                GND               │                   │
            ─┴─     │                                  │              ┌────┴────┐
            GND     │                                  │              │  22µF   │
                    │                                  │              │  10V    │
                    └───► +5V (EN tied high)           │              │  (C4A)  │
                                                       │              └────┬────┘
                                                       │                   │
    Feedback Divider (sets 3.3V output):              │                   │
    ─────────────────────────────────────              │                   │
                           ┌────┴────┐                │                   │
                           │  33kΩ   │ R_FB1          │                   │
                           │   1%    │                │                   │
                           └────┬────┘                │                   │
                                │                     │                   │
                           FB ──┴─────────────────────┘                   │
                                │                                         │
                           ┌────┴────┐                                    │
                           │  10kΩ   │ R_FB2                              │
                           │   1%    │                                    │
                           └────┬────┘                                    │
                                │                                         │
                               ─┴─                                       ─┴─
                               GND                                       GND

    ✅ WHY BUCK INSTEAD OF LDO? (Engineering Review Fix)
    ─────────────────────────────────────────────────────
    LDO Problem: P = (5V - 3.3V) × I = 1.7V × I
    At 250mA in hot environment: junction temp exceeds 125°C max

    Buck Solution: >90% efficient, minimal self-heating
    Same footprint, better thermal reliability

    Component Values:
    ─────────────────
    U3:     TI TPS563200DDCR, 3A sync buck, SOT-23-6
    L1:     2.2µH, 3A saturation, DCR <100mΩ (Murata LQH32CN2R2M23)
            ⚠️ 2.2µH per TI datasheet for 3.3V output - D-CAP2 requires adequate ripple!
    C3:     22µF 25V X5R Ceramic, 1206 (input)
    C4:     22µF 10V X5R Ceramic, 1206 (output)
    C4A:    22µF 10V X5R Ceramic, 1206 (output, parallel for ripple)
    R_FB1:  33kΩ 1% 0805 (feedback upper resistor, FB to 3.3V)
    R_FB2:  10kΩ 1% 0805 (feedback lower resistor, FB to GND)

    Output Voltage Setting:
    V_OUT = 0.768V × (1 + R_FB1/R_FB2) = 0.768V × (1 + 33k/10k) = 3.30V ✓

    Pico Internal Regulator Configuration:
    ─────────────────────────────────────────────
    Pico 2 Pin 37 (3V3_EN) is connected to GND, disabling the internal RT6150B
    regulator. The TPS563200 powers the ENTIRE 3.3V domain via Pico Pin 36 (3V3).
    This avoids "hard parallel" regulator contention and potential reverse current.
```

## 1.4 Precision ADC Voltage Reference (Buffered)

**Purpose:** Provides stable 3.0V reference for NTC measurements, eliminating supply drift errors.

The LM4040 generates a precision 3.0V reference, but cannot source enough current for the NTC
pull-up resistors. An op-amp buffer (U9A) provides the current drive capability.

```
                      PRECISION ADC VOLTAGE REFERENCE (BUFFERED)
    ════════════════════════════════════════════════════════════════════════════

    CIRCUIT OVERVIEW:
    ─────────────────
    Two distinct voltage nodes exist in this circuit:

      NODE A: "REF_3V0" - LM4040 output (3.0V, ~0.3mA max, HIGH IMPEDANCE)
              → Feeds op-amp non-inverting input only (pA load)

      NODE B: "ADC_VREF" - Op-amp buffered output (3.0V, ~25mA capable, LOW IMPEDANCE)
              → Powers NTC pull-up resistors R1, R2

    Both nodes are 3.0V, but ADC_VREF can drive heavy loads without voltage drop.

    ═══════════════════════════════════════════════════════════════════════════
                              SCHEMATIC
    ═══════════════════════════════════════════════════════════════════════════

                            +3.3V_FILTERED
                                  │
                    ┌─────────────┴─────────────┐
                    │                           │
               ┌────┴────┐                 ┌────┴────┐
               │  FB1    │                 │  C80    │
               │ Ferrite │                 │  100nF  │  U9 VCC decoupling
               │ 600Ω@   │                 └────┬────┘
               │ 100MHz  │                      │
               └────┬────┘                      ├─────────────► U9 Pin 8 (VCC)
                    │                           │
               ┌────┴────┐                      │
               │   R7    │                      │
               │   1kΩ   │ Bias current         │
               │   1%    │ I_bias = 0.3mA       │
               └────┬────┘                      │
                    │                           │
                    │ ◄── NODE A: "REF_3V0"     │
                    │     (3.0V, high-Z)        │
                    │                           │
               ┌────┴────┐                      │
               │   U5    │                      │
               │ LM4040  │                      │
               │  3.0V   │                      │
               │  Shunt  │                      │
               │   Ref   │                      │
               └────┬────┘                      │
                    │                           │
                   ─┴─                          │
                   GND                          │
                                                │
    ═══════════════════════════════════════════════════════════════════════════
                         UNITY-GAIN BUFFER (U9A)
    ═══════════════════════════════════════════════════════════════════════════

                                                     VCC (+3.3V)
                                                         │
        NODE A ─────────────────────────────┐            │
        "REF_3V0"                           │       ┌────┴────┐
        (from LM4040)                       │       │   U9A   │
                                            │       │OPA2342  │
                                            └──────►│+       O├────┬──────────┐
                                                    │         │    │          │
                                            ┌──────►│-        │    │     ┌────┴────┐
                                            │       └────┬────┘    │     │  R_ISO  │
                                            │            │         │     │   47Ω   │
                                            │           ─┴─        │     │   1%    │
                                            │           GND        │     └────┬────┘
                                            │                      │          │
                                            │                      │          │
                                            └──────────────────────┘          │
                                                 100% FEEDBACK                │
                                                 (Unity Gain)                 │
                                                                              │
                                                                              │
                                      NODE B: "ADC_VREF" (3.0V, low-Z) ◄──────┘
                                                        │
                          ┌─────────────────────────────┼─────────────────────┐
                          │                             │                     │
                     ┌────┴────┐                   ┌────┴────┐           ┌────┴────┐
                     │   C7    │                   │  C7A    │           │  LOAD   │
                     │  22µF   │ Bulk              │  100nF  │ HF        │ R1, R2  │
                     │  10V    │ decoupling        │  25V    │ bypass    │ NTC     │
                     └────┬────┘                   └────┬────┘           │ pull-ups│
                          │                             │                └────┬────┘
                         ─┴─                           ─┴─                    │
                         GND                           GND              To NTC ADCs
                                                                       (Sheet 5)

    ═══════════════════════════════════════════════════════════════════════════
                            SIGNAL FLOW
    ═══════════════════════════════════════════════════════════════════════════

      +3.3V ──► FB1 ──► R7 ──┬──► LM4040 ──► GND
                             │
                             │ REF_3V0 (3.0V, ~0.3mA available, high impedance)
                             │
                             └──► U9A (+) ──► U9A (OUT) ──► R_ISO ──► ADC_VREF
                                     │                                   │
                                     └─────────── FEEDBACK ──────────────┘
                                                                         │
                                                    (3.0V, ~25mA capable, low impedance)
                                                                         │
                                              ┌──────────────────────────┼──────────┐
                                              │                          │          │
                                             C7                        C7A      R1, R2
                                            22µF                      100nF    (NTC loads)

    ═══════════════════════════════════════════════════════════════════════════

    WHY THE BUFFER IS NEEDED:
    ─────────────────────────
    R7 (1kΩ) limits current to ~0.3mA. The LM4040 needs some of this to regulate.
    Without buffer, this 0.3mA must also power the NTC pull-up resistors:

      NTC Load Current (at operating temperatures):
      • Brew NTC at 93°C: R_NTC ≈ 3.5kΩ → I = 3.0V/(3.3kΩ+3.5kΩ) ≈ 441µA
      • Steam NTC at 135°C: R_NTC ≈ 1kΩ → I = 3.0V/(1.2kΩ+1kΩ) ≈ 1.36mA
      • Total: ~1.8mA needed (6× more than 0.3mA available!)

      WITHOUT BUFFER: REF_3V0 collapses to ~0.5V → ADC readings invalid
      WITH BUFFER:    U9A sources current from +3.3V rail → ADC_VREF stable

    WHY R_ISO (47Ω) IS NEEDED:
    ──────────────────────────
    The 22µF capacitive load (C7) can cause op-amp oscillation.
    R_ISO isolates the op-amp output, ensuring stability.
    DC accuracy: 47Ω × 1.8mA = 85mV drop (negligible for ratiometric ADC)

    COMPONENT VALUES:
    ─────────────────
    U5:    TI LM4040DIM3-3.0, 3.0V shunt reference, SOT-23-3
    U9:    TI OPA2342UA, dual RRIO op-amp, SOIC-8 (U9A used here, U9B spare)
    R7:    1kΩ 1%, 0805 (bias - loads only pA into op-amp input)
    R_ISO: 47Ω 1%, 0805 (output isolation - prevents oscillation)
    FB1:   Murata BLM18PG601SN1D, 600Ω @ 100MHz, 0603
    C7:    22µF 10V X5R Ceramic, 1206 (bulk decoupling on ADC_VREF)
    C7A:   100nF 25V Ceramic, 0805 (HF bypass on ADC_VREF)
    C80:   100nF 25V Ceramic, 0805 (U9 VCC pin decoupling)

    TEST POINT:
    ───────────
    TP2 provides access to ADC_VREF for verification. Expected: 3.0V ±1%

    U9B (SPARE OP-AMP):
    ───────────────────
    Second half of OPA2342 available for future use (e.g., pressure sensor buffer).
    Tie unused inputs to prevent oscillation: IN+ to GND, IN- to OUT.
```

---

# Sheet 2: Microcontroller (Raspberry Pi Pico 2)

## 2.1 Pico 2 Module & Decoupling

```
                           RASPBERRY PI PICO 2 MODULE
    ════════════════════════════════════════════════════════════════════════════

                               U1: Raspberry Pi Pico 2

         +3.3V ◄────────────────┐           ┌────────────────► (Internal 3.3V)
                                │           │
         +5V ───────────────────┤ VSYS  3V3 ├───────────────── (3.3V out, max 300mA)
                   ┌────┐       │           │       ┌────┐
         GND ──┬───┤100n├───────┤ GND   GND ├───────┤100n├───┬── GND
               │   └────┘       │           │       └────┘   │
               │                │           │                │
               │     ┌──────────┤ GP0   GP28├────────────────┼─► ADC2 (Pressure)
               │     │          │           │                │
    ESP32_TX ◄─┼─────┤          │ GP1   GP27├────────────────┼─► ADC1 (Steam NTC)
               │     │          │           │                │
    ESP32_RX ──┼─────┤          │ GP2   GP26├────────────────┼─► ADC0 (Brew NTC)
               │     │          │           │                │
   WATER_SW ◄──┼─────┤          │ GP3   RUN ├────────────────┼── SW1 (Reset)
               │     │          │           │                │
   TANK_LVL ◄──┼─────┤          │ GP4  GP22 ├────────────────┼◄─► SPARE2 (ESP32 GPIO22)
               │     │          │           │                │
   LEVEL_PRB◄──┼─────┤          │ GP5  GP21 ├────────────────┼◄─ WEIGHT_STOP (ESP32)
               │     │          │           │                │
   BREW_SW ◄───┼─────┤          │ GP6  GP20 ├────────────────┼─► RS485 DE/RE
               │     │          │           │                │
   METER_TX ──►┼─────┤          │ GP7  GP19 ├────────────────┼─► BUZZER (PWM)
               │     │          │           │                │
   METER_RX ◄──┼─────┤          │ GP8  GP18 ├────────────────┼── SPI_SCK (Spare)
               │     │          │           │                │
   I2C_SDA ◄──►┼─────┤          │ GP9  GP17 ├────────────────┼── SPI_CS (Spare)
               │     │          │           │                │
   I2C_SCL ───►┼─────┤          │ GP10 GP16 ├────────────────┼◄─► SPARE1 (ESP32 GPIO9)
               │     │          │           │                │
   RELAY_K1 ◄──┼─────┤          │ GP11 GP15 ├────────────────┼─► STATUS_LED
               │     │          │           │                │
   RELAY_K2 ◄──┼─────┤          │ GP12 GP14 ├────────────────┼─► SSR2_STEAM
               │     │          │           │                │
   RELAY_K3 ◄──┼─────┤          │ GP13 BOOT ├────────────────┼── (Pico onboard)
               │     │          │           │                │
   SSR1_BREW◄──┼─────┤          │ GND   GND │                │
               │                │     │     │                │
               └────────────────┴─────┼─────┴────────────────┘
                                      │
                                     ─┴─
                                     GND

    GPIO Pin Summary:
    ─────────────────
    LEFT SIDE (Active Functions)              RIGHT SIDE (Analog + ESP32 I/O)
    ────────────────────────────              ─────────────────────────────
    GP0  = UART0 TX → ESP32                   GP28 = ADC2 (Pressure 0.5-4.5V)
    GP1  = UART0 RX ← ESP32                   GP27 = ADC1 (Steam NTC)
    GP2  = Water Reservoir Switch             GP26 = ADC0 (Brew NTC)
    GP3  = Tank Level Sensor                  RUN  = Reset button (SW1)
    GP4  = Steam Level (Comparator)           GP22 = SPARE2 ↔ ESP32 GPIO22 (J15-8)
    GP5  = Brew Handle Switch                 GP21 = WEIGHT_STOP ← ESP32 GPIO10 (J15-7)
    GP6  = Meter TX (UART1)                   GP20 = RS485 DE/RE
    GP7  = Meter RX (UART1)                   GP19 = Buzzer (PWM)
    GP8  = I2C0 SDA (Accessory)               GP18 = SPI_SCK (available)
    GP9  = I2C0 SCL (Accessory)               GP17 = SPI_CS (available)
    GP10 = Relay K1 (Lamp)                    GP16 = SPARE1 ↔ ESP32 GPIO9 (J15-6)
    GP11 = Relay K2 (Pump)                    GP15 = Status LED
    GP12 = Relay K3 (Solenoid)                GP14 = SSR2 (Steam)
    GP13 = SSR1 (Brew)                        BOOT = Bootsel button (onboard)

    Decoupling Capacitors (place adjacent to Pico):
    ───────────────────────────────────────────────
    • 100nF ceramic on each VSYS pin
    • 100nF ceramic on each 3V3 pin
    • 10µF ceramic near VSYS input (optional)
```

## 2.2 Reset Button

```
                            RESET BUTTON
    ════════════════════════════════════════════════════════════════════════════

    RESET BUTTON (SW1)
    ──────────────────

         +3.3V
           │
      ┌────┴────┐
      │  10kΩ   │  ◄── Internal on Pico
      │  R70    │      (may not need external)
      └────┬────┘
           │
           ├──────────────► RUN Pin
           │                (Pico)
       ┌───┴───┐
       │  SW1  │  6x6mm SMD
       │ RESET │  Tactile
       │       │  (EVQP7A01P)
       └───┬───┘
           │
          ─┴─
          GND

    Operation:
    ──────────
    • Press SW1 → Reset Pico
```

---

# Sheet 3: Relay Drivers

## 3.1 Relay Driver Circuit (K1, K2, K3)

```
                            RELAY DRIVER CIRCUIT
    ════════════════════════════════════════════════════════════════════════════

    (Identical driver circuit for K1, K2, K3. Different relay sizes by load.)

                                        +5V
                                         │
               ┌─────────────────────────┴─────────────────────────┐
               │                                                   │
          ┌────┴────┐                                         ┌────┴────┐
          │  Relay  │                                         │  470Ω   │
          │  Coil   │                                         │  R30+n  │
          │   5V    │                                         │  (LED)  │
          │  K1/2/3 │                                         └────┬────┘
          └────┬────┘                                              │
               │                                              ┌────┴────┐
          ┌────┴────┐                                         │   LED   │
          │  D1/2/3 │ ◄── Flyback diode                       │  Green  │
          │ 1N4007  │                                         │ LED1-3  │
          └────┬────┘                                         └────┬────┘
               │                                                   │
               └───────────────────────┬───────────────────────────┤
                                       │                           │
                                  ┌────┴────┐                      │
                                  │    C    │                      │
                                  │  Q1/2/3 │  MMBT2222A           │
                                  │   NPN   │  SOT-23              │
    GPIO (10/11/12) ────[470Ω]───►│    B    │                      │
                        R20+n     │    E    ├──────────────────────┘
                           │      └────┬────┘
                      ┌────┴────┐      │
                      │  4.7kΩ  │     ─┴─
                      │  R11+n  │     GND
                      │ Pull-dn │
                      └────┬────┘
                          ─┴─
                          GND

    OPERATION:
    ──────────
    GPIO LOW  → Transistor OFF → Relay OFF, LED OFF
    GPIO HIGH → Transistor ON  → Relay ON, LED ON

    Relay coil current: ~70mA (through transistor collector)
    LED current: (5V - 2.0V) / 470Ω = 6.4mA (bright indicator)

    Relay Contact Connections (All 220V AC - 6.3mm Spade Terminals):
    ─────────────────────────────────────────────────────────────────

    K1 (Mains Lamp):           K2 (Pump):                K3 (Solenoid):
    J2-NO ── K1-NO             J3-NO ── K2-NO            J4-NO ── K3-NO
    (6.3mm spade, 220V)        (6.3mm spade, 220V 5A)    (6.3mm spade, 220V)
    COM internal to L_FUSED    COM internal to L_FUSED   COM internal to L_FUSED

    Component Values:
    ─────────────────
    K1, K3: Panasonic APAN3105 (5V coil, 3A contacts, slim 5mm, IEC61010)
            • K1 (Indicator Lamp): Switches mains indicator lamp (~100mA load)
              3A relay provides ample margin for this low-current application
            • K3 (Solenoid): ~0.5A load, 3A rating is plenty
    K2:     Omron G5LE-1A4-E DC5 (5V coil, 16A contacts, standard size)
            ⚠️ MUST use -E (high capacity) variant! Standard G5LE-1A4 = 10A only
            • Pump motor needs robust contacts for inrush current
    D1-D3:  UF4007 (1A, 1000V, 75ns fast recovery) - DO-41
            Fast recovery type ensures snappy relay contact opening
            and reduces arc time at relay contacts
    Q1-Q3:  MMBT2222A (SOT-23)
    LED1-3: Green 0805, Vf~2.0V
    R20-22: 470Ω 5% 0805 (transistor base)
    R30-32: 470Ω 5% 0805 (LED current limit - gives 6.4mA)
    R11-13: 4.7kΩ 5% 0805 (pull-down per RP2350 errata E9)

    ═══════════════════════════════════════════════════════════════════════════
    MOV ARC SUPPRESSION (Inductive Load Protection)
    ═══════════════════════════════════════════════════════════════════════════

    Required for K2 (Pump) and K3 (Solenoid) to prevent contact welding.

    MOV Placement (Across Load):
    ──────────────────────────────────────────────────────────

    Incorrect Placement:            Correct Placement:

         Relay NO ──┬─► Load            Relay NO ─────┬──────► Pump Live
                    │                                 │
               ┌────┴────┐                      ┌─────┴─────┐
               │   MOV   │ ✗ WRONG!            │   Pump    │
               └────┬────┘                      │   Motor   │
                    │                           └─────┬─────┘
         Relay COM ─┴─── Live                         │
                                                  ┌────┴────┐
    If MOV shorts:                               │   MOV   │ ✓ CORRECT
    Direct L → Pump                              │  RV2/3  │   (Load side)
    Relay bypassed!                              └────┬────┘
    UNSAFE!                                            │
                                                  Neutral ──┴─── Return

                                              If MOV shorts:
                                              Live → Neutral short
                                              → F1 fuse blows
                                              → SAFE shutdown

    MOV vs RC Snubber:
    ───────────────────
    • ~70% smaller than X2 capacitor + resistor combo
    • No series resistor needed - simpler BOM
    • Faster clamping response (nanoseconds)
    • Critical with downsized K1/K3 relays (3A contacts)

    MOV Placement Strategy (IEC 60335-1 Compliant):
    ────────────────────────────────────────────────
    K2 (Pump):     RV2 from J3-NO to Neutral (across load)
                   • MOV short creates L-N fault → F1 fuse clears
                   • Provides arc suppression at contact opening

    K3 (Solenoid): RV3 from J4-NO to Neutral (across load)
                   • Same protection strategy as K2

    K1 (LED):      DNP (footprint only, resistive load - no inductive kickback)

    MOV Component Values:
    ─────────────────────
    RV2-RV3: S10K275 (275V AC, 10mm disc, 2500A surge)
             Footprint: Disc_D10mm_W5.0mm_P7.50mm

    Safety Note:
    MOVs are placed across the load (Phase to Neutral) rather than across
    relay contacts. This ensures that MOV failure creates a fuse-clearable
    fault rather than bypassing the switching element.
```

---

# Sheet 4: SSR Drivers

## 4.1 External SSR System Overview

**CRITICAL CONCEPT:** The PCB provides ONLY low-voltage (5V DC) control signals to external
Solid State Relays. The SSRs are mounted externally and switch mains power directly to heaters
using the machine's existing high-voltage wiring. NO HIGH CURRENT flows through this PCB.

```
    ═══════════════════════════════════════════════════════════════════════════════════════
                        COMPLETE SSR SYSTEM WIRING DIAGRAM
    ═══════════════════════════════════════════════════════════════════════════════════════

    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                              CONTROL PCB (This Board)                               │
    │                                                                                     │
    │   J26 Terminal Block (Pins 15-18 for SSR Control)                                  │
    │   ┌───────────────────────────────────────────────────┐                            │
    │   │  ... │ 15  │ 16  │ 17  │ 18  │                    │                            │
    │   │      │SSR1+│SSR1-│SSR2+│SSR2-│                    │                            │
    │   └──────┴──┬──┴──┬──┴──┬──┴──┬──┴────────────────────┘                            │
    │             │     │     │     │                                                     │
    │             │     │     │     └──────── 5V DC (SSR2 control -)                     │
    │             │     │     └────────────── 5V DC (SSR2 control +)                     │
    │             │     └──────────────────── 5V DC (SSR1 control -)                     │
    │             └────────────────────────── 5V DC (SSR1 control +)                     │
    │                                                                                     │
    │   Internal Driver Circuit (GPIO13 → SSR1, GPIO14 → SSR2)                           │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
                  │     │     │     │
                  │     │     │     │   ◄─── 4 wires to external SSRs
                  │     │     │     │        (low voltage, ~5V DC, <20mA)
                  ▼     ▼     ▼     ▼
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                           EXTERNAL SSR MODULES (Mounted in machine)                 │
    │                                                                                     │
    │   ┌─────────────────────────────┐       ┌─────────────────────────────┐            │
    │   │   SSR1 - BREW HEATER        │       │   SSR2 - STEAM HEATER       │            │
    │   │   KS15 D-24Z25-LQ           │       │   KS15 D-24Z25-LQ           │            │
    │   │                             │       │                             │            │
    │   │   DC CONTROL SIDE:          │       │   DC CONTROL SIDE:          │            │
    │   │   ┌─────────────────────┐   │       │   ┌─────────────────────┐   │            │
    │   │   │  (+)           (-)  │   │       │   │  (+)           (-)  │   │            │
    │   │   │   ▲             ▲   │   │       │   │   ▲             ▲   │   │            │
    │   │   └───┼─────────────┼───┘   │       │   └───┼─────────────┼───┘   │            │
    │   │       │             │       │       │       │             │       │            │
    │   │  from J26-15   from J26-16  │       │  from J26-17   from J26-18  │            │
    │   │   (SSR1+)       (SSR1-)     │       │   (SSR2+)       (SSR2-)     │            │
    │   │                             │       │                             │            │
    │   │   AC LOAD SIDE:             │       │   AC LOAD SIDE:             │            │
    │   │   ┌─────────────────────┐   │       │   ┌─────────────────────┐   │            │
    │   │   │  (L)           (T)  │   │       │   │  (L)           (T)  │   │            │
    │   │   │   ▲             │   │   │       │   │   ▲             │   │   │            │
    │   │   └───┼─────────────┼───┘   │       │   └───┼─────────────┼───┘   │            │
    │   │       │             │       │       │       │             │       │            │
    │   └───────┼─────────────┼───────┘       └───────┼─────────────┼───────┘            │
    │           │             │                       │             │                    │
    │           │             ▼                       │             ▼                    │
    │           │       To Brew Heater                │       To Steam Heater            │
    │           │       (1300W typical)               │       (1000W typical)            │
    │           │                                     │                                  │
    └───────────┼─────────────────────────────────────┼──────────────────────────────────┘
                │                                     │
                │                                     │
    ┌───────────┴─────────────────────────────────────┴──────────────────────────────────┐
    │                        MACHINE MAINS WIRING (Existing in machine)                  │
    │                                                                                     │
    │   Machine Mains ────────────┬───────────────────┬──────────────────────────────────│
    │   Live (220V)               │                   │                                  │
    │                             ▼                   ▼                                  │
    │                        SSR1 (L)            SSR2 (L)                                │
    │                                                                                     │
    │   Heater Return ◄───── Brew Heater ◄───── SSR1 (T)                                 │
    │   to Neutral           Element                                                     │
    │                                                                                     │
    │   Heater Return ◄───── Steam Heater ◄──── SSR2 (T)                                 │
    │   to Neutral           Element                                                     │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘

    ═══════════════════════════════════════════════════════════════════════════════════════
                               SSR CONNECTION QUICK REFERENCE
    ═══════════════════════════════════════════════════════════════════════════════════════

    │ Function      │ PCB Terminal │ Wire To        │ Signal Type  │ Wire Gauge │
    │───────────────│──────────────│────────────────│──────────────│────────────│
    │ SSR1 Ctrl (+) │ J26-15       │ SSR1 DC (+)    │ +5V DC       │ 22-26 AWG  │
    │ SSR1 Ctrl (-) │ J26-16       │ SSR1 DC (-)    │ Switched GND │ 22-26 AWG  │
    │ SSR2 Ctrl (+) │ J26-17       │ SSR2 DC (+)    │ +5V DC       │ 22-26 AWG  │
    │ SSR2 Ctrl (-) │ J26-18       │ SSR2 DC (-)    │ Switched GND │ 22-26 AWG  │
    │───────────────│──────────────│────────────────│──────────────│────────────│
    │ SSR1 Load (L) │ NOT ON PCB   │ Machine Live   │ 220V AC      │ 14-16 AWG  │
    │ SSR1 Load (T) │ NOT ON PCB   │ Brew Heater    │ 220V AC      │ 14-16 AWG  │
    │ SSR2 Load (L) │ NOT ON PCB   │ Machine Live   │ 220V AC      │ 14-16 AWG  │
    │ SSR2 Load (T) │ NOT ON PCB   │ Steam Heater   │ 220V AC      │ 14-16 AWG  │

    ⚠️ KEY POINTS FOR HARDWARE ENGINEER:
    ─────────────────────────────────────
    1. J26 pins 15-18 carry ONLY 5V DC control signals (<20mA per channel)
    2. SSR AC terminals connect to EXISTING machine wiring, NOT to this PCB
    3. This PCB has NO high-current traces for heaters
    4. External SSRs must be mounted on heatsink (25A rating handles 1300W load with margin)
    5. SSR internal optocoupler provides 3kV isolation between control and load sides

```

## 4.2 SSR Trigger Circuit (PCB Detail)

```
                            SSR TRIGGER CIRCUIT (On-PCB)
    ════════════════════════════════════════════════════════════════════════════

    (Identical driver circuit for SSR1 and SSR2)

    IMPORTANT: External SSR has internal current limiting - NO series resistor needed!
    External SSR input spec: 4-32V DC (KS15 D-24Z25-LQ)

                                        +5V (from PCB 5V rail)
                                         │
               ┌─────────────────────────┴─────────────────────────┐
               │                                                   │
               │                                              ┌────┴────┐
               │                                              │  330Ω   │
               │                                              │ R34/35  │
               │                                              │ (LED)   │
               │                                              └────┬────┘
               │                                                   │
               │                                              ┌────┴────┐
               │                                              │   LED   │
               │                                              │ Orange  │
               │                                              │ LED5/6  │
               │                                              │ (Status)│
               │                                              └────┬────┘
               │                                                   │
               ▼                                                   │
    ┌──────────────────────┐                                       │
    │     J26-15 or J26-17 │ ◄──── Screw terminal on PCB edge      │
    │        (SSR+)        │       Wire to External SSR (+)        │
    │      +5V output      │                                       │
    └──────────┬───────────┘                                       │
               │                                                   │
               │                                                   │
               ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·│·  ·
               :           EXTERNAL WIRING (to SSR module)         :
               :                    ~50cm cable                    :
               :                                                   :
               :    ┌───────────────────────────────────────┐      :
               :    │   EXTERNAL SSR MODULE                 │      :
               :    │   (KS15 D-24Z25-LQ, mounted on        │      :
               :    │    heatsink inside machine)           │      :
               :    │                                       │      :
               :    │   ┌─────┐ DC Control ┌─────┐          │      :
               :    │   │ (+) │◄───────────│ (-) │          │      :
               :    │   └──┬──┘            └──┬──┘          │      :
               :    │      │    (Internal     │             │      :
               :    │      │   LED+resistor)  │             │      :
               :    │      │                  │             │      :
               :    │   ═══╪══════════════════╪═══          │      :
               :    │   ║  │  OPTOCOUPLER &   │  ║          │      :
               :    │   ║  │  TRIAC (3kV ISO) │  ║          │      :
               :    │   ═══╪══════════════════╪═══          │      :
               :    │      │                  │             │      :
               :    │   ┌──┴──┐  AC Load   ┌──┴──┐          │      :
               :    │   │ (L) │            │ (T) │          │      :
               :    │   └──┬──┘            └──┬──┘          │      :
               :    │      │                  │             │      :
               :    └──────┼──────────────────┼─────────────┘      :
               :           │                  │                    :
               :           │                  ▼                    :
               :           │           To Heater Element           :
               :           │           (NOT connected to PCB)      :
               :           ▼                                       :
               :      From Machine Mains Live                      :
               :      (NOT connected to PCB)                       :
               ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·  ·
                                                                   │
    ┌──────────────────────┐                                       │
    │     J26-16 or J26-18 │ ◄──── Screw terminal on PCB edge      │
    │        (SSR-)        │       Wire to External SSR (-)        │
    │    Switched ground   │                                       │
    └──────────┬───────────┘                                       │
               │                                                   │
               └───────────────────────────────────────────────────┤
                                                                   │
                                                              ┌────┴────┐
                                                              │    C    │
                                                              │ Q5 or Q6│  MMBT2222A
                                                              │   NPN   │  SOT-23
                                                              │    E    │
                                                              └────┬────┘
                                                                   │
    GPIO (13 or 14) ──────[470Ω R24/25]────────────────────────────┤►B (Base)
                                  │                                │
                             ┌────┴────┐                          ─┴─
                             │  4.7kΩ  │ Pull-down                GND
                             │ R14/15  │ (RP2350 E9 errata)
                             └────┬────┘
                                 ─┴─
                                 GND

    ═══════════════════════════════════════════════════════════════════════════
                                 OPERATION
    ═══════════════════════════════════════════════════════════════════════════

    │ GPIO State │ Transistor │ SSR- Pin     │ SSR State │ Heater   │
    │────────────│────────────│──────────────│───────────│──────────│
    │ LOW (0V)   │ OFF        │ Floating     │ OFF       │ OFF      │
    │ HIGH (3.3V)│ ON         │ GND (via Vce)│ ON        │ ON       │

    When GPIO goes HIGH:
    1. Transistor Q5/Q6 saturates (Vce ≈ 0.2V)
    2. Current flows: +5V → J26-15 → External SSR (+) → SSR (-) → J26-16 → Q5 → GND
    3. External SSR internal LED lights (about 10-15mA)
    4. SSR's optocoupler triggers internal TRIAC
    5. TRIAC closes AC circuit: Mains Live → SSR → Heater → Neutral
    6. On-board orange LED (LED5/6) also lights to indicate SSR is active

    Voltage across External SSR = 5V - Vce(sat) = 5V - 0.2V = 4.8V ✓
    (Exceeds minimum 4V trigger requirement of KS15 D-24Z25-LQ)


    ═══════════════════════════════════════════════════════════════════════════
                         J26 TERMINAL BLOCK LAYOUT (SSR Section)
    ═══════════════════════════════════════════════════════════════════════════

    J26 Pins 15-18 (SSR Control - part of 18-position unified terminal block):

         ┌────────────────────────────────────────────────────┐
         │   ...   │  15   │  16   │  17   │  18   │         │
         │         │ SSR1+ │ SSR1- │ SSR2+ │ SSR2- │         │
         │         │  +5V  │ TRIG  │  +5V  │ TRIG  │         │
         └─────────┴───┬───┴───┬───┴───┬───┴───┬───┴─────────┘
                       │       │       │       │
                       │       │       │       └─► To SSR2 (-) terminal
                       │       │       └─────────► To SSR2 (+) terminal
                       │       └─────────────────► To SSR1 (-) terminal
                       └─────────────────────────► To SSR1 (+) terminal

    Wire Colors (Suggested):
    ────────────────────────
    • SSR+ wires: RED (both can share same wire gauge, 22-26 AWG)
    • SSR- wires: BLACK or BLUE


    ═══════════════════════════════════════════════════════════════════════════
                              COMPONENT VALUES
    ═══════════════════════════════════════════════════════════════════════════

    EXTERNAL COMPONENTS (User provides):
    ────────────────────────────────────
    SSR1, SSR2: KS15 D-24Z25-LQ (25A, 4-32V DC input, 24-280V AC output)
                Or equivalent: Crydom D2425, Fotek SSR-25DA
                Mount on aluminum heatsink (>100cm² per SSR for 10A continuous)

    ON-PCB COMPONENTS:
    ──────────────────
    Q5:     MMBT2222A, SOT-23 (SSR1 driver transistor)
    Q6:     MMBT2222A, SOT-23 (SSR2 driver transistor)
    R24:    470Ω 5% 0805 (Q5 base drive, GPIO13)
    R25:    470Ω 5% 0805 (Q6 base drive, GPIO14)
    R34:    330Ω 5% 0805 (LED5 current limit, ~8mA)
    R35:    330Ω 5% 0805 (LED6 current limit, ~8mA)
    R14:    4.7kΩ 5% 0805 (Q5 base pull-down, RP2350 errata E9)
    R15:    4.7kΩ 5% 0805 (Q6 base pull-down, RP2350 errata E9)
    LED5:   Orange 0805, Vf~2.0V (SSR1 status indicator)
    LED6:   Orange 0805, Vf~2.0V (SSR2 status indicator)

    GPIO ASSIGNMENTS:
    ─────────────────
    GPIO13 → SSR1 (Brew Heater)
    GPIO14 → SSR2 (Steam Heater)
```

---

# Sheet 5: Sensor Inputs

## 5.1 NTC Thermistor Inputs

**Multi-Machine Compatibility:** Use **solder jumpers JP1/JP2** to switch NTC configurations.

| Machine Brand     | NTC @ 25°C | JP1       | JP2       | Effective R1 | Effective R2 |
| ----------------- | ---------- | --------- | --------- | ------------ | ------------ |
| **ECM, Profitec** | 50kΩ       | **OPEN**  | **OPEN**  | 3.3kΩ        | 1.2kΩ        |
| Rocket, Rancilio  | 10kΩ       | **CLOSE** | **CLOSE** | ~1kΩ         | ~430Ω        |
| Gaggia            | 10kΩ       | **CLOSE** | **CLOSE** | ~1kΩ         | ~430Ω        |

```
                        NTC THERMISTOR INPUT CIRCUITS
    ════════════════════════════════════════════════════════════════════════════

    ⚠️ EACH SENSOR HAS DIFFERENT PULL-UP - OPTIMIZED FOR TARGET TEMPERATURE
    ⚠️ JP1/JP2 SOLDER JUMPERS SELECT NTC TYPE (NO RESISTOR SWAPPING NEEDED)

    BREW NTC (GPIO26/ADC0)                  STEAM NTC (GPIO27/ADC1)
    Target: 90-100°C                        Target: 125-150°C

         +3.3V_ANALOG                            +3.3V_ANALOG
              │                                       │
         ┌────┴────┐                             ┌────┴────┐
         │  3.3kΩ  │  R1 (Brew)                  │  1.2kΩ  │  R2 (Steam)
         │  ±1%    │  Optimized for 93°C         │  ±1%    │  Optimized for 135°C
         └────┬────┘                             └────┬────┘
              │                                       │
   J26-8 ─────┼────┬───────► GPIO26          J26-10 ──┼────┬───────► GPIO27
              │    │         (ADC0)                   │    │         (ADC1)
         ┌────┴────┐  ┌────┴────┐             ┌────┴────┐  ┌────┴────┐
         │   NTC   │  │  1kΩ    │             │   NTC   │  │  1kΩ    │
         │  50kΩ   │  │  R5     │             │  50kΩ   │  │  R6     │
         │  @25°C  │  └────┬────┘             │  @25°C  │  └────┬────┘
         └────┬────┘       │                  └────┬────┘       │
              │       ┌────┴────┐                  │       ┌────┴────┐
              │       │  100nF  │                  │       │  100nF  │
              │       │   C8    │                  │       │   C9    │
              │       └────┬────┘                  │       └────┬────┘
              │            │                       │            │
              └────────────┴─── GND                └────────────┴─── GND

    ESD Protection (both channels):
    ────────────────────────────────
    J26-8/10 (NTC) ┬──────────────────────────────────────────► To ADC
                   │
              ┌────┴────┐
              │ D10/D11 │  PESD5V0S1BL (ESD clamp)
              └────┬────┘
                  ─┴─
                  GND

    Component Values:
    ─────────────────
    R1:      3.3kΩ ±1%, 0805 (Brew NTC pull-up, always populated)
    R1A:     1.5kΩ ±1%, 0805 (Brew parallel, enabled by JP1)
    R2:      1.2kΩ ±1%, 0805 (Steam NTC pull-up, always populated)
    R2A:     680Ω ±1%, 0805 (Steam parallel, enabled by JP2)
    R5-R6:   1kΩ 1%, 0805 (series protection)
    C8-C9:   100nF 25V, 0805, Ceramic
    D10-D11: PESD5V0S1BL, SOD-323 (bidirectional ESD clamp)
    JP1:     Solder jumper (OPEN=50kΩ NTC, CLOSE=10kΩ NTC)
    JP2:     Solder jumper (OPEN=50kΩ NTC, CLOSE=10kΩ NTC)

    JUMPER MATH:
    • JP1 CLOSED: R1 || R1A = 3.3kΩ || 1.5kΩ ≈ 1.03kΩ (for 10kΩ NTC)
    • JP2 CLOSED: R2 || R2A = 1.2kΩ || 680Ω ≈ 434Ω (for 10kΩ NTC)

    Default: JP1/JP2 OPEN = ECM/Profitec (50kΩ NTC)
    For Rocket/Gaggia: Close JP1 and JP2 with solder bridge

    Resolution at Target Temps:
    ───────────────────────────
    • Brew (93°C):  ~31 ADC counts/°C → 0.032°C resolution
    • Steam (135°C): ~25 ADC counts/°C → 0.04°C resolution
```

## 5.2 Pressure Transducer Input (J26 Pin 12-14 - Amplified 0.5-4.5V)

**⚠️ SENSOR RESTRICTION:** Circuit designed for **0.5-4.5V ratiometric ONLY**.

- ✅ 3-wire sensors (5V, GND, Signal) like YD4060 or automotive pressure sensors
- ✅ 0.5V offset = broken wire detection (0.0V = fault, 0.5V = 0 bar)
- ❌ 4-20mA current loop sensors (require different circuit)

```
                    PRESSURE TRANSDUCER INPUT (AMPLIFIED TYPE)
    ════════════════════════════════════════════════════════════════════════════

    J26-12/13/14: Pressure transducer (part of unified 18-pos terminal)

                            +5V
                             │
    J26-12 (P-5V)────────────┴────────────────────────────────── Transducer VCC
    (5V)                                                         (Red wire)

    J26-13 (P-GND)─────────────────────────────────────────────── Transducer GND
    (GND)           │                                            (Black wire)
                   ─┴─
                   GND

    J26-14 (P-SIG)──────────────────────┬─────────────────────── Transducer Signal
    (Signal)                            │                        (Yellow/White wire)
    0.5V - 4.5V                         │
                                   ┌────┴────┐
                                   │  5.6kΩ  │  R4 (Series) ±1%
                                   │         │
                                   └────┬────┘
                                        │
                                        ├──────────────┬───────────────────► GPIO28
                                        │              │                     (ADC2)
                                   ┌────┴────┐    ┌────┴────┐
                                   │  10kΩ   │    │  100nF  │  LP Filter
                                   │   R3    │    │   C11   │
                                   │   ±1%   │    └────┬────┘
                                   └────┬────┘         │
                                        │              │
                                       ─┴─            ─┴─
                                       GND            GND

    Voltage Divider Calculation (OPTIMIZED for 3.0V ADC reference):
    ─────────────────────────────────────────────────────────────────
    Ratio = R3 / (R3 + R4) = 10k / (10k + 5.6k) = 0.641

    Input 0.5V  → Output 0.32V → ADC ~437
    Input 2.5V  → Output 1.60V → ADC ~2185
    Input 4.5V  → Output 2.88V → ADC ~3940

    ⚠️ WHY 5.6kΩ (not 4.7kΩ)?
    With 4.7kΩ: V_max = 4.5V × 0.68 = 3.06V > 3.0V reference → SATURATES!
    With 5.6kΩ: V_max = 4.5V × 0.641 = 2.88V < 3.0V reference → LINEAR

    Resolution: 0.0046 bar/count (full 16 bar range, no saturation)

    Component Values:
    ─────────────────
    R3: 10kΩ ±1%, 0805 (to GND)
    R4: 5.6kΩ ±1%, 0805 (series, from signal) - prevents ADC saturation
    C11: 100nF 25V Ceramic, 0805

    Selected Transducer: YD4060 Series
    ───────────────────────────────────
    - Pressure Range: 0-1.6 MPa (0-16 bar)
    - Output: 0.5-4.5V ratiometric
    - Supply: 5VDC, ≤3mA
    - Thread: 1/8" NPT (use adapter for espresso machine)
    - Wiring: Red=5V, Black=GND, Yellow=Signal
```

## 5.3 Digital Switch Inputs (J26 - Low Voltage Only)

```
                        DIGITAL SWITCH INPUTS
    ════════════════════════════════════════════════════════════════════════════

    All LOW VOLTAGE digital switch inputs are consolidated in J26 (8-position screw terminal).
    (Water Reservoir, Tank Level, Level Probe, Brew Handle - ALL 3.3V LOGIC)

    ⚠️ J26 is for LOW VOLTAGE (3.3V) inputs ONLY!
    ⚠️ 220V relay outputs (K1, K2, K3) use 6.3mm spade terminals (J2, J3, J4)

    J26 LOW-VOLTAGE SWITCH INPUT TERMINAL BLOCK (Phoenix MKDS 1/8-5.08):
    ─────────────────────────────────────────────────────────────────────
    ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
    │  1  │  2  │  3  │  4  │  5  │  6  │  7  │  8  │
    │ S1  │S1-G │ S2  │S2-G │ S3  │ S4  │S4-G │ GND │
    └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
     WtrRes GND  Tank  GND  Probe Brew  GND  Spare

    SWITCH INPUT CIRCUIT (S1, S2, S4):
    ───────────────────────────────────

                            +3.3V
                              │
                         ┌────┴────┐
                         │  10kΩ   │  Pull-up
                         │  R16-18 │  (or use Pico internal)
                         └────┬────┘
                              │
    J26 Pin 1/3/6  ───────────┼───────────────────────┬──────────────► GPIO 2/3/5
    (Switch Signal)           │                       │
                              │                  ┌────┴────┐
                         ┌────┴────┐             │  D12-14 │  ESD
                         │ Switch  │             │  PESD   │  Clamp
                         │  N.O.   │             └────┬────┘
                         └────┬────┘                  │
                              │                       │
    J26 Pin 2/4/7  ───────────┴───────────────────────┴──────────────── GND
    (Switch GND)

    J26 SIGNAL MAPPING (8-pin):
    ───────────────────────────
    Pin 1 (S1)  → GPIO2 (Water Reservoir Switch Signal)
    Pin 2 (S1-G)→ GND   (Water Reservoir Switch Return)
    Pin 3 (S2)  → GPIO3 (Tank Level Sensor Signal)
    Pin 4 (S2-G)→ GND   (Tank Level Sensor Return)
    Pin 5 (S3)  → GPIO4 (Steam Boiler Level Probe - via comparator)
    Pin 6 (S4)  → GPIO5 (Brew Handle Switch Signal)
    Pin 7 (S4-G)→ GND   (Brew Handle Switch Return)
    Pin 8 (GND) → GND   (Common Ground - spare terminal)


    Steam Boiler Level Probe (OPA342 + TLV3201 AC Sensing):
    ─────────────────────────────────────────────────────────

    ⚠️  AC sensing prevents electrolysis and probe corrosion!

    This circuit has 3 stages:
      1. Wien Bridge Oscillator (U6) - Generates ~1.6kHz AC signal
      2. Probe Interface - Sends AC to probe, receives attenuated signal
      3. Comparator (U7) - Converts analog signal to digital GPIO

    ═══════════════════════════════════════════════════════════════════════════
                    STAGE 1: WIEN BRIDGE OSCILLATOR (~1.6kHz)
    ═══════════════════════════════════════════════════════════════════════════

    CIRCUIT TOPOLOGY:
    ─────────────────
    Non-inverting amplifier with Wien bridge positive feedback network.
    Gain = 1 + R81/R82 = 3.13 (must be >3 for oscillation startup)

                                        +3.3V
                                          │
                                     ┌────┴────┐
                                     │  C60    │
                                     │  100nF  │ U6 VCC decoupling
                                     └────┬────┘
                                          │
                                          │ VCC (pin 7)
                                          │
                 WIEN BRIDGE           ┌──┴──────────────────┐
                 NETWORK               │                     │
                    │                  │    U6: OPA342       │
                    │            ┌─────┤(-)                  │
                    │            │     │     (Op-Amp)    OUT ├───────┬──► AC_OUT
                    │            │  ┌──┤(+)                  │       │
                    │            │  │  │                     │       │
                    │            │  │  │         GND (pin 4) │       │
                    │            │  │  └──────────┬──────────┘       │
                    │            │  │             │                  │
                    │            │  │            ─┴─                 │
                    │            │  │            GND                 │
                    │            │  │                                │
                    │   R81      │  │                                │
                    │   10kΩ     │  │                                │
    ┌───────────────┼────[████]──┘  │                                │
    │               │   (feedback)  │                                │
    │               │               │                                │
    │               │   R82         │                                │
    │               │   4.7kΩ       │                                │
    │               └────[████]─────┤ (gain set)                     │
    │                    │          │                                │
    │                   ─┴─         │                                │
    │                   GND         │                                │
    │                               │                                │
    │                    ┌──────────┘                                │
    │                    │  (+) INPUT NODE                           │
    │                    │                                           │
    │               ┌────┴────┐                                      │
    │               │   R84   │                                      │
    │               │  10kΩ   │  Wien bridge                         │
    │               │ (shunt) │  parallel RC                         │
    │               └────┬────┘                                      │
    │                    │                                           │
    │               ┌────┴────┐                                      │
    │               │   C62   │                                      │
    │               │  10nF   │                                      │
    │               │ (shunt) │                                      │
    │               └────┬────┘                                      │
    │                    │                                           │
    │                   ─┴─                                          │
    │                   GND                                          │
    │                                                                │
    │   ┌────────────────────────────────────────────────────────────┘
    │   │  FEEDBACK PATH (from AC_OUT back to + input)
    │   │
    │   │        R83              C61
    │   │       10kΩ             10nF
    │   └───────[████]───────────┤├───────────┐
    │           (series)        (series)      │
    │                                         │
    │                                         │
    └─────────────────────────────────────────┤
              (to + input node above)         │
                                              │
                                              │
                                              ↓
                                           AC_OUT
                                      (to Stage 2)

    WIEN BRIDGE SIGNAL FLOW:
    ────────────────────────
      AC_OUT ──► R83 ──► C61 ──┬──► (+) input
                               │
                          R84 ─┴─ C62
                               │
                              GND

      The RC network provides frequency-selective positive feedback.
      At f = 1/(2π×R×C) = 1/(2π×10kΩ×10nF) ≈ 1.6kHz, phase shift = 0°

    ═══════════════════════════════════════════════════════════════════════════
                    STAGE 2: PROBE INTERFACE & SIGNAL CONDITIONING
    ═══════════════════════════════════════════════════════════════════════════

    AC_OUT ─────────────────────────────────────────────────────────────────┐
    (from U6)                                                               │
                                                                            │
                     R85                                                    │
                    100Ω                                                    │
               ┌────[████]────┬──────────────────────► J26 Pin 5            │
               │  (current    │                        (LEVEL_PROBE S3)     │
               │   limit)     │                        Screw Terminal       │
               │              │                              │              │
               │         ┌────┴────┐                         │              │
               │         │   C64   │                    ┌────┴────┐         │
               │         │   1µF   │                    │  PROBE  │         │
               │         │   AC    │                    │   ~~~   │         │
               │         │coupling │                    │  Water  │         │
               │         └────┬────┘                    │  Level  │         │
               │              │                         │ Sensor  │         │
               │              │                         └────┬────┘         │
               │              ↓                              │              │
               │          AC_SENSE ◄─────────────────────────┘              │
               │         (attenuated                    Boiler Body         │
               │          if water                      (Grounded via       │
               │          present)                       Protective Earth)  │
               │              │                              │              │
               │              │                             ─┴─             │
               │              │                             GND             │
               │              │                             (PE)            │
               │              │                                             │
               └──────────────┼─────────────────────────────────────────────┘
                              │
                              ↓
                         (to Stage 3)

    PROBE OPERATION:
    ────────────────
      • R85 limits current to probe (protects circuit if probe shorts)
      • C64 provides AC coupling (blocks DC, passes AC signal)
      • When water touches probe: AC signal is attenuated (water = resistive load)
      • When no water: AC signal passes with minimal attenuation

    ═══════════════════════════════════════════════════════════════════════════
                    STAGE 3: COMPARATOR & DIGITAL OUTPUT
    ═══════════════════════════════════════════════════════════════════════════

    AC_SENSE ───────────────────────────────────────────────────────────────┐
    (from Stage 2)                                                          │
                                                                            │
                                            +3.3V                           │
                                              │                             │
                                         ┌────┴────┐                        │
                                         │   C63   │                        │
                                         │  100nF  │ U7 VCC decoupling      │
                                         └────┬────┘                        │
                                              │                             │
                                              │ VCC                         │
                                              │                             │
                      R86               ┌─────┴─────────────────┐           │
                     10kΩ               │                       │           │
               ┌─────[████]──────┬──────┤(+)   U7: TLV3201  OUT ├───► GPIO4 │
               │    (bias)       │      │      (Comparator)     │    (Pico) │
               │                 │   ┌──┤(-)                    │           │
               │            ┌────┴───┤  │                       │           │
               │            │   C65  │  │          GND          │           │
               │            │  100nF │  │           │           │           │
               │            │(filter)│  └───────────┼───────────┘           │
               │            └────┬───┘              │                       │
               │                 │                 ─┴─                      │
               │                ─┴─                GND                      │
               │                GND                                         │
               │                                                            │
               │                                                            │
               │   R89                                                      │
               │   1MΩ                                                      │
               │   [████]◄──────────────────────────────────────── GPIO4    │
               │(hysteresis)                                     (feedback) │
               │     │                                                      │
               └─────┴──────────────────────────────────────────────────────┘
                     │
                     └──► to (+) input (positive feedback for hysteresis)


    VREF VOLTAGE DIVIDER (sets comparator threshold):
    ─────────────────────────────────────────────────

          +3.3V ─────┬─────────────────────────────────────────────┐
                     │                                             │
                ┌────┴────┐                                        │
                │   R87   │                                        │
                │  100kΩ  │                                        │
                │   1%    │                                        │
                └────┬────┘                                        │
                     │                                             │
                     ├───────────────────────► VREF ──► U7 (-) input
                     │                         (~1.65V)            │
                ┌────┴────┐                                        │
                │   R88   │                                        │
                │  100kΩ  │                                        │
                │   1%    │                                        │
                └────┬────┘                                        │
                     │                                             │
                    ─┴─                                            │
                    GND                                            │
                                                                   │
          VREF = 3.3V × R88/(R87+R88) = 3.3V × 100k/200k = 1.65V   │
                                                                   │
               ─────────────────────────────────────────────────────┘

    ═══════════════════════════════════════════════════════════════════════════
                              LOGIC STATES
    ═══════════════════════════════════════════════════════════════════════════

      Water BELOW probe:
        → AC signal passes through unattenuated
        → AC_SENSE amplitude > VREF (1.65V)
        → Comparator output HIGH
        → GPIO4 = HIGH → LOW WATER ALARM / UNSAFE FOR HEATER

      Water AT or ABOVE probe:
        → AC signal attenuated by water conductivity
        → AC_SENSE amplitude < VREF (1.65V)
        → Comparator output LOW
        → GPIO4 = LOW → LEVEL OK

    ═══════════════════════════════════════════════════════════════════════════
                              COMPONENT VALUES
    ═══════════════════════════════════════════════════════════════════════════

    OSCILLATOR (U6):
    ────────────────
    U6:   OPA342UA, SOIC-8 (or OPA2342UA, use one section)
          - Rail-to-rail input/output, single supply
          - Alt: OPA207 for lower noise
    R81:  10kΩ 1%, 0805 (feedback resistor, sets gain with R82)
    R82:  4.7kΩ 1%, 0805 (gain setting, to GND)
    R83:  10kΩ 1%, 0805 (Wien bridge series R)
    R84:  10kΩ 1%, 0805 (Wien bridge shunt R)
    C60:  100nF 25V, 0805 (U6 VCC decoupling)
    C61:  10nF 50V, 0805 (Wien bridge series C)
    C62:  10nF 50V, 0805 (Wien bridge shunt C)

    PROBE INTERFACE:
    ────────────────
    R85:  100Ω 5%, 0805 (probe current limit, protects against shorts)
    C64:  1µF 25V, 0805 (AC coupling capacitor)

    COMPARATOR (U7):
    ────────────────
    U7:   TLV3201AIDBVR, SOT-23-5 (push-pull output, rail-to-rail)
    R86:  10kΩ 5%, 0805 (AC_SENSE input bias)
    R87:  100kΩ 1%, 0805 (VREF divider upper)
    R88:  100kΩ 1%, 0805 (VREF divider lower)
    R89:  1MΩ 5%, 0805 (hysteresis feedback)
    C63:  100nF 25V, 0805 (U7 VCC decoupling)
    C65:  100nF 25V, 0805 (AC_SENSE low-pass filter)

    ═══════════════════════════════════════════════════════════════════════════
                              DESIGN CALCULATIONS
    ═══════════════════════════════════════════════════════════════════════════

    OSCILLATOR FREQUENCY:
    ─────────────────────
    f = 1 / (2π × R × C) = 1 / (2π × 10kΩ × 10nF) = 1.59 kHz ≈ 1.6 kHz

    OSCILLATOR GAIN:
    ────────────────
    A_CL = 1 + (R81/R82) = 1 + (10kΩ/4.7kΩ) = 3.13

    Wien bridge attenuation β = 1/3 at resonance
    Loop gain = A_CL × β = 3.13 × (1/3) = 1.04 > 1 ✓

    Barkhausen Criterion satisfied: Loop gain >1 ensures startup.

    ⚠️ WHY 1.6kHz (NOT 160Hz)?
    ──────────────────────────
    Lower frequencies allow electrochemical reactions (electrolysis) during
    each AC half-cycle, corroding the probe. Industry standard: 1-10 kHz.
    At 1.6kHz, probe life extends from months to 5-10+ years.

    ⚠️ NOTE ON WAVEFORM:
    ────────────────────
    Without AGC (automatic gain control), the oscillator produces a clipped
    waveform (rail-to-rail square-ish wave) rather than pure sine wave.
    This is ACCEPTABLE for conductivity sensing - we only need AC excitation
    at the correct frequency, not a pure sinusoid.

    ═══════════════════════════════════════════════════════════════════════════
                              PCB LAYOUT NOTES
    ═══════════════════════════════════════════════════════════════════════════

    ⚠️ GUARD RING REQUIRED:
    ───────────────────────
    The trace from J26 Pin 5 (Level Probe) is HIGH IMPEDANCE and will pick
    up 50/60Hz mains hum if not properly shielded.

    REQUIRED:
      • Surround probe trace with GND guard ring on BOTH PCB layers
      • Place U6 (OPA342) as CLOSE as possible to J26 terminal (<2cm trace)
      • Avoid routing near relay coils or mains traces
      • Keep C60 and C63 within 5mm of their respective IC VCC pins

    ═══════════════════════════════════════════════════════════════════════════

    Other Switches (unchanged):
    ───────────────────────────
    R16-18: 10kΩ 5%, 0805 (or use Pico internal pull-ups)
    D12-14: PESD5V0S1BL, SOD-323
```

---

# Sheet 6: Communication Interfaces

## 6.1 ESP32 Display Module Interface

```
                    ESP32 DISPLAY MODULE INTERFACE
    ════════════════════════════════════════════════════════════════════════════

    J15: JST-XH 8-pin connector (B8B-XH-A) - Includes brew-by-weight support

                                            +5V
                                             │
                                        ┌────┴────┐
                                        │  100µF  │  Bulk cap
                                        │   C13   │  for ESP32
                                        └────┬────┘
                                             │
    J15 Pin 1 (5V) ──────────────────────────┴──────────────────────────────────

    J15 Pin 2 (GND) ────────────────────────────────────────────────────── GND


    UART TX (GPIO0 → ESP32 RX):
    ───────────────────────────
                                    ┌────┐
    GPIO0 ──────────────────────────┤ 1kΩ├─────────────────── J15 Pin 3 (TX)
    (UART0_TX)                      │R40 │ (ECO-03: 5V tolerance)
                                    └────┘

    UART RX (ESP32 TX → GPIO1):
    ───────────────────────────
                                    ┌────┐
    GPIO1 ◄─────────────────────────┤ 1kΩ├─────────────────── J15 Pin 4 (RX)
    (UART0_RX)                      │R41 │ (ECO-03: 5V tolerance)
                                    └────┘


    ════════════════════════════════════════════════════════════════════════
    ESP32 → PICO CONTROL (OTA Firmware Updates)
    ════════════════════════════════════════════════════════════════════════

    ESP32 updates ITSELF via WiFi OTA (standard ESP-IDF).
    ESP32 updates PICO via UART software bootloader + RUN pin control.
    Pico has no WiFi - relies on ESP32 as firmware update gateway.


    PICO RUN Control (J15 Pin 5 → Pico RUN):
    ─────────────────────────────────────────

                                 +3.3V
                                    │
                               ┌────┴────┐
                               │  10kΩ   │  R71
                               │ Pull-up │  (Pico has internal, add external)
                               └────┬────┘
                                    │
    Pico RUN pin ◄──────────────────┴────────────────── J15 Pin 5 (RUN)
                                                              │
                                                              │
                                                         ESP32 GPIO
                                                      (Open-Drain Output)

    • ESP32 pulls LOW → Pico resets
    • ESP32 releases (high-Z) → Pico runs normally


    SPARE1 Signal (J15 Pin 6 → Available):
    ───────────────────────────────────────

    (No connection) ◄──────────────────────────────── J15 Pin 6 (SPARE1)
                                                              │
                                                              │
                                                         ESP32 GPIO9

    • Available for future expansion


    WEIGHT_STOP Signal (J15 Pin 7 → GPIO21) - BREW BY WEIGHT:
    ──────────────────────────────────────────────────────────

    GPIO21 ◄────────────────────────┬────────────────── J15 Pin 7 (WEIGHT_STOP)
    (Input)                         │                         │
                               ┌────┴────┐                    │
                               │  R73    │               ESP32 GPIO
                               │  4.7kΩ  │             (Push-Pull Output)
                               │Pull-down│
                               │(E9 fix) │
                               └────┬────┘
                                    │
                                   ─┴─
                                   GND
                                (Normally LOW - no brew stop)

    • R73 pull-down keeps GPIO21 LOW when ESP32 is not driving the line
    • ESP32 connects to Bluetooth scale (Acaia, Decent, etc.)
    • ESP32 monitors weight during brew
    • When target weight reached → ESP32 drives Pin 7 HIGH (overrides pull-down)
    • Pico detects HIGH on GPIO21 → Stops pump (K2) immediately
    • ESP32 drives Pin 7 LOW → Ready for next brew
    • If ESP32 disconnected/reset → GPIO21 stays LOW (safe state, no false triggers)


    SPARE1 Signal (J15 Pin 6 → GPIO16):
    ────────────────────────────────────

    GPIO16 ◄────────────────────────┬────────────────── J15 Pin 6 (SPARE1)
    (I/O)                           │                         │
                               ┌────┴────┐                    │
                               │  R74    │               ESP32 GPIO9
                               │  4.7kΩ  │             (Push-Pull I/O)
                               │Pull-down│
                               │(E9 fix) │
                               └────┬────┘
                                    │
                                   ─┴─
                                   GND
                                (Normally LOW when unused)

    • General-purpose spare I/O between ESP32 and Pico
    • 4.7kΩ pull-down ensures defined LOW state when unused (RP2350 E9 errata)


    SPARE2 Signal (J15 Pin 8 → GPIO22):
    ────────────────────────────────────

    GPIO22 ◄────────────────────────┬────────────────── J15 Pin 8 (SPARE2)
    (I/O)                           │                         │
                               ┌────┴────┐                    │
                               │  R75    │               ESP32 GPIO22
                               │  4.7kΩ  │             (Push-Pull I/O)
                               │Pull-down│
                               │(E9 fix) │
                               └────┬────┘
                                    │
                                   ─┴─
                                   GND
                                (Normally LOW when unused)

    • General-purpose spare I/O between ESP32 and Pico
    • 4.7kΩ pull-down ensures defined LOW state when unused (RP2350 E9 errata)


    OTA Update Sequence:
    ─────────────────────
    1. ESP32 downloads Pico firmware via WiFi
    2. ESP32 sends "ENTER_BOOTLOADER" command via UART
    3. Pico custom bootloader acknowledges
    4. ESP32 streams firmware via UART (115200 baud)
    5. Pico writes to flash
    6. ESP32 pulses RUN LOW to reset Pico
    7. Pico boots with new firmware

    Recovery (if Pico firmware corrupted):
    ───────────────────────────────────────
    1. Hold BOOTSEL button on Pico module
    2. Press RUN button (or power cycle)
    3. Release BOOTSEL - Pico appears as USB drive "RPI-RP2"
    4. Drag-drop .uf2 firmware file to the USB drive


    J15 Pinout Summary (8-pin):
    ───────────────────────────
    Pin 1: 5V      → ESP32 VIN (power)
    Pin 2: GND     → Common ground
    Pin 3: TX      → ESP32 RX (from Pico GPIO0)
    Pin 4: RX      ← ESP32 TX (to Pico GPIO1)
    Pin 5: RUN     ← ESP32 GPIO8 (to Pico RUN pin)
    Pin 6: SPARE1  ↔ ESP32 GPIO9 ↔ Pico GPIO16 (spare I/O)
    Pin 7: WGHT    ← ESP32 GPIO10 (to Pico GPIO21) - Brew-by-weight stop signal
    Pin 8: SPARE2  ↔ ESP32 GPIO22 ↔ Pico GPIO22 (spare I/O)

    Component Values:
    ─────────────────
    R40-41: 1kΩ 5%, 0805 (UART series protection - 5V tolerance per ECO-03)
    R71:    10kΩ 5%, 0805 (RUN pull-up)
    R73:    4.7kΩ 5%, 0805 (WEIGHT_STOP pull-down, RP2350 E9)
    R74:    4.7kΩ 5%, 0805 (SPARE1 pull-down, RP2350 E9)
    R75:    4.7kΩ 5%, 0805 (SPARE2 pull-down, RP2350 E9)
    C13:    100µF 10V, Electrolytic (ESP32 power decoupling)
```

## 6.2 Service/Debug Port

```
                        SERVICE/DEBUG PORT
    ════════════════════════════════════════════════════════════════════════════

    J16: 4-pin 2.54mm header

                            +3.3V
                              │
    J16 Pin 1 (3V3) ──────────┴───────────────────────────────────────────────

    J16 Pin 2 (GND) ──────────────────────────────────────────────────── GND

    SERVICE PORT (J16) - Shared with ESP32 on GPIO0/1:
    ─────────────────────────────────────────────────
    ⚠️ DISCONNECT ESP32 (J15) BEFORE USING SERVICE PORT

                                    ┌────┐
    GPIO0 ──────────────────────────┤ 1kΩ├──┬──────────────── J15 Pin 3 (ESP32 RX)
    (UART0_TX)                      │R42 │  │ (ECO-03: 5V tolerance)
                                    └────┘  ├──────────────── J16 Pin 3 (Service TX)
                                            │
                                       ┌────┴────┐
                                       │  D23    │  3.3V Zener
                                       │BZT52C3V3│
                                       └────┬────┘
                                           ─┴─
                                           GND

                                    ┌────┐
    GPIO1 ◄─────────────────────────┤ 1kΩ├──┬──────────────── J15 Pin 4 (ESP32 TX)
    (UART0_RX)                      │R43 │  │ (ECO-03: 5V tolerance)
                                    └────┘  ├──────────────── J16 Pin 4 (Service RX)
                                            │
                                       ┌────┴────┐
                                       │  D24    │  3.3V Zener
                                       │BZT52C3V3│
                                       └────┬────┘
                                           ─┴─
                                           GND

    I2C ACCESSORY PORT (J23) - GPIO8/9:
    ────────────────────────────────────
                  3.3V
                   │
            ┌──────┴──────┐
         [4.7kΩ]       [4.7kΩ]
          R50           R51
            │             │
    GPIO8 ──┴─────────────┼─────────────── J23 Pin 3 (SDA)
    (I2C0_SDA)            │
    GPIO9 ────────────────┴─────────────── J23 Pin 4 (SCL)
    (I2C0_SCL)

    Configuration:
    ──────────────
    Default baud rate: 115200, 8N1

    Component Values:
    ─────────────────
    R42:  1kΩ 5%, 0805 (Service TX series - 5V tolerance per ECO-03)
    R43:  1kΩ 5%, 0805 (Service RX series - 5V tolerance per ECO-03)
    D23:  BZT52C3V3 3.3V Zener, SOD-123 (TX overvoltage clamp)
    D24:  BZT52C3V3 3.3V Zener, SOD-123 (RX overvoltage clamp)

    Silkscreen: "3V3 GND TX RX" or "DEBUG"
```

---

# Sheet 7: User Interface

## 7.1 Status LED

```
                            STATUS LED
    ════════════════════════════════════════════════════════════════════════════

                                    ┌────────┐
    GPIO15 ─────────────────────────┤  330Ω  ├──────────────┬────────────────
                                    │  R50   │              │
                                    └────────┘         ┌────┴────┐
                                                       │   LED   │
                                                       │  Green  │
                                                       │  LED7   │
                                                       │  0805   │
                                                       └────┬────┘
                                                            │
                                                           ─┴─
                                                           GND

    Current: (3.3V - 2.0V) / 330Ω ≈ 4mA (clearly visible)

    NOTE: Green LED chosen over blue because:
    • Blue LEDs have Vf=3.0-3.4V, leaving only 0-0.3V margin with 3.3V supply
    • Green LEDs have Vf=1.8-2.2V, providing reliable ~4mA current
    • Green matches relay indicator LEDs for consistency

    Component Values:
    ─────────────────
    R50:  330Ω 5%, 0805
    LED7: Green 0805, Vf~2.0V
```

## 7.2 Buzzer

```
                            BUZZER (Passive)
    ════════════════════════════════════════════════════════════════════════════

    Using passive buzzer with PWM for variable tones:

                                    ┌────────────┐
    GPIO19 ─────────────────────────┤   100Ω     ├──────────────┐
    (PWM)                           │    R51     │              │
                                    └────────────┘              │
                                                           ┌────┴────┐
                                                           │ Passive │
                                                           │ Buzzer  │
                                                           │   BZ1   │
                                                           └────┬────┘
                                                                │
                                                               ─┴─
                                                               GND

    Component Values:
    ─────────────────
    R51: 100Ω 5%, 0805
    BZ1: CUI CEM-1203(42), 12mm, Passive Piezo

    PWM Frequencies:
    ────────────────
    2000-2500 Hz: Ready/confirmation beeps
    1000-1500 Hz: Warnings
    500 Hz:       Error
```

---

# Sheet 8: Power Metering (Universal External Interface)

## 8.1 Universal External Power Meter Interface

```
                    UNIVERSAL EXTERNAL POWER METER INTERFACE
    ════════════════════════════════════════════════════════════════════════════

    ✅ NO HV MEASUREMENT CIRCUITRY ON CONTROL PCB - METER HANDLES SENSING
    ✅ J24 provides L/N/PE pass-through to external meter (in existing HV zone)
    ✅ Supports TTL UART (direct) or RS485 (differential) meters
    ✅ Compatible with PZEM-004T, JSY-MK-163T/194T, Eastron SDM, and more

    ═══════════════════════════════════════════════════════════════════════════
    OFF-BOARD POWER METER DESIGN (User mounts module externally)
    ═══════════════════════════════════════════════════════════════════════════

    SYSTEM TOPOLOGY:
    ────────────────

    ┌────────────────────────────────────────────────────────────────────────┐
    │                         CONTROL PCB                                    │
    │                                                                        │
    │   J17 (JST-XH 6-pin)                                                   │
    │   ┌─────┬─────┬─────┬─────┬─────┬─────┐                               │
    │   │ 3V3 │ 5V  │ GND │ RX  │ TX  │DE/RE│                               │
    │   │  1  │  2  │  3  │  4  │  5  │  6  │                               │
    │   └──┬──┴──┬──┴──┬──┴──┬──┴──┬──┴──┬──┘                               │
    │      │     │     │     │     │     │                                   │
    │      │     │     │     │     │     └──► GPIO20 (RS485 DE/RE control)  │
    │      │     │     │     │     └────────► GPIO6 (UART TX to meter)       │
    │      │     │     │     └──────────────► GPIO7 (UART RX from meter)     │
    │      │     │     └────────────────────► GND                            │
    │      │     └──────────────────────────► 5V (for 5V meters)             │
    │      └────────────────────────────────► 3.3V (for 3.3V meters)         │
    │                                                                        │
    │   ═══════════════════════════════════════════════════════════════════  │
    │                                                                        │
    │   RS485 TRANSCEIVER (U8: MAX3485) - For Industrial Meters              │
    │   ───────────────────────────────────────────────────────              │
    │                                                                        │
    │                               +3.3V                                    │
    │                                 │                                      │
    │                            ┌────┴────┐                                 │
    │                            │  100nF  │ C70                             │
    │                            └────┬────┘                                 │
    │                            VCC──┤                                      │
    │                                 │  U8: MAX3485                         │
    │                       ┌─────────┴─────────┐                            │
    │   GPIO6 (TX) ────────►│ DI            A   │◄───► J17-4 (RS485 mode)    │
    │                       │                   │      or METER_RX (TTL)     │
    │   GPIO7 (RX) ◄────────│ RO            B   │◄───► J17-5 (RS485 mode)    │
    │                       │                   │      or METER_TX (TTL)     │
    │   GPIO20 (DE/RE) ────►│ DE/RE         GND │                            │
    │                       │                   │                            │
    │                       └─────────┬─────────┘                            │
    │                                ─┴─                                     │
    │                                GND                                     │
    │                                                                        │
    │   FAILSAFE BIASING:                                                      │
    │   ──────────────────                                                   │
    │   R93: 20kΩ pull-up on A line (to +3.3V)                               │
    │   R94: 20kΩ pull-down on B line (to GND)                               │
    │                                                                        │
    │   Defines bus idle state when no driver active, prevents noise         │
    │   from being interpreted as data in electrically noisy environment.    │
    │                                                                        │
    │   TERMINATION:                                                         │
    │   ─────────────                                                        │
    │   No termination resistor (not required for short cable runs)          │
    │                                                                        │
    └────────────────────────────────────────────────────────────────────────┘


    EXTERNAL POWER METER (User-mounted, off-board):
    ────────────────────────────────────────────────

    ┌────────────────────────────────────────────────────────────────────────┐
    │              EXTERNAL POWER METER MODULE                               │
    │              (PZEM-004T, JSY-MK-163T, Eastron SDM, etc.)              │
    │                                                                        │
    │   ┌──────────────────┐     ┌──────────────────────────────────────┐   │
    │   │   LV INTERFACE   │     │         HV INTERFACE                 │   │
    │   │   (JST or wire)  │     │    (Screw terminals on module)       │   │
    │   │                  │     │                                      │   │
    │   │   5V ◄───────────│     │   L ◄──────── Machine Mains Live     │   │
    │   │   GND ◄──────────│     │   N ◄──────── Machine Mains Neutral  │   │
    │   │   TX ────────────│     │                                      │   │
    │   │   RX ◄───────────│     │   CT+ ◄────┬───── CT Clamp           │   │
    │   │                  │     │   CT- ◄────┘      (on Live wire)     │   │
    │   └──────────────────┘     └──────────────────────────────────────┘   │
    │         │                              │                              │
    │         │ JST cable                    │ User wires directly          │
    │         │ to J17                       │ to mains                     │
    │         ▼                              ▼                              │
    │   CONTROL PCB                    MACHINE WIRING                       │
    │                                  (NOT through PCB)                    │
    │                                                                        │
    └────────────────────────────────────────────────────────────────────────┘


    Design Note: No HV measurement circuitry on control PCB
    ────────────────────────────────────────────────────────────
    • J24 routes L/N/PE to external meter (in PCB's existing HV zone)
    • CT clamp wires directly to meter module (not via this PCB)
    • Control PCB provides ONLY 5V/3.3V power and UART/RS485 data
    • User wires machine mains directly to external module terminals
    • CT clamp connects directly to external module (not through PCB)


    SUPPORTED MODULES:
    ──────────────────
    │ Module          │ Baud  │ Protocol   │ Interface │ CT Type        │
    │─────────────────│───────│────────────│───────────│────────────────│
    │ PZEM-004T V3    │ 9600  │ Modbus RTU │ TTL UART  │ Split-core 100A│
    │ JSY-MK-163T     │ 4800  │ Modbus RTU │ TTL UART  │ Internal shunt │
    │ JSY-MK-194T     │ 4800  │ Modbus RTU │ TTL UART  │ Dual-channel   │
    │ Eastron SDM120  │ 2400  │ Modbus RTU │ RS485     │ DIN-rail CT    │
    │ Eastron SDM230  │ 9600  │ Modbus RTU │ RS485     │ DIN-rail CT    │


    J17 PINOUT (JST-XH 6-pin):
    ──────────────────────────
    Pin 1: 3V3    → Power for 3.3V logic modules
    Pin 2: 5V     → Power for 5V modules (PZEM, JSY)
    Pin 3: GND    → System ground
    Pin 4: RX     → GPIO7 (from meter TX, via level shifter)
    Pin 5: TX     → GPIO6 (to meter RX, via 33Ω R44)
    Pin 6: DE/RE  → GPIO20 (RS485 direction control)


    5V to 3.3V Level Shifting (J17 RX Line):
    ─────────────────────────────────────────────────────
    Some power meters (PZEM, JSY, etc.) output 5V TTL. RP2350 is NOT 5V tolerant!
    Without level shifting, 5V signals cause GPIO damage over time.

    SOLUTION: Resistive voltage divider on J17-4 (RX input):

         J17 Pin 4 (5V from PZEM TX)
              │
         ┌────┴────┐
         │  2.2kΩ  │  R45 (upper)
         │   1%    │
         └────┬────┘
              │
              ├──────[33Ω R45B]─────────────► GPIO7 (PZEM_RX)
              │
         ┌────┴────┐
         │  3.3kΩ  │  R45A (lower)
         │   1%    │
         └────┬────┘
              │
             GND

    V_out = 5V × 3.3k / (2.2k + 3.3k) = 5V × 0.6 = 3.0V ✓


    COMPONENT VALUES:
    ─────────────────
    U8:     MAX3485ESA+ or SP3485EN-L/TR (SOIC-8 or SOT-23-8)
    D21:    SM712 (SOT-23) - RS485 A/B line TVS protection
    C70:    100nF 25V Ceramic, 0805 (U8 decoupling)
    R44:    33Ω 5%, 0805 (TX series protection)
    R45:    2.2kΩ 1%, 0805 (J17 RX 5V→3.3V level shift, upper)
    R45A:   3.3kΩ 1%, 0805 (J17 RX 5V→3.3V level shift, lower)
    R45B:   33Ω 5%, 0805 (RX series after divider)
    R93:    20kΩ 5%, 0805 (RS485 failsafe bias - A line pull-up)
    R94:    20kΩ 5%, 0805 (RS485 failsafe bias - B line pull-down)

    ⚠️ RS485 SURGE PROTECTION (D21):
    ─────────────────────────────────
    SM712 asymmetric TVS protects A/B lines from industrial EMI:
    • Clamps to -7V / +12V (matches RS485 common-mode range)
    • Protects against lightning surges and motor switching noise
    • Place close to J17 connector, between connector and MAX3485
    J17:    JST-XH 6-pin header (B6B-XH-A)


    JP4 Hardware Mode Selection:

    JP4 is a 3-pad solder jumper that selects the GPIO7 (RX) signal source.
    This prevents bus contention by physically disconnecting the unused source.

    CIRCUIT TOPOLOGY:
    ─────────────────

        ┌───────────────┐                              ┌───────────────┐
        │  U8 (MAX3485) │                              │ J17 Connector │
        │   RS485 IC    │                              │ (Ext. Meter)  │
        │               │                              │               │
        │      RO ○─────┼──────────┐      ┌───────────┼───○ Pin 4     │
        │               │          │      │           │   (RX signal) │
        └───────────────┘          │      │           └───────────────┘
                                   │      │
                                   │      ↓
                                   │   ┌─────┐  R_DIV
                                   │   │     │  Voltage
                                   │   └──┬──┘  Divider
                                   │      │     (5V→3.3V)
                                   │      │
                                   ↓      ↓
                          ┌────────────────────────┐
                          │    JP4 (3-Pad Jumper)  │
                          │                        │
                          │   [1]    [2]    [3]    │
                          │    ●──────●──────●     │
                          │    │      │      │     │
                          └────┼──────┼──────┼─────┘
                               │      │      │
                   From U8 RO ─┘      │      └─ From J17 RX (divided)
                                      │
                                      ↓
                                   GPIO7
                                (Pico RX input)

    JUMPER PAD CONNECTIONS:
    ───────────────────────
        Pad 1 ────► U8 (MAX3485) RO pin output
        Pad 2 ────► GPIO7 (Pico UART RX) ← CENTER, always connects to MCU
        Pad 3 ────► J17 pin 4 via voltage divider (5V-safe → 3.3V)

    ═══════════════════════════════════════════════════════════════════════
                           CONFIGURATION MODES
    ═══════════════════════════════════════════════════════════════════════

    MODE 1: TTL UART (PZEM, JSY meters - Most Common)
    ─────────────────────────────────────────────────

        Solder bridge pads 2-3:

             U8 RO                              J17 RX (via divider)
               │                                       │
               ○                                       ○
              [1]         [2]═══════════════[3]
                           │     BRIDGED
                           ↓
                        GPIO7

        Signal path: J17 Pin 4 → Voltage Divider → JP4 Pad 3 → Pad 2 → GPIO7
        U8 RO is DISCONNECTED (floating, no contention)

        • GPIO6 (TX) connects directly to J17-5 via series resistor
        • GPIO7 (RX) receives level-shifted signal from J17-4
        • GPIO20 (DE/RE) unused - can be repurposed or left floating
        • U8 MAX3485 is inactive (can be left unpopulated)


    MODE 2: RS485 (Eastron SDM, Industrial Modbus meters)
    ─────────────────────────────────────────────────────

        Solder bridge pads 1-2:

             U8 RO                              J17 RX (via divider)
               │                                       │
               ○                                       ○
              [1]═══════════[2]               [3]
                  BRIDGED    │
                             ↓
                          GPIO7

        Signal path: U8 RO → JP4 Pad 1 → Pad 2 → GPIO7
        J17-4 divider is DISCONNECTED (no contention)

        • GPIO6 (TX) → U8 DI → Differential A/B output
        • GPIO7 (RX) ← U8 RO ← Differential A/B input
        • GPIO20 controls DE/RE (HIGH=transmit, LOW=receive)
        • J17-4/5 become RS485 differential A/B lines

    ═══════════════════════════════════════════════════════════════════════

    WHY JP4 IS NEEDED:
    ──────────────────
    Without JP4, both U8 RO and the J17 voltage divider would drive GPIO7
    simultaneously, causing:
      • Bus contention (two outputs fighting)
      • Corrupted data on GPIO7
      • Potential damage to U8 or the meter

    JP4 provides hardware-level isolation - only ONE signal source can
    reach GPIO7, regardless of firmware configuration or GPIO states.


    FIRMWARE AUTO-DETECTION:
    ────────────────────────
    On startup, firmware scans:
    1. 9600 baud → Try PZEM registers
    2. 4800 baud → Try JSY registers
    3. 2400 baud → Try Eastron registers
    4. 19200 baud → Try other Modbus devices

    Successfully detected meter configuration saved to flash.

```

---

# Sheet 9: Protection & Filtering

## 9.1 5V Rail Protection

```
                        5V RAIL TRANSIENT PROTECTION
    ════════════════════════════════════════════════════════════════════════════

    CIRCUIT OVERVIEW:
    ─────────────────
    This protection circuit sits between the AC/DC power module output and
    the 5V distribution bus. It provides:
      • EMI filtering (ferrite bead FB2)
      • Transient voltage clamping (TVS diode D20)
      • Bulk capacitance for load transients (C2)

    SCHEMATIC:
    ──────────

                              FB2 (Ferrite Bead)
                              600Ω @ 100MHz
      AC/DC Module                 │
      5V Output      ┌─────────────┴─────────────┐
          │          │                           │
          │     ┌────┴────┐                      │
          ○─────┤ ░░░░░░░ ├──────────────────────┼──○ +5V_FILTERED
      5V_RAW    └─────────┘                      │     (To Pico VSYS,
          │                                      │      Relays, SSRs,
          │                        5V_FILTERED   │      ESP32 J15-1)
          │                        Bus Node      │
          │                            │         │
          │                    ┌───────┴───────┐ │
          │                    │               │ │
          │               ┌────┴────┐    ┌─────┴─┴───┐
          │               │   D20   │    │    C2     │
          │               │   TVS   │    │   470µF   │
          │               │SMBJ5.0A │    │   6.3V    │
          │               │  ──┬──  │    │  Polymer  │
          │               │  ──┴──  │    │     │     │
          │               │    │    │    │   ═══     │
          │               └────┬────┘    └─────┬─────┘
          │                    │               │
          │                    └───────┬───────┘
          │                            │
      ────┴────────────────────────────┴────────────────────────
                                  GND

    SIGNAL FLOW:
    ────────────
      [AC/DC 5V Out] ──► [FB2 Ferrite] ──► [5V_FILTERED Bus] ──► [Load]
                                                  │
                                          ┌───────┴───────┐
                                          │               │
                                        [D20]           [C2]
                                        TVS             Bulk Cap
                                          │               │
                                          └───────┬───────┘
                                                 GND

    CONNECTION POINTS:
    ──────────────────
      • Input:  5V_RAW from AC/DC module (e.g., HLK-5M05 or Mean Well IRM-05)
      • Output: 5V_FILTERED to distribution points:
                - Pico VSYS (main MCU power)
                - Relay coil supply
                - SSR driver supply
                - ESP32 connector J15-1

    COMPONENT VALUES:
    ─────────────────
      FB2: Ferrite Bead, 600Ω @ 100MHz, 0805 package
           - Purpose: Filters high-frequency noise from switching supply
           - Note: Optional if AC/DC module has clean output

      D20: SMBJ5.0A, SMB package
           - Standoff voltage: 6.4V (won't conduct during normal operation)
           - Clamping voltage: 9.2V @ 1A
           - Peak pulse power: 600W (10/1000µs)
           - Purpose: Clamps voltage spikes to protect downstream ICs

      C2:  470µF, 6.3V, Polymer Electrolytic (low ESR)
           - Purpose: Bulk decoupling, absorbs load transients
           - Note: Polymer type chosen for low ESR and better life in
                   hot environment inside espresso machine (~60-80°C)

    PROTECTION BEHAVIOR:
    ────────────────────
      Normal operation (5V ±5%):
        → D20 does not conduct (below 6.4V standoff)
        → C2 provides bulk capacitance for load current spikes

      Voltage transient (>6.4V spike):
        → D20 clamps immediately, shunting excess energy to GND
        → Protects Pico and other 5V components from damage

      EMI/RFI noise:
        → FB2 attenuates high-frequency noise (>10MHz)
        → Keeps noise from propagating to sensitive digital circuits
```

---

# Net List Summary

```
                            NET LIST SUMMARY
    ════════════════════════════════════════════════════════════════════════════

    POWER NETS:
    ───────────
    +5V          → Pico VSYS, Relay coils, SSR drivers, ESP32 (J15-1), LED anodes
    +3.3V        → Pico 3V3, Digital I/O, pull-ups, MAX3485 (U8)
    ADC_VREF     → 3.0V Buffered Reference, NTC dividers (R1/R2)
    GND          → System ground (isolated from mains PE)

    HIGH VOLTAGE NETS (⚠️ MAINS):
    ─────────────────────────────
    L_IN         → J1-L (Mains Live Input)
    L_FUSED      → After F1 fuse (10A) - to relay COMs only
    N            → J1-N (Mains Neutral)
    PE           → J1-PE (Protective Earth, to chassis)

    NOTE: No HV measurement circuitry on control PCB. Heaters connect to
    external SSRs. J24 provides L/N/PE pass-through to meter in existing
    HV zone (same traces as relay L_FUSED bus). PCB provides LV control signals.

    RELAY OUTPUT NETS:
    ──────────────────
    K1_COM       → Relay K1 Common (internal to L_FUSED)
    K2_COM       → Relay K2 Common (internal to L_FUSED)
    K3_COM       → Relay K3 Common (internal to L_FUSED)

    GPIO NETS:
    ──────────
    GPIO0  → ESP32_TX (J15-3)
    GPIO1  → ESP32_RX (J15-4)
    GPIO2  → WATER_SW (J26-1, S1)
    GPIO3  → TANK_LVL (J26-3, S2)
    GPIO4  → STEAM_LVL (J26-5, S3 via comparator)
    GPIO5  → BREW_SW (J26-6, S4)
    GPIO6  → METER_TX (UART to meter RX / RS485 DI, J17-5)
    GPIO7  → METER_RX (UART from meter TX / RS485 RO, J17-4)
    GPIO8  → I2C0_SDA (J23-3, with 4.7kΩ pull-up)
    GPIO9  → I2C0_SCL (J23-4, with 4.7kΩ pull-up)
    GPIO10 → K1_DRIVE (relay coil, output to J2 spade - 220V)
    GPIO11 → K2_DRIVE (relay coil, output to J3 spade - 220V)
    GPIO12 → K3_DRIVE (relay coil, output to J4 spade - 220V)
    GPIO13 → SSR1_DRIVE
    GPIO14 → SSR2_DRIVE
    GPIO15 → STATUS_LED
    GPIO16 → SPARE1 (J15-6, ↔ ESP32 GPIO9, 4.7kΩ pull-down)
    GPIO17 → SPARE (SPI0 CS, available)
    GPIO18 → SPARE (SPI0 SCK, available)
    GPIO19 → BUZZER
    GPIO20 → RS485_DE_RE (MAX3485 direction control, J17-6)
    GPIO21 → WEIGHT_STOP (J15-7, ← ESP32 GPIO10, 4.7kΩ pull-down)
    GPIO22 → SPARE2 (J15-8, ↔ ESP32 GPIO22, 4.7kΩ pull-down)
    GPIO23-25 → INTERNAL (Pico 2 module - NOT on header)
    GPIO26 → ADC0_BREW_NTC (J26-8/9)
    GPIO27 → ADC1_STEAM_NTC (J26-10/11)
    GPIO28 → ADC2_PRESSURE (from J26-14 voltage divider)
    RUN    → SW1_RESET

    220V AC RELAY OUTPUTS (6.3mm Spade Terminals):
    ──────────────────────────────────────────────
    J2-NO  → Relay K1 N.O. output (Mains Lamp, 220V ≤100mA)
    J3-NO  → Relay K2 N.O. output (Pump, 220V 5A)
    J4-NO  → Relay K3 N.O. output (Solenoid, 220V ~0.5A)

    J17 POWER METER LV INTERFACE (JST-XH 6-pin):
    ─────────────────────────────────────────────
    J17-1  (3V3)   → 3.3V power for logic-level meters
    J17-2  (5V)    → 5V power for PZEM, JSY modules
    J17-3  (GND)   → System ground
    J17-4  (RX)    → GPIO7 (from meter TX, via 33Ω)
    J17-5  (TX)    → GPIO6 (to meter RX, via 33Ω)
    J17-6  (DE/RE) → GPIO20 (RS485 direction control)

    J24 POWER METER HV TERMINALS (Screw Terminal 3-pos, 5.08mm):
    ────────────────────────────────────────────────────────────
    J24-1  (L)     → Live FUSED (from L_FUSED bus, after F1)
    J24-2  (N)     → Neutral (from N bus)
    J24-3  (PE)    → Protective Earth (optional, for DIN rail meters)

    NOTE: GPIO23-25 and GPIO29 are used internally by Pico 2 module
          and are NOT available on the 40-pin header.
          For expansion, use GPIO22 via J15 Pin 8 (SPARE).

    J26 UNIFIED LOW-VOLTAGE TERMINAL BLOCK (18-pos):
    ─────────────────────────────────────────────────
    J26-1  (S1)    → Water Reservoir Switch Signal → GPIO2
    J26-2  (S1-G)  → Water Reservoir Switch GND
    J26-3  (S2)    → Tank Level Sensor Signal → GPIO3
    J26-4  (S2-G)  → Tank Level Sensor GND
    J26-5  (S3)    → Steam Boiler Level Probe → GPIO4 (via comparator)
    J26-6  (S4)    → Brew Handle Switch Signal → GPIO5
    J26-7  (S4-G)  → Brew Handle Switch GND
    J26-8  (T1)    → Brew NTC Signal → ADC0 via divider
    J26-9  (T1-G)  → Brew NTC GND
    J26-10 (T2)    → Steam NTC Signal → ADC1 via divider
    J26-11 (T2-G)  → Steam NTC GND
    J26-12 (P-5V)  → Pressure transducer 5V
    J26-13 (P-GND) → Pressure transducer GND
    J26-14 (P-SIG) → Pressure signal → ADC2 via divider
    J26-15 (SSR1+) → +5V (Brew heater SSR power)
    J26-16 (SSR1-) → SSR1_NEG (Brew heater SSR trigger)
    J26-17 (SSR2+) → +5V (Steam heater SSR power)
    J26-18 (SSR2-) → SSR2_NEG (Steam heater SSR trigger)

    ⚠️ CT CLAMP: Connects directly to external power meter module (not on J26)

    SOLDER JUMPERS:
    ────────────────
    JP1 → Brew NTC selection (OPEN=50kΩ ECM, CLOSE=10kΩ Rocket/Gaggia)
    JP2 → Steam NTC selection (OPEN=50kΩ ECM, CLOSE=10kΩ Rocket/Gaggia)
    JP3 → J17 RX voltage bypass (OPEN=5V meters, CLOSE=3.3V meters)
    JP4 → GPIO7 source select (1-2=RS485, 2-3=TTL)
```

---

_End of Schematic Reference Document_
