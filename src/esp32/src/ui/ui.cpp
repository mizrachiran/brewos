/**
 * BrewOS UI Manager Implementation
 * 
 * Manages all screens and navigation
 */

#include "platform/platform.h"
#include "ui/ui.h"
#include "ui/screen_setup.h"
#include "ui/screen_idle.h"
#include "ui/screen_home.h"
#include "ui/screen_brewing.h"
#include "ui/screen_complete.h"
#include "ui/screen_settings.h"
#include "ui/screen_cloud.h"
#include "ui/screen_alarm.h"
#include "ui/screen_ota.h"
#include "ui/screen_splash.h"
#ifndef SIMULATOR
#include "ui/screen_temp.h"
#include "ui/screen_scale.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif
#include "display/theme.h"
#include "display/display_config.h"

// Global UI instance
UI ui;

// Local state for tracking
static bool was_brewing = false;
static uint32_t last_brew_time = 0;
static float last_brew_weight = 0;
static bool last_alarm_state = false;  // Track previous alarm state for debouncing
static uint32_t last_alarm_change_time = 0;  // Timestamp of last alarm state change

// =============================================================================
// UI Manager Implementation
// =============================================================================

UI::UI()
    : _currentScreen(SCREEN_HOME)
    , _previousScreen(SCREEN_HOME)
    , _onTurnOn(nullptr)
    , _onTurnOff(nullptr)
    , _onSetTemp(nullptr)
    , _onSetStrategy(nullptr)
    , _onTareScale(nullptr)
    , _onSetTargetWeight(nullptr)
    , _onWifiSetup(nullptr) {
    
    // Initialize state
    memset(&_state, 0, sizeof(_state));
    
    // Initialize screen pointers
    for (int i = 0; i < SCREEN_COUNT; i++) {
        _screens[i] = nullptr;
    }
}

bool UI::begin() {
    LOG_I("Initializing UI...");
    
    // Initialize theme
    theme_init();
    
    // Register theme change callback to rebuild screens
    theme_set_change_callback([]() {
        ui.rebuildScreens();
    });
    
    // Create all screens
    createSetupScreen();
    createIdleScreen();
    createHomeScreen();
    createBrewingScreen();
    createCompleteScreen();
    createSettingsScreen();
    createTempSettingsScreen();
    createScaleScreen();
    createCloudScreen();
    createAlarmScreen();
    createOtaScreen();
    createSplashScreen();
    
    // Show splash screen immediately (no animation for first load)
    if (_screens[SCREEN_SPLASH]) {
        _currentScreen = SCREEN_SPLASH;
        _previousScreen = SCREEN_SPLASH;
        lv_scr_load(_screens[SCREEN_SPLASH]);
        
        // Force LVGL to process and flush the entire screen
        lv_obj_invalidate(_screens[SCREEN_SPLASH]);
        for (int i = 0; i < 15; i++) {
            uint32_t tasks = lv_timer_handler();
            if (tasks == 0 && i > 5) break;
#ifndef SIMULATOR
            vTaskDelay(pdMS_TO_TICKS(5));
#else
            platform_delay(5);
#endif
        }
        LOG_I("Initial screen (SPLASH) loaded");
    }
    
    LOG_I("UI initialized with %d screens", SCREEN_COUNT);
    return true;
}

