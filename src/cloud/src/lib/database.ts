import initSqlJs from "sql.js";
import type { Database as SqlJsDatabase } from "sql.js";
import fs from "fs";
import path from "path";

// Database file path
const DATA_DIR = process.env.DATA_DIR || ".";
const DB_PATH = path.join(DATA_DIR, "brewos.db");

let db: SqlJsDatabase;

// Initialize database
export async function initDatabase(): Promise<SqlJsDatabase> {
  if (db) return db;

  const SQL = await initSqlJs();

  // Try to load existing database
  try {
    if (fs.existsSync(DB_PATH)) {
      const buffer = fs.readFileSync(DB_PATH);
      db = new SQL.Database(buffer);
      console.log(`[DB] Loaded existing database from ${DB_PATH}`);
    } else {
      db = new SQL.Database();
      console.log(`[DB] Created new database`);
    }
  } catch (error) {
    console.error("[DB] Error loading database, creating new one:", error);
    db = new SQL.Database();
  }

  // Initialize schema
  // NOTE: All datetime columns use SQLite's datetime('now') which returns UTC
  // Frontend is responsible for converting to user's local timezone
  db.run(`
    -- Devices table (all timestamps are UTC)
    CREATE TABLE IF NOT EXISTS devices (
      id TEXT PRIMARY KEY,
      owner_id TEXT,
      name TEXT NOT NULL DEFAULT 'My BrewOS',
      device_key_hash TEXT,
      machine_brand TEXT,
      machine_model TEXT,
      firmware_version TEXT,
      hardware_version TEXT,
      machine_type TEXT,
      is_online INTEGER DEFAULT 0,
      last_seen_at TEXT,
      claimed_at TEXT,
      created_at TEXT DEFAULT (datetime('now')),
      updated_at TEXT DEFAULT (datetime('now'))
    );

    -- Claim tokens for QR pairing
    CREATE TABLE IF NOT EXISTS device_claim_tokens (
      id TEXT PRIMARY KEY,
      device_id TEXT UNIQUE NOT NULL,
      token_hash TEXT NOT NULL,
      expires_at TEXT NOT NULL,
      created_at TEXT DEFAULT (datetime('now'))
    );

    -- User profiles (synced from auth provider)
    CREATE TABLE IF NOT EXISTS profiles (
      id TEXT PRIMARY KEY,
      email TEXT,
      display_name TEXT,
      avatar_url TEXT,
      created_at TEXT DEFAULT (datetime('now')),
      updated_at TEXT DEFAULT (datetime('now'))
    );

    -- Push notification subscriptions
    CREATE TABLE IF NOT EXISTS push_subscriptions (
      id TEXT PRIMARY KEY,
      user_id TEXT NOT NULL,
      device_id TEXT,
      endpoint TEXT NOT NULL,
      p256dh TEXT NOT NULL,
      auth TEXT NOT NULL,
      created_at TEXT DEFAULT (datetime('now')),
      updated_at TEXT DEFAULT (datetime('now')),
      FOREIGN KEY (user_id) REFERENCES profiles(id) ON DELETE CASCADE
    );

    -- Notification preferences per user
    CREATE TABLE IF NOT EXISTS notification_preferences (
      id TEXT PRIMARY KEY,
      user_id TEXT NOT NULL UNIQUE,
      machine_ready INTEGER DEFAULT 1,
      water_empty INTEGER DEFAULT 1,
      descale_due INTEGER DEFAULT 1,
      service_due INTEGER DEFAULT 1,
      backflush_due INTEGER DEFAULT 1,
      machine_error INTEGER DEFAULT 1,
      pico_offline INTEGER DEFAULT 1,
      schedule_triggered INTEGER DEFAULT 1,
      brew_complete INTEGER DEFAULT 0,
      created_at TEXT DEFAULT (datetime('now')),
      updated_at TEXT DEFAULT (datetime('now')),
      FOREIGN KEY (user_id) REFERENCES profiles(id) ON DELETE CASCADE
    );

    -- User sessions (our own tokens, not OAuth provider tokens)
    -- This enables: revocation, multiple providers, custom expiry
    CREATE TABLE IF NOT EXISTS sessions (
      id TEXT PRIMARY KEY,
      user_id TEXT NOT NULL,
      access_token_hash TEXT NOT NULL UNIQUE,
      refresh_token_hash TEXT NOT NULL UNIQUE,
      access_expires_at TEXT NOT NULL,
      refresh_expires_at TEXT NOT NULL,
      user_agent TEXT,
      ip_address TEXT,
      created_at TEXT DEFAULT (datetime('now')),
      last_used_at TEXT DEFAULT (datetime('now')),
      FOREIGN KEY (user_id) REFERENCES profiles(id) ON DELETE CASCADE
    );

    -- User-device associations (many-to-many with per-user device names)
    CREATE TABLE IF NOT EXISTS user_devices (
      user_id TEXT NOT NULL,
      device_id TEXT NOT NULL,
      name TEXT NOT NULL DEFAULT 'My BrewOS',
      claimed_at TEXT DEFAULT (datetime('now')),
      created_at TEXT DEFAULT (datetime('now')),
      updated_at TEXT DEFAULT (datetime('now')),
      PRIMARY KEY (user_id, device_id),
      FOREIGN KEY (user_id) REFERENCES profiles(id) ON DELETE CASCADE,
      FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
    );

    -- Share tokens for sharing device access with other users
    CREATE TABLE IF NOT EXISTS device_share_tokens (
      id TEXT PRIMARY KEY,
      device_id TEXT NOT NULL,
      created_by TEXT NOT NULL,
      token_hash TEXT NOT NULL,
      expires_at TEXT NOT NULL,
      created_at TEXT DEFAULT (datetime('now')),
      FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE,
      FOREIGN KEY (created_by) REFERENCES profiles(id) ON DELETE CASCADE
    );
  `);

  // Create indexes
  db.run(`CREATE INDEX IF NOT EXISTS idx_devices_owner ON devices(owner_id)`);
  db.run(
    `CREATE INDEX IF NOT EXISTS idx_claim_tokens_expires ON device_claim_tokens(expires_at)`
  );
  db.run(
    `CREATE INDEX IF NOT EXISTS idx_push_subscriptions_user ON push_subscriptions(user_id)`
  );
  db.run(
    `CREATE INDEX IF NOT EXISTS idx_push_subscriptions_device ON push_subscriptions(device_id)`
  );
  db.run(
    `CREATE INDEX IF NOT EXISTS idx_notification_preferences_user ON notification_preferences(user_id)`
  );
  db.run(`CREATE INDEX IF NOT EXISTS idx_sessions_user ON sessions(user_id)`);
  db.run(
    `CREATE INDEX IF NOT EXISTS idx_sessions_access_token ON sessions(access_token_hash)`
  );
  db.run(
    `CREATE INDEX IF NOT EXISTS idx_sessions_refresh_token ON sessions(refresh_token_hash)`
  );
  db.run(
    `CREATE INDEX IF NOT EXISTS idx_sessions_expires ON sessions(refresh_expires_at)`
  );
  db.run(
    `CREATE INDEX IF NOT EXISTS idx_user_devices_user ON user_devices(user_id)`
  );
  db.run(
    `CREATE INDEX IF NOT EXISTS idx_user_devices_device ON user_devices(device_id)`
  );
  db.run(
    `CREATE INDEX IF NOT EXISTS idx_share_tokens_device ON device_share_tokens(device_id)`
  );
  db.run(
    `CREATE INDEX IF NOT EXISTS idx_share_tokens_expires ON device_share_tokens(expires_at)`
  );

  // Migrations for existing databases
  // Add machine_brand and machine_model columns if they don't exist
  try {
    db.run(`ALTER TABLE devices ADD COLUMN machine_brand TEXT`);
  } catch {
    // Column already exists
  }
  try {
    db.run(`ALTER TABLE devices ADD COLUMN machine_model TEXT`);
  } catch {
    // Column already exists
  }
  // Add device_key_hash column for device authentication
  try {
    db.run(`ALTER TABLE devices ADD COLUMN device_key_hash TEXT`);
  } catch {
    // Column already exists
  }

  // Add is_admin column to profiles for admin role management
  try {
    db.run(`ALTER TABLE profiles ADD COLUMN is_admin INTEGER DEFAULT 0`);
    console.log("[DB] Added is_admin column to profiles table");
  } catch {
    // Column already exists
  }

  // Create index for admin lookups
  db.run(
    `CREATE INDEX IF NOT EXISTS idx_profiles_admin ON profiles(is_admin)`
  );

  // Ensure at least one admin exists - promote first user if needed
  try {
    const adminCount = db.exec(
      `SELECT COUNT(*) FROM profiles WHERE is_admin = 1`
    );
    const hasNoAdmins =
      adminCount.length === 0 ||
      adminCount[0].values.length === 0 ||
      (adminCount[0].values[0][0] as number) === 0;

    if (hasNoAdmins) {
      // Get the first user by creation date
      const firstUser = db.exec(
        `SELECT id, email FROM profiles ORDER BY created_at ASC LIMIT 1`
      );
      if (firstUser.length > 0 && firstUser[0].values.length > 0) {
        const userId = firstUser[0].values[0][0] as string;
        const email = firstUser[0].values[0][1] as string;
        db.run(`UPDATE profiles SET is_admin = 1 WHERE id = ?`, [userId]);
        console.log(`[DB] Auto-promoted first user ${email} to admin`);
        saveDatabase();
      }
    }
  } catch (error) {
    console.error("[DB] Error checking/promoting admin:", error);
  }

  // Migration: Migrate existing owner_id relationships to user_devices table
  // This preserves existing data when upgrading to the new schema
  try {
    const migrationCheck = db.exec(
      `SELECT COUNT(*) as count FROM sqlite_master WHERE type='table' AND name='user_devices'`
    );
    const hasUserDevicesTable =
      migrationCheck.length > 0 &&
      migrationCheck[0].values.length > 0 &&
      (migrationCheck[0].values[0][0] as number) > 0;

    if (hasUserDevicesTable) {
      // Check if migration is needed (devices with owner_id but no user_devices entry)
      const needsMigration = db.exec(`
        SELECT COUNT(*) as count 
        FROM devices d 
        WHERE d.owner_id IS NOT NULL 
        AND NOT EXISTS (
          SELECT 1 FROM user_devices ud 
          WHERE ud.device_id = d.id AND ud.user_id = d.owner_id
        )
      `);

      if (
        needsMigration.length > 0 &&
        needsMigration[0].values.length > 0 &&
        (needsMigration[0].values[0][0] as number) > 0
      ) {
        console.log(
          "[DB] Migrating existing device ownership to user_devices..."
        );
        // Migrate existing owner_id + name to user_devices
        db.run(`
          INSERT INTO user_devices (user_id, device_id, name, claimed_at, created_at, updated_at)
          SELECT owner_id, id, name, claimed_at, created_at, updated_at
          FROM devices
          WHERE owner_id IS NOT NULL
          AND NOT EXISTS (
            SELECT 1 FROM user_devices ud 
            WHERE ud.device_id = devices.id AND ud.user_id = devices.owner_id
          )
        `);
        console.log("[DB] Migration completed successfully");
        saveDatabase();
      }
    }
  } catch (error) {
    console.error("[DB] Error during migration:", error);
    // Don't fail initialization if migration fails
  }

  // Save initial database
  saveDatabase();

  return db;
}

