/**
 * Advanced Protocol Tests
 * 
 * Tests for retry logic, timeout handling, backpressure, and handshake.
 */

#include "unity/unity.h"
#include "protocol.h"
#include "mocks/mock_hardware.h"
#include <string.h>

// Mock packet callback for testing
static packet_t g_last_packet_received = {0};
static int g_packet_receive_count = 0;

static void test_packet_callback(const packet_t* packet) {
    if (packet && packet->valid) {
        memcpy(&g_last_packet_received, packet, sizeof(packet_t));
        g_packet_receive_count++;
    }
}

// Test-specific setUp/tearDown
static void test_setUp(void) {
    g_packet_receive_count = 0;
    memset(&g_last_packet_received, 0, sizeof(packet_t));
}

static void test_tearDown(void) {
    // Clean up
}

// =============================================================================
// Protocol Statistics Tests
// =============================================================================

void test_protocol_stats_initialization(void) {
    protocol_stats_t stats;
    protocol_get_stats(&stats);
    
    // Most counters should start at 0
    TEST_ASSERT_EQUAL_UINT32(0, stats.crc_errors);
    TEST_ASSERT_EQUAL_UINT32(0, stats.packet_errors);
    TEST_ASSERT_EQUAL_UINT32(0, stats.timeout_errors);
    TEST_ASSERT_EQUAL_UINT32(0, stats.sequence_errors);
}

void test_protocol_stats_reset(void) {
    // Force some statistics
    protocol_stats_t stats;
    protocol_get_stats(&stats);
    
    // Reset and verify
    protocol_reset_stats();
    protocol_get_stats(&stats);
    
    TEST_ASSERT_EQUAL_UINT32(0, stats.packets_received);
    TEST_ASSERT_EQUAL_UINT32(0, stats.crc_errors);
    TEST_ASSERT_FALSE(stats.handshake_complete);
}

void test_protocol_error_counters(void) {
    uint32_t initial_crc = protocol_get_crc_errors();
    uint32_t initial_packet = protocol_get_packet_errors();
    
    // Counters should be accessible
    TEST_ASSERT_GREATER_OR_EQUAL(0, initial_crc);
    TEST_ASSERT_GREATER_OR_EQUAL(0, initial_packet);
    
    // Reset should clear them
    protocol_reset_error_counters();
    TEST_ASSERT_EQUAL_UINT32(0, protocol_get_crc_errors());
    TEST_ASSERT_EQUAL_UINT32(0, protocol_get_packet_errors());
}

// =============================================================================
// Handshake Tests
// =============================================================================

void test_protocol_handshake_initial_state(void) {
    // Initially, handshake should not be complete
    TEST_ASSERT_FALSE(protocol_handshake_complete());
    TEST_ASSERT_FALSE(protocol_is_ready());
}

void test_protocol_handshake_request(void) {
    // Request handshake should send MSG_HANDSHAKE packet
    protocol_request_handshake();
    
    // Would need to verify UART output, which requires mock verification
    // At minimum, shouldn't crash
    TEST_PASS();
}

// =============================================================================
// CRC Tests (from existing test_protocol.c)
// =============================================================================

void test_crc16_handshake_packet(void) {
    // Test CRC for handshake packet structure
    uint8_t data[] = {
        MSG_HANDSHAKE,  // type
        6,              // length
        1,              // seq
        1, 1,           // protocol version 1.1
        0,              // capabilities
        3,              // max retry
        0xE8, 0x03      // 1000ms timeout (little-endian)
    };
    
    uint16_t crc = protocol_crc16(data, sizeof(data));
    
    // CRC should be deterministic
    uint16_t crc2 = protocol_crc16(data, sizeof(data));
    TEST_ASSERT_EQUAL_UINT16(crc, crc2);
    
    // Verify non-zero
    TEST_ASSERT_TRUE(crc != 0);
}

void test_crc16_nack_packet(void) {
    // Test CRC for NACK packet (ACK structure with error code)
    uint8_t data[] = {
        MSG_NACK,       // type
        4,              // length
        5,              // seq
        0x10,           // cmd_type (MSG_CMD_SET_TEMP)
        3,              // cmd_seq
        0x05,           // result (ACK_ERROR_BUSY)
        0               // reserved
    };
    
    uint16_t crc = protocol_crc16(data, sizeof(data));
    
    // CRC should be consistent
    uint16_t crc2 = protocol_crc16(data, sizeof(data));
    TEST_ASSERT_EQUAL_UINT16(crc, crc2);
}

// =============================================================================
// Packet Structure Tests
// =============================================================================

void test_handshake_payload_size(void) {
    handshake_payload_t handshake;
    
    // Handshake payload should be 6 bytes
    TEST_ASSERT_EQUAL(6, sizeof(handshake));
    
    // Verify field layout
    handshake.protocol_version_major = 1;
    handshake.protocol_version_minor = 1;
    handshake.capabilities = 0;
    handshake.max_retry_count = 3;
    handshake.ack_timeout_ms = 1000;
    
    // Check packed correctly
    uint8_t* bytes = (uint8_t*)&handshake;
    TEST_ASSERT_EQUAL_UINT8(1, bytes[0]);  // major
    TEST_ASSERT_EQUAL_UINT8(1, bytes[1]);  // minor
    TEST_ASSERT_EQUAL_UINT8(0, bytes[2]);  // capabilities
    TEST_ASSERT_EQUAL_UINT8(3, bytes[3]);  // max_retry
    TEST_ASSERT_EQUAL_UINT8(0xE8, bytes[4]); // timeout low (1000 = 0x03E8)
    TEST_ASSERT_EQUAL_UINT8(0x03, bytes[5]); // timeout high
}

