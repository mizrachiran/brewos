#pragma once
#include <Arduino.h>
#include <FS.h>

// Forward declaration
class File;

// RP2350 AP Selection (from reference)
#define AP_ROM_TABLE    0x0   // ROM Table
#define AP_ARM_CORE0    0x2   // ARM Core 0 AHB-AP (used for memory operations and BootROM)
#define AP_ARM_CORE1    0x4   // ARM Core 1 AHB-AP
#define AP_RP_SPECIFIC  0x8   // RP-AP (Raspberry Pi specific)
#define AP_RISCV        0xA   // RISC-V APB-AP (used only for Debug Module initialization)

// ACK Response Codes (from reference)
#define SWD_ACK_OK     0x1
#define SWD_ACK_WAIT   0x2
#define SWD_ACK_FAULT  0x4
#define SWD_ACK_ERROR  0x7

// Error Codes (from reference project)
typedef enum {
    SWD_OK = 0,                    ///< Operation successful
    SWD_ERROR_TIMEOUT,             ///< Operation timed out
    SWD_ERROR_FAULT,               ///< Target returned FAULT ACK
    SWD_ERROR_PROTOCOL,            ///< SWD protocol error
    SWD_ERROR_PARITY,              ///< Parity check failed
    SWD_ERROR_WAIT,                ///< Target returned WAIT ACK (retry exhausted)
    SWD_ERROR_NOT_CONNECTED,       ///< Target not connected
    SWD_ERROR_NOT_HALTED,          ///< Operation requires hart to be halted
    SWD_ERROR_ALREADY_HALTED,      ///< Hart is already halted
    SWD_ERROR_INVALID_STATE,        ///< Invalid state for operation
    SWD_ERROR_NO_MEMORY,           ///< Memory allocation failed
    SWD_ERROR_INVALID_CONFIG,      ///< Invalid configuration
    SWD_ERROR_RESOURCE_BUSY,       ///< PIO/SM already in use
    SWD_ERROR_INVALID_PARAM,       ///< Invalid parameter
    SWD_ERROR_NOT_INITIALIZED,     ///< Debug module not initialized
    SWD_ERROR_ABSTRACT_CMD,        ///< Abstract command failed
    SWD_ERROR_BUS,                 ///< System bus access error
    SWD_ERROR_ALIGNMENT,           ///< Memory address alignment error
    SWD_ERROR_VERIFY,              ///< Memory verification failed
} swd_error_t;

/**
 * PicoSWD - SWD-based firmware flashing for RP2040/Pico using BootROM functions
 * 
 * This class implements the SWD (Serial Wire Debug) protocol for flashing
 * firmware to the RP2040 via SWDIO and SWCLK pins.
 * 
 * Uses RP2040 BootROM functions instead of loading a flash algorithm blob.
 */
class PicoSWD {
public:
    PicoSWD(int swdio, int swclk, int reset = -1);
    
    /**
     * Initialize SWD interface
     * @return true if initialization successful
     */
    bool begin();
    
    /**
     * Disconnect from SWD interface
     */
    void end();
    
    /**
     * Flash firmware file from LittleFS to the Pico via SWD
     * @param firmwareFile File handle to firmware binary
     * @param size Size of firmware in bytes
     * @return true if flashing successful
     */
    bool flashFirmware(File& firmwareFile, size_t size);
    
    /**
     * Reset the Pico via SWD (using AIRCR register)
     * @return true if reset successful
     */
    bool resetTarget();
    
    /**
     * Resume Pico core from debug halt (breaks out of debug halt state)
     * @return true if resume successful
     */
    bool resumeFromHalt();
    
    /**
     * Check if SWD connection is active
     * @return true if connected
     */
    bool isConnected() { return _connected; }
    
    // Diagnostic
    const char* getLastError() { return _lastErrorStr; }
    static const char* errorToString(swd_error_t error);
    
    /**
     * Diagnostic function to check device state and pin connectivity
     * @return true if device appears to be responding
     */
    bool diagnoseDevice();
    
    /**
     * Read raw ACK bits with detailed logging
     * @return Raw ACK value and detailed diagnostic info
     */
    uint8_t readAckDiagnostic();

private:
    int _swdio;
    int _swclk;
    int _reset;
    bool _connected;
    const char* _lastErrorStr;
    
    // SWD Low Level
    void setSWDIO(bool high);
    void setSWCLK(bool high);
    bool readSWDIO();
    void swdWrite(uint8_t data, uint8_t bits);
    uint8_t swdRead(uint8_t bits);
    void swdTurnaround();
    void swdIdle();
    void swdSendIdleClocks(uint8_t count);
    void swdResetSeq();
    void swdLineReset();
    void swdLineResetSoft();
    void sendDormantSequence();
    bool powerUpDebug();
    bool powerDownDebug();
    
    // Packet Layer
    swd_error_t swdReadPacket(uint8_t request, uint32_t *data);
    swd_error_t swdWritePacket(uint8_t request, uint32_t data, bool ignoreAck = false);
    const char* ackToString(uint8_t ack);
    
    // Protocol Wrappers
    swd_error_t readDP(uint8_t addr, uint32_t *data);
    swd_error_t writeDP(uint8_t addr, uint32_t data, bool ignoreAck = false);
    swd_error_t readAP(uint8_t addr, uint32_t *data, uint8_t ap_id = AP_ARM_CORE0);
    swd_error_t writeAP(uint8_t addr, uint32_t data, uint8_t ap_id = AP_ARM_CORE0);
    
    // RP2350-specific helpers
    uint32_t makeDPSelectRP2350(uint8_t apsel, uint8_t bank, bool ctrlsel);
    bool initRP2350DebugModule();
    bool initDebugModule();  // Simpler DM reset (fixes unhealthy states)
    
    // Memory/Core
    bool writeWord(uint32_t addr, uint32_t data);
    uint32_t readWord(uint32_t addr);
    bool initAP();
    bool haltCore();
    bool runCore();
    bool writeCoreReg(uint8_t reg, uint32_t val);
    uint32_t readCoreReg(uint8_t reg);
    
    // BootROM
    bool connectToTarget();
    uint32_t findRomFunc(char c1, char c2);
    bool callRomFunc(uint32_t funcAddr, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3);
};