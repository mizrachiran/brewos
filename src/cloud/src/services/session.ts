/**
 * Session Management Service
 *
 * Handles user sessions with secure token generation, refresh token rotation,
 * and automatic cleanup of expired sessions.
 *
 * Architecture:
 * - OAuth providers (Google, etc.) verify identity ONCE
 * - We issue our own session tokens with configurable expiry
 * - Refresh tokens allow silent session renewal
 * - Sessions can be revoked server-side at any time
 *
 * Performance:
 * - LRU cache for session verification (no DB query on cache hit)
 * - Batched "last_used_at" updates to minimize DB writes
 * - Cache invalidation on revocation/refresh for security
 */

import { randomBytes, createHash } from "crypto";
import { LRUCache } from "lru-cache";
import { getDb, saveDatabase, rowToObject } from "../lib/database.js";
import { nowUTC, futureUTC, isExpired } from "../lib/date.js";

// =============================================================================
// Configuration
// =============================================================================

const ACCESS_TOKEN_EXPIRY_MINUTES = 60; // 1 hour
const REFRESH_TOKEN_EXPIRY_DAYS = 30; // 30 days
const TOKEN_BYTES = 32; // 256-bit tokens

// Cache configuration
const CACHE_MAX_SIZE = 10000; // Max cached sessions
const CACHE_TTL_MS = 5 * 60 * 1000; // 5 minutes (shorter than token expiry for security)

// Batch update configuration
const LAST_USED_UPDATE_INTERVAL_MS = 60 * 1000; // Update DB every 60 seconds
const LAST_USED_BATCH_SIZE = 100; // Max sessions to update per batch

// =============================================================================
// Types
// =============================================================================

export interface Session {
  id: string;
  user_id: string;
  access_token_hash: string;
  refresh_token_hash: string;
  access_expires_at: string;
  refresh_expires_at: string;
  user_agent: string | null;
  ip_address: string | null;
  created_at: string;
  last_used_at: string;
}

export interface SessionUser {
  id: string;
  email: string;
  displayName: string | null;
  avatarUrl: string | null;
}

export interface TokenPair {
  accessToken: string;
  refreshToken: string;
  accessExpiresAt: number; // Unix timestamp (ms)
  refreshExpiresAt: number;
}

export interface SessionWithUser {
  session: Session;
  user: SessionUser;
}

// =============================================================================
// Cache & Batch Update State
// =============================================================================

/**
 * LRU Cache for verified sessions
 * Key: access_token_hash
 * Value: SessionWithUser
 */
const sessionCache = new LRUCache<string, SessionWithUser>({
  max: CACHE_MAX_SIZE,
  ttl: CACHE_TTL_MS,
  updateAgeOnGet: true, // Extend TTL on access
});

/**
 * Track sessions that need "last_used_at" updates
 * Key: session_id
 * Value: timestamp of last use
 */
const pendingLastUsedUpdates = new Map<string, string>();

/**
 * Batch update interval handle
 */
let batchUpdateInterval: NodeJS.Timeout | null = null;

// =============================================================================
// Cache Statistics (for monitoring)
// =============================================================================

let cacheHits = 0;
let cacheMisses = 0;

export function getCacheStats() {
  return {
    hits: cacheHits,
    misses: cacheMisses,
    hitRate:
      cacheHits + cacheMisses > 0
        ? ((cacheHits / (cacheHits + cacheMisses)) * 100).toFixed(1) + "%"
        : "0%",
    size: sessionCache.size,
    maxSize: CACHE_MAX_SIZE,
    pendingUpdates: pendingLastUsedUpdates.size,
  };
}

// =============================================================================
// Token Utilities
// =============================================================================

/**
 * Generate a cryptographically secure token
 */
function generateToken(): string {
  return randomBytes(TOKEN_BYTES).toString("base64url");
}

/**
 * Hash a token for storage (never store plain tokens!)
 */
