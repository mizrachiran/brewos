import { useEffect, useState } from "react";
import { Routes, Route, Navigate } from "react-router-dom";
import { Layout } from "@/components/Layout";
import { Dashboard } from "@/pages/Dashboard";
import { Brewing } from "@/pages/Brewing";
import { Stats } from "@/pages/Stats";
import { Settings } from "@/pages/settings";
import { Schedules } from "@/pages/Schedules";
import { Diagnostics } from "@/pages/Diagnostics";
import { Setup } from "@/pages/Setup";
import { Login } from "@/pages/Login";
import { Machines } from "@/pages/Machines";
import { AuthCallback } from "@/pages/AuthCallback";
import { Pair } from "@/pages/Pair";
import { Onboarding } from "@/pages/Onboarding";
import { FirstRunWizard } from "@/pages/FirstRunWizard";
import { BrewingPreview } from "@/pages/dev/BrewingPreview";
import {
  initConnection,
  getConnection,
  setActiveConnection,
} from "@/lib/connection";
import { initializeStore } from "@/lib/store";
import { useAppStore } from "@/lib/mode";
import { useThemeStore } from "@/lib/themeStore";
import { Loading } from "@/components/Loading";
import { DemoBanner } from "@/components/DemoBanner";
import { UpdateNotification } from "@/components/UpdateNotification";
import { FirmwareUpdateNotification } from "@/components/FirmwareUpdateNotification";
import { AppBadgeManager } from "@/components/AppBadgeManager";
import { getDemoConnection, clearDemoConnection } from "@/lib/demo-connection";
import { isDemoMode, disableDemoMode } from "@/lib/demo-mode";
import { isRunningAsPWA, isDemoModeAllowed } from "@/lib/pwa";
import { isDevModeEnabled } from "@/lib/dev-mode";

// Maximum time to wait for initialization before showing error/fallback
const INIT_TIMEOUT_MS = 10000;

