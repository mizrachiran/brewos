/**
 * BrewOS Brew-by-Weight Controller Implementation
 * 
 * Monitors scale during brewing and triggers stop at target weight.
 */

#include "brew_by_weight.h"
#include "config.h"

// Global instance
BrewByWeight brewByWeight;

// =============================================================================
// Constructor
// =============================================================================

BrewByWeight::BrewByWeight()
    : _wasBrewing(false)
    , _lastProgressCallback(0)
    , _onStop(nullptr)
    , _onTare(nullptr)
    , _onProgress(nullptr) {
    
    // Initialize settings with defaults
    _settings.target_weight = BBW_DEFAULT_TARGET_WEIGHT;
    _settings.dose_weight = BBW_DEFAULT_DOSE_WEIGHT;
    _settings.stop_offset = BBW_DEFAULT_STOP_OFFSET;
    _settings.auto_stop = BBW_DEFAULT_AUTO_STOP;
    _settings.auto_tare = BBW_DEFAULT_AUTO_TARE;
    
    // Initialize state
    memset(&_state, 0, sizeof(_state));
}

// =============================================================================
// Initialization
// =============================================================================

bool BrewByWeight::begin() {
    LOG_I("Initializing Brew-by-Weight controller...");
    
    loadSettings();
    
    LOG_I("BBW settings: target=%.1fg, dose=%.1fg, offset=%.1fg, auto_stop=%s, auto_tare=%s",
          _settings.target_weight,
          _settings.dose_weight,
          _settings.stop_offset,
          _settings.auto_stop ? "true" : "false",
          _settings.auto_tare ? "true" : "false");
    
    return true;
}

// =============================================================================
// Main Update Loop
// =============================================================================

void BrewByWeight::update(bool is_brewing, float scale_weight, bool scale_connected) {
    uint32_t now = millis();
    
    // Detect brew start
    if (is_brewing && !_wasBrewing) {
        LOG_I("BBW: Brew started");
        startSession(scale_weight);
        
        // Auto-tare at brew start
        if (_settings.auto_tare && scale_connected && _onTare) {
            LOG_I("BBW: Auto-tare triggered");
            _onTare();
        }
    }
    
    // Detect brew end
    if (!is_brewing && _wasBrewing) {
        LOG_I("BBW: Brew ended - final weight: %.1fg", _state.current_weight);
        endSession();
    }
    
    _wasBrewing = is_brewing;
    
    // Update state during brewing
    if (_state.active && scale_connected) {
        _state.current_weight = scale_weight;
        
        // Calculate effective weight (relative to start if auto-tare)
        float effective_weight = scale_weight;
        if (_settings.auto_tare) {
            effective_weight = scale_weight - _state.start_weight;
            if (effective_weight < 0) effective_weight = 0;
        }
        _state.current_weight = effective_weight;
        
        // Check if target reached
        if (!_state.target_reached && !_state.stop_signaled) {
            checkTarget();
        }
        
        // Progress callback (max 10Hz)
        if (_onProgress && (now - _lastProgressCallback > 100)) {
            _lastProgressCallback = now;
            _onProgress(_state.current_weight, _settings.target_weight);
        }
    }
}

// =============================================================================
// Settings
// =============================================================================

void BrewByWeight::setSettings(const bbw_settings_t& settings) {
    _settings = settings;
    
    // Clamp values
    _settings.target_weight = constrain(_settings.target_weight, BBW_MIN_TARGET_WEIGHT, BBW_MAX_TARGET_WEIGHT);
    _settings.dose_weight = constrain(_settings.dose_weight, BBW_MIN_DOSE_WEIGHT, BBW_MAX_DOSE_WEIGHT);
    _settings.stop_offset = constrain(_settings.stop_offset, BBW_MIN_STOP_OFFSET, BBW_MAX_STOP_OFFSET);
    
    saveSettings();
    
    LOG_I("BBW: Settings updated");
}

void BrewByWeight::setTargetWeight(float weight) {
    _settings.target_weight = constrain(weight, BBW_MIN_TARGET_WEIGHT, BBW_MAX_TARGET_WEIGHT);
    saveSettings();
    LOG_I("BBW: Target weight set to %.1fg", _settings.target_weight);
}

void BrewByWeight::setDoseWeight(float weight) {
    _settings.dose_weight = constrain(weight, BBW_MIN_DOSE_WEIGHT, BBW_MAX_DOSE_WEIGHT);
    saveSettings();
    LOG_I("BBW: Dose weight set to %.1fg", _settings.dose_weight);
}

