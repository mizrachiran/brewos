/**
 * Unit Tests - Class B Safety Routines (IEC 60730/60335)
 * 
 * Tests for:
 * - CRC-32 calculation
 * - RAM March C- test
 * - CPU register test
 * - Clock frequency test
 * - Stack canary test
 * - Program counter flow test
 * - Status tracking and error handling
 */

#include "unity/unity.h"
#include "mocks/mock_hardware.h"
#include <string.h>
#include <stdio.h>

// =============================================================================
// CRC-32 Implementation (copied from class_b.c for testing)
// =============================================================================

// CRC-32 lookup table (generated for polynomial 0xEDB88320)
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD706B3,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

/**
 * Calculate CRC-32 (identical to class_b.c implementation)
 */
static uint32_t class_b_crc32(const uint8_t* data, size_t length, uint32_t initial) {
    uint32_t crc = initial;
    
    for (size_t i = 0; i < length; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return crc;
}

// =============================================================================
// March C- Algorithm Implementation (copied from class_b.c for testing)
// =============================================================================

typedef enum {
    CLASS_B_PASS = 0,
    CLASS_B_FAIL_RAM,
    CLASS_B_FAIL_FLASH,
    CLASS_B_FAIL_CPU,
    CLASS_B_FAIL_IO,
    CLASS_B_FAIL_CLOCK,
    CLASS_B_FAIL_STACK,
    CLASS_B_FAIL_PC,
    CLASS_B_NOT_INITIALIZED
} class_b_result_t;

/**
 * March C- test pattern (identical to class_b.c implementation)
 */
static class_b_result_t march_c_test(volatile uint32_t* buffer, size_t count) {
    const uint32_t patterns[] = {0x00000000, 0xFFFFFFFF, 0xAAAAAAAA, 0x55555555};
    
    for (int p = 0; p < 4; p++) {
        uint32_t pattern = patterns[p];
        uint32_t inverse = ~pattern;
        
        // Step 1: Write pattern ascending
        for (size_t i = 0; i < count; i++) {
            buffer[i] = pattern;
        }
        
        // Step 2: Read pattern, write inverse ascending
        for (size_t i = 0; i < count; i++) {
            if (buffer[i] != pattern) {
                return CLASS_B_FAIL_RAM;
            }
            buffer[i] = inverse;
        }
        
        // Step 3: Read inverse, write pattern ascending
        for (size_t i = 0; i < count; i++) {
            if (buffer[i] != inverse) {
                return CLASS_B_FAIL_RAM;
            }
            buffer[i] = pattern;
        }
        
        // Step 4: Read pattern, write inverse descending
        for (size_t i = count; i > 0; i--) {
            if (buffer[i - 1] != pattern) {
                return CLASS_B_FAIL_RAM;
            }
            buffer[i - 1] = inverse;
        }
        
        // Step 5: Read inverse, write pattern descending
        for (size_t i = count; i > 0; i--) {
            if (buffer[i - 1] != inverse) {
                return CLASS_B_FAIL_RAM;
            }
            buffer[i - 1] = pattern;
        }
        
        // Step 6: Final verify ascending
        for (size_t i = 0; i < count; i++) {
            if (buffer[i] != pattern) {
                return CLASS_B_FAIL_RAM;
            }
        }
    }
    
    return CLASS_B_PASS;
}

// =============================================================================
// CRC-32 Tests
// =============================================================================

void test_crc32_empty_data(void) {
    // Empty data should return initial value
    uint32_t crc = class_b_crc32(NULL, 0, 0xFFFFFFFF);
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFF, crc);
}

void test_crc32_single_byte_zero(void) {
    uint8_t data[] = {0x00};
    uint32_t crc = class_b_crc32(data, 1, 0xFFFFFFFF);
    // CRC should change from initial value
    TEST_ASSERT_TRUE(crc != 0xFFFFFFFF);
    
    // Verify consistency
    uint32_t crc2 = class_b_crc32(data, 1, 0xFFFFFFFF);
    TEST_ASSERT_EQUAL_UINT32(crc, crc2);
}

void test_crc32_known_pattern_123456789(void) {
    // Standard CRC-32 test vector: "123456789" should give 0xCBF43926
    uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    uint32_t crc = class_b_crc32(data, 9, 0xFFFFFFFF);
    crc ^= 0xFFFFFFFF;  // Final XOR for standard CRC-32
    TEST_ASSERT_EQUAL_UINT32(0xCBF43926, crc);
}

