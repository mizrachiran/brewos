/**
 * Pico Firmware - Hardware Diagnostics
 * 
 * Self-test and diagnostic functions for validating hardware wiring
 * and component functionality. Similar to boot-time self-tests but
 * can be triggered on-demand from the ESP32/web UI.
 */

#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// Diagnostic Result Structure
// =============================================================================

/**
 * Result of a single diagnostic test
 */
typedef struct {
    uint8_t test_id;           // Test identifier (DIAG_TEST_xxx)
    uint8_t status;            // DIAG_STATUS_xxx
    int16_t raw_value;         // Raw sensor value (if applicable)
    int16_t expected_min;      // Expected minimum value
    int16_t expected_max;      // Expected maximum value
    char message[32];          // Human-readable result message
} diag_result_t;

/**
 * Full diagnostic report containing all test results
 */
typedef struct {
    uint8_t test_count;        // Number of tests run
    uint8_t pass_count;        // Number of tests passed
    uint8_t fail_count;        // Number of tests failed
    uint8_t warn_count;        // Number of tests with warnings
    uint8_t skip_count;        // Number of tests skipped
    uint32_t duration_ms;      // Total test duration
    diag_result_t results[16]; // Individual test results
} diag_report_t;

// =============================================================================
// Diagnostic Functions
// =============================================================================

/**
 * Initialize diagnostics module
 */
void diagnostics_init(void);

/**
 * Run all diagnostic tests
 * @param report Pointer to report structure to fill
 * @return true if all tests passed (no failures)
 */
bool diagnostics_run_all(diag_report_t* report);

/**
 * Run a single diagnostic test
 * @param test_id Test to run (DIAG_TEST_xxx)
 * @param result Pointer to result structure to fill
 * @return Test status (DIAG_STATUS_xxx)
 */
uint8_t diagnostics_run_test(uint8_t test_id, diag_result_t* result);

/**
 * Check if diagnostics are currently running
 * @return true if a diagnostic test is in progress
 */
bool diagnostics_is_running(void);

/**
 * Abort any running diagnostics
 */
void diagnostics_abort(void);

// =============================================================================
// Individual Test Functions
// =============================================================================

/**
 * Test brew boiler NTC sensor
 * Checks: sensor connected, reading in valid range, no open/short circuit
 */
uint8_t diag_test_brew_ntc(diag_result_t* result);

/**
 * Test steam boiler NTC sensor
 * Checks: sensor connected, reading in valid range, no open/short circuit
 */
uint8_t diag_test_steam_ntc(diag_result_t* result);

/**
 * Test pressure transducer
 * Checks: ADC reading, voltage in expected range, ~0 bar when cold
 */
uint8_t diag_test_pressure(diag_result_t* result);

/**
 * Test water level sensors
 * Checks: reservoir sensor, tank level, steam level inputs
 */
uint8_t diag_test_water_level(diag_result_t* result);

/**
 * Test brew SSR output
 * Checks: PWM signal generation (brief pulse)
 */
uint8_t diag_test_ssr_brew(diag_result_t* result);

/**
 * Test steam SSR output
 * Checks: PWM signal generation (brief pulse)
 */
uint8_t diag_test_ssr_steam(diag_result_t* result);

/**
 * Test pump relay
 * Checks: relay control signal (brief activation, no water flow)
 */
uint8_t diag_test_relay_pump(diag_result_t* result);

/**
 * Test brew solenoid relay
 * Checks: relay control signal (brief activation)
 */
uint8_t diag_test_relay_solenoid(diag_result_t* result);

/**
 * Test power meter communication (PZEM, JSY, Eastron, etc.)
 * Checks: UART/Modbus response, voltage/current readings
 */
uint8_t diag_test_power_meter(diag_result_t* result);

/**
 * Test ESP32 communication
 * Checks: UART connectivity, recent packets received
 */
uint8_t diag_test_esp32_comm(diag_result_t* result);

/**
 * Test buzzer output
 * Checks: buzzer GPIO, produces brief chirp
 */
uint8_t diag_test_buzzer(diag_result_t* result);

/**
 * Test status LED
 * Checks: LED GPIO, brief flash
 */
uint8_t diag_test_led(diag_result_t* result);

/**
 * Test WEIGHT_STOP input (GPIO21)
 * Checks: GPIO configured correctly, can read signal from ESP32 GPIO19
 * Note: Requires ESP32 to toggle WEIGHT_STOP signal for full end-to-end test
 */
uint8_t diag_test_weight_stop_input(diag_result_t* result);

// =============================================================================
// Class B Safety Tests (IEC 60730/60335 Annex R)
// =============================================================================

/**
 * Run all Class B safety tests
 * Checks: RAM, Flash CRC, CPU registers, I/O, Clock, Stack, PC
 */
uint8_t diag_test_class_b_all(diag_result_t* result);

/**
 * Test RAM using March C- algorithm
 * Checks: RAM integrity with walking bit pattern
 */
uint8_t diag_test_class_b_ram(diag_result_t* result);

/**
 * Test Flash CRC verification
 * Checks: Application code integrity against stored reference
 */
uint8_t diag_test_class_b_flash(diag_result_t* result);

/**
 * Test CPU registers
 * Checks: Register pattern write/read/verify
 */
uint8_t diag_test_class_b_cpu(diag_result_t* result);

/**
 * Test I/O state verification
 * Checks: GPIO output state read-back matches expected
 */
uint8_t diag_test_class_b_io(diag_result_t* result);

/**
 * Test system clock frequency
 * Checks: Clock within ±5% tolerance
 */
uint8_t diag_test_class_b_clock(diag_result_t* result);

/**
 * Test stack overflow detection
 * Checks: Stack canary values intact
 */
uint8_t diag_test_class_b_stack(diag_result_t* result);

/**
 * Test program counter
 * Checks: Execution flow verification
 */
uint8_t diag_test_class_b_pc(diag_result_t* result);

#endif // DIAGNOSTICS_H

