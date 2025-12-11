/**
 * BrewOS Notification Manager Implementation
 */

#include "notifications/notification_manager.h"
#include "config.h"
#include <cstring>
#include <cstdio>

// Global instance - now a pointer, constructed in main.cpp setup()
NotificationManager* notificationManager = nullptr;

// =============================================================================
// Constructor
// =============================================================================

NotificationManager::NotificationManager()
    : _activeCount(0)
    , _onWebSocket(nullptr)
    , _onMQTT(nullptr)
    , _onCloud(nullptr) {
    
    memset(_active, 0, sizeof(_active));
    memset(_lastNotified, 0, sizeof(_lastNotified));
    
    // Default preferences
    _prefs.push_enabled = true;
    _prefs.machine_ready_push = true;
    _prefs.water_empty_push = true;
    _prefs.maintenance_push = true;
    _prefs.descale_days = NOTIF_DEFAULT_DESCALE_DAYS;
    _prefs.service_shots = NOTIF_DEFAULT_SERVICE_SHOTS;
    _prefs.backflush_days = NOTIF_BACKFLUSH_WEEKLY;
}

// =============================================================================
// Initialization
// =============================================================================

bool NotificationManager::begin() {
    LOG_I("Initializing Notification Manager...");
    loadPreferences();
    LOG_I("Notifications ready (push=%s, descale=%d days, service=%lu shots)",
          _prefs.push_enabled ? "on" : "off",
          _prefs.descale_days,
          _prefs.service_shots);
    return true;
}

void NotificationManager::setEnabled(bool enabled) {
    if (enabled != _enabled) {
        _enabled = enabled;
        LOG_I("Notifications %s", enabled ? "enabled" : "disabled");
    }
}

// =============================================================================
// Create Notifications
// =============================================================================

void NotificationManager::machineReady(float temp) {
    if (isDuplicate(NotificationType::MACHINE_READY, getCooldown(NotificationType::MACHINE_READY))) {
        return;
    }
    
    Notification notif;
    notif.type = NotificationType::MACHINE_READY;
    snprintf(notif.message, NOTIF_MESSAGE_LEN, "Machine ready - %.1f°C", temp);
    notif.timestamp = time(nullptr);
    notif.is_alert = false;
    notif.acknowledged = false;
    notif.sent_push = false;
    
    LOG_I("Notification: %s", notif.message);
    send(notif);
    addActive(notif);
}

void NotificationManager::waterEmpty() {
    if (isDuplicate(NotificationType::WATER_EMPTY, getCooldown(NotificationType::WATER_EMPTY))) {
        return;
    }
    
    Notification notif;
    notif.type = NotificationType::WATER_EMPTY;
    snprintf(notif.message, NOTIF_MESSAGE_LEN, "Refill water tank");
    notif.timestamp = time(nullptr);
    notif.is_alert = false;
    notif.acknowledged = false;
    notif.sent_push = false;
    
    LOG_I("Notification: %s", notif.message);
    send(notif);
    addActive(notif);
}

void NotificationManager::descaleDue(uint32_t days_overdue) {
    if (isDuplicate(NotificationType::DESCALE_DUE, getCooldown(NotificationType::DESCALE_DUE))) {
        return;
    }
    
    Notification notif;
    notif.type = NotificationType::DESCALE_DUE;
    if (days_overdue > 0) {
        snprintf(notif.message, NOTIF_MESSAGE_LEN, "Time to descale (%lu days overdue)", days_overdue);
    } else {
        snprintf(notif.message, NOTIF_MESSAGE_LEN, "Time to descale");
    }
    notif.timestamp = time(nullptr);
    notif.is_alert = false;
    notif.acknowledged = false;
    notif.sent_push = false;
    
    LOG_I("Notification: %s", notif.message);
    send(notif);
    addActive(notif);
}

void NotificationManager::serviceDue(uint32_t total_shots) {
    if (isDuplicate(NotificationType::SERVICE_DUE, getCooldown(NotificationType::SERVICE_DUE))) {
        return;
    }
    
    Notification notif;
    notif.type = NotificationType::SERVICE_DUE;
    snprintf(notif.message, NOTIF_MESSAGE_LEN, "Maintenance recommended (%lu shots)", total_shots);
    notif.timestamp = time(nullptr);
    notif.is_alert = false;
    notif.acknowledged = false;
    notif.sent_push = false;
    
    LOG_I("Notification: %s", notif.message);
    send(notif);
    addActive(notif);
}

void NotificationManager::backflushDue() {
    if (isDuplicate(NotificationType::BACKFLUSH_DUE, getCooldown(NotificationType::BACKFLUSH_DUE))) {
        return;
    }
    
    Notification notif;
    notif.type = NotificationType::BACKFLUSH_DUE;
    snprintf(notif.message, NOTIF_MESSAGE_LEN, "Backflush reminder");
    notif.timestamp = time(nullptr);
    notif.is_alert = false;
    notif.acknowledged = false;
    notif.sent_push = false;
    
    LOG_I("Notification: %s", notif.message);
    send(notif);
    addActive(notif);
}

