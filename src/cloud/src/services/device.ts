import { createHash, randomUUID, timingSafeEqual, randomBytes } from "crypto";
import {
  getDb,
  saveDatabase,
  Device,
  Profile,
  resultToObjects,
} from "../lib/database.js";
import { nowUTC, futureUTC, isExpired } from "../lib/date.js";

/**
 * Generate a hash for token comparison
 */
function hashToken(token: string): string {
  return createHash("sha256").update(token).digest("hex");
}

// ============================================================================
// Device Key Management
// ============================================================================

/**
 * Register or update the device key for a device
 * Called during claim token registration (device provides its key)
 */
export function registerDeviceKey(deviceId: string, deviceKey: string): void {
  const db = getDb();
  const keyHash = hashToken(deviceKey);
  const now = nowUTC();

  // Check if device exists
  const existing = db.exec(`SELECT id FROM devices WHERE id = ?`, [deviceId]);

  if (existing.length > 0 && existing[0].values.length > 0) {
    // Update existing device's key
    db.run(
      `UPDATE devices SET device_key_hash = ?, updated_at = ? WHERE id = ?`,
      [keyHash, now, deviceId]
    );
  } else {
    // Create device record with key (will be fully populated when claimed)
    db.run(
      `INSERT INTO devices (id, device_key_hash, created_at, updated_at) VALUES (?, ?, ?, ?)`,
      [deviceId, keyHash, now, now]
    );
  }

  saveDatabase();
}

/**
 * Verify a device key against the stored hash
 * Used during WebSocket connection to authenticate devices
 */
export function verifyDeviceKey(deviceId: string, deviceKey: string): boolean {
  const db = getDb();
  const keyHash = hashToken(deviceKey);

  const result = db.exec(
    `SELECT device_key_hash FROM devices WHERE id = ?`,
    [deviceId]
  );

  if (result.length === 0 || result[0].values.length === 0) {
    // Device doesn't exist - reject connection
    console.warn(`[Device] Unknown device ${deviceId} attempting connection`);
    return false;
  }

  const storedHash = result[0].values[0][0] as string | null;

  if (!storedHash) {
    // Device exists but has no key set - reject connection
    console.warn(`[Device] Device ${deviceId} has no registered key`);
    return false;
  }

  // Constant-time comparison
  try {
    return timingSafeEqual(
      Buffer.from(keyHash, "hex"),
      Buffer.from(storedHash, "hex")
    );
  } catch {
    return false;
  }
}

/**
 * Check if a device has a registered key
 */
export function deviceHasKey(deviceId: string): boolean {
  const db = getDb();
  const result = db.exec(
    `SELECT device_key_hash FROM devices WHERE id = ? AND device_key_hash IS NOT NULL`,
    [deviceId]
  );

  return result.length > 0 && result[0].values.length > 0;
}

/**
 * Generate a new device key (used by ESP32 on first boot)
 */
export function generateDeviceKey(): string {
  return randomBytes(32).toString("base64url");
}

/**
 * Create or update a claim token for a device
 * Called by ESP32 when generating QR code
 */
export function createClaimToken(deviceId: string, token: string): void {
  const db = getDb();
  const tokenHash = hashToken(token);
  const expiresAt = futureUTC(10, "minutes"); // Expires in 10 minutes (UTC)
  const id = randomUUID();

  // Delete existing token for this device
  db.run(`DELETE FROM device_claim_tokens WHERE device_id = ?`, [deviceId]);

  // Insert new token
  db.run(
    `INSERT INTO device_claim_tokens (id, device_id, token_hash, expires_at) VALUES (?, ?, ?, ?)`,
    [id, deviceId, tokenHash, expiresAt]
  );

  saveDatabase();
}

/**
 * Verify a claim token is valid
 */
export function verifyClaimToken(deviceId: string, token: string): boolean {
  const db = getDb();
  const tokenHash = hashToken(token);

  const result = db.exec(
    `SELECT token_hash, expires_at FROM device_claim_tokens WHERE device_id = ?`,
    [deviceId]
  );

  if (result.length === 0 || result[0].values.length === 0) {
    return false;
  }

  const row = result[0].values[0];
  const storedHash = row[0] as string;
  const expiresAt = row[1] as string;

  // Check expiration (all timestamps are UTC)
  if (isExpired(expiresAt)) {
    return false;
  }

  // Constant-time comparison
  try {
    return timingSafeEqual(
      Buffer.from(tokenHash, "hex"),
      Buffer.from(storedHash, "hex")
    );
  } catch {
    return false;
  }
}

