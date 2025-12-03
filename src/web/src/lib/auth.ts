/**
 * Authentication Module
 *
 * Handles session-based authentication with our cloud server.
 * OAuth providers (Google, etc.) are used ONLY for initial identity verification.
 * After verification, we use our own session tokens for all API calls.
 *
 * Architecture:
 * 1. User clicks "Sign in with Google" → Google verifies identity
 * 2. We send Google credential to /api/auth/google → Server issues our tokens
 * 3. All subsequent API calls use our access token
 * 4. When access token expires, we use refresh token to get new tokens
 * 5. Sessions can be revoked server-side at any time
 */

// Types
export interface User {
  id: string;
  email: string;
  name: string | null;
  picture: string | null;
}

export interface AuthSession {
  accessToken: string;
  refreshToken: string;
  expiresAt: number; // Unix timestamp (ms)
  user: User;
}

export interface AuthState {
  session: AuthSession | null;
  loading: boolean;
  error: string | null;
}

// Storage key
const AUTH_STORAGE_KEY = "brewos_session";

// Token refresh buffer (refresh 5 minutes before expiry)
const REFRESH_BUFFER_MS = 5 * 60 * 1000;

// Refresh lock to prevent concurrent refreshes
let refreshPromise: Promise<AuthSession | null> | null = null;

/**
 * Get stored session from localStorage
 */
export function getStoredSession(): AuthSession | null {
  try {
    const stored = localStorage.getItem(AUTH_STORAGE_KEY);
    console.log('[Auth] getStoredSession:', {
      hasStored: !!stored,
      storageKey: AUTH_STORAGE_KEY,
    });
    if (!stored) return null;
    return JSON.parse(stored) as AuthSession;
  } catch (e) {
    console.error('[Auth] getStoredSession parse error:', e);
    return null;
  }
}

/**
 * Store session in localStorage
 */
export function storeSession(session: AuthSession): void {
  try {
    localStorage.setItem(AUTH_STORAGE_KEY, JSON.stringify(session));
    console.log("[Auth] Session stored successfully for:", session.user?.email);
  } catch (error) {
    console.error("[Auth] Failed to store session:", error);
  }
}

/**
 * Clear stored session
 */
export function clearSession(): void {
  localStorage.removeItem(AUTH_STORAGE_KEY);
}

/**
 * Check if access token is expired or expiring soon
 */
export function isTokenExpired(session: AuthSession | null): boolean {
  if (!session) return true;
  return session.expiresAt < Date.now() + REFRESH_BUFFER_MS;
}

/**
 * Check if refresh token is still valid
 * Note: We don't store refresh expiry on client, so we rely on server response
 */
export function hasValidRefreshToken(session: AuthSession | null): boolean {
  return !!session?.refreshToken;
}

/**
 * Exchange Google credential for our session tokens
 */
export async function loginWithGoogle(
  googleCredential: string
): Promise<AuthSession> {
  const response = await fetch("/api/auth/google", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ credential: googleCredential }),
  });

  if (!response.ok) {
    const error = await response.json().catch(() => ({}));
    throw new Error(error.error || "Login failed");
  }

  const data = await response.json();

  const session: AuthSession = {
    accessToken: data.accessToken,
    refreshToken: data.refreshToken,
    expiresAt: data.expiresAt,
    user: data.user,
  };

  storeSession(session);
  return session;
}

// Timeout for refresh requests (8 seconds)
const REFRESH_TIMEOUT_MS = 8000;

/**
 * Refresh the session using refresh token
 * Implements token rotation (new refresh token each time)
 *
 * IMPORTANT: Only clears session on explicit 401/403 from server.
 * Network errors preserve the session to allow retry on reconnect.
 */
