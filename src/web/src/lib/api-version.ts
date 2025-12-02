/**
 * API Version Management
 * 
 * This module defines the API versioning system for BrewOS.
 * It provides compatibility checking between web UI and backend (ESP32/Cloud).
 * 
 * VERSION HISTORY:
 * - API v1 (0.1.0): Initial release
 * - API v2 (0.2.0): Added eco mode, statistics
 * - API v3 (future): Reserved for breaking changes
 * 
 * RULES:
 * - Increment API_VERSION only for breaking changes to WebSocket/REST APIs
 * - Add new features to FEATURES without incrementing API_VERSION
 * - Web UI should gracefully degrade when features are missing
 */

// Current API version - increment for BREAKING changes only
export const API_VERSION = 1;

// Minimum API version this web UI supports
// If backend reports lower version, show upgrade warning
export const MIN_SUPPORTED_API_VERSION = 1;

// Current web UI version (should match package.json)
export const WEB_UI_VERSION = '0.2.0';

/**
 * Feature flags - granular capability detection
 * Add new features here as they're implemented in the backend
 */
export const FEATURES = {
  // Core features
  TEMPERATURE_CONTROL: 'temperature_control',
  PRESSURE_MONITORING: 'pressure_monitoring',
  POWER_MONITORING: 'power_monitoring',
  
  // Advanced features
  BREW_BY_WEIGHT: 'bbw',
  BLE_SCALE: 'scale',
  MQTT_INTEGRATION: 'mqtt',
  ECO_MODE: 'eco_mode',
  STATISTICS: 'statistics',
  SCHEDULES: 'schedules',
  
  // Cloud-only features
  PUSH_NOTIFICATIONS: 'push_notifications',
  REMOTE_ACCESS: 'remote_access',
  MULTI_DEVICE: 'multi_device',
  
  // OTA features
  PICO_OTA: 'pico_ota',
  ESP32_OTA: 'esp32_ota',
  
  // Debug features
  DEBUG_CONSOLE: 'debug_console',
  PROTOCOL_DEBUG: 'protocol_debug',
} as const;

export type FeatureKey = typeof FEATURES[keyof typeof FEATURES];

/**
 * Backend information returned by /api/info
 */
export interface BackendInfo {
  // API version for breaking change detection
  apiVersion: number;
  
  // Component versions
  firmwareVersion: string;      // ESP32 firmware or cloud server version
  webVersion?: string;          // Web UI version bundled with ESP32
  picoVersion?: string;         // Connected Pico firmware version
  protocolVersion?: number;     // Pico-ESP32 binary protocol version
  
  // Mode detection
  mode: 'local' | 'cloud';
  
  // Granular feature flags
  features: FeatureKey[];
  
  // Device info (optional)
  deviceId?: string;
  machineName?: string;
  machineType?: string;
}

/**
 * Check if a specific feature is supported by the backend
 */
export function hasFeature(info: BackendInfo | null, feature: FeatureKey): boolean {
  if (!info?.features) return false;
  return info.features.includes(feature);
}

/**
 * Check if multiple features are all supported
 */
export function hasAllFeatures(info: BackendInfo | null, features: FeatureKey[]): boolean {
  return features.every(f => hasFeature(info, f));
}

/**
 * Check if at least one of the features is supported
 */
export function hasAnyFeature(info: BackendInfo | null, features: FeatureKey[]): boolean {
  return features.some(f => hasFeature(info, f));
}

/**
 * Check if backend meets minimum API version requirement
 */
export function meetsMinApiVersion(info: BackendInfo | null, minVersion: number = MIN_SUPPORTED_API_VERSION): boolean {
  if (!info) return false;
  return info.apiVersion >= minVersion;
}

/**
 * Check if web UI is compatible with backend
 * Returns compatibility status and any warnings
 */
export function checkCompatibility(info: BackendInfo | null): {
  compatible: boolean;
  warnings: string[];
  errors: string[];
} {
  const warnings: string[] = [];
  const errors: string[] = [];
  
  if (!info) {
    errors.push('Unable to detect backend version');
    return { compatible: false, warnings, errors };
  }
  
  // Check API version
  if (info.apiVersion < MIN_SUPPORTED_API_VERSION) {
    errors.push(
      `Backend API version ${info.apiVersion} is too old. ` +
      `This web UI requires API version ${MIN_SUPPORTED_API_VERSION} or higher. ` +
      `Please update your device firmware.`
    );
  }
  
  // Check if web UI is significantly newer than firmware
  if (info.mode === 'local' && info.webVersion) {
    const webMajor = parseInt(WEB_UI_VERSION.split('.')[0], 10);
    const fwMajor = parseInt(info.webVersion.split('.')[0], 10);
    
    if (webMajor > fwMajor) {
      warnings.push(
        `You're using a newer web UI (${WEB_UI_VERSION}) than your device supports (${info.webVersion}). ` +
        `Some features may not work correctly.`
      );
    }
  }
  
  // Warn about missing common features
  const expectedFeatures = [FEATURES.TEMPERATURE_CONTROL, FEATURES.POWER_MONITORING];
  const missingCore = expectedFeatures.filter(f => !hasFeature(info, f));
  if (missingCore.length > 0) {
    warnings.push(`Some core features may be unavailable: ${missingCore.join(', ')}`);
  }
  
  return {
    compatible: errors.length === 0,
    warnings,
    errors,
  };
}

/**
 * Parse semantic version string
 */
export function parseVersion(version: string): { major: number; minor: number; patch: number } {
  const match = version.match(/^v?(\d+)\.(\d+)\.(\d+)/);
  if (!match) {
    return { major: 0, minor: 0, patch: 0 };
  }
  return {
    major: parseInt(match[1], 10),
    minor: parseInt(match[2], 10),
    patch: parseInt(match[3], 10),
  };
}

/**
 * Compare two semantic versions
 * @returns -1 if a < b, 0 if a == b, 1 if a > b
 */
export function compareVersions(a: string, b: string): number {
  const va = parseVersion(a);
  const vb = parseVersion(b);
  
  if (va.major !== vb.major) return va.major > vb.major ? 1 : -1;
  if (va.minor !== vb.minor) return va.minor > vb.minor ? 1 : -1;
  if (va.patch !== vb.patch) return va.patch > vb.patch ? 1 : -1;
  return 0;
}

