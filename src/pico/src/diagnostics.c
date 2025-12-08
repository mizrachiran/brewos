/**
 * ECM Pico Firmware - Hardware Diagnostics
 * 
 * Self-test and diagnostic functions for validating hardware wiring
 * and component functionality.
 */

#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "hardware/clocks.h"
#include "diagnostics.h"
#include "config.h"
#include "hardware.h"
#include "sensors.h"
#include "pcb_config.h"
#include "machine_config.h"
#include "power_meter.h"
#include "safety.h"
#include "protocol_defs.h"
#include "class_b.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// =============================================================================
// Private State
// =============================================================================

static bool g_running = false;
static uint32_t g_start_time = 0;

// =============================================================================
// Helper Functions
// =============================================================================

/**
 * Initialize a result structure with default values
 */
static void init_result(diag_result_t* result, uint8_t test_id) {
    memset(result, 0, sizeof(diag_result_t));
    result->test_id = test_id;
    result->status = DIAG_STATUS_RUNNING;
    result->raw_value = 0;
    result->expected_min = 0;
    result->expected_max = 0;
    result->message[0] = '\0';
}

/**
 * Set result status and message
 */
static void set_result(diag_result_t* result, uint8_t status, const char* msg) {
    result->status = status;
    strncpy(result->message, msg, sizeof(result->message) - 1);
    result->message[sizeof(result->message) - 1] = '\0';
}

// =============================================================================
// Initialization
// =============================================================================

void diagnostics_init(void) {
    g_running = false;
    DEBUG_PRINT("Diagnostics module initialized\n");
}

bool diagnostics_is_running(void) {
    return g_running;
}

void diagnostics_abort(void) {
    g_running = false;
    DEBUG_PRINT("Diagnostics aborted\n");
}

// =============================================================================
// Run All Tests
// =============================================================================

bool diagnostics_run_all(diag_report_t* report) {
    if (!report) return false;
    
    g_running = true;
    g_start_time = to_ms_since_boot(get_absolute_time());
    
    // Initialize report
    memset(report, 0, sizeof(diag_report_t));
    report->test_count = 0;
    report->pass_count = 0;
    report->fail_count = 0;
    report->warn_count = 0;
    report->skip_count = 0;
    
    // Array of tests to run
    struct {
        uint8_t test_id;
        uint8_t (*test_func)(diag_result_t*);
    } tests[] = {
        { DIAG_TEST_BREW_NTC, diag_test_brew_ntc },
        { DIAG_TEST_STEAM_NTC, diag_test_steam_ntc },
        { DIAG_TEST_PRESSURE, diag_test_pressure },
        { DIAG_TEST_WATER_LEVEL, diag_test_water_level },
        { DIAG_TEST_SSR_BREW, diag_test_ssr_brew },
        { DIAG_TEST_SSR_STEAM, diag_test_ssr_steam },
        { DIAG_TEST_RELAY_PUMP, diag_test_relay_pump },
        { DIAG_TEST_RELAY_SOLENOID, diag_test_relay_solenoid },
        { DIAG_TEST_POWER_METER, diag_test_power_meter },
        { DIAG_TEST_ESP32_COMM, diag_test_esp32_comm },
        { DIAG_TEST_BUZZER, diag_test_buzzer },
        { DIAG_TEST_LED, diag_test_led },
    };
    
    const int num_tests = sizeof(tests) / sizeof(tests[0]);
    
    for (int i = 0; i < num_tests && i < 16 && g_running; i++) {
        diag_result_t* result = &report->results[i];
        tests[i].test_func(result);
        report->test_count++;
        
        switch (result->status) {
            case DIAG_STATUS_PASS:
                report->pass_count++;
                break;
            case DIAG_STATUS_FAIL:
                report->fail_count++;
                break;
            case DIAG_STATUS_WARN:
                report->warn_count++;
                break;
            case DIAG_STATUS_SKIP:
                report->skip_count++;
                break;
        }
        
        // Feed watchdog between tests
        watchdog_update();
    }
    
    report->duration_ms = to_ms_since_boot(get_absolute_time()) - g_start_time;
    g_running = false;
    
    DEBUG_PRINT("Diagnostics complete: %d pass, %d fail, %d warn, %d skip (%.1fs)\n",
                report->pass_count, report->fail_count, report->warn_count,
                report->skip_count, report->duration_ms / 1000.0f);
    
    return report->fail_count == 0;
}

