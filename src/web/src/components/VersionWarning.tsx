/**
 * Version Compatibility Warning Component
 * 
 * Displays warnings when the web UI detects potential compatibility issues
 * with the backend (ESP32 or Cloud server).
 */

import { useBackendInfo } from '@/lib/backend-info';
import { AlertTriangle, X, Info, ExternalLink } from 'lucide-react';
import { useState, useEffect } from 'react';

interface VersionWarningProps {
  /** Whether to allow dismissing the warning */
  dismissible?: boolean;
  /** Callback when warning is dismissed */
  onDismiss?: () => void;
}

export function VersionWarning({ dismissible = true, onDismiss }: VersionWarningProps) {
  const { info, compatible, warnings, errors } = useBackendInfo();
  const [dismissed, setDismissed] = useState(false);
  
  // Check localStorage for previously dismissed warnings
  useEffect(() => {
    const dismissedVersion = localStorage.getItem('brewos-dismissed-version-warning');
    if (dismissedVersion === info?.firmwareVersion) {
      setDismissed(true);
    }
  }, [info?.firmwareVersion]);
  
  // Nothing to show
  if (!info || (compatible && warnings.length === 0)) {
    return null;
  }
  
  // Already dismissed
  if (dismissed && dismissible) {
    return null;
  }
  
  const handleDismiss = () => {
    setDismissed(true);
    // Remember dismissal for this firmware version
    if (info?.firmwareVersion) {
      localStorage.setItem('brewos-dismissed-version-warning', info.firmwareVersion);
    }
    onDismiss?.();
  };
  
  // Critical errors (incompatible)
  if (!compatible && errors.length > 0) {
    return (
      <div className="rounded-lg border border-red-500/50 bg-red-500/10 p-4 mb-4">
        <div className="flex items-start gap-3">
          <AlertTriangle className="h-5 w-5 text-red-500 flex-shrink-0 mt-0.5" />
          <div className="flex-1 min-w-0">
            <h3 className="font-medium text-red-500">Version Incompatibility</h3>
            <div className="mt-1 text-sm text-red-400 space-y-1">
              {errors.map((error, i) => (
                <p key={i}>{error}</p>
              ))}
            </div>
            <div className="mt-3">
              <a 
                href="/settings/system" 
                className="inline-flex items-center gap-1 text-sm text-red-400 hover:text-red-300 underline"
              >
                Update Firmware
                <ExternalLink className="h-3.5 w-3.5" />
              </a>
            </div>
          </div>
          {dismissible && (
            <button
              onClick={handleDismiss}
              className="text-red-400 hover:text-red-300"
              aria-label="Dismiss"
            >
              <X className="h-4 w-4" />
            </button>
          )}
        </div>
      </div>
    );
  }
  
  // Non-critical warnings
  if (warnings.length > 0) {
    return (
      <div className="rounded-lg border border-amber-500/50 bg-amber-500/10 p-4 mb-4">
        <div className="flex items-start gap-3">
          <Info className="h-5 w-5 text-amber-500 flex-shrink-0 mt-0.5" />
          <div className="flex-1 min-w-0">
            <h3 className="font-medium text-amber-500">Compatibility Notice</h3>
            <div className="mt-1 text-sm text-amber-400 space-y-1">
              {warnings.map((warning, i) => (
                <p key={i}>{warning}</p>
              ))}
            </div>
          </div>
          {dismissible && (
            <button
              onClick={handleDismiss}
              className="text-amber-400 hover:text-amber-300"
              aria-label="Dismiss"
            >
              <X className="h-4 w-4" />
            </button>
          )}
        </div>
      </div>
    );
  }
  
  return null;
}

/**
 * Compact version badge for headers/footers
 */
export function VersionBadge() {
  const { info } = useBackendInfo();
  
  if (!info) return null;
  
  return (
    <span className="text-xs text-zinc-500">
      v{info.firmwareVersion}
      {info.mode === 'cloud' && ' (cloud)'}
    </span>
  );
}

/**
 * Hook to check if a specific feature is available
 * Shows a toast/notification if feature is not available
 */
export function useFeatureCheck() {
  const { info, hasFeature } = useBackendInfo();
  
  return {
    checkFeature: (feature: string, featureName?: string): boolean => {
      const available = hasFeature(feature as Parameters<typeof hasFeature>[0]);
      if (!available && featureName) {
        console.warn(`Feature "${featureName}" is not available on this device`);
      }
      return available;
    },
    info,
  };
}

