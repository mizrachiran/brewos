# PCB Design Requirements

## Board Specifications

| Parameter       | Specification                                    |
| --------------- | ------------------------------------------------ |
| Dimensions      | **80mm × 80mm** (target)                         |
| Layers          | **4-layer recommended** (2-layer minimum viable) |
| Material        | FR-4 TG130 or higher                             |
| Thickness       | 1.6mm ±10%                                       |
| Copper Weight   | 2oz (70µm) both layers                           |
| Min Trace/Space | 0.2mm / 0.2mm (8mil/8mil)                        |
| Min Drill       | 0.3mm                                            |
| Surface Finish  | ENIG (preferred) or HASL Lead-Free               |
| Solder Mask     | Green (both sides)                               |
| Silkscreen      | White (both sides)                               |
| IPC Class       | Class 2 minimum                                  |
| **Edge Rails**  | **5mm keep-out zone on all edges**               |

---

## Edge Rails (Keep-Out Zone)

### 5mm Edge Clearance Requirement

**All edges of the PCB must maintain a 5mm (200 mil) keep-out zone** where no components, traces, or copper pours are allowed.

### Purpose

1. **Manufacturing Safety:** Prevents components from being damaged during panelization, routing, and depanelization
2. **Mechanical Clearance:** Ensures proper fit within enclosure and prevents interference with mounting hardware
3. **Handling Safety:** Provides safe handling area during assembly and installation
4. **Creepage/Clearance:** Maintains safety margins for high-voltage traces near board edges

### Implementation Rules

| Zone               | Width           | Restrictions                                                          |
| ------------------ | --------------- | --------------------------------------------------------------------- |
| **All Edges**      | **5mm minimum** | No components, pads, or traces                                        |
| **Exceptions**     | -               | Connectors may extend to board edge (but pads must be ≥2mm from edge) |
| **Mounting Holes** | -               | Holes positioned 5mm from edges (within keep-out zone)                |

### Visual Representation

```
┌─────────────────────────────────────────────────────────┐
│ ═══════════════════════════════════════════════════════ │ ← 5mm Edge Rail
│                                                          │   (Keep-Out Zone)
│  ┌──────────────────────────────────────────────────┐  │
│  │                                                  │  │
│  │         COMPONENT PLACEMENT AREA                 │  │
│  │         (70mm × 70mm usable area)                │  │
│  │                                                  │  │
│  │  • All components must be within this zone      │  │
│  │  • Traces may route to connectors at edge       │  │
│  │  • Mounting holes (MH1-MH4) at 5mm from edge    │  │
│  │                                                  │  │
│  └──────────────────────────────────────────────────┘  │
│                                                          │
│ ═══════════════════════════════════════════════════════ │ ← 5mm Edge Rail
└─────────────────────────────────────────────────────────┘
        80mm × 80mm total board size
```

### Connector Placement Exception

Connectors (J1-J5, J15-J17, J24, J26) may be placed at the board edge, but:

- **Connector body:** May extend to or slightly beyond board edge
- **Connector pads:** Must maintain ≥2mm clearance from board edge
- **Traces to connectors:** May route through edge rail zone, but keep as short as possible

### Design Rule Check (DRC) Requirements

When generating Gerber files, ensure DRC rules include:

- **Edge clearance:** 5mm minimum for all components
- **Edge clearance:** 2mm minimum for connector pads
- **Edge clearance:** 0.5mm minimum for traces (if routing to edge connectors)

---

## Connector Placement Strategy

### Design Goal: Single-Edge Connector Access

**Rationale:** The enclosure opens only from the bottom to prevent water ingress from steam/spills. All external connections must be accessible from this single opening.

### Connector Edge Priority

| Priority          | Edge          | Connectors                                        | Notes                        |
| ----------------- | ------------- | ------------------------------------------------- | ---------------------------- |
| **1 (Required)**  | BOTTOM        | All HV spades (J1-J4), Screw terminals (J24, J26) | Mandatory - mains wiring     |
| **2 (Preferred)** | BOTTOM        | J15 (ESP32), J17 (Power meter)                    | Keep with HV if space allows |
| **3 (Fallback)**  | LEFT or RIGHT | J15, J17 if bottom is full                        | Maximum 2 edges total        |

### ⚠️ Constraint: Maximum 2 Accessible Edges

