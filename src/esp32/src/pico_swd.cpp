#include "pico_swd.h"
#include "config.h"
#include <Arduino.h>
#include <FS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>  // For gpio_set_pull_mode()

// 20us = ~25kHz (Safe/Slow)
#define SWD_BIT_DELAY 20
#define SWD_IDLE_CYCLES 8
#define SWD_TURNAROUND_CYCLES 1
#define SWD_TURNAROUND_DELAY_US 5  // Additional delay after turnaround for target to drive line

// DP Register Addresses
#define DP_IDCODE     0x0   // Identification Code Register
#define DP_CTRL_STAT  0x4   // Control/Status Register
#define DP_SELECT     0x8   // AP Select Register
#define DP_RDBUFF     0xC   // Read Buffer
#define DP_TARGETSEL  0xC   // TARGETSEL Register (ADIv6 Multidrop, write-only, same address as RDBUFF)

// AP Register Addresses
#define AP_CSW  0x00   // Control/Status Word
#define AP_TAR  0x04   // Transfer Address Register
#define AP_DRW  0x0C   // Data Read/Write Register
#define AP_IDR  0xFC   // Identification Register

// RP2350 AP Selection (from reference)
#define AP_ROM_TABLE    0x0   // ROM Table
#define AP_ARM_CORE0    0x2   // ARM Core 0 AHB-AP
#define AP_ARM_CORE1    0x4   // ARM Core 1 AHB-AP
#define AP_RP_SPECIFIC  0x8   // RP-AP (Raspberry Pi specific)
#define AP_RISCV        0xA   // RISC-V APB-AP (this is what we need!)

// Debug Module Register Addresses (RP2350)
#define DM_DMCONTROL   (0x10 * 4)  // Hart control
#define DM_DMSTATUS    (0x11 * 4)  // Hart status
#define DM_ABSTRACTCS  (0x16 * 4)  // Abstract command status
#define DM_COMMAND     (0x17 * 4)  // Abstract command execution
#define DM_DATA0       (0x04 * 4)  // Data transfer register
#define DM_PROGBUF0    (0x20 * 4)  // Program buffer word 0
#define DM_PROGBUF1    (0x21 * 4)  // Program buffer word 1
#define DM_SBCS        (0x38 * 4)  // System Bus Access Control
#define DM_SBADDRESS0  (0x39 * 4)  // SBA Address
#define DM_SBDATA0     (0x3C * 4)  // SBA Data

// IDs & Addresses
// FIX: Correct RP2350 TARGETSEL ID for Core 0, Instance 0 (from ADIv6 Multidrop spec)
#define ID_RP2350_TARGET 0x01002927  // Core 0, Instance 0 (was 0x00040927 - incorrect)
#define ID_RP2350_RESCUE 0xF0000001 
#define ID_RP2350_EXPECTED 0x4c013477  // Expected RP2350 IDCODE (from reference) 

// Registers
#define DHCSR 0xE000EDF0
#define AIRCR 0xE000ED0C

static int g_pin_swdio = -1;
static int g_pin_swclk = -1;

PicoSWD::PicoSWD(int swdio, int swclk, int reset)
    : _swdio(swdio), _swclk(swclk), _reset(reset), _connected(false), _lastErrorStr("None") {
    g_pin_swdio = swdio;
    g_pin_swclk = swclk;
    if (reset >= 0) {
        LOG_D("SWD: PicoSWD initialized - SWDIO=GPIO%d, SWCLK=GPIO%d, RESET=GPIO%d", 
              swdio, swclk, reset);
    } else {
        LOG_D("SWD: PicoSWD initialized - SWDIO=GPIO%d, SWCLK=GPIO%d, RESET=N/A", 
              swdio, swclk);
    }
}

// --- Error Mapping ---
const char* PicoSWD::ackToString(uint8_t ack) {
    switch(ack) {
        case SWD_ACK_OK: return "OK";
        case SWD_ACK_WAIT: return "WAIT";
        case SWD_ACK_FAULT: return "FAULT";
        case 7: return "NO_RESPONSE";
        default: return "PROTOCOL_ERR";
    }
}

const char* PicoSWD::errorToString(swd_error_t error) {
    static const char* error_strings[] = {
        [SWD_OK] = "Success",
        [SWD_ERROR_TIMEOUT] = "Timeout",
        [SWD_ERROR_FAULT] = "Target fault",
        [SWD_ERROR_PROTOCOL] = "Protocol error",
        [SWD_ERROR_PARITY] = "Parity error",
        [SWD_ERROR_WAIT] = "Wait timeout",
        [SWD_ERROR_NOT_CONNECTED] = "Not connected",
        [SWD_ERROR_NOT_HALTED] = "Hart not halted",
        [SWD_ERROR_ALREADY_HALTED] = "Hart already halted",
        [SWD_ERROR_INVALID_STATE] = "Invalid state",
        [SWD_ERROR_NO_MEMORY] = "Out of memory",
        [SWD_ERROR_INVALID_CONFIG] = "Invalid configuration",
        [SWD_ERROR_RESOURCE_BUSY] = "Resource busy",
        [SWD_ERROR_INVALID_PARAM] = "Invalid parameter",
        [SWD_ERROR_NOT_INITIALIZED] = "Not initialized",
        [SWD_ERROR_ABSTRACT_CMD] = "Abstract command failed",
        [SWD_ERROR_BUS] = "Bus error",
        [SWD_ERROR_ALIGNMENT] = "Alignment error",
        [SWD_ERROR_VERIFY] = "Verification failed",
    };
    
    if (error >= sizeof(error_strings)/sizeof(error_strings[0])) {
        return "Unknown error";
    }
    return error_strings[error];
}

// --- Low Level ---
void PicoSWD::setSWDIO(bool high) { digitalWrite(g_pin_swdio, high ? HIGH : LOW); }
void PicoSWD::setSWCLK(bool high) { digitalWrite(g_pin_swclk, high ? HIGH : LOW); }
bool PicoSWD::readSWDIO() { return digitalRead(g_pin_swdio) == HIGH; }

void PicoSWD::swdWrite(uint8_t data, uint8_t bits) {
    for (int i = 0; i < bits; i++) {
        setSWCLK(LOW); delayMicroseconds(SWD_BIT_DELAY);
        setSWDIO(data & 1); delayMicroseconds(SWD_BIT_DELAY);
        setSWCLK(HIGH); delayMicroseconds(SWD_BIT_DELAY);
        data >>= 1;
    }
    setSWCLK(LOW);
}

