/**
 * Feature Gate Component
 * 
 * Conditionally renders children based on backend feature availability.
 * Use this to hide UI elements when the backend doesn't support a feature.
 * 
 * @example
 * ```tsx
 * <FeatureGate feature={FEATURES.BREW_BY_WEIGHT}>
 *   <BrewByWeightCard />
 * </FeatureGate>
 * 
 * // With fallback
 * <FeatureGate feature={FEATURES.ECO_MODE} fallback={<FeatureUnavailable />}>
 *   <EcoModeSettings />
 * </FeatureGate>
 * ```
 */

import { type ReactNode } from 'react';
import { useFeature, FEATURES, type FeatureKey } from '@/lib/backend-info';
import { Lock } from 'lucide-react';

interface FeatureGateProps {
  /** Feature key to check */
  feature: FeatureKey;
  /** Content to render when feature is available */
  children: ReactNode;
  /** Content to render when feature is not available (optional) */
  fallback?: ReactNode;
  /** Show a "feature unavailable" message instead of hiding (optional) */
  showUnavailable?: boolean;
  /** Custom unavailable message */
  unavailableMessage?: string;
}

/**
 * Gate that shows children only when feature is available
 */
export function FeatureGate({
  feature,
  children,
  fallback,
  showUnavailable = false,
  unavailableMessage,
}: FeatureGateProps) {
  const isAvailable = useFeature(feature);
  
  if (isAvailable) {
    return <>{children}</>;
  }
  
  if (fallback) {
    return <>{fallback}</>;
  }
  
  if (showUnavailable) {
    return (
      <FeatureUnavailable 
        feature={feature} 
        message={unavailableMessage} 
      />
    );
  }
  
  // Hide entirely
  return null;
}

/**
 * Component shown when a feature is not available
 */
export function FeatureUnavailable({
  feature,
  message,
}: {
  feature?: FeatureKey;
  message?: string;
}) {
  const featureName = feature ? getFeatureDisplayName(feature) : 'This feature';
  
  return (
    <div className="rounded-lg border border-zinc-700 bg-zinc-800/50 p-4 text-center">
      <Lock className="h-8 w-8 text-zinc-500 mx-auto mb-2" />
      <p className="text-sm text-zinc-400">
        {message || `${featureName} is not available on this device.`}
      </p>
      <p className="text-xs text-zinc-500 mt-1">
        Update your firmware to enable this feature.
      </p>
    </div>
  );
}

/**
 * Hook variant for programmatic feature checking
 */
export function useFeatureGate(feature: FeatureKey) {
  const isAvailable = useFeature(feature);
  
  return {
    isAvailable,
    FeatureGate: ({ children, fallback }: { children: ReactNode; fallback?: ReactNode }) => (
      <FeatureGate feature={feature} fallback={fallback}>
        {children}
      </FeatureGate>
    ),
  };
}

/**
 * Get human-readable feature name
 */
function getFeatureDisplayName(feature: FeatureKey): string {
  const names: Record<string, string> = {
    [FEATURES.TEMPERATURE_CONTROL]: 'Temperature Control',
    [FEATURES.PRESSURE_MONITORING]: 'Pressure Monitoring',
    [FEATURES.POWER_MONITORING]: 'Power Monitoring',
    [FEATURES.BREW_BY_WEIGHT]: 'Brew by Weight',
    [FEATURES.BLE_SCALE]: 'BLE Scale',
    [FEATURES.MQTT_INTEGRATION]: 'MQTT Integration',
    [FEATURES.ECO_MODE]: 'Eco Mode',
    [FEATURES.STATISTICS]: 'Statistics',
    [FEATURES.SCHEDULES]: 'Schedules',
    [FEATURES.PUSH_NOTIFICATIONS]: 'Push Notifications',
    [FEATURES.REMOTE_ACCESS]: 'Remote Access',
    [FEATURES.MULTI_DEVICE]: 'Multi-Device',
    [FEATURES.PICO_OTA]: 'Pico OTA Updates',
    [FEATURES.ESP32_OTA]: 'ESP32 OTA Updates',
    [FEATURES.DEBUG_CONSOLE]: 'Debug Console',
    [FEATURES.PROTOCOL_DEBUG]: 'Protocol Debug',
  };
  
  return names[feature] || feature;
}

// Re-export FEATURES for convenience
export { FEATURES };