function hashToken(token: string): string {
  return createHash("sha256").update(token).digest("hex");
}


// =============================================================================
// Batch Update System
// =============================================================================

/**
 * Flush pending "last_used_at" updates to database
 */
function flushLastUsedUpdates(): void {
  if (pendingLastUsedUpdates.size === 0) return;

  const db = getDb();
  const updates = Array.from(pendingLastUsedUpdates.entries()).slice(
    0,
    LAST_USED_BATCH_SIZE
  );

  for (const [sessionId, timestamp] of updates) {
    db.run(`UPDATE sessions SET last_used_at = ? WHERE id = ?`, [
      timestamp,
      sessionId,
    ]);
    pendingLastUsedUpdates.delete(sessionId);
  }

  if (updates.length > 0) {
    saveDatabase();
    console.log(
      `[Session] Batch updated last_used_at for ${updates.length} sessions`
    );
  }
}

/**
 * Start the batch update interval
 */
export function startBatchUpdateInterval(): void {
  if (batchUpdateInterval) return;

  batchUpdateInterval = setInterval(
    flushLastUsedUpdates,
    LAST_USED_UPDATE_INTERVAL_MS
  );
  console.log(
    `[Session] Started batch update interval (${LAST_USED_UPDATE_INTERVAL_MS}ms)`
  );
}

/**
 * Stop the batch update interval and flush pending updates
 */
export function stopBatchUpdateInterval(): void {
  if (batchUpdateInterval) {
    clearInterval(batchUpdateInterval);
    batchUpdateInterval = null;
  }
  flushLastUsedUpdates();
  console.log("[Session] Stopped batch update interval");
}

// =============================================================================
// Session Management
// =============================================================================

/**
 * Create a new session for a user
 * Called after successful OAuth authentication
 */