/**
 * Claim a device for a user
 * Allows multiple users to claim the same device with per-user names
 */
export function claimDevice(
  deviceId: string,
  userId: string,
  name?: string
): Device {
  const db = getDb();

  // Check if user already has this device
  const existingUserDevice = db.exec(
    `SELECT user_id FROM user_devices WHERE user_id = ? AND device_id = ?`,
    [userId, deviceId]
  );

  if (
    existingUserDevice.length > 0 &&
    existingUserDevice[0].values.length > 0
  ) {
    throw new Error("Device is already claimed by this user");
  }

  const now = nowUTC();
  const deviceName = name || "My BrewOS";

  // Check if device exists in devices table
  const existingDevice = db.exec(`SELECT id FROM devices WHERE id = ?`, [
    deviceId,
  ]);

  if (existingDevice.length === 0 || existingDevice[0].values.length === 0) {
    // Insert new device (without owner_id/name - those are in user_devices now)
    db.run(
      `INSERT INTO devices (id, created_at, updated_at) VALUES (?, ?, ?)`,
      [deviceId, now, now]
    );
  }

  // Add user-device association with per-user name
  db.run(
    `INSERT INTO user_devices (user_id, device_id, name, claimed_at, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?)`,
    [userId, deviceId, deviceName, now, now, now]
  );

  // Delete the claim token (only after successful claim)
  db.run(`DELETE FROM device_claim_tokens WHERE device_id = ?`, [deviceId]);

  saveDatabase();

  // Return the device
  const deviceResult = db.exec(`SELECT * FROM devices WHERE id = ?`, [
    deviceId,
  ]);
  return resultToObjects<Device>(deviceResult[0])[0];
}

/**
 * Get devices for a user with per-user names
 * Returns devices joined with user_devices to get user-specific names
 */
export function getUserDevices(
  userId: string
): Array<Device & { user_name: string; user_claimed_at: string }> {
  const db = getDb();
  const result = db.exec(
    `SELECT 
      d.*,
      ud.name as user_name,
      ud.claimed_at as user_claimed_at
    FROM devices d
    INNER JOIN user_devices ud ON d.id = ud.device_id
    WHERE ud.user_id = ?
    ORDER BY ud.claimed_at DESC`,
    [userId]
  );

  if (result.length === 0) {
    return [];
  }

  return resultToObjects<
    Device & { user_name: string; user_claimed_at: string }
  >(result[0]);
}

/**
 * Get a single device by ID
 * Optionally returns with user-specific name if userId is provided
 */
export function getDevice(
  deviceId: string,
  userId?: string
): Device | (Device & { user_name?: string }) | null {
  const db = getDb();

  if (userId) {
    // Return device with user-specific name
    const result = db.exec(
      `SELECT 
        d.*,
        ud.name as user_name
      FROM devices d
      LEFT JOIN user_devices ud ON d.id = ud.device_id AND ud.user_id = ?
      WHERE d.id = ?`,
      [userId, deviceId]
    );

    if (result.length === 0 || result[0].values.length === 0) {
      return null;
    }

    return resultToObjects<Device & { user_name?: string }>(result[0])[0];
  } else {
    // Return device without user-specific info
    const result = db.exec(`SELECT * FROM devices WHERE id = ?`, [deviceId]);

    if (result.length === 0 || result[0].values.length === 0) {
      return null;
    }

    return resultToObjects<Device>(result[0])[0];
  }
}

/**
 * Check if user owns a device
 */
export function userOwnsDevice(userId: string, deviceId: string): boolean {
  const db = getDb();
  const result = db.exec(
    `SELECT user_id FROM user_devices WHERE user_id = ? AND device_id = ?`,
    [userId, deviceId]
  );

  return result.length > 0 && result[0].values.length > 0;
}

/**
 * Update device online status
 */
export function updateDeviceStatus(
  deviceId: string,
  isOnline: boolean,
  firmwareVersion?: string
): void {
  const db = getDb();
  const now = nowUTC();

  if (firmwareVersion) {
    db.run(
      `UPDATE devices SET is_online = ?, last_seen_at = ?, firmware_version = ?, updated_at = ? WHERE id = ?`,
      [isOnline ? 1 : 0, now, firmwareVersion, now, deviceId]
    );
  } else {
    db.run(
      `UPDATE devices SET is_online = ?, last_seen_at = ?, updated_at = ? WHERE id = ?`,
      [isOnline ? 1 : 0, now, now, deviceId]
    );
  }

  saveDatabase();
}

