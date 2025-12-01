import { Router } from 'express';
import { googleAuthMiddleware } from '../middleware/auth.js';
import {
  subscribeToPush,
  unsubscribeFromPush,
  getVAPIDPublicKey,
  getUserPushSubscriptions,
  sendPushNotificationToDevice,
} from '../services/push.js';
import { userOwnsDevice, getDevice } from '../services/device.js';

const router = Router();

/**
 * GET /api/push/vapid-key
 * Get VAPID public key for client subscription
 */
router.get('/vapid-key', (_req, res) => {
  try {
    const publicKey = getVAPIDPublicKey();
    res.json({ publicKey });
  } catch (error) {
    console.error('Failed to get VAPID key:', error);
    res.status(500).json({ error: 'Failed to get VAPID key' });
  }
});

/**
 * POST /api/push/subscribe
 * Subscribe to push notifications (requires auth)
 */
router.post('/subscribe', googleAuthMiddleware, (req, res) => {
  try {
    const userId = req.user!.id;
    const { subscription, deviceId } = req.body;

    if (!subscription || !subscription.endpoint || !subscription.keys) {
      return res.status(400).json({ error: 'Invalid subscription data' });
    }

    // Verify device ownership if deviceId is provided
    if (deviceId && !userOwnsDevice(userId, deviceId)) {
      return res.status(403).json({ error: 'Device not owned by user' });
    }

    const pushSubscription = subscribeToPush(userId, deviceId || null, subscription);

    res.json({ success: true, subscription: pushSubscription });
  } catch (error) {
    console.error('Failed to subscribe to push:', error);
    res.status(500).json({ error: 'Failed to subscribe to push notifications' });
  }
});

/**
 * POST /api/push/unsubscribe
 * Unsubscribe from push notifications
 */
router.post('/unsubscribe', googleAuthMiddleware, (req, res) => {
  try {
    const { subscription } = req.body;

    if (!subscription || !subscription.endpoint) {
      return res.status(400).json({ error: 'Invalid subscription data' });
    }

    const success = unsubscribeFromPush(subscription);

    res.json({ success });
  } catch (error) {
    console.error('Failed to unsubscribe from push:', error);
    res.status(500).json({ error: 'Failed to unsubscribe from push notifications' });
  }
});

/**
 * GET /api/push/subscriptions
 * Get user's push subscriptions (requires auth)
 */
router.get('/subscriptions', googleAuthMiddleware, (req, res) => {
  try {
    const userId = req.user!.id;
    const subscriptions = getUserPushSubscriptions(userId);

    res.json({ subscriptions });
  } catch (error) {
    console.error('Failed to get push subscriptions:', error);
    res.status(500).json({ error: 'Failed to get push subscriptions' });
  }
});

/**
 * POST /api/push/notify
 * Send push notification from ESP32 device (no auth - device ID is the auth)
 */
router.post('/notify', async (req, res) => {
  try {
    const { deviceId, notification } = req.body;

    if (!deviceId || !notification) {
      return res.status(400).json({ error: 'Missing deviceId or notification' });
    }

    // Validate device ID format
    if (!deviceId.match(/^BRW-[A-F0-9]{8}$/)) {
      return res.status(400).json({ error: 'Invalid device ID format' });
    }

    // Verify device exists and is claimed
    const device = getDevice(deviceId);
    if (!device || !device.owner_id) {
      return res.status(404).json({ error: 'Device not found or not claimed' });
    }

    // Map notification type to title/body
    const notificationType = notification.type || 'info';
    let title = 'BrewOS';
    let body = notification.message || 'You have a new notification';
    let requireInteraction = false;
    let tag = 'brewos-notification';

    switch (notificationType) {
      case 'MACHINE_READY':
        title = 'Machine Ready';
        body = notification.message || 'Your espresso machine is ready to brew';
        tag = 'machine-ready';
        break;
      case 'WATER_EMPTY':
        title = 'Water Tank Empty';
        body = notification.message || 'Please refill the water tank';
        tag = 'water-empty';
        requireInteraction = true;
        break;
      case 'DESCALE_DUE':
        title = 'Descale Due';
        body = notification.message || 'Time to descale your machine';
        tag = 'descale-due';
        break;
      case 'SERVICE_DUE':
        title = 'Service Due';
        body = notification.message || 'Maintenance recommended';
        tag = 'service-due';
        break;
      case 'BACKFLUSH_DUE':
        title = 'Backflush Reminder';
        body = notification.message || 'Time to backflush your machine';
        tag = 'backflush-due';
        break;
      case 'MACHINE_ERROR':
        title = 'Machine Error';
        body = notification.message || 'Your machine needs attention';
        tag = 'machine-error';
        requireInteraction = true;
        break;
      case 'PICO_OFFLINE':
        title = 'Control Board Offline';
        body = notification.message || 'The control board is not responding';
        tag = 'pico-offline';
        requireInteraction = true;
        break;
    }

    // Send push notification to all subscriptions for this device
    const sentCount = await sendPushNotificationToDevice(deviceId, {
      title,
      body,
      icon: '/logo-icon.svg',
      badge: '/logo-icon.svg',
      tag,
      requireInteraction,
      data: {
        deviceId,
        type: notificationType,
        url: `/device/${deviceId}`,
      },
    });

    res.json({ success: true, sentCount });
  } catch (error) {
    console.error('Failed to send push notification:', error);
    res.status(500).json({ error: 'Failed to send push notification' });
  }
});

export default router;

