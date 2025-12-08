# Cloud Service Database Storage

## Overview

The BrewOS cloud service uses **SQLite** (via sql.js) for data storage. All data is stored in a single file: `brewos.db` in the `DATA_DIR` directory.

**Important:** The cloud service does **NOT** store machine state, statistics, or brewing data. It only stores:

- User accounts and authentication
- Device ownership and sharing
- Push notification subscriptions
- Session tokens

All machine data (temperatures, pressure, shots, statistics) remains on the ESP32 and is only relayed through the cloud service in real-time via WebSocket.

---

## Database Schema

### Tables

#### 1. `devices` - Device Metadata

**Purpose:** Shared device information (not user-specific)

| Column             | Type             | Description                                      |
| ------------------ | ---------------- | ------------------------------------------------ |
| `id`               | TEXT PRIMARY KEY | Device ID (e.g., "BRW-A1B2C3D4")                 |
| `owner_id`         | TEXT             | **Deprecated** - kept for backward compatibility |
| `name`             | TEXT             | **Deprecated** - kept for backward compatibility |
| `machine_brand`    | TEXT             | Machine brand (e.g., "ECM")                      |
| `machine_model`    | TEXT             | Machine model (e.g., "Synchronika")              |
| `firmware_version` | TEXT             | ESP32 firmware version                           |
| `hardware_version` | TEXT             | Hardware version                                 |
| `machine_type`     | TEXT             | Machine type (dual_boiler, single_boiler, etc.)  |
| `is_online`        | INTEGER          | 0 = offline, 1 = online                          |
| `last_seen_at`     | TEXT             | Last connection timestamp (UTC)                  |
| `claimed_at`       | TEXT             | **Deprecated** - kept for backward compatibility |
| `created_at`       | TEXT             | Device creation timestamp (UTC)                  |
| `updated_at`       | TEXT             | Last update timestamp (UTC)                      |

**Access Frequency:**

- **Read:** Every time a user lists their devices or views device details
- **Write:**
  - When device connects/disconnects: **Every connection/disconnection** (via `updateDeviceStatus`)
  - When device metadata is updated: **On user action** (name, brand, model changes)
  - When device is first registered: **Once per device**

**Update Pattern:**

- `is_online` and `last_seen_at` are updated **every time a device connects or disconnects**
- `firmware_version` is updated when device connects with version info
- `machine_brand` and `machine_model` are updated when user edits device info

---

#### 2. `user_devices` - User-Device Associations

**Purpose:** Many-to-many relationship between users and devices (enables sharing)

| Column       | Type                     | Description                                        |
| ------------ | ------------------------ | -------------------------------------------------- |
| `user_id`    | TEXT                     | User ID (from OAuth provider)                      |
| `device_id`  | TEXT                     | Device ID                                          |
| `name`       | TEXT                     | **Per-user device name** (e.g., "Kitchen Machine") |
| `claimed_at` | TEXT                     | When this user claimed the device (UTC)            |
| `created_at` | TEXT                     | Record creation timestamp (UTC)                    |
| `updated_at` | TEXT                     | Last update timestamp (UTC)                        |
| PRIMARY KEY  | (`user_id`, `device_id`) | Composite primary key                              |

**Access Frequency:**

- **Read:** Every time a user lists their devices (joins with `devices` table)
- **Write:**
  - When user claims a device: **Once per claim**
  - When user renames device: **On user action**
  - When user removes device: **On user action**
  - When user access is revoked: **On user action**

**Key Feature:** Each user can have a different name for the same device. For example:

- User A: "Kitchen Machine"
- User B: "Office Espresso"

---

#### 3. `profiles` - User Profiles

**Purpose:** User account information synced from OAuth provider

