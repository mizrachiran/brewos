/**
 * BrewOS Display Driver Implementation
 * 
 * Uses ESP-IDF LCD Panel API directly (same as working VIEWESMART BSP)
 * Based on: UEDX48480021-MD80ESP32_2.1inch-Knob/examples/ESP-IDF/UEDX48480021-MD80E-SDK/components/bsp/sub_board/bsp_lcd.c
 * 
 * KEY: Uses on_frame_trans_done callback to synchronize LVGL flushes with DMA transfers.
 */

#include "display/display.h"
#include "display/display_config.h"
#include "display/lv_fs_littlefs.h"
#include "config.h"

#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <esp_rom_sys.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// Global display instance
Display display;

// LCD panel handle
static esp_lcd_panel_handle_t panel_handle = NULL;

// Semaphore for synchronizing LVGL flushes with DMA transfers
static SemaphoreHandle_t s_flush_sem = NULL;

// LVGL display driver for callback
static lv_disp_drv_t* s_disp_drv = NULL;

// Frame transfer done callback - called by DMA when a frame is complete
// ESP-IDF 5.x: event_data is now const
static IRAM_ATTR bool on_frame_trans_done(esp_lcd_panel_handle_t panel, 
                                           const esp_lcd_rgb_panel_event_data_t *edata, 
                                           void *user_ctx) {
    BaseType_t need_yield = pdFALSE;
    if (s_flush_sem) {
        xSemaphoreGiveFromISR(s_flush_sem, &need_yield);
    }
    return (need_yield == pdTRUE);
}

// =============================================================================
// 3-Wire SPI Bit-Bang (exact copy from working BSP)
// =============================================================================

#define LCD_SPI_CS   18
#define LCD_SPI_SCK  13
#define LCD_SPI_SDO  12
#define LCD_RST      8

#define udelay(t) esp_rom_delay_us(t)

static void spi_write_9bit(uint16_t data) {
    for (uint8_t n = 0; n < 9; n++) {
        gpio_set_level((gpio_num_t)LCD_SPI_SDO, (data & 0x0100) ? 1 : 0);
        data = data << 1;
        
        gpio_set_level((gpio_num_t)LCD_SPI_SCK, 0);
        udelay(10);
        gpio_set_level((gpio_num_t)LCD_SPI_SCK, 1);
        udelay(10);
    }
}

static void spi_write_cmd(uint8_t cmd) {
    gpio_set_level((gpio_num_t)LCD_SPI_CS, 0);
    udelay(10);
    spi_write_9bit(cmd & 0x00FF);  // D/C bit = 0 for command
    udelay(10);
    gpio_set_level((gpio_num_t)LCD_SPI_CS, 1);
    gpio_set_level((gpio_num_t)LCD_SPI_SCK, 1);
    gpio_set_level((gpio_num_t)LCD_SPI_SDO, 1);
    udelay(10);
}

static void spi_write_data(uint8_t data) {
    gpio_set_level((gpio_num_t)LCD_SPI_CS, 0);
    udelay(10);
    spi_write_9bit(0x0100 | data);  // D/C bit = 1 for data
    udelay(10);
    gpio_set_level((gpio_num_t)LCD_SPI_CS, 1);
    gpio_set_level((gpio_num_t)LCD_SPI_SCK, 1);
    gpio_set_level((gpio_num_t)LCD_SPI_SDO, 1);
    udelay(10);
}

// =============================================================================
// ST7701S Init Commands (EXACT copy from working washer.bin bsp_lcd.c)
// ALL commands in ONE table, including Sleep Out (0x11)
// =============================================================================

typedef struct {
    uint8_t cmd;
    uint8_t data[52];
    uint8_t data_bytes;  // 0xFF = end of commands
} lcd_init_cmd_t;

