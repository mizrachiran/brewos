import { useState, useEffect, useCallback } from "react";
import { Shield, ExternalLink, RefreshCw } from "lucide-react";
import { getStoredSession } from "../lib/api";

interface LoginProps {
  onLogin: () => void;
}

export function Login({ onLogin }: LoginProps) {
  const [checking, setChecking] = useState(false);
  const [opened, setOpened] = useState(false);

  // Check for session periodically after opening main app
  // Main app stores session under "brewos_session"
  const checkForSession = useCallback(() => {
    const session = getStoredSession();
    if (session?.accessToken) {
      onLogin();
    }
  }, [onLogin]);

  useEffect(() => {
    if (!opened) return;

    // Poll for session every second after user clicked login
    const interval = setInterval(checkForSession, 1000);
    return () => clearInterval(interval);
  }, [opened, checkForSession]);

  // Also check on window focus (user might have logged in and switched back)
  useEffect(() => {
    function handleFocus() {
      checkForSession();
    }
    window.addEventListener("focus", handleFocus);
    return () => window.removeEventListener("focus", handleFocus);
  }, [checkForSession]);

  function handleLogin() {
    setOpened(true);
    // Open main app in new tab for login
    window.open("/", "_blank");
  }

  function handleCheckNow() {
    setChecking(true);
    checkForSession();
    setTimeout(() => setChecking(false), 500);
  }

  return (
    <div className="min-h-screen bg-admin-bg flex items-center justify-center p-4">
      <div className="admin-card max-w-md w-full text-center animate-fade-in">
        <div className="w-16 h-16 rounded-2xl bg-admin-accent/20 flex items-center justify-center mx-auto mb-6">
          <Shield className="w-8 h-8 text-admin-accent" />
        </div>

        <h1 className="text-2xl font-display font-bold text-admin-text mb-2">
          BrewOS Admin
        </h1>
        <p className="text-admin-text-secondary mb-8">
          Sign in with your admin account to access the control panel.
        </p>

        {!opened ? (
          <button
            onClick={handleLogin}
            className="btn btn-primary w-full flex items-center justify-center gap-2"
          >
            <svg className="w-5 h-5" viewBox="0 0 24 24">
              <path
                fill="currentColor"
                d="M22.56 12.25c0-.78-.07-1.53-.2-2.25H12v4.26h5.92c-.26 1.37-1.04 2.53-2.21 3.31v2.77h3.57c2.08-1.92 3.28-4.74 3.28-8.09z"
              />
              <path
                fill="currentColor"
                d="M12 23c2.97 0 5.46-.98 7.28-2.66l-3.57-2.77c-.98.66-2.23 1.06-3.71 1.06-2.86 0-5.29-1.93-6.16-4.53H2.18v2.84C3.99 20.53 7.7 23 12 23z"
              />
              <path
                fill="currentColor"
                d="M5.84 14.09c-.22-.66-.35-1.36-.35-2.09s.13-1.43.35-2.09V7.07H2.18C1.43 8.55 1 10.22 1 12s.43 3.45 1.18 4.93l2.85-2.22.81-.62z"
              />
              <path
                fill="currentColor"
                d="M12 5.38c1.62 0 3.06.56 4.21 1.64l3.15-3.15C17.45 2.09 14.97 1 12 1 7.7 1 3.99 3.47 2.18 7.07l3.66 2.84c.87-2.6 3.3-4.53 6.16-4.53z"
              />
            </svg>
            Sign in with Google
            <ExternalLink className="w-4 h-4 ml-1" />
          </button>
        ) : (
          <div className="space-y-4">
            <div className="bg-admin-surface rounded-lg p-4 text-left">
              <p className="text-admin-text text-sm mb-2">
                <strong>1.</strong> Sign in with Google in the new tab
              </p>
              <p className="text-admin-text text-sm mb-2">
                <strong>2.</strong> After signing in, come back to this tab
              </p>
              <p className="text-admin-text-secondary text-xs">
                This page will automatically detect when you're logged in.
              </p>
            </div>

            <button
              onClick={handleCheckNow}
              disabled={checking}
              className="btn btn-secondary w-full flex items-center justify-center gap-2"
            >
              <RefreshCw className={`w-4 h-4 ${checking ? "animate-spin" : ""}`} />
              {checking ? "Checking..." : "Check login status"}
            </button>

            <button
              onClick={() => setOpened(false)}
              className="text-admin-text-secondary text-sm hover:text-admin-text"
            >
              ← Back
            </button>
          </div>
        )}

        <p className="text-xs text-admin-text-secondary mt-6">
          Only users with admin privileges can access this panel.
        </p>
      </div>
    </div>
  );
}
