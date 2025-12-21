/**
 * BrewOS Settings Screen Implementation
 * 
 * Simplified menu for settings navigation
 * Optimized for 480x480 round display
 */

#include "platform/platform.h"
#include "ui/screen_settings.h"
#include "display/theme.h"
#include "display/display_config.h"

// Simplified menu items (removed MQTT, System - configurable via web UI)
static const char* item_icons[] = {
    LV_SYMBOL_SETTINGS,  // Temp
    LV_SYMBOL_BLUETOOTH, // Scale
    LV_SYMBOL_UPLOAD,    // Cloud
    LV_SYMBOL_EYE_OPEN,  // Theme
    LV_SYMBOL_WIFI,      // WiFi
    LV_SYMBOL_FILE,      // About
    LV_SYMBOL_LEFT       // Exit
};

static const char* item_names[] = {
    "Temperature",
    "Scale",
    "Cloud",
    "Theme",
    "WiFi",
    "About",
    "Exit"
};

static const char* item_descriptions[] = {
    "Boiler temperatures",
    "Connect scale",
    "Pair with cloud",
    "Dark / Light mode",
    "Enter setup mode",
    "Device info",
    "Return to home"
};

// Static elements
static lv_obj_t* screen = nullptr;
static lv_obj_t* title_label = nullptr;
static lv_obj_t* icon_label = nullptr;
static lv_obj_t* name_label = nullptr;
static lv_obj_t* desc_label = nullptr;
static lv_obj_t* status_icons[SETTINGS_COUNT] = {nullptr};
static lv_obj_t* selector_arc = nullptr;

// State
static int selected_index = 0;
static settings_select_callback_t select_callback = nullptr;

// =============================================================================
// Screen Creation
// =============================================================================

