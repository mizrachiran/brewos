/**
 * Cloud Notification Sender
 * Sends notifications from ESP32 to cloud server for push notifications
 */

#ifndef CLOUD_NOTIFIER_H
#define CLOUD_NOTIFIER_H

#include "notification_types.h"

/**
 * Send a notification to the cloud server
 * @param cloudUrl The base URL of the cloud service (e.g., "https://brewos.io")
 * @param deviceId The device ID (e.g., "BRW-12345678")
 * @param deviceKey The device key for authentication
 * @param notif The notification to send
 */
void sendNotificationToCloud(const String& cloudUrl, const String& deviceId, const String& deviceKey, const Notification& notif);

#endif // CLOUD_NOTIFIER_H

