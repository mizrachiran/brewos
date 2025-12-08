/**
 * Unit Tests - Pre-Infusion
 * 
 * Tests for:
 * - Pre-infusion configuration (enable/disable, timing)
 * - Pre-infusion state transitions during brew cycle
 * - Timing validation
 * - Edge cases and boundary conditions
 */

#include "unity/unity.h"
#include "mocks/mock_hardware.h"
#include <stdbool.h>
#include <stdint.h>

// =============================================================================
// Pre-Infusion Constants (from state.c)
// =============================================================================

#define PREINFUSION_DEFAULT_ON_MS     3000   // Default pre-infusion pump on time
#define PREINFUSION_DEFAULT_PAUSE_MS  5000   // Default pre-infusion pause time
#define PREINFUSION_MAX_ON_MS        10000   // Maximum pump on time
#define PREINFUSION_MAX_PAUSE_MS     30000   // Maximum pause time

// Brew phases (from state.c)
typedef enum {
    BREW_PHASE_NONE = 0,
    BREW_PHASE_PREINFUSION,
    BREW_PHASE_BREWING,
    BREW_PHASE_POST_BREW
} brew_phase_t;

// =============================================================================
// Pre-Infusion State (simulated)
// =============================================================================

typedef struct {
    bool enabled;
    uint16_t on_ms;
    uint16_t pause_ms;
} preinfusion_config_t;

typedef struct {
    preinfusion_config_t config;
    brew_phase_t brew_phase;
    uint32_t brew_start_time;
    bool pump_on;
    bool solenoid_on;
} preinfusion_state_t;

static preinfusion_state_t g_state;

// =============================================================================
// Pre-Infusion Functions (simulated from state.c)
// =============================================================================

static void preinfusion_init_test(void) {
    g_state.config.enabled = false;
    g_state.config.on_ms = PREINFUSION_DEFAULT_ON_MS;
    g_state.config.pause_ms = PREINFUSION_DEFAULT_PAUSE_MS;
    g_state.brew_phase = BREW_PHASE_NONE;
    g_state.brew_start_time = 0;
    g_state.pump_on = false;
    g_state.solenoid_on = false;
}

static void state_set_preinfusion(bool enabled, uint16_t on_ms, uint16_t pause_ms) {
    g_state.config.enabled = enabled;
    g_state.config.on_ms = on_ms;
    g_state.config.pause_ms = pause_ms;
}

static void state_get_preinfusion(bool* enabled, uint16_t* on_ms, uint16_t* pause_ms) {
    if (enabled) *enabled = g_state.config.enabled;
    if (on_ms) *on_ms = g_state.config.on_ms;
    if (pause_ms) *pause_ms = g_state.config.pause_ms;
}

// Simulate brew start
static void start_brew(uint32_t current_time) {
    g_state.brew_start_time = current_time;
    
    if (g_state.config.enabled) {
        // Pre-infusion: pump on, solenoid on
        g_state.brew_phase = BREW_PHASE_PREINFUSION;
        g_state.pump_on = true;
        g_state.solenoid_on = true;
    } else {
        // No pre-infusion: full pressure immediately
        g_state.brew_phase = BREW_PHASE_BREWING;
        g_state.pump_on = true;
        g_state.solenoid_on = true;
    }
}

// Simulate brew cycle update (called periodically)
static void update_brew(uint32_t current_time) {
    if (g_state.brew_phase == BREW_PHASE_NONE) return;
    
    if (g_state.brew_phase == BREW_PHASE_PREINFUSION) {
        uint32_t elapsed = current_time - g_state.brew_start_time;
        
        if (elapsed >= g_state.config.on_ms) {
            // Pre-infusion pause: turn off pump, keep solenoid on
            g_state.pump_on = false;
            
            if (elapsed >= (g_state.config.on_ms + g_state.config.pause_ms)) {
                // Pre-infusion complete, start full pressure brew
                g_state.brew_phase = BREW_PHASE_BREWING;
                g_state.pump_on = true;
            }
        }
    }
}