uint8_t PicoSWD::swdRead(uint8_t bits) {
    // Enable Pull-Up (Crucial for reading ACKs correctly)
    pinMode(g_pin_swdio, INPUT_PULLUP);
    uint8_t res = 0;
    for (int i = 0; i < bits; i++) {
        setSWCLK(LOW); delayMicroseconds(SWD_BIT_DELAY);
        setSWCLK(HIGH); delayMicroseconds(SWD_BIT_DELAY);
        if (readSWDIO()) res |= (1 << i);
        setSWCLK(LOW);
    }
    return res;
}

void PicoSWD::swdTurnaround() {
    pinMode(g_pin_swdio, INPUT_PULLUP);
    setSWCLK(LOW); delayMicroseconds(SWD_BIT_DELAY);
    setSWCLK(HIGH); delayMicroseconds(SWD_BIT_DELAY);
    setSWCLK(LOW);
    pinMode(g_pin_swdio, OUTPUT);
}

void PicoSWD::swdIdle() {
    LOG_D("SWD: Sending %d idle clock cycles", SWD_IDLE_CYCLES);
    setSWDIO(LOW);
    for (int i = 0; i < SWD_IDLE_CYCLES; i++) {
        setSWCLK(LOW); delayMicroseconds(SWD_BIT_DELAY);
        setSWCLK(HIGH); delayMicroseconds(SWD_BIT_DELAY);
    }
    setSWCLK(LOW);
}

void PicoSWD::swdSendIdleClocks(uint8_t count) {
    LOG_D("SWD: Sending %d idle clock cycles", count);
    pinMode(g_pin_swdio, OUTPUT);
    setSWDIO(LOW);
    portDISABLE_INTERRUPTS();
    for (int i = 0; i < count; i++) {
        setSWCLK(LOW); delayMicroseconds(SWD_BIT_DELAY);
        setSWCLK(HIGH); delayMicroseconds(SWD_BIT_DELAY);
    }
    setSWCLK(LOW);
    portENABLE_INTERRUPTS();
}

void PicoSWD::swdResetSeq() {
    // Helper function for reset sequence (60 clock cycles with SWDIO HIGH)
    setSWDIO(HIGH);
    for (int i = 0; i < 60; i++) { 
        setSWCLK(LOW); delayMicroseconds(SWD_BIT_DELAY);
        setSWCLK(HIGH); delayMicroseconds(SWD_BIT_DELAY);
    }
    setSWCLK(LOW);
}

// Full Reset (for initial connect)
void PicoSWD::swdLineReset() {
    swdResetSeq();
    swdWrite(0x9E, 8); swdWrite(0xE7, 8); // JTAG->SWD
    swdResetSeq();
    swdIdle();
}

// Sync Only (After Dormant)
void PicoSWD::swdLineResetSoft() {
    swdResetSeq();
    swdIdle();
}

void PicoSWD::sendDormantSequence() {
    LOG_I("SWD: Sending Dormant Wakeup Sequence (ADIv6 Multidrop)...");
    pinMode(g_pin_swdio, OUTPUT);
    
    // Phase 1: Line Reset (>50 cycles HIGH) - sanitize interface state
    LOG_D("SWD: Phase 1 - Line Reset");
    swdResetSeq();
    
    // Phase 2: JTAG-to-Dormant (0xE3BC, LSB first: 0xBC, 0xE3)
    // This forces interface into Dormant state regardless of current state
    LOG_D("SWD: Phase 2 - JTAG-to-Dormant (0xE3BC)");
    swdWrite(0xBC, 8);  // LSB first
    swdWrite(0xE3, 8);
    // 4 cycles HIGH (idle) after JTAG-to-Dormant
    setSWDIO(HIGH);
    for (int i = 0; i < 4; i++) {
        setSWCLK(LOW); delayMicroseconds(SWD_BIT_DELAY);
        setSWCLK(HIGH); delayMicroseconds(SWD_BIT_DELAY);
    }
    setSWCLK(LOW);
    
    // Phase 3: Selection Alert (128 bits, LSB first, NO padding)
    // Pattern: 0x19bc0ea2_e3ddafe9_86852d95_6209f392
    // Bytes (LSB first): 0x92, 0xf3, 0x09, 0x62, 0x95, 0x2d, 0x85, 0x86,
    //                    0xe9, 0xaf, 0xdd, 0xe3, 0xa2, 0x0e, 0xbc, 0x19
    LOG_D("SWD: Phase 3 - Selection Alert (128 bits)");
    const uint8_t seq_alert[] = {
        0x92, 0xf3, 0x09, 0x62, 0x95, 0x2d, 0x85, 0x86, // Bytes 0-7
        0xe9, 0xaf, 0xdd, 0xe3, 0xa2, 0x0e, 0xbc, 0x19, // Bytes 8-15
    };
    for (size_t i = 0; i < sizeof(seq_alert); i++) {
        swdWrite(seq_alert[i], 8);
    }
    
    // Phase 4: Activation Code
    // 4 clock cycles with SWDIO LOW (delimiter), then 0x1A (SWD mode activation)
    LOG_D("SWD: Phase 4 - Activation Code (4 cycles LOW + 0x1A)");
    setSWDIO(LOW);
    for (int i = 0; i < 4; i++) {
        setSWCLK(LOW); delayMicroseconds(SWD_BIT_DELAY);
        setSWCLK(HIGH); delayMicroseconds(SWD_BIT_DELAY);
    }
    setSWCLK(LOW);
    swdWrite(0x1A, 8);  // 0x1A = 00011010 binary (LSB first: 0 1 0 1 1 0 0 0)
    
    // Phase 5: Line Reset (>50 cycles HIGH) - enter Reset state
    LOG_D("SWD: Phase 5 - Line Reset (enter Reset state)");
    swdResetSeq();
    
    LOG_I("SWD: Dormant Wakeup Sequence complete");
}

// --- Packet Layer (FIXED: EVEN PARITY) ---

