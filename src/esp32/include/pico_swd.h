#pragma once
#include <Arduino.h>
#include <FS.h>

// Forward declaration
class File;

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
     * Check if SWD connection is active
     * @return true if connected
     */
    bool isConnected() { return _connected; }

private:
    int _swdio;
    int _swclk;
    int _reset;
    bool _connected;
    
    // SWD Low Level
    void setSWDIO(bool high);
    void setSWCLK(bool high);
    bool readSWDIO();
    void swdWrite(uint8_t data, uint8_t bits);
    uint8_t swdRead(uint8_t bits);
    void swdTurnaround();
    void swdIdle();
    void swdLineReset();
    
    // Packet Layer
    uint32_t swdReadPacket(uint8_t request);
    bool swdWritePacket(uint8_t request, uint32_t data, bool ignoreAck = false);
    
    // DP/AP Access
    uint32_t readDP(uint8_t addr);
    bool writeDP(uint8_t addr, uint32_t data, bool ignoreAck = false);
    uint32_t readAP(uint8_t addr);
    bool writeAP(uint8_t addr, uint32_t data);
    
    // Memory Access
    bool writeWord(uint32_t addr, uint32_t data);
    uint32_t readWord(uint32_t addr);
    bool writeBlock(uint32_t addr, const uint8_t* data, size_t size);
    
    // Core Control
    bool initAP();
    bool haltCore();
    bool runCore();
    bool writeCoreReg(uint8_t reg, uint32_t val);
    uint32_t readCoreReg(uint8_t reg);
    
    // RP2040 Specific
    bool connectToTarget();
    uint32_t findRomFunc(char c1, char c2);
    bool callRomFunc(uint32_t funcAddr, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3);
    
    // Constants
    static constexpr uint32_t SWD_DELAY = 1; // microseconds
    static constexpr uint32_t RP2040_CORE0_ID = 0x01002927;
};
