/**
 * Backend Info Store
 * 
 * Fetches and stores backend capabilities for feature detection.
 * This is used by the entire app to conditionally render features.
 */

import { create } from 'zustand';
import type { BackendInfo, FeatureKey } from './api-version';
import { 
  API_VERSION, 
  FEATURES, 
  hasFeature, 
  checkCompatibility,
  WEB_UI_VERSION,
} from './api-version';

interface BackendInfoState {
  // Backend info from /api/info
  info: BackendInfo | null;
  
  // Loading/error state
  loading: boolean;
  error: string | null;
  
  // Compatibility status
  compatible: boolean;
  warnings: string[];
  errors: string[];
  
  // Actions
  fetchInfo: () => Promise<BackendInfo | null>;
  clearInfo: () => void;
  
  // Helper methods bound to current info
  hasFeature: (feature: FeatureKey) => boolean;
  isLocal: () => boolean;
  isCloud: () => boolean;
}

/**
 * Default features for local/ESP32 mode
 * These are the features typically available on ESP32
 */
const DEFAULT_LOCAL_FEATURES: FeatureKey[] = [
  FEATURES.TEMPERATURE_CONTROL,
  FEATURES.PRESSURE_MONITORING,
  FEATURES.POWER_MONITORING,
  FEATURES.BREW_BY_WEIGHT,
  FEATURES.BLE_SCALE,
  FEATURES.MQTT_INTEGRATION,
  FEATURES.ECO_MODE,
  FEATURES.STATISTICS,
  FEATURES.PICO_OTA,
  FEATURES.ESP32_OTA,
  FEATURES.DEBUG_CONSOLE,
];

/**
 * Default features for cloud mode
 * Cloud has all local features plus cloud-specific ones
 */
const DEFAULT_CLOUD_FEATURES: FeatureKey[] = [
  ...DEFAULT_LOCAL_FEATURES,
  FEATURES.PUSH_NOTIFICATIONS,
  FEATURES.REMOTE_ACCESS,
  FEATURES.MULTI_DEVICE,
];

/**
 * Create fallback info when /api/info is not available
 * This ensures backward compatibility with older firmware
 */
function createFallbackInfo(mode: 'local' | 'cloud'): BackendInfo {
  return {
    apiVersion: API_VERSION,
    firmwareVersion: 'unknown',
    mode,
    features: mode === 'cloud' ? DEFAULT_CLOUD_FEATURES : DEFAULT_LOCAL_FEATURES,
  };
}

export const useBackendInfo = create<BackendInfoState>((set, get) => ({
  info: null,
  loading: false,
  error: null,
  compatible: true,
  warnings: [],
  errors: [],
  
  fetchInfo: async () => {
    set({ loading: true, error: null });
    
    try {
      // First try /api/info (new endpoint)
      let info: BackendInfo | null = null;
      
      try {
        const infoResponse = await fetch('/api/info');
        if (infoResponse.ok) {
          info = await infoResponse.json();
        }
      } catch {
        // /api/info not available, will use fallback
      }
      
      // If /api/info failed, try /api/mode for basic mode detection
      if (!info) {
        try {
          const modeResponse = await fetch('/api/mode');
          if (modeResponse.ok) {
            const modeData = await modeResponse.json();
            const mode = modeData.mode === 'cloud' ? 'cloud' : 'local';
            info = createFallbackInfo(mode);
          }
        } catch {
          // Default to local mode if even /api/mode fails
          info = createFallbackInfo('local');
        }
      }
      
      if (!info) {
        info = createFallbackInfo('local');
      }
      
      // Check compatibility
      const { compatible, warnings, errors } = checkCompatibility(info);
      
      set({ 
        info, 
        loading: false, 
        error: null,
        compatible,
        warnings,
        errors,
      });
      
      return info;
    } catch (err) {
      const errorMessage = err instanceof Error ? err.message : 'Failed to fetch backend info';
      set({ 
        loading: false, 
        error: errorMessage,
        info: createFallbackInfo('local'),
        compatible: true,
        warnings: ['Could not verify backend compatibility'],
        errors: [],
      });
      return null;
    }
  },
  
  clearInfo: () => {
    set({ 
      info: null, 
      loading: false, 
      error: null,
      compatible: true,
      warnings: [],
      errors: [],
    });
  },
  
  hasFeature: (feature: FeatureKey) => {
    return hasFeature(get().info, feature);
  },
  
  isLocal: () => {
    return get().info?.mode === 'local';
  },
  
  isCloud: () => {
    return get().info?.mode === 'cloud';
  },
}));

/**
 * Hook to check if a feature is available
 * Usage: const hasBBW = useFeature(FEATURES.BREW_BY_WEIGHT);
 */
export function useFeature(feature: FeatureKey): boolean {
  return useBackendInfo((state) => hasFeature(state.info, feature));
}

/**
 * Hook to check multiple features at once
 * Usage: const { hasBBW, hasScale } = useFeatures([FEATURES.BREW_BY_WEIGHT, FEATURES.BLE_SCALE]);
 */
export function useFeatures(features: FeatureKey[]): Record<FeatureKey, boolean> {
  const info = useBackendInfo((state) => state.info);
  const result: Record<string, boolean> = {};
  
  for (const feature of features) {
    result[feature] = hasFeature(info, feature);
  }
  
  return result as Record<FeatureKey, boolean>;
}

/**
 * Get the current backend info (non-reactive)
 * Useful for one-time checks outside of React components
 */
export function getBackendInfo(): BackendInfo | null {
  return useBackendInfo.getState().info;
}

/**
 * Check if running in local (ESP32) mode
 */
export function isLocalMode(): boolean {
  return useBackendInfo.getState().info?.mode === 'local';
}

/**
 * Check if running in cloud mode
 */
export function isCloudMode(): boolean {
  return useBackendInfo.getState().info?.mode === 'cloud';
}

// Re-export for convenience
export { FEATURES, WEB_UI_VERSION };
export type { BackendInfo, FeatureKey };

