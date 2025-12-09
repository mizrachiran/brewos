import { useState, useEffect, useCallback, useRef } from 'react';

interface WakeLockState {
  isSupported: boolean;
  isActive: boolean;
  request: () => Promise<boolean>;
  release: () => Promise<void>;
}

/**
 * Hook to prevent the screen from dimming or locking while active.
 * Useful for brewing, recipes, or any screen that should stay on.
 * 
 * @param autoActivate - If true, automatically requests wake lock on mount
 * @returns Wake lock state and controls
 */
export function useWakeLock(autoActivate = false): WakeLockState {
  const [isActive, setIsActive] = useState(false);
  const wakeLockRef = useRef<WakeLockSentinel | null>(null);
  
  // Check if Wake Lock API is supported
  const isSupported = typeof navigator !== 'undefined' && 'wakeLock' in navigator;

  // Request wake lock
  const request = useCallback(async (): Promise<boolean> => {
    if (!isSupported) {
      console.log('[WakeLock] Not supported in this browser');
      return false;
    }

    // Already have an active lock
    if (wakeLockRef.current) {
      return true;
    }

    try {
      wakeLockRef.current = await navigator.wakeLock.request('screen');
      setIsActive(true);
      
      console.log('[WakeLock] Acquired');

      // Handle release (e.g., when tab becomes hidden)
      wakeLockRef.current.addEventListener('release', () => {
        console.log('[WakeLock] Released');
        wakeLockRef.current = null;
        setIsActive(false);
      });

      return true;
    } catch (err) {
      // Wake lock request can fail (e.g., low battery, page not visible)
      console.warn('[WakeLock] Failed to acquire:', err);
      wakeLockRef.current = null;
      setIsActive(false);
      return false;
    }
  }, [isSupported]);

  // Release wake lock
  const release = useCallback(async (): Promise<void> => {
    if (wakeLockRef.current) {
      try {
        await wakeLockRef.current.release();
        console.log('[WakeLock] Manually released');
      } catch (err) {
        console.warn('[WakeLock] Failed to release:', err);
      }
      wakeLockRef.current = null;
      setIsActive(false);
    }
  }, []);

  // Re-acquire wake lock when page becomes visible again
  useEffect(() => {
    if (!isSupported) return;

    const handleVisibilityChange = async () => {
      if (document.visibilityState === 'visible' && !wakeLockRef.current && isActive) {
        // Page became visible and we had an active lock - re-acquire
        await request();
      }
    };

    document.addEventListener('visibilitychange', handleVisibilityChange);
    return () => document.removeEventListener('visibilitychange', handleVisibilityChange);
  }, [isSupported, isActive, request]);

  // Auto-activate on mount if requested
  useEffect(() => {
    if (autoActivate && isSupported) {
      request();
    }

    // Cleanup on unmount
    return () => {
      if (wakeLockRef.current) {
        wakeLockRef.current.release().catch(() => {});
        wakeLockRef.current = null;
      }
    };
  }, [autoActivate, isSupported, request]);

  return {
    isSupported,
    isActive,
    request,
    release,
  };
}