void UI::update(const ui_state_t& state) {
    // Store previous state for change detection
    bool state_changed = (state.machine_state != _state.machine_state);
    bool brewing_changed = (state.is_brewing != _state.is_brewing);
    
    // Initialize alarm state tracking on first update
    static bool alarm_state_initialized = false;
    if (!alarm_state_initialized) {
        last_alarm_state = state.alarm_active;
        alarm_state_initialized = true;
    }
    
    // Store new state
    _state = state;
    
    // Auto screen switching based on machine state
    checkAutoScreenSwitch();
    
    // Track brewing state for shot complete
    if (brewing_changed) {
        if (state.is_brewing) {
            // Brewing started
            was_brewing = true;
            screen_brewing_reset();
        } else if (was_brewing) {
            // Brewing just ended - store final values for complete screen
            last_brew_time = _state.brew_time_ms;
            last_brew_weight = _state.brew_weight;
            was_brewing = false;
            
            // Show complete screen (if we have valid data)
            if (last_brew_time > 5000 || last_brew_weight > 5.0f) {
                screen_complete_update(last_brew_time, last_brew_weight, 
                                       _state.dose_weight, _state.flow_rate);
                showScreen(SCREEN_COMPLETE);
            }
        }
    }
    
    // Update current screen
    switch (_currentScreen) {
        case SCREEN_SETUP:
            updateSetupScreen();
            break;
        case SCREEN_IDLE:
            updateIdleScreen();
            break;
        case SCREEN_HOME:
            updateHomeScreen();
            // Invalidate screen after update to ensure changes are flushed
            if (_screens[SCREEN_HOME]) {
                lv_obj_invalidate(_screens[SCREEN_HOME]);
            }
            break;
        case SCREEN_BREWING:
            updateBrewingScreen();
            break;
        case SCREEN_COMPLETE:
            updateCompleteScreen();
            break;
        case SCREEN_SETTINGS:
            updateSettingsScreen();
            break;
        case SCREEN_TEMP_SETTINGS:
#ifndef SIMULATOR
            screen_temp_update(&_state);
#endif
            break;
        case SCREEN_SCALE:
#ifndef SIMULATOR
            screen_scale_update(&_state);
#endif
            break;
        case SCREEN_CLOUD:
            // Cloud screen updates are event-driven (refresh button)
            break;
        case SCREEN_ALARM:
            updateAlarmScreen();
            break;
        case SCREEN_OTA:
            updateOtaScreen();
            break;
        default:
            break;
    }
}

void UI::showScreen(screen_id_t screen) {
    if (screen >= SCREEN_COUNT || !_screens[screen]) {
        LOG_W("Invalid screen: %d", screen);
        return;
    }
    
    // Rate limit screen switches to prevent rapid toggling (min 100ms between switches)
    // Reduced from 500ms for snappier navigation
    static uint32_t last_switch_time = 0;
    uint32_t now = lv_tick_get();
    if (now - last_switch_time < 100 && _currentScreen != screen) {
        // Too fast - skip this switch (unless it's the same screen)
        return;
    }
    last_switch_time = now;
    
    // Don't switch if already on this screen
    if (_currentScreen == screen) {
        return;
    }
    
    _previousScreen = _currentScreen;
    _currentScreen = screen;
    
    // Use instant screen load for snappy transitions (no animation)
    // Animation was causing slow top-to-bottom drawing with the small LVGL buffer
    lv_scr_load(_screens[screen]);
    
    // Focus an object on the new screen (for encoder navigation)
    lv_group_t* group = lv_group_get_default();
    if (group) {
        // Focus next until we find an object on the current screen
        lv_obj_t* focused = lv_group_get_focused(group);
        if (focused && lv_obj_get_screen(focused) != _screens[screen]) {
            // Current focus is on wrong screen, find one on the right screen
            uint32_t count = lv_group_get_obj_count(group);
            for (uint32_t i = 0; i < count; i++) {
                lv_group_focus_next(group);
                focused = lv_group_get_focused(group);
                if (focused && lv_obj_get_screen(focused) == _screens[screen]) {
                    break;
                }
            }
        }
    }
    
    LOG_I("Switched to screen: %d", screen);
}