If all connectors cannot fit on the bottom edge:

- **Bottom edge:** All HV connectors (J1-J4, J24) + J26 (sensor screw terminal) + J5 (SRif)
- **One side edge:** LV JST connectors (J15, J17)

```
┌─────────────────────────────────────────────────────┐
│                                                     │
│                  COMPONENTS                         │
│                  (top side)                         │
│                                                     │
│  ┌─────┐                                           │
├──┤ J15 │  ← LV connectors on LEFT edge (fallback)  │
│  ├─────┤                                           │
│  │ J17 │                                           │
│  └─────┘                                           │
│                                                     │
└─────────────────────────────────────────────────────┘
  ║    ║    ║    ║         ║              ║
 J1   J2   J3   J4       J24            J26
 L/N  LED  PUMP SOL    METER HV      SENSORS
      ↑                    ↑              ↑
      └────────────────────┴──────────────┘
              BOTTOM EDGE (PRIMARY)
              Enclosure opening here
```

### Ideal Layout (All on Bottom)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                             │
│                           COMPONENTS (top side)                             │
│                                                                             │
│   ┌─────────┐  ┌────────┐  ┌─────────────────────────────────────────────┐ │
│   │  HLK    │  │  PICO  │  │            ANALOG + DIGITAL                 │ │
│   │ 15M05C  │  │   2    │  │                                             │ │
│   └─────────┘  └────────┘  └─────────────────────────────────────────────┘ │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
  ║   ║   ║   ║   ║       ║         ║                    ║          ║
 J1  J2  J3  J4  J24     J26       J15                  J17        J16
 L/N LED PMP SOL MTR-HV SENSORS   ESP32              METER-LV    DEBUG
 ↑_____↑___↑___↑___↑______↑________↑___________________↑___________↑
                    ALL CONNECTORS ON BOTTOM EDGE
