/**
 * Admin Service
 *
 * Provides administrative functions for managing users, devices, and sessions.
 * All functions require admin authorization (handled by middleware).
 */

import { getDb, saveDatabase } from "../lib/database.js";
import { nowUTC } from "../lib/date.js";
import { createSession } from "./session.js";
import { getAdminCount } from "../middleware/admin.js";

// =============================================================================
// Types
// =============================================================================

export interface AdminStats {
  userCount: number;
  adminCount: number;
  deviceCount: number;
  onlineDeviceCount: number;
  sessionCount: number;
  uptime: number;
}

export interface AdminUser {
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

export interface AdminUserDetail extends AdminUser {
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

export interface AdminDevice {
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

export interface AdminDeviceDetail extends AdminDevice {
  users: Array<{
    id: string;
    email: string | null;
    displayName: string | null;
    avatarUrl: string | null;
    claimedAt: string;
  }>;
}

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

export interface PaginatedResult<T> {
  items: T[];
  total: number;
  page: number;
  pageSize: number;
  totalPages: number;
}

// =============================================================================
// System Stats
// =============================================================================

/**
 * Get system-wide statistics
 */
export function getSystemStats(): AdminStats {
  const db = getDb();

  const userCount = db.exec(`SELECT COUNT(*) FROM profiles`);
  const adminCount = db.exec(
    `SELECT COUNT(*) FROM profiles WHERE is_admin = 1`
  );
  const deviceCount = db.exec(`SELECT COUNT(*) FROM devices`);
  const onlineDeviceCount = db.exec(
    `SELECT COUNT(*) FROM devices WHERE is_online = 1`
  );
  const sessionCount = db.exec(`SELECT COUNT(*) FROM sessions`);

  return {
    userCount: (userCount[0]?.values[0]?.[0] as number) || 0,
    adminCount: (adminCount[0]?.values[0]?.[0] as number) || 0,
    deviceCount: (deviceCount[0]?.values[0]?.[0] as number) || 0,
    onlineDeviceCount: (onlineDeviceCount[0]?.values[0]?.[0] as number) || 0,
    sessionCount: (sessionCount[0]?.values[0]?.[0] as number) || 0,
    uptime: process.uptime(),
  };
}

// =============================================================================
// User Management
// =============================================================================

/**
 * Get all users with pagination and search
 */
export function getAllUsers(
  page: number = 1,
  pageSize: number = 20,
  search?: string
): PaginatedResult<AdminUser> {
  const db = getDb();
  const offset = (page - 1) * pageSize;

  let whereClause = "";
  const params: unknown[] = [];

  if (search) {
    whereClause = `WHERE p.email LIKE ? OR p.display_name LIKE ?`;
    params.push(`%${search}%`, `%${search}%`);
  }

  // Get total count
  const countResult = db.exec(
    `SELECT COUNT(*) FROM profiles p ${whereClause}`,
    params
  );
  const total = (countResult[0]?.values[0]?.[0] as number) || 0;

  // Get users with device and session counts
  const result = db.exec(
    `SELECT 
      p.id,
      p.email,
      p.display_name,
      p.avatar_url,
      p.is_admin,
      p.created_at,
      p.updated_at,
      (SELECT COUNT(*) FROM user_devices ud WHERE ud.user_id = p.id) as device_count,
      (SELECT COUNT(*) FROM sessions s WHERE s.user_id = p.id) as session_count
    FROM profiles p
    ${whereClause}
    ORDER BY p.created_at DESC
    LIMIT ? OFFSET ?`,
    [...params, pageSize, offset]
  );

  const items: AdminUser[] =
    result.length > 0
      ? result[0].values.map((row) => ({
          id: row[0] as string,
          email: row[1] as string | null,
          displayName: row[2] as string | null,
          avatarUrl: row[3] as string | null,
          isAdmin: row[4] === 1,
          createdAt: row[5] as string,
          updatedAt: row[6] as string,
          deviceCount: row[7] as number,
          sessionCount: row[8] as number,
        }))
      : [];

  return {
    items,
    total,
    page,
    pageSize,
    totalPages: Math.ceil(total / pageSize),
  };
}

/**
 * Get detailed user information
 */
export function getUserById(userId: string): AdminUserDetail | null {
  const db = getDb();

  // Get user profile
  const profileResult = db.exec(
    `SELECT 
      p.id,
      p.email,
      p.display_name,
      p.avatar_url,
      p.is_admin,
      p.created_at,
      p.updated_at,
      (SELECT COUNT(*) FROM user_devices ud WHERE ud.user_id = p.id) as device_count,
      (SELECT COUNT(*) FROM sessions s WHERE s.user_id = p.id) as session_count
    FROM profiles p
    WHERE p.id = ?`,
    [userId]
  );

  if (profileResult.length === 0 || profileResult[0].values.length === 0) {
    return null;
  }

  const row = profileResult[0].values[0];
  const user: AdminUserDetail = {
    id: row[0] as string,
    email: row[1] as string | null,
    displayName: row[2] as string | null,
    avatarUrl: row[3] as string | null,
    isAdmin: row[4] === 1,
    createdAt: row[5] as string,
    updatedAt: row[6] as string,
    deviceCount: row[7] as number,
    sessionCount: row[8] as number,
    devices: [],
    sessions: [],
  };

  // Get user's devices
  const devicesResult = db.exec(
    `SELECT 
      d.id,
      ud.name,
      d.is_online,
      d.last_seen_at
    FROM devices d
    JOIN user_devices ud ON d.id = ud.device_id
    WHERE ud.user_id = ?
    ORDER BY ud.claimed_at DESC`,
    [userId]
  );

  if (devicesResult.length > 0) {
    user.devices = devicesResult[0].values.map((row) => ({
      id: row[0] as string,
      name: row[1] as string,
      isOnline: row[2] === 1,
      lastSeenAt: row[3] as string | null,
    }));
  }

  // Get user's sessions
  const sessionsResult = db.exec(
    `SELECT 
      id,
      user_agent,
      ip_address,
      created_at,
      last_used_at
    FROM sessions
    WHERE user_id = ?
    ORDER BY last_used_at DESC`,
    [userId]
  );

  if (sessionsResult.length > 0) {
    user.sessions = sessionsResult[0].values.map((row) => ({
      id: row[0] as string,
      userAgent: row[1] as string | null,
      ipAddress: row[2] as string | null,
      createdAt: row[3] as string,
      lastUsedAt: row[4] as string,
    }));
  }

  return user;
}

/**
 * Delete a user and all associated data
 * Cannot delete the last admin
 */
export function deleteUser(userId: string): { success: boolean; error?: string } {
  const db = getDb();

  // Check if user exists
  const userResult = db.exec(
    `SELECT is_admin FROM profiles WHERE id = ?`,
    [userId]
  );

  if (userResult.length === 0 || userResult[0].values.length === 0) {
    return { success: false, error: "User not found" };
  }

  const isAdmin = userResult[0].values[0][0] === 1;

  // Prevent deleting last admin
  if (isAdmin && getAdminCount() <= 1) {
    return { success: false, error: "Cannot delete the last admin" };
  }

  // Delete user (cascades to sessions, user_devices, push_subscriptions, etc.)
  db.run(`DELETE FROM profiles WHERE id = ?`, [userId]);
  saveDatabase();

  return { success: true };
}

/**
 * Promote a user to admin
 */
export function promoteToAdmin(userId: string): { success: boolean; error?: string } {
  const db = getDb();

  // Check if user exists
  const userResult = db.exec(
    `SELECT is_admin FROM profiles WHERE id = ?`,
    [userId]
  );

  if (userResult.length === 0 || userResult[0].values.length === 0) {
    return { success: false, error: "User not found" };
  }

  const isAlreadyAdmin = userResult[0].values[0][0] === 1;
  if (isAlreadyAdmin) {
    return { success: false, error: "User is already an admin" };
  }

  db.run(`UPDATE profiles SET is_admin = 1, updated_at = ? WHERE id = ?`, [
    nowUTC(),
    userId,
  ]);
  saveDatabase();

  return { success: true };
}

/**
 * Demote a user from admin
 * Cannot demote self or the last admin
 */
export function demoteFromAdmin(
  userId: string,
  requestingAdminId: string
): { success: boolean; error?: string } {
  const db = getDb();

  // Cannot demote self
  if (userId === requestingAdminId) {
    return { success: false, error: "Cannot demote yourself" };
  }

  // Check if user exists and is admin
  const userResult = db.exec(
    `SELECT is_admin FROM profiles WHERE id = ?`,
    [userId]
  );

  if (userResult.length === 0 || userResult[0].values.length === 0) {
    return { success: false, error: "User not found" };
  }

  const isAdmin = userResult[0].values[0][0] === 1;
  if (!isAdmin) {
    return { success: false, error: "User is not an admin" };
  }

  // Prevent demoting last admin
  if (getAdminCount() <= 1) {
    return { success: false, error: "Cannot demote the last admin" };
  }

  db.run(`UPDATE profiles SET is_admin = 0, updated_at = ? WHERE id = ?`, [
    nowUTC(),
    userId,
  ]);
  saveDatabase();

  return { success: true };
}

/**
 * Create an impersonation token for a user
 * Allows admin to log in as another user temporarily
 */
export function createImpersonationToken(
  targetUserId: string,
  adminId: string
): { accessToken: string; expiresAt: number } | null {
  const db = getDb();

  // Check if target user exists
  const userResult = db.exec(`SELECT id FROM profiles WHERE id = ?`, [
    targetUserId,
  ]);

  if (userResult.length === 0 || userResult[0].values.length === 0) {
    return null;
  }

  // Create a short-lived session for impersonation (1 hour)
  const tokens = createSession(targetUserId, {
    userAgent: `Impersonation by admin ${adminId}`,
    ipAddress: "admin-impersonation",
  });

  console.log(
    `[Admin] Admin ${adminId} impersonating user ${targetUserId}`
  );

  return {
    accessToken: tokens.accessToken,
    expiresAt: tokens.accessExpiresAt,
  };
}

// =============================================================================
// Device Management
// =============================================================================

/**
 * Get all devices with pagination and filters
 */
export function getAllDevices(
  page: number = 1,
  pageSize: number = 20,
  search?: string,
  onlineOnly?: boolean
): PaginatedResult<AdminDevice> {
  const db = getDb();
  const offset = (page - 1) * pageSize;

  const conditions: string[] = [];
  const params: unknown[] = [];

  if (search) {
    conditions.push(`(d.id LIKE ? OR d.name LIKE ? OR d.machine_brand LIKE ?)`);
    params.push(`%${search}%`, `%${search}%`, `%${search}%`);
  }

  if (onlineOnly) {
    conditions.push(`d.is_online = 1`);
  }

  const whereClause =
    conditions.length > 0 ? `WHERE ${conditions.join(" AND ")}` : "";

  // Get total count
  const countResult = db.exec(
    `SELECT COUNT(*) FROM devices d ${whereClause}`,
    params
  );
  const total = (countResult[0]?.values[0]?.[0] as number) || 0;

  // Get devices with user count
  const result = db.exec(
    `SELECT 
      d.id,
      d.name,
      d.machine_brand,
      d.machine_model,
      d.machine_type,
      d.firmware_version,
      d.is_online,
      d.last_seen_at,
      d.created_at,
      (SELECT COUNT(*) FROM user_devices ud WHERE ud.device_id = d.id) as user_count
    FROM devices d
    ${whereClause}
    ORDER BY d.last_seen_at DESC NULLS LAST
    LIMIT ? OFFSET ?`,
    [...params, pageSize, offset]
  );

  const items: AdminDevice[] =
    result.length > 0
      ? result[0].values.map((row) => ({
          id: row[0] as string,
          name: row[1] as string,
          machineBrand: row[2] as string | null,
          machineModel: row[3] as string | null,
          machineType: row[4] as string | null,
          firmwareVersion: row[5] as string | null,
          isOnline: row[6] === 1,
          lastSeenAt: row[7] as string | null,
          createdAt: row[8] as string,
          userCount: row[9] as number,
        }))
      : [];

  return {
    items,
    total,
    page,
    pageSize,
    totalPages: Math.ceil(total / pageSize),
  };
}

/**
 * Get detailed device information
 */
export function getDeviceById(deviceId: string): AdminDeviceDetail | null {
  const db = getDb();

  const deviceResult = db.exec(
    `SELECT 
      d.id,
      d.name,
      d.machine_brand,
      d.machine_model,
      d.machine_type,
      d.firmware_version,
      d.is_online,
      d.last_seen_at,
      d.created_at,
      (SELECT COUNT(*) FROM user_devices ud WHERE ud.device_id = d.id) as user_count
    FROM devices d
    WHERE d.id = ?`,
    [deviceId]
  );

  if (deviceResult.length === 0 || deviceResult[0].values.length === 0) {
    return null;
  }

  const row = deviceResult[0].values[0];
  const device: AdminDeviceDetail = {
    id: row[0] as string,
    name: row[1] as string,
    machineBrand: row[2] as string | null,
    machineModel: row[3] as string | null,
    machineType: row[4] as string | null,
    firmwareVersion: row[5] as string | null,
    isOnline: row[6] === 1,
    lastSeenAt: row[7] as string | null,
    createdAt: row[8] as string,
    userCount: row[9] as number,
    users: [],
  };

  // Get device users
  const usersResult = db.exec(
    `SELECT 
      p.id,
      p.email,
      p.display_name,
      p.avatar_url,
      ud.claimed_at
    FROM profiles p
    JOIN user_devices ud ON p.id = ud.user_id
    WHERE ud.device_id = ?
    ORDER BY ud.claimed_at ASC`,
    [deviceId]
  );

  if (usersResult.length > 0) {
    device.users = usersResult[0].values.map((row) => ({
      id: row[0] as string,
      email: row[1] as string | null,
      displayName: row[2] as string | null,
      avatarUrl: row[3] as string | null,
      claimedAt: row[4] as string,
    }));
  }

  return device;
}

/**
 * Force delete a device (removes from all users)
 */
export function forceDeleteDevice(deviceId: string): { success: boolean; error?: string } {
  const db = getDb();

  // Check if device exists
  const deviceResult = db.exec(`SELECT id FROM devices WHERE id = ?`, [
    deviceId,
  ]);

  if (deviceResult.length === 0 || deviceResult[0].values.length === 0) {
    return { success: false, error: "Device not found" };
  }

  // Delete device (cascades to user_devices, claim_tokens, share_tokens)
  db.run(`DELETE FROM user_devices WHERE device_id = ?`, [deviceId]);
  db.run(`DELETE FROM device_claim_tokens WHERE device_id = ?`, [deviceId]);
  db.run(`DELETE FROM device_share_tokens WHERE device_id = ?`, [deviceId]);
  db.run(`DELETE FROM devices WHERE id = ?`, [deviceId]);
  saveDatabase();

  return { success: true };
}

// =============================================================================
// Session Management
// =============================================================================

/**
 * Get all sessions with pagination
 */
export function getAllSessions(
  page: number = 1,
  pageSize: number = 20,
  userId?: string
): PaginatedResult<AdminSession> {
  const db = getDb();
  const offset = (page - 1) * pageSize;

  let whereClause = "";
  const params: unknown[] = [];

  if (userId) {
    whereClause = `WHERE s.user_id = ?`;
    params.push(userId);
  }

  // Get total count
  const countResult = db.exec(
    `SELECT COUNT(*) FROM sessions s ${whereClause}`,
    params
  );
  const total = (countResult[0]?.values[0]?.[0] as number) || 0;

  // Get sessions with user info
  const result = db.exec(
    `SELECT 
      s.id,
      s.user_id,
      p.email,
      p.display_name,
      s.user_agent,
      s.ip_address,
      s.created_at,
      s.last_used_at,
      s.access_expires_at
    FROM sessions s
    JOIN profiles p ON s.user_id = p.id
    ${whereClause}
    ORDER BY s.last_used_at DESC
    LIMIT ? OFFSET ?`,
    [...params, pageSize, offset]
  );

  const items: AdminSession[] =
    result.length > 0
      ? result[0].values.map((row) => ({
          id: row[0] as string,
          userId: row[1] as string,
          userEmail: row[2] as string | null,
          userDisplayName: row[3] as string | null,
          userAgent: row[4] as string | null,
          ipAddress: row[5] as string | null,
          createdAt: row[6] as string,
          lastUsedAt: row[7] as string,
          accessExpiresAt: row[8] as string,
        }))
      : [];

  return {
    items,
    total,
    page,
    pageSize,
    totalPages: Math.ceil(total / pageSize),
  };
}

/**
 * Revoke any session (admin can revoke any user's session)
 */
export function revokeAnySession(sessionId: string): { success: boolean; error?: string } {
  const db = getDb();

  // Check if session exists
  const sessionResult = db.exec(`SELECT id FROM sessions WHERE id = ?`, [
    sessionId,
  ]);

  if (sessionResult.length === 0 || sessionResult[0].values.length === 0) {
    return { success: false, error: "Session not found" };
  }

  db.run(`DELETE FROM sessions WHERE id = ?`, [sessionId]);
  saveDatabase();

  return { success: true };
}

