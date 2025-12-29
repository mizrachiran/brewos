/**
 * BrewOS Rotary Encoder Driver Implementation
 * 
 * Uses official ESP32_Knob and ESP32_Button libraries for reliable
 * timer-based input handling with proper debouncing.
 */

#include "display/encoder.h"
#include "display/display.h"
#include "config.h"
#include "ESP_Knob.h"
#include "Button.h"

// Global encoder instance
Encoder encoder;

// Static pointer for callbacks
static Encoder* encoderInstance = nullptr;

// =============================================================================
// Callback wrappers for ESP32_Knob library
// =============================================================================

static void onKnobLeftCallback(int count, void* usr_data) {
    if (encoderInstance) {
        encoderInstance->onKnobLeft(count);
    }
}

static void onKnobRightCallback(int count, void* usr_data) {
    if (encoderInstance) {
        encoderInstance->onKnobRight(count);
    }
}

// =============================================================================
// Callback wrappers for ESP32_Button library
// =============================================================================

static void onButtonSingleClickCallback(void* button_handle, void* usr_data) {
    if (encoderInstance) {
        encoderInstance->onButtonSingleClick();
    }
}

static void onButtonDoubleClickCallback(void* button_handle, void* usr_data) {
    if (encoderInstance) {
        encoderInstance->onButtonDoubleClick();
    }
}

static void onButtonLongPressCallback(void* button_handle, void* usr_data) {
    if (encoderInstance) {
        encoderInstance->onButtonLongPress();
    }
}

// =============================================================================
// Encoder Implementation
// =============================================================================

Encoder::Encoder()
    : _indev(nullptr)
    , _knob(nullptr)
    , _button(nullptr)
    , _position(0)
    , _lastReportedPosition(0)
    , _lastLvglPosition(0)
    , _buttonPressed(false)
    , _buttonState(BTN_RELEASED)
    , _lastReportedButtonState(BTN_RELEASED)
    , _callback(nullptr) {
}

Encoder::~Encoder() {
    if (_knob) {
        _knob->del();
        delete _knob;
        _knob = nullptr;
    }
    if (_button) {
        _button->del();
        delete _button;
        _button = nullptr;
    }
}

bool Encoder::begin() {
    LOG_I("Initializing encoder with ESP32_Knob and ESP32_Button libraries...");
    
    // Store instance for callbacks
    encoderInstance = this;
    
    // Create and initialize the Knob (encoder rotation)
    _knob = new ESP_Knob(ENCODER_A_PIN, ENCODER_B_PIN);
    if (!_knob) {
        LOG_E("Failed to create ESP_Knob");
        return false;
    }
    
    // Invert direction to match physical rotation with UI expectations
    _knob->invertDirection();
    
    _knob->begin();
    
    // Attach rotation callbacks
    _knob->attachLeftEventCallback(onKnobLeftCallback);
    _knob->attachRightEventCallback(onKnobRightCallback);
    
    // Create and initialize the Button
    _button = new Button((gpio_num_t)ENCODER_BTN_PIN, true);  // true = pullup
    if (!_button) {
        LOG_E("Failed to create Button");
        return false;
    }
    
    // Attach button callbacks
    // SingleClick = released after short press (100-2000ms)
    // DoubleClick = two clicks within 300ms
    // LongPressStart = fires immediately when 2s threshold reached (while still holding)
    _button->attachSingleClickEventCb(onButtonSingleClickCallback, nullptr);
    _button->attachDoubleClickEventCb(onButtonDoubleClickCallback, nullptr);
    _button->attachLongPressStartEventCb(onButtonLongPressCallback, nullptr);
    
    // Initialize LVGL input device
    lv_indev_drv_init(&_indevDrv);
    _indevDrv.type = LV_INDEV_TYPE_ENCODER;
    _indevDrv.read_cb = readCallback;
    _indevDrv.user_data = this;
    
    _indev = lv_indev_drv_register(&_indevDrv);
    
    // Create a group and set it as default for encoder navigation
    lv_group_t* group = lv_group_create();
    lv_group_set_default(group);
    lv_indev_set_group(_indev, group);
    
    LOG_I("Encoder initialized on pins A=%d, B=%d, BTN=%d (using ESP libraries)",
          ENCODER_A_PIN, ENCODER_B_PIN, ENCODER_BTN_PIN);
    
    return true;
}

void Encoder::resetPosition() {
    _position = 0;
    _lastReportedPosition = 0;
    _lastLvglPosition = 0;
    if (_knob) {
        _knob->clearCountValue();
    }
}

void Encoder::onKnobLeft(int count) {
    _position = _position - 1;
    LOG_I("Encoder rotate: -1 (count=%d)", count);
}

void Encoder::onKnobRight(int count) {
    _position = _position + 1;
    LOG_I("Encoder rotate: +1 (count=%d)", count);
}

void Encoder::onButtonSingleClick() {
    _buttonState = BTN_PRESSED;
    LOG_I("Encoder button: PRESS");
}

void Encoder::onButtonDoubleClick() {
    _buttonState = BTN_DOUBLE_PRESSED;
    LOG_I("Encoder button: DOUBLE_PRESS");
}

void Encoder::onButtonLongPress() {
    _buttonState = BTN_LONG_PRESSED;
    LOG_I("Encoder button: LONG_PRESS");
}

void Encoder::update() {
    // Check for position change
    int32_t diff = _position - _lastReportedPosition;
    
    // Check for button state change - atomically grab and clear
    button_state_t btnToReport = _buttonState;
    if (btnToReport != BTN_RELEASED) {
        _buttonState = BTN_RELEASED;  // Clear immediately to not miss next press
    }
    
    // Report any events
    if (diff != 0 || btnToReport != BTN_RELEASED) {
        // Note: Don't reset idle timer here - let the callback handle it
        // This allows the callback to check if display was sleeping first
        
        if (_callback) {
            _callback(diff, btnToReport);
        }
        
        _lastReportedPosition = _position;
    }
}

void Encoder::readCallback(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    Encoder* enc = (Encoder*)drv->user_data;
    
    // Don't send encoder rotation to LVGL - we handle navigation via direct callbacks
    // This prevents double navigation (once from LVGL, once from our callback)
    // Keep LVGL position tracking synced to prevent accumulation
    enc->_lastLvglPosition = enc->_position;
    data->enc_diff = 0;
    
    // Report button state (still needed for LVGL focus)
    if (enc->_buttonPressed) {
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
