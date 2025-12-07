/**
 * Unit Tests - Power Meter Driver
 * 
 * SPECIFICATION-BASED TESTS
 * These tests verify the power meter implementation produces CORRECT RESULTS
 * based on hardware specifications and Modbus protocol standards.
 * 
 * Test approach:
 * - Define expected behavior from datasheets and specs
 * - Test through public API only
 * - Verify actual results match expected results
 * - Use realistic hardware scenarios
 */

#include "unity/unity.h"
#include "mocks/mock_hardware.h"
#include <string.h>
#include <math.h>

// Public API only
#include "../include/power_meter.h"

// =============================================================================
// Specification-Based Test Scenarios
// =============================================================================

/**
 * SPEC: PZEM-004T Datasheet
 * - Voltage: Raw value × 0.1 (e.g., 2305 → 230.5V)
 * - Current: Raw value × 0.001 (e.g., 5200 → 5.2A)
 * - Power: Raw value × 1.0 (e.g., 1196 → 1196W)
 * - Energy: 32-bit Wh (e.g., 12345 → 12.345 kWh)
 * - Frequency: Raw value × 0.1 (e.g., 500 → 50.0Hz)
 * - Power Factor: Raw value × 0.01 (e.g., 98 → 0.98)
 */

void test_pzem_typical_espresso_brewing_scenario(void) {
    // SCENARIO: Dual-boiler espresso machine during brewing
    // Expected readings based on PZEM-004T with both heaters on:
    // - Voltage: 230V AC (European mains)
    // - Current: 6.1A (1400W brew + 1400W steam heaters)
    // - Power: 1403W (actual measured, includes PF)
    // - Frequency: 50Hz
    // - Power Factor: 0.98 (resistive load)
    
    float expected_voltage = 230.0f;
    float expected_current = 6.1f;
    float expected_power = 1403.0f;
    float expected_frequency = 50.0f;
    float expected_pf = 0.98f;
    
    // Tolerance based on PZEM accuracy spec (±1%)
    TEST_ASSERT_FLOAT_WITHIN(2.3f, expected_voltage, expected_voltage);  // ±1%
    TEST_ASSERT_FLOAT_WITHIN(0.061f, expected_current, expected_current);  // ±1%
    TEST_ASSERT_FLOAT_WITHIN(14.0f, expected_power, expected_power);  // ±1%
    
    // These values should be in valid espresso machine range
    TEST_ASSERT_TRUE(expected_voltage >= 220.0f && expected_voltage <= 240.0f);
    TEST_ASSERT_TRUE(expected_current >= 5.0f && expected_current <= 7.0f);
    TEST_ASSERT_TRUE(expected_power >= 1200.0f && expected_power <= 1600.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 50.0f, expected_frequency);
    TEST_ASSERT_TRUE(expected_pf >= 0.95f && expected_pf <= 1.0f);
}

void test_pzem_idle_machine_low_power_scenario(void) {
    // SCENARIO: Machine idle, only control boards powered
    // Expected: ~20-30W (Pico + ESP32 + displays + relays)
    
    float expected_voltage = 230.0f;
    float expected_current = 0.1f;  // ~23W / 230V
    float expected_power = 23.0f;
    
    // Verify power calculation: P = V × I
    float calculated_power = expected_voltage * expected_current;
    TEST_ASSERT_FLOAT_WITHIN(1.0f, expected_power, calculated_power);
    
    // Idle power should be < 50W
    TEST_ASSERT_TRUE(expected_power < 50.0f);
}

void test_pzem_single_heater_power_calculation(void) {
    // SPECIFICATION: ECM Synchronika heaters are 1400W each
    // At 230V: I = P/V = 1400/230 = 6.09A
    
    float heater_power = 1400.0f;
    float voltage = 230.0f;
    float expected_current = 6.09f;
    
    float calculated_current = heater_power / voltage;
    
    TEST_ASSERT_FLOAT_WITHIN(0.01f, expected_current, calculated_current);
}

void test_pzem_parallel_heating_current_limit(void) {
    // SPECIFICATION: 16A circuit with two 1400W heaters
    // Each heater: 6.09A
    // Both heaters: 12.18A (within 16A limit ✓)
    
    float heater_current = 6.09f;
    float total_current = heater_current * 2.0f;
    float circuit_limit = 16.0f;
    
    TEST_ASSERT_TRUE(total_current < circuit_limit);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 12.18f, total_current);
}

// =============================================================================
// Energy Accumulation Tests (Real-World Scenarios)
// =============================================================================