// Save database to disk
export function saveDatabase(): void {
  if (!db) return;

  try {
    // Ensure directory exists
    const dir = path.dirname(DB_PATH);
    if (!fs.existsSync(dir)) {
      fs.mkdirSync(dir, { recursive: true });
    }

    const data = db.export();
    const buffer = Buffer.from(data);
    fs.writeFileSync(DB_PATH, buffer);
  } catch (error) {
    console.error("[DB] Error saving database:", error);
  }
}

// Get database instance
export function getDb(): SqlJsDatabase {
  if (!db) {
    throw new Error("Database not initialized. Call initDatabase() first.");
  }
  return db;
}

// Types
export interface Device {
  id: string;
  owner_id: string | null; // Deprecated: kept for backward compatibility, use user_devices instead
  name: string; // Deprecated: kept for backward compatibility, use user_devices.name instead
  device_key_hash: string | null; // Hashed device key for authentication
  machine_brand: string | null;
  machine_model: string | null;
  firmware_version: string | null;
  hardware_version: string | null;
  machine_type: string | null;
  is_online: number;
  last_seen_at: string | null;
  claimed_at: string | null; // Deprecated: kept for backward compatibility
  created_at: string;
  updated_at: string;
}

export interface UserDevice {
  user_id: string;
  device_id: string;
  name: string;
  claimed_at: string;
  created_at: string;
  updated_at: string;
}