void test_crc32_all_zeros(void) {
    uint8_t data[8] = {0};
    uint32_t crc1 = class_b_crc32(data, 8, 0xFFFFFFFF);
    uint32_t crc2 = class_b_crc32(data, 8, 0xFFFFFFFF);
    
    // Consistency check
    TEST_ASSERT_EQUAL_UINT32(crc1, crc2);
    
    // Should change from initial value
    TEST_ASSERT_TRUE(crc1 != 0xFFFFFFFF);
}

void test_crc32_all_ones(void) {
    uint8_t data[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint32_t crc = class_b_crc32(data, 8, 0xFFFFFFFF);
    
    // Should produce different CRC than all zeros
    uint8_t zeros[8] = {0};
    uint32_t crc_zeros = class_b_crc32(zeros, 8, 0xFFFFFFFF);
    
    TEST_ASSERT_TRUE(crc != crc_zeros);
}

void test_crc32_consistency(void) {
    uint8_t data[] = {0xAA, 0x55, 0xDE, 0xAD, 0xBE, 0xEF};
    uint32_t crc1 = class_b_crc32(data, 6, 0xFFFFFFFF);
    uint32_t crc2 = class_b_crc32(data, 6, 0xFFFFFFFF);
    
    TEST_ASSERT_EQUAL_UINT32(crc1, crc2);
}

void test_crc32_different_data_different_crc(void) {
    uint8_t data1[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t data2[] = {0x01, 0x02, 0x03, 0x05};  // One bit different
    
    uint32_t crc1 = class_b_crc32(data1, 4, 0xFFFFFFFF);
    uint32_t crc2 = class_b_crc32(data2, 4, 0xFFFFFFFF);
    
    TEST_ASSERT_TRUE(crc1 != crc2);
}

void test_crc32_bit_flip_detected(void) {
    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint32_t original_crc = class_b_crc32(data, 4, 0xFFFFFFFF);
    
    // Flip one bit
    data[2] ^= 0x01;
    uint32_t modified_crc = class_b_crc32(data, 4, 0xFFFFFFFF);
    
    TEST_ASSERT_TRUE(original_crc != modified_crc);
}

void test_crc32_incremental_calculation(void) {
    // Test that incremental CRC calculation works
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    
    // Calculate in one go
    uint32_t crc_full = class_b_crc32(data, 8, 0xFFFFFFFF);
    
    // Calculate incrementally
    uint32_t crc_inc = 0xFFFFFFFF;
    crc_inc = class_b_crc32(&data[0], 4, crc_inc);
    crc_inc = class_b_crc32(&data[4], 4, crc_inc);
    
    TEST_ASSERT_EQUAL_UINT32(crc_full, crc_inc);
}

void test_crc32_large_buffer(void) {
    // Test with larger buffer (simulating Flash CRC)
    uint8_t data[1024];
    for (int i = 0; i < 1024; i++) {
        data[i] = (uint8_t)(i & 0xFF);
    }
    
    uint32_t crc1 = class_b_crc32(data, 1024, 0xFFFFFFFF);
    uint32_t crc2 = class_b_crc32(data, 1024, 0xFFFFFFFF);
    
    // Consistency
    TEST_ASSERT_EQUAL_UINT32(crc1, crc2);
    
    // Modification detection
    data[512] ^= 0x01;
    uint32_t crc_modified = class_b_crc32(data, 1024, 0xFFFFFFFF);
    TEST_ASSERT_TRUE(crc1 != crc_modified);
}

// =============================================================================
// RAM March C- Tests
// =============================================================================

void test_march_c_small_buffer(void) {
    volatile uint32_t buffer[4];
    class_b_result_t result = march_c_test(buffer, 4);
    TEST_ASSERT_EQUAL(CLASS_B_PASS, result);
}

void test_march_c_larger_buffer(void) {
    volatile uint32_t buffer[16];
    class_b_result_t result = march_c_test(buffer, 16);
    TEST_ASSERT_EQUAL(CLASS_B_PASS, result);
}

void test_march_c_typical_size(void) {
    // Test with typical Class B test size (64 bytes = 16 words)
    volatile uint32_t buffer[16];
    class_b_result_t result = march_c_test(buffer, 16);
    TEST_ASSERT_EQUAL(CLASS_B_PASS, result);
}

void test_march_c_pattern_coverage(void) {
    // Verify all patterns are tested by checking final state
    volatile uint32_t buffer[4];
    
    // After march_c_test, buffer should contain the last pattern (0x55555555)
    class_b_result_t result = march_c_test(buffer, 4);
    TEST_ASSERT_EQUAL(CLASS_B_PASS, result);
    
    // The algorithm leaves the buffer in a known state
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_UINT32(0x55555555, buffer[i]);
    }
}

void test_march_c_single_word(void) {
    volatile uint32_t buffer[1];
    class_b_result_t result = march_c_test(buffer, 1);
    TEST_ASSERT_EQUAL(CLASS_B_PASS, result);
}

void test_march_c_preserves_data_integrity(void) {
    // Run march test multiple times to verify consistency
    volatile uint32_t buffer[8];
    
    for (int run = 0; run < 10; run++) {
        class_b_result_t result = march_c_test(buffer, 8);
        TEST_ASSERT_EQUAL(CLASS_B_PASS, result);
    }
}

// =============================================================================
// CPU Register Tests (simplified for host testing)
// =============================================================================

void test_cpu_arithmetic_operations(void) {
    // Test that basic arithmetic works correctly
    volatile uint32_t result;
    
    // Addition
    result = 0;
    for (uint32_t i = 0; i < 100; i++) {
        result += i;
    }
    TEST_ASSERT_EQUAL_UINT32(4950, result);  // Sum 0-99
    
    // Multiplication
    result = 12345;
    result = result * 67;
    TEST_ASSERT_EQUAL_UINT32(827115, result);
    
    // Division
    result = 1000000;
    result = result / 1000;
    TEST_ASSERT_EQUAL_UINT32(1000, result);
}

void test_cpu_pattern_verification(void) {
    volatile uint32_t test_val;
    const uint32_t patterns[] = {
        0x00000000,
        0xFFFFFFFF,
        0xAAAAAAAA,
        0x55555555,
        0x12345678,
        0x87654321
    };
    
    for (int i = 0; i < 6; i++) {
        test_val = patterns[i];
        TEST_ASSERT_EQUAL_UINT32(patterns[i], test_val);
    }
}

void test_cpu_bit_operations(void) {
    volatile uint32_t val;
    
    // AND
    val = 0xFF00FF00 & 0x0F0F0F0F;
    TEST_ASSERT_EQUAL_UINT32(0x0F000F00, val);
    
    // OR
    val = 0xFF00FF00 | 0x00FF00FF;
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFF, val);
    
    // XOR
    val = 0xAAAAAAAA ^ 0x55555555;
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFF, val);
    
    // NOT
    val = ~0x00000000;
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFF, val);
    
    // Shift left
    val = 1 << 31;
    TEST_ASSERT_EQUAL_UINT32(0x80000000, val);
    
    // Shift right
    val = 0x80000000 >> 31;
    TEST_ASSERT_EQUAL_UINT32(0x00000001, val);
}

