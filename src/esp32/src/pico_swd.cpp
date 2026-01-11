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

// RP2350 AP Selection constants are now defined in pico_swd.h

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
// FIX: Correct RP2350 TARGETSEL ID for Core 0, Instance 0 (from OpenOCD rp2350.cfg)
// OpenOCD defines: set _CPUTAPID 0x00040927
// This is the correct Multidrop Target ID (Instance 0, Part Number 0x40, Designer 0x493/RaspberryPi)
#define ID_RP2350_TARGET 0x00040927  // Core 0, Instance 0 (CORRECT per OpenOCD)
#define ID_RP2350_RESCUE 0xF0000001 
#define ID_RP2350_EXPECTED 0x4c013477  // Expected RP2350 IDCODE (from reference) 

// Registers
#define DHCSR 0xE000EDF0
#define AIRCR 0xE000ED0C
#define DCRSR 0xE000EDF4  // Debug Core Register Selector Register
#define DCRDR 0xE000EDF8  // Debug Core Register Data Register

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
    // Just clock the turnaround cycle. 
    // Direction switching is handled by the caller.
    setSWCLK(LOW); delayMicroseconds(SWD_BIT_DELAY);
    setSWCLK(HIGH); delayMicroseconds(SWD_BIT_DELAY);
    setSWCLK(LOW);
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
    
    // -------------------------------------------------------------------------
    // Phase 0: Ensure we are in SWD mode first (JTAG -> SWD)
    // This handles cases where the chip powers up in JTAG or an undefined state.
    // The RP2350 might start in JTAG mode, and we need to switch to SWD before
    // we can send SWD-to-Dormant commands. Without this, if the chip is in JTAG
    // mode, the SWD-to-Dormant command (0xE3BC) will be interpreted as garbage.
    // -------------------------------------------------------------------------
    LOG_D("SWD: Phase 0 - JTAG-to-SWD (0xE79E)");
    swdResetSeq(); // Line Reset (>50 cycles HIGH)
    swdWrite(0x9E, 8); // LSB of 0xE79E (JTAG-to-SWD magic number)
    swdWrite(0xE7, 8); // MSB of 0xE79E
    swdResetSeq(); // Line Reset again to complete JTAG-to-SWD
    swdIdle();     // Idle cycles to stabilize
    
    // Phase 1: SWD-to-Dormant (0xE3BC, LSB first: 0xBC, 0xE3)
    // Now that we are definitely in SWD mode (Phase 0), send the command to enter Dormant
    // This command is only valid in SWD mode, so Phase 0 ensures we're in SWD first
    LOG_D("SWD: Phase 1 - SWD-to-Dormant (0xE3BC)");
    swdResetSeq();      // >50 cycles HIGH (sanitize interface state)
    swdWrite(0xBC, 8);  // LSB first
    swdWrite(0xE3, 8);  // MSB
    swdResetSeq();      // >50 cycles HIGH (complete Dormant entry)
    
    // Phase 2: Selection Alert (128 bits, LSB first, NO padding)
    // Pattern: 0x19bc0ea2_e3ddafe9_86852d95_6209f392
    // Bytes (LSB first): 0x92, 0xf3, 0x09, 0x62, 0x95, 0x2d, 0x85, 0x86,
    //                    0xe9, 0xaf, 0xdd, 0xe3, 0xa2, 0x0e, 0xbc, 0x19
    LOG_D("SWD: Phase 2 - Selection Alert (128 bits)");
    const uint8_t seq_alert[] = {
        0x92, 0xf3, 0x09, 0x62, 0x95, 0x2d, 0x85, 0x86, // Bytes 0-7
        0xe9, 0xaf, 0xdd, 0xe3, 0xa2, 0x0e, 0xbc, 0x19, // Bytes 8-15
    };
    for (size_t i = 0; i < sizeof(seq_alert); i++) {
        swdWrite(seq_alert[i], 8);
    }
    
    // Phase 3: Activation Code
    // 4 clock cycles with SWDIO LOW (delimiter), then 0x1A (SWD mode activation)
    // CRITICAL: After 0x1A, we need at least 8 cycles HIGH before the line reset
    // This is required per OpenOCD's swd_seq_dormant_to_swd implementation
    LOG_D("SWD: Phase 3 - Activation Code (4 cycles LOW + 0x1A + 8 cycles HIGH)");
    setSWDIO(LOW);
    for (int i = 0; i < 4; i++) {
        setSWCLK(LOW); delayMicroseconds(SWD_BIT_DELAY);
        setSWCLK(HIGH); delayMicroseconds(SWD_BIT_DELAY);
    }
    setSWCLK(LOW);
    swdWrite(0x1A, 8);  // 0x1A = 00011010 binary (LSB first: 0 1 0 1 1 0 0 0)
    
    // CRITICAL: At least 8 SWCLK cycles with SWDIO HIGH after activation code
    // This is required before the line reset per OpenOCD reference
    setSWDIO(HIGH);
    for (int i = 0; i < 8; i++) {
        setSWCLK(LOW); delayMicroseconds(SWD_BIT_DELAY);
        setSWCLK(HIGH); delayMicroseconds(SWD_BIT_DELAY);
    }
    setSWCLK(LOW);
    
    // Phase 4: Line Reset (>50 cycles HIGH) - enter Reset state
    LOG_D("SWD: Phase 4 - Line Reset (enter Reset state)");
    swdResetSeq();
    
    // --- CRITICAL: TRANSITION TO IDLE ---
    // We must pull SWDIO LOW to exit the RESET state.
    // Without this, the Target won't see the Start Bit of the next packet (TARGETSEL).
    // The Start Bit is a rising edge from LOW to HIGH. If the line is already HIGH
    // from the Reset Sequence, the RP2350 cannot detect the Start Bit and ignores the packet.
    LOG_D("SWD: Transitioning to IDLE state (SWDIO LOW)");
    swdIdle();
    
    LOG_I("SWD: Dormant Wakeup Sequence complete");
}