lv_obj_t* screen_settings_create(void) {
    LOG_I("Creating settings screen...");
    
    // Create screen with dark background
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG_DARK, 0);
    
    // Create main container
    lv_obj_t* container = lv_obj_create(screen);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_center(container);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    
    // === Title at top ===
    title_label = lv_label_create(container);
    lv_label_set_text(title_label, "Settings");
    lv_obj_set_style_text_font(title_label, FONT_LARGE, 0);
    lv_obj_set_style_text_color(title_label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 50);
    
    // === Selection Arc (outer ring showing position) ===
    selector_arc = lv_arc_create(container);
    lv_obj_set_size(selector_arc, 420, 420);
    lv_obj_center(selector_arc);
    lv_arc_set_range(selector_arc, 0, SETTINGS_COUNT);
    lv_arc_set_value(selector_arc, 1);
    lv_arc_set_bg_angles(selector_arc, 0, 360);
    lv_arc_set_rotation(selector_arc, 270);
    
    // Arc background
    lv_obj_set_style_arc_color(selector_arc, COLOR_ARC_BG, LV_PART_MAIN);
    lv_obj_set_style_arc_width(selector_arc, 4, LV_PART_MAIN);
    
    // Arc indicator
    lv_obj_set_style_arc_color(selector_arc, COLOR_ACCENT_AMBER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(selector_arc, 4, LV_PART_INDICATOR);
    
    // Hide knob
    lv_obj_set_style_bg_opa(selector_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_clear_flag(selector_arc, LV_OBJ_FLAG_CLICKABLE);
    
    // === Large Icon (center) ===
    icon_label = lv_label_create(container);
    lv_label_set_text(icon_label, item_icons[selected_index]);
    lv_obj_set_style_text_font(icon_label, FONT_TEMP, 0);
    lv_obj_set_style_text_color(icon_label, COLOR_ACCENT_AMBER, 0);
    lv_obj_align(icon_label, LV_ALIGN_CENTER, 0, -30);
    
    // === Item Name ===
    name_label = lv_label_create(container);
    lv_label_set_text(name_label, item_names[selected_index]);
    lv_obj_set_style_text_font(name_label, FONT_LARGE, 0);
    lv_obj_set_style_text_color(name_label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(name_label, LV_ALIGN_CENTER, 0, 40);
    
    // === Description ===
    desc_label = lv_label_create(container);
    lv_label_set_text(desc_label, item_descriptions[selected_index]);
    lv_obj_set_style_text_font(desc_label, FONT_SMALL, 0);
    lv_obj_set_style_text_color(desc_label, COLOR_TEXT_MUTED, 0);
    lv_obj_align(desc_label, LV_ALIGN_CENTER, 0, 70);
    
    // === Page dots at bottom ===
    lv_obj_t* dots_container = lv_obj_create(container);
    lv_obj_remove_style_all(dots_container);
    lv_obj_set_size(dots_container, SETTINGS_COUNT * 18, 12);
    lv_obj_align(dots_container, LV_ALIGN_BOTTOM_MID, 0, -60);
    lv_obj_set_flex_flow(dots_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dots_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    for (int i = 0; i < SETTINGS_COUNT; i++) {
        status_icons[i] = lv_obj_create(dots_container);
        lv_obj_set_size(status_icons[i], 6, 6);
        lv_obj_set_style_radius(status_icons[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(status_icons[i], 0, 0);
        lv_obj_set_style_pad_left(status_icons[i], 3, 0);
        lv_obj_set_style_pad_right(status_icons[i], 3, 0);
        
        if (i == selected_index) {
            lv_obj_set_style_bg_color(status_icons[i], COLOR_ACCENT_AMBER, 0);
        } else {
            lv_obj_set_style_bg_color(status_icons[i], COLOR_BG_ELEVATED, 0);
        }
    }
    
    // === Hint ===
    lv_obj_t* hint = lv_label_create(container);
    lv_label_set_text(hint, "Rotate to browse • Press to select");
    lv_obj_set_style_text_font(hint, FONT_SMALL, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_MUTED, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -80);
    
    // Note: Encoder navigation is handled via direct callbacks in main.cpp (handleEncoderEvent)
    // This avoids double navigation that occurred when both LVGL input device and direct 
    // callbacks were processing encoder events
    
    LOG_I("Settings screen created");
    return screen;
}

// =============================================================================
// Screen Update
// =============================================================================

void screen_settings_update(const ui_state_t* state) {
    // Could update connection status indicators for WiFi, MQTT, Scale
    if (!screen || !state) return;
}

void screen_settings_navigate(int direction) {
    LOG_I("Settings navigate: direction=%d, current=%d", direction, selected_index);
    
    // Update selection
    selected_index += direction;
    if (selected_index < 0) selected_index = SETTINGS_COUNT - 1;
    if (selected_index >= SETTINGS_COUNT) selected_index = 0;
    
    LOG_I("Settings navigate: new index=%d", selected_index);
    
    // Update UI
    lv_label_set_text(icon_label, item_icons[selected_index]);
    lv_label_set_text(name_label, item_names[selected_index]);
    lv_label_set_text(desc_label, item_descriptions[selected_index]);
    
    // Update arc position (each item is 1 unit)
    lv_arc_set_value(selector_arc, selected_index + 1);
    
    // Update dots
    for (int i = 0; i < SETTINGS_COUNT; i++) {
        if (status_icons[i]) {
            if (i == selected_index) {
                lv_obj_set_style_bg_color(status_icons[i], COLOR_ACCENT_AMBER, 0);
            } else {
                lv_obj_set_style_bg_color(status_icons[i], COLOR_BG_ELEVATED, 0);
            }
        }
    }
}

settings_item_t screen_settings_get_selection(void) {
    return (settings_item_t)selected_index;
}

void screen_settings_select(void) {
    if (select_callback) {
        select_callback((settings_item_t)selected_index);
    }
}

void screen_settings_set_select_callback(settings_select_callback_t callback) {
    select_callback = callback;
}