// Simulate brew stop
static void stop_brew(void) {
    g_state.brew_phase = BREW_PHASE_POST_BREW;
    g_state.pump_on = false;
    // Solenoid stays on briefly for post-brew
}

// Helper to initialize state before each test
static void test_preinfusion_init(void) {
    preinfusion_init_test();
}

// =============================================================================
// Configuration Tests
// =============================================================================

void test_preinfusion_init_defaults(void) {
    test_preinfusion_init();
    
    bool enabled;
    uint16_t on_ms, pause_ms;
    state_get_preinfusion(&enabled, &on_ms, &pause_ms);
    
    TEST_ASSERT_FALSE(enabled);
    TEST_ASSERT_EQUAL_UINT16(PREINFUSION_DEFAULT_ON_MS, on_ms);
    TEST_ASSERT_EQUAL_UINT16(PREINFUSION_DEFAULT_PAUSE_MS, pause_ms);
}

void test_preinfusion_enable(void) {
    test_preinfusion_init();
    
    state_set_preinfusion(true, 3000, 5000);
    
    bool enabled;
    uint16_t on_ms, pause_ms;
    state_get_preinfusion(&enabled, &on_ms, &pause_ms);
    
    TEST_ASSERT_TRUE(enabled);
    TEST_ASSERT_EQUAL_UINT16(3000, on_ms);
    TEST_ASSERT_EQUAL_UINT16(5000, pause_ms);
}

void test_preinfusion_disable(void) {
    test_preinfusion_init();
    
    // Enable first
    state_set_preinfusion(true, 3000, 5000);
    
    // Then disable
    state_set_preinfusion(false, 3000, 5000);
    
    bool enabled;
    state_get_preinfusion(&enabled, NULL, NULL);
    
    TEST_ASSERT_FALSE(enabled);
}

void test_preinfusion_set_custom_timing(void) {
    test_preinfusion_init();
    
    state_set_preinfusion(true, 2000, 8000);
    
    bool enabled;
    uint16_t on_ms, pause_ms;
    state_get_preinfusion(&enabled, &on_ms, &pause_ms);
    
    TEST_ASSERT_TRUE(enabled);
    TEST_ASSERT_EQUAL_UINT16(2000, on_ms);
    TEST_ASSERT_EQUAL_UINT16(8000, pause_ms);
}

void test_preinfusion_minimum_timing(void) {
    test_preinfusion_init();
    
    // Set minimum values (500ms on, 0ms pause)
    state_set_preinfusion(true, 500, 0);
    
    uint16_t on_ms, pause_ms;
    state_get_preinfusion(NULL, &on_ms, &pause_ms);
    
    TEST_ASSERT_EQUAL_UINT16(500, on_ms);
    TEST_ASSERT_EQUAL_UINT16(0, pause_ms);
}

void test_preinfusion_maximum_timing(void) {
    test_preinfusion_init();
    
    // Set maximum values
    state_set_preinfusion(true, PREINFUSION_MAX_ON_MS, PREINFUSION_MAX_PAUSE_MS);
    
    uint16_t on_ms, pause_ms;
    state_get_preinfusion(NULL, &on_ms, &pause_ms);
    
    TEST_ASSERT_EQUAL_UINT16(PREINFUSION_MAX_ON_MS, on_ms);
    TEST_ASSERT_EQUAL_UINT16(PREINFUSION_MAX_PAUSE_MS, pause_ms);
}

void test_preinfusion_zero_on_time(void) {
    test_preinfusion_init();
    
    // Zero on_time effectively disables pre-infusion wetness
    state_set_preinfusion(true, 0, 5000);
    
    uint16_t on_ms;
    state_get_preinfusion(NULL, &on_ms, NULL);
    
    TEST_ASSERT_EQUAL_UINT16(0, on_ms);
}

