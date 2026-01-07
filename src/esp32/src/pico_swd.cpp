#include "pico_swd.h"
#include "config.h"
#include <Arduino.h>
#include <FS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// SWD Protocol Constants
#define SWD_DP_IDCODE       0x00
#define SWD_DP_ABORT        0x00
#define SWD_DP_CTRL_STAT    0x04
#define SWD_DP_SELECT       0x08
#define SWD_DP_TARGETSEL    0x0C // Multidrop select

#define SWD_AP_CSW          0x00
#define SWD_AP_TAR          0x04
#define SWD_AP_DRW          0x0C
#define SWD_AP_IDR          0xFC

// Core Registers
#define DHCSR               0xE000EDF0
#define DCRSR               0xE000EDF4
#define DCRDR               0xE000EDF8
#define AIRCR               0xE000ED0C

PicoSWD::PicoSWD(int swdio, int swclk, int reset)
    : _swdio(swdio), _swclk(swclk), _reset(reset), _connected(false) {
}

bool PicoSWD::begin() {
    LOG_I("Initializing SWD interface: SWDIO=GPIO%d, SWCLK=GPIO%d", _swdio, _swclk);
    
    pinMode(_swdio, OUTPUT);
    pinMode(_swclk, OUTPUT);
    digitalWrite(_swdio, HIGH);
    digitalWrite(_swclk, HIGH);

    swdLineReset();
    
    if (!connectToTarget()) {
        LOG_E("SWD: Failed to select RP2040 Target");
        return false;
    }
    
    if (!initAP()) {
        LOG_E("SWD: Failed to init MEM-AP");
        return false;
    }

    _connected = true;
    LOG_I("SWD: Connected to RP2040 Core 0");
    return true;
}

void PicoSWD::end() {
    _connected = false;
    pinMode(_swdio, INPUT);
    pinMode(_swclk, INPUT);
}

// --- Low Level SWD ---

void PicoSWD::setSWDIO(bool high) { 
    digitalWrite(_swdio, high ? HIGH : LOW); 
}

void PicoSWD::setSWCLK(bool high) { 
    digitalWrite(_swclk, high ? HIGH : LOW); 
}

bool PicoSWD::readSWDIO() { 
    return digitalRead(_swdio) == HIGH; 
}

void PicoSWD::swdWrite(uint8_t data, uint8_t bits) {
    for (int i = 0; i < bits; i++) {
        setSWDIO(data & 1);
        delayMicroseconds(SWD_DELAY);
        setSWCLK(LOW);
        delayMicroseconds(SWD_DELAY);
        setSWCLK(HIGH);
        delayMicroseconds(SWD_DELAY);
        data >>= 1;
    }
}

uint8_t PicoSWD::swdRead(uint8_t bits) {
    uint8_t res = 0;
    for (int i = 0; i < bits; i++) {
        setSWCLK(LOW);
        delayMicroseconds(SWD_DELAY);
        if (readSWDIO()) res |= (1 << i);
        setSWCLK(HIGH);
        delayMicroseconds(SWD_DELAY);
    }
    return res;
}

void PicoSWD::swdTurnaround() {
    pinMode(_swdio, INPUT);
    setSWCLK(LOW); 
    delayMicroseconds(SWD_DELAY);
    setSWCLK(HIGH); 
    delayMicroseconds(SWD_DELAY);
    pinMode(_swdio, OUTPUT);
}

void PicoSWD::swdIdle() {
    setSWDIO(LOW);
    for (int i = 0; i < 4; i++) {
        setSWCLK(LOW); 
        delayMicroseconds(SWD_DELAY);
        setSWCLK(HIGH); 
        delayMicroseconds(SWD_DELAY);
    }
}

void PicoSWD::swdLineReset() {
    setSWDIO(HIGH);
    for (int i = 0; i < 60; i++) {
        setSWCLK(LOW); 
        delayMicroseconds(SWD_DELAY);
        setSWCLK(HIGH); 
        delayMicroseconds(SWD_DELAY);
    }
    setSWDIO(LOW); // Idle state
    swdIdle();
}

// --- Packet Layer ---

uint32_t PicoSWD::swdReadPacket(uint8_t request) {
    uint8_t parity = __builtin_parity(request & 0x0F);
    uint8_t header = 0x81 | ((request & 0x0C) << 1) | ((request & 0x02) << 1) | (parity << 5);
    swdWrite(header, 8);
    
    pinMode(_swdio, INPUT); // Turnaround
    setSWCLK(LOW); 
    delayMicroseconds(SWD_DELAY);
    setSWCLK(HIGH); 
    delayMicroseconds(SWD_DELAY);
    
    uint8_t ack = swdRead(3);
    if (ack != 1) { // ACK OK
        swdTurnaround(); // Restore dir
        return 0xFFFFFFFF; // Error
    }
    
    uint32_t data = 0;
    for (int i = 0; i < 32; i++) {
        setSWCLK(LOW); 
        delayMicroseconds(SWD_DELAY);
        if (readSWDIO()) data |= (1UL << i);
        setSWCLK(HIGH); 
        delayMicroseconds(SWD_DELAY);
    }
    uint8_t p = swdRead(1); // Parity
    
    swdTurnaround(); // Turnaround back to output
    return data;
}