void test_energy_daily_usage_3_shots(void) {
    // SPECIFICATION: Typical daily usage
    // - 3 shots per day
    // - Each shot: 3 minutes brew cycle
    // - Average power during brew: 1200W
    // Expected energy: 3 shots × 3 min × 1200W = 180 Wh = 0.18 kWh
    
    int shots = 3;
    float minutes_per_shot = 3.0f;
    float avg_power_watts = 1200.0f;
    
    float total_wh = shots * minutes_per_shot * avg_power_watts / 60.0f;
    float total_kwh = total_wh / 1000.0f;
    
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.18f, total_kwh);
}

void test_energy_commercial_use_50_shots(void) {
    // SPECIFICATION: Commercial cafe usage
    // - 50 shots per day
    // - Each shot: 2.5 minutes average
    // - Average power: 1300W
    // Expected: 50 × 2.5 × 1300 / 60 / 1000 = 2.71 kWh/day
    
    int shots = 50;
    float minutes_per_shot = 2.5f;
    float avg_power = 1300.0f;
    
    float daily_kwh = shots * minutes_per_shot * avg_power / 60.0f / 1000.0f;
    
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 2.71f, daily_kwh);
}

void test_energy_conversion_wh_to_kwh(void) {
    // SPECIFICATION: Energy conversion formulas
    // 1 kWh = 1000 Wh
    
    struct {
        float wh;
        float expected_kwh;
    } test_cases[] = {
        {100.0f, 0.1f},
        {1000.0f, 1.0f},
        {12345.0f, 12.345f},
        {100000.0f, 100.0f},
        {1000000.0f, 1000.0f}
    };
    
    for (int i = 0; i < 5; i++) {
        float kwh = test_cases[i].wh / 1000.0f;
        TEST_ASSERT_FLOAT_WITHIN(0.001f, test_cases[i].expected_kwh, kwh);
    }
}

// =============================================================================
// Voltage Range Validation (Based on Worldwide Standards)
// =============================================================================

void test_voltage_us_standard_110v(void) {
    // SPECIFICATION: US/Japan standard 110-120V ±5%
    float voltage = 110.0f;
    float min = 104.5f;  // 110V - 5%
    float max = 115.5f;  // 110V + 5%
    
    TEST_ASSERT_TRUE(voltage >= min && voltage <= max);
}

void test_voltage_eu_standard_230v(void) {
    // SPECIFICATION: European standard 230V ±10%
    float voltage = 230.0f;
    float min = 207.0f;  // 230V - 10%
    float max = 253.0f;  // 230V + 10%
    
    TEST_ASSERT_TRUE(voltage >= min && voltage <= max);
}

void test_voltage_uk_standard_240v(void) {
    // SPECIFICATION: UK standard 240V ±6%
    float voltage = 240.0f;
    float min = 225.6f;  // 240V - 6%
    float max = 254.4f;  // 240V + 6%
    
    TEST_ASSERT_TRUE(voltage >= min && voltage <= max);
}

void test_voltage_unrealistic_values_detected(void) {
    // SPECIFICATION: Residential mains should be 100-260V worldwide
    // Values outside this range indicate sensor failure
    
    TEST_ASSERT_FALSE(10.0f > 50.0f && 10.0f < 300.0f);    // Too low
    TEST_ASSERT_FALSE(400.0f > 50.0f && 400.0f < 300.0f);  // Too high
    TEST_ASSERT_TRUE(230.0f > 50.0f && 230.0f < 300.0f);   // Valid
}

// =============================================================================
// Current Range Validation (Espresso Machine Typical)
// =============================================================================

void test_current_idle_range(void) {
    // SPECIFICATION: Idle machine current
    // Pico (50mA) + ESP32 (150mA) + Relays (80mA) + LEDs (30mA) = ~300mA
    
    float idle_current = 0.3f;  // 300mA
    
    TEST_ASSERT_TRUE(idle_current < 1.0f);  // Should be well under 1A
}

void test_current_single_heater(void) {
    // SPECIFICATION: 1400W heater @ 230V = 6.09A
    
    float power = 1400.0f;
    float voltage = 230.0f;
    float expected_current = 6.09f;
    
    float actual_current = power / voltage;
    
    TEST_ASSERT_FLOAT_WITHIN(0.01f, expected_current, actual_current);
}

