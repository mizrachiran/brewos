import { useStore } from "@/lib/store";
import { getActiveConnection } from "@/lib/connection";
import { Wifi, RefreshCw, X, Download, AlertCircle, Power } from "lucide-react";
import { Button } from "./Button";
import { useState, useEffect, useRef } from "react";

// Allow bypassing overlay in development for easier testing
const DEV_MODE = import.meta.env.DEV;
const DEV_BYPASS_KEY = "brewos-dev-bypass-overlay";

// Key to track OTA in progress across page reloads (shared with store.ts)
const OTA_IN_PROGRESS_KEY = "brewos-ota-in-progress";

// Grace period before showing offline state
// During this time, we show "Connecting..." to give the device time to respond
const OFFLINE_GRACE_PERIOD_MS = 5000;

// Delay before hiding overlay after successful connection
// This ensures we don't flicker if connection drops again quickly
const HIDE_DELAY_MS = 2000;

// Overlay display states
type OverlayState = "hidden" | "connecting" | "offline" | "updating";

export function ConnectionOverlay() {
  const connectionState = useStore((s) => s.connectionState);
  const machineState = useStore((s) => s.machine.state);
  const ota = useStore((s) => s.ota);
  const [retrying, setRetrying] = useState(false);

  // Dev bypass preference
  const [devBypassed, setDevBypassed] = useState(() => {
    if (!DEV_MODE) return false;
    return localStorage.getItem(DEV_BYPASS_KEY) === "true";
  });

  // Core state values
  const isConnected = connectionState === "connected";
  const isDeviceOffline = machineState === "offline";

  // Check localStorage for OTA state to persist across disconnects
  const storedOtaInProgress =
    localStorage.getItem(OTA_IN_PROGRESS_KEY) === "true";

  const isUpdating =
    ota.isUpdating ||
    (storedOtaInProgress &&
      (!isConnected || isDeviceOffline || machineState === "unknown"));

  // Simple state tracking
  const [overlayState, setOverlayState] = useState<OverlayState>("connecting");
  
  // Track when we first detected offline (for grace period)
  const offlineStartTime = useRef<number | null>(null);
  // Track if we've confirmed offline (sticky - don't go back to connecting)
  const offlineConfirmed = useRef(false);
  // Timer for delayed hide
  const hideTimer = useRef<ReturnType<typeof setTimeout> | null>(null);
  // Timer for grace period check
  const graceTimer = useRef<ReturnType<typeof setTimeout> | null>(null);

  // Track OTA state in localStorage
  useEffect(() => {
    if (ota.isUpdating) {
      localStorage.setItem(OTA_IN_PROGRESS_KEY, "true");
    }
  }, [ota.isUpdating]);

  // Clear OTA state on error or when device is confirmed back online
  useEffect(() => {
    const isError = ota.stage === "error";
    const isBackOnline =
      machineState !== "offline" &&
      machineState !== "unknown" &&
      isConnected &&
      !ota.isUpdating &&
      ota.stage === "idle";

    if (isError || isBackOnline) {
      localStorage.removeItem(OTA_IN_PROGRESS_KEY);
    }
  }, [ota.stage, ota.isUpdating, machineState, isConnected]);

  // Main state machine - simplified
  useEffect(() => {
    // Clear any pending timers
    if (hideTimer.current) {
      clearTimeout(hideTimer.current);
      hideTimer.current = null;
    }
    if (graceTimer.current) {
      clearTimeout(graceTimer.current);
      graceTimer.current = null;
    }

    // OTA always takes priority
    if (isUpdating) {
      setOverlayState("updating");
      return;
    }

    // Device is offline
    if (isDeviceOffline) {
      // Start tracking offline time if not already
      if (offlineStartTime.current === null) {
        offlineStartTime.current = Date.now();
      }

      const elapsed = Date.now() - offlineStartTime.current;

      if (elapsed >= OFFLINE_GRACE_PERIOD_MS || offlineConfirmed.current) {
        // Grace period passed - show offline and mark as confirmed
        offlineConfirmed.current = true;
        setOverlayState("offline");
      } else {
        // Still in grace period - show connecting
        setOverlayState("connecting");
        // Schedule check after grace period ends
        const remaining = OFFLINE_GRACE_PERIOD_MS - elapsed;
        graceTimer.current = setTimeout(() => {
          // Re-check - if still offline, transition to offline state
          const currentState = useStore.getState().machine.state;
          if (currentState === "offline") {
            offlineConfirmed.current = true;
            setOverlayState("offline");
          }
        }, remaining + 100);
      }
      return;
    }

    // Not connected to cloud - show connecting (or offline if previously confirmed)
    if (!isConnected) {
      // If we confirmed offline before, stay offline
      if (offlineConfirmed.current) {
        setOverlayState("offline");
      } else {
        setOverlayState("connecting");
      }
      return;
    }

    // Connected and device is not offline - can hide overlay
    // Reset offline tracking
    offlineStartTime.current = null;
    offlineConfirmed.current = false;

    // Delay hiding to prevent flicker on brief disconnects
    if (overlayState !== "hidden") {
      hideTimer.current = setTimeout(() => {
        // Double-check we're still in good state before hiding
        const state = useStore.getState();
        const stillConnected = state.connectionState === "connected";
        const stillOnline = state.machine.state !== "offline";
        if (stillConnected && stillOnline) {
          setOverlayState("hidden");
        }
      }, HIDE_DELAY_MS);
    }

    return () => {
      if (hideTimer.current) clearTimeout(hideTimer.current);
      if (graceTimer.current) clearTimeout(graceTimer.current);
    };
  }, [isConnected, isDeviceOffline, isUpdating, overlayState]);

  // Lock body scroll when overlay is visible
  const isVisible = overlayState !== "hidden";

  useEffect(() => {
    if (isVisible) {
      const scrollY = window.scrollY;
      document.body.style.position = "fixed";
      document.body.style.top = `-${scrollY}px`;
      document.body.style.left = "0";
      document.body.style.right = "0";
      document.body.style.overflow = "hidden";
      document.body.style.width = "100%";

      const preventScroll = (e: TouchEvent) => {
        e.preventDefault();
      };
      document.addEventListener("touchmove", preventScroll, { passive: false });

      return () => {
        document.body.style.position = "";
        document.body.style.top = "";
        document.body.style.left = "";
        document.body.style.right = "";
        document.body.style.overflow = "";
        document.body.style.width = "";
        document.removeEventListener("touchmove", preventScroll);
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
  if (DEV_MODE && devBypassed && !isDeviceOffline && !isUpdating) return null;

  const handleRetry = async () => {
    setRetrying(true);
    try {
      await getActiveConnection()?.connect();
    } catch (e) {
      console.error("Retry failed:", e);
    }
    // Keep retrying state for a bit to prevent button flicker
    setTimeout(() => setRetrying(false), 2000);
  };

  // Build status based on overlay state
  const getStatus = () => {
    // OTA in progress
    if (overlayState === "updating" || isUpdating) {
      const otaMessage =
        ota.message ||
        "Please wait while the update is being installed. The device will restart automatically.";
      return {
        icon: <Download className="w-16 h-16 text-accent" />,
        title: "Updating BrewOS...",
        subtitle: otaMessage,
        showRetry: false,
        showPulse: true,
        isOTA: true,
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
      };
    }

    // Device offline
    if (overlayState === "offline") {
      return {
        icon: <Power className="w-16 h-16 text-theme-muted" />,
        title: "Machine is offline",
        subtitle:
          "Check that your machine is powered on and connected to the network.",
        showRetry: false,
        showPulse: false,
        isOTA: false,
      };
    }

    // Connecting state - never show retry button (it causes flicker)
    return {
      icon: <Wifi className="w-16 h-16 text-accent" />,
      title: "Connecting to your machine...",
      subtitle: "Please wait while we establish a connection",
      showRetry: false, // Removed retry button to prevent flicker
      showPulse: true,
      isOTA: false,
    };
  };

  const status = getStatus();

  return (
    <div className="fixed inset-0 z-[40] flex items-center justify-center p-6 bg-theme/95 backdrop-blur-md overflow-hidden touch-none overscroll-none">
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
            <div
              className="w-3 h-3 rounded-full bg-accent animate-bounce"
              style={{ animationDelay: "0ms" }}
            />
            <div
              className="w-3 h-3 rounded-full bg-accent animate-bounce"
              style={{ animationDelay: "150ms" }}
            />
            <div
              className="w-3 h-3 rounded-full bg-accent animate-bounce"
              style={{ animationDelay: "300ms" }}
            />
            <div
              className="w-3 h-3 rounded-full bg-accent animate-bounce"
              style={{ animationDelay: "450ms" }}
            />
          </div>
        )}

        {/* Retry Button - only in offline state */}
        {overlayState === "offline" && (
          <Button
            onClick={handleRetry}
            loading={retrying}
            variant="secondary"
            size="lg"
            className="min-w-40"
          >
            <RefreshCw className="w-5 h-5" />
            Try Again
          </Button>
        )}

        {/* Connection animation (not during OTA) */}
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