void NotificationManager::machineError(uint8_t error_code, const char* details) {
    // Alerts have shorter cooldown
    if (isDuplicate(NotificationType::MACHINE_ERROR, getCooldown(NotificationType::MACHINE_ERROR))) {
        return;
    }
    
    Notification notif;
    notif.type = NotificationType::MACHINE_ERROR;
    if (details) {
        snprintf(notif.message, NOTIF_MESSAGE_LEN, "Machine error: %s (0x%02X)", details, error_code);
    } else {
        snprintf(notif.message, NOTIF_MESSAGE_LEN, "Machine error (code 0x%02X)", error_code);
    }
    notif.timestamp = time(nullptr);
    notif.is_alert = true;
    notif.acknowledged = false;
    notif.sent_push = false;
    
    LOG_W("ALERT: %s", notif.message);
    send(notif);
    addActive(notif);
}

void NotificationManager::picoOffline() {
    if (isDuplicate(NotificationType::PICO_OFFLINE, getCooldown(NotificationType::PICO_OFFLINE))) {
        return;
    }
    
    Notification notif;
    notif.type = NotificationType::PICO_OFFLINE;
    snprintf(notif.message, NOTIF_MESSAGE_LEN, "Control board offline");
    notif.timestamp = time(nullptr);
    notif.is_alert = true;
    notif.acknowledged = false;
    notif.sent_push = false;
    
    LOG_W("ALERT: %s", notif.message);
    send(notif);
    addActive(notif);
}

// =============================================================================
// Management
// =============================================================================

void NotificationManager::dismiss(NotificationType type) {
    removeActive(type);
    LOG_D("Dismissed: %s", getNotificationCode(type));
}

void NotificationManager::acknowledge(NotificationType type) {
    for (size_t i = 0; i < _activeCount; i++) {
        if (_active[i].type == type) {
            _active[i].acknowledged = true;
            LOG_I("Acknowledged: %s", getNotificationCode(type));
            return;
        }
    }
}

void NotificationManager::clearReminders() {
    // Remove all non-alerts
    size_t writeIdx = 0;
    for (size_t i = 0; i < _activeCount; i++) {
        if (_active[i].is_alert) {
            if (writeIdx != i) {
                _active[writeIdx] = _active[i];
            }
            writeIdx++;
        }
    }
    _activeCount = writeIdx;
    LOG_D("Cleared reminders, %d alerts remain", _activeCount);
}

void NotificationManager::clearCondition(NotificationType type) {
    removeActive(type);
    // Also reset the last notified time so it can trigger again
    uint8_t idx = static_cast<uint8_t>(type);
    if (idx < sizeof(_lastNotified)/sizeof(_lastNotified[0])) {
        _lastNotified[idx] = 0;
    }
}

// =============================================================================
// Query
// =============================================================================

bool NotificationManager::hasActiveAlerts() const {
    for (size_t i = 0; i < _activeCount; i++) {
        if (_active[i].is_alert && !_active[i].acknowledged) {
            return true;
        }
    }
    return false;
}

std::vector<Notification> NotificationManager::getActive() const {
    std::vector<Notification> result;
    for (size_t i = 0; i < _activeCount; i++) {
        result.push_back(_active[i]);
    }
    return result;
}

const Notification* NotificationManager::get(NotificationType type) const {
    for (size_t i = 0; i < _activeCount; i++) {
        if (_active[i].type == type) {
            return &_active[i];
        }
    }
    return nullptr;
}

// =============================================================================
// Preferences
// =============================================================================

void NotificationManager::setPreferences(const NotificationPreferences& prefs) {
    _prefs = prefs;
    savePreferences();
}

void NotificationManager::savePreferences() {
    Preferences nvs;
    nvs.begin(NVS_NOTIF_NAMESPACE, false);
    
    nvs.putBool("push_enabled", _prefs.push_enabled);
    nvs.putBool("ready_push", _prefs.machine_ready_push);
    nvs.putBool("water_push", _prefs.water_empty_push);
    nvs.putBool("maint_push", _prefs.maintenance_push);
    nvs.putUShort("descale_days", _prefs.descale_days);
    nvs.putULong("service_shots", _prefs.service_shots);
    nvs.putUChar("backflush", _prefs.backflush_days);
    
    nvs.end();
    LOG_I("Notification preferences saved");
}