function App() {
  const [loading, setLoading] = useState(true);
  const [setupComplete, setSetupComplete] = useState(true); // Default true to avoid flash
  const [initError, setInitError] = useState<string | null>(null);

  // Check if running as PWA - PWA mode only supports cloud, not demo/local
  const [isPWA] = useState(() => isRunningAsPWA());

  // Demo mode is only allowed for cloud website visitors in browser
  // NOT allowed when: running as PWA, or running on ESP32 local mode
  const [inDemoMode] = useState(() => {
    if (!isDemoModeAllowed()) {
      // Clear demo mode if it was somehow enabled in a context where it's not allowed
      disableDemoMode();
      return false;
    }
    return isDemoMode();
  });

  const {
    mode,
    apMode,
    initialized,
    user,
    devices,
    devicesLoading,
    initialize,
    getSelectedDevice,
  } = useAppStore();

  // Track if we've completed the initial device fetch (for returning users)
  const [initialDevicesFetched, setInitialDevicesFetched] = useState(false);

  const initTheme = useThemeStore((s) => s.initTheme);

  // Safety timeout to prevent forever loading
  useEffect(() => {
    const timeoutId = setTimeout(() => {
      if (loading || (mode === "cloud" && user && !initialDevicesFetched)) {
        console.error("[App] Initialization timeout - forcing load complete");
        setLoading(false);
        setInitialDevicesFetched(true); // Prevent infinite loading on device fetch timeout
        // If we're still not initialized after timeout, set error for cloud mode
        if (!initialized && !inDemoMode) {
          setInitError(
            "Connection timeout. Please check your network and try again."
          );
        }
      }
    }, INIT_TIMEOUT_MS);

    return () => clearTimeout(timeoutId);
  }, [loading, initialized, inDemoMode, mode, user, initialDevicesFetched]);

  // Initialize demo mode
  useEffect(() => {
    if (!inDemoMode) return;

    const initDemo = async () => {
      try {
        initTheme();

        const demoConnection = getDemoConnection();

        // Set as active connection so useCommand works
        setActiveConnection(demoConnection);

        initializeStore(demoConnection);

        await demoConnection.connect();
      } catch (error) {
        console.error("[App] Demo initialization error:", error);
        setInitError(
          "Failed to initialize demo mode. Please refresh the page."
        );
      } finally {
        setLoading(false);
      }
    };

    initDemo();

    return () => {
      setActiveConnection(null);
      clearDemoConnection();
    };
  }, [inDemoMode, initTheme]);

  // Initialize normal mode
  useEffect(() => {
    if (inDemoMode) return;

    const init = async () => {
      try {
        // Initialize theme first for immediate visual consistency
        initTheme();

        // Initialize app - this fetches mode from server
        await initialize();
      } catch (error) {
        console.error("[App] Initialization error:", error);
        // Don't set error here - we'll rely on timeout for edge cases
      }
    };

    init();
  }, [initialize, initTheme, inDemoMode]);

  // Setup local mode connection after initialization
  useEffect(() => {
    if (inDemoMode) return; // Skip if in demo mode
    if (!initialized) return;

    // IMPORTANT: Always set loading to false, even if setup fails
    // Use an AbortController to timeout the setup request
    const abortController = new AbortController();
    const timeoutId = setTimeout(() => abortController.abort(), 5000); // 5 second timeout

    const setupLocalMode = async () => {
      try {
        // In cloud mode, setupComplete is not applicable - always treat as complete
        // (FirstRunWizard is only for local ESP32 device setup)
        // Also, don't initialize WebSocket in cloud mode - it will be initialized
        // when needed (e.g., when viewing a machine dashboard)
        if (mode === "cloud") {
          setSetupComplete(true);
          setLoading(false);
          // Disconnect any existing connection (in case mode changed)
          getConnection()?.disconnect();
          return;
        }

        // IMPORTANT: If setup is not complete, we're in wizard mode
        // Stay in local mode and check setup status - don't allow mode changes
        // until wizard completes
        if (!setupComplete && !apMode && !isPWA) {
          // Check if setup is complete (with timeout via AbortController)
          try {
            const setupResponse = await fetch("/api/setup/status", {
              signal: abortController.signal,
            });
            if (setupResponse.ok) {
              const setupData = await setupResponse.json();
              setSetupComplete(setupData.complete);
              // If setup is now complete, continue with normal flow
              if (setupData.complete) {
                setLoading(false);
                return;
              }
            }
          } catch (error) {
            // If endpoint doesn't exist, times out, or aborts, keep checking
            if ((error as Error).name !== "AbortError") {
              console.warn("[App] Setup status check failed:", error);
            }
            // Don't set setupComplete to true here - let wizard handle it
          }

          // Initialize WebSocket connection for wizard
          const connection = initConnection({
            mode: "local",
            endpoint: "/ws",
          });

          initializeStore(connection);

          connection.connect().catch((error) => {
            console.error("Initial connection failed:", error);
          });

          setLoading(false);
          return;
        }

        // Local mode is only allowed when NOT running as PWA
        if (mode === "local" && !apMode && !isPWA) {
          // Check if setup is complete (with timeout via AbortController)
          try {
            const setupResponse = await fetch("/api/setup/status", {
              signal: abortController.signal,
            });
            if (setupResponse.ok) {
              const setupData = await setupResponse.json();
              setSetupComplete(setupData.complete);
            }
          } catch (error) {
            // If endpoint doesn't exist, times out, or aborts, assume setup is complete
            if ((error as Error).name !== "AbortError") {
              console.warn("[App] Setup status check failed:", error);
            }
            setSetupComplete(true);
          }

          // Initialize WebSocket connection
          const connection = initConnection({
            mode: "local",
            endpoint: "/ws",
          });

          initializeStore(connection);

          connection.connect().catch((error) => {
            console.error("Initial connection failed:", error);
          });
        }
      } finally {
        // ALWAYS clear timeout and set loading to false
        clearTimeout(timeoutId);
        setLoading(false);
      }
    };

    setupLocalMode();

    return () => {
      abortController.abort(); // Cancel any pending requests on cleanup
      getConnection()?.disconnect();
    };
  }, [initialized, mode, apMode, inDemoMode, isPWA, setupComplete]);

  // Track when initial device fetch completes (for cloud mode with existing user)
  useEffect(() => {
    if (mode !== "cloud" || !user || initialDevicesFetched) return;

    // Once devicesLoading becomes false after initialization, mark as fetched
    if (initialized && !devicesLoading) {
      setInitialDevicesFetched(true);
    }
  }, [mode, user, initialized, devicesLoading, initialDevicesFetched]);

  // Reset initialDevicesFetched when user logs out (so re-login waits for devices)
  useEffect(() => {
    if (!user && initialDevicesFetched) {
      setInitialDevicesFetched(false);
    }
  }, [user, initialDevicesFetched]);

  // Handle setup completion
  const handleSetupComplete = () => {
    setSetupComplete(true);
  };

  // Handle demo exit
  const handleExitDemo = () => {
    // Navigate to login page - the login page will clear demo mode
    // Don't clear here to avoid connection errors during navigation
    window.location.href = "/login?exitDemo=true";
  };

  // Show loading state
  // For cloud mode with existing user, also wait for initial device fetch
  // Skip device waiting in demo mode - demo doesn't fetch real devices
  const isWaitingForDevices =
    !inDemoMode && mode === "cloud" && user && !initialDevicesFetched;

  if (loading || (!inDemoMode && !initialized) || isWaitingForDevices) {
    return <Loading message={initError || undefined} />;
  }

  // Show error state if initialization failed
  if (initError) {
    return (
      <Loading
        message={initError}
        showRetry
        onRetry={() => window.location.reload()}
      />
    );
  }

  // ===== DEMO MODE =====
  if (inDemoMode) {
    const isDev = isDevModeEnabled();
    return (
      <>
        <DemoBanner onExit={handleExitDemo} />
        <Routes>
          {/* Dev preview routes */}
          {isDev && (
            <>
              <Route path="/dev/login" element={<Login />} />
              <Route path="/dev/onboarding" element={<Onboarding />} />
              <Route path="/dev/pair" element={<Pair />} />
              <Route path="/dev/brewing" element={<BrewingPreview />} />
              <Route
                path="/dev/wizard"
                element={
                  <FirstRunWizard
                    onComplete={() => (window.location.href = "/")}
                  />
                }
              />
            </>
          )}
          <Route path="/machines" element={<Machines />} />
          <Route path="/" element={<Layout onExitDemo={handleExitDemo} />}>
            <Route index element={<Dashboard />} />
            <Route path="brewing" element={<Brewing />} />
            <Route path="stats" element={<Stats />} />
            <Route path="schedules" element={<Schedules />} />
            <Route path="settings" element={<Settings />} />
            <Route path="diagnostics" element={<Diagnostics />} />
            <Route path="*" element={<Navigate to="/" replace />} />
          </Route>
        </Routes>
        <UpdateNotification />
        <FirmwareUpdateNotification />
      </>
    );
  }

  // ===== DEV MODE ROUTES =====
  // These are available in any mode for testing welcome screens
  // Enable via ?dev=true or localStorage
  const isDev = isDevModeEnabled();

  // ===== LOCAL MODE (ESP32) =====
  // IMPORTANT: If setup is not complete, we MUST stay in local mode
  // even if mode detection changes. The wizard must complete first.
  const effectiveMode = !setupComplete ? "local" : mode;

  if (effectiveMode === "local") {
    // Show WiFi setup page in AP mode
    if (apMode) {
      return <Setup />;
    }

    // Show first-run wizard if setup not complete
    // This takes precedence over everything - wizard must complete first
    if (!setupComplete) {
      return <FirstRunWizard onComplete={handleSetupComplete} />;
    }

    return (
      <>
        <Routes>
          {/* Dev preview routes for testing welcome screens */}
          {isDev && (
            <>
              <Route path="/dev/login" element={<Login />} />
              <Route path="/dev/onboarding" element={<Onboarding />} />
              <Route path="/dev/pair" element={<Pair />} />
              <Route path="/dev/brewing" element={<BrewingPreview />} />
              <Route
                path="/dev/wizard"
                element={
                  <FirstRunWizard
                    onComplete={() => (window.location.href = "/")}
                  />
                }
              />
            </>
          )}

          <Route path="/" element={<Layout />}>
            <Route index element={<Dashboard />} />
            <Route path="brewing" element={<Brewing />} />
            <Route path="stats" element={<Stats />} />
            <Route path="schedules" element={<Schedules />} />
            <Route path="settings" element={<Settings />} />
            <Route path="diagnostics" element={<Diagnostics />} />
            <Route path="setup" element={<Setup />} />
            <Route path="*" element={<Navigate to="/" replace />} />
          </Route>
        </Routes>
        <UpdateNotification />
        <FirmwareUpdateNotification />
      </>
    );
  }

  // ===== CLOUD MODE =====

  // Not logged in -> Login
  if (!user) {
    return (
      <>
        <Routes>
          {/* Dev preview routes for testing welcome screens */}
          {isDev && (
            <>
              <Route path="/dev/login" element={<Login />} />
              <Route path="/dev/onboarding" element={<Onboarding />} />
              <Route path="/dev/pair" element={<Pair />} />
              <Route path="/dev/brewing" element={<BrewingPreview />} />
              <Route
                path="/dev/wizard"
                element={
                  <FirstRunWizard
                    onComplete={() => (window.location.href = "/")}
                  />
                }
              />
            </>
          )}
          <Route path="/login" element={<Login />} />
          <Route path="/auth/callback" element={<AuthCallback />} />
          <Route path="/pair" element={<Pair />} />
          <Route path="*" element={<Navigate to="/login" replace />} />
        </Routes>
        <UpdateNotification />
      </>
    );
  }

  // Logged in but no devices -> Onboarding
  if (devices.length === 0) {
    return (
      <>
        <Routes>
          {/* Dev preview routes for testing welcome screens */}
          {isDev && (
            <>
              <Route path="/dev/login" element={<Login />} />
              <Route path="/dev/onboarding" element={<Onboarding />} />
              <Route path="/dev/pair" element={<Pair />} />
              <Route path="/dev/brewing" element={<BrewingPreview />} />
              <Route
                path="/dev/wizard"
                element={
                  <FirstRunWizard
                    onComplete={() => (window.location.href = "/")}
                  />
                }
              />
            </>
          )}
          <Route path="/onboarding" element={<Onboarding />} />
          <Route path="/auth/callback" element={<AuthCallback />} />
          <Route path="/pair" element={<Pair />} />
          <Route path="*" element={<Navigate to="/onboarding" replace />} />
        </Routes>
        <UpdateNotification />
        <FirmwareUpdateNotification />
      </>
    );
  }

  // Logged in with devices -> Full app
  const selectedDevice = getSelectedDevice();

  return (
    <>
      <Routes>
        {/* Dev preview routes for testing welcome screens */}
        {isDev && (
          <>
            <Route path="/dev/login" element={<Login />} />
            <Route path="/dev/onboarding" element={<Onboarding />} />
            <Route path="/dev/pair" element={<Pair />} />
            <Route path="/dev/brewing" element={<BrewingPreview />} />
            <Route
              path="/dev/wizard"
              element={
                <FirstRunWizard
                  onComplete={() => (window.location.href = "/")}
                />
              }
            />
          </>
        )}

        {/* Auth routes */}
        <Route path="/auth/callback" element={<AuthCallback />} />
        <Route path="/pair" element={<Pair />} />

        {/* Machine management */}
        <Route path="/machines" element={<Machines />} />
        <Route path="/login" element={<Navigate to="/machines" replace />} />

        {/* Machine control (when connected via cloud) */}
        <Route path="/machine/:deviceId" element={<Layout />}>
          <Route index element={<Dashboard />} />
          <Route path="brewing" element={<Brewing />} />
          <Route path="stats" element={<Stats />} />
          <Route path="schedules" element={<Schedules />} />
          <Route path="settings" element={<Settings />} />
          <Route path="diagnostics" element={<Diagnostics />} />
        </Route>

        {/* Root: redirect to selected machine or machines list */}
        <Route
          path="/"
          element={
            <Navigate
              to={
                selectedDevice ? `/machine/${selectedDevice.id}` : "/machines"
              }
              replace
            />
          }
        />

        {/* Default: redirect to machines */}
        <Route path="*" element={<Navigate to="/machines" replace />} />
      </Routes>
      <UpdateNotification />
      <FirmwareUpdateNotification />
      <AppBadgeManager />
    </>
  );
}

export default App;
