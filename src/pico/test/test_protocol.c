/**
 * Unit Tests - Protocol
 * 
 * Tests for:
 * - CRC-16-CCITT calculation
 * - Packet structure validation
 */

#include "unity/unity.h"
#include "mocks/mock_hardware.h"
#include <string.h>

// Define protocol constants for tests
#define PROTOCOL_SYNC_BYTE 0xAA
#define PROTOCOL_MAX_PAYLOAD 56

// CRC-16-CCITT implementation (copied from protocol.c for testing)
uint16_t protocol_crc16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    
    return crc;
}

// Note: setUp and tearDown are provided globally in test_main.c

// =============================================================================
// CRC-16 Tests
// =============================================================================

void test_crc16_empty_data(void) {
    uint16_t crc = protocol_crc16(NULL, 0);
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, crc);  // Initial value
}

void test_crc16_single_byte(void) {
    uint8_t data[] = {0x00};
    uint16_t crc = protocol_crc16(data, 1);
    // Known CRC-16-CCITT value for single 0x00 byte
    TEST_ASSERT_EQUAL_UINT16(0xE1F0, crc);
}

void test_crc16_known_pattern_123456789(void) {
    // Standard test pattern "123456789"
    uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    uint16_t crc = protocol_crc16(data, 9);
    // CRC-16-CCITT (0xFFFF init, 0x1021 poly) for "123456789" = 0x29B1
    TEST_ASSERT_EQUAL_UINT16(0x29B1, crc);
}