```

## Mounting Holes

| Hole | Size       | Type | Location | Notes                       |
| ---- | ---------- | ---- | -------- | --------------------------- |
| MH1  | M3 (3.2mm) | NPTH | Corner   | Isolated (no PE connection) |
| MH2  | M3 (3.2mm) | NPTH | Corner   | Isolated                    |
| MH3  | M3 (3.2mm) | NPTH | Corner   | Isolated                    |
| MH4  | M3 (3.2mm) | NPTH | Corner   | Isolated                    |

**⚠️ IMPORTANT:** All mounting holes are NPTH (Non-Plated Through Hole).
We do NOT want the board grounding itself randomly through the screws.
Grounding happens ONLY through the J5 (SRif) wire to prevent ground loops
and ensure controlled current paths.

**Position:** All mounting holes positioned 5mm from board edges (within the edge rail keep-out zone).

---

## Trace Width Requirements

### High-Voltage Section (220V AC)

| Current              | Min Trace Width (1oz)     | Min Trace Width (2oz)       | Notes                                                                                   |
| -------------------- | ------------------------- | --------------------------- | --------------------------------------------------------------------------------------- |
| 10A (fused live bus) | **7mm** (or polygon pour) | **3.5mm** (or polygon pour) | **Fused live bus feeding relays - use polygon pours on both layers stitched with vias** |
| 16A (pump)           | 5mm                       | 2.5mm                       | K2 relay path                                                                           |
| 5A (general)         | 2mm                       | 1mm                         | K1, K3 relay paths                                                                      |
| Mains L/N            | 3mm                       | 1.5mm                       | Input traces                                                                            |

**⚠️ CRITICAL - 10A Fused Live Bus:**

- For 10A current on standard 1oz copper, trace width must be **~7mm** to keep temperature rise <10°C
- For 2oz copper, trace width must be **~3.5mm**
- **Recommendation:** Since space is tight (80×80mm), use **Polygon Pours (Zones)** on both Top and Bottom layers stitched with vias for the High-Voltage Live and Neutral paths, rather than simple traces
- This provides better current distribution and thermal management

### Low-Voltage Section

| Signal Type  | Trace Width | Notes               |
| ------------ | ----------- | ------------------- |
| 5V power     | 1.0mm       | Main distribution   |
| 3.3V power   | 0.5mm       | Logic supply        |
| GPIO signals | 0.25mm      | Digital I/O         |
| ADC signals  | 0.3mm       | Guarded, short runs |

---

## Layer Stackup

### Recommended: 4-Layer (Mixed-Signal Design)

For optimal performance in a mixed-signal design (high-speed digital + precision analog + high-power switching), a 4-layer stackup is strongly recommended:

```
┌─────────────────────────────────────────┐
│  LAYER 1 (TOP): Components              │
│  - High-speed traces (QSPI, USB, UART)  │
│  - Analog signals                        │
│  - Component pads                       │
│  - HV traces (wide, isolated)            │
├─────────────────────────────────────────┤
│  LAYER 2 (INNER 1): SOLID GROUND PLANE  │
│  - GND (no splits)                       │
│  - Reference for all signals             │
│  - Analog island strategy (zone partition)│
├─────────────────────────────────────────┤
│  LAYER 3 (INNER 2): POWER PLANES        │
│  - 3.3V plane                            │
│  - 5V plane                               │
│  - 24V plane (if needed)                 │
├─────────────────────────────────────────┤
│  LAYER 4 (BOTTOM): Non-Critical Routing │
│  - Low-speed signals                     │
│  - Return paths                          │
│  - Additional ground pour (LV section)   │
└─────────────────────────────────────────┘
```

**Benefits:**

- Solid ground plane provides excellent signal integrity
- Power planes reduce voltage drop and EMI
- Analog island strategy: partition zones without splitting ground
- Better thermal management through power plane copper

### Minimum Viable: 2-Layer (Fallback)

If cost constraints require 2-layer, the following stackup is acceptable but with performance trade-offs:

```
┌─────────────────────────────────────────┐
│  TOP COPPER (2oz)                       │
│  - Signal routing                       │
│  - Component pads                       │
│  - HV traces (wide, isolated)           │
├─────────────────────────────────────────┤
│  FR-4 CORE (1.6mm)                      │
│  TG130 minimum                          │
├─────────────────────────────────────────┤
│  BOTTOM COPPER (2oz)                    │
│  - Ground plane (LV section)            │
│  - Power distribution                   │
│  - Return paths                         │
└─────────────────────────────────────────┘
```

**Trade-offs:**

- Reduced signal integrity (no dedicated power planes)
- More challenging analog routing (ground return paths)
- Higher EMI susceptibility
- Still functional but not optimal for production

---

## PCB Layout Zones (80×80mm Target)

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                         PCB LAYOUT ZONES (80×80mm)                              │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│    TOP EDGE (no connectors - sealed)                                            │
│    ════════════════════════════════════════════════════════════════════════    │
│    │                                                                        │   │
│    │  ┌────────────────┐ ║ ┌──────────────────────────────────────────┐   │   │
│    │  │  HV SECTION    │ ║ │           LV SECTION                     │   │   │
│    │  │                │ ║ │                                          │   │   │
│    │  │  • HLK-15M05C  │ ║ │  ┌─────────────────┐  ┌──────────────┐  │   │   │
│    │  │  • F1, F2      │ ║ │  │    RP2354       │  │   ANALOG     │  │   │   │
│    │  │  • RV1 (MOV)   │ ║ │  │    (U1) QFN-60  │  │  U5,U6,U7,U9 │  │   │   │
│    │  │  • K1,K2,K3    │ ║ │  └─────────────────┘  └──────────────┘  │   │   │
│    │  │  • RV2,RV3     │ ║ │                                          │   │   │
│    │  │                │ ║ │  ┌─────────────────────────────────────┐ │   │   │
│    │  │  (compact)     │ ║ │  │  DRIVERS: Q1-Q5, LEDs, Buck (U3)   │ │   │   │
│    │  └────────────────┘ ║ │  └─────────────────────────────────────┘ │   │   │
│    │                     ║ │                                          │   │   │
│    │    6mm ISOLATION    ║ └──────────────────────────────────────────┘   │   │
│    │        SLOT         ║                                                │   │
│    ════════════════════════════════════════════════════════════════════════    │
│    BOTTOM EDGE - ALL CONNECTORS (enclosure opening)                             │
│    ┌───┬───┬───┬───┬─────┬──────────────────┬────────┬────────┬──────┬────┬──────┐ │
│    │J1 │J2 │J3 │J4 │ J24 │       J26        │  J15   │  J17   │ J16  │ J5 │J_USB │ │
│    │L/N│LED│PMP│SOL│MTR  │    SENSORS       │ ESP32  │ METER  │DEBUG │SRif│ USB-C│ │
│    │   │   │   │   │ HV  │    (18-pos)      │(8-pin) │(6-pin) │(4pin)│    │      │ │
│    └───┴───┴───┴───┴─────┴──────────────────┴────────┴────────┴──────┴────┴──────┘ │
│                                                                                  │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### Space Optimization for 80×80mm

**Note:** With 5mm edge rails on all sides, the usable component placement area is **70mm × 70mm**.

| Strategy                   | Implementation                                                                                                     |
| -------------------------- | ------------------------------------------------------------------------------------------------------------------ |
| **Vertical HLK placement** | Stand HLK-15M05C upright if needed (48×28×18mm)                                                                    |
| **Relay clustering**       | K1, K3 (slim 5mm) flanking K2 (standard)                                                                           |
| **RP2354 placement**       | QFN-60 package (7×7mm), keep crystal close to XIN/XOUT, verify thermal pad connection                              |
| **Crystal placement**      | 12MHz crystal + load caps within 5mm of RP2354                                                                     |
| **QFN-60 footprint**       | **VERIFY:** Use QFN-60 (7×7mm) footprint, NOT QFN-80 (10×10mm). Thermal pad must connect to GND with multiple vias |
| **Bottom-edge connectors** | All connectors aligned, vertical entry                                                                             |
| **Component density**      | 0603/0805 passives, tight but DFM-compliant                                                                        |
| **Edge rail compliance**   | All components ≥5mm from board edges                                                                               |

### Fallback: 2-Edge Layout (if 80mm too tight)

If bottom edge cannot accommodate all connectors:

```
         80mm
    ┌────────────┐
    │            │
    │ COMPONENTS │  80mm