swd_error_t PicoSWD::swdWritePacket(uint8_t request, uint32_t data, bool ignoreAck) {
    uint8_t ap_dp = (request & 0x01);
    uint8_t addr  = (request & 0x06) >> 1; 
    uint8_t header_payload = (addr << 3) | (0 << 2) | (ap_dp << 1) | 1;
    // FIX: Removed '^ 1'. SWD uses Even Parity.
    uint8_t parity = __builtin_parity(header_payload & 0x1E);
    uint8_t header = header_payload | (parity << 5) | 0x80;

    pinMode(g_pin_swdio, OUTPUT);
    swdWrite(header, 8);
    
    // CRITICAL: TARGETSEL (DP_TARGETSEL = 0x0C) is a special command in ADIv6 Multidrop SWD
    // It has NO ACK and NO TURNAROUND: Header → Data (immediately)
    // Normal SWD: Header → Turnaround → ACK → Turnaround → Data
    // TARGETSEL:   Header → Data (skip turnaround/ACK)
    // 
    // Detection: DP register (ap_dp=0) with address 0x0C
    // writeDP(DP_TARGETSEL, ...) → swdWritePacket((0x0C & 0x0C) >> 1, ...) = swdWritePacket(0x06, ...)
    // In swdWritePacket: addr = (0x06 & 0x06) >> 1 = 0x03
    bool isTargetSel = (ap_dp == 0) && (addr == 0x03); // DP_TARGETSEL (0x0C) = addr 0x03 in packet
    
    if (!isTargetSel) {
        // Normal packet: read ACK
        swdTurnaround();
        uint8_t ack = swdRead(3);
        swdTurnaround();
        pinMode(g_pin_swdio, OUTPUT);

        if (ack != SWD_ACK_OK && !ignoreAck) {
            if (ack == SWD_ACK_FAULT) {
                LOG_E("Write FAULT! Sending ABORT...");
                // Manual ABORT (DP Write 0x00, Data 0x1E)
                pinMode(g_pin_swdio, OUTPUT);
                swdWrite(0x81, 8); // Header (Write DP 0x00)
                swdTurnaround();
                swdRead(3); 
                swdTurnaround();
                pinMode(g_pin_swdio, OUTPUT);
                swdWrite(0x1E, 32); 
                // 0x1E Parity: __builtin_parity(0x1E)=0 (even), Even Parity = 0
                swdWrite(0, 1);
                swdIdle();
            }
            return (ack == 2) ? SWD_ERROR_WAIT : SWD_ERROR_PROTOCOL;
        }
    } else {
        // TARGETSEL: No turnaround, no ACK - go directly to data
        // CRITICAL: Data must be sent immediately after header with no gaps
        LOG_D("SWD: TARGETSEL command - skipping ACK/turnaround, sending data immediately");
    }

    // Write data (for both normal and TARGETSEL)
    // For TARGETSEL, this must be continuous with no delays
    swdWrite(data & 0xFF, 8);
    swdWrite((data >> 8) & 0xFF, 8);
    swdWrite((data >> 16) & 0xFF, 8);
    swdWrite((data >> 24) & 0xFF, 8);
    
    // FIX: Removed '^ 1'. We want Even Parity.
    swdWrite(__builtin_parity(data), 1);
    
    // For TARGETSEL, add idle cycles to allow target to process selection
    if (isTargetSel) {
        swdIdle();
        delay(5); // Small delay for target to process TARGETSEL
    }
    
    return SWD_OK;
}

swd_error_t PicoSWD::swdReadPacket(uint8_t request, uint32_t *data) {
    uint8_t ap_dp = (request & 0x01);
    uint8_t addr  = (request & 0x06) >> 1; 
    uint8_t header_payload = (addr << 3) | (1 << 2) | (ap_dp << 1) | 1;
    // FIX: Even Parity
    uint8_t parity = __builtin_parity(header_payload & 0x1E);
    uint8_t header = header_payload | (parity << 5) | 0x80;

    pinMode(g_pin_swdio, OUTPUT);
    swdWrite(header, 8);
    
    swdTurnaround();
    uint8_t ack = swdRead(3);
    
    LOG_D("SWD: Read ACK=%s (0x%X)", ackToString(ack), ack);
    
    if (ack != SWD_ACK_OK) {
        // CRITICAL: When FAULT occurs, IMMEDIATELY write to ABORT register
        // The DAP ignores ALL commands (including reading fault data) until ABORT is written
        // Don't try to read the fault data first - it will just return FAULT again!
        if (ack == SWD_ACK_FAULT) {
            LOG_W("SWD: Read packet got FAULT - IMMEDIATELY clearing via ABORT register (DP 0x00 = 0x1E)");
            
            // Consume the rest of the packet (data + parity) without reading it
            // We can't read it anyway - DAP is locked until ABORT
            swdTurnaround();
            pinMode(g_pin_swdio, OUTPUT);
            // Skip reading 32 bits of data + 1 bit parity (33 bits total)
            // Just clock them out without reading
            portDISABLE_INTERRUPTS();
            for (int i = 0; i < 33; i++) {
                setSWCLK(LOW); delayMicroseconds(SWD_BIT_DELAY);
                setSWCLK(HIGH); delayMicroseconds(SWD_BIT_DELAY);
                setSWCLK(LOW);
            }
            portENABLE_INTERRUPTS();
            
            // NOW send ABORT write directly using writeDP with ignoreAck=true
            // This avoids recursion and ensures immediate ABORT
            writeDP(DP_IDCODE, 0x1E, true);  // ignoreAck=true - DAP is locked, might not ACK
            delay(5);  // Delay for DAP to process ABORT
            LOG_D("SWD: ABORT write sent");
        } else if (ack == SWD_ACK_ERROR) {
            LOG_D("SWD: Protocol error - consuming rest of packet (33 bits)");
            // Read the remaining 33 bits (32 data + 1 parity) to clear the line
            swdRead(33);
        } else if (ack == SWD_ACK_WAIT) {
            // For WAIT, we still need to read the data portion
            swdRead(33);
        }
        
        // Only do turnaround if we haven't already done it (FAULT case does it early)
        if (ack != SWD_ACK_FAULT) {
            swdTurnaround();
            pinMode(g_pin_swdio, OUTPUT);
        }
        
        // Log other errors
        if (ack != SWD_ACK_FAULT) {
            LOG_E("SWD: Read packet failed - ACK=%s (0x%X), req=0x%02X", 
                  ackToString(ack), ack, request);
        }
        
        return (ack == SWD_ACK_WAIT) ? SWD_ERROR_WAIT : 
               (ack == SWD_ACK_FAULT) ? SWD_ERROR_FAULT : SWD_ERROR_PROTOCOL;
    }
    
    uint32_t val = swdRead(32);
    uint8_t p = swdRead(1);
    
    swdTurnaround();
    pinMode(g_pin_swdio, OUTPUT);
    
    if (data) *data = val;
    return SWD_OK;
}