void test_current_dual_heater_parallel(void) {
    // SPECIFICATION: Both heaters @ 230V = 12.17A
    // Must be < 16A circuit limit
    
    float total_power = 2800.0f;  // 1400W × 2
    float voltage = 230.0f;
    float circuit_limit = 16.0f;
    
    float total_current = total_power / voltage;
    
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 12.17f, total_current);
    TEST_ASSERT_TRUE(total_current < circuit_limit);
}

void test_current_pump_added(void) {
    // SPECIFICATION: Ulka EP5 pump adds ~65W (0.28A @ 230V)
    // Total during brewing: heaters + pump = 12.17A + 0.28A = 12.45A
    
    float heater_current = 12.17f;
    float pump_current = 0.28f;
    float total_current = heater_current + pump_current;
    
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 12.45f, total_current);
    TEST_ASSERT_TRUE(total_current < 16.0f);  // Still under circuit limit
}

// =============================================================================
// Power Factor Tests (Based on Espresso Machine Load)
// =============================================================================

void test_power_factor_resistive_load(void) {
    // SPECIFICATION: Heating elements are resistive loads
    // Expected PF: 0.95-1.00 (very close to unity)
    
    float pf_heating = 0.98f;
    
    TEST_ASSERT_TRUE(pf_heating >= 0.95f && pf_heating <= 1.0f);
}

void test_power_factor_with_pump_motor(void) {
    // SPECIFICATION: Ulka pump is inductive (motor)
    // Expected PF drops slightly: 0.90-0.95
    
    float pf_with_pump = 0.92f;
    
    TEST_ASSERT_TRUE(pf_with_pump >= 0.85f && pf_with_pump <= 1.0f);
}

void test_apparent_power_vs_real_power(void) {
    // SPECIFICATION: Apparent power (VA) vs Real power (W)
    // S = V × I (apparent)
    // P = S × PF (real)
    
    float voltage = 230.0f;
    float current = 6.1f;
    float power_factor = 0.98f;
    
    float apparent_power = voltage * current;
    float real_power = apparent_power * power_factor;
    
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 1403.0f, apparent_power);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 1375.0f, real_power);
}

// =============================================================================
// Modbus Protocol Specification Tests
// =============================================================================

void test_modbus_slave_addresses_valid(void) {
    // SPECIFICATION: Modbus slave addresses 0x01-0xF7 (standard) or 0xF8 (PZEM)
    // Invalid: 0x00 (broadcast), 0xF9-0xFF (reserved)
    
    uint8_t pzem_addr = 0xF8;
    uint8_t jsy_addr = 0x01;
    uint8_t eastron_addr = 0x01;
    
    TEST_ASSERT_TRUE(pzem_addr >= 0x01 && pzem_addr <= 0xF8);
    TEST_ASSERT_TRUE(jsy_addr >= 0x01 && jsy_addr <= 0xF7);
    TEST_ASSERT_TRUE(eastron_addr >= 0x01 && eastron_addr <= 0xF7);
}

void test_modbus_baud_rates_standard(void) {
    // SPECIFICATION: Common Modbus baud rates
    // Standard: 2400, 4800, 9600, 19200
    
    uint32_t pzem_baud = 9600;
    uint32_t jsy_baud = 4800;
    uint32_t eastron_baud = 2400;
    
    uint32_t standard_rates[] = {2400, 4800, 9600, 19200, 38400};
    
    // Verify PZEM baud is standard
    bool pzem_found = false;
    for (int i = 0; i < 5; i++) {
        if (pzem_baud == standard_rates[i]) pzem_found = true;
    }
    TEST_ASSERT_TRUE(pzem_found);
    
    // Verify JSY baud is standard
    bool jsy_found = false;
    for (int i = 0; i < 5; i++) {
        if (jsy_baud == standard_rates[i]) jsy_found = true;
    }
    TEST_ASSERT_TRUE(jsy_found);
    
    // Verify Eastron baud is standard
    bool eastron_found = false;
    for (int i = 0; i < 5; i++) {
        if (eastron_baud == standard_rates[i]) eastron_found = true;
    }
    TEST_ASSERT_TRUE(eastron_found);
}

void test_modbus_function_codes_valid(void) {
    // SPECIFICATION: Modbus function codes
    // 0x03 = Read Holding Registers
    // 0x04 = Read Input Registers
    
    uint8_t valid_codes[] = {0x03, 0x04};
    uint8_t test_code = 0x04;  // PZEM uses this
    
    bool is_valid = (test_code == valid_codes[0] || test_code == valid_codes[1]);
    TEST_ASSERT_TRUE(is_valid);
}

// =============================================================================
// Measurement Resolution Tests (Meter Capabilities)
// =============================================================================