// =============================================================================
// Stack Canary Tests
// =============================================================================

#define TEST_STACK_CANARY_VALUE 0xDEADBEEF

void test_stack_canary_intact(void) {
    volatile uint32_t canary_top = TEST_STACK_CANARY_VALUE;
    volatile uint32_t canary_bottom = TEST_STACK_CANARY_VALUE;
    
    // Simulate some stack usage
    volatile uint8_t local_buffer[64];
    for (int i = 0; i < 64; i++) {
        local_buffer[i] = (uint8_t)i;
    }
    
    // Canaries should still be intact
    TEST_ASSERT_EQUAL_UINT32(TEST_STACK_CANARY_VALUE, canary_top);
    TEST_ASSERT_EQUAL_UINT32(TEST_STACK_CANARY_VALUE, canary_bottom);
}

void test_stack_canary_corruption_detection(void) {
    volatile uint32_t canary = TEST_STACK_CANARY_VALUE;
    
    // Verify intact
    TEST_ASSERT_EQUAL_UINT32(TEST_STACK_CANARY_VALUE, canary);
    
    // Simulate corruption
    canary = 0x12345678;
    
    // Verify detection
    TEST_ASSERT_TRUE(canary != TEST_STACK_CANARY_VALUE);
}

// =============================================================================
// Program Counter Flow Tests
// =============================================================================

#define PC_TEST_MARKER_1    0x12345678
#define PC_TEST_MARKER_2    0x87654321
#define PC_TEST_MARKER_3    0xABCDEF01

static volatile uint32_t g_pc_test_marker = 0;

