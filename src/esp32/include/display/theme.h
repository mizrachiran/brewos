/**
 * BrewOS Coffee Theme
 * 
 * Dark theme using official BrewOS brand colors:
 * - Primary Dark:   #361E12 (rich coffee brown)
 * - Accent:         #D5A071 (caramel/tan)
 * - Medium:         #714C30 (medium brown)
 * - Background:     #1A0F0A (darkest coffee)
 */

#ifndef THEME_H
#define THEME_H

#include <lvgl.h>

// =============================================================================
// Color Definitions (RGB888 format) - BrewOS Brand Colors
// =============================================================================

// Background colors (from brand palette)
#define COLOR_BG_DARK           lv_color_hex(0x1A0F0A)  // Darkest coffee
#define COLOR_BG_CARD           lv_color_hex(0x361E12)  // Dark brown (brand primary)
#define COLOR_BG_ELEVATED       lv_color_hex(0x4A2A1A)  // Elevated surface

// Accent colors (from brand palette)
#define COLOR_ACCENT_PRIMARY    lv_color_hex(0xD5A071)  // Caramel/tan (brand accent)
#define COLOR_ACCENT_AMBER      lv_color_hex(0xD5A071)  // Alias for primary
#define COLOR_ACCENT_ORANGE     lv_color_hex(0xC4703C)  // Warm orange
#define COLOR_ACCENT_COPPER     lv_color_hex(0x714C30)  // Medium brown (brand)

// Temperature indicator colors
#define COLOR_TEMP_COLD         lv_color_hex(0x3B82F6)  // Blue (cold/heating up)
#define COLOR_TEMP_WARM         lv_color_hex(0xF59E0B)  // Yellow (warming)
#define COLOR_TEMP_READY        lv_color_hex(0x22C55E)  // Green (at temp)
#define COLOR_TEMP_HOT          lv_color_hex(0xEF4444)  // Red (steam/hot)

// State colors
#define COLOR_SUCCESS           lv_color_hex(0x22C55E)  // Green
#define COLOR_WARNING           lv_color_hex(0xF59E0B)  // Yellow
#define COLOR_ERROR             lv_color_hex(0xDC2626)  // Red
#define COLOR_INFO              lv_color_hex(0x3B82F6)  // Blue

// Text colors (optimized for dark brown backgrounds)
#define COLOR_TEXT_PRIMARY      lv_color_hex(0xFBFCF8)  // Cream white
#define COLOR_TEXT_SECONDARY    lv_color_hex(0xD5A071)  // Caramel (accent)
#define COLOR_TEXT_MUTED        lv_color_hex(0x9B6E46)  // Light coffee
#define COLOR_TEXT_DISABLED     lv_color_hex(0x714C30)  // Medium brown

// Pressure gauge colors
#define COLOR_PRESSURE_LOW      lv_color_hex(0x3B82F6)  // Blue (< 5 bar)
#define COLOR_PRESSURE_OPTIMAL  lv_color_hex(0x22C55E)  // Green (8-10 bar)
#define COLOR_PRESSURE_HIGH     lv_color_hex(0xEF4444)  // Red (> 11 bar)

// =============================================================================
// Arc/Gauge Colors
// =============================================================================

#define COLOR_ARC_BG            lv_color_hex(0x4A2A1A)  // Arc background (elevated)
#define COLOR_ARC_INDICATOR     COLOR_ACCENT_PRIMARY    // Arc indicator (caramel)

// =============================================================================
// Brand-specific UI Colors
// =============================================================================

#define COLOR_GEAR_SILVER       lv_color_hex(0xBBB9B5)  // Gear/metallic elements
#define COLOR_CREAM             lv_color_hex(0xFBFCF8)  // Cream white
#define COLOR_COFFEE_LIGHT      lv_color_hex(0x9B6E46)  // Light coffee

// =============================================================================
// Font Definitions
// =============================================================================

// Use Montserrat fonts (included in LVGL)
#define FONT_SMALL          &lv_font_montserrat_12
#define FONT_NORMAL         &lv_font_montserrat_16
#define FONT_MEDIUM         &lv_font_montserrat_20
#define FONT_LARGE          &lv_font_montserrat_24
#define FONT_XLARGE         &lv_font_montserrat_28
#define FONT_HUGE           &lv_font_montserrat_32
#define FONT_TEMP           &lv_font_montserrat_48  // For temperature display

// =============================================================================
// Style Definitions
// =============================================================================

// Padding and margins
#define PADDING_SMALL       8
#define PADDING_NORMAL      16
#define PADDING_LARGE       24

// Border radius for rounded elements
#define RADIUS_SMALL        8
#define RADIUS_NORMAL       12
#define RADIUS_LARGE        16
#define RADIUS_FULL         240     // For circular elements (radius = 480/2)

// Shadow opacity
#define SHADOW_OPA          LV_OPA_20

// =============================================================================
// Theme Functions
// =============================================================================

/**
 * Initialize the BrewOS coffee theme
 * Call this after lv_init() and before creating any widgets
 */
void theme_init(void);

/**
 * Apply card style to an object (rounded, elevated background)
 */
void theme_apply_card_style(lv_obj_t* obj);

/**
 * Apply button style to an object
 */
void theme_apply_button_style(lv_obj_t* obj, bool is_primary);

/**
 * Create a styled arc for temperature/progress display
 */
lv_obj_t* theme_create_arc(lv_obj_t* parent, uint16_t size, lv_color_t color);

/**
 * Get color based on temperature state
 */
lv_color_t theme_get_temp_color(float current, float setpoint);

/**
 * Get color based on pressure value
 */
lv_color_t theme_get_pressure_color(float pressure);

/**
 * Get state color (success/warning/error/info)
 */
typedef enum {
    THEME_STATE_SUCCESS,
    THEME_STATE_WARNING,
    THEME_STATE_ERROR,
    THEME_STATE_INFO
} theme_state_t;

lv_color_t theme_get_state_color(theme_state_t state);

#endif // THEME_H

