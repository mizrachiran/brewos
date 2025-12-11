import { useStore } from "@/lib/store";
import { getActiveConnection } from "@/lib/connection";
import { Wifi, RefreshCw, X, Download, RotateCw, AlertCircle } from "lucide-react";
import { Button } from "./Button";
import { useState, useEffect } from "react";

// Allow bypassing overlay in development for easier testing
const DEV_MODE = import.meta.env.DEV;
const DEV_BYPASS_KEY = "brewos-dev-bypass-overlay";

// Debounce time before hiding overlay after connection
const HIDE_DELAY_MS = 500;

// Reconnection settings after OTA
const OTA_RECONNECT_DELAY_MS = 3000;
const OTA_RECONNECT_MAX_ATTEMPTS = 30; // 30 attempts * 3 seconds = 90 seconds max

export function ConnectionOverlay() {
  const connectionState = useStore((s) => s.connectionState);
  const ota = useStore((s) => s.ota);
  const [retrying, setRetrying] = useState(false);
  const [reconnectAttempts, setReconnectAttempts] = useState(0);

  // Check localStorage for dev bypass preference
  const [devBypassed, setDevBypassed] = useState(() => {
    if (!DEV_MODE) return false;
    return localStorage.getItem(DEV_BYPASS_KEY) === "true";
  });

  const isConnected = connectionState === "connected";
  const isUpdating = ota.isUpdating || ota.stage === "complete";
  const [isVisible, setIsVisible] = useState(!isConnected || isUpdating);

  // Visibility control - show during OTA or when not connected
  useEffect(() => {
    if (isUpdating) {
      // Always show during OTA
      setIsVisible(true);
    } else if (isConnected) {
      // Hide with delay when connected (and not updating)
      const timeout = setTimeout(() => {
        setIsVisible(false);
        setReconnectAttempts(0);
      }, HIDE_DELAY_MS);
      return () => clearTimeout(timeout);
    } else {
      setIsVisible(true);
    }
  }, [isConnected, isUpdating]);

  // Auto-reconnect after OTA complete (device restarts)
  useEffect(() => {
    if (ota.stage === "complete" && !isConnected) {
      const attemptReconnect = () => {
        if (reconnectAttempts < OTA_RECONNECT_MAX_ATTEMPTS) {
          console.log(`[OTA] Reconnect attempt ${reconnectAttempts + 1}/${OTA_RECONNECT_MAX_ATTEMPTS}`);
          getActiveConnection()?.connect();
          setReconnectAttempts((prev) => prev + 1);
        }
      };

      const timeout = setTimeout(attemptReconnect, OTA_RECONNECT_DELAY_MS);
      return () => clearTimeout(timeout);
    }
  }, [ota.stage, isConnected, reconnectAttempts]);

  // Lock body scroll when overlay is visible
  useEffect(() => {
    if (isVisible) {
      const scrollY = window.scrollY;
      document.body.style.position = "fixed";
      document.body.style.top = `-${scrollY}px`;
      document.body.style.left = "0";
      document.body.style.right = "0";
      document.body.style.overflow = "hidden";

      return () => {
        document.body.style.position = "";
        document.body.style.top = "";
        document.body.style.left = "";
        document.body.style.right = "";
        document.body.style.overflow = "";
        window.scrollTo(0, scrollY);
      };
    }
  }, [isVisible]);

  const handleDevBypass = () => {
    localStorage.setItem(DEV_BYPASS_KEY, "true");
    setDevBypassed(true);
  };

  // Don't render if bypassed in dev mode or not visible
  if (DEV_MODE && devBypassed) return null;
  if (!isVisible) return null;

  const handleRetry = async () => {
    setRetrying(true);
    try {
      await getActiveConnection()?.connect();
    } catch (e) {
      console.error("Retry failed:", e);
    }
    setRetrying(false);
  };

  // Determine status based on OTA state or connection state
  const isRetryingOrConnecting = retrying || 
    connectionState === "connecting" || 
    connectionState === "reconnecting";

  // Build status based on current state
  const getStatus = () => {
    // OTA in progress
    if (ota.isUpdating) {
      const stageLabels: Record<string, string> = {
        download: "Downloading update...",
        flash: "Installing update...",
      };
      return {
        icon: <Download className="w-16 h-16 text-accent" />,
        title: stageLabels[ota.stage] || "Updating...",
        subtitle: ota.message || `Progress: ${ota.progress}%`,
        showRetry: false,
        showPulse: true,
        showProgress: true,
        progress: ota.progress,
      };
    }

    // OTA complete - waiting for device to restart
    if (ota.stage === "complete") {
      return {
        icon: <RotateCw className="w-16 h-16 text-accent animate-spin" />,
        title: "Update installed!",
        subtitle: "Device is restarting... Please wait.",
        showRetry: false,
        showPulse: false,
        showProgress: false,
        progress: 100,
      };
    }

    // OTA error
    if (ota.stage === "error") {
      return {
        icon: <AlertCircle className="w-16 h-16 text-error" />,
        title: "Update failed",
        subtitle: ota.message || "The device will restart.",
        showRetry: false,
        showPulse: false,
        showProgress: false,
        progress: 0,
      };
    }

    // Normal connection state
    return {
      icon: <Wifi className="w-16 h-16 text-accent" />,
      title: "Connecting to your machine...",
      subtitle: "Please wait while we establish a connection",
      showRetry: !isRetryingOrConnecting,
      showPulse: true,
      showProgress: false,
      progress: 0,
    };
  };

  const status = getStatus();

  return (
    <div className="fixed inset-0 z-[60] flex items-center justify-center p-6 bg-theme/95 backdrop-blur-md">
      {/* Dev mode bypass button */}
      {DEV_MODE && (
        <button
          onClick={handleDevBypass}
          className="absolute top-4 right-4 p-2 rounded-lg bg-theme-tertiary hover:bg-theme-secondary text-theme-muted transition-colors"
          title="Bypass overlay (dev mode only - persists in localStorage)"
        >
          <X className="w-5 h-5" />
        </button>
      )}

      <div className="max-w-md w-full text-center space-y-6">
        {/* Animated Icon */}
        <div className="relative inline-flex items-center justify-center">
          {/* Pulsing ring for connecting state */}
          {status.showPulse && (
            <>
              <div className="absolute inset-0 w-24 h-24 -m-4 rounded-full bg-accent/20 animate-ping" />
              <div className="absolute inset-0 w-20 h-20 -m-2 rounded-full bg-accent/30 animate-pulse" />
            </>
          )}
          <div className={status.showPulse ? "animate-pulse" : ""}>
            {status.icon}
          </div>
        </div>

        {/* Status Text */}
        <div className="space-y-2">
          <h2 className="text-2xl font-bold text-theme">{status.title}</h2>
          <p className="text-theme-muted leading-relaxed">{status.subtitle}</p>
        </div>

        {/* OTA Progress Bar */}
        {status.showProgress && (
          <div className="w-full max-w-xs mx-auto">
            <div className="h-2 bg-theme-tertiary rounded-full overflow-hidden">
              <div 
                className="h-full bg-accent transition-all duration-300 ease-out rounded-full"
                style={{ width: `${status.progress}%` }}
              />
            </div>
            <p className="text-sm text-theme-muted mt-2">{status.progress}%</p>
          </div>
        )}

        {/* Retry Button */}
        {status.showRetry && (
          <Button
            onClick={handleRetry}
            loading={retrying}
            variant="primary"
            size="lg"
            className="min-w-40"
          >
            <RefreshCw className="w-5 h-5" />
            Retry Connection
          </Button>
        )}

        {/* Connection attempts indicator */}
        {status.showPulse && (
          <div className="flex items-center justify-center gap-1.5 text-xs text-theme-muted">
            <span
              className="w-1.5 h-1.5 rounded-full bg-accent animate-bounce"
              style={{ animationDelay: "0ms" }}
            />
            <span
              className="w-1.5 h-1.5 rounded-full bg-accent animate-bounce"
              style={{ animationDelay: "150ms" }}
            />
            <span
              className="w-1.5 h-1.5 rounded-full bg-accent animate-bounce"
              style={{ animationDelay: "300ms" }}
            />
          </div>
        )}

        {/* Dev mode hint */}
        {DEV_MODE && (
          <p className="text-xs text-theme-muted opacity-60">
            Dev mode: Click × to bypass (saved to localStorage)
          </p>
        )}
      </div>
    </div>
  );
}
