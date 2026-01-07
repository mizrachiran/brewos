/**
 * BrewOS UI Manager
 * 
 * Manages all UI screens and navigation for the round display
 * See docs/esp32/UI_Design.md for screen specifications
 */

#ifndef UI_H
#define UI_H

#include <lvgl.h>

// =============================================================================
// Screen IDs
// =============================================================================

typedef enum {
    SCREEN_SETUP,       // WiFi setup (first boot / no WiFi)
    SCREEN_IDLE,        // Machine off, can turn on
    SCREEN_HOME,        // Main dashboard (temps, pressure)
    SCREEN_BREWING,     // Active brewing
    SCREEN_COMPLETE,    // Shot complete summary
    SCREEN_SETTINGS,    // Settings menu
    SCREEN_TEMP_SETTINGS, // Temperature adjustment
    SCREEN_SCALE,       // Scale pairing
    SCREEN_CLOUD,       // Cloud pairing QR code
    SCREEN_ALARM,       // Alarm display
    SCREEN_OTA,         // OTA update in progress
    SCREEN_SPLASH,      // Boot splash screen
    SCREEN_COUNT
} screen_id_t;

// =============================================================================
// Machine States (from Pico)
// Note: Using UI_ prefix to avoid conflict with protocol_defs.h macros
// IMPORTANT: These values MUST match Pico's state.h exactly!
// =============================================================================

typedef enum {
    UI_STATE_INIT = 0,      // Initializing
    UI_STATE_IDLE = 1,      // Machine on but not heating (MODE_IDLE)
    UI_STATE_HEATING = 2,   // Actively heating to setpoint
    UI_STATE_READY = 3,     // At temperature, ready to brew
    UI_STATE_BREWING = 4,   // Brewing in progress
    UI_STATE_FAULT = 5,     // Fault condition
    UI_STATE_SAFE = 6,      // Safe state (all outputs off)
    UI_STATE_ECO = 7        // Eco mode (reduced temperature)
} machine_state_t;

// =============================================================================
// Heating Strategies (internal implementation - not shown to users)
// =============================================================================

typedef enum {
    HEAT_BREW_ONLY = 0,
    HEAT_SEQUENTIAL = 1,
    HEAT_PARALLEL = 2,
    HEAT_SMART_STAGGER = 3
} heating_strategy_t;

// =============================================================================
// Power Modes (user-facing selection)
// =============================================================================

typedef enum {
    POWER_MODE_BREW_ONLY = 0,   // Heat only brew boiler
    POWER_MODE_BREW_STEAM = 1   // Heat both boilers (strategy auto-selected)
} power_mode_t;

// =============================================================================
// Machine State for UI Display
// =============================================================================

typedef struct {
    // Temperatures
    float brew_temp;
    float brew_setpoint;
    float steam_temp;
    float steam_setpoint;
    float group_temp;       // Group head temp (for HX machines)
    
    // Limits
    float brew_max_temp;
    float steam_max_temp;
    
    // Pressure
    float pressure;
    
    // State
    uint8_t machine_state;      // machine_state_t
    uint8_t heating_strategy;   // heating_strategy_t
    uint8_t machine_type;       // 0=unknown, 1=dual_boiler, 2=single_boiler, 3=heat_exchanger
    bool is_brewing;
    bool is_heating;
    bool water_low;
    bool alarm_active;
    uint8_t alarm_code;
    
    // Brewing info
    uint32_t brew_time_ms;      // Current brew time
    float brew_weight;          // Current weight (from scale)
    float target_weight;        // Target weight
    float dose_weight;          // Dose weight (for ratio)
    float flow_rate;            // ml/s
    
    // Power
    uint16_t power_watts;       // Current power consumption
    
    // Cleaning
    uint16_t brew_count;        // Brews since last cleaning
    bool cleaning_reminder;     // true if cleaning reminder is due
    
    // Connection status
    bool pico_connected;
    bool wifi_connected;
    bool mqtt_connected;
    bool scale_connected;
    bool cloud_connected;
    
    // WiFi info
    char wifi_ssid[32];
    char wifi_password[32];
    char wifi_ip[16];
    int wifi_rssi;
    bool wifi_ap_mode;
} ui_state_t;

// =============================================================================
// Callback Types
// =============================================================================

