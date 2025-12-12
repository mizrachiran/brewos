/**
 * Pico Firmware - Serial Bootloader
 * RESTORED: The working version, with critical variable fix and safer timing.
 */

 #include "bootloader.h"
 #include "config.h"
 #include "flash_safe.h"       
 #include "safety.h"           
 #include <string.h>
 #include <stdio.h>
 #include "pico/stdlib.h"
 #include "pico/multicore.h"   
 #include "pico/platform.h"
 #include "pico/bootrom.h"     
 #include "hardware/uart.h"
 #include "hardware/flash.h"
 #include "hardware/sync.h"
 #include "hardware/watchdog.h"
 #include "hardware/structs/watchdog.h"
 #include "hardware/structs/scb.h"
 
 // Bootloader protocol constants
 #define BOOTLOADER_MAGIC_1          0x55
 #define BOOTLOADER_MAGIC_2          0xAA
 #define BOOTLOADER_END_MAGIC_1      0xAA
 #define BOOTLOADER_END_MAGIC_2      0x55
 #define BOOTLOADER_CHUNK_MAX_SIZE   256
 #define BOOTLOADER_TIMEOUT_MS       30000 
 #define BOOTLOADER_CHUNK_TIMEOUT_MS 5000   
 #define FLASH_WRITE_RETRIES         3
 
 // Fallback for missing error code in header
 #ifndef BOOTLOADER_ERROR_FAILED
 #define BOOTLOADER_ERROR_FAILED BOOTLOADER_ERROR_FLASH_WRITE
 #endif
 
 // Flash layout
 #define FLASH_TARGET_OFFSET         (1536 * 1024)  // Staging area
 #define FLASH_MAIN_OFFSET           0              // Main firmware area
 
 // Bootloader state
 static uint32_t g_received_size = 0;
 static uint32_t g_chunk_count = 0;
 static bool g_receiving = false; // [FIXED] Restored missing declaration
 static volatile bool g_bootloader_active = false;
 
 // -----------------------------------------------------------------------------
 // BootROM Function Typedefs
 // -----------------------------------------------------------------------------
 typedef void (*rom_connect_internal_flash_fn)(void);
 typedef void (*rom_flash_exit_xip_fn)(void);
 typedef void (*rom_flash_range_erase_fn)(uint32_t addr, size_t count, uint32_t block_size, uint8_t block_cmd);
 typedef void (*rom_flash_range_program_fn)(uint32_t addr, const uint8_t *data, size_t count);
 typedef void (*rom_flash_flush_cache_fn)(void);
 
 typedef struct {
     rom_connect_internal_flash_fn connect_internal_flash;
     rom_flash_exit_xip_fn flash_exit_xip;
     rom_flash_range_erase_fn flash_range_erase;
     rom_flash_range_program_fn flash_range_program;
     rom_flash_flush_cache_fn flash_flush_cache;
 } boot_rom_funcs_t;
 
 // -----------------------------------------------------------------------------
 // Bootloader Mode Control
 // -----------------------------------------------------------------------------
 
 bool bootloader_is_active(void) {
     return g_bootloader_active;
 }
 
 void bootloader_prepare(void) {
     LOG_PRINT("Bootloader: Entering safe state (heaters OFF)\n");
     safety_enter_safe_state();
     g_bootloader_active = true;
     __dmb();
     sleep_ms(100);
     LOG_PRINT("Bootloader: System paused, safe to proceed\n");
 }
 
 void bootloader_exit(void) {
     g_bootloader_active = false;
     __dmb();
 }
 
 // -----------------------------------------------------------------------------
 // Utilities & Protocol Helpers
 // -----------------------------------------------------------------------------
 static uint32_t crc32_calculate(const uint8_t* data, size_t length) {
     uint32_t crc = 0xFFFFFFFF;
     const uint32_t polynomial = 0xEDB88320;
     for (size_t i = 0; i < length; i++) {
         crc ^= data[i];
         for (int j = 0; j < 8; j++) {
             if (crc & 1) crc = (crc >> 1) ^ polynomial;
             else crc >>= 1;
         }
     }
     return ~crc;
 }
 
 static bool uart_read_byte_timeout(uint8_t* byte, uint32_t timeout_ms) {
     absolute_time_t timeout = make_timeout_time_ms(timeout_ms);
     while (!uart_is_readable(ESP32_UART_ID)) {
         if (time_reached(timeout)) return false;
         // [TUNED] Reduced from 100us to 10us. 
         // 100us allows ~11 bytes to arrive at 921k baud, risking FIFO (32 byte) overflow.
         // 10us allows ~1 byte, which is completely safe while still being nice to the CPU.
         sleep_us(10); 
     }
     *byte = uart_getc(ESP32_UART_ID);
     return true;
 }
 
 static bool uart_read_bytes_timeout(uint8_t* buffer, size_t count, uint32_t timeout_ms) {
     absolute_time_t start = get_absolute_time();
     for (size_t i = 0; i < count; i++) {
         uint32_t elapsed_ms = to_ms_since_boot(get_absolute_time()) - to_ms_since_boot(start);
         uint32_t remaining = timeout_ms > elapsed_ms ? timeout_ms - elapsed_ms : 100;
         if (!uart_read_byte_timeout(&buffer[i], remaining)) return false;
     }
     return true;
 }
 
 static void uart_write_byte(uint8_t byte) {
     uart_putc(ESP32_UART_ID, byte);
 }
 
 static bool receive_chunk_header(uint32_t* chunk_num, uint16_t* chunk_size) {
     absolute_time_t timeout_time = make_timeout_time_ms(BOOTLOADER_CHUNK_TIMEOUT_MS);
     while (!time_reached(timeout_time)) {
         uint8_t b1;
         if (!uart_read_byte_timeout(&b1, 100)) continue;
         
         if (b1 == BOOTLOADER_MAGIC_1) { 
             uint8_t b2;
             if (!uart_read_byte_timeout(&b2, 100)) continue;
             if (b2 == BOOTLOADER_MAGIC_2) { 
                 uint8_t h[6];
                 if (!uart_read_bytes_timeout(h, 6, BOOTLOADER_CHUNK_TIMEOUT_MS)) return false;
                 *chunk_num = h[0] | (h[1]<<8) | (h[2]<<16) | (h[3]<<24);
                 *chunk_size = h[4] | (h[5]<<8);
                 return true;
             }
         } else if (b1 == BOOTLOADER_END_MAGIC_1) {
             uint8_t b2;
             if (!uart_read_byte_timeout(&b2, 100)) continue;
             if (b2 == BOOTLOADER_END_MAGIC_2) {
                 uint8_t b3;
                 if (!uart_read_byte_timeout(&b3, 200)) { *chunk_num = 0xFFFFFFFF; return true; }
                 if (b3 == BOOTLOADER_MAGIC_2) continue; 
                 *chunk_num = 0xFFFFFFFF; return true;
             }
         }
     }
     return false;
 }
 
 static bool receive_chunk_data(uint8_t* buffer, uint16_t size, uint8_t* checksum) {
     if (!uart_read_bytes_timeout(buffer, size, BOOTLOADER_CHUNK_TIMEOUT_MS)) return false;
     if (!uart_read_byte_timeout(checksum, BOOTLOADER_CHUNK_TIMEOUT_MS)) return false;
     uint8_t calc = 0;
     for(int i=0; i<size; i++) calc ^= buffer[i];
     return (calc == *checksum);
 }
 
 // -----------------------------------------------------------------------------
 // CRITICAL: Safe Flash Copy (RAM Only)
 // -----------------------------------------------------------------------------
 
 static uint8_t g_sector_buffer[FLASH_SECTOR_SIZE] __attribute__((aligned(16)));
 
 /**
  * Copy firmware using ONLY BootROM functions.
  * - Runs entirely from RAM
  * - Disables interrupts
  * - Verifies write success
  * - Handles watchdog manually
  */
 static void __no_inline_not_in_flash_func(copy_firmware_to_main)(const boot_rom_funcs_t* rom, uint32_t firmware_size) {
     // 1. DISABLE INTERRUPTS GLOBALLY
     save_and_disable_interrupts();
     
     // Calculate iterations
     uint32_t size_sectors = (firmware_size + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE;
     const uint8_t* staging_base = (const uint8_t*)(XIP_BASE + FLASH_TARGET_OFFSET);
 
     for (uint32_t sector = 0; sector < size_sectors; sector++) {
         // [WATCHDOG] Feed manually via register write (safe in RAM)
         // SDK's watchdog_update() is in Flash and cannot be called.
         watchdog_hw->load = 0x7fffff; // Load large value (~8.3s at 1MHz)
         
         uint32_t offset = sector * FLASH_SECTOR_SIZE;
         
         // [READ] Copy from Staging to RAM buffer (Manual loop)
         for (int i = 0; i < FLASH_SECTOR_SIZE; i++) {
             g_sector_buffer[i] = staging_base[offset + i];
         }
 
         bool verify_success = false;
         
         // [WRITE & VERIFY] Loop with retries
         for (int retry = 0; retry < FLASH_WRITE_RETRIES; retry++) {
             // A. Connect Flash (Command Mode) - Disables XIP
             rom->connect_internal_flash();
 
             // B. Erase Sector (0x20 = 4KB Sector Erase)
             rom->flash_range_erase(FLASH_MAIN_OFFSET + offset, FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE, 0x20);
 
             // C. Program Sector
             rom->flash_range_program(FLASH_MAIN_OFFSET + offset, g_sector_buffer, FLASH_SECTOR_SIZE);
 
             // D. Restore XIP Mode (Required to read back for verification)
             rom->flash_flush_cache();
             rom->flash_exit_xip();
             
             // E. Verification
             const uint8_t* written_ptr = (const uint8_t*)(XIP_BASE + FLASH_MAIN_OFFSET + offset);
             bool match = true;
             for (int v = 0; v < FLASH_SECTOR_SIZE; v++) {
                 if (written_ptr[v] != g_sector_buffer[v]) {
                     match = false;
                     break;
                 }
             }
             
             if (match) {
                 verify_success = true;
                 break; // Exit retry loop
             }
         }
         
         // If verification failed 3 times, we can't do much but try to reset.
         if (!verify_success) {
             break; // Proceed to reset, hope for the best
         }
     }
 
     // 4. Hard Reset via AIRCR
     __dmb();
     *((volatile uint32_t *)0xE000ED0C) = 0x05FA0004;
     
     while(1) __asm volatile("nop");
 }
 
 // -----------------------------------------------------------------------------
 // Main Receive Loop
 // -----------------------------------------------------------------------------
 
 bootloader_result_t bootloader_receive_firmware(void) {
     g_receiving = true; // [FIXED] Variable set, no longer causes error
     g_received_size = 0;
     g_chunk_count = 0;
     
     // Flush UART
     while (uart_is_readable(ESP32_UART_ID)) (void)uart_getc(ESP32_UART_ID);
     
     // Send ACK
     static const uint8_t BOOT_ACK[] = {0xB0, 0x07, 0xAC, 0x4B};
     for (size_t i = 0; i < sizeof(BOOT_ACK); i++) uart_write_byte(BOOT_ACK[i]);
     uart_tx_wait_blocking(ESP32_UART_ID);
     LOG_PRINT("Bootloader: ACK sent, waiting for firmware...\n");
     
     // Use SDK Flash Safe functions for the download phase
     static uint8_t page_buffer[FLASH_PAGE_SIZE];
     uint32_t page_buffer_offset = 0;
     uint32_t current_page_start = FLASH_TARGET_OFFSET;
     uint32_t current_sector = FLASH_TARGET_OFFSET / FLASH_SECTOR_SIZE;
     bool sector_erased = false;
     
     absolute_time_t start_time = get_absolute_time();
     watchdog_enable(BOOTLOADER_CHUNK_TIMEOUT_MS + 2000, 1);
     
     while (true) {
         watchdog_update();
         
         if (absolute_time_diff_us(start_time, get_absolute_time()) > BOOTLOADER_TIMEOUT_MS * 1000) {
             uart_write_byte(0xFF); uart_write_byte(BOOTLOADER_ERROR_TIMEOUT);
             return BOOTLOADER_ERROR_TIMEOUT;
         }
         
         uint32_t chunk_num; uint16_t chunk_size;
         if (!receive_chunk_header(&chunk_num, &chunk_size)) {
             uart_write_byte(0xFF); uart_write_byte(BOOTLOADER_ERROR_TIMEOUT);
             return BOOTLOADER_ERROR_TIMEOUT;
         }
         
         if (chunk_num == 0xFFFFFFFF) break; // End marker
         
         if (chunk_size == 0 || chunk_size > BOOTLOADER_CHUNK_MAX_SIZE || chunk_num != g_chunk_count) {
             uart_write_byte(0xFF); uart_write_byte(BOOTLOADER_ERROR_INVALID_SIZE);
             return BOOTLOADER_ERROR_INVALID_SIZE;
         }
         
         uint8_t chunk_data[BOOTLOADER_CHUNK_MAX_SIZE];
         uint8_t csum;
         if (!receive_chunk_data(chunk_data, chunk_size, &csum)) {
             uart_write_byte(0xFF); uart_write_byte(BOOTLOADER_ERROR_CHECKSUM);
             return BOOTLOADER_ERROR_CHECKSUM;
         }
         
         if (g_chunk_count % 50 == 0) {
             LOG_PRINT("Bootloader: Chunk %lu (%u bytes)\n", (unsigned long)g_chunk_count, chunk_size);
         }
         
         uint32_t offset = 0;
         while (offset < chunk_size) {
             uint32_t space = FLASH_PAGE_SIZE - page_buffer_offset;
             uint32_t copy = (chunk_size - offset < space) ? (chunk_size - offset) : space;
             memcpy(page_buffer + page_buffer_offset, chunk_data + offset, copy);
             page_buffer_offset += copy; 
             offset += copy;
             
             if (page_buffer_offset >= FLASH_PAGE_SIZE) {
                 uint32_t sect_start = current_page_start & ~(FLASH_SECTOR_SIZE - 1);
                 if (!sector_erased || (sect_start != current_sector)) {
                     if (!flash_safe_erase(sect_start, FLASH_SECTOR_SIZE)) return BOOTLOADER_ERROR_FLASH_ERASE;
                     sector_erased = true;
                     current_sector = sect_start;
                 }
                 if (!flash_safe_program(current_page_start, page_buffer, FLASH_PAGE_SIZE)) return BOOTLOADER_ERROR_FLASH_WRITE;
                 current_page_start += FLASH_PAGE_SIZE;
                 page_buffer_offset = 0;
             }
         }
         
         g_received_size += chunk_size;
         g_chunk_count++;
         uart_write_byte(0xAA);
         uart_tx_wait_blocking(ESP32_UART_ID);
     }
     
     if (page_buffer_offset > 0) {
         uint32_t sect_start = current_page_start & ~(FLASH_SECTOR_SIZE - 1);
         if (!sector_erased || (sect_start != current_sector)) {
             flash_safe_erase(sect_start, FLASH_SECTOR_SIZE);
         }
         memset(&page_buffer[page_buffer_offset], 0xFF, FLASH_PAGE_SIZE - page_buffer_offset);
         flash_safe_program(current_page_start, page_buffer, FLASH_PAGE_SIZE);
     }
     
     const uint8_t* staged = (const uint8_t*)(XIP_BASE + FLASH_TARGET_OFFSET);
     uint32_t crc = crc32_calculate(staged, g_received_size);
     LOG_PRINT("Bootloader: CRC32=0x%08lX (size=%lu)\n", (unsigned long)crc, (unsigned long)g_received_size);
     
     uart_write_byte(0xAA); uart_write_byte(0x55); uart_write_byte(0x00);
     sleep_ms(50);
     
     LOG_PRINT("Bootloader: Starting flash copy. USB will disconnect...\n");
     sleep_ms(50);
     
     // Resolve ROM Pointers
     boot_rom_funcs_t rom_funcs;
     rom_funcs.connect_internal_flash = (rom_connect_internal_flash_fn)rom_func_lookup(ROM_FUNC_CONNECT_INTERNAL_FLASH);
     rom_funcs.flash_exit_xip = (rom_flash_exit_xip_fn)rom_func_lookup(ROM_FUNC_FLASH_EXIT_XIP);
     rom_funcs.flash_range_erase = (rom_flash_range_erase_fn)rom_func_lookup(ROM_FUNC_FLASH_RANGE_ERASE);
     rom_funcs.flash_range_program = (rom_flash_range_program_fn)rom_func_lookup(ROM_FUNC_FLASH_RANGE_PROGRAM);
     rom_funcs.flash_flush_cache = (rom_flash_flush_cache_fn)rom_func_lookup(ROM_FUNC_FLASH_FLUSH_CACHE);
 
     if (!rom_funcs.connect_internal_flash || !rom_funcs.flash_range_erase || !rom_funcs.flash_range_program) {
         LOG_PRINT("CRITICAL: Failed to resolve BootROM functions!\n");
         return BOOTLOADER_ERROR_FAILED;
     }
 
     watchdog_enable(8300, 1);
     
     // Transfer control to RAM - NO RETURN
     copy_firmware_to_main(&rom_funcs, g_received_size);
     
     return BOOTLOADER_SUCCESS;
 }
 
 const char* bootloader_get_status_message(bootloader_result_t result) {
     return (result == BOOTLOADER_SUCCESS) ? "Success" : "Error";
 }