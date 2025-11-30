import { createHash, randomUUID, timingSafeEqual } from 'crypto';
import { getDb, saveDatabase, Device, resultToObjects } from '../lib/database.js';

/**
 * Generate a hash for token comparison
 */
function hashToken(token: string): string {
  return createHash('sha256').update(token).digest('hex');
}

/**
 * Create or update a claim token for a device
 * Called by ESP32 when generating QR code
 */
export function createClaimToken(deviceId: string, token: string): void {
  const db = getDb();
  const tokenHash = hashToken(token);
  const expiresAt = new Date(Date.now() + 10 * 60 * 1000).toISOString(); // 10 minutes
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

  // Check expiration
  if (new Date(expiresAt) < new Date()) {
    return false;
  }

  // Constant-time comparison
  try {
    return timingSafeEqual(
      Buffer.from(tokenHash, 'hex'),
      Buffer.from(storedHash, 'hex')
    );
  } catch {
    return false;
  }
}

/**
 * Claim a device for a user
 */
export function claimDevice(
  deviceId: string,
  userId: string,
  name?: string
): Device {
  const db = getDb();

  // Check if device exists and is already claimed
  const existingResult = db.exec(
    `SELECT id, owner_id FROM devices WHERE id = ?`,
    [deviceId]
  );

  if (existingResult.length > 0 && existingResult[0].values.length > 0) {
    const ownerId = existingResult[0].values[0][1];
    if (ownerId) {
      throw new Error('Device is already claimed');
    }
  }

  const now = new Date().toISOString();
  const deviceName = name || 'My BrewOS';

  // Check if device exists
  if (existingResult.length > 0 && existingResult[0].values.length > 0) {
    // Update existing device
    db.run(
      `UPDATE devices SET owner_id = ?, name = ?, claimed_at = ?, updated_at = ? WHERE id = ?`,
      [userId, deviceName, now, now, deviceId]
    );
  } else {
    // Insert new device
    db.run(
      `INSERT INTO devices (id, owner_id, name, claimed_at, updated_at) VALUES (?, ?, ?, ?, ?)`,
      [deviceId, userId, deviceName, now, now]
    );
  }

  // Delete the claim token
  db.run(`DELETE FROM device_claim_tokens WHERE device_id = ?`, [deviceId]);

  saveDatabase();

  // Return the device
  const deviceResult = db.exec(`SELECT * FROM devices WHERE id = ?`, [deviceId]);
  return resultToObjects<Device>(deviceResult[0])[0];
}

/**
 * Get devices for a user
 */
export function getUserDevices(userId: string): Device[] {
  const db = getDb();
  const result = db.exec(
    `SELECT * FROM devices WHERE owner_id = ? ORDER BY created_at DESC`,
    [userId]
  );

  if (result.length === 0) {
    return [];
  }

  return resultToObjects<Device>(result[0]);
}

/**
 * Get a single device by ID
 */
export function getDevice(deviceId: string): Device | null {
  const db = getDb();
  const result = db.exec(`SELECT * FROM devices WHERE id = ?`, [deviceId]);

  if (result.length === 0 || result[0].values.length === 0) {
    return null;
  }

  return resultToObjects<Device>(result[0])[0];
}

/**
 * Check if user owns a device
 */
export function userOwnsDevice(userId: string, deviceId: string): boolean {
  const db = getDb();
  const result = db.exec(
    `SELECT id FROM devices WHERE id = ? AND owner_id = ?`,
    [deviceId, userId]
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
  const now = new Date().toISOString();

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
 */
export function removeDevice(deviceId: string, userId: string): void {
  const db = getDb();
  const now = new Date().toISOString();

  db.run(
    `UPDATE devices SET owner_id = NULL, claimed_at = NULL, updated_at = ? WHERE id = ? AND owner_id = ?`,
    [now, deviceId, userId]
  );

  if (db.getRowsModified() === 0) {
    throw new Error('Failed to remove device');
  }

  saveDatabase();
}

/**
 * Rename a device
 */
export function renameDevice(
  deviceId: string,
  userId: string,
  name: string
): void {
  const db = getDb();
  const now = new Date().toISOString();

  db.run(
    `UPDATE devices SET name = ?, updated_at = ? WHERE id = ? AND owner_id = ?`,
    [name, now, deviceId, userId]
  );

  if (db.getRowsModified() === 0) {
    throw new Error('Failed to rename device');
  }

  saveDatabase();
}

/**
 * Cleanup expired claim tokens
 */
export function cleanupExpiredTokens(): number {
  const db = getDb();
  db.run(`DELETE FROM device_claim_tokens WHERE expires_at < datetime('now')`);
  const deleted = db.getRowsModified();
  
  if (deleted > 0) {
    saveDatabase();
  }
  
  return deleted;
}

/**
 * Ensure user profile exists (upsert from auth provider)
 */
export function ensureProfile(
  userId: string,
  email?: string,
  displayName?: string,
  avatarUrl?: string
): void {
  const db = getDb();
  const now = new Date().toISOString();

  // Check if profile exists
  const result = db.exec(`SELECT id FROM profiles WHERE id = ?`, [userId]);

  if (result.length > 0 && result[0].values.length > 0) {
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
    // Insert new profile
    db.run(
      `INSERT INTO profiles (id, email, display_name, avatar_url, updated_at) VALUES (?, ?, ?, ?, ?)`,
      [userId, email || null, displayName || null, avatarUrl || null, now]
    );
  }
}