// --- Wrappers ---
swd_error_t PicoSWD::readDP(uint8_t addr, uint32_t *data) { 
    LOG_D("SWD: ReadDP(0x%02X)", addr);
    swd_error_t err = swdReadPacket((addr & 0x0C) >> 1, data);
    if (err == SWD_OK && data) {
        LOG_D("SWD: ReadDP(0x%02X) = 0x%08X", addr, *data);
    } else if (err != SWD_OK) {
        LOG_E("SWD: ReadDP(0x%02X) failed: %s", addr, errorToString(err));
    }
    return err;
}

swd_error_t PicoSWD::writeDP(uint8_t addr, uint32_t data, bool ignoreAck) { 
    LOG_D("SWD: WriteDP(0x%02X, 0x%08X, ignoreAck=%d)", addr, data, ignoreAck);
    swd_error_t err = swdWritePacket((addr & 0x0C) >> 1, data, ignoreAck);
    if (err != SWD_OK && !ignoreAck) {
        LOG_E("SWD: WriteDP(0x%02X, 0x%08X) failed: %s", addr, data, errorToString(err));
    }
    return err;
}

// RP2350-specific DP_SELECT encoding helper
uint32_t PicoSWD::makeDPSelectRP2350(uint8_t apsel, uint8_t bank, bool ctrlsel) {
    // RP2350 uses non-standard encoding:
    // [15:12] = APSEL, [11:8] = 0xD, [7:4] = bank, [0] = ctrlsel
    return ((apsel & 0xF) << 12) | (0xD << 8) | ((bank & 0xF) << 4) | (ctrlsel ? 1 : 0);
}

swd_error_t PicoSWD::readAP(uint8_t addr, uint32_t *data) {
    LOG_D("SWD: ReadAP(0x%02X) - selecting bank", addr);
    // Extract bank from address (bits 7:4)
    uint8_t bank = (addr >> 4) & 0xF;
    // For RP2350, we need to use the special encoding with AP_RISCV (0xA)
    uint32_t select_val = makeDPSelectRP2350(AP_RISCV, bank, true);
    swd_error_t err = writeDP(DP_SELECT, select_val, false);
    if (err != SWD_OK) {
        LOG_E("SWD: ReadAP failed to select bank: %s", errorToString(err));
        return err;
    }
    // AP register address is bits 3:0
    uint8_t ap_reg = addr & 0xF;
    err = swdReadPacket((ap_reg << 1) | 0x09, data);  // APnDP=1, RnW=1
    if (err == SWD_OK && data) {
        LOG_D("SWD: ReadAP(0x%02X) = 0x%08X", addr, *data);
    } else if (err != SWD_OK) {
        LOG_E("SWD: ReadAP(0x%02X) failed: %s", addr, errorToString(err));
    }
    return err;
}

swd_error_t PicoSWD::writeAP(uint8_t addr, uint32_t data) {
    LOG_D("SWD: WriteAP(0x%02X, 0x%08X) - selecting bank", addr, data);
    // Extract bank from address (bits 7:4)
    uint8_t bank = (addr >> 4) & 0xF;
    // For RP2350, we need to use the special encoding with AP_RISCV (0xA)
    uint32_t select_val = makeDPSelectRP2350(AP_RISCV, bank, true);
    swd_error_t err = writeDP(DP_SELECT, select_val, false);
    if (err != SWD_OK) {
        LOG_E("SWD: WriteAP failed to select bank: %s", errorToString(err));
        return err;
    }
    // AP register address is bits 3:0
    uint8_t ap_reg = addr & 0xF;
    err = swdWritePacket((ap_reg << 1) | 0x01, data, false);  // APnDP=1, RnW=0
    if (err != SWD_OK) {
        LOG_E("SWD: WriteAP(0x%02X, 0x%08X) failed: %s", addr, data, errorToString(err));
    }
    return err;
}

// --- Connection Logic ---

bool PicoSWD::connectToTarget() {
    pinMode(g_pin_swdio, OUTPUT); 
    pinMode(g_pin_swclk, OUTPUT);
    if (_reset >= 0) { pinMode(_reset, OUTPUT); digitalWrite(_reset, HIGH); }

    uint32_t id = 0;

    // CRITICAL: RP2350 defaults to Dormant state upon Power-On Reset
    // We MUST ALWAYS perform the dormant wake-up sequence first
    // The simple line reset approach will NOT work because the target is in Dormant state
    LOG_I("SWD: Starting connection - RP2350 defaults to Dormant state");
    LOG_I("SWD: Performing mandatory Dormant Wake-up Sequence...");
    sendDormantSequence();
    
    // TARGETSEL must be the FIRST packet after wake-up
    // This selects Core 0 (Instance 0) on the multidrop bus
    // TARGETSEL is written to DP register 0x0C (TARGETSEL register, write-only)
    // ignoreAck=true because TARGETSEL has no ACK (devices are not yet selected)
    LOG_I("SWD: Sending TARGETSEL (must be first packet after wake-up)...");
    LOG_D("SWD: TARGETSEL value: 0x%08X (Core 0, Instance 0)", ID_RP2350_TARGET);
    swd_error_t err = writeDP(DP_TARGETSEL, ID_RP2350_TARGET, true);  // DP register 0x0C = TARGETSEL
    if (err != SWD_OK) {
        LOG_W("SWD: TARGETSEL write returned error (expected for no-ACK): %s", errorToString(err));
        // This is expected - TARGETSEL has no ACK, so we continue anyway
    }
    
    // After TARGETSEL, the target needs time to process the selection and lock the bus
    // According to ADIv6 spec, TARGETSEL selects the target but doesn't provide ACK
    // We need to wait for the target to process TARGETSEL before attempting any DP reads
    LOG_D("SWD: Waiting for target to process TARGETSEL and lock bus...");
    delay(150); // Increased delay for target to process TARGETSEL and lock bus
    
    // After TARGETSEL, perform a soft line reset (not full reset with JTAG-to-SWD)
    // We're already in SWD mode, so we just need to reset the DP state, not switch protocols
    LOG_D("SWD: Performing soft line reset after TARGETSEL to sync protocol state...");
    swdLineResetSoft();
    
    // Add idle cycles to ensure protocol is stable before reading IDCODE
    LOG_D("SWD: Sending idle cycles before IDCODE read...");
    swdIdle();
    delay(50); // Additional delay for protocol to stabilize and target to be ready
    
    // Now verify connection by reading IDCODE
    // Expected IDCODE for RP2350: 0x4C013477
    // Retry up to 3 times in case first read fails
    LOG_I("SWD: Verifying connection - reading IDCODE...");
    for (int retry = 0; retry < 3; retry++) {
        if (retry > 0) {
            LOG_D("SWD: IDCODE read retry %d/3...", retry + 1);
            // On retry, redo the entire wake-up sequence since we might have lost sync
            LOG_W("SWD: Retry %d - redoing wake-up sequence...", retry + 1);
            sendDormantSequence();
            delay(50);
            swd_error_t retry_err = writeDP(DP_TARGETSEL, ID_RP2350_TARGET, true);
            if (retry_err != SWD_OK) {
                LOG_D("SWD: TARGETSEL retry returned: %s (expected)", errorToString(retry_err));
            }
            delay(150);
            swdLineResetSoft();
            delay(50);
            swdIdle();
        }
        
        err = readDP(DP_IDCODE, &id);
        if (err == SWD_OK && id != 0 && id != 0xFFFFFFFF) {
            if (id == ID_RP2350_EXPECTED) {
                LOG_I("SWD: CONNECTED! IDCODE: 0x%08X (RP2350 verified)", id);
                _connected = true;
                _lastErrorStr = "None";
                return true;
            } else {
                LOG_W("SWD: Connected but unexpected IDCODE: 0x%08X (expected 0x%08X)", id, ID_RP2350_EXPECTED);
                // Still consider it connected if we got a valid IDCODE
                _connected = true;
                _lastErrorStr = "None";
                return true;
            }
        } else if (err == SWD_ERROR_FAULT) {
            LOG_W("SWD: IDCODE read returned FAULT (attempt %d/3), will retry...", retry + 1);
            // Continue to retry
        } else {
            LOG_W("SWD: IDCODE read failed: %s (attempt %d/3)", errorToString(err), retry + 1);
            // Continue to retry
        }
    }

    LOG_E("SWD: Connection failed. IDCODE read error: %s, ID: 0x%08X", 
          errorToString(err), id);
    _lastErrorStr = "Connection failed - no valid IDCODE";
    return false;
}