// =============================================================================
// Run Single Test
// =============================================================================

uint8_t diagnostics_run_test(uint8_t test_id, diag_result_t* result) {
    if (!result) return DIAG_STATUS_FAIL;
    
    g_running = true;
    
    switch (test_id) {
        case DIAG_TEST_BREW_NTC:
            diag_test_brew_ntc(result);
            break;
        case DIAG_TEST_STEAM_NTC:
            diag_test_steam_ntc(result);
            break;
        case DIAG_TEST_PRESSURE:
            diag_test_pressure(result);
            break;
        case DIAG_TEST_WATER_LEVEL:
            diag_test_water_level(result);
            break;
        case DIAG_TEST_SSR_BREW:
            diag_test_ssr_brew(result);
            break;
        case DIAG_TEST_SSR_STEAM:
            diag_test_ssr_steam(result);
            break;
        case DIAG_TEST_RELAY_PUMP:
            diag_test_relay_pump(result);
            break;
        case DIAG_TEST_RELAY_SOLENOID:
            diag_test_relay_solenoid(result);
            break;
        case DIAG_TEST_POWER_METER:
            diag_test_power_meter(result);
            break;
        case DIAG_TEST_ESP32_COMM:
            diag_test_esp32_comm(result);
            break;
        case DIAG_TEST_BUZZER:
            diag_test_buzzer(result);
            break;
        case DIAG_TEST_LED:
            diag_test_led(result);
            break;
        // Class B Safety Tests
        case DIAG_TEST_CLASS_B_ALL:
            diag_test_class_b_all(result);
            break;
        case DIAG_TEST_CLASS_B_RAM:
            diag_test_class_b_ram(result);
            break;
        case DIAG_TEST_CLASS_B_FLASH:
            diag_test_class_b_flash(result);
            break;
        case DIAG_TEST_CLASS_B_CPU:
            diag_test_class_b_cpu(result);
            break;
        case DIAG_TEST_CLASS_B_IO:
            diag_test_class_b_io(result);
            break;
        case DIAG_TEST_CLASS_B_CLOCK:
            diag_test_class_b_clock(result);
            break;
        case DIAG_TEST_CLASS_B_STACK:
            diag_test_class_b_stack(result);
            break;
        case DIAG_TEST_CLASS_B_PC:
            diag_test_class_b_pc(result);
            break;
        default:
            init_result(result, test_id);
            set_result(result, DIAG_STATUS_FAIL, "Unknown test");
            break;
    }
    
    g_running = false;
    return result->status;
}

// =============================================================================
// Individual Test Implementations
// =============================================================================

uint8_t diag_test_brew_ntc(diag_result_t* result) {
    init_result(result, DIAG_TEST_BREW_NTC);
    
    // Check if machine has brew NTC
    if (!machine_has_brew_ntc()) {
        set_result(result, DIAG_STATUS_SKIP, "No brew NTC (HX machine)");
        return result->status;
    }
    
    const pcb_config_t* pcb = pcb_config_get();
    if (!pcb || pcb->pins.adc_brew_ntc < 0) {
        set_result(result, DIAG_STATUS_SKIP, "Not configured");
        return result->status;
    }
    
    // Read ADC
    uint8_t adc_channel = pcb->pins.adc_brew_ntc - 26;
    uint16_t adc_value = hw_read_adc(adc_channel);
    result->raw_value = (int16_t)adc_value;
    
    // Expected range for NTC at room temperature (10-40°C)
    // ADC value ~1500-2500 for 10k NTC with 10k pullup at 25°C
    result->expected_min = 500;
    result->expected_max = 3500;
    
    if (adc_value < 100) {
        set_result(result, DIAG_STATUS_FAIL, "Short circuit");
    } else if (adc_value > 4000) {
        set_result(result, DIAG_STATUS_FAIL, "Open circuit");
    } else if (adc_value < result->expected_min || adc_value > result->expected_max) {
        set_result(result, DIAG_STATUS_WARN, "Value out of expected range");
    } else {
        set_result(result, DIAG_STATUS_PASS, "OK");
    }
    
    DEBUG_PRINT("Brew NTC: ADC=%d, status=%d\n", adc_value, result->status);
    return result->status;
}