| Column         | Type             | Description                                     |
| -------------- | ---------------- | ----------------------------------------------- |
| `id`           | TEXT PRIMARY KEY | User ID (from OAuth provider, e.g., Google sub) |
| `email`        | TEXT             | User email address                              |
| `display_name` | TEXT             | User's display name                             |
| `avatar_url`   | TEXT             | Profile picture URL                             |
| `created_at`   | TEXT             | Account creation timestamp (UTC)                |
| `updated_at`   | TEXT             | Last profile update timestamp (UTC)             |

**Access Frequency:**

- **Read:**
  - On every authenticated API request (to get user info)
  - When listing device users
- **Write:**
  - On first OAuth login: **Once per new user**
  - On subsequent logins: **When profile info changes** (rare, only if user updates Google profile)

**Update Pattern:** Profile is updated (upsert) on every OAuth login to sync latest info from provider.

---

#### 4. `sessions` - User Sessions

**Purpose:** Session tokens for authenticated access (not OAuth tokens)

| Column               | Type             | Description                       |
| -------------------- | ---------------- | --------------------------------- |
| `id`                 | TEXT PRIMARY KEY | Session ID                        |
| `user_id`            | TEXT             | User ID (foreign key to profiles) |
| `access_token_hash`  | TEXT UNIQUE      | SHA-256 hash of access token      |
| `refresh_token_hash` | TEXT UNIQUE      | SHA-256 hash of refresh token     |
| `access_expires_at`  | TEXT             | Access token expiration (UTC)     |
| `refresh_expires_at` | TEXT             | Refresh token expiration (UTC)    |
| `user_agent`         | TEXT             | Browser/client user agent         |
| `ip_address`         | TEXT             | Client IP address                 |
| `created_at`         | TEXT             | Session creation timestamp (UTC)  |
| `last_used_at`       | TEXT             | Last token usage timestamp (UTC)  |

**Access Frequency:**

- **Read:**
  - **Every authenticated API request** (to verify access token)
  - **Every WebSocket connection** (to verify session)
  - When listing user sessions: **On user action**
- **Write:**
  - On login: **Once per login**
  - On token refresh: **Every refresh** (creates new session, deletes old)
  - On logout: **On user action**
  - `last_used_at` updates: **Batched every 60 seconds** (not on every request)

**Optimization:** `last_used_at` is updated in batches every 60 seconds to minimize database writes. Individual session access updates are queued in memory and flushed periodically.

**Session Lifecycle:**

- Access token: 60 minutes
- Refresh token: 30 days
- Expired sessions are cleaned up periodically

---

#### 5. `device_claim_tokens` - QR Pairing Tokens

**Purpose:** Short-lived tokens for ESP32 QR code pairing

| Column       | Type             | Description                        |
| ------------ | ---------------- | ---------------------------------- |
| `id`         | TEXT PRIMARY KEY | Token ID                           |
| `device_id`  | TEXT UNIQUE      | Device ID                          |
| `token_hash` | TEXT             | SHA-256 hash of token              |
| `expires_at` | TEXT             | Token expiration (UTC, 10 minutes) |
| `created_at` | TEXT             | Token creation timestamp (UTC)     |

**Access Frequency:**

- **Read:** When user claims device via QR code: **Once per pairing**
- **Write:**
  - When ESP32 generates QR code: **Every time QR is generated**
  - When device is claimed: **Once per successful claim** (token deleted)
  - Expired tokens cleaned up: **Periodically**

**Lifetime:** 10 minutes

---

#### 6. `device_share_tokens` - User-to-User Sharing Tokens

**Purpose:** Longer-lived tokens for sharing device access with other users

| Column       | Type             | Description                          |
| ------------ | ---------------- | ------------------------------------ |
| `id`         | TEXT PRIMARY KEY | Token ID                             |
| `device_id`  | TEXT             | Device ID                            |
| `created_by` | TEXT             | User ID who generated the share link |
| `token_hash` | TEXT             | SHA-256 hash of token                |
| `expires_at` | TEXT             | Token expiration (UTC, 24 hours)     |
| `created_at` | TEXT             | Token creation timestamp (UTC)       |

**Access Frequency:**

