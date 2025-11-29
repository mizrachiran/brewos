/**
 * BrewOS Coffee Theme Implementation
 * 
 * Uses official BrewOS brand colors from Brewos.jpg palette
 */

#include "display/theme.h"
#include "display/display_config.h"

// Static styles
static lv_style_t style_card;
static lv_style_t style_btn_primary;
static lv_style_t style_btn_secondary;
static lv_style_t style_arc;
static bool styles_initialized = false;

// =============================================================================
// Theme Initialization
// =============================================================================

void theme_init(void) {
    if (styles_initialized) return;
    
    // Get the default theme and modify it with brand colors
    lv_theme_t* theme = lv_theme_default_init(
        lv_disp_get_default(),
        COLOR_ACCENT_PRIMARY,       // Primary color (caramel)
        COLOR_ACCENT_COPPER,        // Secondary color (medium brown)
        LV_THEME_DEFAULT_DARK,      // Dark mode
        FONT_NORMAL                 // Default font
    );
    lv_disp_set_theme(lv_disp_get_default(), theme);
    
    // Initialize card style
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, COLOR_BG_CARD);
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_radius(&style_card, RADIUS_NORMAL);
    lv_style_set_pad_all(&style_card, PADDING_NORMAL);
    lv_style_set_border_width(&style_card, 0);
    lv_style_set_shadow_width(&style_card, 20);
    lv_style_set_shadow_color(&style_card, lv_color_black());
    lv_style_set_shadow_opa(&style_card, SHADOW_OPA);
    
    // Initialize primary button style (brand caramel accent)
    lv_style_init(&style_btn_primary);
    lv_style_set_bg_color(&style_btn_primary, COLOR_ACCENT_PRIMARY);
    lv_style_set_bg_opa(&style_btn_primary, LV_OPA_COVER);
    lv_style_set_radius(&style_btn_primary, RADIUS_NORMAL);
    lv_style_set_text_color(&style_btn_primary, COLOR_BG_DARK);
    lv_style_set_pad_all(&style_btn_primary, PADDING_NORMAL);
    
    // Initialize secondary button style (outlined with brand accent)
    lv_style_init(&style_btn_secondary);
    lv_style_set_bg_color(&style_btn_secondary, COLOR_BG_ELEVATED);
    lv_style_set_bg_opa(&style_btn_secondary, LV_OPA_COVER);
    lv_style_set_radius(&style_btn_secondary, RADIUS_NORMAL);
    lv_style_set_text_color(&style_btn_secondary, COLOR_TEXT_PRIMARY);
    lv_style_set_border_width(&style_btn_secondary, 2);
    lv_style_set_border_color(&style_btn_secondary, COLOR_ACCENT_PRIMARY);
    lv_style_set_pad_all(&style_btn_secondary, PADDING_NORMAL);
    
    // Initialize arc style
    lv_style_init(&style_arc);
    lv_style_set_arc_width(&style_arc, 12);
    lv_style_set_arc_rounded(&style_arc, true);
    
    styles_initialized = true;
}

// =============================================================================
// Style Application Functions
// =============================================================================

void theme_apply_card_style(lv_obj_t* obj) {
    if (!styles_initialized) theme_init();
    lv_obj_add_style(obj, &style_card, 0);
}

void theme_apply_button_style(lv_obj_t* obj, bool is_primary) {
    if (!styles_initialized) theme_init();
    lv_obj_add_style(obj, is_primary ? &style_btn_primary : &style_btn_secondary, 0);
}

// =============================================================================
// Widget Creation Functions
// =============================================================================

lv_obj_t* theme_create_arc(lv_obj_t* parent, uint16_t size, lv_color_t color) {
    if (!styles_initialized) theme_init();
    
    lv_obj_t* arc = lv_arc_create(parent);
    lv_obj_set_size(arc, size, size);
    
    // Background arc style
    lv_obj_set_style_arc_color(arc, COLOR_ARC_BG, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 12, LV_PART_MAIN);
    
    // Indicator arc style
    lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    
    // Hide the knob
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    
    // Set default range
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_bg_angles(arc, 135, 45);
    lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
    
    return arc;
}

// =============================================================================
// Color Utility Functions
// =============================================================================

lv_color_t theme_get_temp_color(float current, float setpoint) {
    float diff = current - setpoint;
    
    if (diff < -10.0f) {
        // Cold - blue
        return COLOR_TEMP_COLD;
    } else if (diff < -2.0f) {
        // Warming up - yellow
        return COLOR_TEMP_WARM;
    } else if (diff < 2.0f) {
        // At temperature - green
        return COLOR_TEMP_READY;
    } else {
        // Too hot - red
        return COLOR_TEMP_HOT;
    }
}

lv_color_t theme_get_pressure_color(float pressure) {
    if (pressure < 5.0f) {
        // Low pressure - blue
        return COLOR_PRESSURE_LOW;
    } else if (pressure <= 11.0f) {
        // Optimal range - green
        return COLOR_PRESSURE_OPTIMAL;
    } else {
        // High pressure - red
        return COLOR_PRESSURE_HIGH;
    }
}

lv_color_t theme_get_state_color(theme_state_t state) {
    switch (state) {
        case THEME_STATE_SUCCESS:
            return COLOR_SUCCESS;
        case THEME_STATE_WARNING:
            return COLOR_WARNING;
        case THEME_STATE_ERROR:
            return COLOR_ERROR;
        case THEME_STATE_INFO:
        default:
            return COLOR_INFO;
    }
}

