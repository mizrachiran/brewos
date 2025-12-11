/**
 * BrewOS Notification Manager
 * 
 * Simple notification system for reminders and alerts.
 * Sends to WebSocket (UI), MQTT (Home Assistant), and Cloud (push).
 */

#ifndef NOTIFICATION_MANAGER_H
#define NOTIFICATION_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <Preferences.h>
#include "notification_types.h"

// =============================================================================
// Configuration Defaults
// =============================================================================

#define NOTIF_MAX_ACTIVE            5
#define NOTIF_DEFAULT_DESCALE_DAYS  30
#define NOTIF_DEFAULT_SERVICE_SHOTS 500
#define NOTIF_BACKFLUSH_WEEKLY      7
#define NOTIF_BACKFLUSH_DAILY       1

// NVS namespace
#define NVS_NOTIF_NAMESPACE         "notif"

// =============================================================================
// Preferences Structure
// =============================================================================

struct NotificationPreferences {
    bool push_enabled;           // Global push enable
    bool machine_ready_push;     // Push when ready
    bool water_empty_push;       // Push when water empty
    bool maintenance_push;       // Push for maintenance reminders
    uint16_t descale_days;       // Days between descale reminders
    uint32_t service_shots;      // Shots before service reminder
    uint8_t backflush_days;      // Days between backflush (0 = off)
};

// =============================================================================
// Callbacks
// =============================================================================

// Called when notification should be sent to a channel
// Simple function pointer to avoid std::function PSRAM allocation issues
typedef void (*NotificationSendCallback)(const Notification&);

// =============================================================================
// Notification Manager
// =============================================================================

class NotificationManager {
public:
    NotificationManager();
    
    /**
     * Initialize - load preferences from NVS
     */
    bool begin();
    
    /**
     * Enable/disable notifications (for OTA updates)
     * When disabled, notifications are queued but not sent
     */
    void setEnabled(bool enabled);
    bool isEnabled() const { return _enabled; }
    
    // =========================================================================
    // Create Notifications (call these when conditions are met)
    // =========================================================================
    
    void machineReady(float temp);
    void waterEmpty();
    void descaleDue(uint32_t days_overdue = 0);
    void serviceDue(uint32_t total_shots);
    void backflushDue();
    void machineError(uint8_t error_code, const char* details = nullptr);
    void picoOffline();
    
    // =========================================================================
    // Management
    // =========================================================================
    
    /**
     * Dismiss a reminder (clears it)
     */
    void dismiss(NotificationType type);
    
    /**
     * Acknowledge an alert (marks as seen but keeps in history)
     */
    void acknowledge(NotificationType type);
    
    /**
     * Clear all reminders (not alerts)
     */
    void clearReminders();
    
    /**
     * Clear a specific notification when condition resolves
     * e.g., clearCondition(WATER_EMPTY) when tank is refilled
     */
    void clearCondition(NotificationType type);
    
    // =========================================================================
    // Query
    // =========================================================================
    
    /**
     * Check if there are unacknowledged alerts
     */
    bool hasActiveAlerts() const;
    
    /**
     * Get all active notifications
     */
    std::vector<Notification> getActive() const;
    
    /**
     * Get notification by type (nullptr if not active)
     */
    const Notification* get(NotificationType type) const;
    
    // =========================================================================
    // Preferences
    // =========================================================================
    
    NotificationPreferences getPreferences() const { return _prefs; }
    void setPreferences(const NotificationPreferences& prefs);
    void savePreferences();
    
    // =========================================================================
    // Channel Callbacks
    // =========================================================================
    
    void onWebSocket(NotificationSendCallback cb) { _onWebSocket = cb; }
    void onMQTT(NotificationSendCallback cb) { _onMQTT = cb; }
    void onCloud(NotificationSendCallback cb) { _onCloud = cb; }
    
private:
    Notification _active[NOTIF_MAX_ACTIVE];
    size_t _activeCount;
    NotificationPreferences _prefs;
    bool _enabled = true;  // Pause during OTA
    
    // Deduplication - track last notification time per type
    uint32_t _lastNotified[12];  // Indexed by type
    
    // Callbacks
    NotificationSendCallback _onWebSocket;
    NotificationSendCallback _onMQTT;
    NotificationSendCallback _onCloud;
    
    // Internal
    void send(const Notification& notif);
    void addActive(const Notification& notif);
    void removeActive(NotificationType type);
    bool isDuplicate(NotificationType type, uint32_t cooldown_ms);
    void loadPreferences();
    
    // Default cooldowns (milliseconds)
    uint32_t getCooldown(NotificationType type) const;
};

// Global instance
extern NotificationManager* notificationManager;

#endif // NOTIFICATION_MANAGER_H
