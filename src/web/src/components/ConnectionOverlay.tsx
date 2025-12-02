import { useStore } from "@/lib/store";
import { getConnection } from "@/lib/connection";
import { cn } from "@/lib/utils";
import { Wifi, WifiOff, RefreshCw, AlertTriangle, X } from "lucide-react";
import { Button } from "./Button";
import { useState, useEffect } from "react";

// Allow bypassing overlay in development for easier testing
const DEV_MODE = import.meta.env.DEV;
const DEV_BYPASS_KEY = "brewos-dev-bypass-overlay";

export function ConnectionOverlay() {
  const connectionState = useStore((s) => s.connectionState);
  const [retrying, setRetrying] = useState(false);

  // Check localStorage for dev bypass preference
  const [devBypassed, setDevBypassed] = useState(() => {
    if (!DEV_MODE) return false;
    return localStorage.getItem(DEV_BYPASS_KEY) === "true";
  });

  const isConnected = connectionState === "connected";
  const isConnecting =
    connectionState === "connecting" || connectionState === "reconnecting";
  const isError = connectionState === "error";

  // Determine if overlay should be visible - show immediately when not connected
  const isOverlayVisible = !isConnected && !(DEV_MODE && devBypassed);

  // Lock body scroll when overlay is visible
  useEffect(() => {
    if (isOverlayVisible) {
      // Save current scroll position and lock
      const scrollY = window.scrollY;
      document.body.style.position = "fixed";
      document.body.style.top = `-${scrollY}px`;
      document.body.style.left = "0";
      document.body.style.right = "0";
      document.body.style.overflow = "hidden";

      return () => {
        // Restore scroll position
        document.body.style.position = "";
        document.body.style.top = "";
        document.body.style.left = "";
        document.body.style.right = "";
        document.body.style.overflow = "";
        window.scrollTo(0, scrollY);
      };
    }
  }, [isOverlayVisible]);

  const handleDevBypass = () => {
    localStorage.setItem(DEV_BYPASS_KEY, "true");
    setDevBypassed(true);
  };

  // Don't render if not visible
  if (!isOverlayVisible) return null;

  const handleRetry = async () => {
    setRetrying(true);
    try {
      await getConnection()?.connect();
    } catch (e) {
      console.error("Retry failed:", e);
    }
    setRetrying(false);
  };

  const getStatusContent = () => {
    if (isConnecting || retrying) {
      return {
        icon: <Wifi className="w-16 h-16 text-accent animate-pulse" />,
        title: "Connecting to your machine...",
        subtitle: "Please wait while we establish a connection",
        showRetry: false,
      };
    }

    if (isError) {
      return {
        icon: <AlertTriangle className="w-16 h-16 text-amber-500" />,
        title: "Connection Error",
        subtitle:
          "Unable to connect to your espresso machine. Please check that your device is powered on and connected to the same network.",
        showRetry: true,
      };
    }

    // Disconnected
    return {
      icon: <WifiOff className="w-16 h-16 text-red-400" />,
      title: "Disconnected",
      subtitle:
        "Lost connection to your espresso machine. Attempting to reconnect automatically...",
      showRetry: true,
    };
  };

  const status = getStatusContent();

  return (
    <div
      className={cn(
        "fixed inset-0 z-[60] flex items-center justify-center p-6",
        "bg-theme/95 backdrop-blur-md",
        "animate-in fade-in duration-300"
      )}
    >
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
          {(isConnecting || retrying) && (
            <>
              <div className="absolute inset-0 w-24 h-24 -m-4 rounded-full bg-accent/20 animate-ping" />
              <div className="absolute inset-0 w-20 h-20 -m-2 rounded-full bg-accent/30 animate-pulse" />
            </>
          )}
          {status.icon}
        </div>

        {/* Status Text */}
        <div className="space-y-2">
          <h2 className="text-2xl font-bold text-theme">{status.title}</h2>
          <p className="text-theme-muted leading-relaxed">{status.subtitle}</p>
        </div>

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
        {(isConnecting || retrying) && (
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