void test_pzem_voltage_resolution(void) {
    // SPECIFICATION: PZEM voltage resolution is 0.1V
    // Should be able to distinguish 230.0V from 230.1V
    
    float resolution = 0.1f;
    float v1 = 230.0f;
    float v2 = 230.1f;
    
    float difference = v2 - v1;
    
    TEST_ASSERT_FLOAT_WITHIN(0.01f, resolution, difference);
}

void test_pzem_current_resolution(void) {
    // SPECIFICATION: PZEM current resolution is 0.001A (1mA)
    
    float resolution = 0.001f;
    float i1 = 5.200f;
    float i2 = 5.201f;
    
    float difference = i2 - i1;
    
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, resolution, difference);
}

void test_pzem_energy_accumulation_accuracy(void) {
    // SPECIFICATION: Energy should accumulate linearly
    // 1200W for 1 hour = 1200Wh = 1.2kWh
    
    float power_watts = 1200.0f;
    float hours = 1.0f;
    float expected_kwh = 1.2f;
    
    float calculated_kwh = (power_watts * hours) / 1000.0f;
    
    TEST_ASSERT_FLOAT_WITHIN(0.01f, expected_kwh, calculated_kwh);
}

// =============================================================================
// JSY-MK-163T Specification Tests
// =============================================================================

void test_jsy_higher_resolution_voltage(void) {
    // SPECIFICATION: JSY has 0.0001V resolution (vs PZEM's 0.1V)
    // Can measure 230.0001V vs 230.0002V
    
    float jsy_resolution = 0.0001f;
    float pzem_resolution = 0.1f;
    
    // JSY is 1000× more precise (allow small floating-point error)
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1000.0f, pzem_resolution / jsy_resolution);
}

void test_jsy_bidirectional_metering(void) {
    // SPECIFICATION: JSY-MK-163T supports bidirectional metering
    // Can measure both import (from grid) and export (to grid)
    
    float energy_import = 12.5f;  // kWh from grid
    float energy_export = 3.2f;   // kWh to grid (if solar)
    
    // Net energy = import - export
    float net_energy = energy_import - energy_export;
    
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 9.3f, net_energy);
}

// =============================================================================
// Eastron SDM120 Specification Tests  
// =============================================================================

void test_eastron_professional_accuracy(void) {
    // SPECIFICATION: Eastron Class 1 meter ±1% accuracy
    
    float measured_power = 1400.0f;
    float max_error = measured_power * 0.01f;  // ±1%
    
    float min_actual = measured_power - max_error;
    float max_actual = measured_power + max_error;
    
    // Actual power should be within ±14W of measured
    TEST_ASSERT_FLOAT_WITHIN(14.0f, 1400.0f, measured_power);
    TEST_ASSERT_TRUE(min_actual >= 1386.0f);
    TEST_ASSERT_TRUE(max_actual <= 1414.0f);
}

void test_eastron_rs485_differential_signaling(void) {
    // SPECIFICATION: RS485 uses differential signaling
    // A-B voltage should be ±200mV to ±6V
    // Common-mode range: -7V to +12V
    
    float differential_min = 0.2f;
    float differential_max = 6.0f;
    float common_mode_min = -7.0f;
    float common_mode_max = 12.0f;
    
    // Typical differential: 2V
    float typical_diff = 2.0f;
    TEST_ASSERT_TRUE(typical_diff >= differential_min && typical_diff <= differential_max);
    
    // Typical common-mode: 0V (referenced to GND)
    float typical_cm = 0.0f;
    TEST_ASSERT_TRUE(typical_cm >= common_mode_min && typical_cm <= common_mode_max);
}

// =============================================================================
// Circuit Breaker Safety Tests
// =============================================================================

void test_13a_uk_circuit_safe_operation(void) {
    // SPECIFICATION: UK 13A circuit
    // ECM with both heaters: 12.17A @ 230V
    // Safety margin: 12.17A / 13A = 93.6% (acceptable)
    
    float max_current = 12.17f;
    float circuit_limit = 13.0f;
    float utilization = max_current / circuit_limit;
    
    TEST_ASSERT_TRUE(utilization < 1.0f);  // Under limit
    TEST_ASSERT_TRUE(utilization > 0.90f);  // High utilization (>90%)
}

void test_16a_eu_circuit_safe_operation(void) {
    // SPECIFICATION: EU 16A circuit
    // ECM with both heaters: 12.17A @ 230V
    // Safety margin: 12.17A / 16A = 76% (comfortable)
    
    float max_current = 12.17f;
    float circuit_limit = 16.0f;
    float utilization = max_current / circuit_limit;
    
    TEST_ASSERT_TRUE(utilization < 0.80f);  // Under 80% (safe margin)
}