/**
 * Remove a device from user's account
 * Only removes the user-device association, not the device itself
 */
export function removeDevice(deviceId: string, userId: string): void {
  const db = getDb();

  // Remove user-device association
  db.run(`DELETE FROM user_devices WHERE user_id = ? AND device_id = ?`, [
    userId,
    deviceId,
  ]);

  if (db.getRowsModified() === 0) {
    throw new Error("Failed to remove device");
  }

  // Optionally: if no users have this device anymore, we could delete the device
  // For now, we keep the device record for potential future claims
  // You could add cleanup logic here if needed

  saveDatabase();
}

/**
 * Rename a device (per-user name)
 */
export function renameDevice(
  deviceId: string,
  userId: string,
  name: string
): void {
  const db = getDb();
  const now = nowUTC();

  // Update per-user device name
  db.run(
    `UPDATE user_devices SET name = ?, updated_at = ? WHERE user_id = ? AND device_id = ?`,
    [name, now, userId, deviceId]
  );

  if (db.getRowsModified() === 0) {
    throw new Error("Failed to rename device");
  }

  saveDatabase();
}

/**
 * Update device machine info (brand, model)
 * Name updates are per-user and go to user_devices table
 */
export function updateDeviceMachineInfo(
  deviceId: string,
  userId: string,
  data: { name?: string; brand?: string; model?: string }
): void {
  const db = getDb();
  const now = nowUTC();

  // Check if user owns the device
  if (!userOwnsDevice(userId, deviceId)) {
    throw new Error("User does not own this device");
  }

  // Update per-user name if provided
  if (data.name) {
    db.run(
      `UPDATE user_devices SET name = ?, updated_at = ? WHERE user_id = ? AND device_id = ?`,
      [data.name, now, userId, deviceId]
    );
  }

  // Update shared device metadata (brand, model) if provided
  const deviceUpdates: string[] = ["updated_at = ?"];
  const deviceValues: unknown[] = [now];

  if (data.brand) {
    deviceUpdates.push("machine_brand = ?");
    deviceValues.push(data.brand);
  }
  if (data.model) {
    deviceUpdates.push("machine_model = ?");
    deviceValues.push(data.model);
  }

  if (deviceUpdates.length > 1) {
    deviceValues.push(deviceId);
    db.run(
      `UPDATE devices SET ${deviceUpdates.join(", ")} WHERE id = ?`,
      deviceValues
    );
  }

  saveDatabase();
}

/**
 * Get the count of users who have access to a device
 */
export function getDeviceUserCount(deviceId: string): number {
  const db = getDb();
  const result = db.exec(
    `SELECT COUNT(*) as count FROM user_devices WHERE device_id = ?`,
    [deviceId]
  );

  if (result.length === 0 || result[0].values.length === 0) {
    return 0;
  }

  return (result[0].values[0][0] as number) || 0;
}

/**
 * Get all users who have access to a device
 * Returns users with their profile info and claim details
 */
export function getDeviceUsers(deviceId: string): Array<{
  user_id: string;
  device_id: string;
  name: string;
  claimed_at: string;
  email: string | null;
  display_name: string | null;
  avatar_url: string | null;
}> {
  const db = getDb();
  const result = db.exec(
    `SELECT 
      ud.user_id,
      ud.device_id,
      ud.name,
      ud.claimed_at,
      p.email,
      p.display_name,
      p.avatar_url
    FROM user_devices ud
    INNER JOIN profiles p ON ud.user_id = p.id
    WHERE ud.device_id = ?
    ORDER BY ud.claimed_at ASC`,
    [deviceId]
  );

  if (result.length === 0) {
    return [];
  }

  return resultToObjects<{
    user_id: string;
    device_id: string;
    name: string;
    claimed_at: string;
    email: string | null;
    display_name: string | null;
    avatar_url: string | null;
  }>(result[0]);
}

/**
 * Revoke another user's access to a device (mutual removal)
 * Any user who has access can remove any other user
 */
