import { Router } from "express";
import rateLimit from "express-rate-limit";
import { sessionAuthMiddleware } from "../middleware/auth.js";
import {
  subscribeToPush,
  unsubscribeFromPush,
  getVAPIDPublicKey,
  getUserPushSubscriptions,
  sendPushNotificationToDevice,
  getOrCreateNotificationPreferences,
  updateNotificationPreferences,
} from "../services/push.js";
import { userOwnsDevice, getDevice } from "../services/device.js";
import { getDb } from "../lib/database.js";
import type { NotificationType } from "../lib/database.js";

const router = Router();

// ============================================================================
// Rate Limiters
// ============================================================================

// General rate limiter applied to ALL routes in this router
// 100 requests per 15 minutes per IP
const generalLimiter = rateLimit({
  windowMs: 15 * 60 * 1000,
  max: 100,
  standardHeaders: true,
  legacyHeaders: false,
  message: { error: "Too many requests, please try again later" },
});

// Write limiter for subscription management
// 20 requests per 15 minutes per IP
const writeLimiter = rateLimit({
  windowMs: 15 * 60 * 1000,
  max: 20,
  standardHeaders: true,
  legacyHeaders: false,
  message: { error: "Too many requests, please try again later" },
});

// Notify limiter for device notifications (called by ESP32)
// 60 requests per minute per IP
const notifyLimiter = rateLimit({
  windowMs: 60 * 1000,
  max: 60,
  standardHeaders: true,
  legacyHeaders: false,
  message: { error: "Too many notification requests, please try again later" },
});

// Apply general rate limiting to ALL routes in this router
router.use(generalLimiter);

// ============================================================================
// Routes
// ============================================================================

/**
 * GET /api/push/vapid-key
 * Get VAPID public key for client subscription
 */
router.get("/vapid-key", (_req, res) => {
  try {
    const publicKey = getVAPIDPublicKey();
    res.json({ publicKey });
  } catch (error) {
    console.error("Failed to get VAPID key:", error);
    res.status(500).json({ error: "Failed to get VAPID key" });
  }
});

/**
 * POST /api/push/subscribe
 * Subscribe to push notifications (requires auth)
 */
router.post("/subscribe", writeLimiter, sessionAuthMiddleware, (req, res) => {
  try {
    const userId = req.user!.id;
    const { subscription, deviceId } = req.body;

    if (!subscription || !subscription.endpoint || !subscription.keys) {
      return res.status(400).json({ error: "Invalid subscription data" });
    }

    if (deviceId && !userOwnsDevice(userId, deviceId)) {
      return res.status(403).json({ error: "Device not owned by user" });
    }

    const pushSubscription = subscribeToPush(
      userId,
      deviceId || null,
      subscription
    );

    res.json({ success: true, subscription: pushSubscription });
  } catch (error) {
    console.error("Failed to subscribe to push:", error);
    res
      .status(500)
      .json({ error: "Failed to subscribe to push notifications" });
  }
});

/**
 * POST /api/push/unsubscribe
 * Unsubscribe from push notifications
 */
router.post("/unsubscribe", writeLimiter, sessionAuthMiddleware, (req, res) => {
  try {
    const { subscription } = req.body;

    if (!subscription || !subscription.endpoint) {
      return res.status(400).json({ error: "Invalid subscription data" });
    }

    const success = unsubscribeFromPush(subscription);

    res.json({ success });
  } catch (error) {
    console.error("Failed to unsubscribe from push:", error);
    res
      .status(500)
      .json({ error: "Failed to unsubscribe from push notifications" });
  }
});

/**
 * GET /api/push/subscriptions
 * Get user's push subscriptions (requires auth)
 */
router.get("/subscriptions", sessionAuthMiddleware, (req, res) => {
  try {
    const userId = req.user!.id;
    const subscriptions = getUserPushSubscriptions(userId);

    res.json({ subscriptions });
  } catch (error) {
    console.error("Failed to get push subscriptions:", error);
    res.status(500).json({ error: "Failed to get push subscriptions" });
  }
});

/**
 * GET /api/push/preferences
 * Get user's notification preferences (requires auth)
 */
router.get("/preferences", sessionAuthMiddleware, (req, res) => {
  try {
    const userId = req.user!.id;
    const preferences = getOrCreateNotificationPreferences(userId);

    res.json({
      preferences: {
        machineReady: !!preferences.machine_ready,
        waterEmpty: !!preferences.water_empty,
        descaleDue: !!preferences.descale_due,
        serviceDue: !!preferences.service_due,
        backflushDue: !!preferences.backflush_due,
        machineError: !!preferences.machine_error,
        picoOffline: !!preferences.pico_offline,
        scheduleTriggered: !!preferences.schedule_triggered,
        brewComplete: !!preferences.brew_complete,
      },
    });
  } catch (error) {
    console.error("Failed to get notification preferences:", error);
    res.status(500).json({ error: "Failed to get notification preferences" });
  }
});

/**
 * PUT /api/push/preferences
 * Update user's notification preferences (requires auth)
 */