export interface ClaimToken {
  id: string;
  device_id: string;
  token_hash: string;
  expires_at: string;
  created_at: string;
}

export interface ShareToken {
  id: string;
  device_id: string;
  created_by: string;
  token_hash: string;
  expires_at: string;
  created_at: string;
}

export interface Profile {
  id: string;
  email: string | null;
  display_name: string | null;
  avatar_url: string | null;
  is_admin: number; // 0 = not admin, 1 = admin
  created_at: string;
  updated_at: string;
}

export interface PushSubscription {
  id: string;
  user_id: string;
  device_id: string | null;
  endpoint: string;
  p256dh: string;
  auth: string;
  created_at: string;
  updated_at: string;
}

export interface NotificationPreferences {
  id: string;
  user_id: string;
  machine_ready: number;
  water_empty: number;
  descale_due: number;
  service_due: number;
  backflush_due: number;
  machine_error: number;
  pico_offline: number;
  schedule_triggered: number;
  brew_complete: number;
  created_at: string;
  updated_at: string;
}

// Notification type enum matching the database columns
export type NotificationType =
  | "MACHINE_READY"
  | "WATER_EMPTY"
  | "DESCALE_DUE"
  | "SERVICE_DUE"
  | "BACKFLUSH_DUE"
  | "MACHINE_ERROR"
  | "PICO_OFFLINE"
  | "SCHEDULE_TRIGGERED"
  | "BREW_COMPLETE";

// Helper to convert sql.js results to objects
export function rowToObject<T>(columns: string[], values: unknown[]): T {
  const obj: Record<string, unknown> = {};
  columns.forEach((col, i) => {
    obj[col] = values[i];
  });
  return obj as T;
}

export function resultToObjects<T>(result: {
  columns: string[];
  values: unknown[][];
}): T[] {
  return result.values.map((values) => rowToObject<T>(result.columns, values));
}

// Auto-save every 30 seconds
setInterval(() => {
  saveDatabase();
}, 30 * 1000);

// Save on exit
process.on("exit", () => {
  saveDatabase();
});

process.on("SIGINT", () => {
  saveDatabase();
  process.exit(0);
});

process.on("SIGTERM", () => {
  saveDatabase();
  process.exit(0);
});
