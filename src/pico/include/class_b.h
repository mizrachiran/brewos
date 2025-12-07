/**
 * ECM Pico Firmware - IEC 60730/60335 Class B Safety Routines
 * 
 * Implements self-test routines required for Class B compliance per Annex R:
 * - RAM test (walking bit pattern)
 * - Flash CRC verification (periodic)
 * - CPU register test
 * - I/O state verification
 * - Clock test
 * 
 * IMPORTANT: This implementation provides Class B safety self-tests but has
 * NOT been certified by an accredited test laboratory. For safety-critical
 * production use, formal certification (e.g., TÜV, UL) is required.
 * 
 * For hobbyist/non-certified use: These routines provide additional safety
 * margin but do not replace formal certification for commercial products.
 */

#ifndef CLASS_B_H
#define CLASS_B_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Class B Test Results
// =============================================================================

typedef enum {
    CLASS_B_PASS = 0,           // Test passed
    CLASS_B_FAIL_RAM,           // RAM test failed
    CLASS_B_FAIL_FLASH,         // Flash CRC mismatch
    CLASS_B_FAIL_CPU,           // CPU register test failed
    CLASS_B_FAIL_IO,            // I/O state verification failed
    CLASS_B_FAIL_CLOCK,         // Clock frequency out of tolerance
    CLASS_B_FAIL_STACK,         // Stack overflow detected
    CLASS_B_FAIL_PC,            // Program counter test failed
    CLASS_B_NOT_INITIALIZED     // Class B not initialized
} class_b_result_t;

// =============================================================================
// Class B Status Structure
// =============================================================================

typedef struct {
    class_b_result_t last_result;       // Result of last test cycle
    uint32_t ram_test_count;            // Number of RAM tests completed
    uint32_t flash_test_count;          // Number of Flash CRC checks completed
    uint32_t cpu_test_count;            // Number of CPU tests completed
    uint32_t io_test_count;             // Number of I/O tests completed
    uint32_t clock_test_count;          // Number of clock tests completed
    uint32_t fail_count;                // Total number of failures detected
    uint32_t last_test_time_ms;         // Timestamp of last test cycle
    uint32_t flash_crc_reference;       // Reference CRC stored at boot
    uint32_t flash_crc_calculated;      // Last calculated CRC
    bool initialized;                   // True if class B is initialized
} class_b_status_t;

// =============================================================================
// Configuration
// =============================================================================

// RAM test configuration
#define CLASS_B_RAM_TEST_SIZE       64      // Bytes to test per cycle (non-destructive)
#define CLASS_B_RAM_TEST_PATTERN    0xAA55  // Walking bit pattern

// Flash CRC configuration (CRC-32 over application code)
#define CLASS_B_FLASH_START         0x10000000  // Start of flash (XIP base)
#define CLASS_B_FLASH_SIZE          (256 * 1024) // 256KB application size

// Clock test configuration (system clock = 125MHz typical)
#define CLASS_B_CLOCK_NOMINAL_HZ    125000000
#define CLASS_B_CLOCK_TOLERANCE_PCT 5           // ±5% tolerance

// Test intervals (in main loop cycles at 10Hz = 100ms per cycle)
#define CLASS_B_RAM_TEST_INTERVAL       10      // Every 1 second
#define CLASS_B_FLASH_TEST_INTERVAL     600     // Every 60 seconds
#define CLASS_B_CPU_TEST_INTERVAL       10      // Every 1 second
#define CLASS_B_IO_TEST_INTERVAL        10      // Every 1 second
#define CLASS_B_CLOCK_TEST_INTERVAL     100     // Every 10 seconds

// Stack canary configuration
#define CLASS_B_STACK_CANARY_VALUE      0xDEADBEEF

// =============================================================================
// Initialization
// =============================================================================

/**
 * Initialize Class B safety routines
 * 
 * Must be called early in boot sequence, before main loop.
 * - Calculates and stores reference Flash CRC
 * - Initializes stack canaries
 * - Runs initial self-test
 * 
 * @return CLASS_B_PASS on success, error code on failure
 */
class_b_result_t class_b_init(void);

// =============================================================================
// Startup Tests (Run Once at Boot)
// =============================================================================

/**
 * Run full startup self-test (all tests)
 * 
 * This is a comprehensive test run at boot before entering main loop.
 * Takes longer than periodic tests but covers full RAM and Flash.
 * 
 * @return CLASS_B_PASS if all tests pass, first error code otherwise
 */
