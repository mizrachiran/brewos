/**
 * BrewOS Brew-by-Weight Controller
 * 
 * Monitors scale weight during brewing and signals the Pico
 * to stop when target weight is reached.
 * 
 * Features:
 * - Configurable target weight
 * - Pre-infusion offset (stop early to account for drip)
 * - Dose weight for ratio calculation
 * - Auto-stop toggle
 * - Settings persisted in NVS
 */

#ifndef BREW_BY_WEIGHT_H
#define BREW_BY_WEIGHT_H

#include <Arduino.h>
#include <Preferences.h>

// =============================================================================
// Configuration
// =============================================================================

// Default values
#define BBW_DEFAULT_TARGET_WEIGHT     36.0f   // grams
#define BBW_DEFAULT_DOSE_WEIGHT       18.0f   // grams  
#define BBW_DEFAULT_STOP_OFFSET       2.0f    // grams (stop early for drip)
#define BBW_DEFAULT_AUTO_STOP         true
#define BBW_DEFAULT_AUTO_TARE         true    // Tare when brew starts

// Limits
#define BBW_MIN_TARGET_WEIGHT         10.0f   // grams
#define BBW_MAX_TARGET_WEIGHT         100.0f  // grams
#define BBW_MIN_DOSE_WEIGHT           5.0f    // grams
#define BBW_MAX_DOSE_WEIGHT           30.0f   // grams
#define BBW_MIN_STOP_OFFSET           0.0f    // grams
#define BBW_MAX_STOP_OFFSET           10.0f   // grams

// NVS keys
#define NVS_BBW_NAMESPACE             "bbw"
#define NVS_BBW_TARGET                "target"
#define NVS_BBW_DOSE                  "dose"
#define NVS_BBW_OFFSET                "offset"
#define NVS_BBW_AUTO_STOP             "auto_stop"
#define NVS_BBW_AUTO_TARE             "auto_tare"

// =============================================================================
// Settings Structure
// =============================================================================

typedef struct {
    float target_weight;    // Target output weight (grams)
    float dose_weight;      // Input dose weight (grams)
    float stop_offset;      // Stop this many grams early (for drip)
    bool auto_stop;         // Automatically signal stop at target
    bool auto_tare;         // Tare scale when brew starts
} bbw_settings_t;

// =============================================================================
// State Structure  
// =============================================================================

typedef struct {
    bool active;            // Brew-by-weight session active
    bool target_reached;    // Target weight has been reached
    bool stop_signaled;     // Stop signal has been sent to Pico
    float start_weight;     // Weight at brew start (for relative measurement)
    float current_weight;   // Current weight reading
    float target_ratio;     // Calculated ratio (output/dose)
    uint32_t start_time;    // Brew start timestamp
} bbw_state_t;

// =============================================================================
// Brew-by-Weight Controller Class
// =============================================================================

class BrewByWeight {
public:
    BrewByWeight();
    
    /**
     * Initialize - load settings from NVS
     */
    bool begin();
    
    /**
     * Process - call in main loop
     * @param is_brewing Current brewing state from Pico
     * @param scale_weight Current weight from scale (grams)
     * @param scale_connected Whether scale is connected
     */
    void update(bool is_brewing, float scale_weight, bool scale_connected);
    
    // =========================================================================
    // Settings
    // =========================================================================
    
    bbw_settings_t getSettings() const { return _settings; }
    void setSettings(const bbw_settings_t& settings);
    
    float getTargetWeight() const { return _settings.target_weight; }
    void setTargetWeight(float weight);
    
    float getDoseWeight() const { return _settings.dose_weight; }
    void setDoseWeight(float weight);
    
    float getStopOffset() const { return _settings.stop_offset; }
    void setStopOffset(float offset);
    
    bool isAutoStopEnabled() const { return _settings.auto_stop; }
    void setAutoStop(bool enabled);
    
    bool isAutoTareEnabled() const { return _settings.auto_tare; }
    void setAutoTare(bool enabled);
    
    // =========================================================================
    // State
    // =========================================================================
    
    bbw_state_t getState() const { return _state; }
    bool isActive() const { return _state.active; }
    bool isTargetReached() const { return _state.target_reached; }
    float getProgress() const;  // 0.0 - 1.0
    float getCurrentRatio() const;  // output / dose
    
    // =========================================================================
    // Actions
    // =========================================================================
    
    /**
     * Manually trigger stop signal
     */
    void triggerStop();
    
    /**
     * Reset state (e.g., after shot complete screen dismissed)
     */
    void reset();
    
    // =========================================================================
    // Callbacks
    // =========================================================================
    
    typedef std::function<void()> stop_callback_t;
    typedef std::function<void()> tare_callback_t;
    typedef std::function<void(float weight, float target)> progress_callback_t;
    
    void onStop(stop_callback_t cb) { _onStop = cb; }
    void onTare(tare_callback_t cb) { _onTare = cb; }
    void onProgress(progress_callback_t cb) { _onProgress = cb; }

private:
    bbw_settings_t _settings;
    bbw_state_t _state;
    
    bool _wasBrewing;
    uint32_t _lastProgressCallback;
    
    // Callbacks
    stop_callback_t _onStop;
    tare_callback_t _onTare;
    progress_callback_t _onProgress;
    
    void loadSettings();
    void saveSettings();
    void startSession(float initial_weight);
    void endSession();
    void checkTarget();
};

// Global instance
extern BrewByWeight brewByWeight;

#endif // BREW_BY_WEIGHT_H