- **Read:** When user claims device via share link: **Once per claim**
- **Write:**
  - When owner generates share link: **On user action**
  - Expired tokens cleaned up: **Periodically**

**Lifetime:** 24 hours (can be used by multiple users until expiration)

---

#### 7. `push_subscriptions` - Push Notification Subscriptions

**Purpose:** Web Push API subscriptions for browser notifications

| Column       | Type             | Description                                             |
| ------------ | ---------------- | ------------------------------------------------------- |
| `id`         | TEXT PRIMARY KEY | Subscription ID                                         |
| `user_id`    | TEXT             | User ID                                                 |
| `device_id`  | TEXT             | Device ID (optional, for device-specific notifications) |
| `endpoint`   | TEXT             | Web Push endpoint URL                                   |
| `p256dh`     | TEXT             | Public key for encryption                               |
| `auth`       | TEXT             | Authentication secret                                   |
| `created_at` | TEXT             | Subscription creation timestamp (UTC)                   |
| `updated_at` | TEXT             | Last update timestamp (UTC)                             |

**Access Frequency:**

- **Read:** When sending push notifications: **On notification event**
- **Write:**
  - When user subscribes: **Once per subscription**
  - When subscription is updated: **On user action** (re-subscribe)

**Update Pattern:** Subscriptions are updated (upsert) when user re-subscribes from same browser.

---

#### 8. `notification_preferences` - Notification Settings

**Purpose:** Per-user notification preferences

| Column               | Type             | Description                               |
| -------------------- | ---------------- | ----------------------------------------- |
| `id`                 | TEXT PRIMARY KEY | Preference ID                             |
| `user_id`            | TEXT UNIQUE      | User ID                                   |
| `machine_ready`      | INTEGER          | Notify when machine is ready (default: 1) |
| `water_empty`        | INTEGER          | Notify when water is low (default: 1)     |
| `descale_due`        | INTEGER          | Notify when descale is due (default: 1)   |
| `service_due`        | INTEGER          | Notify when service is due (default: 1)   |
| `backflush_due`      | INTEGER          | Notify when backflush is due (default: 1) |
| `machine_error`      | INTEGER          | Notify on machine errors (default: 1)     |
| `pico_offline`       | INTEGER          | Notify when Pico disconnects (default: 1) |
| `schedule_triggered` | INTEGER          | Notify when schedule runs (default: 1)    |
| `brew_complete`      | INTEGER          | Notify when brew completes (default: 0)   |
| `created_at`         | TEXT             | Preference creation timestamp (UTC)       |
| `updated_at`         | TEXT             | Last update timestamp (UTC)               |

**Access Frequency:**

- **Read:** When sending push notifications: **On notification event**
- **Write:** When user updates preferences: **On user action**

---

## Access Patterns Summary

### High Frequency (Real-time)

| Operation            | Frequency                       | Table(s)                    |
| -------------------- | ------------------------------- | --------------------------- |
| Session verification | Every API/WebSocket request     | `sessions` (read)           |
| Device online status | Every device connect/disconnect | `devices` (write)           |
| Session last_used_at | Batched every 60 seconds        | `sessions` (write, batched) |

### Medium Frequency (User Actions)

| Operation              | Frequency           | Table(s)                                      |
| ---------------------- | ------------------- | --------------------------------------------- |
| List user devices      | On page load        | `user_devices`, `devices` (read)              |
| Get device details     | On device page view | `devices`, `user_devices` (read)              |
| Update device name     | On user edit        | `user_devices` (write)                        |
| Update device metadata | On user edit        | `devices` (write)                             |
| Claim device           | Once per pairing    | `user_devices`, `device_claim_tokens` (write) |
| Generate share link    | On user action      | `device_share_tokens` (write)                 |

### Low Frequency (Rare Events)

