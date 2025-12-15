/**
 * Admin Types
 *
 * Type definitions for the admin area.
 */

// User types
export interface AdminUser {
  id: string;
  email: string | null;
  name: string | null;
  picture: string | null;
  isAdmin: boolean;
}

export interface AdminUserListItem {
  id: string;
  email: string | null;
  displayName: string | null;
  avatarUrl: string | null;
  isAdmin: boolean;
  deviceCount: number;
  sessionCount: number;
  createdAt: string;
  updatedAt: string;
}

export interface AdminUserDetail extends AdminUserListItem {
  devices: Array<{
    id: string;
    name: string;
    isOnline: boolean;
    lastSeenAt: string | null;
  }>;
  sessions: Array<{
    id: string;
    userAgent: string | null;
    ipAddress: string | null;
    createdAt: string;
    lastUsedAt: string;
  }>;
}

// Device types
export interface AdminDeviceListItem {
  id: string;
  name: string;
  machineBrand: string | null;
  machineModel: string | null;
  machineType: string | null;
  firmwareVersion: string | null;
  isOnline: boolean;
  lastSeenAt: string | null;
  userCount: number;
  createdAt: string;
}

export interface AdminDeviceDetail extends AdminDeviceListItem {
  users: Array<{
    id: string;
    email: string | null;
    displayName: string | null;
    avatarUrl: string | null;
    claimedAt: string;
  }>;
}

// Session types
export interface AdminSession {
  id: string;
  userId: string;
  userEmail: string | null;
  userDisplayName: string | null;
  userAgent: string | null;
  ipAddress: string | null;
  createdAt: string;
  lastUsedAt: string;
  accessExpiresAt: string;
}

// Stats types
export interface AdminStats {
  userCount: number;
  adminCount: number;
  deviceCount: number;
  onlineDeviceCount: number;
  sessionCount: number;
  uptime: number;
}

// Connection stats
export interface ConnectionStats {
  devices: {
    connectedDevices: number;
    devices: Array<{
      id: string;
      connectedAt: string;
      lastSeen: string;
      connectionAge: number;
    }>;
  };
  clients: {
    connectedClients: number;
    totalConnections: number;
    totalMessages: number;
    queuedMessagesTotal: number;
    clientsByDevice: Record<string, number>;
  };
}

// Paginated response
export interface PaginatedResponse<T> {
  items: T[];
  total: number;
  page: number;
  pageSize: number;
  totalPages: number;
}

// API response types
export interface ApiResponse<T> {
  data?: T;
  error?: string;
}

export interface SuccessResponse {
  success: boolean;
}

export interface ImpersonationResponse {
  accessToken: string;
  expiresAt: number;
}