router.put("/preferences", writeLimiter, sessionAuthMiddleware, (req, res) => {
  try {
    const userId = req.user!.id;
    const { preferences } = req.body;

    if (!preferences || typeof preferences !== "object") {
      return res.status(400).json({ error: "Invalid preferences data" });
    }

    const dbPreferences: Record<string, boolean> = {};
    if ("machineReady" in preferences)
      dbPreferences.machine_ready = preferences.machineReady;
    if ("waterEmpty" in preferences)
      dbPreferences.water_empty = preferences.waterEmpty;
    if ("descaleDue" in preferences)
      dbPreferences.descale_due = preferences.descaleDue;
    if ("serviceDue" in preferences)
      dbPreferences.service_due = preferences.serviceDue;
    if ("backflushDue" in preferences)
      dbPreferences.backflush_due = preferences.backflushDue;
    if ("machineError" in preferences)
      dbPreferences.machine_error = preferences.machineError;
    if ("picoOffline" in preferences)
      dbPreferences.pico_offline = preferences.picoOffline;
    if ("scheduleTriggered" in preferences)
      dbPreferences.schedule_triggered = preferences.scheduleTriggered;
    if ("brewComplete" in preferences)
      dbPreferences.brew_complete = preferences.brewComplete;

    const updated = updateNotificationPreferences(userId, dbPreferences);

    res.json({
      success: true,
      preferences: {
        machineReady: !!updated.machine_ready,
        waterEmpty: !!updated.water_empty,
        descaleDue: !!updated.descale_due,
        serviceDue: !!updated.service_due,
        backflushDue: !!updated.backflush_due,
        machineError: !!updated.machine_error,
        picoOffline: !!updated.pico_offline,
        scheduleTriggered: !!updated.schedule_triggered,
        brewComplete: !!updated.brew_complete,
      },
    });
  } catch (error) {
    console.error("Failed to update notification preferences:", error);
    res
      .status(500)
      .json({ error: "Failed to update notification preferences" });
  }
});

/**
 * POST /api/push/notify
 * Send push notification from ESP32 device (no auth - device ID is the auth)
 */
router.post("/notify", notifyLimiter, async (req, res) => {
  try {
    const { deviceId, notification } = req.body;

    if (!deviceId || !notification) {
      return res
        .status(400)
        .json({ error: "Missing deviceId or notification" });
    }

    if (!deviceId.match(/^BRW-[A-F0-9]{8}$/)) {
      return res.status(400).json({ error: "Invalid device ID format" });
    }

    const device = getDevice(deviceId);
    if (!device) {
      return res.status(404).json({ error: "Device not found" });
    }

    const db = getDb();
    const userCheck = db.exec(
      `SELECT COUNT(*) as count FROM user_devices WHERE device_id = ?`,
      [deviceId]
    );
    const hasUsers =
      userCheck.length > 0 &&
      userCheck[0].values.length > 0 &&
      (userCheck[0].values[0][0] as number) > 0;

    if (!hasUsers) {
      return res.status(404).json({ error: "Device not claimed" });
    }

    const notificationType = (notification.type || "info") as NotificationType;
    let title = "BrewOS";
    let body = notification.message || "You have a new notification";
    let requireInteraction = false;
    let tag = "brewos-notification";

    switch (notificationType) {
      case "MACHINE_READY":
        title = "Machine Ready";
        body = notification.message || "Your espresso machine is ready to brew";
        tag = "machine-ready";
        break;
      case "WATER_EMPTY":
        title = "Water Tank Empty";
        body = notification.message || "Please refill the water tank";
        tag = "water-empty";
        requireInteraction = true;
        break;
      case "DESCALE_DUE":
        title = "Descale Due";
        body = notification.message || "Time to descale your machine";
        tag = "descale-due";
        break;
      case "SERVICE_DUE":
        title = "Service Due";
        body = notification.message || "Maintenance recommended";
        tag = "service-due";
        break;
      case "BACKFLUSH_DUE":
        title = "Backflush Reminder";
        body = notification.message || "Time to backflush your machine";
        tag = "backflush-due";
        break;
      case "MACHINE_ERROR":
        title = "Machine Error";
        body = notification.message || "Your machine needs attention";
        tag = "machine-error";
        requireInteraction = true;
        break;
      case "PICO_OFFLINE":
        title = "Control Board Offline";
        body = notification.message || "The control board is not responding";
        tag = "pico-offline";
        requireInteraction = true;
        break;
      case "SCHEDULE_TRIGGERED":
        title = "Schedule Triggered";
        body = notification.message || "A schedule has been triggered";
        tag = "schedule-triggered";
        break;
      case "BREW_COMPLETE":
        title = "Brew Complete";
        body = notification.message || "Your coffee is ready!";
        tag = "brew-complete";
        break;
    }

    const sentCount = await sendPushNotificationToDevice(
      deviceId,
      {
        title,
        body,
        icon: "/logo-icon.svg",
        badge: "/logo-icon.svg",
        tag,
        requireInteraction,
        data: {
          deviceId,
          type: notificationType,
          url: `/device/${deviceId}`,
        },
      },
      notificationType
    );

    res.json({ success: true, sentCount });
  } catch (error) {
    console.error("Failed to send push notification:", error);
    res.status(500).json({ error: "Failed to send push notification" });
  }
});

export default router;
