import { useEffect, useState, useCallback } from 'react';
import {
  registerServiceWorker,
  subscribeToPush,
  unsubscribeFromPush,
  getPushSubscription,
  registerPushSubscription,
  unregisterPushSubscription,
  type PushSubscriptionData,
} from '@/lib/push-notifications';
import { useAppStore } from '@/lib/mode';

export interface PushNotificationState {
  isSupported: boolean;
  isRegistered: boolean;
  isSubscribed: boolean;
  permission: NotificationPermission;
  registration: ServiceWorkerRegistration | null;
  subscription: PushSubscription | null;
}

/**
 * Hook to manage push notifications
 */
export function usePushNotifications() {
  const [state, setState] = useState<PushNotificationState>({
    isSupported: false,
    isRegistered: false,
    isSubscribed: false,
    permission: 'default',
    registration: null,
    subscription: null,
  });

  const { user, getSelectedDevice } = useAppStore();

  // Check support and initialize
  useEffect(() => {
    const isSupported =
      'serviceWorker' in navigator && 'PushManager' in window && 'Notification' in window;

    setState((prev) => ({
      ...prev,
      isSupported,
      permission: 'Notification' in window ? Notification.permission : 'denied',
    }));

    if (!isSupported) {
      return;
    }

    // Register service worker and check subscription
    const init = async () => {
      const registration = await registerServiceWorker();
      if (!registration) {
        return;
      }

      const subscription = await getPushSubscription(registration);

      setState((prev) => ({
        ...prev,
        isRegistered: true,
        registration,
        isSubscribed: subscription !== null,
        subscription: subscription || null,
      }));
    };

    init();
  }, []);

  // Subscribe to push notifications
  const subscribe = useCallback(async (): Promise<boolean> => {
    if (!state.registration) {
      console.error('[PWA] Service worker not registered');
      return false;
    }

    const subscription = await subscribeToPush(state.registration);
    if (!subscription) {
      return false;
    }

    // Register with server
    const selectedDevice = getSelectedDevice();
    const success = await registerPushSubscription(
      subscription,
      user?.id,
      selectedDevice?.id
    );

    if (success) {
      setState((prev) => ({
        ...prev,
        isSubscribed: true,
        subscription,
      }));
    }

    return success;
  }, [state.registration, user, getSelectedDevice]);

  // Unsubscribe from push notifications
  const unsubscribe = useCallback(async (): Promise<boolean> => {
    if (!state.registration || !state.subscription) {
      return false;
    }

    // Unregister from server first
    await unregisterPushSubscription(state.subscription);

    // Then unsubscribe locally
    const success = await unsubscribeFromPush(state.registration);

    if (success) {
      setState((prev) => ({
        ...prev,
        isSubscribed: false,
        subscription: null,
      }));
    }

    return success;
  }, [state.registration, state.subscription]);

  // Update subscription when device changes
  useEffect(() => {
    if (state.isSubscribed && state.subscription && user) {
      const selectedDevice = getSelectedDevice();
      registerPushSubscription(state.subscription, user.id, selectedDevice?.id).catch(
        (error) => {
          console.error('[PWA] Failed to update push subscription:', error);
        }
      );
    }
  }, [state.isSubscribed, state.subscription, user, getSelectedDevice]);

  return {
    ...state,
    subscribe,
    unsubscribe,
  };
}