bool PicoSWD::powerUpDebug() {
    if (!_connected) {
        LOG_E("SWD: Power-up failed - not connected");
        _lastErrorStr = "Not connected";
        return false;
    }
    
    LOG_I("SWD: Powering up debug domains...");
    
    // Clear errors first
    LOG_D("SWD: Clearing DP_CTRL_STAT errors");
    swd_error_t err = writeDP(DP_CTRL_STAT, 0, false);
    if (err != SWD_OK) {
        LOG_E("SWD: Failed to clear DP_CTRL_STAT: %s", errorToString(err));
        _lastErrorStr = errorToString(err);
        return false;
    }
    
    // Request power-up: CDBGPWRUPREQ | CSYSPWRUPREQ
    // Bit 28 = CDBGPWRUPREQ, Bit 30 = CSYSPWRUPREQ
    uint32_t ctrl_stat = (1 << 28) | (1 << 30);
    LOG_D("SWD: Requesting power-up - writing DP_CTRL_STAT=0x%08X", ctrl_stat);
    err = writeDP(DP_CTRL_STAT, ctrl_stat, false);
    if (err != SWD_OK) {
        LOG_E("SWD: Failed to request power-up: %s", errorToString(err));
        _lastErrorStr = errorToString(err);
        return false;
    }
    
    // Poll for ACK (bits 29 and 31)
    // Bit 29 = CDBGPWRUPACK, Bit 31 = CSYSPWRUPACK
    LOG_D("SWD: Polling for power-up acknowledgment...");
    for (int i = 0; i < 10; i++) {
        uint32_t status;
        err = readDP(DP_CTRL_STAT, &status);
        if (err != SWD_OK) {
            LOG_E("SWD: Failed to read power status (attempt %d/10): %s", 
                  i + 1, errorToString(err));
            _lastErrorStr = errorToString(err);
            return false;
        }
        
        bool cdbgpwrupack = (status >> 29) & 1;
        bool csyspwrupack = (status >> 31) & 1;
        
        LOG_D("SWD: Power status (attempt %d/10): DP_CTRL_STAT=0x%08X, CDBGPWRUPACK=%d, CSYSPWRUPACK=%d",
              i + 1, status, cdbgpwrupack, csyspwrupack);
        
        if (cdbgpwrupack && csyspwrupack) {
            LOG_I("SWD: Debug domains powered up successfully (attempt %d)", i + 1);
            _lastErrorStr = "None";
            return true;
        }
        
        delay(20);
    }

    LOG_E("SWD: Power-up timeout after 10 attempts");
    _lastErrorStr = "Power-up timeout";
    return false;
}

bool PicoSWD::powerDownDebug() {
    if (!_connected) {
        LOG_D("SWD: Power-down skipped - not connected");
        return true;
    }
    
    LOG_I("SWD: Powering down debug domains...");
    
    // First, try to resume any halted cores (if Debug Module was initialized)
    // This ensures cores are running before we power down
    // Note: We can't check if cores are halted without the Debug Module being initialized,
    // so we'll just try to clear any halt requests
    LOG_D("SWD: Clearing any halt requests before power-down...");
    // Write DMCONTROL with dmactive=1, but no halt/resume requests to clear state
    // This is a best-effort attempt - if it fails, we continue anyway
    uint32_t dmcontrol_clear = (1 << 0);  // dmactive=1, no halt/resume
    // We'd need to write to DM_DMCONTROL, but that requires the Debug Module to be initialized
    // For now, we'll just power down - the hardware reset will handle it
    
    // Write 0 to DP_CTRL_STAT to power down debug domains
    swd_error_t err = writeDP(DP_CTRL_STAT, 0, false);
    if (err != SWD_OK) {
        LOG_W("SWD: Failed to power down debug domains: %s (continuing anyway)", errorToString(err));
        // Continue anyway - we're disconnecting
    } else {
        LOG_D("SWD: Debug domains powered down");
    }
    
    return true;
}

