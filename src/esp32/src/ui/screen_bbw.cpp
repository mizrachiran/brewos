/**
 * BrewOS Brew-by-Weight Settings Screen Implementation
 */

#include <Arduino.h>
#include "ui/screen_bbw.h"
#include "display/theme.h"
#include "display/display_config.h"
#include "config.h"

// Field indices
enum {
    FIELD_TARGET_WEIGHT = 0,
    FIELD_DOSE_WEIGHT,
    FIELD_STOP_OFFSET,
    FIELD_AUTO_STOP,
    FIELD_AUTO_TARE,
    FIELD_SAVE,
    FIELD_COUNT
};

// Static elements
static lv_obj_t* screen = nullptr;
static lv_obj_t* title_label = nullptr;
static lv_obj_t* target_value = nullptr;
static lv_obj_t* dose_value = nullptr;
static lv_obj_t* offset_value = nullptr;
static lv_obj_t* ratio_label = nullptr;
static lv_obj_t* auto_stop_switch = nullptr;
static lv_obj_t* auto_tare_switch = nullptr;
static lv_obj_t* save_btn = nullptr;
static lv_obj_t* field_indicators[FIELD_COUNT] = {nullptr};

// State
static int selected_field = 0;
static bool editing = false;
static bbw_settings_t current_settings;
static bbw_save_callback_t save_callback = nullptr;

// Forward declarations
static void update_display();
static void update_ratio();
static void highlight_field(int field, bool highlight);

// =============================================================================
// Screen Creation
// =============================================================================