export async function refreshSession(
  currentSession: AuthSession,
  retryCount = 0
): Promise<AuthSession | null> {
  // Prevent concurrent refresh requests
  if (refreshPromise) {
    console.log("[Auth] Refresh already in progress, waiting...");
    return refreshPromise;
  }

  const MAX_RETRIES = 2;
  const RETRY_DELAY_MS = 1000;

  console.log("[Auth] Starting session refresh, attempt:", retryCount + 1);

  refreshPromise = (async () => {
    try {
      const controller = new AbortController();
      const timeoutId = setTimeout(
        () => controller.abort(),
        REFRESH_TIMEOUT_MS
      );

      const response = await fetch("/api/auth/refresh", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ refreshToken: currentSession.refreshToken }),
        signal: controller.signal,
      });

      clearTimeout(timeoutId);

      console.log("[Auth] Refresh response status:", response.status);

      if (!response.ok) {
        // Only clear session on explicit auth rejection (token invalid/revoked)
        if (response.status === 401 || response.status === 403) {
          const errorBody = await response.json().catch(() => ({}));
          console.error("[Auth] Refresh rejected by server:", {
            status: response.status,
            error: errorBody,
          });
          clearSession();
          return null;
        }

        // Server error - don't clear session, allow retry later
        console.warn("[Auth] Refresh server error:", response.status);
        return null;
      }

      const data = await response.json();

      const newSession: AuthSession = {
        accessToken: data.accessToken,
        refreshToken: data.refreshToken,
        expiresAt: data.expiresAt,
        user: currentSession.user, // Keep user info
      };

      storeSession(newSession);
      console.log("[Auth] Refresh successful, new expiry:", new Date(data.expiresAt).toISOString());
      return newSession;
    } catch (error) {
      // Network error - DON'T clear session!
      // The refresh token might still be valid, just no network right now.
      console.warn("[Auth] Refresh failed due to network error:", error);

      // Retry on network errors (common on iOS PWA cold start)
      if (retryCount < MAX_RETRIES) {
        await new Promise((r) =>
          setTimeout(r, RETRY_DELAY_MS * (retryCount + 1))
        );
        refreshPromise = null; // Allow retry
        return refreshSession(currentSession, retryCount + 1);
      }

      // After retries, still don't clear - session preserved for next attempt
      return null;
    } finally {
      refreshPromise = null;
    }
  })();

  return refreshPromise;
}

/**
 * Get a valid access token, refreshing if necessary
 * This is the main function to use before making API calls
 */
export async function getValidAccessToken(): Promise<string | null> {
  const session = getStoredSession();

  if (!session) {
    return null;
  }

  // If token is still valid, return it
  if (!isTokenExpired(session)) {
    return session.accessToken;
  }

  // Token expired, try to refresh
  if (hasValidRefreshToken(session)) {
    const newSession = await refreshSession(session);
    return newSession?.accessToken || null;
  }

  // No valid token or refresh token
  clearSession();
  return null;
}

/**
 * Logout - revoke session on server and clear local storage
 */
export async function logout(): Promise<void> {
  const session = getStoredSession();

  if (session) {
    try {
      await fetch("/api/auth/logout", {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
          Authorization: `Bearer ${session.accessToken}`,
        },
      });
    } catch {
      // Ignore errors - we'll clear local session anyway
    }
  }

  clearSession();
}

/**
 * Logout from all devices
 */
export async function logoutAll(): Promise<void> {
  const session = getStoredSession();

  if (session) {
    try {
      await fetch("/api/auth/logout-all", {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
          Authorization: `Bearer ${session.accessToken}`,
        },
      });
    } catch {
      // Ignore errors
    }
  }

  clearSession();
}

/**
 * Get current user info from stored session
 */
export function getCurrentUser(): User | null {
  const session = getStoredSession();
  return session?.user || null;
}

/**
 * Check if user is authenticated (has valid session)
 */
export function isAuthenticated(): boolean {
  const session = getStoredSession();
  return session !== null && !isTokenExpired(session);
}

// Re-export Google Client ID for the sign-in button
export const GOOGLE_CLIENT_ID = import.meta.env.VITE_GOOGLE_CLIENT_ID || "";
export const isGoogleAuthConfigured = !!GOOGLE_CLIENT_ID;

// Import store for hooks
import { useAppStore } from "./mode";

/**
 * Hook for authentication state and actions
 */
export function useAuth() {
  const user = useAppStore((state) => state.user);
  const loading = useAppStore((state) => state.authLoading);
  const handleGoogleLogin = useAppStore((state) => state.handleGoogleLogin);
  const signOut = useAppStore((state) => state.signOut);

  return {
    user,
    loading,
    handleGoogleLogin,
    signOut,
  };
}

/**
 * Hook for device management
 */
export function useDevices() {
  const devices = useAppStore((state) => state.devices);
  const selectedDeviceId = useAppStore((state) => state.selectedDeviceId);
  const loading = useAppStore((state) => state.devicesLoading);
  const fetchDevices = useAppStore((state) => state.fetchDevices);
  const selectDevice = useAppStore((state) => state.selectDevice);
  const claimDevice = useAppStore((state) => state.claimDevice);
  const removeDevice = useAppStore((state) => state.removeDevice);
  const renameDevice = useAppStore((state) => state.renameDevice);
  const getSelectedDevice = useAppStore((state) => state.getSelectedDevice);

  return {
    devices,
    selectedDeviceId,
    loading,
    fetchDevices,
    selectDevice,
    claimDevice,
    removeDevice,
    renameDevice,
    getSelectedDevice,
  };
}