J15─┤            │
J17─┤            │
    │            │
    └────────────┘
     J1 J2 J3 J4 J24 J26
     ════════════════════
     BOTTOM (HV + Sensors)
```

**Rule:** HV connectors (J1-J4, J24) MUST be on bottom edge. LV connectors (J15, J17) may move to left edge if required.

---

## Creepage and Clearance

### IEC 60950-1 / IEC 62368-1 Requirements

| Boundary              | Isolation  | Creepage  | Clearance |
| --------------------- | ---------- | --------- | --------- |
| Mains → LV (5V)       | Reinforced | **6.0mm** | **4.0mm** |
| Relay coil → contacts | Basic      | 3.0mm     | 2.5mm     |
| Phase → Neutral       | Functional | 2.5mm     | 2.5mm     |

### Implementation

- **Milled slot** between HV and LV sections (minimum 2mm wide, full PCB thickness)
- **No copper pour** in isolation zone
- **Conformal coating** recommended for production

**⚠️ CRITICAL: Mains Creepage Slots (IPC-2221B Compliance)**

Standard FR4 coating is NOT sufficient for high-voltage isolation. IPC-2221B standards require **physical milled slots (cutouts)** in the PCB between High-Voltage Relay contacts and Low-Voltage logic sections.

**Requirements:**

- **Slot width:** Minimum 2mm (80 mil) milled cutout
- **Slot depth:** Full PCB thickness (1.6mm)
- **Clearance:** >3mm (120 mil) between any HV trace and LV section
- **Location:** Between relay contacts (K1, K2, K3) and all LV components
- **Verification:** Visual inspection required - slots must be visible on both top and bottom layers

**Rationale:** If contamination (moisture, dust, flux residue) bridges the isolation gap, a milled slot prevents arcing by creating a physical air gap. Standard solder mask alone cannot prevent tracking under fault conditions.

**Implementation Example:**

```
┌─────────────────────────────────────────────────────────┐
│  HV SECTION (Relays, MOVs, Fuses)                      │
│  ┌─────┐  ┌─────┐  ┌─────┐                             │
│  │ K1  │  │ K2  │  │ K3  │                             │
│  └─────┘  └─────┘  └─────┘                             │
│                                                          │
│  ═══════════════════════════════════════════════════   │ ← Milled Slot
│  ═══════════════════════════════════════════════════   │   (2mm wide)
│                                                          │
│  LV SECTION (MCU, Sensors, Logic)                       │
│  ┌─────┐  ┌─────┐  ┌─────┐                             │
│  │RP2354│  │ U5  │  │ U9  │                             │
│  └─────┘  └─────┘  └─────┘                             │
└─────────────────────────────────────────────────────────┘
```

**Gerber File Requirements:**

- Add milled slot as a "routed slot" or "plated slot" layer in PCB design software
- Specify slot width: 2mm minimum
- Ensure slot extends full PCB thickness (no copper in slot)
- Verify with PCB manufacturer that slots will be milled (not just routed)

---

## Critical Layout Notes

### Analog Section (High Priority)

1. **ADC traces**: Keep short, route away from switching noise
2. **Ground plane**: Solid under analog section, connect at single point
3. **Ferrite bead** (FB1): Place between digital and analog 3.3V
4. **Reference capacitors** (C7, C7A): Place directly at U9 output
5. **NTC filter caps** (C8, C9): Place close to ADC pins
6. **⚠️ CRITICAL - Thermal Isolation:** Place NTC analog front-end (ADC resistors R1/R2, filter caps C8/C9, hum filters R_HUM/C_HUM) **physically far away** from heat-generating components:
   - **F1 (10A Fuse):** Generates localized heat during high-current operation
   - **K2 (Pump Relay):** Generates heat during extended pump operation
   - **Minimum distance:** ≥20mm from F1 and K2 to prevent thermal gradients affecting temperature reading accuracy
   - **Thermal vias:** Do NOT place thermal vias near NTC circuits (they conduct heat from bottom layer)

### RP2354 MCU Section (v2.31)

1. **⚠️ CRITICAL - QFN-60 Footprint Verification:**
   - **Package:** RP2354A uses **QFN-60 (7×7mm)** footprint, NOT QFN-80 (10×10mm)
   - **Pin 1 orientation:** Verify Pin 1 marker matches RP2354A datasheet (typically corner dot/bevel)
   - **Pad dimensions:** Verify pad size matches QFN-60 specification (typically 0.25mm × 0.5mm)
   - **Thermal pad:** Central thermal pad (GND) must connect to PCB ground plane with **multiple vias** (minimum 4 vias, preferably 9 in 3×3 grid) for heat dissipation
   - **Via size:** Use 0.2mm vias with 0.1mm thermal relief spokes (prevents tombstoning during reflow)
   - **Solder paste:** Thermal pad requires 50-80% paste coverage (check stencil design)
2. **Crystal placement**: 12MHz crystal (Y1) + load caps (C_XTAL) + series resistor (R_XTAL) within 5mm of XIN/XOUT pins
   - **Crystal selection**: Raspberry Pi recommends using characterized crystal parts (e.g., Abracon ABM8-12.000MHZ-B2-T) with recommended external components
   - **Load capacitance calculation**: C_L,external = 2 × (C_L,crystal - C_stray)
   - Typical: C_L,crystal = 12-20pF (from crystal datasheet), C_stray = 2-5pF (PCB + MCU pins)
   - Result: C_XTAL = 2 × (15pF - 3pF) = 24pF each → use 22pF standard value (or 15pF/27pF per crystal datasheet)
   - **Series resistor (R_XTAL)**: 1kΩ resistor in series with XOUT pin to prevent overdriving the crystal (recommended by Raspberry Pi)
   - **Ground guard ring**: Surround crystal on layer below to prevent frequency pulling from EMI
   - **⚠️ CRITICAL:** Crystal (Y1) is **REQUIRED** for USB operation and reliable high-speed PLL (150MHz) performance. Internal ring oscillator (ROSC) is not precise enough for 921k baud UART or accurate timing.
   - **Alternative**: External active clock signal can be supplied to XIN pin if internal oscillator circuit is disabled in software (not recommended for this design)
3. **Decoupling**: 100nF ceramic cap at each IOVDD pin, placed as close as possible
4. **Core voltage (DVDD)**: External LDO (U4) generates 1.1V
   - **LDO placement**: Close to RP2354, minimize trace length to DVDD pins
   - **Decoupling**: 10µF bulk (1206) + 100nF (0805) on LDO output, placed near DVDD pins
   - **L2 inductor REMOVED**: No longer needed (internal SMPS disabled)
5. **VREG pins configuration**:
   - VREG_VIN: Connect to +3.3V
   - VREG_AVDD: Connect to +3.3V via RC filter (33Ω + 100nF)
   - VREG_LX: Leave unconnected (floating)
   - VREG_PGND: Connect to ground plane
6. **QSPI pin protection**:
   - QSPI_SS (CSn): Add 10kΩ pull-up to +3.3V (R_QSPI_CS) to ensure defined state during POR
   - Prevents parasitic boot mode entry that could corrupt internal flash boot sequence
   - **BOOTSEL button (SW2)**: Add tactile switch for USB bootloader entry
     - Connection: QSPI_SS → R_BOOTSEL (1kΩ) → SW2 → GND
     - Usage: Hold SW2, press & release SW1 (Reset), release SW2 → Enters USB boot mode
     - Placement: Accessible to user, near SW1 (Reset button)
     - Protection: R_BOOTSEL (1kΩ) limits current if button pressed during active flash access
7. **SWD routing**: SWDIO/SWCLK (dedicated pins) traces to J15 Pins 6/8 via 47Ω series resistors (R_SWD)
   - Route as differential-like pair with ground guard trace
   - Keep short and away from noisy signals
   - **Reset line protection**: Place R_RUN_EXT (1kΩ) between J15 Pin 5 and RP2354_RUN net
     - Prevents short-circuit when SW1 (Reset button) is pressed while ESP32 drives RUN HIGH
     - Limits current to ~3.3mA (safe for ESP32 GPIO)
     - Placement: Near J15 connector
8. **USB-C port (J_USB)**: USB D+/D- routing to RP2354 USB pins
   - **USB D+/D- traces**: Route as differential pair (90Ω impedance, if possible)
   - **Length matching**: Keep D+ and D- traces equal length (±0.1mm)
   - **ESD protection**: Place **D_USB_DP/D_USB_DM (PESD5V0S1BL) as close as possible to the USB Connector (J_USB)**. This shunts ESD spikes at the entry point before they enter the board's internal circuitry (best practice per Raspberry Pi guidelines).
   - **Termination resistors**: Place **R_USB_DP/R_USB_DM (27Ω) as close as possible to the RP2354 USB pins** (required for impedance matching). Signal flow: Connector → ESD Diode → Trace → Termination Resistor → MCU.
   - **VBUS routing**: VBUS → F_USB (1A PTC fuse) → D_VBUS (SS14, 1A Schottky) → VSYS
   - **CC resistors**: Place R_CC1/R_CC2 (5.1kΩ) close to connector CC pins
   - **Ground**: Solid ground plane under USB traces, connect connector shield to GND
   - **Placement**: USB-C connector on bottom edge, **covered or internal** (requiring panel removal to access) to discourage casual use while machine is mains-powered
   - **⚠️ CRITICAL:** D_VBUS must be 1A rated (SS14) for ESP32 load. BAT54S (200mA) will fail.
   - **⚠️ CRITICAL:** 27Ω termination resistors are required for stable USB communication. Without them, USB may be unstable or fail.

### Power Section

1. **SMD Fuses (F1, F2):** Use wide traces (>3mm) connecting to fuse pads to act as heat sinks.
2. **Buck converter**: Tight layout, short SW trace
3. **Input/output caps**: Adjacent to TPS563200
4. **Inductor**: Keep away from sensitive analog

### High-Voltage Section

1. **Wide traces**: ≥5mm for 16A pump path
2. **Thermal relief**: On relay pads for easier soldering
3. **MOV placement**: Close to relay terminals
4. **Slot isolation**: 6mm minimum to LV section
5. **Keep-Out Zone**: Maintain strict 6mm (240 mil) Keep-Out Zone between any High Voltage trace (L, N, Relay Contacts) and the Low Voltage Ground Pour
6. **No Ground Plane**: Delete all copper within 6mm of the L/N tracks. Verify no Ground plane is under the HLK module's AC pins

### EMI Considerations

1. **Keep HV traces short** and away from board edges
2. **Ground pour** around sensitive signals
3. **Guard traces** around ADC inputs
4. **Decoupling caps** at every IC VCC pin
5. **UART ESD protection**: Place TVS diodes (D_UART_TX, D_UART_RX) near J15 connector
   - Protects RP2354 GPIOs from static discharge during display installation
   - Series resistors (R40, R41 = 1kΩ) provide 5V tolerance protection (limits fault current to <500µA)
6. **Analog island strategy** (4-layer recommended):
   - Zone A (Noisy): Power input, Solenoid Drivers, 5V/24V regulators
   - Zone B (Digital): RP2354, Flash, Display Connector
   - Zone C (Quiet Analog): Sensor inputs (Temperature, Water Level)
   - Place RP2354 between Zone B and C
   - Route analog signals cleanly over solid ground plane without crossing noisy power traces

---

## Grounding Strategy (SRif Architecture)

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                        NEW GROUNDING STRATEGY (SRif)                             │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│    1. MACHINE CHASSIS is Earthed via the main power cord (Wall Plug).          │
│    2. PCB HV SIDE is Floating (No Earth connection).                            │
│    3. PCB LV SIDE (GND) connects to CHASSIS via J5 (SRif) with C-R network.      │
│                                                                                  │
│                              MACHINE CHASSIS                                    │
│                            (Earthed via wall plug)                              │
│                                    │                                            │
│                              ┌─────┴─────┐                                      │
│                              │   J5      │  ← SRif (Chassis Reference)            │
│                              │ (6.3mm    │    18AWG Green/Yellow wire          │
│                              │  Spade)   │                                      │
│                              └─────┬─────┘                                      │
│                                    │                                            │
│                    ┌───────────────┴───────────────┐                            │
│                    │                               │                            │
│              ┌─────┴─────┐                   ┌─────┴─────┐                      │
│              │  C_SRif   │                   │  R_SRif   │                      │
│              │  100nF     │                   │  1MΩ      │                      │
│              │  (AC path) │                   │  (DC block)│                      │
│              └─────┬─────┘                   └─────┬─────┘                      │
│                    │                               │                            │
│                    └───────────────┬───────────────┘                            │
│                                    │                                            │
│                              ┌─────┴─────┐                                      │
│                              │  LV GND   │  ← PCB Logic Ground                  │
│                              │  (Logic)  │                                      │
│                              └─────┬─────┘                                      │
│                                    │                                            │
│                    ┌───────────────┴───────────────┐                            │
│                    │                               │                            │
│              ┌─────┴─────┐                   ┌─────┴─────┐                      │
│              │  HV GND   │                   │  LV GND   │                      │
│              │  (Mains)  │                   │  (Logic)  │                      │
│              │           │                   │           │                      │
│              │ FLOATING  │                   │           │                      │
│              │ (No PE)   │                   │           │                      │
│              └─────┬─────┘                   └─────┬─────┘                      │
│                    │                               │                            │
│                 Isolated                    ┌──────┴──────┐                     │
│              via HLK module                 │             │                     │
│                                       ┌─────┴────┐  ┌─────┴────┐               │
│                                       │ DGND     │  │ AGND     │               │
│                                       │ (Digital)│  │ (Analog) │               │
│                                       └──────────┘  └──────────┘               │
│                                                                                  │
│    KEY RULES:                                                                   │
│    ──────────                                                                   │
│    1. Single connection point between Chassis and LV GND (at J5 SRif via C-R)  │
│    2. C-R network: Capacitor allows 1kHz AC return, Resistor blocks DC loops   │
│    3. HV GND isolated from LV GND via HLK module                               │
│    4. AGND and DGND connect at single point near RP2354 ADC_GND                │
│    5. No ground loops - star topology only                                     │
│    6. All mounting holes (MH1-MH4) are NPTH - no random grounding            │
│                                                                                  │
│    Safety Benefit:                                                              │
│    ───────────────                                                              │
│    If 'L' shorts to the PCB surface, it hits FR4 (fiberglass), not a           │
│    Ground Plane. It cannot create a dead short or explode.                     │
│    The fuse will eventually blow if it finds a path, but the "Explosive        │
│    Arc" risk is eliminated.                                                    │
│                                                                                  │
└─────────────────────────────────────────────────────────────────────────────────┘
```