uint8_t diag_test_steam_ntc(diag_result_t* result) {
    init_result(result, DIAG_TEST_STEAM_NTC);
    
    // Check if machine has steam NTC
    if (!machine_has_steam_ntc()) {
        set_result(result, DIAG_STATUS_SKIP, "No steam NTC (single boiler)");
        return result->status;
    }
    
    const pcb_config_t* pcb = pcb_config_get();
    if (!pcb || pcb->pins.adc_steam_ntc < 0) {
        set_result(result, DIAG_STATUS_SKIP, "Not configured");
        return result->status;
    }
    
    // Read ADC
    uint8_t adc_channel = pcb->pins.adc_steam_ntc - 26;
    uint16_t adc_value = hw_read_adc(adc_channel);
    result->raw_value = (int16_t)adc_value;
    
    result->expected_min = 500;
    result->expected_max = 3500;
    
    if (adc_value < 100) {
        set_result(result, DIAG_STATUS_FAIL, "Short circuit");
    } else if (adc_value > 4000) {
        set_result(result, DIAG_STATUS_FAIL, "Open circuit");
    } else if (adc_value < result->expected_min || adc_value > result->expected_max) {
        set_result(result, DIAG_STATUS_WARN, "Value out of expected range");
    } else {
        set_result(result, DIAG_STATUS_PASS, "OK");
    }
    
    DEBUG_PRINT("Steam NTC: ADC=%d, status=%d\n", adc_value, result->status);
    return result->status;
}

uint8_t diag_test_pressure(diag_result_t* result) {
    init_result(result, DIAG_TEST_PRESSURE);
    
    const pcb_config_t* pcb = pcb_config_get();
    if (!pcb || pcb->pins.adc_pressure < 0) {
        set_result(result, DIAG_STATUS_SKIP, "Not configured");
        return result->status;
    }
    
    // Read ADC voltage
    uint8_t adc_channel = pcb->pins.adc_pressure - 26;
    float voltage = hw_read_adc_voltage(adc_channel);
    result->raw_value = (int16_t)(voltage * 100);  // mV * 10
    
    // Expected: 0.3V - 0.5V at 0 bar (after voltage divider)
    // This corresponds to 0.5V - 0.8V transducer output at rest
    result->expected_min = 25;   // 0.25V
    result->expected_max = 60;   // 0.60V
    
    if (voltage < 0.1f) {
        set_result(result, DIAG_STATUS_FAIL, "No signal (disconnected?)");
    } else if (voltage > 2.0f) {
        set_result(result, DIAG_STATUS_FAIL, "Voltage too high");
    } else if (voltage < 0.2f || voltage > 0.7f) {
        set_result(result, DIAG_STATUS_WARN, "Unexpected resting voltage");
    } else {
        set_result(result, DIAG_STATUS_PASS, "OK");
    }
    
    DEBUG_PRINT("Pressure: %.2fV, status=%d\n", voltage, result->status);
    return result->status;
}

uint8_t diag_test_water_level(diag_result_t* result) {
    init_result(result, DIAG_TEST_WATER_LEVEL);
    
    const pcb_config_t* pcb = pcb_config_get();
    if (!pcb) {
        set_result(result, DIAG_STATUS_SKIP, "PCB not configured");
        return result->status;
    }
    
    bool has_any_sensor = false;
    bool all_ok = true;
    char msg[32] = "";
    
    // Check reservoir sensor
    if (PIN_VALID(pcb->pins.input_reservoir)) {
        has_any_sensor = true;
        bool state = hw_read_gpio(pcb->pins.input_reservoir);
        if (!state) {
            snprintf(msg, sizeof(msg), "Reservoir: empty");
            all_ok = false;
        }
    }
    
    // Check tank level sensor
    if (PIN_VALID(pcb->pins.input_tank_level)) {
        has_any_sensor = true;
        bool state = hw_read_gpio(pcb->pins.input_tank_level);
        if (!state) {
            snprintf(msg, sizeof(msg), "Tank: low");
            all_ok = false;
        }
    }
    
    // Check steam level sensor
    if (PIN_VALID(pcb->pins.input_steam_level)) {
        has_any_sensor = true;
        bool state = hw_read_gpio(pcb->pins.input_steam_level);
        if (!state) {
            snprintf(msg, sizeof(msg), "Steam: low");
            all_ok = false;
        }
    }
    
    if (!has_any_sensor) {
        set_result(result, DIAG_STATUS_SKIP, "No sensors configured");
    } else if (!all_ok) {
        set_result(result, DIAG_STATUS_WARN, msg[0] ? msg : "Level low");
    } else {
        set_result(result, DIAG_STATUS_PASS, "All levels OK");
    }
    
    DEBUG_PRINT("Water level: status=%d, %s\n", result->status, result->message);
    return result->status;
}