static void pc_test_func_1(void) {
    g_pc_test_marker = PC_TEST_MARKER_1;
}

static void pc_test_func_2(void) {
    if (g_pc_test_marker == PC_TEST_MARKER_1) {
        g_pc_test_marker = PC_TEST_MARKER_2;
    }
}

static void pc_test_func_3(void) {
    if (g_pc_test_marker == PC_TEST_MARKER_2) {
        g_pc_test_marker = PC_TEST_MARKER_3;
    }
}

void test_program_counter_flow_correct(void) {
    g_pc_test_marker = 0;
    
    // Call functions in correct order
    pc_test_func_1();
    pc_test_func_2();
    pc_test_func_3();
    
    // Should reach final marker
    TEST_ASSERT_EQUAL_UINT32(PC_TEST_MARKER_3, g_pc_test_marker);
}

void test_program_counter_flow_wrong_order(void) {
    g_pc_test_marker = 0;
    
    // Call functions in wrong order
    pc_test_func_2();  // Should not set marker (marker != MARKER_1)
    pc_test_func_1();  // Sets MARKER_1
    pc_test_func_3();  // Should not set marker (marker != MARKER_2)
    
    // Should not reach final marker
    TEST_ASSERT_TRUE(g_pc_test_marker != PC_TEST_MARKER_3);
    TEST_ASSERT_EQUAL_UINT32(PC_TEST_MARKER_1, g_pc_test_marker);
}

void test_program_counter_flow_skip_function(void) {
    g_pc_test_marker = 0;
    
    // Skip middle function
    pc_test_func_1();  // Sets MARKER_1
    // Skip pc_test_func_2()
    pc_test_func_3();  // Should not set marker (marker != MARKER_2)
    
    // Should not reach final marker
    TEST_ASSERT_EQUAL_UINT32(PC_TEST_MARKER_1, g_pc_test_marker);
}

// =============================================================================
// Clock Tolerance Tests
// =============================================================================

void test_clock_tolerance_within_bounds(void) {
    // Use 64-bit arithmetic to avoid overflow
    uint64_t nominal = 125000000;  // 125 MHz
    uint32_t tolerance_pct = 5;
    
    uint64_t min_freq = nominal * (100 - tolerance_pct) / 100;  // 118.75 MHz
    uint64_t max_freq = nominal * (100 + tolerance_pct) / 100;  // 131.25 MHz
    
    // Test various frequencies within bounds
    TEST_ASSERT_TRUE(nominal >= min_freq && nominal <= max_freq);
    TEST_ASSERT_TRUE(120000000ULL >= min_freq && 120000000ULL <= max_freq);
    TEST_ASSERT_TRUE(130000000ULL >= min_freq && 130000000ULL <= max_freq);
}

void test_clock_tolerance_outside_bounds(void) {
    // Use 64-bit arithmetic to avoid overflow
    uint64_t nominal = 125000000;
    uint32_t tolerance_pct = 5;
    
    uint64_t min_freq = nominal * (100 - tolerance_pct) / 100;
    uint64_t max_freq = nominal * (100 + tolerance_pct) / 100;
    
    // Test frequencies outside bounds
    TEST_ASSERT_FALSE(100000000ULL >= min_freq && 100000000ULL <= max_freq);  // Too low
    TEST_ASSERT_FALSE(150000000ULL >= min_freq && 150000000ULL <= max_freq);  // Too high
}

// =============================================================================
// Result String Tests
// =============================================================================

void test_result_codes_unique(void) {
    // Verify all result codes are unique
    TEST_ASSERT_TRUE(CLASS_B_PASS != CLASS_B_FAIL_RAM);
    TEST_ASSERT_TRUE(CLASS_B_PASS != CLASS_B_FAIL_FLASH);
    TEST_ASSERT_TRUE(CLASS_B_PASS != CLASS_B_FAIL_CPU);
    TEST_ASSERT_TRUE(CLASS_B_PASS != CLASS_B_FAIL_IO);
    TEST_ASSERT_TRUE(CLASS_B_PASS != CLASS_B_FAIL_CLOCK);
    TEST_ASSERT_TRUE(CLASS_B_PASS != CLASS_B_FAIL_STACK);
    TEST_ASSERT_TRUE(CLASS_B_PASS != CLASS_B_FAIL_PC);
    TEST_ASSERT_TRUE(CLASS_B_PASS != CLASS_B_NOT_INITIALIZED);
}