// --- Packet Layer (FIXED: EVEN PARITY) ---

swd_error_t PicoSWD::swdWritePacket(uint8_t request, uint32_t data, bool ignoreAck) {
    uint8_t ap_dp = (request & 0x01);
    uint8_t addr  = (request & 0x06) >> 1; 
    uint8_t header_payload = (addr << 3) | (0 << 2) | (ap_dp << 1) | 1;
    uint8_t parity = __builtin_parity(header_payload & 0x1E);
    uint8_t header = header_payload | (parity << 5) | 0x80;

    pinMode(g_pin_swdio, OUTPUT);
    swdWrite(header, 8);
    
    // TARGETSEL (0x0C) is special: No ACK, No Turnaround
    bool isTargetSel = (ap_dp == 0) && (addr == 0x03);
    
    if (!isTargetSel) {
        // --- TURNAROUND 1: Host -> Target ---
        pinMode(g_pin_swdio, INPUT_PULLUP);
        swdTurnaround();
        
        uint8_t ack = swdRead(3);
        
        // --- TURNAROUND 2: Target -> Host ---
        swdTurnaround();
        pinMode(g_pin_swdio, OUTPUT);

        if (ack != SWD_ACK_OK && !ignoreAck) {
            if (ack == SWD_ACK_FAULT) {
                // If FAULT, we need to abort. 
                // For simplicity here, just return error and let upper layer reset.
            }
            return (ack == SWD_ACK_WAIT) ? SWD_ERROR_WAIT : SWD_ERROR_PROTOCOL;
        }
    } else {
        // TARGETSEL: No delays, no turnaround
    }

    // Write Data
    swdWrite(data & 0xFF, 8);
    swdWrite((data >> 8) & 0xFF, 8);
    swdWrite((data >> 16) & 0xFF, 8);
    swdWrite((data >> 24) & 0xFF, 8);
    swdWrite(__builtin_parity(data), 1);
    
    return SWD_OK;
}