void test_preinfusion_zero_pause_time(void) {
    test_preinfusion_init();
    
    // Zero pause = no soak, goes straight to full pressure after wetting
    state_set_preinfusion(true, 3000, 0);
    
    uint16_t pause_ms;
    state_get_preinfusion(NULL, NULL, &pause_ms);
    
    TEST_ASSERT_EQUAL_UINT16(0, pause_ms);
}

// =============================================================================
// Brew Cycle State Transition Tests
// =============================================================================

void test_preinfusion_brew_start_with_preinfusion(void) {
    test_preinfusion_init();
    state_set_preinfusion(true, 3000, 5000);
    
    start_brew(1000);  // Start at t=1000ms
    
    TEST_ASSERT_EQUAL(BREW_PHASE_PREINFUSION, g_state.brew_phase);
    TEST_ASSERT_TRUE(g_state.pump_on);
    TEST_ASSERT_TRUE(g_state.solenoid_on);
}

void test_preinfusion_brew_start_without_preinfusion(void) {
    test_preinfusion_init();
    state_set_preinfusion(false, 3000, 5000);
    
    start_brew(1000);  // Start at t=1000ms
    
    TEST_ASSERT_EQUAL(BREW_PHASE_BREWING, g_state.brew_phase);
    TEST_ASSERT_TRUE(g_state.pump_on);
    TEST_ASSERT_TRUE(g_state.solenoid_on);
}

void test_preinfusion_pump_off_during_pause(void) {
    test_preinfusion_init();
    state_set_preinfusion(true, 3000, 5000);  // 3s on, 5s pause
    
    start_brew(0);
    TEST_ASSERT_TRUE(g_state.pump_on);  // Initially on
    
    // Update at 2999ms - still in pre-infusion wetting
    update_brew(2999);
    TEST_ASSERT_TRUE(g_state.pump_on);
    TEST_ASSERT_EQUAL(BREW_PHASE_PREINFUSION, g_state.brew_phase);
    
    // Update at 3000ms - should enter pause (pump off)
    update_brew(3000);
    TEST_ASSERT_FALSE(g_state.pump_on);
    TEST_ASSERT_EQUAL(BREW_PHASE_PREINFUSION, g_state.brew_phase);
    TEST_ASSERT_TRUE(g_state.solenoid_on);  // Solenoid stays on
}

void test_preinfusion_transition_to_full_brew(void) {
    test_preinfusion_init();
    state_set_preinfusion(true, 3000, 5000);  // 3s on, 5s pause = 8s total
    
    start_brew(0);
    
    // Still in pre-infusion pause at 7999ms
    update_brew(7999);
    TEST_ASSERT_EQUAL(BREW_PHASE_PREINFUSION, g_state.brew_phase);
    
    // Should transition to full brew at 8000ms
    update_brew(8000);
    TEST_ASSERT_EQUAL(BREW_PHASE_BREWING, g_state.brew_phase);
    TEST_ASSERT_TRUE(g_state.pump_on);  // Pump back on
    TEST_ASSERT_TRUE(g_state.solenoid_on);
}

void test_preinfusion_full_cycle_timing(void) {
    test_preinfusion_init();
    state_set_preinfusion(true, 2000, 3000);  // 2s on, 3s pause = 5s total
    
    // T=0: Start brew
    start_brew(0);
    TEST_ASSERT_EQUAL(BREW_PHASE_PREINFUSION, g_state.brew_phase);
    TEST_ASSERT_TRUE(g_state.pump_on);
    
    // T=1000ms: Still wetting
    update_brew(1000);
    TEST_ASSERT_TRUE(g_state.pump_on);
    
    // T=2000ms: Pause begins
    update_brew(2000);
    TEST_ASSERT_FALSE(g_state.pump_on);
    TEST_ASSERT_EQUAL(BREW_PHASE_PREINFUSION, g_state.brew_phase);
    
    // T=4000ms: Still in pause
    update_brew(4000);
    TEST_ASSERT_FALSE(g_state.pump_on);
    TEST_ASSERT_EQUAL(BREW_PHASE_PREINFUSION, g_state.brew_phase);
    
    // T=5000ms: Full brew starts
    update_brew(5000);
    TEST_ASSERT_TRUE(g_state.pump_on);
    TEST_ASSERT_EQUAL(BREW_PHASE_BREWING, g_state.brew_phase);
}