class_b_result_t class_b_startup_test(void);

// =============================================================================
// Periodic Tests (Run During Operation)
// =============================================================================

/**
 * Run periodic Class B tests
 * 
 * Call this from the main control loop. Tests are staggered across
 * multiple calls to minimize impact on real-time performance.
 * 
 * @return CLASS_B_PASS if tests pass, error code on failure
 */
class_b_result_t class_b_periodic_test(void);

// =============================================================================
// Individual Tests
// =============================================================================

/**
 * RAM test using walking bit pattern
 * 
 * Tests a portion of RAM each call (non-destructive march test).
 * Uses a dedicated test region to avoid corrupting application data.
 * 
 * @return CLASS_B_PASS or CLASS_B_FAIL_RAM
 */
class_b_result_t class_b_test_ram(void);

/**
 * Flash CRC verification
 * 
 * Calculates CRC-32 over application Flash and compares to reference.
 * This is done incrementally to avoid blocking the main loop.
 * 
 * @return CLASS_B_PASS or CLASS_B_FAIL_FLASH
 */
class_b_result_t class_b_test_flash(void);

/**
 * CPU register test
 * 
 * Tests core CPU registers using pattern write/read/verify.
 * Covers R0-R12, SP, LR (PC is tested separately).
 * 
 * @return CLASS_B_PASS or CLASS_B_FAIL_CPU
 */
class_b_result_t class_b_test_cpu_registers(void);

/**
 * I/O state verification
 * 
 * Verifies GPIO output states match expected values.
 * Reads back output registers and compares to shadow state.
 * 
 * @return CLASS_B_PASS or CLASS_B_FAIL_IO
 */
class_b_result_t class_b_test_io(void);

/**
 * System clock frequency test
 * 
 * Verifies system clock is within tolerance using independent
 * timer/counter measurement.
 * 
 * @return CLASS_B_PASS or CLASS_B_FAIL_CLOCK
 */
class_b_result_t class_b_test_clock(void);

/**
 * Stack overflow detection
 * 
 * Checks stack canary values to detect overflow.
 * 
 * @return CLASS_B_PASS or CLASS_B_FAIL_STACK
 */
class_b_result_t class_b_test_stack(void);

/**
 * Program counter test
 * 
 * Verifies program execution flow using function call verification.
 * 
 * @return CLASS_B_PASS or CLASS_B_FAIL_PC
 */
class_b_result_t class_b_test_program_counter(void);

// =============================================================================
// Status and Error Handling
// =============================================================================

/**
 * Get Class B status structure
 * 
 * @param status Pointer to status structure to fill
 */
void class_b_get_status(class_b_status_t* status);

/**
 * Get human-readable error message
 * 
 * @param result Class B result code
 * @return Static string describing the result
 */
const char* class_b_result_string(class_b_result_t result);

/**
 * Check if Class B is in safe state
 * 
 * Returns true if any Class B test has failed.
 * When true, system should be in safe state with outputs disabled.
 * 
 * @return true if Class B failure detected
 */
bool class_b_is_failed(void);

/**
 * Reset Class B failure state
 * 
 * Only call this after verifying all conditions are safe.
 * Typically requires power cycle or explicit user action.
 * 
 * @return true if reset successful
 */
bool class_b_reset(void);

// =============================================================================
// CRC-32 Utility (also used by protocol for Flash verification)
// =============================================================================

/**
 * Calculate CRC-32 over data buffer
 * 
 * Uses standard CRC-32 polynomial (0xEDB88320, reflected).
 * 
 * @param data Pointer to data buffer
 * @param length Number of bytes to process
 * @param initial Initial CRC value (use 0xFFFFFFFF for first call)
 * @return Calculated CRC-32 value
 */
uint32_t class_b_crc32(const uint8_t* data, size_t length, uint32_t initial);

/**
 * Calculate CRC-32 over Flash region
 * 
 * Incrementally calculates CRC over Flash, processing a chunk per call.
 * Call repeatedly until it returns true (complete).
 * 
 * @param crc Pointer to running CRC value
 * @param complete Set to true when calculation is complete
 * @return CLASS_B_PASS while in progress, result when complete
 */
class_b_result_t class_b_crc32_flash_incremental(uint32_t* crc, bool* complete);

#ifdef __cplusplus
}
#endif

#endif // CLASS_B_H

