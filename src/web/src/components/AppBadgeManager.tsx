import { useEffect } from 'react';
import { useAppBadge } from '@/hooks/useAppBadge';
import { useStore } from '@/lib/store';
import { useAppStore } from '@/lib/mode';

/**
 * Manages the app badge (notification dot) on the PWA icon.
 * Shows a badge when any machine is online/powered on.
 * 
 * Should be mounted once at the app root level.
 */
export function AppBadgeManager() {
  const { isSupported, setBadge, clearBadge } = useAppBadge();
  const showAppBadge = useStore((s) => s.preferences.showAppBadge);
  
  // Cloud mode: check if any device is online
  const { mode, devices } = useAppStore();
  const onlineDeviceCount = devices.filter((d) => d.isOnline).length;
  
  // Local mode: check if machine is on (mode is not "off")
  const machineMode = useStore((s) => s.machine.mode);
  const isLocalMachineOn = machineMode !== 'off';

  useEffect(() => {
    if (!isSupported) return;

    // Determine if we should show the badge
    const shouldShowBadge = showAppBadge && (
      mode === 'cloud' 
        ? onlineDeviceCount > 0 
        : isLocalMachineOn
    );

    if (shouldShowBadge) {
      setBadge();
    } else {
      clearBadge();
    }

    // Cleanup on unmount
    return () => {
      clearBadge();
    };
  }, [isSupported, showAppBadge, mode, onlineDeviceCount, isLocalMachineOn, setBadge, clearBadge]);

  // This component doesn't render anything
  return null;
}

