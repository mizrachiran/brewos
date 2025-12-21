/**
 * BrewOS OTA Update Screen Implementation
 * 
 * Full-screen OTA update display
 */

#include "platform/platform.h"
#include "ui/screen_ota.h"
#include "display/theme.h"
#include "display/display_config.h"

// Static elements
static lv_obj_t* screen = nullptr;

// =============================================================================
// Screen Creation
// =============================================================================

lv_obj_t* screen_ota_create(void) {
    LOG_I("Creating OTA screen...");
    
    screen = lv_obj_create(NULL);
    // Ensure screen fills entire display
    lv_obj_set_size(screen, DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_obj_set_pos(screen, 0, 0);
    lv_obj_set_style_bg_color(screen, COLOR_BG_DARK, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // OTA title
    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "UPDATING");
    lv_obj_set_style_text_font(title, FONT_HUGE, 0);
    lv_obj_set_style_text_color(title, COLOR_ACCENT_PRIMARY, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -80);
    
    // OTA message label (will be updated when OTA is set)
    lv_obj_t* msg_label = lv_label_create(screen);
    lv_label_set_text(msg_label, "Please wait...");
    lv_obj_set_style_text_font(msg_label, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(msg_label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_align(msg_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(msg_label, DISPLAY_WIDTH - 80);
    lv_obj_align(msg_label, LV_ALIGN_CENTER, 0, -20);
    
    // Progress indicator (animated dots)
    lv_obj_t* progress_label = lv_label_create(screen);
    lv_label_set_text(progress_label, "...");
    lv_obj_set_style_text_font(progress_label, FONT_LARGE, 0);
    lv_obj_set_style_text_color(progress_label, COLOR_TEXT_MUTED, 0);
    lv_obj_align(progress_label, LV_ALIGN_CENTER, 0, 40);
    
    // Store label pointers for updates (using user_data)
    lv_obj_set_user_data(msg_label, (void*)1);      // Mark as message label
    lv_obj_set_user_data(progress_label, (void*)2); // Mark as progress label
    
    LOG_I("OTA screen created");
    return screen;
}

// =============================================================================
// Screen Update
// =============================================================================

void screen_ota_set(const char* message) {
    if (!screen) return;
    LOG_I("OTA set: %s", message ? message : "(null)");
    
    // Find and update message label
    uint32_t child_cnt = lv_obj_get_child_cnt(screen);
    for (uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t* child = lv_obj_get_child(screen, i);
        void* user_data = lv_obj_get_user_data(child);
        
        if (user_data == (void*)1) {  // Message label
            lv_label_set_text(child, message ? message : "Please wait...");
        }
    }
}

void screen_ota_clear(void) {
    LOG_I("OTA cleared");
    
    // Clear message label
    if (!screen) return;
    uint32_t child_cnt = lv_obj_get_child_cnt(screen);
    for (uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t* child = lv_obj_get_child(screen, i);
        void* user_data = lv_obj_get_user_data(child);
        
        if (user_data == (void*)1) {  // Message label
            lv_label_set_text(child, "Please wait...");
        }
    }
}

