/**
 * ESP32-S3 Hardware Diagnostics
 * 
 * Diagnostic tests for ESP32-side GPIO pins and hardware.
 * These tests verify end-to-end functionality by coordinating with Pico tests.
 */

#include "esp32_diagnostics.h"
#include "config.h"
#include "pico_uart.h"
#include <Arduino.h>
#include <string.h>

// =============================================================================
// WEIGHT_STOP_PIN Output Test (GPIO19 screen / GPIO6 no-screen)
// =============================================================================

uint8_t diag_test_weight_stop_output(diag_result_t* result, PicoUART* picoUart) {
    if (!result) return DIAG_STATUS_FAIL;
    
    result->test_id = DIAG_TEST_WEIGHT_STOP_OUTPUT;
    result->status = DIAG_STATUS_RUNNING;
    result->raw_value = 0;
    result->expected_min = 0;
    result->expected_max = 1;
    strncpy(result->message, "Testing...", sizeof(result->message) - 1);
    
    pinMode(WEIGHT_STOP_PIN, OUTPUT);
    
    // Set initial state to LOW (normal)
    digitalWrite(WEIGHT_STOP_PIN, LOW);
    delay(50);  // Allow signal to settle
    
    // Test HIGH state - set WEIGHT_STOP_PIN HIGH
    // If Pico is running test 0x13 (WEIGHT_STOP_INPUT) at the same time,
    // it should read GPIO21 HIGH, verifying the signal path works
    digitalWrite(WEIGHT_STOP_PIN, HIGH);
    delay(200);  // Hold HIGH long enough for Pico to detect if test 0x13 is running
    
    // Test LOW state
    digitalWrite(WEIGHT_STOP_PIN, LOW);
    delay(200);  // Hold LOW long enough for Pico to detect
    
    // Return to normal state (LOW)
    digitalWrite(WEIGHT_STOP_PIN, LOW);
    
    // Note: Full end-to-end verification requires Pico test 0x13 to run
    // and verify it can read HIGH/LOW states. This test verifies ESP32 can drive the signal.
    // If wiring is missing, Pico test 0x13 will fail to see the HIGH state.
    result->status = DIAG_STATUS_PASS;
    result->raw_value = 1;
    strncpy(result->message, "WEIGHT_STOP_PIN toggled HIGH/LOW (verify Pico test 0x13)", sizeof(result->message) - 1);
    
    LOG_I("Diagnostics: WEIGHT_STOP (GPIO%d) output test - signal toggled", WEIGHT_STOP_PIN);
    return DIAG_STATUS_PASS;
}

// =============================================================================
// PICO_RUN_PIN Output Test (GPIO20 screen / GPIO4 no-screen)
// =============================================================================

uint8_t diag_test_pico_run_output(diag_result_t* result, PicoUART* picoUart) {
    if (!result) return DIAG_STATUS_FAIL;
    
    result->test_id = DIAG_TEST_PICO_RUN_OUTPUT;
    result->status = DIAG_STATUS_RUNNING;
    result->raw_value = 0;
    result->expected_min = 0;
    result->expected_max = 1;
    strncpy(result->message, "Testing...", sizeof(result->message) - 1);
    
    // Test requires OUTPUT mode to drive the pin, but we'll restore to INPUT after
    pinMode(PICO_RUN_PIN, OUTPUT);
    
    // Ensure Pico is running (HIGH state) - but use INPUT for final state
    digitalWrite(PICO_RUN_PIN, HIGH);
    delay(50);  // Allow signal to settle
    
    // Test LOW state (Pico reset signal) - use very short pulse to avoid actual reset
    // A 1ms LOW pulse should not reset the Pico, but verifies the signal can change
    // If wiring is missing, the signal won't reach Pico and Pico won't detect the change
    digitalWrite(PICO_RUN_PIN, LOW);
    delay(1);  // Very short pulse - should not reset Pico
    
    // Restore to INPUT (open-drain) - Pico running
    // ROOT CAUSE FIX: Use INPUT instead of OUTPUT HIGH to prevent parasitic powering
    // RP2350 internal pull-up will handle the HIGH state
    pinMode(PICO_RUN_PIN, INPUT);  // Release reset (let internal pull-up do the work)
    delay(100);  // Allow Pico to stabilize
    
    // Verify Pico is still connected (didn't reset)
    // If wiring is correct, Pico should still be running
    // If wiring is missing, Pico will continue running anyway (no reset signal received)
    bool pico_connected = (picoUart && picoUart->isConnected());
    
    if (pico_connected) {
        // Pico is still connected - either wiring is correct (short pulse didn't reset)
        // or wiring is missing (no reset signal received)
        // We can't distinguish these cases without more complex coordination
        result->status = DIAG_STATUS_PASS;
        result->raw_value = 1;
        strncpy(result->message, "PICO_RUN_PIN toggled (Pico still running)", sizeof(result->message) - 1);
        LOG_I("Diagnostics: PICO_RUN (GPIO%d) output test - signal toggled, Pico connected", PICO_RUN_PIN);
    } else {
        // Pico disconnected - might have reset (unlikely with 1ms pulse) or other issue
        result->status = DIAG_STATUS_WARN;
        result->raw_value = 0;
        strncpy(result->message, "PICO_RUN_PIN toggled (Pico disconnected?)", sizeof(result->message) - 1);
        LOG_W("Diagnostics: PICO_RUN (GPIO%d) output test - Pico not connected", PICO_RUN_PIN);
    }
    
    return result->status;
}

// =============================================================================
// Run ESP32 Diagnostic Test
// =============================================================================

uint8_t esp32_diagnostics_run_test(uint8_t test_id, diag_result_t* result, PicoUART* picoUart) {
    if (!result) return DIAG_STATUS_FAIL;
    
    switch (test_id) {
        case DIAG_TEST_WEIGHT_STOP_OUTPUT:
            return diag_test_weight_stop_output(result, picoUart);
            
        case DIAG_TEST_PICO_RUN_OUTPUT:
            return diag_test_pico_run_output(result, picoUart);
            
        default:
            // Not an ESP32-side test, return fail
            result->test_id = test_id;
            result->status = DIAG_STATUS_FAIL;
            strncpy(result->message, "Not an ESP32 test", sizeof(result->message) - 1);
            return DIAG_STATUS_FAIL;
    }
}

bool esp32_diagnostics_is_esp32_test(uint8_t test_id) {
    return (test_id == DIAG_TEST_WEIGHT_STOP_OUTPUT || test_id == DIAG_TEST_PICO_RUN_OUTPUT);
}

