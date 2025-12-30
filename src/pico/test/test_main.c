/**
 * BrewOS Pico Firmware - Unit Test Runner
 * 
 * Runs all unit tests for the Pico firmware.
 * Tests are designed to run on the host machine (not the Pico),
 * using mock implementations of hardware functions.
 * 
 * Usage:
 *   ./brewos_tests
 * 
 * Returns:
 *   0 if all tests pass
 *   Non-zero if any tests fail
 */

#include <stdio.h>
#include "unity/unity.h"

// =============================================================================
// Global Setup and Teardown (required by Unity)
// =============================================================================

void setUp(void) {
    // Called before each test
    // Individual test files can initialize their own state as needed
}

void tearDown(void) {
    // Called after each test
}

// =============================================================================
// External Test Runners
// =============================================================================

extern int run_sensor_utils_tests(void);
extern int run_protocol_tests(void);
extern int run_protocol_advanced_tests(void);
extern int run_pid_tests(void);
extern int run_cleaning_tests(void);
extern int run_config_validation_tests(void);
extern int run_power_meter_tests(void);
extern int run_class_b_tests(void);
extern int run_preinfusion_tests(void);
extern int run_validation_tests(void);

// =============================================================================
// Main Entry Point
// =============================================================================

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    int total_failures = 0;
    
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║           BrewOS Pico Firmware - Unit Tests                  ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    // Run all test suites
    printf("Running Sensor Utils Tests...\n");
    total_failures += run_sensor_utils_tests();
    
    printf("Running Protocol Tests...\n");
    total_failures += run_protocol_tests();
    
    printf("Running Protocol Advanced Tests...\n");
    total_failures += run_protocol_advanced_tests();
    
    printf("Running PID Controller Tests...\n");
    total_failures += run_pid_tests();
    
    printf("Running Cleaning Mode Tests...\n");
    total_failures += run_cleaning_tests();
    
    printf("Running Config Validation Tests...\n");
    total_failures += run_config_validation_tests();
    
    printf("Running Power Meter Tests...\n");
    total_failures += run_power_meter_tests();
    
    printf("Running Class B Safety Tests...\n");
    total_failures += run_class_b_tests();
    
    printf("Running Pre-Infusion Tests...\n");
    total_failures += run_preinfusion_tests();
    
    printf("Running Validation Tests...\n");
    total_failures += run_validation_tests();
    
    // Print summary
    printf("\n");
    printf("══════════════════════════════════════════════════════════════\n");
    if (total_failures == 0) {
        printf("                    ✅ ALL TESTS PASSED ✅                    \n");
    } else {
        printf("              ❌ %d TEST SUITE(S) HAD FAILURES ❌              \n", total_failures);
    }
    printf("══════════════════════════════════════════════════════════════\n");
    
    return total_failures;
}

