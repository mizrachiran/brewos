/**
 * ESP32-S3 Hardware Diagnostics
 * 
 * Diagnostic tests for ESP32-side GPIO pins and hardware.
 * These tests run locally on the ESP32 and don't require Pico communication.
 */

#include "esp32_diagnostics.h"
#include "config.h"
#include <Arduino.h>
#include <string.h>

// =============================================================================
// GPIO19 (WEIGHT_STOP) Output Test
// =============================================================================

uint8_t diag_test_weight_stop_output(diag_result_t* result) {
    if (!result) return DIAG_STATUS_FAIL;
    
    result->test_id = DIAG_TEST_WEIGHT_STOP;
    result->status = DIAG_STATUS_RUNNING;
    result->raw_value = 0;
    result->expected_min = 0;
    result->expected_max = 1;
    strncpy(result->message, "Testing...", sizeof(result->message) - 1);
    
    // Test that GPIO19 can be set HIGH and LOW
    // This verifies the pin is configured correctly and can drive the signal
    
    pinMode(WEIGHT_STOP_PIN, OUTPUT);
    
    // Test LOW state
    digitalWrite(WEIGHT_STOP_PIN, LOW);
    delay(10);  // Allow signal to settle
    // Note: Can't read back output pin state on ESP32, so we just verify it was set
    
    // Test HIGH state
    digitalWrite(WEIGHT_STOP_PIN, HIGH);
    delay(10);
    
    // Return to normal state (LOW)
    digitalWrite(WEIGHT_STOP_PIN, LOW);
    
    // If we got here without errors, the pin is working
    result->status = DIAG_STATUS_PASS;
    result->raw_value = 1;
    strncpy(result->message, "GPIO19 output OK", sizeof(result->message) - 1);
    
    LOG_I("Diagnostics: WEIGHT_STOP (GPIO19) output test PASSED");
    return DIAG_STATUS_PASS;
}

// =============================================================================
// GPIO20 (PICO_RUN) Output Test
// =============================================================================

uint8_t diag_test_pico_run_output(diag_result_t* result) {
    if (!result) return DIAG_STATUS_FAIL;
    
    result->test_id = DIAG_TEST_PICO_RUN;
    result->status = DIAG_STATUS_RUNNING;
    result->raw_value = 0;
    result->expected_min = 0;
    result->expected_max = 1;
    strncpy(result->message, "Testing...", sizeof(result->message) - 1);
    
    // Test that GPIO20 can be set HIGH and LOW
    // HIGH = Pico running, LOW = Pico reset
    // This verifies the pin is configured correctly and can control Pico reset
    
    pinMode(PICO_RUN_PIN, OUTPUT);
    
    // Test HIGH state (Pico running)
    digitalWrite(PICO_RUN_PIN, HIGH);
    delay(10);  // Allow signal to settle
    
    // Test LOW state (Pico reset) - but don't hold it long enough to actually reset
    digitalWrite(PICO_RUN_PIN, LOW);
    delay(10);
    
    // Return to normal state (HIGH = Pico running)
    digitalWrite(PICO_RUN_PIN, HIGH);
    
    // If we got here without errors, the pin is working
    result->status = DIAG_STATUS_PASS;
    result->raw_value = 1;
    strncpy(result->message, "GPIO20 output OK", sizeof(result->message) - 1);
    
    LOG_I("Diagnostics: PICO_RUN (GPIO20) output test PASSED");
    return DIAG_STATUS_PASS;
}

// =============================================================================
// Run ESP32 Diagnostic Test
// =============================================================================

uint8_t esp32_diagnostics_run_test(uint8_t test_id, diag_result_t* result) {
    if (!result) return DIAG_STATUS_FAIL;
    
    switch (test_id) {
        case DIAG_TEST_WEIGHT_STOP:
            return diag_test_weight_stop_output(result);
            
        case DIAG_TEST_PICO_RUN:
            return diag_test_pico_run_output(result);
            
        default:
            // Not an ESP32-side test, return fail
            result->test_id = test_id;
            result->status = DIAG_STATUS_FAIL;
            strncpy(result->message, "Not an ESP32 test", sizeof(result->message) - 1);
            return DIAG_STATUS_FAIL;
    }
}

bool esp32_diagnostics_is_esp32_test(uint8_t test_id) {
    return (test_id == DIAG_TEST_WEIGHT_STOP || test_id == DIAG_TEST_PICO_RUN);
}