void BrewByWeight::setStopOffset(float offset) {
    _settings.stop_offset = constrain(offset, BBW_MIN_STOP_OFFSET, BBW_MAX_STOP_OFFSET);
    saveSettings();
    LOG_I("BBW: Stop offset set to %.1fg", _settings.stop_offset);
}

void BrewByWeight::setAutoStop(bool enabled) {
    _settings.auto_stop = enabled;
    saveSettings();
    LOG_I("BBW: Auto-stop %s", enabled ? "enabled" : "disabled");
}

void BrewByWeight::setAutoTare(bool enabled) {
    _settings.auto_tare = enabled;
    saveSettings();
    LOG_I("BBW: Auto-tare %s", enabled ? "enabled" : "disabled");
}

// =============================================================================
// State Queries
// =============================================================================

float BrewByWeight::getProgress() const {
    if (_settings.target_weight <= 0) return 0;
    float progress = _state.current_weight / _settings.target_weight;
    return constrain(progress, 0.0f, 1.0f);
}

float BrewByWeight::getCurrentRatio() const {
    if (_settings.dose_weight <= 0) return 0;
    return _state.current_weight / _settings.dose_weight;
}

// =============================================================================
// Actions
// =============================================================================

void BrewByWeight::triggerStop() {
    if (_state.stop_signaled) return;
    
    _state.stop_signaled = true;
    _state.target_reached = true;
    
    if (_onStop) {
        LOG_I("BBW: Manual stop triggered at %.1fg", _state.current_weight);
        _onStop();
    }
}

void BrewByWeight::reset() {
    memset(&_state, 0, sizeof(_state));
    LOG_D("BBW: State reset");
}

// =============================================================================
// Internal Methods
// =============================================================================

void BrewByWeight::startSession(float initial_weight) {
    memset(&_state, 0, sizeof(_state));
    _state.active = true;
    _state.start_weight = initial_weight;
    _state.start_time = millis();
    _state.target_ratio = _settings.target_weight / _settings.dose_weight;
}

void BrewByWeight::endSession() {
    _state.active = false;
    
    // Log final stats
    uint32_t duration = millis() - _state.start_time;
    float ratio = getCurrentRatio();
    
    LOG_I("BBW: Session complete - %.1fg in %lus (1:%.1f ratio)",
          _state.current_weight,
          duration / 1000,
          ratio);
}

void BrewByWeight::checkTarget() {
    // Calculate stop threshold (target minus offset for drip)
    float stop_threshold = _settings.target_weight - _settings.stop_offset;
    
    if (_state.current_weight >= stop_threshold) {
        _state.target_reached = true;
        
        if (_settings.auto_stop && _onStop) {
            _state.stop_signaled = true;
            LOG_I("BBW: Target reached! Signaling stop at %.1fg (target: %.1fg, offset: %.1fg)",
                  _state.current_weight,
                  _settings.target_weight,
                  _settings.stop_offset);
            _onStop();
        } else {
            LOG_I("BBW: Target reached at %.1fg (auto-stop disabled)",
                  _state.current_weight);
        }
    }
}

void BrewByWeight::loadSettings() {
    Preferences prefs;
    prefs.begin(NVS_BBW_NAMESPACE, true);
    
    _settings.target_weight = prefs.getFloat(NVS_BBW_TARGET, BBW_DEFAULT_TARGET_WEIGHT);
    _settings.dose_weight = prefs.getFloat(NVS_BBW_DOSE, BBW_DEFAULT_DOSE_WEIGHT);
    _settings.stop_offset = prefs.getFloat(NVS_BBW_OFFSET, BBW_DEFAULT_STOP_OFFSET);
    _settings.auto_stop = prefs.getBool(NVS_BBW_AUTO_STOP, BBW_DEFAULT_AUTO_STOP);
    _settings.auto_tare = prefs.getBool(NVS_BBW_AUTO_TARE, BBW_DEFAULT_AUTO_TARE);
    
    prefs.end();
}

void BrewByWeight::saveSettings() {
    Preferences prefs;
    prefs.begin(NVS_BBW_NAMESPACE, false);
    
    prefs.putFloat(NVS_BBW_TARGET, _settings.target_weight);
    prefs.putFloat(NVS_BBW_DOSE, _settings.dose_weight);
    prefs.putFloat(NVS_BBW_OFFSET, _settings.stop_offset);
    prefs.putBool(NVS_BBW_AUTO_STOP, _settings.auto_stop);
    prefs.putBool(NVS_BBW_AUTO_TARE, _settings.auto_tare);
    
    prefs.end();
}