export function createSession(
  userId: string,
  metadata?: { userAgent?: string; ipAddress?: string }
): TokenPair {
  const db = getDb();
  const now = nowUTC();

  // Generate secure tokens
  const accessToken = generateToken();
  const refreshToken = generateToken();

  // Calculate expiry times
  const accessExpiresAt = futureUTC(ACCESS_TOKEN_EXPIRY_MINUTES, "minutes");
  const refreshExpiresAt = futureUTC(REFRESH_TOKEN_EXPIRY_DAYS, "days");

  // Generate session ID
  const sessionId = randomBytes(16).toString("hex");

  // Hash tokens for storage
  const accessHash = hashToken(accessToken);
  const refreshHash = hashToken(refreshToken);

  // Store session with hashed tokens
  db.run(
    `INSERT INTO sessions (
      id, user_id, access_token_hash, refresh_token_hash,
      access_expires_at, refresh_expires_at,
      user_agent, ip_address, created_at, last_used_at
    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
    [
      sessionId,
      userId,
      accessHash,
      refreshHash,
      accessExpiresAt,
      refreshExpiresAt,
      metadata?.userAgent || null,
      metadata?.ipAddress || null,
      now,
      now,
    ]
  );

  saveDatabase();

  console.log(`[Session] Created session ${sessionId} for user ${userId}`);

  return {
    accessToken,
    refreshToken,
    accessExpiresAt: new Date(accessExpiresAt).getTime(),
    refreshExpiresAt: new Date(refreshExpiresAt).getTime(),
  };
}

/**
 * Verify an access token and return session + user info
 * Uses LRU cache to minimize database queries
 */
export function verifyAccessToken(accessToken: string): SessionWithUser | null {
  const accessHash = hashToken(accessToken);

  // Check cache first
  const cached = sessionCache.get(accessHash);
  if (cached) {
    cacheHits++;

    // Check if cached session is still valid (not expired)
    if (!isExpired(cached.session.access_expires_at)) {
      // Queue last_used_at update (batched, not immediate)
      const now = nowUTC();
      pendingLastUsedUpdates.set(cached.session.id, now);

      return cached;
    }

    // Cached session expired, remove from cache
    sessionCache.delete(accessHash);
  }

  cacheMisses++;

  // Cache miss - query database
  const db = getDb();
  const result = db.exec(
    `SELECT s.*, p.email, p.display_name, p.avatar_url
     FROM sessions s
     JOIN profiles p ON s.user_id = p.id
     WHERE s.access_token_hash = ?`,
    [accessHash]
  );

  if (result.length === 0 || result[0].values.length === 0) {
    return null;
  }

  const row = result[0].values[0];
  const columns = result[0].columns;

  // Map to object
  const data: Record<string, unknown> = {};
  columns.forEach((col, i) => {
    data[col] = row[i];
  });

  // Check if access token is expired
  if (isExpired(data.access_expires_at as string)) {
    return null;
  }

  const now = nowUTC();

  const sessionWithUser: SessionWithUser = {
    session: {
      id: data.id as string,
      user_id: data.user_id as string,
      access_token_hash: data.access_token_hash as string,
      refresh_token_hash: data.refresh_token_hash as string,
      access_expires_at: data.access_expires_at as string,
      refresh_expires_at: data.refresh_expires_at as string,
      user_agent: data.user_agent as string | null,
      ip_address: data.ip_address as string | null,
      created_at: data.created_at as string,
      last_used_at: now,
    },
    user: {
      id: data.user_id as string,
      email: data.email as string,
      displayName: data.display_name as string | null,
      avatarUrl: data.avatar_url as string | null,
    },
  };

  // Store in cache
  sessionCache.set(accessHash, sessionWithUser);

  // Queue last_used_at update
  pendingLastUsedUpdates.set(sessionWithUser.session.id, now);

  return sessionWithUser;
}

/**
 * Refresh a session using a refresh token
 * Implements refresh token rotation for security
 */
export function refreshSession(refreshToken: string): TokenPair | null {
  const db = getDb();
  const refreshHash = hashToken(refreshToken);

  // Find session by refresh token hash
  const result = db.exec(
    `SELECT * FROM sessions WHERE refresh_token_hash = ?`,
    [refreshHash]
  );

  if (result.length === 0 || result[0].values.length === 0) {
    return null;
  }

  const session = rowToObject<Session>(result[0].columns, result[0].values[0]);

  // Check if refresh token is expired
  if (isExpired(session.refresh_expires_at)) {
    // Delete expired session
    db.run(`DELETE FROM sessions WHERE id = ?`, [session.id]);
    saveDatabase();
    return null;
  }

  // Invalidate old access token in cache
  sessionCache.delete(session.access_token_hash);

  // Generate new tokens (refresh token rotation)
  const newAccessToken = generateToken();
  const newRefreshToken = generateToken();
  const now = nowUTC();
  const accessExpiresAt = futureUTC(ACCESS_TOKEN_EXPIRY_MINUTES, "minutes");
  const refreshExpiresAt = futureUTC(REFRESH_TOKEN_EXPIRY_DAYS, "days");

  const newAccessHash = hashToken(newAccessToken);
  const newRefreshHash = hashToken(newRefreshToken);

  // Update session with new tokens
  db.run(
    `UPDATE sessions SET
      access_token_hash = ?,
      refresh_token_hash = ?,
      access_expires_at = ?,
      refresh_expires_at = ?,
      last_used_at = ?
     WHERE id = ?`,
    [
      newAccessHash,
      newRefreshHash,
      accessExpiresAt,
      refreshExpiresAt,
      now,
      session.id,
    ]
  );

  saveDatabase();

  // Remove from pending updates (we just updated it)
  pendingLastUsedUpdates.delete(session.id);

  console.log(`[Session] Refreshed session ${session.id}`);

  return {
    accessToken: newAccessToken,
    refreshToken: newRefreshToken,
    accessExpiresAt: new Date(accessExpiresAt).getTime(),
    refreshExpiresAt: new Date(refreshExpiresAt).getTime(),
  };
}

/**
 * Revoke a specific session (logout)
 */
export function revokeSession(sessionId: string): boolean {
  const db = getDb();

  // Get session to find access token hash for cache invalidation
  const result = db.exec(
    `SELECT access_token_hash FROM sessions WHERE id = ?`,
    [sessionId]
  );

  if (result.length > 0 && result[0].values.length > 0) {
    const accessHash = result[0].values[0][0] as string;
    sessionCache.delete(accessHash);
  }

  db.run(`DELETE FROM sessions WHERE id = ?`, [sessionId]);
  const deleted = db.getRowsModified();

  if (deleted > 0) {
    saveDatabase();
    pendingLastUsedUpdates.delete(sessionId);
    console.log(`[Session] Revoked session ${sessionId}`);
  }

  return deleted > 0;
}

/**
 * Revoke all sessions for a user (logout everywhere)
 */
export function revokeAllUserSessions(userId: string): number {
  const db = getDb();

  // Get all access token hashes for cache invalidation
  const result = db.exec(
    `SELECT id, access_token_hash FROM sessions WHERE user_id = ?`,
    [userId]
  );

  if (result.length > 0) {
    for (const row of result[0].values) {
      const sessionId = row[0] as string;
      const accessHash = row[1] as string;
      sessionCache.delete(accessHash);
      pendingLastUsedUpdates.delete(sessionId);
    }
  }

  db.run(`DELETE FROM sessions WHERE user_id = ?`, [userId]);
  const deleted = db.getRowsModified();

  if (deleted > 0) {
    saveDatabase();
    console.log(`[Session] Revoked ${deleted} sessions for user ${userId}`);
  }

  return deleted;
}

/**
 * Get all active sessions for a user
 */
export function getUserSessions(userId: string): Session[] {
  const db = getDb();
  const now = nowUTC();

  const result = db.exec(
    `SELECT * FROM sessions 
     WHERE user_id = ? AND refresh_expires_at > ?
     ORDER BY last_used_at DESC`,
    [userId, now]
  );

  if (result.length === 0) {
    return [];
  }

  return result[0].values.map((row) =>
    rowToObject<Session>(result[0].columns, row)
  );
}

/**
 * Cleanup expired sessions
 * Should be called periodically (e.g., every hour)
 */
export function cleanupExpiredSessions(): number {
  const db = getDb();
  const now = nowUTC();

  // Get expired sessions for cache invalidation
  const expiredResult = db.exec(
    `SELECT id, access_token_hash FROM sessions WHERE refresh_expires_at < ?`,
    [now]
  );

  if (expiredResult.length > 0) {
    for (const row of expiredResult[0].values) {
      const sessionId = row[0] as string;
      const accessHash = row[1] as string;
      sessionCache.delete(accessHash);
      pendingLastUsedUpdates.delete(sessionId);
    }
  }

  db.run(`DELETE FROM sessions WHERE refresh_expires_at < ?`, [now]);
  const deleted = db.getRowsModified();

  if (deleted > 0) {
    saveDatabase();
    console.log(`[Session] Cleaned up ${deleted} expired sessions`);
  }

  return deleted;
}

/**
 * Get session by ID (for WebSocket auth)
 */
export function getSessionById(sessionId: string): Session | null {
  const db = getDb();

  const result = db.exec(`SELECT * FROM sessions WHERE id = ?`, [sessionId]);

  if (result.length === 0 || result[0].values.length === 0) {
    return null;
  }

  return rowToObject<Session>(result[0].columns, result[0].values[0]);
}

/**
 * Clear all caches (useful for testing)
 */
export function clearCaches(): void {
  sessionCache.clear();
  pendingLastUsedUpdates.clear();
  cacheHits = 0;
  cacheMisses = 0;
  console.log("[Session] Cleared all caches");
}