void UI::showNotification(const char* message, uint16_t duration_ms) {
    // Create notification using msgbox
    lv_obj_t* mbox = lv_msgbox_create(NULL, NULL, message, NULL, false);
    lv_obj_center(mbox);
    
    // Style the msgbox
    lv_obj_set_style_bg_color(mbox, COLOR_BG_CARD, 0);
    lv_obj_set_style_text_color(mbox, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_radius(mbox, RADIUS_NORMAL, 0);
    lv_obj_set_style_border_color(mbox, COLOR_ACCENT_AMBER, 0);
    lv_obj_set_style_border_width(mbox, 2, 0);
    
    // Auto-close timer
    lv_obj_del_delayed(mbox, duration_ms);
}

void UI::showAlarm(uint8_t code, const char* message) {
    screen_alarm_set(code, message);
    showScreen(SCREEN_ALARM);
}

void UI::clearAlarm() {
    screen_alarm_clear();
    showScreen(_previousScreen);
}

void UI::triggerWifiSetup() {
    // Rate limit to prevent repeated calls
    static unsigned long lastTrigger = 0;
    unsigned long now = platform_millis();
    if (now - lastTrigger < 2000) {  // 2 second debounce
        LOG_W("WiFi setup trigger rate limited");
        return;
    }
    lastTrigger = now;
    
    // Call the callback to reset static IP to DHCP and start AP mode
    if (_onWifiSetup) {
        _onWifiSetup();
    }
    // Show the setup screen
    showScreen(SCREEN_SETUP);
}

void UI::handleEncoder(int direction) {
    switch (_currentScreen) {
        case SCREEN_IDLE:
            // Navigate heating strategies (only for dual boiler machines)
            if (screen_idle_is_showing_strategies()) {
                screen_idle_select_strategy(
                    screen_idle_get_selected_strategy() + direction);
            }
            // For non-dual-boiler, encoder does nothing on idle screen
            break;
            
        case SCREEN_HOME:
            // Rotate from home → go to idle screen (heating strategy selection)
            // This allows quick access to turn-on options and strategy selection
            if (direction != 0) {
                showScreen(SCREEN_IDLE);
            }
            break;
            
        case SCREEN_SETTINGS:
            // Normalize direction to -1, 0, or +1 for consistent navigation
            // This ensures smooth scrolling regardless of encoder speed
            LOG_D("Settings screen encoder: direction=%d", direction);
            if (direction > 0) {
                screen_settings_navigate(1);
            } else if (direction < 0) {
                screen_settings_navigate(-1);
            }
            break;
            
        case SCREEN_TEMP_SETTINGS:
#ifndef SIMULATOR
            // Temperature adjustment
            screen_temp_encoder(direction);
#endif
            break;
            
        case SCREEN_SCALE:
#ifndef SIMULATOR
            // Scale list navigation
            screen_scale_encoder(direction);
#endif
            break;
            
        case SCREEN_CLOUD:
            // Cloud screen navigation
            screen_cloud_encoder(direction);
            break;
            
        case SCREEN_BREWING:
            // During brewing, encoder could adjust target weight
            if (_onSetTargetWeight) {
                float new_target = _state.target_weight + (direction * 0.5f);
                if (new_target >= 10.0f && new_target <= 100.0f) {
                    _onSetTargetWeight(new_target);
                }
            }
            break;
            
        default:
            break;
    }
}

void UI::handleButtonPress() {
    // Rate limit button presses to prevent rapid repeated actions
    static unsigned long lastButtonPress = 0;
    unsigned long now = platform_millis();
    if (now - lastButtonPress < 300) {  // 300ms debounce
        return;
    }
    lastButtonPress = now;
    
    switch (_currentScreen) {
        case SCREEN_SETUP:
            // Short press during setup - no action (wait for WiFi config)
            break;
            
        case SCREEN_IDLE:
            // Turn on machine with selected strategy
            if (_onTurnOn) {
                _onTurnOn();
            }
            if (_onSetStrategy) {
                _onSetStrategy(screen_idle_get_selected_strategy());
            }
            showScreen(SCREEN_HOME);
            break;
            
        case SCREEN_HOME:
            // Short press on home - go to settings
            showScreen(SCREEN_SETTINGS);
            break;
            
        case SCREEN_BREWING:
            // During brewing, button tares scale
            if (_onTareScale) {
                _onTareScale();
            }
            break;
            
        case SCREEN_COMPLETE:
            // Dismiss complete screen - go to Home (if on) or Idle (if off)
            if (_state.machine_state == UI_STATE_IDLE || _state.machine_state == UI_STATE_INIT) {
                showScreen(SCREEN_IDLE);
            } else {
                showScreen(SCREEN_HOME);
            }
            break;
            
        case SCREEN_SETTINGS:
            // Select current menu item
            screen_settings_select();
            break;
            
        case SCREEN_TEMP_SETTINGS:
#ifndef SIMULATOR
            // Handle temperature edit
            if (screen_temp_select()) {
                LOG_I("Temp screen: button handled");
            } else {
                LOG_W("Temp screen: button press not handled");
            }
#endif
            break;
            
        case SCREEN_SCALE:
#ifndef SIMULATOR
            // Handle scale pairing actions
            screen_scale_select();
#endif
            break;
            
        case SCREEN_CLOUD:
            // Refresh pairing code
            screen_cloud_select();
            break;
            
        case SCREEN_ALARM:
            // Acknowledge alarm
            clearAlarm();
            break;
            
        default:
            break;
    }
}

void UI::handleLongPress() {
    // Rate limit long presses to prevent repeated actions
    static unsigned long lastLongPress = 0;
    unsigned long now = platform_millis();
    if (now - lastLongPress < 1000) {  // 1 second debounce
        return;
    }
    lastLongPress = now;
    
    switch (_currentScreen) {
        case SCREEN_SETTINGS:
            // Long press from menu -> Exit menu (go to Home/Idle)
            // If machine is off, go to Idle, otherwise Home
            if (_state.machine_state == UI_STATE_IDLE || _state.machine_state == UI_STATE_INIT) {
                showScreen(SCREEN_IDLE);
            } else {
                showScreen(SCREEN_HOME);
            }
            break;
            
        default:
            // Long press from ANY other screen -> Go to Settings Menu
            showScreen(SCREEN_SETTINGS);
            break;
    }
}

void UI::handleDoublePress() {
    switch (_currentScreen) {
        case SCREEN_BREWING:
            // Double press - tare scale
            if (_onTareScale) {
                _onTareScale();
                showNotification("Scale tared", 1000);
            }
            break;
            
        case SCREEN_SCALE:
            // Double press on scale screen - tare
            if (_onTareScale) {
                _onTareScale();
                showNotification("Scale tared", 1000);
            }
            break;
            
        case SCREEN_HOME:
            // Double press on home - quick settings access
            showScreen(SCREEN_SETTINGS);
            break;
            
        default:
            break;
    }
}

// =============================================================================
// Screen Creation Methods
// =============================================================================

void UI::createSetupScreen() {
    _screens[SCREEN_SETUP] = screen_setup_create();
}

void UI::createIdleScreen() {
    _screens[SCREEN_IDLE] = screen_idle_create();
}

void UI::createHomeScreen() {
    _screens[SCREEN_HOME] = screen_home_create();
}

void UI::createBrewingScreen() {
    _screens[SCREEN_BREWING] = screen_brewing_create();
}

void UI::createCompleteScreen() {
    _screens[SCREEN_COMPLETE] = screen_complete_create();
}

void UI::createSettingsScreen() {
    _screens[SCREEN_SETTINGS] = screen_settings_create();
    
    // Set callback for menu item selection
    screen_settings_set_select_callback([](settings_item_t item) {
        switch (item) {
            case SETTINGS_TEMP:
                // Temperature settings
                ui.showScreen(SCREEN_TEMP_SETTINGS);
                break;
            case SETTINGS_SCALE:
                // Connect to scale
                ui.showScreen(SCREEN_SCALE);
                break;
            case SETTINGS_CLOUD:
                // Cloud pairing
                ui.showScreen(SCREEN_CLOUD);
                break;
            case SETTINGS_THEME:
                // Toggle theme between dark/light
                if (theme_get_mode() == THEME_MODE_DARK) {
                    theme_set_mode(THEME_MODE_LIGHT);
                    ui.showNotification("Light Theme", 1500);
                } else {
                    theme_set_mode(THEME_MODE_DARK);
                    ui.showNotification("Dark Theme", 1500);
                }
                break;
            case SETTINGS_WIFI:
                // Enter WiFi setup mode - callback will reset to DHCP and start AP
                ui.triggerWifiSetup();
                break;
            case SETTINGS_ABOUT:
                // Show device info notification
                ui.showNotification("BrewOS v1.0", 3000);
                break;
            case SETTINGS_EXIT:
                // Return to appropriate screen based on state
                if (ui.getState().machine_state == UI_STATE_IDLE || 
                    ui.getState().machine_state == UI_STATE_INIT) {
                    ui.showScreen(SCREEN_IDLE);
                } else {
                    ui.showScreen(SCREEN_HOME);
                }
                break;
            default:
                break;
        }
    });
}

void UI::createTempSettingsScreen() {
#ifndef SIMULATOR
    // Create temperature settings screen with full encoder interaction
    _screens[SCREEN_TEMP_SETTINGS] = screen_temp_create();
    
    // Set callback for temperature changes
    screen_temp_set_callback([](bool is_steam, float temp) {
        if (ui._onSetTemp) {
            ui._onSetTemp(is_steam, temp);
        }
    });
#else
    // Simulator placeholder
    _screens[SCREEN_TEMP_SETTINGS] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screens[SCREEN_TEMP_SETTINGS], COLOR_BG_DARK, 0);
    
    lv_obj_t* label = lv_label_create(_screens[SCREEN_TEMP_SETTINGS]);
    lv_label_set_text(label, "Temperature Settings\n(Simulator)");
    lv_obj_set_style_text_color(label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(label);
#endif
}

void UI::createScaleScreen() {
#ifndef SIMULATOR
    // Create scale pairing screen with BLE scanning functionality
    _screens[SCREEN_SCALE] = screen_scale_create();
#else
    // Simulator placeholder
    _screens[SCREEN_SCALE] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screens[SCREEN_SCALE], COLOR_BG_DARK, 0);
    
    lv_obj_t* icon = lv_label_create(_screens[SCREEN_SCALE]);
    lv_label_set_text(icon, LV_SYMBOL_BLUETOOTH);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(icon, COLOR_INFO, 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -30);
    
    lv_obj_t* label = lv_label_create(_screens[SCREEN_SCALE]);
    lv_label_set_text(label, "Scale Pairing\n(Simulator)");
    lv_obj_set_style_text_color(label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 30);
#endif
}