void test_preinfusion_immediate_full_brew_when_disabled(void) {
    test_preinfusion_init();
    state_set_preinfusion(false, 3000, 5000);
    
    start_brew(0);
    
    // Should be in full brew immediately
    TEST_ASSERT_EQUAL(BREW_PHASE_BREWING, g_state.brew_phase);
    TEST_ASSERT_TRUE(g_state.pump_on);
    
    // Update shouldn't change phase
    update_brew(3000);
    TEST_ASSERT_EQUAL(BREW_PHASE_BREWING, g_state.brew_phase);
    TEST_ASSERT_TRUE(g_state.pump_on);
}

void test_preinfusion_zero_pause_direct_to_brew(void) {
    test_preinfusion_init();
    state_set_preinfusion(true, 2000, 0);  // 2s on, 0s pause
    
    start_brew(0);
    TEST_ASSERT_EQUAL(BREW_PHASE_PREINFUSION, g_state.brew_phase);
    
    // At 2000ms, should go directly to full brew (no pause)
    update_brew(2000);
    TEST_ASSERT_EQUAL(BREW_PHASE_BREWING, g_state.brew_phase);
    TEST_ASSERT_TRUE(g_state.pump_on);
}

// =============================================================================
// Getter with NULL Parameters Tests
// =============================================================================

void test_preinfusion_get_with_null_enabled(void) {
    test_preinfusion_init();
    state_set_preinfusion(true, 3000, 5000);
    
    uint16_t on_ms, pause_ms;
    state_get_preinfusion(NULL, &on_ms, &pause_ms);
    
    TEST_ASSERT_EQUAL_UINT16(3000, on_ms);
    TEST_ASSERT_EQUAL_UINT16(5000, pause_ms);
}

void test_preinfusion_get_with_null_on_ms(void) {
    test_preinfusion_init();
    state_set_preinfusion(true, 3000, 5000);
    
    bool enabled;
    uint16_t pause_ms;
    state_get_preinfusion(&enabled, NULL, &pause_ms);
    
    TEST_ASSERT_TRUE(enabled);
    TEST_ASSERT_EQUAL_UINT16(5000, pause_ms);
}

void test_preinfusion_get_with_null_pause_ms(void) {
    test_preinfusion_init();
    state_set_preinfusion(true, 3000, 5000);
    
    bool enabled;
    uint16_t on_ms;
    state_get_preinfusion(&enabled, &on_ms, NULL);
    
    TEST_ASSERT_TRUE(enabled);
    TEST_ASSERT_EQUAL_UINT16(3000, on_ms);
}

void test_preinfusion_get_all_null(void) {
    test_preinfusion_init();
    state_set_preinfusion(true, 3000, 5000);
    
    // Should not crash
    state_get_preinfusion(NULL, NULL, NULL);
    
    // Verify state wasn't changed
    bool enabled;
    uint16_t on_ms, pause_ms;
    state_get_preinfusion(&enabled, &on_ms, &pause_ms);
    
    TEST_ASSERT_TRUE(enabled);
    TEST_ASSERT_EQUAL_UINT16(3000, on_ms);
    TEST_ASSERT_EQUAL_UINT16(5000, pause_ms);
}

// =============================================================================
// Edge Cases
// =============================================================================

void test_preinfusion_reconfigure_during_idle(void) {
    test_preinfusion_init();
    
    // Configure once
    state_set_preinfusion(true, 3000, 5000);
    
    // Reconfigure
    state_set_preinfusion(true, 4000, 6000);
    
    bool enabled;
    uint16_t on_ms, pause_ms;
    state_get_preinfusion(&enabled, &on_ms, &pause_ms);
    
    TEST_ASSERT_TRUE(enabled);
    TEST_ASSERT_EQUAL_UINT16(4000, on_ms);
    TEST_ASSERT_EQUAL_UINT16(6000, pause_ms);
}

