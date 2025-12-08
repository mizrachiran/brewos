/**
 * ECM Pico Firmware - Serial Bootloader
 * 
 * Receives firmware over UART and writes to flash for OTA updates.
 */

#include "bootloader.h"
#include "config.h"
#include "flash_safe.h"       // Flash safety utilities
#include <string.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"   // For multicore_lockout
#include "pico/platform.h"    // For __not_in_flash_func
#include "hardware/uart.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"

// Bootloader protocol constants
#define BOOTLOADER_MAGIC_1          0x55
#define BOOTLOADER_MAGIC_2          0xAA
#define BOOTLOADER_END_MAGIC_1      0xAA
#define BOOTLOADER_END_MAGIC_2      0x55
#define BOOTLOADER_CHUNK_MAX_SIZE   256
#define BOOTLOADER_TIMEOUT_MS       30000  // 30 seconds total timeout
#define BOOTLOADER_CHUNK_TIMEOUT_MS 5000   // 5 seconds per chunk

// Flash layout
// RP2040 has 2MB flash typically, firmware starts at 0x10000000
// We'll use a reserved region for new firmware (last 512KB)
// After receiving, we copy from staging to main area and reboot.
#define FLASH_TARGET_OFFSET         (1536 * 1024)  // Staging area: start of last 512KB
#define FLASH_MAIN_OFFSET           0              // Main firmware area: start of flash
#define FLASH_MAX_FIRMWARE_SIZE     (512 * 1024)   // Max firmware size: 512KB
// Note: Using SDK definitions for sector/page size to avoid redefinition warnings
// FLASH_SECTOR_SIZE and FLASH_PAGE_SIZE are already defined in hardware/flash.h

// Bootloader state
static uint32_t g_total_size = 0;
static uint32_t g_received_size = 0;
static uint32_t g_chunk_count = 0;
static uint32_t g_expected_crc = 0;  // Expected CRC (if provided by ESP32)
static bool g_receiving = false;

// -----------------------------------------------------------------------------
// CRC32 Calculation
// -----------------------------------------------------------------------------

/**
 * Calculate CRC32 of data using standard polynomial (0xEDB88320)
 * Same algorithm as config_persistence.c for consistency
 */
static uint32_t crc32_calculate(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    const uint32_t polynomial = 0xEDB88320;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ polynomial;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return ~crc;
}

// -----------------------------------------------------------------------------
// UART Helpers
// -----------------------------------------------------------------------------

static bool uart_read_byte_timeout(uint8_t* byte, uint32_t timeout_ms) {
    absolute_time_t timeout = make_timeout_time_ms(timeout_ms);
    
    while (!uart_is_readable(ESP32_UART_ID)) {
        if (time_reached(timeout)) {
            return false;
        }
        sleep_us(100);
    }
    
    *byte = uart_getc(ESP32_UART_ID);
    return true;
}

static bool uart_read_bytes_timeout(uint8_t* buffer, size_t count, uint32_t timeout_ms) {
    absolute_time_t start = get_absolute_time();
    
    for (size_t i = 0; i < count; i++) {
        uint32_t elapsed_ms = to_ms_since_boot(get_absolute_time()) - to_ms_since_boot(start);
        uint32_t remaining = timeout_ms > elapsed_ms ? timeout_ms - elapsed_ms : 100;
        
        if (!uart_read_byte_timeout(&buffer[i], remaining)) {
            return false;
        }
    }
    
    return true;
}

static void uart_write_byte(uint8_t byte) {
    uart_putc(ESP32_UART_ID, byte);
}

static void uart_write_bytes(const uint8_t* data, size_t len) {
    uart_write_blocking(ESP32_UART_ID, data, len);
}

// -----------------------------------------------------------------------------
// Flash Helpers (using flash_safe API)
// -----------------------------------------------------------------------------
// Now using centralized flash_safe API which handles:
// - Multicore lockout
// - Interrupt safety  
// - RAM execution requirements
// Compatible with both RP2040 and RP2350 (Pico 2)