---

## Silkscreen Requirements

### Mandatory Markings

- Board name and version
- Safety warnings near HV section ("⚠️ HIGH VOLTAGE")
- Pin 1 indicators on all connectors
- Polarity markings (diodes, caps, LEDs)
- Jumper configuration labels (JP1-JP4)
- Test point labels (TP1-TP3)
- Component references

### Recommended

- QR code linking to documentation
- Manufacturer logo/branding
- Date code placeholder

---

## Enclosure Integration

### Design Constraints

| Requirement             | Rationale                                         |
| ----------------------- | ------------------------------------------------- |
| **Bottom-only opening** | Prevents water/steam ingress from above           |
| **Single-edge wiring**  | Simplifies installation, reduces cable management |
| **Compact form factor** | Fits in existing machine control cavity           |
| **Vertical clearance**  | Account for HLK module height (18mm) + connectors |

### Enclosure Dimensions (Target)

```
┌─────────────────────────────────────────┐
│                                         │
│        ┌─────────────────────┐         │
│        │                     │         │
│        │    PCB (80×80mm)    │  ~25mm  │
│        │                     │  height │
│        └─────────────────────┘         │
│                                         │
│   ═══════════════════════════════════  │  ← Bottom opening
│   Connectors accessible from below     │
│                                         │
└─────────────────────────────────────────┘
         ~90mm × 90mm × 35mm
         (external dimensions)
```