| Operation                 | Frequency        | Table(s)                                             |
| ------------------------- | ---------------- | ---------------------------------------------------- |
| User login                | Once per session | `profiles`, `sessions` (write)                       |
| Token refresh             | Every 60 minutes | `sessions` (write)                                   |
| User logout               | On user action   | `sessions` (write)                                   |
| Subscribe to push         | Once per browser | `push_subscriptions` (write)                         |
| Update notification prefs | On user action   | `notification_preferences` (write)                   |
| Cleanup expired tokens    | Periodically     | `device_claim_tokens`, `device_share_tokens` (write) |

---

## What is NOT Stored

The cloud service does **NOT** store:

- ❌ Machine state (temperatures, pressure, mode)
- ❌ Brewing data (shot history, weights, durations)
- ❌ Statistics (total shots, energy consumption)
- ❌ Settings (temperature setpoints, PID parameters)
- ❌ Real-time sensor readings
- ❌ Shot history or brew records

**All machine data remains on the ESP32** and is only relayed through the cloud service in real-time via WebSocket. The cloud service acts as a **stateless relay** for machine data.

---

## Database Size Estimates

For a typical deployment with 100 users and 200 devices:

| Table                      | Estimated Rows         | Size per Row | Total Size  |
| -------------------------- | ---------------------- | ------------ | ----------- |
| `devices`                  | 200                    | ~500 bytes   | ~100 KB     |
| `user_devices`             | 300                    | ~200 bytes   | ~60 KB      |
| `profiles`                 | 100                    | ~300 bytes   | ~30 KB      |
| `sessions`                 | 500 (active + expired) | ~400 bytes   | ~200 KB     |
| `device_claim_tokens`      | 10 (active)            | ~200 bytes   | ~2 KB       |
| `device_share_tokens`      | 20 (active)            | ~200 bytes   | ~4 KB       |
| `push_subscriptions`       | 150                    | ~500 bytes   | ~75 KB      |
| `notification_preferences` | 100                    | ~200 bytes   | ~20 KB      |
| **Total**                  |                        |              | **~500 KB** |

**Note:** SQLite overhead and indexes add ~20-30% to total size. A typical deployment would use **< 1 MB** of storage.

---

## Backup Recommendations

1. **Regular Backups:** Backup `brewos.db` file daily
2. **Before Updates:** Backup before deploying updates
3. **Retention:** Keep at least 7 days of backups
4. **Location:** Store backups in separate location from production

**Backup Command:**

```bash
cp ${DATA_DIR}/brewos.db ${DATA_DIR}/backups/brewos-$(date +%Y%m%d).db
```

---

## Performance Considerations

1. **Session Verification:** Most frequent operation. Uses indexed lookup on `access_token_hash` (UNIQUE index).

2. **Device Status Updates:** Happens on every connect/disconnect. Uses indexed lookup on `id` (PRIMARY KEY).

3. **Batch Updates:** `last_used_at` updates are batched to minimize writes (every 60 seconds, max 100 sessions per batch).

4. **Indexes:**

   - `devices.id` (PRIMARY KEY)
   - `devices.owner_id` (for backward compatibility)
   - `user_devices(user_id, device_id)` (PRIMARY KEY)
   - `sessions.access_token_hash` (UNIQUE)
   - `sessions.refresh_token_hash` (UNIQUE)
   - `sessions.user_id` (for cleanup queries)

5. **Cleanup:** Expired tokens and sessions are cleaned up periodically, not on every access.

---

## Security Notes

1. **Token Hashing:** All tokens are stored as SHA-256 hashes, never in plaintext.

2. **Constant-Time Comparison:** Token verification uses constant-time comparison to prevent timing attacks.

3. **Token Rotation:** Refresh tokens are rotated on every refresh (old token deleted, new token created).

4. **Session Revocation:** Sessions can be revoked individually or all at once (logout everywhere).

5. **Foreign Key Constraints:** Database enforces referential integrity with CASCADE deletes.

---

## See Also

- [Cloud Service README](./README.md) - Overall architecture
- [Pairing and Sharing](./Pairing_and_Sharing.md) - Device pairing flows
- [Deployment Guide](./Deployment.md) - Production deployment