bool PicoSWD::swdWritePacket(uint8_t request, uint32_t data, bool ignoreAck) {
    uint8_t parity = __builtin_parity(request & 0x0F);
    uint8_t header = 0x81 | ((request & 0x0C) << 1) | ((request & 0x02) << 1) | (parity << 5);
    swdWrite(header, 8);
    
    pinMode(_swdio, INPUT); // Turnaround
    setSWCLK(LOW); 
    delayMicroseconds(SWD_DELAY);
    setSWCLK(HIGH); 
    delayMicroseconds(SWD_DELAY);
    
    uint8_t ack = swdRead(3);
    swdTurnaround(); // Back to output
    
    if (!ignoreAck && ack != 1) return false;
    
    // Write Data (LSB first)
    swdWrite(data & 0xFF, 8);
    swdWrite((data >> 8) & 0xFF, 8);
    swdWrite((data >> 16) & 0xFF, 8);
    swdWrite((data >> 24) & 0xFF, 8);
    
    swdWrite(__builtin_parity(data), 1); // Parity
    return true;
}

// --- RP2040 Specific ---

bool PicoSWD::connectToTarget() {
    // 1. Line Reset
    swdLineReset();
    
    // 2. TARGETSEL (Critical for RP2040 Multidrop)
    // Write to TARGETSEL (0x0C) requires special handling (ignore ACK)
    uint8_t req = 0x00 | 0x00 | 0x0C; // DP Write TARGETSEL
    if (!swdWritePacket(req, RP2040_CORE0_ID, true)) {
        // RP2040 might not ACK this, which is allowed in spec
    }
    
    // 3. Read IDCODE to confirm selection
    uint32_t id = readDP(SWD_DP_IDCODE);
    LOG_I("SWD IDCODE: 0x%08X", id);
    return (id == 0x0BC12477); // RP2040 Core 0 ID
}

bool PicoSWD::initAP() {
    writeDP(SWD_DP_ABORT, 0x1E); // Clear errors
    writeDP(SWD_DP_SELECT, 0x00); // Select AP 0
    writeDP(SWD_DP_CTRL_STAT, 0x50000000); // Power up
    
    // Check AP
    uint32_t idr = readAP(SWD_AP_IDR);
    return (idr != 0 && idr != 0xFFFFFFFF);
}

// --- Memory & Core Access ---

bool PicoSWD::writeWord(uint32_t addr, uint32_t data) {
    writeAP(SWD_AP_CSW, 0x23000052); // 32-bit inc
    writeAP(SWD_AP_TAR, addr);
    return writeAP(SWD_AP_DRW, data);
}

uint32_t PicoSWD::readWord(uint32_t addr) {
    writeAP(SWD_AP_CSW, 0x23000052);
    writeAP(SWD_AP_TAR, addr);
    readAP(SWD_AP_DRW); // Dummy read
    return readAP(SWD_AP_DRW); // Data
}

bool PicoSWD::haltCore() {
    return writeWord(DHCSR, 0xA05F0003); // Halt & DebugEnable
}

bool PicoSWD::runCore() {
    return writeWord(DHCSR, 0xA05F0001); // Run & DebugEnable
}

bool PicoSWD::writeCoreReg(uint8_t reg, uint32_t val) {
    if (!writeWord(DCRDR, val)) return false;
    return writeWord(DCRSR, 0x00010000 | reg); // Write, reg index
}

uint32_t PicoSWD::readCoreReg(uint8_t reg) {
    writeWord(DCRSR, 0x00000000 | reg); // Read, reg index
    delayMicroseconds(10); // Wait for transfer
    return readWord(DCRDR);
}

// --- Firmware Flashing (Using BootROM Functions) ---

// Look up BootROM function address from the table at 0x00000014
uint32_t PicoSWD::findRomFunc(char c1, char c2) {
    uint32_t tableAddr = readWord(0x00000014) & 0xFFFF; // Pointer is 16-bit
    uint16_t code = (c2 << 8) | c1;
    
    while (true) {
        uint32_t entry = readWord(tableAddr);
        if (entry == 0) break; // End of table
        if ((entry & 0xFFFF) == code) return (entry >> 16);
        tableAddr += 4;
    }
    return 0;
}

// Execute a function on the target by setting registers and PC
bool PicoSWD::callRomFunc(uint32_t funcAddr, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3) {
    if (funcAddr == 0) return false;
    
    writeCoreReg(0, r0);
    writeCoreReg(1, r1);
    writeCoreReg(2, r2);
    writeCoreReg(3, r3);
    writeCoreReg(14, 0); // LR = 0 (trap)
    writeCoreReg(15, funcAddr); // PC
    writeCoreReg(16, 0x01000000); // xPSR (Thumb bit)
    
    runCore();
    
    // Wait for halt (breakpoint/fault)
    for (int i = 0; i < 1000; i++) {
        uint32_t s = readWord(DHCSR);
        if (s & 0x00020000) return true; // S_HALT
        delay(1);
    }
    haltCore(); // Force halt if timeout
    return false;
}

