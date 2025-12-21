/**
 * BrewOS Display Configuration
 * 
 * Target: UEDX48480021-MD80E (ESP32-S3 Knob Display / Spotpear 2.1")
 * - 2.1" Round IPS 480x480
 * - GC9503V controller with RGB666 parallel interface
 * - 3-wire SPI for initialization commands
 * - Rotary Encoder + Push Button
 */

#ifndef DISPLAY_CONFIG_H
#define DISPLAY_CONFIG_H

// =============================================================================
// Display Specifications
// =============================================================================

#define DISPLAY_WIDTH           480
#define DISPLAY_HEIGHT          480
#define DISPLAY_ROTATION        0       // 0, 90, 180, 270

// Round display - center point
#define DISPLAY_CENTER_X        (DISPLAY_WIDTH / 2)
#define DISPLAY_CENTER_Y        (DISPLAY_HEIGHT / 2)
#define DISPLAY_RADIUS          (DISPLAY_WIDTH / 2)

// Safe area for round display (content should stay within this)
#define DISPLAY_SAFE_MARGIN     70
#define DISPLAY_SAFE_RADIUS     (DISPLAY_RADIUS - DISPLAY_SAFE_MARGIN)

// =============================================================================
// Display Pins for UEDX48480021-MD80E (Spotpear 2.1" Round)
// Uses RGB666 parallel interface with 3-wire SPI for init
// =============================================================================

// 3-wire SPI for display initialization (no DC pin)
#define DISPLAY_SPI_CS_PIN      18      // SPI Chip Select
#define DISPLAY_SPI_SCK_PIN     13      // SPI Clock  
#define DISPLAY_SPI_MOSI_PIN    12      // SPI MOSI
#define DISPLAY_RST_PIN         8       // Reset

// RGB parallel interface pins
#define DISPLAY_DE_PIN          17      // Data Enable
#define DISPLAY_VSYNC_PIN       3       // Vertical Sync
#define DISPLAY_HSYNC_PIN       46      // Horizontal Sync
#define DISPLAY_PCLK_PIN        9       // Pixel Clock

// RGB data pins - OFFICIAL VIEWESMART UEDX48480021-MD80E pinout
// From: https://github.com/VIEWESMART/ESP32-IDF/blob/main/examples/2.1inch/UEDX48480021-MD80E-SDK/components/bsp/include/bsp/esp-bsp.h
// Uses RGB666 format (16 data lines mapped to 18-bit color)
// DATA0-4 = Blue, DATA5-10 = Green, DATA11-15 = Red

#define DISPLAY_R0_PIN          40      // DATA11 (R3)
#define DISPLAY_R1_PIN          41      // DATA12 (R4)
#define DISPLAY_R2_PIN          42      // DATA13 (R5)
#define DISPLAY_R3_PIN          2       // DATA14 (R6)
#define DISPLAY_R4_PIN          1       // DATA15 (R7)

#define DISPLAY_G0_PIN          21      // DATA5 (G2)
#define DISPLAY_G1_PIN          47      // DATA6 (G3)
#define DISPLAY_G2_PIN          48      // DATA7 (G4)
#define DISPLAY_G3_PIN          45      // DATA8 (G5)
#define DISPLAY_G4_PIN          38      // DATA9 (G6)
#define DISPLAY_G5_PIN          39      // DATA10 (G7)

#define DISPLAY_B0_PIN          10      // DATA0 (B3)
#define DISPLAY_B1_PIN          11      // DATA1 (B4)
#define DISPLAY_B2_PIN          12      // DATA2 (B5)
#define DISPLAY_B3_PIN          13      // DATA3 (B6)
#define DISPLAY_B4_PIN          14      // DATA4 (B7)

// Backlight control
#define DISPLAY_BL_PIN          7       // Backlight PWM

// Legacy SPI pins for compatibility (not used for RGB display)
#define DISPLAY_MOSI_PIN        DISPLAY_SPI_MOSI_PIN
#define DISPLAY_SCLK_PIN        DISPLAY_SPI_SCK_PIN
#define DISPLAY_CS_PIN          DISPLAY_SPI_CS_PIN
#define DISPLAY_DC_PIN          -1      // Not used (3-wire SPI)
#define DISPLAY_SPI_FREQ        10000000
#define DISPLAY_SPI_HOST        SPI2_HOST

// =============================================================================
// Rotary Encoder Pins for UEDX48480021-MD80E
// =============================================================================

#define ENCODER_A_PIN           6       // Encoder A (CLK)
#define ENCODER_B_PIN           5       // Encoder B (DT)
#define ENCODER_BTN_PIN         0       // Encoder Button (SW) - also boot button

// Encoder settings
#define ENCODER_STEPS_PER_NOTCH 4       // Typical for most encoders
#define ENCODER_DEBOUNCE_MS     5       // Debounce time

// Button settings
#define BTN_DEBOUNCE_MS         50      // Button debounce
#define BTN_LONG_PRESS_MS       2000    // Long press threshold
#define BTN_DOUBLE_PRESS_MS     300     // Double press window

// =============================================================================
// Backlight Settings
// =============================================================================

#define BACKLIGHT_PWM_CHANNEL   0
#define BACKLIGHT_PWM_FREQ      5000    // 5kHz PWM
#define BACKLIGHT_PWM_RES       8       // 8-bit resolution (0-255)

#define BACKLIGHT_MAX           255
#define BACKLIGHT_MIN           10
#define BACKLIGHT_DEFAULT       200

// Auto-dim settings
#define BACKLIGHT_DIM_TIMEOUT   30000   // Dim after 30s idle
#define BACKLIGHT_DIM_LEVEL     50      // Dimmed brightness
#define BACKLIGHT_OFF_TIMEOUT   60000   // Turn off after 60s idle (1 minute)

// =============================================================================
// LVGL Display Buffer
// =============================================================================

// Partial buffer for RGB displays (full buffer would be too large)
// For systems without PSRAM, use smaller buffer to save internal RAM
#define LVGL_BUFFER_SIZE        (DISPLAY_WIDTH * DISPLAY_HEIGHT / 10)

// Double buffering disabled for low memory systems (no PSRAM)
#define LVGL_DOUBLE_BUFFER      0

#endif // DISPLAY_CONFIG_H