lv_obj_t* screen_bbw_create(void) {
    LOG_I("Creating BBW settings screen...");
    
    // Create screen
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BG_DARK, 0);
    
    // Container
    lv_obj_t* container = lv_obj_create(screen);
    lv_obj_remove_style_all(container);
    lv_obj_set_size(container, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_center(container);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Title
    title_label = lv_label_create(container);
    lv_label_set_text(title_label, LV_SYMBOL_DOWNLOAD " Brew by Weight");
    lv_obj_set_style_text_font(title_label, FONT_LARGE, 0);
    lv_obj_set_style_text_color(title_label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 40);
    
    // Settings card
    lv_obj_t* card = lv_obj_create(container);
    lv_obj_set_size(card, 360, 280);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_color(card, COLOR_BG_CARD, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, RADIUS_NORMAL, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, PADDING_NORMAL, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    
    int y_pos = 0;
    int row_height = 45;
    
    // Target Weight row
    lv_obj_t* target_row = lv_obj_create(card);
    lv_obj_remove_style_all(target_row);
    lv_obj_set_size(target_row, lv_pct(100), row_height);
    lv_obj_align(target_row, LV_ALIGN_TOP_MID, 0, y_pos);
    lv_obj_set_flex_flow(target_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(target_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    field_indicators[FIELD_TARGET_WEIGHT] = lv_obj_create(target_row);
    lv_obj_set_size(field_indicators[FIELD_TARGET_WEIGHT], 4, 30);
    lv_obj_set_style_bg_color(field_indicators[FIELD_TARGET_WEIGHT], COLOR_ACCENT_AMBER, 0);
    lv_obj_set_style_radius(field_indicators[FIELD_TARGET_WEIGHT], 2, 0);
    lv_obj_set_style_bg_opa(field_indicators[FIELD_TARGET_WEIGHT], LV_OPA_TRANSP, 0);
    
    lv_obj_t* target_label = lv_label_create(target_row);
    lv_label_set_text(target_label, "Target Weight");
    lv_obj_set_style_text_font(target_label, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(target_label, COLOR_TEXT_MUTED, 0);
    
    target_value = lv_label_create(target_row);
    lv_label_set_text(target_value, "36.0g");
    lv_obj_set_style_text_font(target_value, FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(target_value, COLOR_ACCENT_AMBER, 0);
    
    y_pos += row_height;
    
    // Dose Weight row
    lv_obj_t* dose_row = lv_obj_create(card);
    lv_obj_remove_style_all(dose_row);
    lv_obj_set_size(dose_row, lv_pct(100), row_height);
    lv_obj_align(dose_row, LV_ALIGN_TOP_MID, 0, y_pos);
    lv_obj_set_flex_flow(dose_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dose_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    field_indicators[FIELD_DOSE_WEIGHT] = lv_obj_create(dose_row);
    lv_obj_set_size(field_indicators[FIELD_DOSE_WEIGHT], 4, 30);
    lv_obj_set_style_bg_color(field_indicators[FIELD_DOSE_WEIGHT], COLOR_ACCENT_AMBER, 0);
    lv_obj_set_style_radius(field_indicators[FIELD_DOSE_WEIGHT], 2, 0);
    lv_obj_set_style_bg_opa(field_indicators[FIELD_DOSE_WEIGHT], LV_OPA_TRANSP, 0);
    
    lv_obj_t* dose_label = lv_label_create(dose_row);
    lv_label_set_text(dose_label, "Dose Weight");
    lv_obj_set_style_text_font(dose_label, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(dose_label, COLOR_TEXT_MUTED, 0);
    
    dose_value = lv_label_create(dose_row);
    lv_label_set_text(dose_value, "18.0g");
    lv_obj_set_style_text_font(dose_value, FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(dose_value, COLOR_TEXT_PRIMARY, 0);
    
    y_pos += row_height;
    
    // Ratio display
    ratio_label = lv_label_create(card);
    lv_label_set_text(ratio_label, "Ratio: 1:2.0");
    lv_obj_set_style_text_font(ratio_label, FONT_SMALL, 0);
    lv_obj_set_style_text_color(ratio_label, COLOR_TEXT_MUTED, 0);
    lv_obj_align(ratio_label, LV_ALIGN_TOP_RIGHT, -10, y_pos - 35);
    
    // Stop Offset row
    lv_obj_t* offset_row = lv_obj_create(card);
    lv_obj_remove_style_all(offset_row);
    lv_obj_set_size(offset_row, lv_pct(100), row_height);
    lv_obj_align(offset_row, LV_ALIGN_TOP_MID, 0, y_pos);
    lv_obj_set_flex_flow(offset_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(offset_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    field_indicators[FIELD_STOP_OFFSET] = lv_obj_create(offset_row);
    lv_obj_set_size(field_indicators[FIELD_STOP_OFFSET], 4, 30);
    lv_obj_set_style_bg_color(field_indicators[FIELD_STOP_OFFSET], COLOR_ACCENT_AMBER, 0);
    lv_obj_set_style_radius(field_indicators[FIELD_STOP_OFFSET], 2, 0);
    lv_obj_set_style_bg_opa(field_indicators[FIELD_STOP_OFFSET], LV_OPA_TRANSP, 0);
    
    lv_obj_t* offset_label = lv_label_create(offset_row);
    lv_label_set_text(offset_label, "Stop Offset");
    lv_obj_set_style_text_font(offset_label, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(offset_label, COLOR_TEXT_MUTED, 0);
    
    offset_value = lv_label_create(offset_row);
    lv_label_set_text(offset_value, "2.0g");
    lv_obj_set_style_text_font(offset_value, FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(offset_value, COLOR_TEXT_PRIMARY, 0);
    
    y_pos += row_height;
    
    // Auto-Stop row
    lv_obj_t* auto_stop_row = lv_obj_create(card);
    lv_obj_remove_style_all(auto_stop_row);
    lv_obj_set_size(auto_stop_row, lv_pct(100), row_height);
    lv_obj_align(auto_stop_row, LV_ALIGN_TOP_MID, 0, y_pos);
    lv_obj_set_flex_flow(auto_stop_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(auto_stop_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    field_indicators[FIELD_AUTO_STOP] = lv_obj_create(auto_stop_row);
    lv_obj_set_size(field_indicators[FIELD_AUTO_STOP], 4, 30);
    lv_obj_set_style_bg_color(field_indicators[FIELD_AUTO_STOP], COLOR_ACCENT_AMBER, 0);
    lv_obj_set_style_radius(field_indicators[FIELD_AUTO_STOP], 2, 0);
    lv_obj_set_style_bg_opa(field_indicators[FIELD_AUTO_STOP], LV_OPA_TRANSP, 0);
    
    lv_obj_t* auto_stop_label = lv_label_create(auto_stop_row);
    lv_label_set_text(auto_stop_label, "Auto-Stop");
    lv_obj_set_style_text_font(auto_stop_label, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(auto_stop_label, COLOR_TEXT_MUTED, 0);
    
    auto_stop_switch = lv_switch_create(auto_stop_row);
    lv_obj_set_size(auto_stop_switch, 50, 26);
    lv_obj_add_state(auto_stop_switch, LV_STATE_CHECKED);
    
    y_pos += row_height;
    
    // Auto-Tare row
    lv_obj_t* auto_tare_row = lv_obj_create(card);
    lv_obj_remove_style_all(auto_tare_row);
    lv_obj_set_size(auto_tare_row, lv_pct(100), row_height);
    lv_obj_align(auto_tare_row, LV_ALIGN_TOP_MID, 0, y_pos);
    lv_obj_set_flex_flow(auto_tare_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(auto_tare_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    field_indicators[FIELD_AUTO_TARE] = lv_obj_create(auto_tare_row);
    lv_obj_set_size(field_indicators[FIELD_AUTO_TARE], 4, 30);
    lv_obj_set_style_bg_color(field_indicators[FIELD_AUTO_TARE], COLOR_ACCENT_AMBER, 0);
    lv_obj_set_style_radius(field_indicators[FIELD_AUTO_TARE], 2, 0);
    lv_obj_set_style_bg_opa(field_indicators[FIELD_AUTO_TARE], LV_OPA_TRANSP, 0);
    
    lv_obj_t* auto_tare_label = lv_label_create(auto_tare_row);
    lv_label_set_text(auto_tare_label, "Auto-Tare");
    lv_obj_set_style_text_font(auto_tare_label, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(auto_tare_label, COLOR_TEXT_MUTED, 0);
    
    auto_tare_switch = lv_switch_create(auto_tare_row);
    lv_obj_set_size(auto_tare_switch, 50, 26);
    lv_obj_add_state(auto_tare_switch, LV_STATE_CHECKED);
    
    // Save button
    save_btn = lv_btn_create(container);
    lv_obj_set_size(save_btn, 120, 40);
    lv_obj_align(save_btn, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_obj_set_style_bg_color(save_btn, COLOR_ACCENT_AMBER, 0);
    lv_obj_set_style_radius(save_btn, RADIUS_NORMAL, 0);
    
    field_indicators[FIELD_SAVE] = save_btn;  // Use button itself as indicator
    
    lv_obj_t* save_label = lv_label_create(save_btn);
    lv_label_set_text(save_label, "Save");
    lv_obj_set_style_text_font(save_label, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(save_label, COLOR_BG_DARK, 0);
    lv_obj_center(save_label);
    
    // Hint
    lv_obj_t* hint = lv_label_create(container);
    lv_label_set_text(hint, "Rotate to navigate • Press to edit • Long press to exit");
    lv_obj_set_style_text_font(hint, FONT_SMALL, 0);
    lv_obj_set_style_text_color(hint, COLOR_TEXT_MUTED, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -20);
    
    // Initial highlight
    highlight_field(selected_field, true);
    
    LOG_I("BBW settings screen created");
    return screen;
}

// =============================================================================
// Update
// =============================================================================

void screen_bbw_update(const bbw_settings_t* settings) {
    if (!settings || !screen) return;
    
    current_settings = *settings;
    update_display();
}

static void update_display() {
    char buf[16];
    
    snprintf(buf, sizeof(buf), "%.1fg", current_settings.target_weight);
    lv_label_set_text(target_value, buf);
    
    snprintf(buf, sizeof(buf), "%.1fg", current_settings.dose_weight);
    lv_label_set_text(dose_value, buf);
    
    snprintf(buf, sizeof(buf), "%.1fg", current_settings.stop_offset);
    lv_label_set_text(offset_value, buf);
    
    if (current_settings.auto_stop) {
        lv_obj_add_state(auto_stop_switch, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(auto_stop_switch, LV_STATE_CHECKED);
    }
    
    if (current_settings.auto_tare) {
        lv_obj_add_state(auto_tare_switch, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(auto_tare_switch, LV_STATE_CHECKED);
    }
    
    update_ratio();
}

static void update_ratio() {
    if (current_settings.dose_weight > 0) {
        float ratio = current_settings.target_weight / current_settings.dose_weight;
        char buf[24];
        snprintf(buf, sizeof(buf), "Ratio: 1:%.1f", ratio);
        lv_label_set_text(ratio_label, buf);
    }
}

static void highlight_field(int field, bool highlight) {
    if (field < 0 || field >= FIELD_COUNT) return;
    
    lv_obj_t* indicator = field_indicators[field];
    if (!indicator) return;
    
    if (field == FIELD_SAVE) {
        // Save button - change color
        if (highlight) {
            lv_obj_set_style_bg_color(indicator, COLOR_SUCCESS, 0);
        } else {
            lv_obj_set_style_bg_color(indicator, COLOR_ACCENT_AMBER, 0);
        }
    } else {
        // Other fields - show/hide indicator bar
        if (highlight) {
            lv_obj_set_style_bg_opa(indicator, LV_OPA_COVER, 0);
        } else {
            lv_obj_set_style_bg_opa(indicator, LV_OPA_TRANSP, 0);
        }
    }
}

// =============================================================================
// Navigation
// =============================================================================

void screen_bbw_navigate(int direction) {
    if (editing) {
        // Adjust current value
        switch (selected_field) {
            case FIELD_TARGET_WEIGHT:
                current_settings.target_weight += direction * 0.5f;
                current_settings.target_weight = constrain(current_settings.target_weight, 
                                                           BBW_MIN_TARGET_WEIGHT, BBW_MAX_TARGET_WEIGHT);
                break;
            case FIELD_DOSE_WEIGHT:
                current_settings.dose_weight += direction * 0.5f;
                current_settings.dose_weight = constrain(current_settings.dose_weight,
                                                         BBW_MIN_DOSE_WEIGHT, BBW_MAX_DOSE_WEIGHT);
                break;
            case FIELD_STOP_OFFSET:
                current_settings.stop_offset += direction * 0.5f;
                current_settings.stop_offset = constrain(current_settings.stop_offset,
                                                         BBW_MIN_STOP_OFFSET, BBW_MAX_STOP_OFFSET);
                break;
            default:
                break;
        }
        update_display();
    } else {
        // Navigate between fields
        highlight_field(selected_field, false);
        selected_field += direction;
        if (selected_field < 0) selected_field = FIELD_COUNT - 1;
        if (selected_field >= FIELD_COUNT) selected_field = 0;
        highlight_field(selected_field, true);
    }
}

void screen_bbw_select(void) {
    switch (selected_field) {
        case FIELD_TARGET_WEIGHT:
        case FIELD_DOSE_WEIGHT:
        case FIELD_STOP_OFFSET:
            // Toggle edit mode
            editing = !editing;
            if (editing) {
                // Highlight value in edit mode
                lv_obj_t* value = (selected_field == FIELD_TARGET_WEIGHT) ? target_value :
                                  (selected_field == FIELD_DOSE_WEIGHT) ? dose_value : offset_value;
                lv_obj_set_style_text_color(value, COLOR_SUCCESS, 0);
            } else {
                // Restore color
                lv_obj_t* value = (selected_field == FIELD_TARGET_WEIGHT) ? target_value :
                                  (selected_field == FIELD_DOSE_WEIGHT) ? dose_value : offset_value;
                lv_color_t color = (selected_field == FIELD_TARGET_WEIGHT) ? COLOR_ACCENT_AMBER : COLOR_TEXT_PRIMARY;
                lv_obj_set_style_text_color(value, color, 0);
            }
            break;
            
        case FIELD_AUTO_STOP:
            current_settings.auto_stop = !current_settings.auto_stop;
            update_display();
            break;
            
        case FIELD_AUTO_TARE:
            current_settings.auto_tare = !current_settings.auto_tare;
            update_display();
            break;
            
        case FIELD_SAVE:
            if (save_callback) {
                save_callback(&current_settings);
            }
            break;
    }
}

bool screen_bbw_is_editing(void) {
    return editing;
}

void screen_bbw_set_save_callback(bbw_save_callback_t callback) {
    save_callback = callback;
}