bool PicoSWD::flashFirmware(File& firmwareFile, size_t size) {
    LOG_I("SWD: Starting firmware flash (%zu bytes)...", size);
    
    if (!haltCore()) {
        LOG_E("SWD: Failed to halt core");
        return false;
    }
    
    // Initialize Stack Pointer (Top of RAM)
    writeCoreReg(13, 0x20042000); 
    
    // Find ROM Functions
    uint32_t fn_connect = findRomFunc('I', 'F');
    uint32_t fn_exit_xip = findRomFunc('E', 'X');
    uint32_t fn_erase = findRomFunc('R', 'E');
    uint32_t fn_program = findRomFunc('R', 'P');
    uint32_t fn_flush = findRomFunc('F', 'C');
    
    LOG_I("ROM Funcs: Conn=0x%08X Exit=0x%08X Erase=0x%08X Prog=0x%08X Flush=0x%08X", 
          fn_connect, fn_exit_xip, fn_erase, fn_program, fn_flush);
    
    if (!fn_connect || !fn_program) {
        LOG_E("SWD: Failed to find required ROM functions");
        return false;
    }
    
    // 1. Init Flash
    LOG_I("SWD: Initializing flash...");
    if (!callRomFunc(fn_connect, 0, 0, 0, 0)) {
        LOG_E("SWD: Flash init failed");
        return false;
    }
    
    if (!callRomFunc(fn_exit_xip, 0, 0, 0, 0)) {
        LOG_E("SWD: Exit XIP failed");
        return false;
    }
    
    // 2. Erase Flash
    LOG_I("SWD: Erasing flash (%zu bytes)...", size);
    // Round up size to 4096 (sector size)
    size_t eraseSize = (size + 4095) & ~4095;
    // Parameters: addr=0, count=eraseSize, block_size=4096, block_cmd=0x20 (4KB Sector Erase)
    if (!callRomFunc(fn_erase, 0, eraseSize, 4096, 0x20)) {
        LOG_E("SWD: Erase failed");
        return false;
    }
    
    // 3. Program Loop
    uint8_t buf[256];
    uint32_t ramAddr = 0x20000000; // Buffer in RAM
    size_t written = 0;
    firmwareFile.seek(0);
    
    LOG_I("SWD: Programming flash...");
    while (written < size) {
        size_t chunk = firmwareFile.read(buf, 256);
        if (chunk == 0) break;
        
        // Write to RAM
        writeAP(SWD_AP_CSW, 0x23000051); // 8-bit inc
        writeAP(SWD_AP_TAR, ramAddr);
        for (size_t i = 0; i < chunk; i++) {
            // Write byte-by-byte (can be optimized to packed writes if needed)
            writeAP(SWD_AP_DRW, buf[i]);
        }
        
        // Call Program: addr, data, count
        if (!callRomFunc(fn_program, written, ramAddr, chunk, 0)) {
            LOG_E("SWD: Program failed at offset %zu", written);
            return false;
        }
        written += chunk;
        
        if (written % 4096 == 0) {
            LOG_I("SWD: Flashed %zu bytes (%.1f%%)...", written, (written * 100.0f / size));
        }
        
        // Feed ESP32 watchdog
        vTaskDelay(1); 
    }
    
    // 4. Flush Cache & Reboot
    LOG_I("SWD: Flushing cache...");
    callRomFunc(fn_flush, 0, 0, 0, 0);
    
    LOG_I("SWD: Firmware flash complete (%zu bytes)", written);
    return true;
}

bool PicoSWD::resetTarget() {
    LOG_I("SWD: Resetting target...");
    
    // AIRCR System Reset
    if (writeWord(AIRCR, 0x05FA0004)) {
        delay(100); // Wait for reset
        return true;
    }
    
    // Fallback to hardware reset pin if available
    if (_reset >= 0) {
        pinMode(_reset, OUTPUT);
        digitalWrite(_reset, LOW);
        delay(10);
        digitalWrite(_reset, HIGH);
        delay(100);
        LOG_I("SWD: Target reset via GPIO%d", _reset);
        return true;
    }
    
    LOG_W("SWD: Reset failed");
    return false;
}

// --- Helpers ---

uint32_t PicoSWD::readDP(uint8_t addr) { 
    return swdReadPacket(addr >> 2); 
}

bool PicoSWD::writeDP(uint8_t addr, uint32_t data, bool ignoreAck) { 
    return swdWritePacket(addr >> 2, data, ignoreAck); 
}

uint32_t PicoSWD::readAP(uint8_t addr) {
    writeDP(SWD_DP_SELECT, addr & 0xF0);
    return swdReadPacket((addr & 0x0C) >> 2 | 1); // AP Access
}

bool PicoSWD::writeAP(uint8_t addr, uint32_t data) {
    writeDP(SWD_DP_SELECT, addr & 0xF0);
    return swdWritePacket((addr & 0x0C) >> 2 | 1, data);
}
