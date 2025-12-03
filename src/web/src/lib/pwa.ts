/**
 * PWA and mode detection utilities
 * 
 * Demo mode is ONLY for cloud website visitors to preview the app.
 * It is NOT available when:
 * - Running as an installed PWA (should use cloud mode)
 * - Running on ESP32 local mode (real hardware, no need for demo)
 */

/**
 * Check if the app is running as an installed PWA (standalone mode)
 * This includes iOS "Add to Home Screen" and Android/Chrome PWA installs.
 */
export function isRunningAsPWA(): boolean {
  if (typeof window === 'undefined') return false;
  
  // Check for standalone display mode (standard PWA detection)
  const isStandalone = window.matchMedia('(display-mode: standalone)').matches;
  
  // Check for iOS standalone mode (Add to Home Screen)
  const isIOSStandalone = (window.navigator as { standalone?: boolean }).standalone === true;
  
  // Check for TWA (Trusted Web Activity) on Android
  const isTWA = document.referrer.includes('android-app://');
  
  return isStandalone || isIOSStandalone || isTWA;
}

/**
 * Check if we're running on ESP32 local mode by checking the hostname.
 * ESP32 serves the web UI on its own IP address or hostname.
 * Cloud mode runs on cloud.brewos.io or localhost (dev).
 */
export function isRunningOnESP32(): boolean {
  if (typeof window === 'undefined') return false;
  
  const hostname = window.location.hostname;
  
  // Cloud domains - NOT ESP32
  if (hostname === 'cloud.brewos.io' || hostname === 'brewos.io') {
    return false;
  }
  
  // Development - NOT ESP32
  if (hostname === 'localhost' || hostname === '127.0.0.1') {
    return false;
  }
  
  // If it's an IP address (e.g., 192.168.x.x) or other hostname, assume ESP32
  return true;
}

/**
 * Check if demo mode should be allowed.
 * Demo mode is ONLY for cloud website visitors to preview the app.
 * 
 * NOT allowed when:
 * - Running as a PWA (use cloud mode)
 * - Running on ESP32 (real hardware)
 */
export function isDemoModeAllowed(): boolean {
  // PWA users should use cloud mode, not demo
  if (isRunningAsPWA()) return false;
  
  // ESP32 users have real hardware, no need for demo
  if (isRunningOnESP32()) return false;
  
  // Only allow demo mode for cloud website visitors in browser
  return true;
}

/**
 * Check if local mode should be allowed
 * Local mode is NOT allowed when running as a PWA
 */
export function isLocalModeAllowed(): boolean {
  return !isRunningAsPWA();
}