/**
 * Erase a flash sector safely.
 */
static bool flash_erase_sector(uint32_t offset) {
    return flash_safe_erase(offset, FLASH_SECTOR_SIZE);
}

/**
 * Write a flash page safely.
 */
static bool flash_write_page(uint32_t offset, const uint8_t* data) {
    if (!data) return false;
    return flash_safe_program(offset, data, FLASH_PAGE_SIZE);
}

/**
 * Copy firmware from staging area to main area.
 * 
 * CRITICAL: This function runs from RAM and copies the staged firmware
 * to the main execution area (0x10000000). After this, the device must
 * reboot to run the new firmware.
 * 
 * This function does not return on success - it reboots the device.
 */
static void __not_in_flash_func(copy_firmware_to_main)(uint32_t firmware_size) {
    // XIP_BASE is where flash is memory-mapped for execution
    #ifndef XIP_BASE
    #define XIP_BASE 0x10000000
    #endif
    
    // Round up to page size
    uint32_t size_pages = (firmware_size + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE;
    uint32_t size_sectors = (firmware_size + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE;
    
    // CRITICAL: Kick watchdog to give us full timeout budget (2000ms)
    // The flash copy takes time:
    //   - Erasing 512KB: ~200-400ms
    //   - Programming 512KB: ~200-300ms
    // If watchdog fires during copy, device will be bricked!
    watchdog_update();
    
    // Pause Core 0 (we're on Core 1) for the entire copy operation
    multicore_lockout_start_blocking();
    
    // Disable interrupts for the entire copy (this is a long operation!)
    uint32_t ints = save_and_disable_interrupts();
    
    // Erase main firmware area
    for (uint32_t sector = 0; sector < size_sectors; sector++) {
        uint32_t offset = FLASH_MAIN_OFFSET + (sector * FLASH_SECTOR_SIZE);
        flash_range_erase(offset, FLASH_SECTOR_SIZE);
    }
    
    // Copy from staging to main, page by page
    const uint8_t* staging_addr = (const uint8_t*)(XIP_BASE + FLASH_TARGET_OFFSET);
    
    for (uint32_t page = 0; page < size_pages; page++) {
        uint32_t offset = FLASH_MAIN_OFFSET + (page * FLASH_PAGE_SIZE);
        
        // Read page from staging area (memory-mapped read is safe)
        // But we need to copy to a RAM buffer first because flash_range_program
        // reads from the source while programming, which won't work during erase
        static uint8_t page_buffer[FLASH_PAGE_SIZE];
        memcpy(page_buffer, staging_addr + (page * FLASH_PAGE_SIZE), FLASH_PAGE_SIZE);
        
        // Write to main area
        flash_range_program(offset, page_buffer, FLASH_PAGE_SIZE);
    }
    
    restore_interrupts(ints);
    multicore_lockout_end_blocking();
    
    // Reboot to run new firmware
    // Use AIRCR (Application Interrupt and Reset Control Register) for clean reset
    watchdog_reboot(0, 0, 0);
    
    // Should never reach here
    while (1) {
        tight_loop_contents();
    }
}

// -----------------------------------------------------------------------------
// Bootloader Protocol
// -----------------------------------------------------------------------------

static bool receive_chunk_header(uint32_t* chunk_num, uint16_t* chunk_size) {
    uint8_t header[8];
    
    // Read magic bytes
    if (!uart_read_bytes_timeout(header, 2, BOOTLOADER_CHUNK_TIMEOUT_MS)) {
        return false;
    }
    
    if (header[0] != BOOTLOADER_MAGIC_1 || header[1] != BOOTLOADER_MAGIC_2) {
        // Check for end marker
        if (header[0] == BOOTLOADER_END_MAGIC_1 && header[1] == BOOTLOADER_END_MAGIC_2) {
            *chunk_num = 0xFFFFFFFF;
            *chunk_size = 0;
            return true;
        }
        return false;
    }
    
    // Read chunk number (4 bytes, little-endian)
    if (!uart_read_bytes_timeout(&header[2], 4, BOOTLOADER_CHUNK_TIMEOUT_MS)) {
        return false;
    }
    *chunk_num = header[2] | (header[3] << 8) | (header[4] << 16) | (header[5] << 24);
    
    // Read chunk size (2 bytes, little-endian)
    if (!uart_read_bytes_timeout(&header[6], 2, BOOTLOADER_CHUNK_TIMEOUT_MS)) {
        return false;
    }
    *chunk_size = header[6] | (header[7] << 8);
    
    return true;
}

static bool receive_chunk_data(uint8_t* buffer, uint16_t size, uint8_t* checksum) {
    // Read data
    if (!uart_read_bytes_timeout(buffer, size, BOOTLOADER_CHUNK_TIMEOUT_MS)) {
        return false;
    }
    
    // Read checksum
    if (!uart_read_byte_timeout(checksum, BOOTLOADER_CHUNK_TIMEOUT_MS)) {
        return false;
    }
    
    // Verify checksum (XOR of all data bytes)
    uint8_t calculated_checksum = 0;
    for (uint16_t i = 0; i < size; i++) {
        calculated_checksum ^= buffer[i];
    }
    
    if (calculated_checksum != *checksum) {
        return false;
    }
    
    return true;
}

// -----------------------------------------------------------------------------
// Main Bootloader Function
// -----------------------------------------------------------------------------

bootloader_result_t bootloader_receive_firmware(void) {
    g_receiving = true;
    g_total_size = 0;
    g_received_size = 0;
    g_chunk_count = 0;
    
    // Send acknowledgment to ESP32
    uart_write_byte(0xAA);  // Simple ACK byte
    uart_write_byte(0x55);
    
    // Wait a bit for ESP32 to start streaming
    sleep_ms(100);
    
    // Flash write state (page buffering)
    static uint8_t page_buffer[FLASH_PAGE_SIZE];
    static uint32_t page_buffer_offset = 0;
    static uint32_t current_page_start = FLASH_TARGET_OFFSET;
    static uint32_t current_sector = FLASH_TARGET_OFFSET / FLASH_SECTOR_SIZE;
    static bool sector_erased = false;
    
    // Reset page buffer state
    page_buffer_offset = 0;
    current_page_start = FLASH_TARGET_OFFSET;
    current_sector = FLASH_TARGET_OFFSET / FLASH_SECTOR_SIZE;
    sector_erased = false;
    
    absolute_time_t start_time = get_absolute_time();
    
    while (true) {
        // Check timeout
        uint32_t elapsed_ms = to_ms_since_boot(get_absolute_time()) - to_ms_since_boot(start_time);
        if (elapsed_ms > BOOTLOADER_TIMEOUT_MS) {
            uart_write_byte(0xFF);  // Error marker
            uart_write_byte(BOOTLOADER_ERROR_TIMEOUT);
            return BOOTLOADER_ERROR_TIMEOUT;
        }
        
        // Receive chunk header
        uint32_t chunk_num;
        uint16_t chunk_size;
        
        if (!receive_chunk_header(&chunk_num, &chunk_size)) {
            uart_write_byte(0xFF);  // Error marker
            uart_write_byte(BOOTLOADER_ERROR_TIMEOUT);
            return BOOTLOADER_ERROR_TIMEOUT;
        }
        
        // Check for end marker
        if (chunk_num == 0xFFFFFFFF) {
            // End of firmware
            break;
        }
        
        // Validate chunk size
        if (chunk_size == 0 || chunk_size > BOOTLOADER_CHUNK_MAX_SIZE) {
            // Send error indication to ESP32
            uart_write_byte(0xFF);  // Error marker
            uart_write_byte(BOOTLOADER_ERROR_INVALID_SIZE);
            return BOOTLOADER_ERROR_INVALID_SIZE;
        }
        
        // Validate chunk number (must be sequential)
        if (chunk_num != g_chunk_count) {
            // Out of order chunk - send error indication
            uart_write_byte(0xFF);  // Error marker
            uart_write_byte(BOOTLOADER_ERROR_INVALID_CHUNK);
            return BOOTLOADER_ERROR_INVALID_CHUNK;
        }
        
        // Check for flash overflow
        uint32_t total_written = g_received_size + chunk_size;
        uint32_t max_firmware_size = (2 * 1024 * 1024) - FLASH_TARGET_OFFSET;  // Remaining flash space
        if (total_written > max_firmware_size) {
            uart_write_byte(0xFF);  // Error marker
            uart_write_byte(BOOTLOADER_ERROR_INVALID_SIZE);
            return BOOTLOADER_ERROR_INVALID_SIZE;
        }
        
        // Receive chunk data
        uint8_t chunk_data[BOOTLOADER_CHUNK_MAX_SIZE];
        uint8_t checksum;
        
        if (!receive_chunk_data(chunk_data, chunk_size, &checksum)) {
            // Checksum error or timeout - send error indication
            uart_write_byte(0xFF);  // Error marker
            uart_write_byte(BOOTLOADER_ERROR_CHECKSUM);
            return BOOTLOADER_ERROR_CHECKSUM;
        }
        
        // Erase sector if needed (before writing to it)
        uint32_t page_sector = current_page_start / FLASH_SECTOR_SIZE;
        if (page_sector != current_sector || !sector_erased) {
            uint32_t sector_start = page_sector * FLASH_SECTOR_SIZE;
            
            // Validate sector start is within bounds
            if (sector_start < FLASH_TARGET_OFFSET || sector_start > (2 * 1024 * 1024)) {
                uart_write_byte(0xFF);  // Error marker
                uart_write_byte(BOOTLOADER_ERROR_FLASH_WRITE);
                return BOOTLOADER_ERROR_FLASH_WRITE;
            }
            
            if (!flash_erase_sector(sector_start)) {
                uart_write_byte(0xFF);  // Error marker
                uart_write_byte(BOOTLOADER_ERROR_FLASH_ERASE);
                return BOOTLOADER_ERROR_FLASH_ERASE;
            }
            current_sector = page_sector;
            sector_erased = true;
        }
        
        // Write chunk to flash
        // Flash writes must be page-aligned (256 bytes) and sector-erased
        // We'll buffer chunks until we have a full page
        
        // Copy chunk into page buffer
        if (page_buffer_offset + chunk_size <= FLASH_PAGE_SIZE) {
            // Fits in current page
            memcpy(&page_buffer[page_buffer_offset], chunk_data, chunk_size);
            page_buffer_offset += chunk_size;
            
            // If page is full, write it
            if (page_buffer_offset == FLASH_PAGE_SIZE) {
                if (!flash_write_page(current_page_start, page_buffer)) {
                    return BOOTLOADER_ERROR_FLASH_WRITE;
                }
                current_page_start += FLASH_PAGE_SIZE;
                page_buffer_offset = 0;
            }
        } else {
            // Chunk spans page boundary - write current page first
            if (page_buffer_offset > 0) {
                // Fill rest of page with 0xFF
                memset(&page_buffer[page_buffer_offset], 0xFF, FLASH_PAGE_SIZE - page_buffer_offset);
                if (!flash_write_page(current_page_start, page_buffer)) {
                    return BOOTLOADER_ERROR_FLASH_WRITE;
                }
                current_page_start += FLASH_PAGE_SIZE;
            }
            
            // Handle remaining data
            uint16_t remaining = chunk_size;
            uint16_t offset = 0;
            
            while (remaining > 0) {
                uint16_t to_copy = (remaining > FLASH_PAGE_SIZE) ? FLASH_PAGE_SIZE : remaining;
                memcpy(page_buffer, &chunk_data[offset], to_copy);
                
                if (to_copy < FLASH_PAGE_SIZE) {
                    // Partial page - fill rest with 0xFF
                    memset(&page_buffer[to_copy], 0xFF, FLASH_PAGE_SIZE - to_copy);
                    page_buffer_offset = to_copy;
                } else {
                    // Full page - write immediately
                    if (!flash_write_page(current_page_start, page_buffer)) {
                        return BOOTLOADER_ERROR_FLASH_WRITE;
                    }
                    current_page_start += FLASH_PAGE_SIZE;
                    page_buffer_offset = 0;
                }
                
                remaining -= to_copy;
                offset += to_copy;
            }
        }
        
        g_received_size += chunk_size;
        g_chunk_count++;
        
        // Check if we need to erase next sector
        uint32_t next_page_sector = (current_page_start + FLASH_PAGE_SIZE) / FLASH_SECTOR_SIZE;
        if (next_page_sector != current_sector) {
            sector_erased = false;
        }
        
        // Send progress acknowledgment (optional, helps ESP32 know we're alive)
        if (g_chunk_count % 10 == 0) {
            uart_write_byte(0xAA);  // Progress ACK
        }
    }
    
    // Write any remaining partial page
    if (page_buffer_offset > 0) {
        memset(&page_buffer[page_buffer_offset], 0xFF, FLASH_PAGE_SIZE - page_buffer_offset);
        if (!flash_write_page(current_page_start, page_buffer)) {
            return BOOTLOADER_ERROR_FLASH_WRITE;
        }
    }
    
    g_receiving = false;
    
    // Validate we received something
    if (g_received_size == 0) {
        uart_write_byte(0xFF);  // Error marker
        uart_write_byte(BOOTLOADER_ERROR_INVALID_SIZE);
        return BOOTLOADER_ERROR_INVALID_SIZE;
    }
    
    // Verify firmware integrity (CRC32 of entire image)
    // Calculate CRC32 of the staged firmware for verification
    #ifndef XIP_BASE
    #define XIP_BASE 0x10000000
    #endif
    const uint8_t* staged_firmware = (const uint8_t*)(XIP_BASE + FLASH_TARGET_OFFSET);
    uint32_t calculated_crc = crc32_calculate(staged_firmware, g_received_size);
    
    // Log the calculated CRC for debugging (can be verified by ESP32)
    // Note: If ESP32 sends expected CRC in the future, we can compare here
    // For now, we log it and proceed if chunk checksums passed
    printf("Bootloader: Firmware CRC32 = 0x%08lX (size=%lu)\n", 
           (unsigned long)calculated_crc, (unsigned long)g_received_size);
    
    // Send success acknowledgment before copy
    // (after copy, we reboot immediately)
    uart_write_byte(0xAA);
    uart_write_byte(0x55);
    uart_write_byte(0x00);  // Success code
    
    // Small delay to ensure message is sent
    sleep_ms(100);
    
    // Copy firmware from staging area to main area
    // This function does not return - it reboots the device
    copy_firmware_to_main(g_received_size);
    
    // Should never reach here
    return BOOTLOADER_SUCCESS;
}

const char* bootloader_get_status_message(bootloader_result_t result) {
    switch (result) {
        case BOOTLOADER_SUCCESS:
            return "Firmware update successful";
        case BOOTLOADER_ERROR_TIMEOUT:
            return "Bootloader timeout";
        case BOOTLOADER_ERROR_INVALID_MAGIC:
            return "Invalid magic bytes";
        case BOOTLOADER_ERROR_INVALID_SIZE:
            return "Invalid chunk size or flash overflow";
        case BOOTLOADER_ERROR_INVALID_CHUNK:
            return "Invalid or out-of-order chunk";
        case BOOTLOADER_ERROR_CHECKSUM:
            return "Checksum verification failed";
        case BOOTLOADER_ERROR_FLASH_WRITE:
            return "Flash write error";
        case BOOTLOADER_ERROR_FLASH_ERASE:
            return "Flash erase error";
        case BOOTLOADER_ERROR_UNKNOWN:
        default:
            return "Unknown error";
    }
}