void test_preinfusion_stop_during_preinfusion(void) {
    test_preinfusion_init();
    state_set_preinfusion(true, 3000, 5000);
    
    start_brew(0);
    update_brew(1000);  // Mid pre-infusion
    TEST_ASSERT_EQUAL(BREW_PHASE_PREINFUSION, g_state.brew_phase);
    
    stop_brew();
    
    TEST_ASSERT_EQUAL(BREW_PHASE_POST_BREW, g_state.brew_phase);
    TEST_ASSERT_FALSE(g_state.pump_on);
}

void test_preinfusion_stop_during_pause(void) {
    test_preinfusion_init();
    state_set_preinfusion(true, 3000, 5000);
    
    start_brew(0);
    update_brew(4000);  // During pause phase
    TEST_ASSERT_EQUAL(BREW_PHASE_PREINFUSION, g_state.brew_phase);
    TEST_ASSERT_FALSE(g_state.pump_on);
    
    stop_brew();
    
    TEST_ASSERT_EQUAL(BREW_PHASE_POST_BREW, g_state.brew_phase);
}

void test_preinfusion_large_time_values(void) {
    test_preinfusion_init();
    
    // Max uint16 values
    state_set_preinfusion(true, 65535, 65535);
    
    uint16_t on_ms, pause_ms;
    state_get_preinfusion(NULL, &on_ms, &pause_ms);
    
    TEST_ASSERT_EQUAL_UINT16(65535, on_ms);
    TEST_ASSERT_EQUAL_UINT16(65535, pause_ms);
}

void test_preinfusion_typical_espresso_settings(void) {
    test_preinfusion_init();
    
    // Typical settings: 3s wetting, 5s soak
    state_set_preinfusion(true, 3000, 5000);
    
    start_brew(0);
    
    // Verify timing matches espresso best practices
    // 0-3s: Wetting (pump on)
    update_brew(1500);
    TEST_ASSERT_TRUE(g_state.pump_on);
    TEST_ASSERT_EQUAL(BREW_PHASE_PREINFUSION, g_state.brew_phase);
    
    // 3-8s: Soak (pump off)
    update_brew(5000);
    TEST_ASSERT_FALSE(g_state.pump_on);
    TEST_ASSERT_EQUAL(BREW_PHASE_PREINFUSION, g_state.brew_phase);
    
    // 8s+: Full extraction
    update_brew(8000);
    TEST_ASSERT_TRUE(g_state.pump_on);
    TEST_ASSERT_EQUAL(BREW_PHASE_BREWING, g_state.brew_phase);
}

void test_preinfusion_blooming_style_long_pause(void) {
    test_preinfusion_init();
    
    // Blooming style: short wetting, long pause (like pour-over)
    state_set_preinfusion(true, 1000, 15000);  // 1s wet, 15s bloom
    
    start_brew(0);
    
    // Quick wetting
    update_brew(500);
    TEST_ASSERT_TRUE(g_state.pump_on);
    
    // Long bloom
    update_brew(8000);
    TEST_ASSERT_FALSE(g_state.pump_on);
    TEST_ASSERT_EQUAL(BREW_PHASE_PREINFUSION, g_state.brew_phase);
    
    // Still blooming
    update_brew(15000);
    TEST_ASSERT_FALSE(g_state.pump_on);
    
    // Extract at 16s
    update_brew(16000);
    TEST_ASSERT_TRUE(g_state.pump_on);
    TEST_ASSERT_EQUAL(BREW_PHASE_BREWING, g_state.brew_phase);
}

// =============================================================================
// Config Validation Tests (protocol layer)
// =============================================================================