// EXACT command sequence from reference_local/UEDX48480021-MD80ESP32_2.1inch-Knob/examples/ESP-IDF/UEDX48480021-MD80E-SDK/components/bsp/sub_board/bsp_lcd.c
static const lcd_init_cmd_t LCD_INIT_CMDS[] = {
    {0xFF, {0x77,0x01,0x00,0x00,0x13}, 5},
    {0xEF, {0x08}, 1},
    {0xFF, {0x77,0x01,0x00,0x00,0x10}, 5},
    {0xC0, {0x3B,0x00}, 2},
    {0xC1, {0x0B,0x02}, 2},
    {0xC2, {0x07,0x02}, 2},
    {0xC7, {0x00}, 1},
    {0xCC, {0x10}, 1},
    {0xCD, {0x08}, 1},
    {0xB0, {0x00,0x11,0x16,0x0E,0x11,0x06,0x05,0x09,0x08,0x21,0x06,0x13,0x10,0x29,0x31,0x18}, 16},
    {0xB1, {0x00,0x11,0x16,0x0E,0x11,0x07,0x05,0x09,0x09,0x21,0x05,0x13,0x11,0x2A,0x31,0x18}, 16},
    {0xFF, {0x77,0x01,0x00,0x00,0x11}, 5},
    {0xB0, {0x6D}, 1},
    {0xB1, {0x37}, 1},
    {0xB2, {0x8B}, 1},
    {0xB3, {0x80}, 1},
    {0xB5, {0x43}, 1},
    {0xB7, {0x85}, 1},
    {0xB8, {0x20}, 1},
    {0xC0, {0x09}, 1},
    {0xC1, {0x78}, 1},
    {0xC2, {0x78}, 1},
    {0xD0, {0x88}, 1},
    {0xE0, {0x00,0x00,0x02}, 3},
    {0xE1, {0x03,0xA0,0x00,0x00,0x04,0xA0,0x00,0x00,0x00,0x20,0x20}, 11},
    {0xE2, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, 13},
    {0xE3, {0x00,0x00,0x11,0x00}, 4},
    {0xE4, {0x22,0x00}, 2},
    {0xE5, {0x05,0xEC,0xF6,0xCA,0x07,0xEE,0xF6,0xCA,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, 16},
    {0xE6, {0x00,0x00,0x11,0x00}, 4},
    {0xE7, {0x22,0x00}, 2},
    {0xE8, {0x06,0xED,0xF6,0xCA,0x08,0xEF,0xF6,0xCA,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, 16},
    {0xE9, {0x36,0x00}, 2},
    {0xEB, {0x00,0x00,0x40,0x40,0x00,0x00,0x00}, 7},
    {0xED, {0xFF,0xFF,0xFF,0xBA,0x0A,0xFF,0x45,0xFF,0xFF,0x54,0xFF,0xA0,0xAB,0xFF,0xFF,0xFF}, 16},
    {0xEF, {0x08,0x08,0x08,0x45,0x3F,0x54}, 6},
    {0xFF, {0x77,0x01,0x00,0x00,0x13}, 5},
    {0xE8, {0x00,0x0E}, 2},
    {0xFF, {0x77,0x01,0x00,0x00,0x00}, 5},
    {0x11, {0x00}, 1},   // Sleep Out with data byte (CRITICAL: must have data byte!)
    {0xFF, {0x77,0x01,0x00,0x00,0x13}, 5},
    {0xE8, {0x00,0x0C}, 2},
    {0xE8, {0x00,0x00}, 2},
    {0xFF, {0x77,0x01,0x00,0x00,0x00}, 5},
    {0x36, {0x00}, 1},
    {0x3A, {0x66}, 1},   // RGB666 color format (0x66)
    {0x00, {0x00}, 0xFF}, // End marker
};

static void send_lcd_init_commands(void) {
    // Configure SPI pins (EXACT copy from working bsp_lcd.c)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LCD_SPI_CS) | (1ULL << LCD_SPI_SCK) | (1ULL << LCD_SPI_SDO) | (1ULL << LCD_RST) | (1ULL << 7), // Include BL pin
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    // Initial pin states (same as reference - NO hardware reset toggle!)
    gpio_set_level((gpio_num_t)LCD_RST, 1);
    gpio_set_level((gpio_num_t)LCD_SPI_CS, 1);
    gpio_set_level((gpio_num_t)LCD_SPI_SCK, 1);
    gpio_set_level((gpio_num_t)LCD_SPI_SDO, 1);
    
    LOG_I("Sending ST7701S init commands (washer.bin sequence)...");
    
    // Send ALL init commands in one loop (EXACT match to reference)
    uint8_t i = 0;
    while (LCD_INIT_CMDS[i].data_bytes != 0xFF) {
        // Skip the 0x36 (rotation) command - we'll send it separately with the correct value
        if (LCD_INIT_CMDS[i].cmd == 0x36) {
            i++;
            continue;
        }
        spi_write_cmd(LCD_INIT_CMDS[i].cmd);
        for (uint8_t j = 0; j < LCD_INIT_CMDS[i].data_bytes; j++) {
            spi_write_data(LCD_INIT_CMDS[i].data[j]);
        }
        i++;
    }
    
    // Wait 120ms after all commands (including Sleep Out which is now in the table)
    vTaskDelay(pdMS_TO_TICKS(120));
    
    // Set display rotation via 0x36 (Memory Access Control) command
    // ST7701S rotation values:
    // 0°:   0x00 (normal)
    // 90°:  0x60 (MV + MX)
    // 180°: 0xC0 (MX + MY)
    // 270°: 0xA0 (MV + MY)
    uint8_t rotation_value = 0x00;
    switch (DISPLAY_ROTATION) {
        case 0:
            rotation_value = 0x00;
            break;
        case 90:
            rotation_value = 0x60;
            break;
        case 180:
            rotation_value = 0xC0;
            break;
        case 270:
            rotation_value = 0xA0;
            break;
        default:
            LOG_W("Invalid rotation %d, using 0°", DISPLAY_ROTATION);
            rotation_value = 0x00;
            break;
    }
    spi_write_cmd(0x36);
    spi_write_data(rotation_value);
    LOG_I("Display rotation set to %d° (0x36=0x%02X)", DISPLAY_ROTATION, rotation_value);
    
    // Display ON command (0x29)
    spi_write_cmd(0x29);
    
    // Wait 20ms (reference uses 20ms, not 120ms!)
    vTaskDelay(pdMS_TO_TICKS(20));
    
    // Reset SPI pins so they can be used as RGB data pins
    // Reference: gpio_reset_pin(BSP_LCD_SPI_SCK); gpio_reset_pin(BSP_LCD_SPI_SDO);
    // Note: Reference does NOT reset CS pin
    gpio_reset_pin((gpio_num_t)LCD_SPI_SCK);  // GPIO13 -> DATA3
    gpio_reset_pin((gpio_num_t)LCD_SPI_SDO);  // GPIO12 -> DATA2
    
    LOG_I("ST7701S init commands sent, SPI pins released for RGB mode");
}

// =============================================================================
// Display Class Implementation
// =============================================================================

Display::Display()
    : _display(nullptr)
    , _buf1(nullptr)
    , _buf2(nullptr)
    , _backlightLevel(BACKLIGHT_DEFAULT)
    , _backlightSaved(BACKLIGHT_DEFAULT)
    , _isDimmed(false)
    , _lastActivityTime(0)
    , _lvglTaskHandle(nullptr) {
}

bool Display::begin() {
    LOG_I("Initializing display...");
    
    initHardware();
    initLVGL();
    
    // Keep backlight at full brightness (already set LOW = ON in initHardware)
    // Don't call setBacklight() here as it may conflict with the GPIO
    _backlightLevel = BACKLIGHT_DEFAULT;
    resetIdleTimer();
    
    // Start LVGL timer handler task on Core 1
    startLVGLTask();
    
    LOG_I("Display initialized: %dx%d", DISPLAY_WIDTH, DISPLAY_HEIGHT);
    return true;
}

void Display::initHardware() {
    LOG_I("Initializing display using ESP-IDF LCD Panel API...");
    
    // Turn on backlight (active LOW)
    gpio_config_t bl_conf = {
        .pin_bit_mask = (1ULL << DISPLAY_BL_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&bl_conf);
    gpio_set_level((gpio_num_t)DISPLAY_BL_PIN, 0);  // Active LOW = ON
    LOG_I("Backlight ON");
    
    // Send LCD init commands via 3-wire SPI
    send_lcd_init_commands();
    
    // Create RGB panel
    LOG_I("Creating RGB panel...");
    esp_lcd_rgb_panel_config_t panel_config = {};
    panel_config.clk_src = LCD_CLK_SRC_PLL160M;
    
    // Pixel clock - with GPIO drive strength reduced, we can use higher PCLK
    // 10 MHz provides good display quality while EMI is controlled by low drive strength
    panel_config.timings.pclk_hz = 10 * 1000 * 1000;  // 10 MHz
    LOG_I("PCLK set to 10MHz (EMI controlled via low GPIO drive strength)");
    panel_config.timings.h_res = 480;
    panel_config.timings.v_res = 480;
    panel_config.timings.hsync_pulse_width = 8;
    panel_config.timings.hsync_back_porch = 20;
    panel_config.timings.hsync_front_porch = 40;
    panel_config.timings.vsync_pulse_width = 8;
    panel_config.timings.vsync_back_porch = 20;
    panel_config.timings.vsync_front_porch = 50;
    panel_config.timings.flags.pclk_active_neg = 0;
    panel_config.data_width = 16;
    // Note: psram_trans_align deprecated in ESP-IDF 5.x, bounce buffer handles alignment
    panel_config.de_gpio_num = 17;
    panel_config.pclk_gpio_num = 9;
    panel_config.vsync_gpio_num = 3;
    panel_config.hsync_gpio_num = 46;
    panel_config.disp_gpio_num = -1;
    panel_config.data_gpio_nums[0] = 10;   // DATA0 - B3
    panel_config.data_gpio_nums[1] = 11;   // DATA1 - B4
    panel_config.data_gpio_nums[2] = 12;   // DATA2 - B5 (was SPI SDO)
    panel_config.data_gpio_nums[3] = 13;   // DATA3 - B6 (was SPI SCK)
    panel_config.data_gpio_nums[4] = 14;   // DATA4 - B7
    panel_config.data_gpio_nums[5] = 21;   // DATA5 - G2
    panel_config.data_gpio_nums[6] = 47;   // DATA6 - G3
    panel_config.data_gpio_nums[7] = 48;   // DATA7 - G4
    panel_config.data_gpio_nums[8] = 45;   // DATA8 - G5
    panel_config.data_gpio_nums[9] = 38;   // DATA9 - G6
    panel_config.data_gpio_nums[10] = 39;  // DATA10 - G7
    panel_config.data_gpio_nums[11] = 40;  // DATA11 - R3
    panel_config.data_gpio_nums[12] = 41;  // DATA12 - R4
    panel_config.data_gpio_nums[13] = 42;  // DATA13 - R5
    panel_config.data_gpio_nums[14] = 2;   // DATA14 - R6
    panel_config.data_gpio_nums[15] = 1;   // DATA15 - R7
    panel_config.flags.fb_in_psram = 1;
    
    // =========================================================================
    // ESP-IDF 5.x: BOUNCE BUFFER AND DOUBLE BUFFERING
    // =========================================================================
    // These features improve display stability during WiFi operations:
    //
    // 1. bounce_buffer_size_px - Internal SRAM buffer for DMA
    //    Decouples LCD DMA from PSRAM bus contention during WiFi/Flash operations
    //    480 * 10 = 10 scanlines × 2 bytes/pixel = ~9.6KB of internal SRAM
    //
    // 2. num_fbs = 2 - Double buffering for tear-free display
    //    DMA reads from one buffer while CPU draws to another
    //    Requires ~900KB additional PSRAM (we have 8MB available)
    
    panel_config.bounce_buffer_size_px = 480 * 10;  // 10 scanlines bounce buffer
    panel_config.num_fbs = 2;  // Double buffering
    LOG_I("Enabled bounce buffer (10 scanlines) and double buffering");
    
    // Create semaphore for flush synchronization
    s_flush_sem = xSemaphoreCreateBinary();
    xSemaphoreGive(s_flush_sem);  // Start with semaphore available
    
    esp_err_t ret = esp_lcd_new_rgb_panel(&panel_config, &panel_handle);
    if (ret != ESP_OK) {
        LOG_E("Failed to create RGB panel: %s", esp_err_to_name(ret));
        return;
    }
    
    // ESP-IDF 5.x: Register callbacks after panel creation
    esp_lcd_rgb_panel_event_callbacks_t callbacks = {};
    callbacks.on_color_trans_done = on_frame_trans_done;  // Called when color buffer copied to FB
    ret = esp_lcd_rgb_panel_register_event_callbacks(panel_handle, &callbacks, NULL);
    if (ret != ESP_OK) {
        LOG_W("Failed to register panel callbacks: %s (display will work but may tear)", esp_err_to_name(ret));
    } else {
        LOG_I("Registered panel event callbacks for vsync synchronization");
    }
    
    // =========================================================================
    // CRITICAL FIX FOR WIFI INTERFERENCE (EMI REDUCTION)
    // =========================================================================
    // The RGB panel signals (PCLK/DATA) generate noise that jams the WiFi radio.
    // We reduce the GPIO drive strength to the minimum (0) to reduce this noise (EMI).
    // Before: Sharp square waves on 20 pins → Massive harmonics → WiFi jammed
    // After: Softer edges on 20 pins → Reduced harmonics → WiFi works
    
    const gpio_num_t lcd_pins[] = {
        (gpio_num_t)DISPLAY_PCLK_PIN,
        (gpio_num_t)DISPLAY_VSYNC_PIN,
        (gpio_num_t)DISPLAY_HSYNC_PIN,
        (gpio_num_t)DISPLAY_DE_PIN,
        // Blue
        (gpio_num_t)DISPLAY_B0_PIN, (gpio_num_t)DISPLAY_B1_PIN, 
        (gpio_num_t)DISPLAY_B2_PIN, (gpio_num_t)DISPLAY_B3_PIN, (gpio_num_t)DISPLAY_B4_PIN,
        // Green
        (gpio_num_t)DISPLAY_G0_PIN, (gpio_num_t)DISPLAY_G1_PIN, (gpio_num_t)DISPLAY_G2_PIN,
        (gpio_num_t)DISPLAY_G3_PIN, (gpio_num_t)DISPLAY_G4_PIN, (gpio_num_t)DISPLAY_G5_PIN,
        // Red
        (gpio_num_t)DISPLAY_R0_PIN, (gpio_num_t)DISPLAY_R1_PIN, (gpio_num_t)DISPLAY_R2_PIN,
        (gpio_num_t)DISPLAY_R3_PIN, (gpio_num_t)DISPLAY_R4_PIN
    };

    LOG_I("Reducing LCD pin drive strength to minimize WiFi interference...");
    for (size_t i = 0; i < sizeof(lcd_pins)/sizeof(lcd_pins[0]); i++) {
        gpio_set_drive_capability(lcd_pins[i], GPIO_DRIVE_CAP_0);
    }
    LOG_I("LCD pin drive strength set to minimum (GPIO_DRIVE_CAP_0)");
    // =========================================================================
    
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    
    LOG_I("RGB panel created successfully!");
    LOG_I("Display hardware initialized successfully");
}

void Display::initLVGL() {
    LOG_I("Initializing LVGL...");
    
    lv_init();
    
    // Register LittleFS driver for images (drive letter 'S')
    lv_fs_littlefs_init();
    
    // Allocate LVGL draw buffer
    // Use PSRAM for larger buffer - internal RAM is too precious (needed for SSL)
    // 40 lines in PSRAM = 480 * 40 * 2 = 38,400 bytes (12 flushes per full screen)
    // Note: May have slight display noise due to PSRAM bandwidth contention with RGB panel
    size_t buf_size = DISPLAY_WIDTH * 40;
    
    // Allocate in PSRAM to preserve internal heap for SSL
    _buf1 = (lv_color_t *)heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    
    if (!_buf1) {
        LOG_W("PSRAM allocation failed, trying smaller internal RAM buffer");
        // Fallback to small internal RAM buffer
        buf_size = DISPLAY_WIDTH * 5;
        _buf1 = (lv_color_t *)heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    
    _buf2 = nullptr;
    
    if (!_buf1) {
        LOG_E("Failed to allocate LVGL buffer!");
        return;
    }
    
    // Check where it ended up
    if ((uintptr_t)_buf1 >= 0x3FC00000 && (uintptr_t)_buf1 < 0x3FD00000) {
        LOG_I("LVGL buffer allocated in INTERNAL RAM (%d bytes)", buf_size * sizeof(lv_color_t));
    } else {
        LOG_I("LVGL buffer allocated in PSRAM (%d bytes)", buf_size * sizeof(lv_color_t));
    }
    
    lv_disp_draw_buf_init(&_drawBuf, _buf1, _buf2, buf_size);
    
    lv_disp_drv_init(&_dispDrv);
    _dispDrv.hor_res = DISPLAY_WIDTH;
    _dispDrv.ver_res = DISPLAY_HEIGHT;
    _dispDrv.physical_hor_res = DISPLAY_WIDTH;  // Physical resolution (same as logical)
    _dispDrv.physical_ver_res = DISPLAY_HEIGHT;  // Physical resolution (same as logical)
    _dispDrv.offset_x = 0;  // No horizontal offset
    _dispDrv.offset_y = 0;  // No vertical offset
    _dispDrv.flush_cb = flushCallback;
    _dispDrv.draw_buf = &_drawBuf;
    _dispDrv.user_data = this;
    
    // Store driver pointer for callback access
    s_disp_drv = &_dispDrv;
    
    _display = lv_disp_drv_register(&_dispDrv);
    
    LOG_I("LVGL initialized with %d pixel buffer", buf_size);
}

void Display::flushCallback(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    if (!panel_handle) {
        LOG_E("Flush called but panel is NULL!");
        lv_disp_flush_ready(drv);
        return;
    }
    
    // Wait for any previous DMA transfer to complete
    // The semaphore is given by on_frame_trans_done when previous DMA completes
    if (s_flush_sem) {
        xSemaphoreTake(s_flush_sem, portMAX_DELAY);
    }
    
    // Draw the buffer to the RGB panel
    // This starts an asynchronous DMA transfer
    esp_lcd_panel_draw_bitmap(panel_handle, 
                              area->x1, area->y1, 
                              area->x2 + 1, area->y2 + 1, 
                              color_p);
    
    // Wait for THIS transfer to complete before returning
    // This ensures the buffer remains valid until DMA is done
    // The on_frame_trans_done callback will give the semaphore when transfer completes
    if (s_flush_sem) {
        xSemaphoreTake(s_flush_sem, portMAX_DELAY);
        // Give semaphore back for next flush
        xSemaphoreGive(s_flush_sem);
    }
    
    lv_disp_flush_ready(drv);
}

void Display::update() {
    // LVGL timer handler now runs in dedicated FreeRTOS task
    // This function only handles backlight idle timeout
    updateBacklightIdle();
}

void Display::startLVGLTask() {
    if (_lvglTaskHandle != nullptr) {
        LOG_W("LVGL task already started");
        return;
    }
    
    xTaskCreatePinnedToCore(
        lvglTaskCode,
        "LVGLTask",
        LVGL_TASK_STACK_SIZE,
        this,
        LVGL_TASK_PRIORITY,
        &_lvglTaskHandle,
        LVGL_TASK_CORE
    );
    
    if (_lvglTaskHandle != nullptr) {
        LOG_I("LVGL task started on Core %d (priority %d)", LVGL_TASK_CORE, LVGL_TASK_PRIORITY);
    } else {
        LOG_E("Failed to create LVGL task!");
    }
}

void Display::lvglTaskCode(void* parameter) {
    Display* self = static_cast<Display*>(parameter);
    
    LOG_I("LVGL task running on Core %d", xPortGetCoreID());
    
    while (true) {
        // Call LVGL timer handler - this processes animations and screen updates
        // Returns time until next call needed (in ms), but we use fixed interval for smooth updates
        uint32_t nextCall = lv_timer_handler();
        
        // Use fixed 5ms interval for consistent frame rate
        // LVGL typically needs ~16ms for 60 FPS, but calling more frequently
        // ensures smooth animations even during network operations
        vTaskDelay(pdMS_TO_TICKS(LVGL_TASK_INTERVAL_MS));
    }
}

void Display::setBacklight(uint8_t brightness) {
    _backlightLevel = brightness;
    // Active LOW: 0 = ON, 1 = OFF
    // For simplicity, use digital on/off to avoid PWM conflicts
    if (brightness > 0) {
        gpio_set_level((gpio_num_t)DISPLAY_BL_PIN, 0);  // ON
    } else {
        gpio_set_level((gpio_num_t)DISPLAY_BL_PIN, 1);  // OFF
    }
}

void Display::backlightOn() {
    // Re-enable RGB signals BEFORE turning on backlight
    // esp_lcd_panel_disp_on_off(handle, true) = turn display ON
    if (panel_handle) {
        esp_lcd_panel_disp_on_off(panel_handle, true);  // true = display ON
    }
    
    _isDimmed = false;
    gpio_set_level((gpio_num_t)DISPLAY_BL_PIN, 0);  // Active LOW = ON
    setBacklight(_backlightSaved);
}

void Display::backlightOff() {
    _backlightSaved = _backlightLevel;
    setBacklight(0);
    gpio_set_level((gpio_num_t)DISPLAY_BL_PIN, 1);  // Active LOW = OFF
    
    // Stop RGB signals to eliminate WiFi interference when screen is off
    // This silences the 20+ data pins that generate EMI
    if (panel_handle) {
        esp_lcd_panel_disp_on_off(panel_handle, false);  // false = display OFF
    }
}

void Display::resetIdleTimer() {
    _lastActivityTime = millis();
    if (_isDimmed) {
        // If screen was fully OFF (0 brightness), re-enable RGB signals first
        if (_backlightLevel == 0 && panel_handle) {
            esp_lcd_panel_disp_on_off(panel_handle, true);  // true = display ON
        }
        
        _isDimmed = false;
        setBacklight(_backlightSaved);
    }
}

void Display::updateBacklightIdle() {
    unsigned long now = millis();
    unsigned long idleTime = now - _lastActivityTime;
    
#if BACKLIGHT_OFF_TIMEOUT > 0
    if (idleTime >= BACKLIGHT_OFF_TIMEOUT && _backlightLevel > 0) {
        if (!_isDimmed) _backlightSaved = _backlightLevel;
        setBacklight(0);
        
        // Stop RGB signals to eliminate WiFi interference
        if (panel_handle) {
            esp_lcd_panel_disp_on_off(panel_handle, false);  // false = display OFF
        }
        
        _isDimmed = true;
        return;
    }
#endif
    
    if (idleTime >= BACKLIGHT_DIM_TIMEOUT && !_isDimmed) {
        _backlightSaved = _backlightLevel;
        setBacklight(BACKLIGHT_DIM_LEVEL);
        _isDimmed = true;
    }
}
