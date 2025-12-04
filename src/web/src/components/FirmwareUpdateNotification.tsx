/**
 * Firmware Update Notification Component
 * 
 * Manages periodic firmware update checks and shows in-app notifications.
 * Works alongside native browser notifications when available.
 */

import { useState, useEffect, useCallback } from "react";
import { Download, X, Bell } from "lucide-react";
import { useStore } from "@/lib/store";
import type { UpdateChannel } from "@/lib/updates";
import {
  startFirmwareUpdateChecker,
  stopFirmwareUpdateChecker,
  setFirmwareUpdateCallback,
  checkFirmwareUpdates,
  getFirmwareUpdateNotificationEnabled,
} from "@/lib/firmware-update-checker";
import { useNavigate } from "react-router-dom";

interface PendingUpdate {
  version: string;
  channel: UpdateChannel;
}

export function FirmwareUpdateNotification() {
  const [pendingUpdate, setPendingUpdate] = useState<PendingUpdate | null>(null);
  const [dismissed, setDismissed] = useState(false);
  const esp32Version = useStore((s) => s.esp32.version);
  const navigate = useNavigate();

  // Set up the callback for in-app notifications
  useEffect(() => {
    setFirmwareUpdateCallback((version, channel) => {
      // Only show in-app notification if enabled
      if (getFirmwareUpdateNotificationEnabled()) {
        setPendingUpdate({ version, channel });
        setDismissed(false);
      }
    });
  }, []);

  // Start the update checker when we have a version (and notifications enabled)
  useEffect(() => {
    if (!esp32Version) return;

    // Add version to DOM for the checker to read
    document.body.setAttribute("data-firmware-version", esp32Version);

    // Start periodic checking (will check if notifications are enabled)
    startFirmwareUpdateChecker(esp32Version);

    return () => {
      stopFirmwareUpdateChecker();
      document.body.removeAttribute("data-firmware-version");
    };
  }, [esp32Version]);

  // Clear pending notification if notifications get disabled
  useEffect(() => {
    if (!getFirmwareUpdateNotificationEnabled() && pendingUpdate) {
      setPendingUpdate(null);
    }
  }, [pendingUpdate]);

  const handleViewUpdate = useCallback(() => {
    navigate("/settings?tab=system");
    setDismissed(true);
  }, [navigate]);

  const handleDismiss = useCallback(() => {
    setDismissed(true);
  }, []);

  // Don't render if no update or dismissed
  if (!pendingUpdate || dismissed) {
    return null;
  }

  const channelColors = {
    stable: "bg-emerald-600 text-white",
    beta: "bg-amber-600 text-white",
    dev: "bg-purple-600 text-white",
  };

  const channelLabels = {
    stable: "Official Release",
    beta: "Beta Release",
    dev: "Dev Build",
  };

  return (
    <div className="fixed bottom-4 left-4 right-4 md:left-auto md:right-4 md:w-96 z-50 animate-slide-up">
      <div className={`${channelColors[pendingUpdate.channel]} rounded-xl shadow-2xl p-4`}>
        <div className="flex items-start gap-3">
          <div className="flex-shrink-0 w-10 h-10 bg-white/20 rounded-lg flex items-center justify-center">
            <Bell className="w-5 h-5" />
          </div>
          <div className="flex-1 min-w-0">
            <p className="font-semibold text-sm">Firmware Update Available</p>
            <p className="text-xs opacity-90 mt-0.5">
              {channelLabels[pendingUpdate.channel]}: v{pendingUpdate.version}
            </p>
            <p className="text-xs opacity-75 mt-1">
              {pendingUpdate.channel === "dev" 
                ? "Latest build from main branch"
                : "Tap to view details and install"}
            </p>
          </div>
          <button
            onClick={handleDismiss}
            className="flex-shrink-0 p-1.5 hover:bg-white/20 rounded-lg transition-colors"
            aria-label="Dismiss"
          >
            <X className="w-4 h-4" />
          </button>
        </div>
        <div className="flex gap-2 mt-3">
          <button
            onClick={handleViewUpdate}
            className="flex-1 flex items-center justify-center gap-2 px-4 py-2 bg-white/20 hover:bg-white/30 font-semibold text-sm rounded-lg transition-colors"
          >
            <Download className="w-4 h-4" />
            View Update
          </button>
        </div>
      </div>
    </div>
  );
}

/**
 * Hook to manually trigger a firmware update check
 */
export function useFirmwareUpdateCheck() {
  const esp32Version = useStore((s) => s.esp32.version);
  const [checking, setChecking] = useState(false);

  const checkNow = useCallback(async () => {
    if (!esp32Version || checking) return null;
    
    setChecking(true);
    try {
      const result = await checkFirmwareUpdates(esp32Version, true);
      return result;
    } finally {
      setChecking(false);
    }
  }, [esp32Version, checking]);

  return { checkNow, checking };
}