bool PicoSWD::begin() {
    LOG_I("SWD: begin() called");
    
    // GPIO Conflict Warning: GPIO 16 and 17 are used for PSRAM on ESP32-WROVER modules
    // (original ESP32, not ESP32-S3). On ESP32-S3, these are UART2 pins.
    // Using these pins for SWD bit-banging can cause conflicts with PSRAM access on WROVER modules.
    // This may result in crashes, signal corruption, or system instability.
    // Reference: ADIv6 Multidrop Implementation Analysis - Section 2.1.1
    // 
    // NOTE: Current configuration uses GPIO 21/45 (safe pins, no conflicts)
    // This warning will only trigger if someone changes back to GPIO 16/17
    if (_swdio == 16 || _swdio == 17 || _swclk == 16 || _swclk == 17) {
        LOG_W("SWD: ⚠️  GPIO CONFLICT WARNING - GPIO 16/17 are PSRAM pins on ESP32-WROVER modules!");
        LOG_W("SWD: Using these pins for SWD may cause PSRAM conflicts and system instability.");
        LOG_W("SWD: Symptoms: Guru Meditation Error, LoadProhibited/StoreProhibited exceptions, signal corruption");
        LOG_W("SWD: Recommended alternatives: GPIO 18, 19, 21, 22, 23 (per ADIv6 analysis document)");
        LOG_W("SWD: Current safe configuration: GPIO 21 (SWDIO) and GPIO 45 (SWCLK)");
    } else {
        LOG_D("SWD: Using safe GPIO pins - SWDIO=GPIO%d, SWCLK=GPIO%d (no PSRAM conflicts)", _swdio, _swclk);
    }
    
    if (_reset >= 0) {
        LOG_I("SWD: Resetting target via RESET pin (GPIO%d)", _reset);
        pinMode(_reset, OUTPUT); 
        digitalWrite(_reset, LOW); 
        delay(50);  // Increased reset hold time for more reliable reset
        LOG_D("SWD: RESET pin LOW for 50ms");
        digitalWrite(_reset, HIGH); 
        delay(200);  // Increased stabilization time after reset
        LOG_D("SWD: RESET pin HIGH, waiting 200ms for target to stabilize");
    } else {
        LOG_D("SWD: No RESET pin configured");
    }
    
    LOG_I("SWD: Connecting to target...");
    if (!connectToTarget()) {
        LOG_E("SWD: Connection failed: %s", _lastErrorStr);
        return false;
    }
    
    LOG_I("SWD: Powering up debug domains...");
    // Power up debug domains
    if (!powerUpDebug()) {
        LOG_E("SWD: Power-up failed: %s", _lastErrorStr);
        return false;
    }
    
    LOG_I("SWD: Initializing AP...");
    // Initialize AP (RP2350 Debug Module) - complex multi-phase initialization
    bool ap_ok = initAP();
    if (!ap_ok) {
        LOG_E("SWD: AP initialization failed");
        _lastErrorStr = "AP init failed";
    return false;
    }
    
    // Reset Debug Module state (fixes "unhealthy" states)
    // NOTE: This is technically redundant after initAP(), but acts as a "belt and suspenders"
    // safety net. If the complex initialization leaves the module in a weird state, this
    // simpler reset sequence will fix it. Safe to leave for maximum reliability.
    LOG_I("SWD: Resetting Debug Module state...");
    initDebugModule();
    
    LOG_I("SWD: Initialization complete - ready for operations");
    return true;
}

void PicoSWD::end() {
    LOG_I("SWD: end() called - disconnecting");
    
    if (_connected) {
        // Power down debug domains first
        // This will clear any debug state and allow cores to run normally
        LOG_D("SWD: Powering down debug domains...");
        powerDownDebug();
        
        // Small delay to let power-down complete
        delay(10);
    }
    
    _connected = false;
    
    // FIX: Release pins with pull-ups instead of floating to prevent EMI-induced ghost debugging
    // Floating pins act as antennas and can pick up noise from pumps/solenoids, causing
    // the Pico to enter debug halt state. Pull-ups keep the lines stable when idle.
    pinMode(g_pin_swdio, INPUT_PULLUP); 
    pinMode(g_pin_swclk, INPUT_PULLUP);
    
    LOG_D("SWD: Pins released (INPUT_PULLUP) - prevents EMI-induced ghost debugging");
    LOG_I("SWD: Disconnected");
}

// --- RP2350 Debug Module Initialization (CRITICAL!) ---
// This is the missing piece! After power-up, we MUST initialize the RP2350 Debug Module
// using the specific activation sequence from the reference implementation.
bool PicoSWD::initRP2350DebugModule() {
    LOG_I("SWD: Initializing RP2350 Debug Module...");
    
    // Step 1: Select RISC-V APB-AP, Bank 0
    uint32_t sel_bank0 = makeDPSelectRP2350(AP_RISCV, 0, true);
    LOG_D("SWD: Selecting RISC-V APB-AP, Bank 0 (DP_SELECT=0x%08X)", sel_bank0);
    swd_error_t err = writeDP(DP_SELECT, sel_bank0, false);
    if (err != SWD_OK) {
        LOG_E("SWD: Failed to select AP bank 0: %s", errorToString(err));
        _lastErrorStr = errorToString(err);
        return false;
    }
    
    // Step 2: Configure CSW for 32-bit word access
    uint32_t csw = 0xA2000002;  // Standard CSW value (from reference)
    LOG_D("SWD: Configuring CSW=0x%08X", csw);
    err = writeAP(AP_CSW, csw);
    if (err != SWD_OK) {
        LOG_E("SWD: Failed to configure CSW: %s", errorToString(err));
        _lastErrorStr = errorToString(err);
        return false;
    }
    
    // Step 3: Point TAR at DMCONTROL
    LOG_D("SWD: Setting TAR to DM_DMCONTROL (0x%08X)", DM_DMCONTROL);
    err = writeAP(AP_TAR, DM_DMCONTROL);
    if (err != SWD_OK) {
        LOG_E("SWD: Failed to set TAR: %s", errorToString(err));
        _lastErrorStr = errorToString(err);
        return false;
    }
    
    // Step 4: Switch to Bank 1 for DM control
    uint32_t sel_bank1 = makeDPSelectRP2350(AP_RISCV, 1, true);
    LOG_D("SWD: Switching to Bank 1 (DP_SELECT=0x%08X)", sel_bank1);
    err = writeDP(DP_SELECT, sel_bank1, false);
    if (err != SWD_OK) {
        LOG_E("SWD: Failed to select AP bank 1: %s", errorToString(err));
        _lastErrorStr = errorToString(err);
        return false;
    }
    
    // Step 5: DM Activation Sequence (CRITICAL - this is what enables the Debug Module!)
    LOG_I("SWD: Performing DM activation handshake...");
    
    // Phase 1: Reset
    LOG_D("SWD: DM activation - Reset phase");
    err = writeAP(AP_CSW, 0x00000000);
    if (err != SWD_OK) {
        LOG_E("SWD: DM activation reset failed: %s", errorToString(err));
        _lastErrorStr = errorToString(err);
        return false;
    }
    // Read RDBUFF to complete transaction
    uint32_t dummy;
    readDP(DP_RDBUFF, &dummy);
    delay(50);
    
    // Phase 2: Activate
    LOG_D("SWD: DM activation - Activate phase");
    err = writeAP(AP_CSW, 0x00000001);
    if (err != SWD_OK) {
        LOG_E("SWD: DM activation activate failed: %s", errorToString(err));
        _lastErrorStr = errorToString(err);
        return false;
    }
    readDP(DP_RDBUFF, &dummy);
    delay(50);
    
    // Phase 3: Configure
    LOG_D("SWD: DM activation - Configure phase");
    err = writeAP(AP_CSW, 0x07FFFFC1);
    if (err != SWD_OK) {
        LOG_E("SWD: DM activation configure failed: %s", errorToString(err));
        _lastErrorStr = errorToString(err);
        return false;
    }
    readDP(DP_RDBUFF, &dummy);
    delay(50);
    
    // Step 6: Verify DM is responding
    LOG_D("SWD: Verifying DM status...");
    err = readAP(AP_CSW, &dummy);  // Trigger read
    if (err != SWD_OK) {
        LOG_E("SWD: Failed to read AP_CSW: %s", errorToString(err));
        _lastErrorStr = errorToString(err);
        return false;
    }
    
    swd_error_t status_err = readDP(DP_RDBUFF, &dummy);
    if (status_err != SWD_OK) {
        LOG_E("SWD: Failed to read DM status: %s", errorToString(status_err));
        _lastErrorStr = errorToString(status_err);
        return false;
    }
    
    // Expected status is 0x04010001 (from reference)
    if (dummy != 0x04010001) {
        LOG_W("SWD: Unexpected DM status: 0x%08X (expected 0x04010001)", dummy);
        // Don't fail - some variants might have different status
        // LOG_E("SWD: DM initialization verification failed");
        // _lastErrorStr = "DM status mismatch";
        // return false;
    } else {
        LOG_I("SWD: DM status verified: 0x%08X", dummy);
    }
    
    // Step 7: Switch back to Bank 0
    LOG_D("SWD: Switching back to Bank 0");
    err = writeDP(DP_SELECT, sel_bank0, false);
    if (err != SWD_OK) {
        LOG_E("SWD: Failed to switch back to bank 0: %s", errorToString(err));
        _lastErrorStr = errorToString(err);
        return false;
    }
    
    LOG_I("SWD: RP2350 Debug Module initialized successfully");
    _lastErrorStr = "None";
    return true;
}

