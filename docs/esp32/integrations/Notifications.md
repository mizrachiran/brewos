# Notifications Framework

Push reminders and alerts to keep users informed when **away from the machine**.

## Design Philosophy

- **Reminders, not events** - Only notify about things that require user action
- **Away from machine** - If you're standing at the machine, you don't need a notification
- **Actionable** - Each notification should have a clear "what to do next"
- **Minimal noise** - Less is more; every notification should be valuable

## Notification Types

### Reminders

| Code | Message | Trigger | Frequency |
|------|---------|---------|-----------|
| `MACHINE_READY` | "Machine is ready" | Brew temp stable | Once per heat-up |
| `WATER_EMPTY` | "Refill water tank" | Tank sensor triggered | Once until refilled |
| `DESCALE_DUE` | "Time to descale" | X days since last | Weekly reminder |
| `SERVICE_DUE` | "Maintenance recommended" | Shot counter threshold | Monthly |
| `BACKFLUSH_DUE` | "Backflush reminder" | Daily/weekly schedule | Configurable |

### Alerts (Critical)

| Code | Message | Trigger |
|------|---------|---------|
| `MACHINE_ERROR` | "Machine needs attention" | Any Pico alarm |
| `PICO_OFFLINE` | "Control board offline" | UART timeout > 30s |

That's it. **7 notification types total.**

## Priority Levels

| Priority | Behavior |
|----------|----------|
| **Reminder** | Sent once, auto-dismissed when condition clears |
| **Alert** | Persists until acknowledged, shown prominently in UI |

## Delivery Channels

| Channel | Use Case |
|---------|----------|
| **WebSocket** | Real-time UI updates |
| **MQTT** | Home Assistant automations |
| **Cloud** | Mobile push notifications (future) |

Not all notifications go to all channels:

| Notification | WebSocket | MQTT | Push |
|--------------|-----------|------|------|
| Machine Ready | ✓ | ✓ | ✓ |
| Water Empty | ✓ | ✓ | ✓ |
| Descale Due | ✓ | ✓ | ✓ |
| Service Due | ✓ | ✓ | ✓ |
| Backflush Due | ✓ | ✓ | Optional |
| Machine Error | ✓ | ✓ | ✓ |
| Pico Offline | ✓ | ✓ | ✓ |

## User Preferences

Users can configure:
- Enable/disable push notifications per type
- Descale reminder interval (default: 30 days)
- Service reminder threshold (default: 500 shots)
- Backflush reminder schedule (daily/weekly/off)

```json
{
  "preferences": {
    "push_enabled": true,
    "machine_ready_push": true,
    "water_empty_push": true,
    "descale_interval_days": 30,
    "service_threshold_shots": 500,
    "backflush_schedule": "weekly"
  }
}
```

## MQTT Integration

Published to `brewos/notification`:

```json
{
  "code": "MACHINE_READY",
  "message": "Machine is ready - 93.5°C",
  "timestamp": "2024-11-29T08:30:00Z"
}
```

Home Assistant automation example:

```yaml
automation:
  - alias: "Notify when coffee machine ready"
    trigger:
      platform: mqtt
      topic: "brewos/notification"
    condition:
      condition: template
      value_template: "{{ trigger.payload_json.code == 'MACHINE_READY' }}"
    action:
      service: notify.mobile_app_phone
      data:
        title: "Coffee Time ☕"
        message: "{{ trigger.payload_json.message }}"
```

## API

### Get Active Notifications
```http
GET /api/notifications
```

### Dismiss
```http
POST /api/notifications/{code}/dismiss
```

### Get/Set Preferences
```http
GET /api/notifications/preferences
POST /api/notifications/preferences
```

## Implementation

### Simple Structure

```cpp
enum class NotificationType : uint8_t {
    // Reminders
    MACHINE_READY = 1,
    WATER_EMPTY   = 2,
    DESCALE_DUE   = 3,
    SERVICE_DUE   = 4,
    BACKFLUSH_DUE = 5,
    
    // Alerts
    MACHINE_ERROR = 10,
    PICO_OFFLINE  = 11
};

struct Notification {
    NotificationType type;
    char message[64];
    uint32_t timestamp;
    bool is_alert;        // true = critical, requires ack
    bool acknowledged;
};
```

### NotificationManager

```cpp
class NotificationManager {
public:
    void begin();
    
    // Create notifications
    void machineReady(float temp);
    void waterEmpty();
    void descaleDue();
    void serviceDue(uint32_t shots);
    void backflushDue();
    void machineError(const char* details);
    void picoOffline();
    
    // Management
    void dismiss(NotificationType type);
    void acknowledge(NotificationType type);
    void clearReminders();
    
    // Query
    bool hasActiveAlerts();
    std::vector<Notification> getActive();
};
```

### Trigger Logic

```cpp
// In main loop
void checkNotifications() {
    static bool wasReady = false;
    static bool wasWaterOk = true;
    
    // Machine ready - notify once when temp stabilizes
    bool isReady = machineState.brew_temp_stable && machineState.is_on;
    if (isReady && !wasReady) {
        notificationManager.machineReady(machineState.brew_temp);
    }
    wasReady = isReady;
    
    // Water empty - notify when tank sensor triggers
    bool waterOk = !machineState.water_empty;
    if (!waterOk && wasWaterOk) {
        notificationManager.waterEmpty();
    }
    wasWaterOk = waterOk;
    
    // Maintenance reminders - check once per hour
    static uint32_t lastMaintenanceCheck = 0;
    if (millis() - lastMaintenanceCheck > 3600000) {
        lastMaintenanceCheck = millis();
        checkMaintenanceReminders();
    }
}

void checkMaintenanceReminders() {
    // Descale
    uint32_t daysSinceDescale = getDaysSinceDescale();
    if (daysSinceDescale >= preferences.descale_interval_days) {
        notificationManager.descaleDue();
    }
    
    // Service
    if (stats.total_shots >= preferences.service_threshold_shots) {
        notificationManager.serviceDue(stats.total_shots);
    }
    
    // Backflush (based on schedule)
    if (isBackflushDue()) {
        notificationManager.backflushDue();
    }
}
```

## Cloud Push (Future)

## Cloud Integration (Implemented)

The ESP32 sends notifications to the cloud server for push notification delivery:

1. ESP32 sends notification to cloud server via HTTP POST
2. Cloud looks up user's push subscriptions
3. Cloud sends push notification to all subscribed browsers

```
ESP32 --> Cloud Server --> Web Push API --> Browser
```

### Implementation

The cloud notification callback is set up in `main.cpp`:

```cpp
notificationManager.onCloud([&State](const Notification& notif) {
    String cloudUrl = String(State.settings().cloud.serverUrl);
    String deviceId = String(State.settings().cloud.deviceId);
    
    if (!cloudUrl.isEmpty() && !deviceId.isEmpty()) {
        sendNotificationToCloud(cloudUrl, deviceId, notif);
    }
});
```

### Cloud Notifier

The `cloud_notifier.cpp` module handles sending notifications to the cloud:

- **Endpoint:** `POST /api/push/notify`
- **Authentication:** Device ID (no user auth required)
- **Payload:** Notification type, message, timestamp, alert flag

See [Cloud Push Notifications Documentation](../../cloud/Push_Notifications.md) for API details.

## Summary

| What | Count |
|------|-------|
| Notification types | 7 |
| User-configurable | 5 preferences |
| Delivery channels | 3 (WS, MQTT, Cloud) |

Simple, focused, actionable.
