import { create } from "zustand";
import { persist } from "zustand/middleware";
import type { CloudDevice, ConnectionMode } from "./types";
import {
  getStoredSession,
  loginWithGoogle,
  logout as authLogout,
  getValidAccessToken,
  isTokenExpired,
  refreshSession,
  type User,
  type AuthSession,
} from "./auth";
import { useBackendInfo } from "./backend-info";
import { checkCompatibility, type BackendInfo } from "./api-version";
import { isRunningAsPWA } from "./pwa";

/**
 * Get initial auth state from localStorage synchronously.
 * This ensures user is populated immediately on cold start,
 * preventing flash of login screen when session exists.
 */
function getInitialAuthState(): {
  user: User | null;
  session: AuthSession | null;
} {
  const session = getStoredSession();
  if (session && !isTokenExpired(session)) {
    return { user: session.user, session };
  }
  // Even if token is expired, keep the user info for display
  // The initialize() function will handle token refresh
  if (session) {
    return { user: session.user, session };
  }
  return { user: null, session: null };
}

// Timeout for fetch requests (5 seconds)
const FETCH_TIMEOUT_MS = 5000;

/**
 * Fetch with timeout to prevent hanging on slow/unresponsive networks
 */
async function fetchWithTimeout(
  url: string,
  timeoutMs: number = FETCH_TIMEOUT_MS
): Promise<Response> {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), timeoutMs);

  try {
    const response = await fetch(url, { signal: controller.signal });
    clearTimeout(timeoutId);
    return response;
  } catch (error) {
    clearTimeout(timeoutId);
    throw error;
  }
}

/**
 * Fetch mode from server - the server knows if it's ESP32 (local) or cloud
 * Now uses /api/info as primary endpoint with fallback to /api/mode
 *
 * IMPORTANT: When running as a PWA, always return cloud mode.
 * PWA installations are from cloud.brewos.io and should only support cloud mode.
 */
async function fetchModeFromServer(): Promise<{
  mode: ConnectionMode;
  apMode?: boolean;
  backendInfo?: BackendInfo;
}> {
  // PWA mode only supports cloud - no local mode allowed
  if (isRunningAsPWA()) {
    console.log("[Mode] Running as PWA, forcing cloud mode");
    return { mode: "cloud", apMode: false };
  }

  // Try /api/info first (new endpoint with full capabilities)
  try {
    const infoResponse = await fetchWithTimeout("/api/info");
    if (infoResponse.ok) {
      const info = (await infoResponse.json()) as BackendInfo;
      return {
        mode: info.mode === "cloud" ? "cloud" : "local",
        apMode: (info as { apMode?: boolean }).apMode,
        backendInfo: info,
      };
    }
  } catch {
    // /api/info not available, try fallback
  }

  // Fallback to /api/mode (for backward compatibility with older firmware)
  try {
    const response = await fetchWithTimeout("/api/mode");
    if (response.ok) {
      const data = await response.json();
      return {
        mode: data.mode === "cloud" ? "cloud" : "local",
        apMode: data.apMode,
      };
    }
  } catch {
    // If fetch fails, default to local (ESP32 might be in AP mode with no network)
  }
  return { mode: "local", apMode: false };
}

/**
 * Token refresh monitor
 * Automatically refreshes tokens before they expire
 */
let tokenRefreshInterval: number | null = null;

function startTokenRefreshMonitor(store: AppState) {
  // Clear existing monitor
  if (tokenRefreshInterval !== null) {
    clearInterval(tokenRefreshInterval);
  }

  // Check every 30 seconds
  tokenRefreshInterval = window.setInterval(async () => {
    const session = getStoredSession();

    if (!session) {
      stopTokenRefreshMonitor();
      return;
    }

    // If token is expiring soon, refresh it
    if (isTokenExpired(session)) {
      const newSession = await refreshSession(session);

      if (newSession) {
        // Update store with new session
        store.updateSession(newSession);
      } else {
        // Refresh failed - check if session was cleared or just network issue
        const currentSession = getStoredSession();

        if (!currentSession) {
          // Session was cleared (invalid token) - user needs to re-login
          store.signOut();

          // Redirect to login if not already there
          if (window.location.pathname !== "/login") {
            window.location.href = "/login?expired=true";
          }
        }
      }
    }
  }, 30 * 1000); // Check every 30 seconds
}

function stopTokenRefreshMonitor() {
  if (tokenRefreshInterval !== null) {
    clearInterval(tokenRefreshInterval);
    tokenRefreshInterval = null;
  }
}

