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
import type { BackendInfo } from "./api-version";

/**
 * Fetch mode from server - the server knows if it's ESP32 (local) or cloud
 * Now uses /api/info as primary endpoint with fallback to /api/mode
 */
async function fetchModeFromServer(): Promise<{
  mode: ConnectionMode;
  apMode?: boolean;
  backendInfo?: BackendInfo;
}> {
  // Try /api/info first (new endpoint with full capabilities)
  try {
    const infoResponse = await fetch("/api/info");
    if (infoResponse.ok) {
      const info = await infoResponse.json() as BackendInfo;
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
    const response = await fetch("/api/mode");
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
      console.log("[Auth] Token expiring, refreshing...");
      const newSession = await refreshSession(session);

      if (newSession) {
        // Update store with new session
        store.updateSession(newSession);
      } else {
        // Refresh failed, sign out
        console.log("[Auth] Refresh failed, signing out");
        store.signOut();

        // Redirect to login if not already there
        if (window.location.pathname !== "/login") {
          window.location.href = "/login?expired=true";
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

export const useAppStore = create<AppState>()(
  persist(
    (set, get) => ({
      // Initial state
      mode: "local",
      apMode: false,
      initialized: false,
      user: null,
      session: null,
      authLoading: true,
      devices: [],
      selectedDeviceId: null,
      devicesLoading: false,

      initialize: async () => {
        // Fetch mode and backend info from server
        const { mode, apMode, backendInfo } = await fetchModeFromServer();
        set({ mode, apMode: apMode ?? false });
        
        // Update backend info store (for feature detection)
        if (backendInfo) {
          // Directly set info instead of fetching again
          const { compatible, warnings, errors } = await import("./api-version").then(
            (m) => m.checkCompatibility(backendInfo)
          );
          useBackendInfo.setState({
            info: backendInfo,
            loading: false,
            error: null,
            compatible,
            warnings,
            errors,
          });
        } else {
          // Fallback: fetch backend info separately
          useBackendInfo.getState().fetchInfo();
        }

        if (mode === "local") {
          set({ initialized: true, authLoading: false });
          return;
        }

        // Cloud mode: check for stored session
        const session = getStoredSession();

        if (session && !isTokenExpired(session)) {
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
            set({
              user: newSession.user,
              session: newSession,
              authLoading: false,
              initialized: true,
            });
            get().fetchDevices();
            startTokenRefreshMonitor(get());
          } else {
            // Refresh failed
            set({ authLoading: false, initialized: true });
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
            // Token expired/invalid, sign out
            get().signOut();
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
            set((state) => ({
              devices: state.devices.filter((d) => d.id !== deviceId),
              selectedDeviceId:
                state.selectedDeviceId === deviceId
                  ? state.devices[0]?.id ?? null
                  : state.selectedDeviceId,
            }));
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
