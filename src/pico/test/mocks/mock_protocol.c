/**
 * Mock Protocol Functions for Unit Tests
 * 
 * Stub implementations for advanced protocol functions that aren't needed
 * for basic protocol tests but are required for linking advanced tests.
 */

#include "protocol.h"
#include <string.h>

// Mock statistics
static protocol_stats_t g_mock_stats = {0};
static bool g_mock_handshake_complete = false;

void protocol_get_stats(protocol_stats_t* stats) {
    if (stats) {
        memcpy(stats, &g_mock_stats, sizeof(protocol_stats_t));
    }
}

void protocol_reset_stats(void) {
    memset(&g_mock_stats, 0, sizeof(protocol_stats_t));
    g_mock_handshake_complete = false;
}

uint32_t protocol_get_crc_errors(void) {
    return g_mock_stats.crc_errors;
}

uint32_t protocol_get_packet_errors(void) {
    return g_mock_stats.packet_errors;
}

void protocol_reset_error_counters(void) {
    g_mock_stats.crc_errors = 0;
    g_mock_stats.packet_errors = 0;
    g_mock_stats.timeout_errors = 0;
    g_mock_stats.sequence_errors = 0;
}

bool protocol_is_ready(void) {
    return g_mock_handshake_complete;
}

bool protocol_handshake_complete(void) {
    return g_mock_handshake_complete;
}

void protocol_request_handshake(void) {
    // Mock: immediately complete handshake
    g_mock_handshake_complete = true;
    g_mock_stats.handshake_complete = true;
}