interface AppState {
  // Mode
  mode: ConnectionMode;
  apMode: boolean;
  initialized: boolean;

  // Auth (cloud only)
  user: User | null;
  session: AuthSession | null;
  authLoading: boolean;

  // Devices (cloud only)
  devices: CloudDevice[];
  selectedDeviceId: string | null;
  devicesLoading: boolean;

  // Actions
  initialize: () => Promise<void>;
  handleGoogleLogin: (credential: string) => Promise<void>;
  signOut: () => void;
  updateSession: (session: AuthSession) => void;

  // Device management
  fetchDevices: () => Promise<void>;
  selectDevice: (deviceId: string) => void;
  claimDevice: (
    deviceId: string,
    token: string,
    name?: string
  ) => Promise<boolean>;
  removeDevice: (deviceId: string) => Promise<boolean>;
  renameDevice: (deviceId: string, name: string) => Promise<boolean>;

  // Helpers
  getSelectedDevice: () => CloudDevice | null;
  getAccessToken: () => Promise<string | null>;
}

// Get initial auth state synchronously before store creation
const initialAuth = getInitialAuthState();

export const useAppStore = create<AppState>()(
  persist(
    (set, get) => ({
      // Initial state
      // Use build-time constant to set correct default mode
      // ESP32 builds default to "local", cloud builds default to "cloud"
      mode: __ESP32__ ? "local" : "cloud",
      apMode: false,
      initialized: false,
      // Pre-populate user/session from localStorage to prevent flash of login screen
      user: initialAuth.user,
      session: initialAuth.session,
      authLoading: true,
      devices: [],
      selectedDeviceId: null,
      devicesLoading: false,

      initialize: async () => {
        const isPWA = isRunningAsPWA();

        // Check if we're on actual ESP32 hardware
        // Use both build-time constant AND runtime hostname check for safety
        const isOnESP32Hardware =
          __ESP32__ && window.location.hostname === "brewos.local";

        // For ESP32 local mode (non-PWA), initialize immediately with cached state
        // This allows instant UI display. Skip this on cloud builds - always check server.
        if (!isPWA && isOnESP32Hardware && !get().initialized) {
          set({ mode: "local", initialized: true, authLoading: false });
          // Fetch backend info in background (non-blocking) for feature detection
          useBackendInfo.getState().fetchInfo();
          return;
        }

        // Fetch mode from server - always wait for response on localhost/cloud
        const { mode, apMode, backendInfo } = await fetchModeFromServer();

        set({ mode, apMode: apMode ?? false });

        // Update backend info store
        if (backendInfo) {
          const { compatible, warnings, errors } =
            checkCompatibility(backendInfo);
          useBackendInfo.setState({
            info: backendInfo,
            loading: false,
            error: null,
            compatible,
            warnings,
            errors,
          });
        } else {
          useBackendInfo.getState().fetchInfo();
        }

        if (mode === "local") {
          set({ initialized: true, authLoading: false });
          return;
        }

        // Cloud mode: check for stored session
        const session = getStoredSession();

        if (session && !isTokenExpired(session)) {
          // Token still valid - use it
          set({
            user: session.user,
            session,
            authLoading: false,
            initialized: true,
          });

          // Fetch devices
          get().fetchDevices();

          // Start token refresh monitoring
          startTokenRefreshMonitor(get());
        } else if (session) {
          // Session exists but token expired, try to refresh
          const newSession = await refreshSession(session);

          if (newSession) {
            // Refresh succeeded
            set({
              user: newSession.user,
              session: newSession,
              authLoading: false,
              initialized: true,
            });
            get().fetchDevices();
            startTokenRefreshMonitor(get());
          } else {
            // Refresh failed - check if session was cleared or just network issue
            const currentSession = getStoredSession();

            if (currentSession) {
              // Session preserved (network error) - show user as logged in
              // with expired token. They can retry or features will try refresh.
              set({
                user: currentSession.user,
                session: currentSession,
                authLoading: false,
                initialized: true,
              });
              // Try fetching devices anyway - might work if network recovers
              get().fetchDevices();
              startTokenRefreshMonitor(get());
            } else {
              // Session was cleared (invalid token) - user needs to re-login
              set({ authLoading: false, initialized: true });
            }
          }
        } else {
          set({ authLoading: false, initialized: true });
        }
      },

      handleGoogleLogin: async (credential: string) => {
        try {
          set({ authLoading: true });

          const session = await loginWithGoogle(credential);

          set({
            user: session.user,
            session,
            authLoading: false,
          });

          // Fetch devices after login
          get().fetchDevices();

          // Start token refresh monitoring
          startTokenRefreshMonitor(get());
        } catch (error) {
          console.error("[Auth] Login failed:", error);
          set({ authLoading: false });
          throw error;
        }
      },

      signOut: () => {
        authLogout();
        stopTokenRefreshMonitor();
        set({
          user: null,
          session: null,
          devices: [],
          selectedDeviceId: null,
        });
      },

      updateSession: (session: AuthSession) => {
        set({ session, user: session.user });
      },

      fetchDevices: async () => {
        const token = await get().getAccessToken();
        if (!token) return;

        set({ devicesLoading: true });

        try {
          const response = await fetch("/api/devices", {
            headers: {
              Authorization: `Bearer ${token}`,
            },
          });

          if (response.ok) {
            const data = await response.json();
            const devices = data.devices as CloudDevice[];

            set({ devices });

            // Auto-select first device if none selected
            if (!get().selectedDeviceId && devices.length > 0) {
              set({ selectedDeviceId: devices[0].id });
            }
          } else if (response.status === 401) {
            // Token rejected - try refreshing once before signing out
            const session = getStoredSession();

            if (session) {
              const newSession = await refreshSession(session);

              if (newSession) {
                // Retry the request with fresh token
                const retryResponse = await fetch("/api/devices", {
                  headers: {
                    Authorization: `Bearer ${newSession.accessToken}`,
                  },
                });

                if (retryResponse.ok) {
                  const data = await retryResponse.json();
                  const devices = data.devices as CloudDevice[];
                  set({ devices });

                  if (!get().selectedDeviceId && devices.length > 0) {
                    set({ selectedDeviceId: devices[0].id });
                  }

                  // Update session in store
                  get().updateSession(newSession);
                  set({ devicesLoading: false });
                  return;
                }
              }
            }

            // Refresh failed or retry still 401 - check if session was cleared
            if (!getStoredSession()) {
              get().signOut();
            }
            // Session preserved - will retry later
          }
        } catch (error) {
          console.error("Failed to fetch devices:", error);
        }

        set({ devicesLoading: false });
      },

      selectDevice: (deviceId) => {
        set({ selectedDeviceId: deviceId });
      },

      claimDevice: async (deviceId, token, name) => {
        const accessToken = await get().getAccessToken();
        if (!accessToken) return false;

        try {
          const response = await fetch("/api/devices/claim", {
            method: "POST",
            headers: {
              "Content-Type": "application/json",
              Authorization: `Bearer ${accessToken}`,
            },
            body: JSON.stringify({ deviceId, token, name }),
          });

          if (response.ok) {
            await get().fetchDevices();
            return true;
          }
        } catch (error) {
          console.error("Failed to claim device:", error);
        }

        return false;
      },

      removeDevice: async (deviceId) => {
        const token = await get().getAccessToken();
        if (!token) return false;

        try {
          const response = await fetch(`/api/devices/${deviceId}`, {
            method: "DELETE",
            headers: {
              Authorization: `Bearer ${token}`,
            },
          });

          if (response.ok) {
            set((state) => {
              const remainingDevices = state.devices.filter(
                (d) => d.id !== deviceId
              );
              // If removing the selected device, select the first remaining one
              const newSelectedId =
                state.selectedDeviceId === deviceId
                  ? remainingDevices[0]?.id ?? null
                  : state.selectedDeviceId;

              return {
                devices: remainingDevices,
                selectedDeviceId: newSelectedId,
              };
            });
            return true;
          }
        } catch (error) {
          console.error("Failed to remove device:", error);
        }

        return false;
      },

      renameDevice: async (deviceId, name) => {
        const token = await get().getAccessToken();
        if (!token) return false;

        try {
          const response = await fetch(`/api/devices/${deviceId}`, {
            method: "PATCH",
            headers: {
              "Content-Type": "application/json",
              Authorization: `Bearer ${token}`,
            },
            body: JSON.stringify({ name }),
          });

          if (response.ok) {
            set((state) => ({
              devices: state.devices.map((d) =>
                d.id === deviceId ? { ...d, name } : d
              ),
            }));
            return true;
          }
        } catch (error) {
          console.error("Failed to rename device:", error);
        }

        return false;
      },

      getSelectedDevice: () => {
        const { devices, selectedDeviceId } = get();
        return devices.find((d) => d.id === selectedDeviceId) ?? null;
      },

      getAccessToken: async () => {
        return getValidAccessToken();
      },
    }),
    {
      name: "brewos-app-state",
      partialize: (state) => ({
        selectedDeviceId: state.selectedDeviceId,
      }),
    }
  )
);