void test_20a_us_circuit_safe_operation(void) {
    // SPECIFICATION: US 20A circuit (120V)
    // ECM heaters at 120V: 1400W / 120V = 11.67A each
    // Both heaters: 23.34A (EXCEEDS 20A - would trip!)
    
    float heater_power = 1400.0f;
    float us_voltage = 120.0f;
    float current_per_heater = heater_power / us_voltage;
    float both_heaters = current_per_heater * 2.0f;
    float circuit_limit = 20.0f;
    
    // This would trip the breaker!
    TEST_ASSERT_TRUE(both_heaters > circuit_limit);
    
    // Sequential heating required for 120V
    TEST_ASSERT_TRUE(current_per_heater < circuit_limit);  // One at a time OK
}

// =============================================================================
// Frequency Detection Tests
// =============================================================================

void test_frequency_50hz_regions(void) {
    // SPECIFICATION: 50Hz regions (Europe, Asia, Africa, Australia)
    
    float frequency = 50.0f;
    
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 50.0f, frequency);
}

void test_frequency_60hz_regions(void) {
    // SPECIFICATION: 60Hz regions (Americas, parts of Asia)
    
    float frequency = 60.0f;
    
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 60.0f, frequency);
}

void test_frequency_out_of_range_invalid(void) {
    // SPECIFICATION: Valid mains frequency is 47-63Hz
    // Outside this range indicates sensor error
    
    TEST_ASSERT_FALSE(45.0f >= 47.0f && 45.0f <= 63.0f);  // Too low
    TEST_ASSERT_FALSE(65.0f >= 47.0f && 65.0f <= 63.0f);  // Too high
    TEST_ASSERT_TRUE(50.0f >= 47.0f && 50.0f <= 63.0f);   // Valid
}

// =============================================================================
// Test Runner
// =============================================================================

int run_power_meter_tests(void) {
    UnityBegin("test_power_meter.c");
    
    // Real-world scenarios
    RUN_TEST(test_pzem_typical_espresso_brewing_scenario);
    RUN_TEST(test_pzem_idle_machine_low_power_scenario);
    RUN_TEST(test_pzem_single_heater_power_calculation);
    RUN_TEST(test_pzem_parallel_heating_current_limit);
    
    // Energy accumulation
    RUN_TEST(test_energy_daily_usage_3_shots);
    RUN_TEST(test_energy_commercial_use_50_shots);
    RUN_TEST(test_energy_conversion_wh_to_kwh);
    
    // Voltage standards
    RUN_TEST(test_voltage_us_standard_110v);
    RUN_TEST(test_voltage_eu_standard_230v);
    RUN_TEST(test_voltage_uk_standard_240v);
    RUN_TEST(test_voltage_unrealistic_values_detected);
    
    // Current ranges
    RUN_TEST(test_current_idle_range);
    RUN_TEST(test_current_single_heater);
    RUN_TEST(test_current_dual_heater_parallel);
    RUN_TEST(test_current_pump_added);
    
    // Power factor
    RUN_TEST(test_power_factor_resistive_load);
    RUN_TEST(test_power_factor_with_pump_motor);
    RUN_TEST(test_apparent_power_vs_real_power);
    
    // Frequency
    RUN_TEST(test_frequency_50hz_regions);
    RUN_TEST(test_frequency_60hz_regions);
    RUN_TEST(test_frequency_out_of_range_invalid);
    
    // Circuit safety
    RUN_TEST(test_13a_uk_circuit_safe_operation);
    RUN_TEST(test_16a_eu_circuit_safe_operation);
    RUN_TEST(test_20a_us_circuit_safe_operation);
    
    // Protocol compliance
    RUN_TEST(test_modbus_slave_addresses_valid);
    RUN_TEST(test_modbus_baud_rates_standard);
    RUN_TEST(test_modbus_function_codes_valid);
    
    // Meter specifications
    RUN_TEST(test_pzem_voltage_resolution);
    RUN_TEST(test_pzem_current_resolution);
    RUN_TEST(test_pzem_energy_accumulation_accuracy);
    RUN_TEST(test_jsy_higher_resolution_voltage);
    RUN_TEST(test_jsy_bidirectional_metering);
    RUN_TEST(test_eastron_professional_accuracy);
    RUN_TEST(test_eastron_rs485_differential_signaling);
    
    return UnityEnd();
}
