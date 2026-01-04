/**
 * ESP32-S3 Hardware Diagnostics
 * 
 * Diagnostic tests for ESP32-side GPIO pins and hardware.
 */

#ifndef ESP32_DIAGNOSTICS_H
#define ESP32_DIAGNOSTICS_H

#include <stdint.h>
#include "protocol_defs.h"

// =============================================================================
// Diagnostic Result Structure (matches Pico diagnostics)
// =============================================================================

typedef struct {
    uint8_t test_id;           // Test identifier (DIAG_TEST_xxx)
    uint8_t status;            // DIAG_STATUS_xxx
    int16_t raw_value;         // Raw value (if applicable)
    int16_t expected_min;      // Expected minimum value
    int16_t expected_max;      // Expected maximum value
    char message[32];          // Human-readable result message
} diag_result_t;

// =============================================================================
// ESP32 Diagnostic Functions
// =============================================================================

/**
 * Test GPIO19 (WEIGHT_STOP) output
 * Verifies pin can be set HIGH and LOW
 */
uint8_t diag_test_weight_stop_output(diag_result_t* result);

/**
 * Test GPIO20 (PICO_RUN) output
 * Verifies pin can be set HIGH and LOW
 */
uint8_t diag_test_pico_run_output(diag_result_t* result);

/**
 * Run an ESP32-side diagnostic test
 * @param test_id Test ID (DIAG_TEST_WEIGHT_STOP or DIAG_TEST_PICO_RUN)
 * @param result Result structure to fill
 * @return Test status
 */
uint8_t esp32_diagnostics_run_test(uint8_t test_id, diag_result_t* result);

/**
 * Check if a test ID is an ESP32-side test
 * @param test_id Test ID to check
 * @return true if test runs on ESP32, false if it runs on Pico
 */
bool esp32_diagnostics_is_esp32_test(uint8_t test_id);

#endif // ESP32_DIAGNOSTICS_H

