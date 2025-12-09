import { useCallback } from 'react';

interface AppBadgeState {
  isSupported: boolean;
  setBadge: (count?: number) => Promise<boolean>;
  clearBadge: () => Promise<boolean>;
}

/**
 * Hook to manage the app badge (notification dot/count on app icon).
 * Works on PWAs installed on the home screen.
 * 
 * @returns Badge state and controls
 */
export function useAppBadge(): AppBadgeState {
  // Check if Badge API is supported
  const isSupported = typeof navigator !== 'undefined' && 'setAppBadge' in navigator;

  /**
   * Set the app badge.
   * @param count - If provided, shows the count. If omitted or 0, shows a dot.
   */
  const setBadge = useCallback(async (count?: number): Promise<boolean> => {
    if (!isSupported) {
      return false;
    }

    try {
      if (count !== undefined && count > 0) {
        await navigator.setAppBadge(count);
      } else {
        // Show dot (no count)
        await navigator.setAppBadge();
      }
      return true;
    } catch (err) {
      // Badge API can fail if permission denied or not in PWA context
      console.warn('[AppBadge] Failed to set badge:', err);
      return false;
    }
  }, [isSupported]);

  /**
   * Clear the app badge.
   */
  const clearBadge = useCallback(async (): Promise<boolean> => {
    if (!isSupported) {
      return false;
    }

    try {
      await navigator.clearAppBadge();
      return true;
    } catch (err) {
      console.warn('[AppBadge] Failed to clear badge:', err);
      return false;
    }
  }, [isSupported]);

  return {
    isSupported,
    setBadge,
    clearBadge,
  };
}
