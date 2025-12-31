/**
 * BrewOS Alarm Screen Implementation
 * 
 * Full-screen alarm display
 */

#include "platform/platform.h"
#include "ui/screen_alarm.h"
#include "display/theme.h"
#include "display/display_config.h"

// Static elements
static lv_obj_t* screen = nullptr;

// =============================================================================
// Screen Creation
// =============================================================================

lv_obj_t* screen_alarm_create(void) {
    LOG_I("Creating alarm screen...");
    
    screen = lv_obj_create(NULL);
    // Ensure screen fills entire display
    lv_obj_set_size(screen, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_pos(screen, 0, 0);
    lv_obj_set_style_bg_color(screen, COLOR_BG_DARK, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // Alarm title
    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "ALARM");
    lv_obj_set_style_text_font(title, FONT_HUGE, 0);
    lv_obj_set_style_text_color(title, COLOR_ERROR, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -80);
    
    // Alarm code label (will be updated when alarm is set)
    lv_obj_t* code_label = lv_label_create(screen);
    lv_label_set_text(code_label, "");
    lv_obj_set_style_text_font(code_label, FONT_LARGE, 0);
    lv_obj_set_style_text_color(code_label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(code_label, LV_ALIGN_CENTER, 0, -20);
    
    // Alarm message label (will be updated when alarm is set)
    lv_obj_t* msg_label = lv_label_create(screen);
    lv_label_set_text(msg_label, "");
    lv_obj_set_style_text_font(msg_label, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(msg_label, COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_align(msg_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(msg_label, DISPLAY_WIDTH - 80);
    lv_obj_align(msg_label, LV_ALIGN_CENTER, 0, 40);
    
    // Store label pointers for updates (using user_data)
    lv_obj_set_user_data(code_label, (void*)1);  // Mark as code label
    lv_obj_set_user_data(msg_label, (void*)2);   // Mark as message label
    
    LOG_I("Alarm screen created");
    return screen;
}

// =============================================================================
// Screen Update
// =============================================================================

void screen_alarm_set(uint8_t code, const char* message) {
    if (!screen) return;
    LOG_I("Alarm set: code=0x%02X, msg=%s", code, message ? message : "(null)");
    
    // Find and update code and message labels
    uint32_t child_cnt = lv_obj_get_child_cnt(screen);
    for (uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t* child = lv_obj_get_child(screen, i);
        if (!child) continue;  // Safety check
        
        void* user_data = lv_obj_get_user_data(child);
        
        if (user_data == (void*)1) {  // Code label
            char code_str[16];
            snprintf(code_str, sizeof(code_str), "0x%02X", code);
            lv_label_set_text(child, code_str);
        } else if (user_data == (void*)2) {  // Message label
            lv_label_set_text(child, message ? message : "");
        }
    }
}

void screen_alarm_clear(void) {
    LOG_I("Alarm cleared");
    
    // Clear code and message labels
    if (!screen) return;
    uint32_t child_cnt = lv_obj_get_child_cnt(screen);
    for (uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t* child = lv_obj_get_child(screen, i);
        if (!child) continue;  // Safety check
        
        void* user_data = lv_obj_get_user_data(child);
        
        if (user_data == (void*)1 || user_data == (void*)2) {  // Code or message label
            lv_label_set_text(child, "");
        }
    }
}

