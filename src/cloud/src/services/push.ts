import webpush from 'web-push';
import { getDb, saveDatabase, resultToObjects, PushSubscription } from '../lib/database.js';
import { randomUUID } from 'crypto';

// Initialize web-push with VAPID keys
let vapidKeys: { publicKey: string; privateKey: string } | null = null;

export function initializePushNotifications(): void {
  const publicKey = process.env.VAPID_PUBLIC_KEY;
  const privateKey = process.env.VAPID_PRIVATE_KEY;

  if (!publicKey || !privateKey) {
    console.warn('[Push] VAPID keys not configured. Generating new keys...');
    const keys = webpush.generateVAPIDKeys();
    vapidKeys = {
      publicKey: keys.publicKey,
      privateKey: keys.privateKey,
    };
    console.warn('[Push] Generated VAPID keys. Please set these in your .env file:');
    console.warn(`VAPID_PUBLIC_KEY=${keys.publicKey}`);
    console.warn(`VAPID_PRIVATE_KEY=${keys.privateKey}`);
  } else {
    vapidKeys = {
      publicKey,
      privateKey,
    };
  }

  webpush.setVapidDetails(
    process.env.VAPID_SUBJECT || 'mailto:admin@brewos.app',
    vapidKeys.publicKey,
    vapidKeys.privateKey
  );

  console.log('[Push] Push notifications initialized');
}

export function getVAPIDPublicKey(): string {
  if (!vapidKeys) {
    initializePushNotifications();
  }
  return vapidKeys!.publicKey;
}

/**
 * Subscribe a user to push notifications
 */
export function subscribeToPush(
  userId: string,
  deviceId: string | null,
  subscription: {
    endpoint: string;
    keys: {
      p256dh: string;
      auth: string;
    };
  }
): PushSubscription {
  const db = getDb();
  const now = new Date().toISOString();

  // Check if subscription already exists (by endpoint)
  const existingResult = db.exec(
    `SELECT id FROM push_subscriptions WHERE endpoint = ?`,
    [subscription.endpoint]
  );

  if (existingResult.length > 0 && existingResult[0].values.length > 0) {
    // Update existing subscription
    const id = existingResult[0].values[0][0] as string;
    db.run(
      `UPDATE push_subscriptions 
       SET user_id = ?, device_id = ?, p256dh = ?, auth = ?, updated_at = ?
       WHERE id = ?`,
      [
        userId,
        deviceId || null,
        subscription.keys.p256dh,
        subscription.keys.auth,
        now,
        id,
      ]
    );
    saveDatabase();

    // Return updated subscription
    const result = db.exec(`SELECT * FROM push_subscriptions WHERE id = ?`, [id]);
    return resultToObjects<PushSubscription>(result[0])[0];
  } else {
    // Create new subscription
    const id = randomUUID();
    db.run(
      `INSERT INTO push_subscriptions (id, user_id, device_id, endpoint, p256dh, auth, created_at, updated_at)
       VALUES (?, ?, ?, ?, ?, ?, ?, ?)`,
      [
        id,
        userId,
        deviceId || null,
        subscription.endpoint,
        subscription.keys.p256dh,
        subscription.keys.auth,
        now,
        now,
      ]
    );
    saveDatabase();

    const result = db.exec(`SELECT * FROM push_subscriptions WHERE id = ?`, [id]);
    return resultToObjects<PushSubscription>(result[0])[0];
  }
}

/**
 * Unsubscribe from push notifications
 */
export function unsubscribeFromPush(subscription: {
  endpoint: string;
  keys: {
    p256dh: string;
    auth: string;
  };
}): boolean {
  const db = getDb();
  db.run(`DELETE FROM push_subscriptions WHERE endpoint = ?`, [subscription.endpoint]);
  const deleted = db.getRowsModified();
  
  if (deleted > 0) {
    saveDatabase();
  }
  
  return deleted > 0;
}

/**
 * Get all push subscriptions for a user
 */
export function getUserPushSubscriptions(userId: string): PushSubscription[] {
  const db = getDb();
  const result = db.exec(
    `SELECT * FROM push_subscriptions WHERE user_id = ?`,
    [userId]
  );

  if (result.length === 0) {
    return [];
  }

  return resultToObjects<PushSubscription>(result[0]);
}

/**
 * Get push subscriptions for a device
 */
export function getDevicePushSubscriptions(deviceId: string): PushSubscription[] {
  const db = getDb();
  const result = db.exec(
    `SELECT * FROM push_subscriptions WHERE device_id = ?`,
    [deviceId]
  );

  if (result.length === 0) {
    return [];
  }

  return resultToObjects<PushSubscription>(result[0]);
}

/**
 * Send push notification to a subscription
 */
export async function sendPushNotification(
  subscription: PushSubscription,
  payload: {
    title: string;
    body: string;
    icon?: string;
    badge?: string;
    tag?: string;
    data?: Record<string, unknown>;
    requireInteraction?: boolean;
  }
): Promise<boolean> {
  try {
    const pushSubscription = {
      endpoint: subscription.endpoint,
      keys: {
        p256dh: subscription.p256dh,
        auth: subscription.auth,
      },
    };

    const notificationPayload = JSON.stringify({
      title: payload.title,
      body: payload.body,
      icon: payload.icon || '/logo-icon.svg',
      badge: payload.badge || '/logo-icon.svg',
      tag: payload.tag || 'brewos-notification',
      requireInteraction: payload.requireInteraction || false,
      data: payload.data || {},
    });

    await webpush.sendNotification(pushSubscription, notificationPayload);
    return true;
  } catch (error) {
    console.error('[Push] Failed to send notification:', error);
    
    // If subscription is invalid, remove it
    if (error && typeof error === 'object' && 'statusCode' in error) {
      const statusCode = (error as { statusCode: number }).statusCode;
      if (statusCode === 410 || statusCode === 404) {
        console.log('[Push] Removing invalid subscription');
        unsubscribeFromPush({
          endpoint: subscription.endpoint,
          keys: {
            p256dh: subscription.p256dh,
            auth: subscription.auth,
          },
        });
      }
    }
    
    return false;
  }
}

/**
 * Send push notification to all subscriptions for a user
 */
export async function sendPushNotificationToUser(
  userId: string,
  payload: {
    title: string;
    body: string;
    icon?: string;
    badge?: string;
    tag?: string;
    data?: Record<string, unknown>;
    requireInteraction?: boolean;
  }
): Promise<number> {
  const subscriptions = getUserPushSubscriptions(userId);
  let successCount = 0;

  for (const subscription of subscriptions) {
    const success = await sendPushNotification(subscription, payload);
    if (success) {
      successCount++;
    }
  }

  return successCount;
}

/**
 * Send push notification to all subscriptions for a device
 */
export async function sendPushNotificationToDevice(
  deviceId: string,
  payload: {
    title: string;
    body: string;
    icon?: string;
    badge?: string;
    tag?: string;
    data?: Record<string, unknown>;
    requireInteraction?: boolean;
  }
): Promise<number> {
  const subscriptions = getDevicePushSubscriptions(deviceId);
  let successCount = 0;

  for (const subscription of subscriptions) {
    const success = await sendPushNotification(subscription, payload);
    if (success) {
      successCount++;
    }
  }

  return successCount;
}