### Water Protection (IP Rating Target)

| Area        | Protection                   |
| ----------- | ---------------------------- |
| Top, sides  | Sealed (IP44 minimum)        |
| Bottom      | Open for wiring access       |
| Cable entry | Gland or grommet recommended |

**Note:** Conformal coating on PCB provides additional moisture protection.

---

## Manufacturing Pre-Flight Checklist

**⚠️ CRITICAL:** Verify all items below before exporting Gerber files for fabrication.

### Component Verification

- [ ] **Crystal (Y1):** 12MHz crystal (recommended: Abracon ABM8-12.000MHZ-B2-T or equivalent characterized part) + load capacitors (C_XTAL1, C_XTAL2 = 22pF) + series resistor (R_XTAL = 1kΩ) added to schematic/BOM/layout
- [ ] **Crystal placement:** Within 5mm of RP2354 XIN/XOUT pins (Pins 20/21)
- [ ] **Series resistor:** R_XTAL (1kΩ) connected in series with XOUT pin to prevent overdriving crystal (per Raspberry Pi recommendations)
- [ ] **C_SRif voltage rating:** Verify capacitor is **250V AC (X2 safety rating)** or **630V DC**, NOT 25V standard ceramic

### Footprint Verification

- [ ] **RP2354 QFN-60:** Verify footprint is **QFN-60 (7×7mm)**, NOT QFN-80 (10×10mm)
- [ ] **Pin 1 orientation:** Verify Pin 1 marker matches RP2354A datasheet
- [ ] **Thermal pad:** Central GND pad connected to ground plane with **multiple vias** (minimum 4, preferably 9 in 3×3 grid)
- [ ] **Via thermal relief:** Use 0.1mm thermal relief spokes to prevent tombstoning during reflow