swd_error_t PicoSWD::swdReadPacket(uint8_t request, uint32_t *data) {
    uint8_t ap_dp = (request & 0x01);
    uint8_t addr  = (request & 0x06) >> 1; 
    uint8_t header_payload = (addr << 3) | (1 << 2) | (ap_dp << 1) | 1;
    uint8_t parity = __builtin_parity(header_payload & 0x1E);
    uint8_t header = header_payload | (parity << 5) | 0x80;

    pinMode(g_pin_swdio, OUTPUT);
    swdWrite(header, 8);
    
    // --- TURNAROUND 1: Host -> Target ---
    // CRITICAL: Switch to INPUT *before* clocking the turnaround
    pinMode(g_pin_swdio, INPUT_PULLUP); 
    swdTurnaround();
    
    // Read ACK (3 bits)
    uint8_t ack = swdRead(3);
    
    if (ack != SWD_ACK_OK) {
        // Even on error, we must complete the transaction cycle or reset
        // For now, just return the error, the retry logic will handle reset
        LOG_E("SWD: Read packet failed - ACK=%s (0x%X), req=0x%02X", 
                  ackToString(ack), ack, request);
        
        // Ensure we leave in a known state (OUTPUT) if we bail
        swdTurnaround(); 
        pinMode(g_pin_swdio, OUTPUT);
        
        return (ack == SWD_ACK_WAIT) ? SWD_ERROR_WAIT : 
               (ack == SWD_ACK_FAULT) ? SWD_ERROR_FAULT : SWD_ERROR_PROTOCOL;
    }
    
    // Read Data (32 bits) + Parity (1 bit)
    uint32_t val = swdRead(32);
    uint8_t p = swdRead(1); // Read parity bit (ignore result for now)
    
    // --- TURNAROUND 2: Target -> Host ---
    swdTurnaround();
    pinMode(g_pin_swdio, OUTPUT); // Host takes control back
    
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

swd_error_t PicoSWD::readAP(uint8_t addr, uint32_t *data, uint8_t ap_id) {
    LOG_D("SWD: ReadAP(0x%02X) on AP 0x%X - selecting bank", addr, ap_id);
    // Extract bank from address (bits 7:4)
    uint8_t bank = (addr >> 4) & 0xF;
    // Use the passed ap_id (defaults to AP_ARM_CORE0, but can be AP_RISCV for Debug Module init)
    uint32_t select_val = makeDPSelectRP2350(ap_id, bank, true);
    swd_error_t err = writeDP(DP_SELECT, select_val, false);
    if (err != SWD_OK) {
        LOG_E("SWD: ReadAP failed to select bank: %s", errorToString(err));
        return err;
    }
    // AP register address is bits 3:0
    uint8_t ap_reg = addr & 0xF;
    err = swdReadPacket((ap_reg << 1) | 0x09, data);  // APnDP=1, RnW=1
    if (err == SWD_OK && data) {
        LOG_D("SWD: ReadAP(0x%02X) on AP 0x%X = 0x%08X", addr, ap_id, *data);
    } else if (err != SWD_OK) {
        LOG_E("SWD: ReadAP(0x%02X) on AP 0x%X failed: %s", addr, ap_id, errorToString(err));
    }
    return err;
}

swd_error_t PicoSWD::writeAP(uint8_t addr, uint32_t data, uint8_t ap_id) {
    LOG_D("SWD: WriteAP(0x%02X, 0x%08X) on AP 0x%X - selecting bank", addr, data, ap_id);
    // Extract bank from address (bits 7:4)
    uint8_t bank = (addr >> 4) & 0xF;
    // Use the passed ap_id (defaults to AP_ARM_CORE0, but can be AP_RISCV for Debug Module init)
    uint32_t select_val = makeDPSelectRP2350(ap_id, bank, true);
    swd_error_t err = writeDP(DP_SELECT, select_val, false);
    if (err != SWD_OK) {
        LOG_E("SWD: WriteAP failed to select bank: %s", errorToString(err));
        return err;
    }
    // AP register address is bits 3:0
    uint8_t ap_reg = addr & 0xF;
    err = swdWritePacket((ap_reg << 1) | 0x01, data, false);  // APnDP=1, RnW=0
    if (err != SWD_OK) {
        LOG_E("SWD: WriteAP(0x%02X, 0x%08X) on AP 0x%X failed: %s", addr, data, ap_id, errorToString(err));
    }
    return err;
}

// --- Connection Logic ---

bool PicoSWD::connectToTarget() {
    pinMode(g_pin_swdio, OUTPUT); 
    pinMode(g_pin_swclk, OUTPUT);
    if (_reset >= 0) { pinMode(_reset, OUTPUT); digitalWrite(_reset, HIGH); }

    // List of Target IDs to try: Standard first, then Rescue
    // Rescue ID is needed if the chip is in a locked/dormant/bad state
    uint32_t targetIds[] = { ID_RP2350_TARGET, ID_RP2350_RESCUE };
    const char* targetNames[] = { "Standard", "Rescue" };
    
    for (int t = 0; t < 2; t++) {
        uint32_t currentTargetId = targetIds[t];
        LOG_I("SWD: Attempting connection using %s ID (0x%08X)...", targetNames[t], currentTargetId);

        // CRITICAL: RP2350 defaults to Dormant state upon Power-On Reset
        // We MUST ALWAYS perform the dormant wake-up sequence first
        sendDormantSequence();
        
        // TARGETSEL must be the FIRST packet after wake-up
        LOG_D("SWD: Sending TARGETSEL...");
        swd_error_t err = writeDP(DP_TARGETSEL, currentTargetId, true);
        
        // Allow time for selection to latch
        delay(100);
        
        // Soft reset to sync protocol
        swdLineResetSoft();
        swdIdle();
        delay(20);
        
        // Clear any sticky errors immediately before trying to read IDCODE
        // This is crucial if the previous attempt left the DAP in a FAULT state
        // ABORT register is at DP address 0x0 (write-only, same address as IDCODE read)
        writeDP(DP_IDCODE, 0x1E, true); // Write ABORT (0x1E clears all error flags)
        
        // Now verify connection by reading IDCODE
        uint32_t id = 0;
        LOG_D("SWD: Reading IDCODE...");
        
        // Retry logic for the read itself
        for (int retry = 0; retry < 3; retry++) {
            err = readDP(DP_IDCODE, &id);
            
            if (err == SWD_OK && id != 0 && id != 0xFFFFFFFF) {
                // We got a valid ID!
                if (id == ID_RP2350_EXPECTED) {
                    LOG_I("SWD: CONNECTED! IDCODE: 0x%08X (RP2350 verified)", id);
                    _connected = true;
                    _lastErrorStr = "None";
                    return true;
                } else {
                    LOG_W("SWD: Connected with unexpected IDCODE: 0x%08X (expected 0x%08X)", id, ID_RP2350_EXPECTED);
                    _connected = true;
                    _lastErrorStr = "None";
                    return true;
                }
            }
            
            if (err == SWD_ERROR_FAULT) {
                LOG_W("SWD: IDCODE FAULT (attempt %d/3). Clearing ABORT...", retry + 1);
                writeDP(DP_IDCODE, 0x1E, true); // Write ABORT to clear error flags
                delay(10);
            } else {
                delay(10);
            }
        }
        
        LOG_W("SWD: Failed to connect with %s ID.", targetNames[t]);
    }

    LOG_E("SWD: Connection failed after trying all Target IDs.");
    _lastErrorStr = "Connection failed - no valid IDCODE";
    return false;
}

// --- Diagnostic Functions ---

uint8_t PicoSWD::readAckDiagnostic() {
    // Switch to input before turnaround
    pinMode(g_pin_swdio, INPUT_PULLUP);
    swdTurnaround();
    
    // Read ACK bit-by-bit with logging
    uint8_t ack = 0;
    Serial.print("[DIAG] ACK bits: ");
    for (int i = 0; i < 3; i++) {
        setSWCLK(LOW); 
        delayMicroseconds(SWD_BIT_DELAY);
        bool bit = readSWDIO();
        Serial.print(bit ? "1" : "0");
        if (bit) ack |= (1 << i);
        setSWCLK(HIGH); 
        delayMicroseconds(SWD_BIT_DELAY);
        setSWCLK(LOW);
    }
    Serial.printf(" (0x%X = %s)\n", ack, ackToString(ack));
    
    // Check if line is floating (always HIGH due to pull-up)
    // If device is driving, we should see at least one LOW bit for valid ACK
    if (ack == 0x7) {
        Serial.println("[DIAG] WARNING: ACK=0x7 (NO_RESPONSE) - SWDIO line appears to be floating HIGH");
        Serial.println("[DIAG] This suggests:");
        Serial.println("[DIAG]   1. Device is not powered");
        Serial.println("[DIAG]   2. Device is not responding to SWD");
        Serial.println("[DIAG]   3. SWDIO pin is not connected");
        Serial.println("[DIAG]   4. Device is in a state where it won't respond");
    }
    
    return ack;
}

bool PicoSWD::diagnoseDevice() {
    LOG_I("SWD: Starting device diagnostics...");
    
    // 1. Check pin states
    Serial.println("[DIAG] === Pin State Check ===");
    Serial.printf("[DIAG] SWDIO pin (GPIO%d): ", g_pin_swdio);
    pinMode(g_pin_swdio, INPUT_PULLUP);
    bool swdio_state = digitalRead(g_pin_swdio);
    Serial.println(swdio_state ? "HIGH (pulled up)" : "LOW");
    
    Serial.printf("[DIAG] SWCLK pin (GPIO%d): ", g_pin_swclk);
    pinMode(g_pin_swclk, INPUT_PULLUP);
    bool swclk_state = digitalRead(g_pin_swclk);
    Serial.println(swclk_state ? "HIGH (pulled up)" : "LOW");
    
    if (_reset >= 0) {
        Serial.printf("[DIAG] RESET pin (GPIO%d): ", _reset);
        pinMode(_reset, INPUT_PULLUP);
        bool reset_state = digitalRead(_reset);
        Serial.println(reset_state ? "HIGH" : "LOW");
    }
    
    // 2. Try simple line reset + IDCODE read (without dormant sequence)
    Serial.println("[DIAG] === Simple Connection Test (no dormant sequence) ===");
    pinMode(g_pin_swdio, OUTPUT);
    pinMode(g_pin_swclk, OUTPUT);
    
    // Simple line reset
    Serial.println("[DIAG] Sending line reset...");
    swdResetSeq();
    swdIdle();
    
    // Try to read IDCODE directly
    Serial.println("[DIAG] Attempting to read IDCODE (simple test)...");
    pinMode(g_pin_swdio, OUTPUT);
    
    // Send IDCODE read request (DP 0x00, Read)
    uint8_t header = 0x85; // Read DP 0x00: START(1) | APnDP(0) | RnW(1) | A[3:2](00) | PARITY(0) | STOP(0) | PARK(1)
    swdWrite(header, 8);
    
    // Read ACK with diagnostics
    uint8_t ack = readAckDiagnostic();
    
    if (ack == SWD_ACK_OK) {
        Serial.println("[DIAG] ✓ Got OK ACK! Device is responding.");
        // Try to read the IDCODE
        uint32_t idcode = 0;
        for (int i = 0; i < 32; i++) {
            setSWCLK(LOW); delayMicroseconds(SWD_BIT_DELAY);
            bool bit = readSWDIO();
            if (bit) idcode |= (1 << i);
            setSWCLK(HIGH); delayMicroseconds(SWD_BIT_DELAY);
            setSWCLK(LOW);
        }
        // Read parity
        setSWCLK(LOW); delayMicroseconds(SWD_BIT_DELAY);
        bool parity = readSWDIO();
        setSWCLK(HIGH); delayMicroseconds(SWD_BIT_DELAY);
        setSWCLK(LOW);
        
        swdTurnaround();
        pinMode(g_pin_swdio, OUTPUT);
        
        Serial.printf("[DIAG] IDCODE: 0x%08X (parity: %d)\n", idcode, parity);
        if (idcode == ID_RP2350_EXPECTED || idcode != 0xFFFFFFFF) {
            Serial.println("[DIAG] ✓ Device is responding without dormant sequence!");
            return true;
        }
    } else {
        Serial.printf("[DIAG] ✗ No response: ACK=0x%X (%s)\n", ack, ackToString(ack));
        swdTurnaround();
        pinMode(g_pin_swdio, OUTPUT);
    }
    
    // 3. Test with dormant sequence
    Serial.println("[DIAG] === Dormant Sequence Test ===");
    sendDormantSequence();
    
    // Try TARGETSEL + IDCODE
    Serial.println("[DIAG] Sending TARGETSEL...");
    writeDP(DP_TARGETSEL, ID_RP2350_TARGET, true);
    delay(100);
    
    swdLineResetSoft();
    swdIdle();
    delay(20);
    
    // Try IDCODE read
    Serial.println("[DIAG] Attempting IDCODE read after TARGETSEL...");
    pinMode(g_pin_swdio, OUTPUT);
    header = 0x85; // Read DP 0x00
    swdWrite(header, 8);
    
    ack = readAckDiagnostic();
    
    if (ack == SWD_ACK_OK) {
        Serial.println("[DIAG] ✓ Got OK ACK after dormant sequence!");
        return true;
    } else {
        Serial.printf("[DIAG] ✗ Still no response after dormant: ACK=0x%X\n", ack);
        swdTurnaround();
        pinMode(g_pin_swdio, OUTPUT);
    }
    
    // 4. Check if device is driving the line at all
    Serial.println("[DIAG] === Line Drive Test ===");
    Serial.println("[DIAG] Setting SWDIO to INPUT (no pull-up) and checking if device drives it...");
    pinMode(g_pin_swdio, INPUT); // No pull-up
    delay(10);
    bool driven_state = digitalRead(g_pin_swdio);
    Serial.printf("[DIAG] SWDIO state (no pull-up): %s\n", driven_state ? "HIGH (floating or driven HIGH)" : "LOW (driven LOW)");
    
    if (driven_state) {
        Serial.println("[DIAG] WARNING: SWDIO is HIGH even without pull-up.");
        Serial.println("[DIAG] This could mean:");
        Serial.println("[DIAG]   - Device is driving it HIGH");
        Serial.println("[DIAG]   - Line is floating and picking up noise");
        Serial.println("[DIAG]   - Hardware issue");
        
        // Test if device is actually driving by trying to pull it LOW
        Serial.println("[DIAG] Testing if device resists pull-down...");
        pinMode(g_pin_swdio, OUTPUT);
        digitalWrite(g_pin_swdio, LOW);
        delay(5);
        pinMode(g_pin_swdio, INPUT); // Back to input
        delay(5);
        bool after_low = digitalRead(g_pin_swdio);
        Serial.printf("[DIAG] After forcing LOW, SWDIO state: %s\n", after_low ? "HIGH (device driving it back HIGH!)" : "LOW (stayed LOW - floating)");
        
        if (after_low) {
            Serial.println("[DIAG] ✓ Device IS driving SWDIO HIGH - device is powered and active!");
            Serial.println("[DIAG] But device is not responding to SWD commands.");
            Serial.println("[DIAG] Possible reasons:");
            Serial.println("[DIAG]   1. SWD is disabled in boot mode");
            Serial.println("[DIAG]   2. SWD pins are configured for other functions");
            Serial.println("[DIAG]   3. Device needs different boot sequence");
            Serial.println("[DIAG]   4. Timing issue - device needs more time");
        } else {
            Serial.println("[DIAG] ✗ SWDIO stayed LOW - line is floating (not driven by device)");
            Serial.println("[DIAG] This suggests device is NOT powered or NOT connected");
        }
    }
    
    // 5. Test SWCLK to see if device responds to clock
    Serial.println("[DIAG] === Clock Response Test ===");
    Serial.println("[DIAG] Testing if device responds to SWCLK pulses...");
    pinMode(g_pin_swdio, INPUT_PULLUP);
    pinMode(g_pin_swclk, OUTPUT);
    
    // Send a few clock pulses and see if SWDIO changes
    bool initial_swdio = digitalRead(g_pin_swdio);
    Serial.printf("[DIAG] Initial SWDIO state: %s\n", initial_swdio ? "HIGH" : "LOW");
    
    for (int i = 0; i < 10; i++) {
        digitalWrite(g_pin_swclk, LOW);
        delayMicroseconds(10);
        bool swdio_low = digitalRead(g_pin_swdio);
        digitalWrite(g_pin_swclk, HIGH);
        delayMicroseconds(10);
        bool swdio_high = digitalRead(g_pin_swdio);
        
        if (swdio_low != swdio_high || swdio_low != initial_swdio) {
            Serial.printf("[DIAG] ✓ SWDIO changed during clock pulse %d! (LOW=%d, HIGH=%d)\n", i+1, swdio_low, swdio_high);
            Serial.println("[DIAG] Device may be responding to clock!");
            break;
        }
    }
    
    // Restore pull-up
    pinMode(g_pin_swdio, INPUT_PULLUP);
    
    // 6. Check if device might be in BOOTSEL mode or check chip ID
    Serial.println("[DIAG] === Boot Mode Check ===");
    Serial.println("[DIAG] Attempting to read chip ID register (0x40000000)...");
    
    // Try to read chip ID via SWD (if we can get any response)
    // This requires SWD to be working, but let's try anyway
    pinMode(g_pin_swdio, OUTPUT);
    pinMode(g_pin_swclk, OUTPUT);
    
    // Send dormant sequence one more time
    sendDormantSequence();
    writeDP(DP_TARGETSEL, ID_RP2350_TARGET, true);
    delay(100);
    swdLineResetSoft();
    swdIdle();
    delay(20);
    writeDP(DP_IDCODE, 0x1E, true); // Clear ABORT
    
    // Try to read IDCODE with detailed logging
    Serial.println("[DIAG] Attempting IDCODE read with detailed bus monitoring...");
    pinMode(g_pin_swdio, OUTPUT);
    uint8_t header2 = 0x85; // Read DP 0x00
    swdWrite(header2, 8);
    
    // Monitor the bus during ACK read
    pinMode(g_pin_swdio, INPUT_PULLUP);
    swdTurnaround();
    
    Serial.print("[DIAG] ACK bits (detailed): ");
    uint8_t ack2 = 0;
    for (int i = 0; i < 3; i++) {
        setSWCLK(LOW);
        delayMicroseconds(SWD_BIT_DELAY);
        bool bit_before = digitalRead(g_pin_swdio);
        setSWCLK(HIGH);
        delayMicroseconds(SWD_BIT_DELAY);
        bool bit_after = digitalRead(g_pin_swdio);
        bool bit = bit_after;
        Serial.printf("bit%d=%d(before:%d,after:%d) ", i, bit, bit_before, bit_after);
        if (bit) ack2 |= (1 << i);
        setSWCLK(LOW);
    }
    Serial.printf("-> ACK=0x%X\n", ack2);
    
    if (ack2 == 0x7) {
        Serial.println("[DIAG] CRITICAL: Device is driving SWDIO HIGH but not responding to SWD.");
        Serial.println("[DIAG] This suggests:");
        Serial.println("[DIAG]   1. SWD pins are configured for other functions (GPIO, UART, etc.)");
        Serial.println("[DIAG]   2. Device is in BOOTSEL mode (SWD disabled)");
        Serial.println("[DIAG]   3. Device boot mode disables SWD");
        Serial.println("[DIAG]   4. Hardware: Wrong pins connected or pin conflict");
        Serial.println("[DIAG]");
        Serial.println("[DIAG] RECOMMENDATION: Check hardware connections and verify:");
        Serial.println("[DIAG]   - SWDIO (GPIO21) is connected to RP2350 SWDIO");
        Serial.println("[DIAG]   - SWCLK (GPIO45) is connected to RP2350 SWCLK");
        Serial.println("[DIAG]   - No other devices driving these pins");
        Serial.println("[DIAG]   - Device is not in BOOTSEL mode (check QSPI_SS pin)");
    }
    
    swdTurnaround();
    pinMode(g_pin_swdio, OUTPUT);
    
    // 7. Final Summary and Recommendations
    Serial.println("[DIAG] === Summary and Recommendations ===");
    Serial.println("[DIAG] Device Status:");
    Serial.println("[DIAG]   - Powered: YES (driving SWDIO HIGH)");
    Serial.println("[DIAG]   - SWD Response: NO (ACK=0x7 always)");
    Serial.println("[DIAG]   - UART Communication: UNKNOWN (check separately)");
    Serial.println("[DIAG]");
    Serial.println("[DIAG] CRITICAL: Device appears to be in a state where SWD is disabled.");
    Serial.println("[DIAG] This is NOT a software recovery issue - requires hardware/firmware check.");
    Serial.println("[DIAG]");
    Serial.println("[DIAG] ACTION REQUIRED:");
    Serial.println("[DIAG] 1. Verify Pico firmware: Check if SWD pins are configured as GPIO");
    Serial.println("[DIAG]    - SWDIO/SWCLK should NOT be configured as GPIO in Pico firmware");
    Serial.println("[DIAG]    - Check Pico firmware initialization code");
    Serial.println("[DIAG]");
    Serial.println("[DIAG] 2. Check BOOTSEL mode: Verify QSPI_SS pin state");
    Serial.println("[DIAG]    - If QSPI_SS is LOW during boot, device enters BOOTSEL (SWD disabled)");
    Serial.println("[DIAG]    - Check if BOOTSEL button is stuck or QSPI_SS is shorted to GND");
    Serial.println("[DIAG]");
    Serial.println("[DIAG] 3. Verify hardware connections:");
    Serial.println("[DIAG]    - ESP32 GPIO21 -> RP2350 SWDIO (via 47Ω resistor)");
    Serial.println("[DIAG]    - ESP32 GPIO45 -> RP2350 SWCLK (via 22Ω resistor)");
    Serial.println("[DIAG]    - ESP32 GPIO4 -> RP2350 RUN pin");
    Serial.println("[DIAG]    - Check for shorts, wrong pins, or loose connections");
    Serial.println("[DIAG]");
    Serial.println("[DIAG] 4. Check if device is actually running:");
    Serial.println("[DIAG]    - If device has firmware, it should send UART messages");
    Serial.println("[DIAG]    - If no UART activity, device may not have firmware or is stuck");
    Serial.println("[DIAG]    - Try flashing Pico firmware via USB (if available)");
    Serial.println("[DIAG]");
    Serial.println("[DIAG] 5. Last resort: Use external SWD debugger");
    Serial.println("[DIAG]    - Connect external SWD debugger (e.g., J-Link, ST-Link)");
    Serial.println("[DIAG]    - This will confirm if SWD hardware is functional");
    Serial.println("[DIAG]    - If external debugger works, issue is with ESP32 SWD implementation");
    Serial.println("[DIAG]    - If external debugger fails, issue is with Pico hardware/firmware");
    
    Serial.println("[DIAG] === Diagnostic Complete ===");
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
    
    // FIX: Release pins to INPUT (High-Z) to prevent parasitic powering
    // When Pico is disconnected, driving SWCLK HIGH causes back-feeding through protection diodes.
    // We only drive SWCLK HIGH during active boot/reset sequences, not when idle.
    // Weak pull-ups are okay - they don't provide enough current to latch up disconnected Pico.
    pinMode(g_pin_swdio, INPUT_PULLUP);
    pinMode(g_pin_swclk, INPUT_PULLUP);
    
    LOG_D("SWD: Pins released (INPUT_PULLUP) - prevents parasitic powering when Pico offline");
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
    err = writeAP(AP_CSW, csw, AP_RISCV);
    if (err != SWD_OK) {
        LOG_E("SWD: Failed to configure CSW: %s", errorToString(err));
        _lastErrorStr = errorToString(err);
        return false;
    }
    
    // Step 3: Point TAR at DMCONTROL
    LOG_D("SWD: Setting TAR to DM_DMCONTROL (0x%08X)", DM_DMCONTROL);
    err = writeAP(AP_TAR, DM_DMCONTROL, AP_RISCV);
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
    err = writeAP(AP_CSW, 0x00000000, AP_RISCV);
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
    err = writeAP(AP_CSW, 0x00000001, AP_RISCV);
    if (err != SWD_OK) {
        LOG_E("SWD: DM activation activate failed: %s", errorToString(err));
        _lastErrorStr = errorToString(err);
        return false;
    }
    readDP(DP_RDBUFF, &dummy);
    delay(50);
    
    // Phase 3: Configure
    LOG_D("SWD: DM activation - Configure phase");
    err = writeAP(AP_CSW, 0x07FFFFC1, AP_RISCV);
    if (err != SWD_OK) {
        LOG_E("SWD: DM activation configure failed: %s", errorToString(err));
        _lastErrorStr = errorToString(err);
        return false;
    }
    readDP(DP_RDBUFF, &dummy);
    delay(50);
    
    // Step 6: Verify DM is responding
    LOG_D("SWD: Verifying DM status...");
    err = readAP(AP_CSW, &dummy, AP_RISCV);  // Trigger read
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
    err = writeAP(AP_CSW, 0xA2000002, AP_RISCV);
    if (err != SWD_OK) {
        LOG_W("SWD: Failed to configure CSW: %s (continuing anyway)", errorToString(err));
    }
    
    // Set TAR to point at DMCONTROL (0x10 * 4 = 0x40)
    err = writeAP(AP_TAR, DM_DMCONTROL * 4, AP_RISCV);
    if (err != SWD_OK) {
        LOG_W("SWD: Failed to set TAR: %s (continuing anyway)", errorToString(err));
    }
    
    // Reset DM: dmactive=0
    err = writeAP(AP_DRW, 0x00000000, AP_RISCV);
    if (err != SWD_OK) {
        LOG_W("SWD: Failed to reset DM: %s (continuing anyway)", errorToString(err));
    }
    delay(10);
    
    // Activate DM: dmactive=1
    err = writeAP(AP_DRW, 0x00000001, AP_RISCV);
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
// FIX: writeWord/readWord must use TAR/DRW sequence with ARM AP (0x2)
// TAR (0x04) sets the address, DRW (0x0C) performs the read/write
bool PicoSWD::writeWord(uint32_t addr, uint32_t data) {
    // Set transfer address via TAR
    if (writeAP(AP_TAR, addr) != SWD_OK) return false;
    // Write data via DRW
    return writeAP(AP_DRW, data) == SWD_OK;
}
uint32_t PicoSWD::readWord(uint32_t addr) {
    // Set transfer address via TAR
    if (writeAP(AP_TAR, addr) != SWD_OK) return 0;
    // Read data via DRW
    uint32_t d = 0;
    if (readAP(AP_DRW, &d) != SWD_OK) return 0;
    return d;
}
bool PicoSWD::haltCore() { return writeWord(DHCSR, 0xA05F0003); }
bool PicoSWD::runCore() { return writeWord(DHCSR, 0xA05F0001); }
// Core Register Access (ARM Cortex-M via DCRSR/DCRDR)
// FIX: Use ARM DCRSR/DCRDR registers instead of RISC-V abstract commands
bool PicoSWD::writeCoreReg(uint8_t reg, uint32_t val) {
    // ARM Cortex-M: R0-R3 are 0-3, PC is 15
    if (reg > 15) {
        LOG_E("SWD: Invalid register %d (must be 0-15 for ARM)", reg);
        return false;
    }
    
    // 1. Write value to DCRDR (Debug Core Register Data Register)
    if (!writeWord(DCRDR, val)) {
        LOG_E("SWD: Failed to write value to DCRDR");
        return false;
    }
    
    // 2. Write 'write' command to DCRSR (Bit 16 = Write, Bits 4:0 = Reg Index)
    // DCRSR format: [16] = WnR (1=write), [4:0] = RegSel
    uint32_t dcsr_cmd = (1 << 16) | (reg & 0x1F);
    if (!writeWord(DCRSR, dcsr_cmd)) {
        LOG_E("SWD: Failed to write DCRSR command");
        return false;
    }
    
    // 3. Wait for S_REGRDY in DHCSR (bit 16) - register transfer complete
    for (int i = 0; i < 100; i++) {
        uint32_t dhcsr = readWord(DHCSR);
        if (dhcsr & (1 << 16)) {  // S_REGRDY bit
            return true;
        }
        delay(1);
    }
    
    LOG_E("SWD: writeCoreReg timeout - S_REGRDY not set");
    return false;
}

uint32_t PicoSWD::readCoreReg(uint8_t reg) {
    // ARM Cortex-M: R0-R3 are 0-3, PC is 15
    if (reg > 15) {
        LOG_E("SWD: Invalid register %d (must be 0-15 for ARM)", reg);
        return 0;
    }
    
    // 1. Write 'read' command to DCRSR (Bit 16 = 0 for read, Bits 4:0 = Reg Index)
    // DCRSR format: [16] = WnR (0=read), [4:0] = RegSel
    uint32_t dcsr_cmd = (0 << 16) | (reg & 0x1F);
    if (!writeWord(DCRSR, dcsr_cmd)) {
        LOG_E("SWD: Failed to write DCRSR read command");
        return 0;
    }
    
    // 2. Wait for S_REGRDY in DHCSR (bit 16) - register transfer complete
    for (int i = 0; i < 100; i++) {
        uint32_t dhcsr = readWord(DHCSR);
        if (dhcsr & (1 << 16)) {  // S_REGRDY bit
            // 3. Read value from DCRDR
            uint32_t data = readWord(DCRDR);
            return data;
        }
        delay(1);
    }
    
    LOG_E("SWD: readCoreReg timeout - S_REGRDY not set");
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
    
    // FIX: Set PC using ARM register access (PC is register 15)
    if (!writeCoreReg(15, funcAddr)) return false;
    
    // Set LR (Link Register, R14) to trap address
    if (!writeCoreReg(14, 0x20000000)) return false;
    
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
bool PicoSWD::resumeFromHalt() {
    if (!_connected) {
        LOG_E("SWD: resumeFromHalt() failed - not connected");
        _lastErrorStr = "Not connected";
        return false;
    }
    
    LOG_I("SWD: Resuming core from debug halt...");
    
    // Power up debug domain first (required for core access)
    if (!powerUpDebug()) {
        LOG_E("SWD: Failed to power up debug domain");
        return false;
    }
    
    // Initialize Debug Module (required for core control)
    if (!initDebugModule()) {
        LOG_E("SWD: Failed to initialize debug module");
        return false;
    }
    
    // Resume the core (sends run command via DHCSR)
    if (runCore()) {
        LOG_I("SWD: ✓ Core resumed from debug halt!");
        _lastErrorStr = "None";
        return true;
    } else {
        LOG_E("SWD: Failed to resume core");
        _lastErrorStr = "runCore() failed";
        return false;
    }
}

bool PicoSWD::resetTarget() {
    LOG_I("SWD: resetTarget() called");
    
    // Option 1: Hardware Reset (Preferred if pin available)
    if (_reset >= 0) {
        // Always release debug connection first (if connected)
        // This ensures debug domains are powered down and cores can run
        if (_connected) {
            LOG_I("SWD: Releasing debug connection before reset...");
            end();
            // Give time for power-down to complete
            delay(50);
        }
        
        LOG_I("SWD: Performing hardware reset via RESET pin (GPIO%d)", _reset);
        
        // ---------------------------------------------------------------------
        // STEP 1: NEUTRALIZE ALL PINS (SWD + UART) - RP2350 Latch-up Prevention
        // ---------------------------------------------------------------------
        // RP2350 A0 Errata: Driving ANY GPIO high while RUN is LOW causes latch-up.
        // We must float ALL pins connected to Pico (SWD + UART) before asserting reset.
        
        LOG_D("SWD: Floating SWD pins before reset (prevent GPIO back-power lock-up)...");
        gpio_set_direction((gpio_num_t)g_pin_swdio, GPIO_MODE_INPUT);
        gpio_set_direction((gpio_num_t)g_pin_swclk, GPIO_MODE_INPUT);
        gpio_set_pull_mode((gpio_num_t)g_pin_swdio, GPIO_FLOATING);
        gpio_set_pull_mode((gpio_num_t)g_pin_swclk, GPIO_FLOATING);
        
        // CRITICAL: Float UART pins too (UART TX stays HIGH in idle, causing latch-up)
        LOG_D("SWD: Floating UART pins before reset (prevent latch-up via UART TX)...");
        #if defined(PICO_UART_TX_PIN) && defined(PICO_UART_RX_PIN)
            gpio_set_direction((gpio_num_t)PICO_UART_TX_PIN, GPIO_MODE_INPUT);
            gpio_set_direction((gpio_num_t)PICO_UART_RX_PIN, GPIO_MODE_INPUT);
            gpio_set_pull_mode((gpio_num_t)PICO_UART_TX_PIN, GPIO_FLOATING);
            gpio_set_pull_mode((gpio_num_t)PICO_UART_RX_PIN, GPIO_FLOATING);
        #endif
        
        delay(10); // Wait for parasitic capacitance to discharge
        
        // ---------------------------------------------------------------------
        // STEP 2: ASSERT RESET
        // ---------------------------------------------------------------------
        pinMode(_reset, OUTPUT);
        digitalWrite(_reset, LOW);  // Assert reset (LOW)
        delay(100);  // Hold reset for 100ms to ensure clean reset
        
        // ---------------------------------------------------------------------
        // STEP 3: RELEASE RESET (Open-Drain Method)
        // ---------------------------------------------------------------------
        // Use Open-Drain simulation: Set to INPUT instead of driving HIGH
        // This prevents parasitic powering if RP2350 is unpowered
        // The RP2350 has an internal pull-up that will pull RUN high when ready
        pinMode(_reset, INPUT);  // Release reset (open-drain - let internal pull-up do the work)
        delay(500);  // Wait longer (500ms) for Pico to fully boot and initialize flash
        
        LOG_I("SWD: Hardware reset complete - Pico should boot normally");
        _lastErrorStr = "None";
        return true;
    }
    
    // Option 2: Software Reset via SWD (AIRCR)
    // Useful when RESET pin is not connected or we want to reset via protocol
    LOG_I("SWD: No hardware reset pin - attempting SWD AIRCR Soft Reset...");
    
    // We need to be connected to send the command
    bool wasConnected = _connected;
    if (!wasConnected) {
        LOG_I("SWD: Connecting to target for soft reset...");
        if (!begin()) {
            LOG_E("SWD: Failed to connect for soft reset: %s", _lastErrorStr);
            return false;
        }
    }
    
    // 1. Halt Core (Optional but recommended)
    haltCore();
    
    // 2. Write SYSRESETREQ to AIRCR
    // Address: 0xE000ED0C (AIRCR)
    // Value: 0x05FA0004 (VECTKEY=0x05FA | SYSRESETREQ=1)
    LOG_I("SWD: Writing SYSRESETREQ to AIRCR...");
    bool success = writeWord(0xE000ED0C, 0x05FA0004);
    
    if (success) {
        LOG_I("SWD: Soft reset command sent successfully");
        _lastErrorStr = "None";
    } else {
        LOG_E("SWD: Failed to write soft reset command");
        _lastErrorStr = "Soft reset failed";
    }
    
    // 3. Disconnect immediately to let it boot
    end();
    
    // 4. Wait for boot
    delay(300);
    
    return success;
}