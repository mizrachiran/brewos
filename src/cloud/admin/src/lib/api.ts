/**
 * Admin API Client
 *
 * Handles all API calls to the admin endpoints.
 * 
 * Note: The main web app stores session under "brewos_session" as JSON.
 * We read from there to share authentication.
 */

import type {
  AdminStats,
  AdminUserListItem,
  AdminUserDetail,
  AdminDeviceListItem,
  AdminDeviceDetail,
  AdminSession,
  PaginatedResponse,
  SuccessResponse,
  ImpersonationResponse,
  ConnectionStats,
  AdminUser,
} from "./types";

const API_BASE = "/api";

// Session storage key used by main web app
const SESSION_STORAGE_KEY = "brewos_session";

interface StoredSession {
  accessToken: string;
  refreshToken: string;
  expiresAt: number;
  user: {
    id: string;
    email: string;
    name: string | null;
    picture: string | null;
  };
}

/**
 * Get stored session from localStorage (shared with main app)
 */
export function getStoredSession(): StoredSession | null {
  try {
    const stored = localStorage.getItem(SESSION_STORAGE_KEY);
    if (!stored) return null;
    return JSON.parse(stored) as StoredSession;
  } catch {
    return null;
  }
}

/**
 * Store session in localStorage (shared format with main app)
 */
function storeSession(session: StoredSession): void {
  localStorage.setItem(SESSION_STORAGE_KEY, JSON.stringify(session));
}

/**
 * Clear stored session
 */
export function clearSession(): void {
  localStorage.removeItem(SESSION_STORAGE_KEY);
}

class ApiError extends Error {
  constructor(
    public status: number,
    message: string
  ) {
    super(message);
    this.name = "ApiError";
  }
}

async function fetchWithAuth(
  path: string,
  options: RequestInit = {}
): Promise<Response> {
  const session = getStoredSession();
  const token = session?.accessToken;

  const headers: HeadersInit = {
    "Content-Type": "application/json",
    ...(options.headers || {}),
  };

  if (token) {
    (headers as Record<string, string>)["Authorization"] = `Bearer ${token}`;
  }

  const response = await fetch(`${API_BASE}${path}`, {
    ...options,
    headers,
  });

  // Handle 401 - try to refresh token
  if (response.status === 401) {
    const refreshed = await tryRefreshToken();
    if (refreshed) {
      // Retry with new token
      const newSession = getStoredSession();
      const newToken = newSession?.accessToken;
      if (newToken) {
        (headers as Record<string, string>)["Authorization"] = `Bearer ${newToken}`;
        return fetch(`${API_BASE}${path}`, {
          ...options,
          headers,
        });
      }
    }
    throw new ApiError(401, "Session expired");
  }

  return response;
}

async function tryRefreshToken(): Promise<boolean> {
  const session = getStoredSession();
  const refreshToken = session?.refreshToken;
  if (!refreshToken) return false;

  try {
    const response = await fetch(`${API_BASE}/auth/refresh`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ refreshToken }),
    });

    if (!response.ok) return false;

    const data = await response.json();
    
    // Update session in shared format
    if (session) {
      storeSession({
        ...session,
        accessToken: data.accessToken,
        refreshToken: data.refreshToken,
        expiresAt: data.expiresAt,
      });
    }
    return true;
  } catch {
    return false;
  }
}

async function apiGet<T>(path: string): Promise<T> {
  const response = await fetchWithAuth(path);

  if (!response.ok) {
    const error = await response.json().catch(() => ({ error: "Unknown error" }));
    throw new ApiError(response.status, error.error || "Request failed");
  }

  return response.json();
}

async function apiPost<T>(path: string, data?: unknown): Promise<T> {
  const response = await fetchWithAuth(path, {
    method: "POST",
    body: data ? JSON.stringify(data) : undefined,
  });

  if (!response.ok) {
    const error = await response.json().catch(() => ({ error: "Unknown error" }));
    throw new ApiError(response.status, error.error || "Request failed");
  }

  return response.json();
}

