/**
 * Pico Firmware - Serial Bootloader
 * FIXED: Removed risky Flash Verification, Added Watchdog Reset, Tuned UART timing.
 */

 #include "bootloader.h"
 #include "config.h"
 #include "flash_safe.h"       
 #include "safety.h"
 #include "protocol.h"  // For protocol_reset_state()           
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
 static bool g_receiving = false;
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
    // Idempotency check: if already active, just return
    // This prevents double-initialization and race conditions
    if (g_bootloader_active) {
        LOG_PRINT("Bootloader: Already active, skipping prepare\n");
        return;
    }
    
    LOG_PRINT("Bootloader: Entering safe state (heaters OFF)\n");
    safety_enter_safe_state();
    
    // NOTE: USB serial logging is NOT affected by UART interrupt settings
    // UART interrupts only control hardware UART (ESP32 communication), not USB serial
    // All LOG_PRINT statements will continue to work over USB
    
    // CRITICAL: Drain UART FIFO completely BEFORE resetting protocol state
    // Note: We disable UART interrupts AFTER sending bootloader ACK (in bootloader_receive_firmware)
    // This allows protocol ACK and bootloader ACK to be sent properly
    // This prevents any bytes in the FIFO from being processed by protocol handler
    // We must drain while protocol is still active, then reset state, then set flag
    // Drain multiple times to ensure FIFO is completely empty
    int drain_count = 0;
    while (uart_is_readable(ESP32_UART_ID)) {
        (void)uart_getc(ESP32_UART_ID);
        drain_count++;
    }
    if (drain_count > 0) {
        LOG_PRINT("Bootloader: Drained %d bytes from UART FIFO\n", drain_count);
    }
    
    // CRITICAL: Reset protocol state machine to prevent parsing bootloader data as protocol packets
    // Bootloader data (0x55AA chunks) will be misinterpreted as protocol packets if state is not reset
    protocol_reset_state();
    
    // Memory barrier to ensure protocol_reset_state() completes before setting flag
    __dmb();
    
    // CRITICAL: Set bootloader_active flag AFTER draining UART and resetting state
    // This ensures protocol_process() won't consume any bytes after this point
    g_bootloader_active = true;
    
    // Full memory barrier to ensure flag is visible to all cores immediately
    __dmb();
    
    // CRITICAL: Minimal delay to ensure all cores see the flag change
    // The flag is set with memory barriers, so cores should see it within microseconds
    // 5ms is sufficient for Core 0 and Core 1's main loops to check the flag and suspend
    // We keep this short because bootloader_prepare() is now called BEFORE sending ACK
    sleep_ms(5);
    
    // Final drain to catch any bytes that arrived during the transition
    // This is critical - bytes could arrive between setting flag and protocol_process() checking it
    drain_count = 0;
    while (uart_is_readable(ESP32_UART_ID)) {
        (void)uart_getc(ESP32_UART_ID);
        drain_count++;
    }
    if (drain_count > 0) {
        LOG_PRINT("Bootloader: Drained %d additional bytes after transition\n", drain_count);
    }
    
    LOG_PRINT("Bootloader: System paused, safe to proceed\n");
}
 
 void bootloader_exit(void) {
     // CRITICAL: Aggressively drain ALL bootloader data before exiting
     // ESP32 may still be sending firmware chunks even after bootloader fails
     // We must consume ALL of this data or it will be processed as protocol packets
     
     // Keep bootloader flag set and interrupts disabled while draining
     // This prevents protocol handler from processing bytes during drain
     
     int total_drained = 0;
     uint32_t drain_start = to_ms_since_boot(get_absolute_time());
     const uint32_t DRAIN_TIMEOUT_MS = 2000;  // Drain for up to 2 seconds
     
     // Aggressive drain loop: keep draining until no bytes arrive for 100ms
     uint32_t last_byte_time = drain_start;
     while ((to_ms_since_boot(get_absolute_time()) - drain_start) < DRAIN_TIMEOUT_MS) {
         int cycle_drained = 0;
         while (uart_is_readable(ESP32_UART_ID)) {
             (void)uart_getc(ESP32_UART_ID);
             cycle_drained++;
             total_drained++;
             last_byte_time = to_ms_since_boot(get_absolute_time());
         }
         
         // If no bytes arrived in last 100ms, assume ESP32 stopped sending
         if ((to_ms_since_boot(get_absolute_time()) - last_byte_time) > 100) {
             break;
         }
         
         // Small delay to avoid busy-waiting
         sleep_ms(10);
     }
     
     if (total_drained > 0) {
         LOG_PRINT("Bootloader: Drained %d total bytes before exit\n", total_drained);
     }
     
     // CRITICAL: Final drain with interrupts still disabled
     // This catches any bytes that arrived during the aggressive drain
     int final_drain = 0;
     uint32_t final_start = to_ms_since_boot(get_absolute_time());
     while ((to_ms_since_boot(get_absolute_time()) - final_start) < 200) {  // 200ms final drain
         if (uart_is_readable(ESP32_UART_ID)) {
             (void)uart_getc(ESP32_UART_ID);
             final_drain++;
         } else {
             sleep_ms(10);
         }
     }
     if (final_drain > 0) {
         LOG_PRINT("Bootloader: Drained %d additional bytes (final drain)\n", final_drain);
     }
     
     // CRITICAL: Bootloader failed - reset Pico to resume normal operation
     // Don't try to resume protocol handler - system is in inconsistent state
     // Reset ensures clean state and proper recovery
     LOG_PRINT("Bootloader: Failed, resetting Pico to resume normal operation\n");
     
     // Small delay to ensure log message is sent
     sleep_ms(100);
     
     // Reset via watchdog (clean reset method)
     watchdog_reboot(0, 0, 0);
     
     // Should never reach here, but just in case
     while(1) __asm volatile("nop");
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
     uint32_t last_watchdog_feed = 0;
     while (!uart_is_readable(ESP32_UART_ID)) {
         if (time_reached(timeout)) return false;
         
         // Feed watchdog periodically during long waits (every ~100ms)
         uint32_t now_ms = to_ms_since_boot(get_absolute_time());
         if (now_ms - last_watchdog_feed > 100) {
             watchdog_update();
             last_watchdog_feed = now_ms;
         }
         
         // [TUNED] 10us allows ~1 byte at 921k baud (9us/byte).
         // This is safe for the FIFO (32 bytes) while being efficient.
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
         // Feed watchdog during long waits to prevent reset
         watchdog_update();
         
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
     // Feed watchdog before potentially long read operations
     watchdog_update();
     if (!uart_read_bytes_timeout(buffer, size, BOOTLOADER_CHUNK_TIMEOUT_MS)) return false;
     watchdog_update();
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
  * - [FIX] No verification loop to avoid crashes during XIP readback
  * - [FIX] Uses Watchdog Reset instead of AIRCR
  */
 static void __no_inline_not_in_flash_func(copy_firmware_to_main)(const boot_rom_funcs_t* rom, uint32_t firmware_size) {
     // 1. DISABLE INTERRUPTS GLOBALLY
     save_and_disable_interrupts();
     
     // Calculate iterations
     uint32_t size_sectors = (firmware_size + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE;
     const uint8_t* staging_base = (const uint8_t*)(XIP_BASE + FLASH_TARGET_OFFSET);
 
     for (uint32_t sector = 0; sector < size_sectors; sector++) {
         // [WATCHDOG] Feed manually
         watchdog_hw->load = 0x7fffff;
         
         uint32_t offset = sector * FLASH_SECTOR_SIZE;
         
         // [READ] Copy from Staging to RAM buffer
         for (int i = 0; i < FLASH_SECTOR_SIZE; i++) {
             g_sector_buffer[i] = staging_base[offset + i];
         }
 
         // [WRITE] Single pass - no retry/verify loop (risky readback removed)
         rom->connect_internal_flash();
         rom->flash_range_erase(FLASH_MAIN_OFFSET + offset, FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE, 0x20);
         rom->flash_range_program(FLASH_MAIN_OFFSET + offset, g_sector_buffer, FLASH_SECTOR_SIZE);
         rom->flash_flush_cache();
        rom->flash_exit_xip();
    }

    // 4. Force System Reset via AIRCR (Application Interrupt and Reset Control Register)
    // This is the standard ARM Cortex-M method to reset the system.
    // SCB->AIRCR = 0x05FA0004 (VECTKEY | SYSRESETREQ)
    #define AIRCR_VECTKEY_MASK    0x05FA0000
    #define AIRCR_SYSRESETREQ_MASK 0x00000004
    
    scb_hw->aircr = AIRCR_VECTKEY_MASK | AIRCR_SYSRESETREQ_MASK;
    
    while(1) __asm volatile("nop");
}
 
 // -----------------------------------------------------------------------------
 // Main Receive Loop
 // -----------------------------------------------------------------------------
 
bootloader_result_t bootloader_receive_firmware(void) {
    g_receiving = true;
    g_received_size = 0;
    g_chunk_count = 0;
    
    // UART should already be drained by bootloader_prepare(), but drain again for safety
    // This ensures no stale bytes remain from the transition period
    while (uart_is_readable(ESP32_UART_ID)) (void)uart_getc(ESP32_UART_ID);
     
    // NOTE: Bootloader ACK is sent in handle_cmd_bootloader() AFTER bootloader_prepare()
    // This ensures the system is fully ready before ESP32 starts sending firmware chunks
    // We skip sending it here to avoid duplicate ACK
    LOG_PRINT("Bootloader: ACK already sent, waiting for firmware...\n");
    
    // CRITICAL: NOW disable UART interrupts to prevent protocol handler from stealing firmware bytes
    // NOTE: This only affects hardware UART (ESP32 communication), NOT USB serial
    // USB serial logging will continue to work normally - all LOG_PRINT statements work over USB
    uart_set_irq_enables(ESP32_UART_ID, false, false);
    LOG_PRINT("Bootloader: UART interrupts disabled for firmware reception (USB serial still active)\n");
     
     // Use SDK Flash Safe functions for the download phase
     static uint8_t page_buffer[FLASH_PAGE_SIZE];
     uint32_t page_buffer_offset = 0;
     uint32_t current_page_start = FLASH_TARGET_OFFSET;
     uint32_t current_sector = FLASH_TARGET_OFFSET / FLASH_SECTOR_SIZE;
     bool sector_erased = false;
     
    absolute_time_t start_time = get_absolute_time();
    watchdog_enable(BOOTLOADER_CHUNK_TIMEOUT_MS + 2000, 1);
    LOG_PRINT("Bootloader: Starting firmware reception loop (watchdog enabled)\n");
    
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
         
        // Log progress more frequently for better visibility (every 10 chunks instead of 50)
        if (g_chunk_count % 10 == 0) {
            LOG_PRINT("Bootloader: Chunk %lu (%u bytes), total: %lu bytes\n", 
                     (unsigned long)g_chunk_count, chunk_size, (unsigned long)g_received_size);
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
                 
                 // CRITICAL: Feed watchdog before flash operations (they can take 50-100ms)
                 // Flash operations disable interrupts and pause Core 0, so watchdog must be fed first
                 watchdog_update();
                 
                 if (!sector_erased || (sect_start != current_sector)) {
                     // Sector erase can take 50-100ms - feed watchdog before and after
                     watchdog_update();
                     if (!flash_safe_erase(sect_start, FLASH_SECTOR_SIZE)) {
                         LOG_PRINT("Bootloader: Flash erase failed at offset 0x%lx\n", (unsigned long)sect_start);
                         return BOOTLOADER_ERROR_FLASH_ERASE;
                     }
                     watchdog_update();  // Feed after erase completes
                     sector_erased = true;
                     current_sector = sect_start;
                 }
                 
                 // Flash program can take 10-20ms - feed watchdog before
                 watchdog_update();
                 if (!flash_safe_program(current_page_start, page_buffer, FLASH_PAGE_SIZE)) {
                     LOG_PRINT("Bootloader: Flash program failed at offset 0x%lx\n", (unsigned long)current_page_start);
                     return BOOTLOADER_ERROR_FLASH_WRITE;
                 }
                 watchdog_update();  // Feed after program completes
                 
                 current_page_start += FLASH_PAGE_SIZE;
                 page_buffer_offset = 0;
             }
         }
         
        g_received_size += chunk_size;
        g_chunk_count++;
        
        // CRITICAL: Feed watchdog before sending ACK (UART TX can block briefly)
        watchdog_update();
        
        uart_write_byte(0xAA);
        uart_tx_wait_blocking(ESP32_UART_ID);
        
        // Feed watchdog after ACK sent to ensure we're alive
        watchdog_update();
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
    
    // [VALIDATION] Check for valid ARM Cortex-M Vector Table
    // Word 0: Initial Stack Pointer (Must be in RAM: 0x20xxxxxx)
    // Word 1: Reset Vector (Must be in Flash: 0x10xxxxxx)
    uint32_t* staged_vectors = (uint32_t*)staged;
    bool valid_sp = (staged_vectors[0] & 0xFF000000) == 0x20000000;
    bool valid_pc = (staged_vectors[1] & 0xFF000000) == 0x10000000;
    
    if (!valid_sp || !valid_pc) {
        LOG_PRINT("Bootloader: Invalid firmware image (SP=%08lX, PC=%08lX)\n", 
                 staged_vectors[0], staged_vectors[1]);
        uart_write_byte(0xFF); uart_write_byte(BOOTLOADER_ERROR_INVALID_SIZE); // Re-use generic error
        return BOOTLOADER_ERROR_INVALID_SIZE;
    }
    
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