uint8_t diag_test_ssr_brew(diag_result_t* result) {
    init_result(result, DIAG_TEST_SSR_BREW);
    
    const pcb_config_t* pcb = pcb_config_get();
    if (!pcb || pcb->pins.ssr_brew < 0) {
        set_result(result, DIAG_STATUS_SKIP, "Not configured");
        return result->status;
    }
    
    // Brief PWM pulse to verify signal path (10% for 100ms)
    // This won't heat anything but verifies the output works
    uint8_t slice_num;
    hw_pwm_init_ssr(pcb->pins.ssr_brew, &slice_num);
    hw_set_pwm_duty(slice_num, 10.0f);
    sleep_ms(100);
    hw_set_pwm_duty(slice_num, 0.0f);
    
    // Note: We can't verify the SSR actually switched without additional feedback
    // This test just verifies the GPIO/PWM is functional
    set_result(result, DIAG_STATUS_PASS, "PWM signal OK");
    
    DEBUG_PRINT("Brew SSR: test pulse sent\n");
    return result->status;
}

uint8_t diag_test_ssr_steam(diag_result_t* result) {
    init_result(result, DIAG_TEST_SSR_STEAM);
    
    const pcb_config_t* pcb = pcb_config_get();
    if (!pcb || pcb->pins.ssr_steam < 0) {
        set_result(result, DIAG_STATUS_SKIP, "Not configured");
        return result->status;
    }
    
    // Brief PWM pulse to verify signal path
    uint8_t slice_num;
    hw_pwm_init_ssr(pcb->pins.ssr_steam, &slice_num);
    hw_set_pwm_duty(slice_num, 10.0f);
    sleep_ms(100);
    hw_set_pwm_duty(slice_num, 0.0f);
    
    set_result(result, DIAG_STATUS_PASS, "PWM signal OK");
    
    DEBUG_PRINT("Steam SSR: test pulse sent\n");
    return result->status;
}

uint8_t diag_test_relay_pump(diag_result_t* result) {
    init_result(result, DIAG_TEST_RELAY_PUMP);
    
    const pcb_config_t* pcb = pcb_config_get();
    if (!pcb || pcb->pins.relay_pump < 0) {
        set_result(result, DIAG_STATUS_SKIP, "Not configured");
        return result->status;
    }
    
    // Brief activation to verify relay driver (50ms click)
    // Warning: This may briefly energize the pump!
    hw_set_gpio(pcb->pins.relay_pump, true);
    sleep_ms(50);
    hw_set_gpio(pcb->pins.relay_pump, false);
    
    set_result(result, DIAG_STATUS_PASS, "Relay activated");
    
    DEBUG_PRINT("Pump relay: test click\n");
    return result->status;
}

uint8_t diag_test_relay_solenoid(diag_result_t* result) {
    init_result(result, DIAG_TEST_RELAY_SOLENOID);
    
    const pcb_config_t* pcb = pcb_config_get();
    if (!pcb || pcb->pins.relay_brew_solenoid < 0) {
        set_result(result, DIAG_STATUS_SKIP, "Not configured");
        return result->status;
    }
    
    // Brief activation to verify relay driver (50ms click)
    hw_set_gpio(pcb->pins.relay_brew_solenoid, true);
    sleep_ms(50);
    hw_set_gpio(pcb->pins.relay_brew_solenoid, false);
    
    set_result(result, DIAG_STATUS_PASS, "Relay activated");
    
    DEBUG_PRINT("Solenoid relay: test click\n");
    return result->status;
}

