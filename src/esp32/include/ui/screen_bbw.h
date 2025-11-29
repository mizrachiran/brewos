/**
 * BrewOS Brew-by-Weight Settings Screen
 * 
 * Configure target weight, dose, and auto-stop settings
 */

#ifndef SCREEN_BBW_H
#define SCREEN_BBW_H

#include <lvgl.h>
#include "brew_by_weight.h"

/**
 * Create the brew-by-weight settings screen
 */
lv_obj_t* screen_bbw_create(void);

/**
 * Update screen with current settings
 */
void screen_bbw_update(const bbw_settings_t* settings);

/**
 * Navigate between fields (encoder rotation)
 */
void screen_bbw_navigate(int direction);

/**
 * Select/edit current field (encoder press)
 */
void screen_bbw_select(void);

/**
 * Check if currently editing a value
 */
bool screen_bbw_is_editing(void);

/**
 * Callback when settings are saved
 */
typedef void (*bbw_save_callback_t)(const bbw_settings_t* settings);
void screen_bbw_set_save_callback(bbw_save_callback_t callback);

#endif // SCREEN_BBW_H

