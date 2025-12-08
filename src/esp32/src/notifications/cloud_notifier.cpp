/**
 * Cloud Notification Sender
 * Sends notifications from ESP32 to cloud server for push notifications
 */

#include "notifications/cloud_notifier.h"
#include "notifications/notification_types.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

void sendNotificationToCloud(const String& cloudUrl, const String& deviceId, const String& deviceKey, const Notification& notif) {
    if (cloudUrl.isEmpty() || deviceId.isEmpty() || !WiFi.isConnected()) {
        return;
    }

    HTTPClient http;
    String url = cloudUrl + "/api/push/notify";
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    // Add device key for authentication (required for secure notifications)
    if (!deviceKey.isEmpty()) {
        http.addHeader("X-Device-Key", deviceKey);
    }
    
    // Create request body
    JsonDocument doc;
    doc["deviceId"] = deviceId;
    
    // Map notification type to string
    const char* typeStr = "INFO";
    switch (notif.type) {
        case NotificationType::MACHINE_READY:
            typeStr = "MACHINE_READY";
            break;
        case NotificationType::WATER_EMPTY:
            typeStr = "WATER_EMPTY";
            break;
        case NotificationType::DESCALE_DUE:
            typeStr = "DESCALE_DUE";
            break;
        case NotificationType::SERVICE_DUE:
            typeStr = "SERVICE_DUE";
            break;
        case NotificationType::BACKFLUSH_DUE:
            typeStr = "BACKFLUSH_DUE";
            break;
        case NotificationType::MACHINE_ERROR:
            typeStr = "MACHINE_ERROR";
            break;
        case NotificationType::PICO_OFFLINE:
            typeStr = "PICO_OFFLINE";
            break;
    }
    
    JsonObject notification = doc["notification"].to<JsonObject>();
    notification["type"] = typeStr;
    notification["message"] = notif.message;
    notification["timestamp"] = notif.timestamp;
    notification["is_alert"] = notif.is_alert;
    
    String body;
    serializeJson(doc, body);
    
    int httpCode = http.POST(body);
    
    if (httpCode == 200) {
        Serial.printf("[Cloud] Notification sent: %s\n", typeStr);
    } else {
        Serial.printf("[Cloud] Failed to send notification: %d\n", httpCode);
    }
    
    http.end();
}