uint8_t diag_test_power_meter(diag_result_t* result) {
    init_result(result, DIAG_TEST_POWER_METER);
    
    if (!power_meter_is_connected()) {
        set_result(result, DIAG_STATUS_SKIP, "Power meter not configured");
        return result->status;
    }
    
    // Try to read from power meter
    power_meter_reading_t reading;
    if (!power_meter_get_reading(&reading) || !reading.valid) {
        set_result(result, DIAG_STATUS_FAIL, "Read failed");
        return result->status;
    }
    
    result->raw_value = (int16_t)(reading.voltage * 10);
    
    // Check for reasonable voltage (85-265V)
    if (reading.voltage < 85.0f || reading.voltage > 265.0f) {
        set_result(result, DIAG_STATUS_WARN, "Unexpected voltage");
    } else {
        set_result(result, DIAG_STATUS_PASS, "OK");
    }
    
    const char* meter_name = power_meter_get_name();
    DEBUG_PRINT("Power meter (%s): %.1fV, %.2fA\n", meter_name, reading.voltage, reading.current);
    return result->status;
}

uint8_t diag_test_esp32_comm(diag_result_t* result) {
    init_result(result, DIAG_TEST_ESP32_COMM);
    
    // Check if ESP32 is connected (based on recent heartbeat)
    if (safety_esp32_connected()) {
        set_result(result, DIAG_STATUS_PASS, "Connected");
    } else {
        set_result(result, DIAG_STATUS_FAIL, "No heartbeat");
    }
    
    DEBUG_PRINT("ESP32 comm: status=%d\n", result->status);
    return result->status;
}

uint8_t diag_test_buzzer(diag_result_t* result) {
    init_result(result, DIAG_TEST_BUZZER);
    
    const pcb_config_t* pcb = pcb_config_get();
    if (!pcb || pcb->pins.buzzer < 0) {
        set_result(result, DIAG_STATUS_SKIP, "Not configured");
        return result->status;
    }
    
    // Brief chirp (100ms)
    hw_set_gpio(pcb->pins.buzzer, true);
    sleep_ms(100);
    hw_set_gpio(pcb->pins.buzzer, false);
    
    set_result(result, DIAG_STATUS_PASS, "Chirp played");
    
    DEBUG_PRINT("Buzzer: test chirp\n");
    return result->status;
}

uint8_t diag_test_led(diag_result_t* result) {
    init_result(result, DIAG_TEST_LED);
    
    const pcb_config_t* pcb = pcb_config_get();
    if (!pcb || pcb->pins.led_status < 0) {
        set_result(result, DIAG_STATUS_SKIP, "Not configured");
        return result->status;
    }
    
    // Flash LED 3 times
    for (int i = 0; i < 3; i++) {
        hw_set_gpio(pcb->pins.led_status, true);
        sleep_ms(100);
        hw_set_gpio(pcb->pins.led_status, false);
        sleep_ms(100);
    }
    
    // Leave LED in normal state (on)
    hw_set_gpio(pcb->pins.led_status, true);
    
    set_result(result, DIAG_STATUS_PASS, "LED flashed");
    
    DEBUG_PRINT("LED: test flash\n");
    return result->status;
}

// =============================================================================
// Class B Safety Test Implementations (IEC 60730/60335 Annex R)
// =============================================================================

uint8_t diag_test_class_b_all(diag_result_t* result) {
    init_result(result, DIAG_TEST_CLASS_B_ALL);
    
    class_b_result_t class_b_res = class_b_startup_test();
    
    if (class_b_res == CLASS_B_PASS) {
        class_b_status_t status;
        class_b_get_status(&status);
        
        result->raw_value = (int16_t)status.fail_count;
        set_result(result, DIAG_STATUS_PASS, "All Class B tests PASS");
    } else {
        result->raw_value = (int16_t)class_b_res;
        set_result(result, DIAG_STATUS_FAIL, class_b_result_string(class_b_res));
    }
    
    DEBUG_PRINT("Class B All: %s\n", result->message);
    return result->status;
}

uint8_t diag_test_class_b_ram(diag_result_t* result) {
    init_result(result, DIAG_TEST_CLASS_B_RAM);
    
    class_b_result_t class_b_res = class_b_test_ram();
    
    if (class_b_res == CLASS_B_PASS) {
        set_result(result, DIAG_STATUS_PASS, "RAM March C- PASS");
    } else {
        set_result(result, DIAG_STATUS_FAIL, "RAM test failed");
    }
    
    DEBUG_PRINT("Class B RAM: %s\n", result->message);
    return result->status;
}