export function revokeUserAccess(
  deviceId: string,
  userIdToRemove: string,
  userIdRemoving: string
): void {
  const db = getDb();

  // Verify the user doing the removal has access to the device
  if (!userOwnsDevice(userIdRemoving, deviceId)) {
    throw new Error("You do not have access to this device");
  }

  // Prevent users from removing themselves (they should use removeDevice instead)
  if (userIdToRemove === userIdRemoving) {
    throw new Error("Cannot remove yourself. Use remove device instead.");
  }

  // Verify the user to remove actually has access
  if (!userOwnsDevice(userIdToRemove, deviceId)) {
    throw new Error("User does not have access to this device");
  }

  // Remove the user-device association
  db.run(`DELETE FROM user_devices WHERE user_id = ? AND device_id = ?`, [
    userIdToRemove,
    deviceId,
  ]);

  if (db.getRowsModified() === 0) {
    throw new Error("Failed to revoke user access");
  }

  saveDatabase();
}

/**
 * Cleanup expired claim tokens
 */
export function cleanupExpiredTokens(): number {
  const db = getDb();
  db.run(`DELETE FROM device_claim_tokens WHERE expires_at < datetime('now')`);
  const claimDeleted = db.getRowsModified();

  // Also cleanup expired share tokens
  db.run(`DELETE FROM device_share_tokens WHERE expires_at < datetime('now')`);
  const shareDeleted = db.getRowsModified();

  const deleted = claimDeleted + shareDeleted;
  if (deleted > 0) {
    saveDatabase();
  }

  return deleted;
}

/**
 * Cleanup orphaned devices
 *
 * Deletes devices that:
 * 1. Have no users in user_devices table (orphaned)
 * 2. AND haven't been seen in the specified number of days (stale)
 *
 * This handles scenarios like:
 * - All users removed a device from their accounts
 * - ESP32 chip was replaced (new device ID, old one orphaned)
 * - Device was paired but never connected
 *
 * The staleness check prevents deleting a device that was just
 * removed by all users but might be re-paired soon.
 */
export function cleanupOrphanedDevices(staleDays: number = 30): number {
  const db = getDb();

  // Find orphaned devices: no entries in user_devices AND stale (not seen recently)
  // A device is considered stale if:
  // - last_seen_at is NULL (never connected), OR
  // - last_seen_at is older than staleDays
  const orphanedDevices = db.exec(
    `SELECT d.id FROM devices d
     LEFT JOIN user_devices ud ON d.id = ud.device_id
     WHERE ud.device_id IS NULL
       AND (d.last_seen_at IS NULL OR d.last_seen_at < datetime('now', ?))`,
    [`-${staleDays} days`]
  );

  if (orphanedDevices.length === 0 || orphanedDevices[0].values.length === 0) {
    return 0;
  }

  const deviceIds = orphanedDevices[0].values.map((row) => row[0] as string);

  // Delete related records first (foreign key cascades may not work with sql.js)
  for (const deviceId of deviceIds) {
    // Delete any lingering claim tokens
    db.run(`DELETE FROM device_claim_tokens WHERE device_id = ?`, [deviceId]);
    // Delete any lingering share tokens
    db.run(`DELETE FROM device_share_tokens WHERE device_id = ?`, [deviceId]);
    // Delete the device
    db.run(`DELETE FROM devices WHERE id = ?`, [deviceId]);
  }

  if (deviceIds.length > 0) {
    saveDatabase();
    console.log(
      `[Cleanup] Deleted ${deviceIds.length} orphaned devices: ${deviceIds.join(", ")}`
    );
  }

  return deviceIds.length;
}

/**
 * Generate a share token for a device
 * Used by device owners to share access with others
 */
export function createShareToken(
  deviceId: string,
  userId: string
): { token: string; expiresAt: string } {
  const db = getDb();

  // Verify user has access to the device
  if (!userOwnsDevice(userId, deviceId)) {
    throw new Error("You do not have access to this device");
  }

  // Generate a random token
  const token = randomUUID().replace(/-/g, "").toUpperCase();
  const tokenHash = hashToken(token);
  const expiresAt = futureUTC(24, "hours"); // Share links expire in 24 hours
  const id = randomUUID();

  // Delete any existing share tokens for this device from this user
  db.run(
    `DELETE FROM device_share_tokens WHERE device_id = ? AND created_by = ?`,
    [deviceId, userId]
  );

  // Insert new share token
  db.run(
    `INSERT INTO device_share_tokens (id, device_id, created_by, token_hash, expires_at) VALUES (?, ?, ?, ?, ?)`,
    [id, deviceId, userId, tokenHash, expiresAt]
  );

  saveDatabase();

  return { token, expiresAt };
}