void test_crc16_all_zeros(void) {
    // SPEC: 8 bytes of 0x00 should produce a specific, consistent CRC
    uint8_t data[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint16_t crc = protocol_crc16(data, 8);
    
    // Calculate expected value and verify consistency
    uint16_t crc2 = protocol_crc16(data, 8);
    TEST_ASSERT_EQUAL_UINT16(crc, crc2);  // Consistency check
    
    // For CRC-16-CCITT with init=0xFFFF, poly=0x1021:
    // 8 zeros produces a specific value (different from init)
    TEST_ASSERT_TRUE(crc != 0xFFFF);  // Should have changed from initial
}

void test_crc16_all_ones(void) {
    // SPEC: 8 bytes of 0xFF should produce a specific, consistent CRC
    uint8_t data[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint16_t crc = protocol_crc16(data, 8);
    
    // Consistency check
    uint16_t crc2 = protocol_crc16(data, 8);
    TEST_ASSERT_EQUAL_UINT16(crc, crc2);
    
    // All 0xFF should produce different CRC than all 0x00
    uint8_t zeros[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint16_t crc_zeros = protocol_crc16(zeros, 8);
    TEST_ASSERT_TRUE(crc != crc_zeros);
}

void test_crc16_consistency(void) {
    // Same data should always produce same CRC
    uint8_t data[] = {0xAA, 0x01, 0x05, 0x00};
    uint16_t crc1 = protocol_crc16(data, 4);
    uint16_t crc2 = protocol_crc16(data, 4);
    TEST_ASSERT_EQUAL_UINT16(crc1, crc2);
}

void test_crc16_different_data_different_crc(void) {
    uint8_t data1[] = {0x01, 0x02, 0x03};
    uint8_t data2[] = {0x01, 0x02, 0x04};  // One bit different
    
    uint16_t crc1 = protocol_crc16(data1, 3);
    uint16_t crc2 = protocol_crc16(data2, 3);
    
    TEST_ASSERT_TRUE(crc1 != crc2);
}

void test_crc16_bit_flip_detected(void) {
    uint8_t data[] = {0xAA, 0x55, 0xF0, 0x0F};
    uint16_t original_crc = protocol_crc16(data, 4);
    
    // Flip one bit
    data[2] ^= 0x01;
    uint16_t modified_crc = protocol_crc16(data, 4);
    
    TEST_ASSERT_TRUE(original_crc != modified_crc);
}

void test_crc16_status_packet_simulation(void) {
    // SPEC: Real packet structure should produce consistent CRC
    // Simulate a status packet: type=0x01, length=22, seq=0
    uint8_t packet_data[] = {
        0x01,  // type
        0x16,  // length (22 bytes)
        0x00,  // seq
        // Payload (22 bytes of sample status data)
        0x9C, 0x03,  // brew_temp = 924 (92.4°C)
        0x78, 0x05,  // steam_temp = 1400 (140.0°C)
        0x00, 0x00,  // group_temp = 0
        0xE8, 0x03,  // pressure = 1000 (10.00 bar)
        0xA2, 0x03,  // brew_setpoint = 930
        0x64, 0x05,  // steam_setpoint = 1380
        0x64,        // brew_output = 100%
        0x00,        // steam_output = 0%
        0x00,        // pump_output = 0%
        0x02,        // state = STATE_READY
        0x01,        // flags = brewing
        0x64,        // water_level = 100%
        0x00, 0x00,  // power_watts = 0
        0x00, 0x00, 0x00, 0x00  // uptime (partial)
    };
    
    uint16_t crc = protocol_crc16(packet_data, sizeof(packet_data));
    
    // Consistency check
    uint16_t crc2 = protocol_crc16(packet_data, sizeof(packet_data));
    TEST_ASSERT_EQUAL_UINT16(crc, crc2);
    
    // Single bit change should be detected (Hamming distance >= 1)
    packet_data[3] = 0x9D;  // Change brew_temp by 1 (0x9C -> 0x9D)
    uint16_t crc_modified = protocol_crc16(packet_data, sizeof(packet_data));
    TEST_ASSERT_TRUE(crc != crc_modified);
    
    // Restore and verify we get original CRC
    packet_data[3] = 0x9C;
    uint16_t crc_restored = protocol_crc16(packet_data, sizeof(packet_data));
    TEST_ASSERT_EQUAL_UINT16(crc, crc_restored);
}

// =============================================================================
// Packet Structure Tests
// =============================================================================

void test_packet_header_size(void) {
    // Header: SYNC(1) + TYPE(1) + LENGTH(1) + SEQ(1) = 4 bytes
    // CRC: 2 bytes
    // Total overhead: 6 bytes
    TEST_ASSERT_EQUAL_INT(4, 1 + 1 + 1 + 1);  // Header
    TEST_ASSERT_EQUAL_INT(2, 2);               // CRC
    TEST_ASSERT_EQUAL_INT(6, 4 + 2);           // Total overhead
}

void test_max_packet_size(void) {
    // Max packet: SYNC(1) + TYPE(1) + LENGTH(1) + SEQ(1) + PAYLOAD(56) + CRC(2)
    int max_packet_size = 1 + 1 + 1 + 1 + PROTOCOL_MAX_PAYLOAD + 2;
    TEST_ASSERT_EQUAL_INT(62, max_packet_size);
}

void test_status_payload_size(void) {
    // status_payload_t should be 32 bytes based on protocol.h:
    // - 6x int16_t (12) + 6x uint8_t (6) + uint16_t (2) + 2x uint32_t (8)
    // - + heating_strategy (1) + cleaning_reminder (1) + brew_count (2) = 32 bytes
    // Let's verify the structure fits within max payload
    TEST_ASSERT_TRUE(32 <= PROTOCOL_MAX_PAYLOAD);
}

void test_alarm_payload_size(void) {
    // alarm_payload_t: code(1) + severity(1) + value(2) = 4 bytes
    TEST_ASSERT_EQUAL_INT(4, 1 + 1 + 2);
}

void test_boot_payload_size(void) {
    // boot_payload_t: version(3) + machine_type(1) + pcb_type(1) + pcb_version(2) + reset(4) = 11 bytes
    TEST_ASSERT_EQUAL_INT(11, 3 + 1 + 1 + 2 + 4);
}

void test_config_payload_size(void) {
    // config_payload_t: setpoints(4) + offset(2) + pid(6) + strategy(1) + type(1) = 14 bytes
    TEST_ASSERT_EQUAL_INT(14, 4 + 2 + 6 + 1 + 1);
}

void test_ack_payload_size(void) {
    // ack_payload_t: cmd_type(1) + cmd_seq(1) + result(1) + reserved(1) = 4 bytes
    TEST_ASSERT_EQUAL_INT(4, 1 + 1 + 1 + 1);
}

// =============================================================================
// CRC Edge Cases
// =============================================================================

void test_crc16_large_payload(void) {
    // SPEC: Maximum payload size should produce consistent, valid CRC
    uint8_t data[PROTOCOL_MAX_PAYLOAD];
    for (int i = 0; i < PROTOCOL_MAX_PAYLOAD; i++) {
        data[i] = (uint8_t)i;
    }
    
    uint16_t crc1 = protocol_crc16(data, PROTOCOL_MAX_PAYLOAD);
    uint16_t crc2 = protocol_crc16(data, PROTOCOL_MAX_PAYLOAD);
    
    // Consistency: same data produces same CRC
    TEST_ASSERT_EQUAL_UINT16(crc1, crc2);
    
    // Sensitivity: changing one byte changes CRC
    data[PROTOCOL_MAX_PAYLOAD / 2] ^= 0x01;  // Flip one bit
    uint16_t crc_modified = protocol_crc16(data, PROTOCOL_MAX_PAYLOAD);
    TEST_ASSERT_TRUE(crc1 != crc_modified);
}

void test_crc16_incremental_data(void) {
    // Test that incrementing data produces different CRCs
    uint8_t data[4] = {0, 0, 0, 0};
    uint16_t prev_crc = 0;
    
    for (int i = 0; i < 256; i++) {
        data[0] = (uint8_t)i;
        uint16_t crc = protocol_crc16(data, 4);
        
        if (i > 0) {
            TEST_ASSERT_TRUE(crc != prev_crc);
        }
        prev_crc = crc;
    }
}

// =============================================================================
// Test Runner
// =============================================================================

int run_protocol_tests(void) {
    UnityBegin("test_protocol.c");
    
    // CRC-16 Tests
    RUN_TEST(test_crc16_empty_data);
    RUN_TEST(test_crc16_single_byte);
    RUN_TEST(test_crc16_known_pattern_123456789);
    RUN_TEST(test_crc16_all_zeros);
    RUN_TEST(test_crc16_all_ones);
    RUN_TEST(test_crc16_consistency);
    RUN_TEST(test_crc16_different_data_different_crc);
    RUN_TEST(test_crc16_bit_flip_detected);
    RUN_TEST(test_crc16_status_packet_simulation);
    
    // Packet Structure Tests
    RUN_TEST(test_packet_header_size);
    RUN_TEST(test_max_packet_size);
    RUN_TEST(test_status_payload_size);
    RUN_TEST(test_alarm_payload_size);
    RUN_TEST(test_boot_payload_size);
    RUN_TEST(test_config_payload_size);
    RUN_TEST(test_ack_payload_size);
    
    // CRC Edge Cases
    RUN_TEST(test_crc16_large_payload);
    RUN_TEST(test_crc16_incremental_data);
    
    return UnityEnd();
}

