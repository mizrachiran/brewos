import { useEffect, useState } from "react";
import { Routes, Route, Navigate } from "react-router-dom";
import { Layout } from "@/components/Layout";
import { Dashboard } from "@/pages/Dashboard";
import { Brewing } from "@/pages/Brewing";
import { Stats } from "@/pages/Stats";
import { Settings } from "@/pages/settings";
import { Schedules } from "@/pages/Schedules";
import { Setup } from "@/pages/Setup";
import { Login } from "@/pages/Login";
import { Machines } from "@/pages/Machines";
import { AuthCallback } from "@/pages/AuthCallback";
import { Pair } from "@/pages/Pair";
import { Onboarding } from "@/pages/Onboarding";
import { FirstRunWizard } from "@/pages/FirstRunWizard";
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
import { getDemoConnection, clearDemoConnection } from "@/lib/demo-connection";
import { isDemoMode, disableDemoMode } from "@/lib/demo-mode";
import { isRunningAsPWA, isDemoModeAllowed } from "@/lib/pwa";

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
    initialize,
    getSelectedDevice,
  } = useAppStore();

  const initTheme = useThemeStore((s) => s.initTheme);

  // Safety timeout to prevent forever loading
  useEffect(() => {
    const timeoutId = setTimeout(() => {
      if (loading) {
        console.error('[App] Initialization timeout - forcing load complete');
        setLoading(false);
        // If we're still not initialized after timeout, set error for cloud mode
        if (!initialized && !inDemoMode) {
          setInitError('Connection timeout. Please check your network and try again.');
        }
      }
    }, INIT_TIMEOUT_MS);

    return () => clearTimeout(timeoutId);
  }, [loading, initialized, inDemoMode]);

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
        console.error('[App] Demo initialization error:', error);
        setInitError('Failed to initialize demo mode. Please refresh the page.');
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
        console.error('[App] Initialization error:', error);
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
            if ((error as Error).name !== 'AbortError') {
              console.warn('[App] Setup status check failed:', error);
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
  }, [initialized, mode, apMode, inDemoMode]);

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
  if (loading || (!inDemoMode && !initialized)) {
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
    return (
      <>
        <DemoBanner onExit={handleExitDemo} />
        <Routes>
          <Route path="/machines" element={<Machines />} />
          <Route path="/" element={<Layout onExitDemo={handleExitDemo} />}>
            <Route index element={<Dashboard />} />
            <Route path="brewing" element={<Brewing />} />
            <Route path="stats" element={<Stats />} />
            <Route path="schedules" element={<Schedules />} />
            <Route path="settings" element={<Settings />} />
            <Route path="*" element={<Navigate to="/" replace />} />
          </Route>
        </Routes>
        <UpdateNotification />
      </>
    );
  }

  // ===== LOCAL MODE (ESP32) =====
  if (mode === "local") {
    // Show WiFi setup page in AP mode
    if (apMode) {
      return <Setup />;
    }

    // Show first-run wizard if setup not complete
    if (!setupComplete) {
      return <FirstRunWizard onComplete={handleSetupComplete} />;
    }

    return (
      <>
        <Routes>
          <Route path="/" element={<Layout />}>
            <Route index element={<Dashboard />} />
            <Route path="brewing" element={<Brewing />} />
            <Route path="stats" element={<Stats />} />
            <Route path="schedules" element={<Schedules />} />
            <Route path="settings" element={<Settings />} />
            <Route path="setup" element={<Setup />} />
            <Route path="*" element={<Navigate to="/" replace />} />
          </Route>
        </Routes>
        <UpdateNotification />
      </>
    );
  }

  // ===== CLOUD MODE =====

  // Not logged in -> Login
  if (!user) {
    return (
      <>
        <Routes>
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
          <Route path="/onboarding" element={<Onboarding />} />
          <Route path="/auth/callback" element={<AuthCallback />} />
          <Route path="/pair" element={<Pair />} />
          <Route path="*" element={<Navigate to="/onboarding" replace />} />
        </Routes>
        <UpdateNotification />
      </>
    );
  }

  // Logged in with devices -> Full app
  const selectedDevice = getSelectedDevice();

  return (
    <>
      <Routes>
        {/* Auth routes */}
        <Route path="/auth/callback" element={<AuthCallback />} />
        <Route path="/pair" element={<Pair />} />
        <Route path="/onboarding" element={<Onboarding />} />

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
        </Route>

        {/* Root: redirect to selected machine or machines list */}
        <Route
          path="/"
          element={
            <Navigate
              to={selectedDevice ? `/machine/${selectedDevice.id}` : "/machines"}
              replace
            />
          }
        />

        {/* Default: redirect to machines */}
        <Route path="*" element={<Navigate to="/" replace />} />
      </Routes>
      <UpdateNotification />
    </>
  );
}

export default App;
