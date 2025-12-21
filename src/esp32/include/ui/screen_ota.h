/**
 * BrewOS OTA Update Screen
 * 
 * Full-screen OTA update display:
 * - Update icon
 * - Update message
 * - Progress indication
 */

#ifndef SCREEN_OTA_H
#define SCREEN_OTA_H

#include <lvgl.h>
#include "ui.h"

/**
 * Create the OTA screen
 */
lv_obj_t* screen_ota_create(void);

/**
 * Set OTA update message
 */
void screen_ota_set(const char* message);

/**
 * Clear OTA display
 */
void screen_ota_clear(void);

#endif // SCREEN_OTA_H