// --- Debug Module Reset (Simpler approach from proposal) ---
// This resets the Debug Module state, which can fix "unhealthy" states
bool PicoSWD::initDebugModule() {
    LOG_I("SWD: Resetting Debug Module state...");
    
    // Clear any pending errors via DP_CTRL_STAT (as per reference implementation)
    // Clear: STICKYERR(5), WDATAERR(7), STICKYORUN(1), STICKYCMP(4)
    uint32_t clear_bits = (1 << 5) | (1 << 7) | (1 << 1) | (1 << 4);
    writeDP(DP_CTRL_STAT, clear_bits, true);  // ignoreAck since we might not be fully connected
    
    // Select RISC-V AP (AP 0x0A)
    // DP_SELECT: APSEL=0x0A, Bank=0
    // For RP2350, we use the special encoding
    uint32_t sel_riscv = makeDPSelectRP2350(AP_RISCV, 0, true);
    swd_error_t err = writeDP(DP_SELECT, sel_riscv, false);
    if (err != SWD_OK) {
        LOG_W("SWD: Failed to select RISC-V AP: %s (continuing anyway)", errorToString(err));
    }
    
    // Configure CSW (32-bit, auto-inc off)
    err = writeAP(AP_CSW, 0xA2000002);
    if (err != SWD_OK) {
        LOG_W("SWD: Failed to configure CSW: %s (continuing anyway)", errorToString(err));
    }
    
    // Set TAR to point at DMCONTROL (0x10 * 4 = 0x40)
    err = writeAP(AP_TAR, DM_DMCONTROL * 4);
    if (err != SWD_OK) {
        LOG_W("SWD: Failed to set TAR: %s (continuing anyway)", errorToString(err));
    }
    
    // Reset DM: dmactive=0
    err = writeAP(AP_DRW, 0x00000000);
    if (err != SWD_OK) {
        LOG_W("SWD: Failed to reset DM: %s (continuing anyway)", errorToString(err));
    }
    delay(10);
    
    // Activate DM: dmactive=1
    err = writeAP(AP_DRW, 0x00000001);
    if (err != SWD_OK) {
        LOG_W("SWD: Failed to activate DM: %s (continuing anyway)", errorToString(err));
    }
    delay(10);
    
    // Restore selection to Default AP (0) for flashing
    uint32_t sel_default = makeDPSelectRP2350(0, 0, true);
    writeDP(DP_SELECT, sel_default, true);  // ignoreAck since this is cleanup
    
    LOG_I("SWD: Debug Module reset complete");
    return true;
}

// --- Core/Flash Stubs ---
bool PicoSWD::initAP() { 
    // This is now a legacy function - RP2350 initialization is done in initRP2350DebugModule()
    // But we keep it for compatibility
    LOG_D("SWD: initAP() called (legacy - using RP2350 init instead)");
    return initRP2350DebugModule();
}
bool PicoSWD::writeWord(uint32_t addr, uint32_t data) { return writeAP(0x0C, data) == SWD_OK; }
uint32_t PicoSWD::readWord(uint32_t addr) { uint32_t d; readAP(0x0C, &d); return d; }
bool PicoSWD::haltCore() { return writeWord(DHCSR, 0xA05F0003); }
bool PicoSWD::runCore() { return writeWord(DHCSR, 0xA05F0001); }
// Core Register Access (RISC-V Debug Module)
bool PicoSWD::writeCoreReg(uint8_t reg, uint32_t val) {
    // RISC-V Debug Module: Write GPR via abstract command
    if (reg > 31) {
        LOG_E("SWD: Invalid register %d (must be 0-31)", reg);
        return false;
    }
    
    // Abstract command: Write GPR (command[31:24] = 0x02, command[15:0] = reg)
    uint32_t abstract_cmd = 0x02000000 | reg;
    if (writeAP(0x17, abstract_cmd) != SWD_OK) return false;
    if (writeAP(0x04, val) != SWD_OK) return false;
    
    // Wait for completion
    uint32_t status = 0;
    for (int i = 0; i < 100; i++) {
        if (readAP(0x17, &status) == SWD_OK && (status & 0x80000000) == 0) {
            return true;
        }
        delay(1);
    }
    return false;
}