void UI::createCloudScreen() {
    _screens[SCREEN_CLOUD] = screen_cloud_create();
    
    // Set refresh callback to generate new pairing token
    screen_cloud_set_refresh_callback([]() {
        // In real implementation, this would call pairingManager.generateToken()
        // and update the screen with the new token
        // For now, show a placeholder
        screen_cloud_update("BRW-12345678", "ABC123XY", 
                           "brewos://pair?id=BRW-12345678&token=ABC123XY", 600);
    });
    
    // Initialize with placeholder data
    screen_cloud_update("BRW---------", "--------", 
                       "brewos://pair", 0);
}

void UI::createAlarmScreen() {
    _screens[SCREEN_ALARM] = screen_alarm_create();
}

void UI::createOtaScreen() {
    _screens[SCREEN_OTA] = screen_ota_create();
}

void UI::createSplashScreen() {
    _screens[SCREEN_SPLASH] = screen_splash_create();
}

// =============================================================================
// Screen Update Methods
// =============================================================================

void UI::updateSetupScreen() {
    screen_setup_update(&_state);
}

void UI::updateIdleScreen() {
    screen_idle_update(&_state);
}

void UI::updateHomeScreen() {
    screen_home_update(_screens[SCREEN_HOME], &_state);
}

void UI::updateBrewingScreen() {
    screen_brewing_update(&_state);
}