uint8_t diag_test_class_b_flash(diag_result_t* result) {
    init_result(result, DIAG_TEST_CLASS_B_FLASH);
    
    // For diagnostic test, do full CRC verification
    class_b_status_t status;
    class_b_get_status(&status);
    
    // Force complete Flash CRC calculation
    uint32_t crc;
    bool complete = false;
    while (!complete) {
        class_b_crc32_flash_incremental(&crc, &complete);
        // Note: watchdog_update() is safe to call even if watchdog is not enabled.
        // This ensures the watchdog doesn't timeout during long CRC calculations.
        watchdog_update();
    }
    
    result->raw_value = (int16_t)(crc >> 16);  // High word of CRC
    
    if (crc == status.flash_crc_reference) {
        char msg[32];
        snprintf(msg, sizeof(msg), "CRC OK: 0x%08lX", crc);
        set_result(result, DIAG_STATUS_PASS, msg);
    } else {
        char msg[32];
        snprintf(msg, sizeof(msg), "CRC fail: 0x%08lX", crc);
        set_result(result, DIAG_STATUS_FAIL, msg);
    }
    
    DEBUG_PRINT("Class B Flash: %s\n", result->message);
    return result->status;
}

uint8_t diag_test_class_b_cpu(diag_result_t* result) {
    init_result(result, DIAG_TEST_CLASS_B_CPU);
    
    class_b_result_t class_b_res = class_b_test_cpu_registers();
    
    if (class_b_res == CLASS_B_PASS) {
        set_result(result, DIAG_STATUS_PASS, "CPU registers PASS");
    } else {
        set_result(result, DIAG_STATUS_FAIL, "CPU register test failed");
    }
    
    DEBUG_PRINT("Class B CPU: %s\n", result->message);
    return result->status;
}

uint8_t diag_test_class_b_io(diag_result_t* result) {
    init_result(result, DIAG_TEST_CLASS_B_IO);
    
    class_b_result_t class_b_res = class_b_test_io();
    
    if (class_b_res == CLASS_B_PASS) {
        set_result(result, DIAG_STATUS_PASS, "I/O verification PASS");
    } else {
        set_result(result, DIAG_STATUS_FAIL, "I/O state mismatch");
    }
    
    DEBUG_PRINT("Class B I/O: %s\n", result->message);
    return result->status;
}

uint8_t diag_test_class_b_clock(diag_result_t* result) {
    init_result(result, DIAG_TEST_CLASS_B_CLOCK);
    
    class_b_result_t class_b_res = class_b_test_clock();
    
    // Get actual clock frequency for raw value
    uint32_t sys_clk = clock_get_hz(clk_sys);
    result->raw_value = (int16_t)(sys_clk / 1000000);  // MHz
    result->expected_min = 118;  // 118 MHz (-5%)
    result->expected_max = 131;  // 131 MHz (+5%)
    
    if (class_b_res == CLASS_B_PASS) {
        char msg[32];
        snprintf(msg, sizeof(msg), "Clock OK: %ld MHz", sys_clk / 1000000);
        set_result(result, DIAG_STATUS_PASS, msg);
    } else {
        char msg[32];
        snprintf(msg, sizeof(msg), "Clock error: %ld MHz", sys_clk / 1000000);
        set_result(result, DIAG_STATUS_FAIL, msg);
    }
    
    DEBUG_PRINT("Class B Clock: %s\n", result->message);
    return result->status;
}

uint8_t diag_test_class_b_stack(diag_result_t* result) {
    init_result(result, DIAG_TEST_CLASS_B_STACK);
    
    class_b_result_t class_b_res = class_b_test_stack();
    
    if (class_b_res == CLASS_B_PASS) {
        set_result(result, DIAG_STATUS_PASS, "Stack canaries intact");
    } else {
        set_result(result, DIAG_STATUS_FAIL, "Stack overflow detected");
    }
    
    DEBUG_PRINT("Class B Stack: %s\n", result->message);
    return result->status;
}

uint8_t diag_test_class_b_pc(diag_result_t* result) {
    init_result(result, DIAG_TEST_CLASS_B_PC);
    
    class_b_result_t class_b_res = class_b_test_program_counter();
    
    if (class_b_res == CLASS_B_PASS) {
        set_result(result, DIAG_STATUS_PASS, "PC flow verified");
    } else {
        set_result(result, DIAG_STATUS_FAIL, "PC flow error");
    }
    
    DEBUG_PRINT("Class B PC: %s\n", result->message);
    return result->status;
}