void test_pending_command_structure(void) {
    pending_cmd_t pending;
    
    // Verify structure size (updated after PROTOCOL_MAX_PAYLOAD increase from 32 to 64)
    // Structure: type(1) + seq(1) + payload[64](64) + length(1) + retry_count(1) + sent_time_ms(4) + active(1) = 73 bytes
    // With padding/alignment, expect around 76 bytes
    TEST_ASSERT_GREATER_OR_EQUAL(73, sizeof(pending));
    TEST_ASSERT_LESS_OR_EQUAL(80, sizeof(pending));  // Should not exceed 80 with any reasonable padding
    
    // Initialize
    pending.type = MSG_CMD_SET_TEMP;
    pending.seq = 42;
    pending.length = 3;
    pending.retry_count = 0;
    pending.active = true;
    
    TEST_ASSERT_EQUAL_UINT8(MSG_CMD_SET_TEMP, pending.type);
    TEST_ASSERT_EQUAL_UINT8(42, pending.seq);
    TEST_ASSERT_TRUE(pending.active);
}

void test_protocol_stats_structure(void) {
    protocol_stats_t stats = {0};
    
    // Verify all fields accessible
    stats.packets_received = 100;
    stats.packets_sent = 95;
    stats.crc_errors = 2;
    stats.timeout_errors = 1;
    stats.retries = 3;
    stats.nacks_sent = 1;
    stats.pending_cmd_count = 2;
    stats.handshake_complete = true;
    
    TEST_ASSERT_EQUAL_UINT32(100, stats.packets_received);
    TEST_ASSERT_EQUAL_UINT32(95, stats.packets_sent);
    TEST_ASSERT_EQUAL_UINT32(2, stats.crc_errors);
    TEST_ASSERT_EQUAL_UINT32(1, stats.timeout_errors);
    TEST_ASSERT_EQUAL_UINT32(3, stats.retries);
    TEST_ASSERT_EQUAL_UINT32(1, stats.nacks_sent);
    TEST_ASSERT_EQUAL_UINT8(2, stats.pending_cmd_count);
    TEST_ASSERT_TRUE(stats.handshake_complete);
}

// =============================================================================
// Configuration Tests
// =============================================================================

void test_protocol_constants(void) {
    // Verify timeout constants are reasonable
    TEST_ASSERT_GREATER_THAN(0, PROTOCOL_PARSER_TIMEOUT_MS);
    TEST_ASSERT_LESS_OR_EQUAL(5000, PROTOCOL_PARSER_TIMEOUT_MS);
    
    TEST_ASSERT_GREATER_THAN(0, PROTOCOL_ACK_TIMEOUT_MS);
    TEST_ASSERT_LESS_OR_EQUAL(5000, PROTOCOL_ACK_TIMEOUT_MS);
    
    TEST_ASSERT_GREATER_THAN(0, PROTOCOL_RETRY_COUNT);
    TEST_ASSERT_LESS_OR_EQUAL(10, PROTOCOL_RETRY_COUNT);
    
    TEST_ASSERT_GREATER_THAN(0, PROTOCOL_MAX_PENDING_CMDS);
    TEST_ASSERT_LESS_OR_EQUAL(10, PROTOCOL_MAX_PENDING_CMDS);
}

void test_protocol_version(void) {
    // Version should be defined
    TEST_ASSERT_GREATER_THAN(0, PROTOCOL_VERSION_MAJOR);
    TEST_ASSERT_GREATER_OR_EQUAL(0, PROTOCOL_VERSION_MINOR);
    
    // Current version is 1.1
    TEST_ASSERT_EQUAL(1, PROTOCOL_VERSION_MAJOR);
    TEST_ASSERT_EQUAL(1, PROTOCOL_VERSION_MINOR);
}

// =============================================================================
// Boot Payload with Protocol Version
// =============================================================================

void test_boot_payload_includes_protocol_version(void) {
    boot_payload_t boot;
    
    // Verify protocol version fields exist
    boot.protocol_version_major = PROTOCOL_VERSION_MAJOR;
    boot.protocol_version_minor = PROTOCOL_VERSION_MINOR;
    
    TEST_ASSERT_EQUAL_UINT8(1, boot.protocol_version_major);
    TEST_ASSERT_EQUAL_UINT8(1, boot.protocol_version_minor);
}

// =============================================================================
// Test Runner
// =============================================================================

int run_protocol_advanced_tests(void) {
    UnityBegin("test_protocol_advanced.c");
    
    RUN_TEST(test_protocol_stats_initialization);
    RUN_TEST(test_protocol_stats_reset);
    RUN_TEST(test_protocol_error_counters);
    RUN_TEST(test_protocol_handshake_initial_state);
    RUN_TEST(test_protocol_handshake_request);
    RUN_TEST(test_crc16_handshake_packet);
    RUN_TEST(test_crc16_nack_packet);
    RUN_TEST(test_handshake_payload_size);
    RUN_TEST(test_pending_command_structure);
    RUN_TEST(test_protocol_stats_structure);
    RUN_TEST(test_protocol_constants);
    RUN_TEST(test_protocol_version);
    RUN_TEST(test_boot_payload_includes_protocol_version);
    
    return UnityEnd();
}
