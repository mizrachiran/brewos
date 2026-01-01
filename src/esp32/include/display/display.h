/**
 * BrewOS Display Driver Interface
 * 
 * Handles display initialization, backlight control, and LVGL integration
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <lvgl.h>
#include "display_config.h"

// =============================================================================
// Display Driver Class
// =============================================================================

class Display {
public:
    Display();
    
    /**
     * Initialize the display and LVGL
     * Must be called before any other display operations
     */
    bool begin();
    
    /**
     * Update display (backlight idle handling)
     * Note: LVGL timer handler now runs in dedicated FreeRTOS task
     * This function only handles backlight idle timeout
     */
    void update();
    
    /**
     * Set backlight brightness (0-255)
     */
    void setBacklight(uint8_t brightness);
    
    /**
     * Get current backlight brightness
     */
    uint8_t getBacklight() const { return _backlightLevel; }
    
    /**
     * Turn backlight on/off
     */
    void backlightOn();
    void backlightOff();
    
    /**
     * Handle idle timeout for backlight dimming
     * Call this when there's user input to reset the idle timer
     */
    void resetIdleTimer();
    
    /**
     * Check if display is dimmed due to inactivity
     */
    bool isDimmed() const { return _isDimmed; }
    
    /**
     * Get the LVGL display pointer (for advanced usage)
     */
    lv_disp_t* getDisplay() const { return _display; }
    
    /**
     * Get screen width/height
     */
    uint16_t width() const { return DISPLAY_WIDTH; }
    uint16_t height() const { return DISPLAY_HEIGHT; }
    
private:
    // LVGL display driver
    lv_disp_t* _display;
    lv_disp_draw_buf_t _drawBuf;
    lv_disp_drv_t _dispDrv;
    
    // Display buffers (allocated in PSRAM)
    lv_color_t* _buf1;
    lv_color_t* _buf2;
    
    // Backlight
    uint8_t _backlightLevel;
    uint8_t _backlightSaved;    // Saved level before dim/off
    bool _isDimmed;
    unsigned long _lastActivityTime;
    
    // FreeRTOS task for LVGL timer handler
    TaskHandle_t _lvglTaskHandle;
    static void lvglTaskCode(void* parameter);
    static const int LVGL_TASK_STACK_SIZE = 4096;  // 4KB stack
    static const int LVGL_TASK_PRIORITY = 2;       // Above idle, below critical tasks
    static const int LVGL_TASK_CORE = 1;           // Run on Core 1 (same as Arduino loop)
    static const int LVGL_TASK_INTERVAL_MS = 16;   // ~16ms = ~60 FPS (optimal for smooth animations)
    
    // Internal methods
    void initHardware();
    void initLVGL();
    void updateBacklightIdle();
    void startLVGLTask();
    
    // Static callbacks for LVGL
    static void flushCallback(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p);
};

// Global display instance
extern Display display;

#endif // DISPLAY_H