### Power Trace Verification

- [ ] **10A Fused Live Bus:** Trace width ≥7mm (1oz) or ≥3.5mm (2oz), OR use **polygon pours** on both layers stitched with vias
- [ ] **Polygon pours:** Verify HV Live and Neutral use polygon pours (zones) rather than simple traces for better current distribution
- [ ] **Temperature rise:** Verify trace width keeps temperature rise <10°C at 10A

### Thermal Considerations

- [ ] **NTC placement:** Verify NTC analog front-end (R1/R2, C8/C9, R_HUM/C_HUM) is **≥20mm away** from F1 (fuse) and K2 (pump relay)
- [ ] **Thermal vias:** Do NOT place thermal vias near NTC circuits (they conduct heat from bottom layer)

### Safety Verification

- [ ] **Isolation slots:** Verify 6mm isolation slots between HV and LV sections
- [ ] **Keep-out zones:** Verify 6mm keep-out zone between HV traces and LV ground pour
- [ ] **Edge clearance:** Verify 5mm edge clearance for all components (except connectors)

### Design Rule Check (DRC)

- [ ] **Run DRC:** Verify all design rules pass (trace width, clearance, via size, etc.)
- [ ] **Gerber review:** Review all Gerber layers before sending to manufacturer
- [ ] **Assembly drawing:** Verify component placement matches assembly drawing

**Once all items are checked, the board is ready for fabrication.**
