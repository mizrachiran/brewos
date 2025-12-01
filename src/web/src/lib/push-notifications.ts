/**
 * Push Notification Service
 * Handles PWA service worker registration and push subscription management
 */

export interface PushSubscriptionData {
  endpoint: string;
  keys: {
    p256dh: string;
    auth: string;
  };
}

/**
 * Register service worker for PWA
 */
export async function registerServiceWorker(): Promise<ServiceWorkerRegistration | null> {
  if (!('serviceWorker' in navigator)) {
    console.warn('[PWA] Service workers are not supported');
    return null;
  }

  try {
    const registration = await navigator.serviceWorker.register('/sw.js', {
      scope: '/',
    });

    console.log('[PWA] Service worker registered:', registration.scope);

    // Check for updates
    registration.addEventListener('updatefound', () => {
      const newWorker = registration.installing;
      if (newWorker) {
        newWorker.addEventListener('statechange', () => {
          if (newWorker.state === 'installed' && navigator.serviceWorker.controller) {
            console.log('[PWA] New service worker available');
            // Optionally notify user to refresh
          }
        });
      }
    });

    return registration;
  } catch (error) {
    console.error('[PWA] Service worker registration failed:', error);
    return null;
  }
}

/**
 * Request push notification permission
 */
export async function requestNotificationPermission(): Promise<NotificationPermission> {
  if (!('Notification' in window)) {
    console.warn('[PWA] Notifications are not supported');
    return 'denied';
  }

  if (Notification.permission === 'granted') {
    return 'granted';
  }

  if (Notification.permission === 'denied') {
    return 'denied';
  }

  const permission = await Notification.requestPermission();
  return permission;
}

/**
 * Subscribe to push notifications
 */
export async function subscribeToPush(
  registration: ServiceWorkerRegistration
): Promise<PushSubscription | null> {
  try {
    // Check if already subscribed
    let subscription = await registration.pushManager.getSubscription();

    if (subscription) {
      console.log('[PWA] Already subscribed to push notifications');
      return subscription;
    }

    // Request permission first
    const permission = await requestNotificationPermission();
    if (permission !== 'granted') {
      console.warn('[PWA] Notification permission denied');
      return null;
    }

    // Get VAPID public key from server
    const vapidKeyResponse = await fetch('/api/push/vapid-key');
    if (!vapidKeyResponse.ok) {
      throw new Error('Failed to get VAPID key');
    }

    const { publicKey } = await vapidKeyResponse.json();
    if (!publicKey) {
      throw new Error('VAPID public key not found');
    }

    // Convert base64 URL to Uint8Array
    const applicationServerKey = urlBase64ToUint8Array(publicKey);

    // Subscribe
    subscription = await registration.pushManager.subscribe({
      userVisibleOnly: true,
      applicationServerKey,
    });

    console.log('[PWA] Subscribed to push notifications');
    return subscription;
  } catch (error) {
    console.error('[PWA] Push subscription failed:', error);
    return null;
  }
}

/**
 * Unsubscribe from push notifications
 */
export async function unsubscribeFromPush(
  registration: ServiceWorkerRegistration
): Promise<boolean> {
  try {
    const subscription = await registration.pushManager.getSubscription();
    if (subscription) {
      await subscription.unsubscribe();
      console.log('[PWA] Unsubscribed from push notifications');
      return true;
    }
    return false;
  } catch (error) {
    console.error('[PWA] Push unsubscription failed:', error);
    return false;
  }
}

/**
 * Get current push subscription
 */
export async function getPushSubscription(
  registration: ServiceWorkerRegistration
): Promise<PushSubscription | null> {
  try {
    return await registration.pushManager.getSubscription();
  } catch (error) {
    console.error('[PWA] Failed to get push subscription:', error);
    return null;
  }
}

/**
 * Convert subscription to JSON format for sending to server
 */
export function subscriptionToJSON(subscription: PushSubscription): PushSubscriptionData {
  const key = subscription.getKey('p256dh');
  const auth = subscription.getKey('auth');

  return {
    endpoint: subscription.endpoint,
    keys: {
      p256dh: key ? btoa(String.fromCharCode(...new Uint8Array(key))) : '',
      auth: auth ? btoa(String.fromCharCode(...new Uint8Array(auth))) : '',
    },
  };
}

/**
 * Register push subscription with server
 */
export async function registerPushSubscription(
  subscription: PushSubscription,
  userId?: string,
  deviceId?: string
): Promise<boolean> {
  try {
    const subscriptionData = subscriptionToJSON(subscription);

    const response = await fetch('/api/push/subscribe', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        subscription: subscriptionData,
        userId,
        deviceId,
      }),
    });

    if (!response.ok) {
      throw new Error(`Failed to register subscription: ${response.statusText}`);
    }

    console.log('[PWA] Push subscription registered with server');
    return true;
  } catch (error) {
    console.error('[PWA] Failed to register push subscription:', error);
    return false;
  }
}

/**
 * Unregister push subscription from server
 */
export async function unregisterPushSubscription(
  subscription: PushSubscription
): Promise<boolean> {
  try {
    const subscriptionData = subscriptionToJSON(subscription);

    const response = await fetch('/api/push/unsubscribe', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        subscription: subscriptionData,
      }),
    });

    if (!response.ok) {
      throw new Error(`Failed to unregister subscription: ${response.statusText}`);
    }

    console.log('[PWA] Push subscription unregistered from server');
    return true;
  } catch (error) {
    console.error('[PWA] Failed to unregister push subscription:', error);
    return false;
  }
}

/**
 * Convert base64 URL string to Uint8Array
 */
function urlBase64ToUint8Array(base64String: string): Uint8Array {
  const padding = '='.repeat((4 - (base64String.length % 4)) % 4);
  const base64 = (base64String + padding).replace(/-/g, '+').replace(/_/g, '/');

  const rawData = window.atob(base64);
  const outputArray = new Uint8Array(rawData.length);

  for (let i = 0; i < rawData.length; ++i) {
    outputArray[i] = rawData.charCodeAt(i);
  }

  return outputArray;
}

