/**
 * Mock Hardware Layer for Unit Tests
 * 
 * Provides stub implementations for Pico SDK and hardware functions
 * that are not available when running tests on the host machine.
 */

#ifndef MOCK_HARDWARE_H
#define MOCK_HARDWARE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

// =============================================================================
// Pico SDK Type Definitions
// =============================================================================

// Pico SDK uses 'uint' which is not standard C
typedef unsigned int uint;

typedef uint64_t absolute_time_t;

// =============================================================================
// Time Functions (Mock)
// =============================================================================

// Mock time in milliseconds - MUST be initialized by tests before use
// Example: mock_time_ms = 0; before calling functions that rely on timing
extern uint32_t mock_time_ms;

static inline uint32_t to_ms_since_boot(absolute_time_t t) {
    return (uint32_t)t;
}

static inline absolute_time_t get_absolute_time(void) {
    return (absolute_time_t)mock_time_ms;
}

static inline absolute_time_t make_timeout_time_ms(uint32_t ms) {
    return get_absolute_time() + ms;
}

static inline bool time_reached(absolute_time_t t) {
    return get_absolute_time() >= t;
}

// Function declaration for time_us_32 (used by power meter)
uint32_t time_us_32(void);

static inline void sleep_ms(uint32_t ms) {
    // No-op in tests
    (void)ms;
}

static inline void sleep_us(uint32_t us) {
    // No-op in tests
    (void)us;
}

// =============================================================================
// GPIO Mock
// =============================================================================

#define GPIO_OUT 1
#define GPIO_IN  0
#define GPIO_FUNC_UART 2
#define GPIO_FUNC_PWM 4

static inline void gpio_init(uint gpio) { (void)gpio; }
static inline void gpio_set_dir(uint gpio, bool out) { (void)gpio; (void)out; }
static inline void gpio_put(uint gpio, bool value) { (void)gpio; (void)value; }
static inline bool gpio_get(uint gpio) { (void)gpio; return false; }
static inline void gpio_set_function(uint gpio, uint func) { (void)gpio; (void)func; }

// =============================================================================
// ADC Mock
// =============================================================================

static uint16_t mock_adc_values[5] = {0};
static int mock_adc_channel = 0;

static inline void adc_init(void) {}
static inline void adc_gpio_init(uint gpio) { (void)gpio; }
static inline void adc_select_input(uint input) { mock_adc_channel = input; }
static inline uint16_t adc_read(void) { return mock_adc_values[mock_adc_channel]; }

static inline void mock_adc_set_value(int channel, uint16_t value) {
    if (channel >= 0 && channel < 5) {
        mock_adc_values[channel] = value;
    }
}

// =============================================================================
// UART Mock
// =============================================================================

#define uart0 ((void*)0)
#define uart1 ((void*)1)
#define UART_PARITY_NONE 0

typedef void* uart_inst_t;

static inline void uart_init(uart_inst_t* uart, uint baudrate) { (void)uart; (void)baudrate; }
static inline void uart_set_format(uart_inst_t* uart, uint data_bits, uint stop_bits, uint parity) {
    (void)uart; (void)data_bits; (void)stop_bits; (void)parity;
}
static inline void uart_set_hw_flow(uart_inst_t* uart, bool cts, bool rts) {
    (void)uart; (void)cts; (void)rts;
}
static inline void uart_set_fifo_enabled(uart_inst_t* uart, bool enabled) {
    (void)uart; (void)enabled;
}
// UART RX buffer for mocking responses (can be overridden in tests)
extern uint8_t mock_uart_rx_buffer[128];
extern int mock_uart_rx_length;
extern int mock_uart_rx_index;

bool uart_is_readable(uart_inst_t* uart);
char uart_getc(uart_inst_t* uart);

static inline void uart_putc(uart_inst_t* uart, char c) { (void)uart; (void)c; }
static inline void uart_write_blocking(uart_inst_t* uart, const uint8_t* data, size_t len) {
    (void)uart; (void)data; (void)len;
}
static inline void uart_deinit(uart_inst_t* uart) {
    (void)uart;
}

// =============================================================================
// PWM Mock
// =============================================================================

typedef struct {
    uint16_t wrap;
    uint16_t level;
    bool enabled;
} mock_pwm_state_t;

static mock_pwm_state_t mock_pwm_states[8] = {0};

static inline uint pwm_gpio_to_slice_num(uint gpio) { return gpio / 2; }
static inline uint pwm_gpio_to_channel(uint gpio) { return gpio % 2; }
static inline void pwm_set_wrap(uint slice, uint16_t wrap) {
    if (slice < 8) mock_pwm_states[slice].wrap = wrap;
}
static inline void pwm_set_chan_level(uint slice, uint channel, uint16_t level) {
    (void)channel;
    if (slice < 8) mock_pwm_states[slice].level = level;
}
static inline void pwm_set_enabled(uint slice, bool enabled) {
    if (slice < 8) mock_pwm_states[slice].enabled = enabled;
}
static inline void pwm_set_clkdiv(uint slice, float div) { (void)slice; (void)div; }

// =============================================================================
// Flash Mock
// =============================================================================

#define FLASH_SECTOR_SIZE 4096
#define FLASH_PAGE_SIZE 256
#define PICO_FLASH_SIZE_BYTES (2 * 1024 * 1024)

static inline void flash_range_erase(uint32_t offset, size_t count) {
    (void)offset; (void)count;
}
static inline void flash_range_program(uint32_t offset, const uint8_t* data, size_t count) {
    (void)offset; (void)data; (void)count;
}

// =============================================================================
// Watchdog Mock
// =============================================================================

static inline void watchdog_enable(uint32_t delay_ms, bool pause_on_debug) {
    (void)delay_ms; (void)pause_on_debug;
}
static inline void watchdog_update(void) {}
static inline void watchdog_reboot(uint32_t pc, uint32_t sp, uint32_t delay_ms) {
    (void)pc; (void)sp; (void)delay_ms;
}

// =============================================================================
// Multicore Mock
// =============================================================================

static inline void multicore_launch_core1(void (*entry)(void)) { (void)entry; }
static inline void multicore_lockout_victim_init(void) {}
static inline void multicore_lockout_start_blocking(void) {}
static inline void multicore_lockout_end_blocking(void) {}

// =============================================================================
// Interrupt Mock
// =============================================================================

static inline uint32_t save_and_disable_interrupts(void) { return 0; }
static inline void restore_interrupts(uint32_t status) { (void)status; }

// =============================================================================
// Mutex Mock
// =============================================================================

typedef struct {
    bool locked;
} mutex_t;

static inline void mutex_init(mutex_t* mtx) { mtx->locked = false; }
static inline void mutex_enter_blocking(mutex_t* mtx) { mtx->locked = true; }
static inline void mutex_exit(mutex_t* mtx) { mtx->locked = false; }

// =============================================================================
// Debug Print (redirect to printf for tests)
// =============================================================================

#define DEBUG_PRINT(...) // Disabled in tests

#endif // MOCK_HARDWARE_H

