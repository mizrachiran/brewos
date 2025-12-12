import { useStore } from "@/lib/store";
import { getActiveConnection } from "@/lib/connection";
import { Wifi, RefreshCw, X, Download, AlertCircle, Power } from "lucide-react";
import { Button } from "./Button";
import { useState, useEffect } from "react";

// Allow bypassing overlay in development for easier testing
const DEV_MODE = import.meta.env.DEV;
const DEV_BYPASS_KEY = "brewos-dev-bypass-overlay";

// Debounce time before hiding overlay after connection
const HIDE_DELAY_MS = 500;

// Key to track OTA in progress across page reloads (shared with store.ts)
const OTA_IN_PROGRESS_KEY = "brewos-ota-in-progress";

export function ConnectionOverlay() {
  const connectionState = useStore((s) => s.connectionState);
  const machineState = useStore((s) => s.machine.state);
  const ota = useStore((s) => s.ota);
  const [retrying, setRetrying] = useState(false);

  // Check localStorage for dev bypass preference
  const [devBypassed, setDevBypassed] = useState(() => {
    if (!DEV_MODE) return false;
    return localStorage.getItem(DEV_BYPASS_KEY) === "true";
  });

  const isConnected = connectionState === "connected";
  const isDeviceOffline = machineState === "offline";  // Device offline in cloud mode
  const isUpdating = ota.isUpdating || ota.stage === "complete";
  const [isVisible, setIsVisible] = useState(!isConnected || isUpdating || isDeviceOffline);

  // Track OTA state in localStorage so store.ts can detect it after reconnect
  useEffect(() => {
    if (ota.isUpdating) {
      localStorage.setItem(OTA_IN_PROGRESS_KEY, "true");
    }
  }, [ota.isUpdating]);

  // Visibility control - show during OTA, when not connected, or when device is offline
  useEffect(() => {
    if (isUpdating) {
      // Always show during OTA
      setIsVisible(true);
    } else if (isDeviceOffline) {
      // Show when device is offline (cloud mode)
      setIsVisible(true);
    } else if (isConnected) {
      // Hide with delay when connected (and not updating and device is online)
      const timeout = setTimeout(() => {
        setIsVisible(false);
      }, HIDE_DELAY_MS);
      return () => clearTimeout(timeout);
    } else {
      setIsVisible(true);
    }
  }, [isConnected, isUpdating, isDeviceOffline]);

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

  // Don't render if not visible
  if (!isVisible) return null;
  
  // Dev bypass only works for connection issues, not device offline or OTA
  // This allows testing the UI without a real connection but still shows important overlays
  if (DEV_MODE && devBypassed && !isDeviceOffline && !isUpdating) return null;

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
    // OTA in progress - show simple animation without progress bar
    if (ota.isUpdating || ota.stage === "complete") {
      return {
        icon: <Download className="w-16 h-16 text-accent" />,
        title: "Updating BrewOS...",
        subtitle: "Please wait while the update is being installed. The device will restart automatically.",
        showRetry: false,
        showPulse: true,
        isOTA: true,
        isDeviceOffline: false,
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
        isOTA: false,
        isDeviceOffline: false,
      };
    }

    // Device offline (cloud mode - connected to cloud but device is offline)
    if (isDeviceOffline && isConnected) {
      return {
        icon: <Power className="w-16 h-16 text-theme-muted" />,
        title: "Machine is offline",
        subtitle: "Check that your machine is powered on and connected to the network.",
        showRetry: false,
        showPulse: false,
        isOTA: false,
        isDeviceOffline: true,
      };
    }

    // Normal connection state
    return {
      icon: <Wifi className="w-16 h-16 text-accent" />,
      title: "Connecting to your machine...",
      subtitle: "Please wait while we establish a connection",
      showRetry: !isRetryingOrConnecting,
      showPulse: true,
      isOTA: false,
      isDeviceOffline: false,
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

        {/* OTA Animation - pulsing dots */}
        {status.isOTA && (
          <div className="flex items-center justify-center gap-2 py-4">
            <div className="w-3 h-3 rounded-full bg-accent animate-bounce" style={{ animationDelay: "0ms" }} />
            <div className="w-3 h-3 rounded-full bg-accent animate-bounce" style={{ animationDelay: "150ms" }} />
            <div className="w-3 h-3 rounded-full bg-accent animate-bounce" style={{ animationDelay: "300ms" }} />
            <div className="w-3 h-3 rounded-full bg-accent animate-bounce" style={{ animationDelay: "450ms" }} />
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

        {/* Connection attempts indicator (not during OTA - has its own animation) */}
        {status.showPulse && !status.isOTA && (
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