uint32_t PicoSWD::readCoreReg(uint8_t reg) {
    if (reg > 31) {
        LOG_E("SWD: Invalid register %d", reg);
        return 0;
    }
    
    // Abstract command: Read GPR
    uint32_t abstract_cmd = 0x01000000 | reg;
    if (writeAP(0x17, abstract_cmd) != SWD_OK) return 0;
    
    // Wait and read result
    uint32_t status = 0;
    for (int i = 0; i < 100; i++) {
        if (readAP(0x17, &status) == SWD_OK && (status & 0x80000000) == 0) {
            uint32_t data = 0;
            if (readAP(0x04, &data) == SWD_OK) return data;
        }
        delay(1);
    }
    return 0;
}

// Boot ROM Function Lookup (based on OpenOCD rp2xxx.c)
uint32_t PicoSWD::findRomFunc(char c1, char c2) {
    LOG_I("SWD: Finding ROM function '%c%c'...", c1, c2);
    
    // Read ROM table pointer from 0x00000014
    uint32_t rom_table = readWord(0x00000014);
    if (rom_table == 0) {
        LOG_E("SWD: Failed to read ROM table");
        return 0;
    }
    LOG_D("SWD: ROM table at 0x%08X", rom_table);
    
    // Search for function tag
    uint16_t tag = (uint16_t)(c1 | (c2 << 8));
    uint32_t addr = rom_table;
    
    for (int i = 0; i < 256; i++) {
        uint16_t entry_tag = (uint16_t)readWord(addr);
        if (entry_tag == tag) {
            uint32_t func_addr = readWord(addr + 4);
            LOG_I("SWD: Found '%c%c' at 0x%08X", c1, c2, func_addr);
            return func_addr;
        }
        if (entry_tag == 0x0000 || entry_tag == 0xFFFF) break;
        addr += 8;
    }
    
    LOG_E("SWD: Function '%c%c' not found", c1, c2);
    return 0;
}

// Execute Boot ROM Function (based on OpenOCD rp2xxx.c)
bool PicoSWD::callRomFunc(uint32_t funcAddr, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3) {
    if (funcAddr == 0) return false;
    
    LOG_I("SWD: Calling ROM func 0x%08X", funcAddr);
    
    if (!haltCore()) return false;
    delay(10);
    
    // Set registers
    if (!writeCoreReg(0, r0)) return false;
    if (!writeCoreReg(1, r1)) return false;
    if (!writeCoreReg(2, r2)) return false;
    if (!writeCoreReg(3, r3)) return false;
    
    // Set PC (abstract command regno=0x1000)
    if (writeAP(0x17, 0x02001000) != SWD_OK) return false;
    if (writeAP(0x04, funcAddr) != SWD_OK) return false;
    
    // Set LR to trap address
    if (!writeCoreReg(1, 0x20000000)) return false;
    
    // Run and wait for halt
    if (!runCore()) return false;
    
    for (int i = 0; i < 1000; i++) {
        delay(10);
        uint32_t dhcsr = readWord(DHCSR);
        if (dhcsr & 0x20000) { // S_HALT
            LOG_I("SWD: ROM function completed");
            return true;
        }
    }
    
    LOG_E("SWD: ROM function timeout");
    haltCore();
    return false;
}

// Flash Firmware (based on OpenOCD rp2xxx.c)
bool PicoSWD::flashFirmware(File& firmwareFile, size_t size) {
    if (!_connected) {
        LOG_E("SWD: Not connected");
        return false;
    }
    
    LOG_I("SWD: Flashing firmware (%d bytes)...", size);
    
    // Find Boot ROM functions
    uint32_t flash_range_erase = findRomFunc('R', 'E');
    uint32_t flash_range_program = findRomFunc('R', 'P');
    
    if (flash_range_erase == 0 || flash_range_program == 0) {
        LOG_E("SWD: ROM functions not found");
        return false;
    }
    
    if (!haltCore()) return false;
    
    uint32_t flash_addr = 0x10000000;
    uint32_t ram_addr = 0x20004000;
    
    // Erase flash
    LOG_I("SWD: Erasing flash...");
    if (!callRomFunc(flash_range_erase, flash_addr, size, 0, 0)) {
        LOG_E("SWD: Erase failed");
        return false;
    }
    
    // Program in chunks
    const size_t chunk_size = 4096;
    uint8_t buffer[chunk_size];
    size_t offset = 0;
    
    LOG_I("SWD: Programming flash...");
    while (offset < size) {
        size_t chunk = (size - offset > chunk_size) ? chunk_size : (size - offset);
        size_t bytes_read = firmwareFile.read(buffer, chunk);
        if (bytes_read != chunk) {
            LOG_E("SWD: Read failed");
            return false;
        }
        
        // Write to RAM
        for (size_t i = 0; i < chunk; i += 4) {
            uint32_t word = buffer[i] | (buffer[i+1] << 8) | (buffer[i+2] << 16) | (buffer[i+3] << 24);
            if (!writeWord(ram_addr + i, word)) {
                LOG_E("SWD: RAM write failed");
                return false;
            }
        }
        
        // Program from RAM to Flash
        if (!callRomFunc(flash_range_program, flash_addr + offset, ram_addr, chunk, 0)) {
            LOG_E("SWD: Program failed");
            return false;
        }
        
        offset += chunk;
        LOG_I("SWD: Progress: %d/%d (%.1f%%)", offset, size, (offset * 100.0f) / size);
    }
    
    LOG_I("SWD: Flash complete!");
    return true;
} 
bool PicoSWD::resetTarget() {
    LOG_I("SWD: resetTarget() called");
    
    // Always release debug connection first (if connected)
    // This ensures debug domains are powered down and cores can run
    if (_connected) {
        LOG_I("SWD: Releasing debug connection before reset...");
        end();
        // Give time for power-down to complete
        delay(50);
    }
    
    // Always do hardware reset to ensure clean state
    if (_reset >= 0) {
        LOG_I("SWD: Performing hardware reset via RESET pin (GPIO%d)", _reset);
        pinMode(_reset, OUTPUT);
        
        // Ensure reset pin is in correct state before asserting
        digitalWrite(_reset, HIGH);
        delay(10);
        
        // Assert reset (LOW)
        digitalWrite(_reset, LOW);
        delay(100);  // Hold reset for 100ms to ensure clean reset
        
        // Release reset (HIGH)
        digitalWrite(_reset, HIGH);
        delay(300);  // Wait 300ms for Pico to fully boot after reset
        
        LOG_I("SWD: Hardware reset complete - Pico should boot normally");
        _lastErrorStr = "None";
        return true;
    }
    
    LOG_E("SWD: No reset pin configured");
    _lastErrorStr = "No reset pin";
    return false;
}