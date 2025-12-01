import initSqlJs from 'sql.js';
import type { Database as SqlJsDatabase } from 'sql.js';
import fs from 'fs';
import path from 'path';

// Database file path
const DATA_DIR = process.env.DATA_DIR || '.';
const DB_PATH = path.join(DATA_DIR, 'brewos.db');

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
    console.error('[DB] Error loading database, creating new one:', error);
    db = new SQL.Database();
  }

  // Initialize schema
  db.run(`
    -- Devices table
    CREATE TABLE IF NOT EXISTS devices (
      id TEXT PRIMARY KEY,
      owner_id TEXT,
      name TEXT NOT NULL DEFAULT 'My BrewOS',
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
  `);

  // Create indexes
  db.run(`CREATE INDEX IF NOT EXISTS idx_devices_owner ON devices(owner_id)`);
  db.run(`CREATE INDEX IF NOT EXISTS idx_claim_tokens_expires ON device_claim_tokens(expires_at)`);
  db.run(`CREATE INDEX IF NOT EXISTS idx_push_subscriptions_user ON push_subscriptions(user_id)`);
  db.run(`CREATE INDEX IF NOT EXISTS idx_push_subscriptions_device ON push_subscriptions(device_id)`);

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
    console.error('[DB] Error saving database:', error);
  }
}

// Get database instance
export function getDb(): SqlJsDatabase {
  if (!db) {
    throw new Error('Database not initialized. Call initDatabase() first.');
  }
  return db;
}

// Types
export interface Device {
  id: string;
  owner_id: string | null;
  name: string;
  machine_brand: string | null;
  machine_model: string | null;
  firmware_version: string | null;
  hardware_version: string | null;
  machine_type: string | null;
  is_online: number;
  last_seen_at: string | null;
  claimed_at: string | null;
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

export interface Profile {
  id: string;
  email: string | null;
  display_name: string | null;
  avatar_url: string | null;
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

// Helper to convert sql.js results to objects
export function rowToObject<T>(columns: string[], values: unknown[]): T {
  const obj: Record<string, unknown> = {};
  columns.forEach((col, i) => {
    obj[col] = values[i];
  });
  return obj as T;
}

export function resultToObjects<T>(result: { columns: string[]; values: unknown[][] }): T[] {
  return result.values.map(values => rowToObject<T>(result.columns, values));
}

// Auto-save every 30 seconds
setInterval(() => {
  saveDatabase();
}, 30 * 1000);

// Save on exit
process.on('exit', () => {
  saveDatabase();
});

process.on('SIGINT', () => {
  saveDatabase();
  process.exit(0);
});

process.on('SIGTERM', () => {
  saveDatabase();
  process.exit(0);
});