void UI::updateCompleteScreen() {
    // Complete screen is updated when shown (with final values)
}

void UI::updateSettingsScreen() {
    screen_settings_update(&_state);
}

void UI::updateAlarmScreen() {
    // Alarm screen is updated when set
}

void UI::updateOtaScreen() {
    // OTA screen is updated when set
}

// =============================================================================
// Helper Methods
// =============================================================================

void UI::checkAutoScreenSwitch() {
    // OTA takes absolute priority - don't show alarm screen during OTA
    // OTA screen is managed externally (in main.cpp) when webServer->isOtaInProgress() is true
    
    // Handle alarm screen transitions with debouncing
    // Debounce alarm state changes to prevent rapid toggling (min 500ms between changes)
    uint32_t now = platform_millis();
    bool alarm_state_changed = (_state.alarm_active != last_alarm_state);
    
    if (alarm_state_changed && (now - last_alarm_change_time) > 500) {
        last_alarm_state = _state.alarm_active;
        last_alarm_change_time = now;
        
        if (_state.alarm_active) {
            // Alarm just activated - show alarm screen
            // Don't interrupt settings navigation or OTA screen
            if (_currentScreen != SCREEN_ALARM && 
                _currentScreen != SCREEN_SETTINGS && 
                _currentScreen != SCREEN_TEMP_SETTINGS && 
                _currentScreen != SCREEN_SCALE && 
                _currentScreen != SCREEN_CLOUD &&
                _currentScreen != SCREEN_OTA) {
                showAlarm(_state.alarm_code, nullptr);
            }
        } else {
            // Alarm just cleared - switch away from alarm screen
            if (_currentScreen == SCREEN_ALARM) {
                screen_alarm_clear();
                // Return to previous screen or default to home
                if (_previousScreen != SCREEN_ALARM && _previousScreen != SCREEN_SETUP) {
                    showScreen(_previousScreen);
                } else {
                    showScreen(SCREEN_HOME);
                }
            }
        }
        return;
    }
    
    // Don't auto-switch if on setup screen (but allow alarm screen transitions above)
    if (_currentScreen == SCREEN_SETUP) {
        return;
    }
    
    // Show alarm screen if alarm is active (fallback for initial state)
    // Don't interrupt settings navigation or OTA screen
    if (_state.alarm_active && _currentScreen != SCREEN_ALARM && !alarm_state_changed &&
        _currentScreen != SCREEN_SETTINGS && _currentScreen != SCREEN_TEMP_SETTINGS && 
        _currentScreen != SCREEN_SCALE && _currentScreen != SCREEN_CLOUD &&
        _currentScreen != SCREEN_OTA) {
        showAlarm(_state.alarm_code, nullptr);
        last_alarm_state = true;
        return;
    }
    
    // Switch to brewing screen when brewing starts
    if (_state.is_brewing && _currentScreen != SCREEN_BREWING) {
        showScreen(SCREEN_BREWING);
        return;
    }
    
    // Show setup screen if in AP-only mode (not AP+STA)
    // Only auto-switch if we're truly in setup mode (AP only, no WiFi connection)
    // Don't auto-switch if user manually triggered setup (they're already on setup screen)
    // Don't auto-switch if we're in AP+STA mode (WiFi is still connected)
    if (_state.wifi_ap_mode && !_state.wifi_connected && _currentScreen != SCREEN_SETUP) {
        // Only auto-show if we're not already on a settings/setup screen
        // This prevents interrupting user navigation
        if (_currentScreen != SCREEN_SETTINGS && _currentScreen != SCREEN_TEMP_SETTINGS && 
            _currentScreen != SCREEN_SCALE && _currentScreen != SCREEN_CLOUD) {
            showScreen(SCREEN_SETUP);
            return;
        }
    }
    
    // Auto-dismiss splash screen when machine is active
    if (_currentScreen == SCREEN_SPLASH && 
        _state.machine_state >= UI_STATE_HEATING &&
        _state.machine_state <= UI_STATE_ECO) {
        showScreen(SCREEN_HOME);
        return;
    }

    // Show idle screen if machine is off or initializing
    if ((_state.machine_state == UI_STATE_IDLE || _state.machine_state == UI_STATE_INIT) && 
        _currentScreen != SCREEN_IDLE && 
        _currentScreen != SCREEN_SETTINGS) {
        
        // If currently on Splash screen, ensure we show it for at least 2 seconds
        if (_currentScreen == SCREEN_SPLASH) {
            static unsigned long splashStart = 0;
            if (splashStart == 0) splashStart = platform_millis();
            if (platform_millis() - splashStart < 3000) return; // 3 seconds splash
        }
        
        // Only auto-switch to idle if not in settings
        showScreen(SCREEN_IDLE);
        return;
    }
}