async function apiDelete<T>(path: string): Promise<T> {
  const response = await fetchWithAuth(path, {
    method: "DELETE",
  });

  if (!response.ok) {
    const error = await response.json().catch(() => ({ error: "Unknown error" }));
    throw new ApiError(response.status, error.error || "Request failed");
  }

  return response.json();
}

// Auth APIs
async function getMe(): Promise<{ user: AdminUser }> {
  return apiGet("/auth/me");
}

// Admin APIs
async function getStats(): Promise<AdminStats> {
  return apiGet("/admin/stats");
}

// User APIs
async function getUsers(
  page: number = 1,
  pageSize: number = 20,
  search?: string
): Promise<PaginatedResponse<AdminUserListItem>> {
  const params = new URLSearchParams({
    page: page.toString(),
    pageSize: pageSize.toString(),
  });
  if (search) params.set("search", search);
  return apiGet(`/admin/users?${params}`);
}

async function getUser(id: string): Promise<AdminUserDetail> {
  return apiGet(`/admin/users/${id}`);
}

async function deleteUser(id: string): Promise<SuccessResponse> {
  return apiDelete(`/admin/users/${id}`);
}

async function promoteUser(id: string): Promise<SuccessResponse> {
  return apiPost(`/admin/users/${id}/promote`);
}

async function demoteUser(id: string): Promise<SuccessResponse> {
  return apiPost(`/admin/users/${id}/demote`);
}

async function impersonateUser(id: string): Promise<ImpersonationResponse> {
  return apiPost(`/admin/users/${id}/impersonate`);
}

// Device APIs
async function getDevices(
  page: number = 1,
  pageSize: number = 20,
  search?: string,
  onlineOnly?: boolean
): Promise<PaginatedResponse<AdminDeviceListItem>> {
  const params = new URLSearchParams({
    page: page.toString(),
    pageSize: pageSize.toString(),
  });
  if (search) params.set("search", search);
  if (onlineOnly) params.set("onlineOnly", "true");
  return apiGet(`/admin/devices?${params}`);
}

async function getDevice(id: string): Promise<AdminDeviceDetail> {
  return apiGet(`/admin/devices/${id}`);
}

async function deleteDevice(id: string): Promise<SuccessResponse> {
  return apiDelete(`/admin/devices/${id}`);
}

async function disconnectDevice(id: string): Promise<SuccessResponse> {
  return apiPost(`/admin/devices/${id}/disconnect`);
}

// Session APIs
async function getSessions(
  page: number = 1,
  pageSize: number = 20,
  userId?: string
): Promise<PaginatedResponse<AdminSession>> {
  const params = new URLSearchParams({
    page: page.toString(),
    pageSize: pageSize.toString(),
  });
  if (userId) params.set("userId", userId);
  return apiGet(`/admin/sessions?${params}`);
}

async function revokeSession(id: string): Promise<SuccessResponse> {
  return apiDelete(`/admin/sessions/${id}`);
}

// Monitoring APIs
async function getConnections(): Promise<ConnectionStats> {
  return apiGet("/admin/connections");
}

// Health APIs (not admin protected)
async function getHealth(): Promise<Record<string, unknown>> {
  return apiGet("/health");
}

async function getHealthDetailed(): Promise<Record<string, unknown>> {
  return apiGet("/health/detailed");
}

export const api = {
  // Auth
  getMe,

  // Admin
  getStats,

  // Users
  getUsers,
  getUser,
  deleteUser,
  promoteUser,
  demoteUser,
  impersonateUser,

  // Devices
  getDevices,
  getDevice,
  deleteDevice,
  disconnectDevice,

  // Sessions
  getSessions,
  revokeSession,

  // Monitoring
  getConnections,
  getHealth,
  getHealthDetailed,
};

export { ApiError };