void NotificationManager::loadPreferences() {
    Serial.println("[Notif] loadPreferences() starting...");
    Serial.flush();
    
    Preferences nvs;
    Serial.println("[Notif] Calling nvs.begin()...");
    Serial.flush();
    
    // Try read-write first to create namespace if it doesn't exist
    // This is normal after a fresh flash - will use defaults
    bool beginOk = nvs.begin(NVS_NOTIF_NAMESPACE, false);
    Serial.print("[Notif] nvs.begin() returned: ");
    Serial.println(beginOk ? "true" : "false");
    Serial.flush();
    
    if (!beginOk) {
        Serial.println("[Notif] No saved preferences (fresh flash) - using defaults");
        Serial.flush();
        return;  // Use default values already set in constructor
    }
    
    Serial.println("[Notif] Reading preferences from NVS...");
    Serial.flush();
    _prefs.push_enabled = nvs.getBool("push_enabled", true);
    _prefs.machine_ready_push = nvs.getBool("ready_push", true);
    _prefs.water_empty_push = nvs.getBool("water_push", true);
    _prefs.maintenance_push = nvs.getBool("maint_push", true);
    _prefs.descale_days = nvs.getUShort("descale_days", NOTIF_DEFAULT_DESCALE_DAYS);
    _prefs.service_shots = nvs.getULong("service_shots", NOTIF_DEFAULT_SERVICE_SHOTS);
    _prefs.backflush_days = nvs.getUChar("backflush", NOTIF_BACKFLUSH_WEEKLY);
    
    Serial.println("[Notif] Closing NVS...");
    Serial.flush();
    nvs.end();
    Serial.println("[Notif] loadPreferences() complete");
    Serial.flush();
}

// =============================================================================
// Internal Methods
// =============================================================================

void NotificationManager::send(const Notification& notif) {
    // Skip sending if disabled (during OTA)
    if (!_enabled) {
        return;
    }
    
    // Always send to WebSocket (UI)
    if (_onWebSocket) {
        _onWebSocket(notif);
    }
    
    // Send to MQTT
    if (_onMQTT) {
        _onMQTT(notif);
    }
    
    // Send to Cloud (respects push preferences)
    if (_onCloud && _prefs.push_enabled) {
        bool shouldPush = false;
        
        switch (notif.type) {
            case NotificationType::MACHINE_READY:
                shouldPush = _prefs.machine_ready_push;
                break;
            case NotificationType::WATER_EMPTY:
                shouldPush = _prefs.water_empty_push;
                break;
            case NotificationType::DESCALE_DUE:
            case NotificationType::SERVICE_DUE:
            case NotificationType::BACKFLUSH_DUE:
                shouldPush = _prefs.maintenance_push;
                break;
            case NotificationType::MACHINE_ERROR:
            case NotificationType::PICO_OFFLINE:
                shouldPush = true;  // Always push alerts
                break;
        }
        
        if (shouldPush) {
            _onCloud(notif);
        }
    }
}

void NotificationManager::addActive(const Notification& notif) {
    // Check if already exists
    for (size_t i = 0; i < _activeCount; i++) {
        if (_active[i].type == notif.type) {
            // Update existing
            _active[i] = notif;
            return;
        }
    }
    
    // Add new if space available
    if (_activeCount < NOTIF_MAX_ACTIVE) {
        _active[_activeCount++] = notif;
    } else {
        // Replace oldest non-alert
        for (size_t i = 0; i < _activeCount; i++) {
            if (!_active[i].is_alert) {
                _active[i] = notif;
                return;
            }
        }
    }
}

void NotificationManager::removeActive(NotificationType type) {
    for (size_t i = 0; i < _activeCount; i++) {
        if (_active[i].type == type) {
            // Shift remaining
            for (size_t j = i; j < _activeCount - 1; j++) {
                _active[j] = _active[j + 1];
            }
            _activeCount--;
            return;
        }
    }
}

bool NotificationManager::isDuplicate(NotificationType type, uint32_t cooldown_ms) {
    uint8_t idx = static_cast<uint8_t>(type);
    if (idx >= sizeof(_lastNotified)/sizeof(_lastNotified[0])) {
        return false;
    }
    
    uint32_t now = millis();
    if (now - _lastNotified[idx] < cooldown_ms) {
        return true;
    }
    
    _lastNotified[idx] = now;
    return false;
}

uint32_t NotificationManager::getCooldown(NotificationType type) const {
    switch (type) {
        // Reminders - long cooldown (don't spam)
        case NotificationType::MACHINE_READY:
            return 60000;       // 1 minute
        case NotificationType::WATER_EMPTY:
            return 300000;      // 5 minutes
        case NotificationType::DESCALE_DUE:
        case NotificationType::SERVICE_DUE:
        case NotificationType::BACKFLUSH_DUE:
            return 86400000;    // 24 hours
            
        // Alerts - shorter cooldown (important)
        case NotificationType::MACHINE_ERROR:
            return 60000;       // 1 minute
        case NotificationType::PICO_OFFLINE:
            return 30000;       // 30 seconds
            
        default:
            return 60000;
    }
}