const char* UI::getStateText(uint8_t state) {
    switch (state) {
        case UI_STATE_INIT: return "INIT";
        case UI_STATE_IDLE: return "OFF";
        case UI_STATE_HEATING: return "HEATING";
        case UI_STATE_READY: return "READY";
        case UI_STATE_BREWING: return "BREWING";
        case UI_STATE_FAULT: return "FAULT";
        case UI_STATE_SAFE: return "SAFE MODE";
        case UI_STATE_ECO: return "ECO";
        default: return "UNKNOWN";
    }
}

const char* UI::getStrategyText(uint8_t strategy) {
    switch (strategy) {
        case HEAT_BREW_ONLY: return "Brew Only";
        case HEAT_SEQUENTIAL: return "Sequential";
        case HEAT_PARALLEL: return "Parallel";
        case HEAT_SMART_STAGGER: return "Smart Stagger";
        default: return "Unknown";
    }
}

lv_color_t UI::getStateColor(uint8_t state) {
    switch (state) {
        case UI_STATE_INIT: return COLOR_INFO;
        case UI_STATE_IDLE: return COLOR_TEXT_MUTED;
        case UI_STATE_HEATING: return COLOR_WARNING;
        case UI_STATE_READY: return COLOR_SUCCESS;
        case UI_STATE_BREWING: return COLOR_ACCENT_ORANGE;
        case UI_STATE_FAULT: return COLOR_ERROR;
        case UI_STATE_SAFE: return COLOR_ERROR;
        case UI_STATE_ECO: return COLOR_INFO;
        default: return COLOR_TEXT_MUTED;
    }
}

void UI::rebuildScreens() {
    LOG_I("Rebuilding screens for theme change...");
    
    // Remember current screen
    screen_id_t current = _currentScreen;
    
    // Store old screen pointers
    lv_obj_t* old_screens[SCREEN_COUNT];
    for (int i = 0; i < SCREEN_COUNT; i++) {
        old_screens[i] = _screens[i];
        _screens[i] = nullptr;
    }
    
    // Create new screens with new theme colors
    createSetupScreen();
    createIdleScreen();
    createHomeScreen();
    createBrewingScreen();
    createCompleteScreen();
    createSettingsScreen();
    createTempSettingsScreen();
    createScaleScreen();
    createCloudScreen();
    createAlarmScreen();
    
    // Switch to new screen first (before deleting old ones)
    if (_screens[current]) {
        lv_scr_load(_screens[current]);
        _currentScreen = current;
    }
    
    // Now safely delete old screens
    for (int i = 0; i < SCREEN_COUNT; i++) {
        if (old_screens[i]) {
            lv_obj_del(old_screens[i]);
        }
    }
    
    // Update with current state
    update(_state);
    
    LOG_I("Screens rebuilt");
}