typedef void (*ui_turn_on_callback_t)(void);
typedef void (*ui_turn_off_callback_t)(void);
typedef void (*ui_set_temp_callback_t)(bool is_steam, float temp);
typedef void (*ui_set_strategy_callback_t)(heating_strategy_t strategy);
typedef void (*ui_tare_scale_callback_t)(void);
typedef void (*ui_set_target_weight_callback_t)(float weight);
typedef void (*ui_wifi_setup_callback_t)(void);

// =============================================================================
// UI Manager Class
// =============================================================================

class UI {
public:
    UI();
    
    /**
     * Initialize all UI screens
     * Call after display.begin()
     */
    bool begin();
    
    /**
     * Update UI with new state data
     * Call periodically from main loop
     */
    void update(const ui_state_t& state);
    
    /**
     * Get current state
     */
    const ui_state_t& getState() const { return _state; }
    
    /**
     * Switch to a specific screen
     */
    void showScreen(screen_id_t screen);
    
    /**
     * Get current screen
     */
    screen_id_t getCurrentScreen() const { return _currentScreen; }
    
    /**
     * Show a notification/toast message
     */
    void showNotification(const char* message, uint16_t duration_ms = 3000);
    
    /**
     * Show an alarm
     */
    void showAlarm(uint8_t code, const char* message);
    
    /**
     * Clear alarm
     */
    void clearAlarm();
    
    /**
     * Trigger WiFi setup mode (resets to DHCP and starts AP)
     */
    void triggerWifiSetup();
    
    /**
     * Handle encoder rotation
     * @param direction: positive = CW, negative = CCW
     */
    void handleEncoder(int direction);
    
    /**
     * Handle button press
     */
    void handleButtonPress();
    
    /**
     * Handle long press (2 seconds)
     */
    void handleLongPress();
    
    /**
     * Handle double press
     */
    void handleDoublePress();
    
    // Callbacks
    void onTurnOn(ui_turn_on_callback_t cb) { _onTurnOn = cb; }
    void onTurnOff(ui_turn_off_callback_t cb) { _onTurnOff = cb; }
    void onSetTemp(ui_set_temp_callback_t cb) { _onSetTemp = cb; }
    void onSetStrategy(ui_set_strategy_callback_t cb) { _onSetStrategy = cb; }
    void onTareScale(ui_tare_scale_callback_t cb) { _onTareScale = cb; }
    void onSetTargetWeight(ui_set_target_weight_callback_t cb) { _onSetTargetWeight = cb; }
    void onWifiSetup(ui_wifi_setup_callback_t cb) { _onWifiSetup = cb; }

private:
    screen_id_t _currentScreen;
    screen_id_t _previousScreen;
    ui_state_t _state;
    
    // Callbacks
    ui_turn_on_callback_t _onTurnOn;
    ui_turn_off_callback_t _onTurnOff;
    ui_set_temp_callback_t _onSetTemp;
    ui_set_strategy_callback_t _onSetStrategy;
    ui_tare_scale_callback_t _onTareScale;
    ui_set_target_weight_callback_t _onSetTargetWeight;
    ui_wifi_setup_callback_t _onWifiSetup;
    
    // Screen objects
    lv_obj_t* _screens[SCREEN_COUNT];
    
    // Screen creation methods
    void createSetupScreen();
    void createIdleScreen();
    void createHomeScreen();
    void createBrewingScreen();
    void createCompleteScreen();
    void createSettingsScreen();
    void createTempSettingsScreen();
    void createScaleScreen();
    void createCloudScreen();
    void createAlarmScreen();
    void createOtaScreen();
    void createSplashScreen();
    
    // Update methods
    void updateSetupScreen();
    void updateIdleScreen();
    void updateHomeScreen();
    void updateBrewingScreen();
    void updateCompleteScreen();
    void updateSettingsScreen();
    void updateAlarmScreen();
    void updateOtaScreen();
    
    // Screen memory management
    bool ensureScreenExists(screen_id_t screen);
    void destroyScreen(screen_id_t screen);
    void switchToScreen(screen_id_t screen);
    
    // Helper methods
    const char* getStateText(uint8_t state);
    const char* getStrategyText(uint8_t strategy);
    lv_color_t getStateColor(uint8_t state);
    
    // Auto screen switching based on machine state
    void checkAutoScreenSwitch();
    
    // Rebuild all screens (for theme changes)
    void rebuildScreens();
};

// Global UI instance
extern UI ui;

#endif // UI_H