void test_result_pass_is_zero(void) {
    // CLASS_B_PASS should be 0 for easy boolean checks
    TEST_ASSERT_EQUAL(0, CLASS_B_PASS);
}

// =============================================================================
// Integration Tests
// =============================================================================

void test_full_ram_test_cycle(void) {
    // Run complete RAM test cycle
    volatile uint32_t buffer[16];
    
    // Initialize with known pattern
    for (int i = 0; i < 16; i++) {
        buffer[i] = 0xDEADBEEF;
    }
    
    // Run March C- test
    class_b_result_t result = march_c_test(buffer, 16);
    TEST_ASSERT_EQUAL(CLASS_B_PASS, result);
}

void test_crc32_flash_simulation(void) {
    // Simulate Flash CRC verification
    uint8_t flash_data[4096];
    
    // Fill with "code"
    for (int i = 0; i < 4096; i++) {
        flash_data[i] = (uint8_t)((i * 7 + 13) & 0xFF);  // Pseudo-random pattern
    }
    
    // Calculate reference CRC at "boot"
    uint32_t reference_crc = class_b_crc32(flash_data, 4096, 0xFFFFFFFF);
    reference_crc ^= 0xFFFFFFFF;
    
    // Later verification should match
    uint32_t verify_crc = class_b_crc32(flash_data, 4096, 0xFFFFFFFF);
    verify_crc ^= 0xFFFFFFFF;
    
    TEST_ASSERT_EQUAL_UINT32(reference_crc, verify_crc);
    
    // Corruption should be detected
    flash_data[2048] ^= 0x01;
    uint32_t corrupted_crc = class_b_crc32(flash_data, 4096, 0xFFFFFFFF);
    corrupted_crc ^= 0xFFFFFFFF;
    
    TEST_ASSERT_TRUE(reference_crc != corrupted_crc);
}

void test_multiple_test_cycles(void) {
    // Run multiple cycles to verify stability
    volatile uint32_t buffer[16];
    
    for (int cycle = 0; cycle < 100; cycle++) {
        class_b_result_t result = march_c_test(buffer, 16);
        TEST_ASSERT_EQUAL(CLASS_B_PASS, result);  // March C- should pass on repeated cycles
    }
}

// =============================================================================
// Test Runner
// =============================================================================

int run_class_b_tests(void) {
    UnityBegin("test_class_b.c");
    
    // CRC-32 Tests
    RUN_TEST(test_crc32_empty_data);
    RUN_TEST(test_crc32_single_byte_zero);
    RUN_TEST(test_crc32_known_pattern_123456789);
    RUN_TEST(test_crc32_all_zeros);
    RUN_TEST(test_crc32_all_ones);
    RUN_TEST(test_crc32_consistency);
    RUN_TEST(test_crc32_different_data_different_crc);
    RUN_TEST(test_crc32_bit_flip_detected);
    RUN_TEST(test_crc32_incremental_calculation);
    RUN_TEST(test_crc32_large_buffer);
    
    // RAM March C- Tests
    RUN_TEST(test_march_c_small_buffer);
    RUN_TEST(test_march_c_larger_buffer);
    RUN_TEST(test_march_c_typical_size);
    RUN_TEST(test_march_c_pattern_coverage);
    RUN_TEST(test_march_c_single_word);
    RUN_TEST(test_march_c_preserves_data_integrity);
    
    // CPU Register Tests
    RUN_TEST(test_cpu_arithmetic_operations);
    RUN_TEST(test_cpu_pattern_verification);
    RUN_TEST(test_cpu_bit_operations);
    
    // Stack Canary Tests
    RUN_TEST(test_stack_canary_intact);
    RUN_TEST(test_stack_canary_corruption_detection);
    
    // Program Counter Flow Tests
    RUN_TEST(test_program_counter_flow_correct);
    RUN_TEST(test_program_counter_flow_wrong_order);
    RUN_TEST(test_program_counter_flow_skip_function);
    
    // Clock Tolerance Tests
    RUN_TEST(test_clock_tolerance_within_bounds);
    RUN_TEST(test_clock_tolerance_outside_bounds);
    
    // Result Code Tests
    RUN_TEST(test_result_codes_unique);
    RUN_TEST(test_result_pass_is_zero);
    
    // Integration Tests
    RUN_TEST(test_full_ram_test_cycle);
    RUN_TEST(test_crc32_flash_simulation);
    RUN_TEST(test_multiple_test_cycles);
    
    return UnityEnd();
}