// Simulated validation function (from main.c handler)
static bool validate_preinfusion_config(uint16_t on_ms, uint16_t pause_ms) {
    if (on_ms > PREINFUSION_MAX_ON_MS) return false;
    if (pause_ms > PREINFUSION_MAX_PAUSE_MS) return false;
    return true;
}

void test_preinfusion_config_valid(void) {
    TEST_ASSERT_TRUE(validate_preinfusion_config(3000, 5000));
}

void test_preinfusion_config_valid_minimum(void) {
    TEST_ASSERT_TRUE(validate_preinfusion_config(0, 0));
}

void test_preinfusion_config_valid_maximum(void) {
    TEST_ASSERT_TRUE(validate_preinfusion_config(PREINFUSION_MAX_ON_MS, PREINFUSION_MAX_PAUSE_MS));
}

void test_preinfusion_config_invalid_on_time(void) {
    TEST_ASSERT_FALSE(validate_preinfusion_config(PREINFUSION_MAX_ON_MS + 1, 5000));
}

void test_preinfusion_config_invalid_pause_time(void) {
    TEST_ASSERT_FALSE(validate_preinfusion_config(3000, PREINFUSION_MAX_PAUSE_MS + 1));
}

void test_preinfusion_config_invalid_both(void) {
    TEST_ASSERT_FALSE(validate_preinfusion_config(PREINFUSION_MAX_ON_MS + 1, PREINFUSION_MAX_PAUSE_MS + 1));
}

// =============================================================================
// Test Runner
// =============================================================================

int run_preinfusion_tests(void) {
    UnityBegin("test_preinfusion.c");
    
    // Configuration Tests
    RUN_TEST(test_preinfusion_init_defaults);
    RUN_TEST(test_preinfusion_enable);
    RUN_TEST(test_preinfusion_disable);
    RUN_TEST(test_preinfusion_set_custom_timing);
    RUN_TEST(test_preinfusion_minimum_timing);
    RUN_TEST(test_preinfusion_maximum_timing);
    RUN_TEST(test_preinfusion_zero_on_time);
    RUN_TEST(test_preinfusion_zero_pause_time);
    
    // Brew Cycle State Transition Tests
    RUN_TEST(test_preinfusion_brew_start_with_preinfusion);
    RUN_TEST(test_preinfusion_brew_start_without_preinfusion);
    RUN_TEST(test_preinfusion_pump_off_during_pause);
    RUN_TEST(test_preinfusion_transition_to_full_brew);
    RUN_TEST(test_preinfusion_full_cycle_timing);
    RUN_TEST(test_preinfusion_immediate_full_brew_when_disabled);
    RUN_TEST(test_preinfusion_zero_pause_direct_to_brew);
    
    // Getter with NULL Parameters Tests
    RUN_TEST(test_preinfusion_get_with_null_enabled);
    RUN_TEST(test_preinfusion_get_with_null_on_ms);
    RUN_TEST(test_preinfusion_get_with_null_pause_ms);
    RUN_TEST(test_preinfusion_get_all_null);
    
    // Edge Cases
    RUN_TEST(test_preinfusion_reconfigure_during_idle);
    RUN_TEST(test_preinfusion_stop_during_preinfusion);
    RUN_TEST(test_preinfusion_stop_during_pause);
    RUN_TEST(test_preinfusion_large_time_values);
    RUN_TEST(test_preinfusion_typical_espresso_settings);
    RUN_TEST(test_preinfusion_blooming_style_long_pause);
    
    // Config Validation Tests
    RUN_TEST(test_preinfusion_config_valid);
    RUN_TEST(test_preinfusion_config_valid_minimum);
    RUN_TEST(test_preinfusion_config_valid_maximum);
    RUN_TEST(test_preinfusion_config_invalid_on_time);
    RUN_TEST(test_preinfusion_config_invalid_pause_time);
    RUN_TEST(test_preinfusion_config_invalid_both);
    
    return UnityEnd();
}