/**
 * Verify a share token is valid
 */
export function verifyShareToken(deviceId: string, token: string): boolean {
  const db = getDb();
  const tokenHash = hashToken(token);

  const result = db.exec(
    `SELECT token_hash, expires_at FROM device_share_tokens WHERE device_id = ?`,
    [deviceId]
  );

  if (result.length === 0 || result[0].values.length === 0) {
    return false;
  }

  // Check all tokens (there might be multiple from different users)
  for (const row of result[0].values) {
    const storedHash = row[0] as string;
    const expiresAt = row[1] as string;

    // Check expiration
    if (isExpired(expiresAt)) {
      continue;
    }

    // Constant-time comparison
    try {
      if (
        timingSafeEqual(
          Buffer.from(tokenHash, "hex"),
          Buffer.from(storedHash, "hex")
        )
      ) {
        return true;
      }
    } catch {
      continue;
    }
  }

  return false;
}

/**
 * Claim a device using a share token (user-generated sharing link)
 */
export function claimDeviceWithShareToken(
  deviceId: string,
  token: string,
  userId: string,
  name?: string
): Device {
  // First verify the share token
  if (!verifyShareToken(deviceId, token)) {
    throw new Error("Invalid or expired share link");
  }

  const db = getDb();

  // Check if user already has this device
  const existingUserDevice = db.exec(
    `SELECT user_id FROM user_devices WHERE user_id = ? AND device_id = ?`,
    [userId, deviceId]
  );

  if (
    existingUserDevice.length > 0 &&
    existingUserDevice[0].values.length > 0
  ) {
    throw new Error("Device is already in your account");
  }

  const now = nowUTC();
  const deviceName = name || "My BrewOS";

  // Add user-device association
  db.run(
    `INSERT INTO user_devices (user_id, device_id, name, claimed_at, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?)`,
    [userId, deviceId, deviceName, now, now, now]
  );

  // Note: We don't delete the share token after use - it can be used by multiple people
  // until it expires (24 hours)

  saveDatabase();

  // Return the device
  const deviceResult = db.exec(`SELECT * FROM devices WHERE id = ?`, [
    deviceId,
  ]);
  return resultToObjects<Device>(deviceResult[0])[0];
}

/**
 * Ensure user profile exists (upsert from auth provider)
 * Auto-promotes the first user to admin if no admins exist
 */
export function ensureProfile(
  userId: string,
  email?: string,
  displayName?: string,
  avatarUrl?: string
): void {
  const db = getDb();
  const now = nowUTC();

  // Check if profile exists
  const result = db.exec(`SELECT id FROM profiles WHERE id = ?`, [userId]);
  const isNewUser = result.length === 0 || result[0].values.length === 0;

  if (!isNewUser) {
    // Update existing profile
    db.run(
      `UPDATE profiles SET 
        email = COALESCE(?, email),
        display_name = COALESCE(?, display_name),
        avatar_url = COALESCE(?, avatar_url),
        updated_at = ?
       WHERE id = ?`,
      [email || null, displayName || null, avatarUrl || null, now, userId]
    );
  } else {
    // Check if any admin exists before inserting new user
    const adminCount = db.exec(
      `SELECT COUNT(*) as count FROM profiles WHERE is_admin = 1`
    );
    const hasNoAdmins =
      adminCount.length === 0 ||
      adminCount[0].values.length === 0 ||
      (adminCount[0].values[0][0] as number) === 0;

    // Insert new profile - make admin if first user
    const shouldBeAdmin = hasNoAdmins ? 1 : 0;
    db.run(
      `INSERT INTO profiles (id, email, display_name, avatar_url, is_admin, updated_at) VALUES (?, ?, ?, ?, ?, ?)`,
      [
        userId,
        email || null,
        displayName || null,
        avatarUrl || null,
        shouldBeAdmin,
        now,
      ]
    );

    if (shouldBeAdmin) {
      console.log(`[Auth] First user ${email} auto-promoted to admin`);
    }
  }

  saveDatabase();
}

/**
 * Get user profile by ID
 */
export function getProfile(userId: string): Profile | null {
  const db = getDb();
  const result = db.exec(`SELECT * FROM profiles WHERE id = ?`, [userId]);

  if (result.length === 0 || result[0].values.length === 0) {
    return null;
  }

  return resultToObjects<Profile>(result[0])[0];
}
