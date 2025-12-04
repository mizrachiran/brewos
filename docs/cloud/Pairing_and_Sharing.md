# Device Pairing & Sharing

This document describes how devices are paired to user accounts and how users can share device access with others.

## Overview

BrewOS supports two ways to add devices to a user's account:

1. **ESP32 QR Pairing** - Scan a QR code displayed on the ESP32 device (physical access required)
2. **User-to-User Sharing** - Generate a share link to give others access to your device (no physical access needed)

Both methods result in the device being added to the user's account via the `user_devices` table, which enables multi-user device ownership.

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        DEVICE ACCESS MODEL                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌──────────┐         ┌──────────────┐         ┌──────────┐                 │
│  │  User A  │────────►│   Device     │◄────────│  User B  │                 │
│  └──────────┘         │ (BRW-XXXXX)  │         └──────────┘                 │
│       │               └──────────────┘               │                       │
│       │                      ▲                       │                       │
│       │                      │                       │                       │
│       ▼                      │                       ▼                       │
│  ┌──────────────────────────────────────────────────────────────┐           │
│  │                    user_devices table                         │           │
│  │  ┌─────────────────────────────────────────────────────────┐ │           │
│  │  │ user_id | device_id   | name              | claimed_at  │ │           │
│  │  ├─────────────────────────────────────────────────────────┤ │           │
│  │  │ user_a  | BRW-XXXXX   | Kitchen Espresso  | 2024-01-01  │ │           │
│  │  │ user_b  | BRW-XXXXX   | Office Machine    | 2024-01-15  │ │           │
│  │  └─────────────────────────────────────────────────────────┘ │           │
│  └──────────────────────────────────────────────────────────────┘           │
│                                                                              │
│  ► Each user has their own name for the device                              │
│  ► All users can control the device via the cloud                           │
│  ► Any user can remove any other user's access                              │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Database Schema

### Tables

```sql
-- Devices table (shared device metadata)
devices (
  id TEXT PRIMARY KEY,           -- e.g., "BRW-A1B2C3D4"
  machine_brand TEXT,
  machine_model TEXT,
  firmware_version TEXT,
  is_online INTEGER,
  last_seen_at TEXT,
  created_at TEXT,
  updated_at TEXT
)

-- User-device associations (many-to-many)
user_devices (
  user_id TEXT,
  device_id TEXT,
  name TEXT DEFAULT 'My BrewOS',  -- Per-user device name
  claimed_at TEXT,
  PRIMARY KEY (user_id, device_id)
)

-- ESP32 QR pairing tokens (short-lived, 10 minutes)
device_claim_tokens (
  id TEXT PRIMARY KEY,
  device_id TEXT UNIQUE,
  token_hash TEXT,
  expires_at TEXT
)

-- User-to-user sharing tokens (longer-lived, 24 hours)
device_share_tokens (
  id TEXT PRIMARY KEY,
  device_id TEXT,
  created_by TEXT,               -- User who generated the share link
  token_hash TEXT,
  expires_at TEXT
)
```

---

## Method 1: ESP32 QR Pairing

This is the primary method for initially pairing a device to a user account. Requires physical access to the ESP32 device.

### Flow Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          ESP32 QR PAIRING FLOW                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  STEP 1: ESP32 Generates Token                                              │
│  ───────────────────────────────                                            │
│                                                                              │
│  ESP32                          Cloud Server                                 │
│    │                                 │                                       │
│    │  Generate random token          │                                       │
│    │  Display QR code on screen      │                                       │
│    │                                 │                                       │
│    │  POST /api/devices/register-claim                                       │
│    │  { deviceId: "BRW-XXXXX", token: "abc123..." }                         │
│    │────────────────────────────────►│                                       │
│    │                                 │  Store hash(token) with 10min expiry  │
│    │◄────────────────────────────────│  { success: true }                    │
│                                                                              │
│  STEP 2: User Scans QR Code                                                 │
│  ──────────────────────────────                                             │
│                                                                              │
│  QR Code contains: https://cloud.brewos.io/pair?id=BRW-XXXXX&token=abc123   │
│                                                                              │
│  User's Phone                   Cloud Server                                 │
│    │                                 │                                       │
│    │  Open URL in browser            │                                       │
│    │  (redirects to /pair page)      │                                       │
│    │                                 │                                       │
│    │  If not logged in:              │                                       │
│    │  Show Google Sign-In button     │                                       │
│    │                                 │                                       │
│                                                                              │
│  STEP 3: Claim Device                                                       │
│  ─────────────────────                                                      │
│                                                                              │
│  User's Phone                   Cloud Server                                 │
│    │                                 │                                       │
│    │  POST /api/devices/claim        │                                       │
│    │  { deviceId, token, name? }     │                                       │
│    │  Authorization: Bearer <token>  │                                       │
│    │────────────────────────────────►│                                       │
│    │                                 │  Verify: hash(token) == stored_hash   │
│    │                                 │  Check: not expired                   │
│    │                                 │  Insert into user_devices             │
│    │                                 │  Delete claim token                   │
│    │◄────────────────────────────────│  { success: true, device: {...} }     │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### API Endpoints

#### Register Claim Token (ESP32)

```http
POST /api/devices/register-claim
Content-Type: application/json

{
  "deviceId": "BRW-A1B2C3D4",
  "token": "randomtoken123..."
}
```

**Response:**
```json
{ "success": true }
```

**Notes:**
- No authentication required (the token itself is the secret)
- Token is hashed before storage (SHA-256)
- Expires in 10 minutes
- Rate limited (5 requests/minute per IP)

#### Claim Device (User)

```http
POST /api/devices/claim
Authorization: Bearer <session_token>
Content-Type: application/json

{
  "deviceId": "BRW-A1B2C3D4",
  "token": "randomtoken123...",
  "name": "Kitchen Espresso"  // optional
}
```

**Response:**
```json
{
  "success": true,
  "device": {
    "id": "BRW-A1B2C3D4",
    "name": "Kitchen Espresso",
    "claimedAt": "2024-01-01T12:00:00Z"
  }
}
```

**Error Responses:**
- `400` - Invalid or expired claim token
- `400` - Device already claimed by this user
- `401` - Not authenticated

---

## Method 2: User-to-User Sharing

Allows existing device owners to share access with others without requiring physical access to the device.

### Flow Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        USER-TO-USER SHARING FLOW                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  STEP 1: Owner Generates Share Link                                         │
│  ────────────────────────────────────                                       │
│                                                                              │
│  User A (Owner)                 Cloud Server                                 │
│    │                                 │                                       │
│    │  Click "Share" button           │                                       │
│    │  on device card                 │                                       │
│    │                                 │                                       │
│    │  POST /api/devices/:id/share    │                                       │
│    │  Authorization: Bearer <token>  │                                       │
│    │────────────────────────────────►│                                       │
│    │                                 │  Verify user owns device              │
│    │                                 │  Generate random token                │
│    │                                 │  Store hash(token) with 24hr expiry   │
│    │◄────────────────────────────────│                                       │
│    │  {                              │                                       │
│    │    url: "https://.../pair?...&share=true",                             │
│    │    manualCode: "XXXX-XXXX",     │                                       │
│    │    expiresIn: 86400             │                                       │
│    │  }                              │                                       │
│                                                                              │
│  STEP 2: Owner Shares the Link                                              │
│  ──────────────────────────────                                             │
│                                                                              │
│  User A can share via:                                                       │
│  • QR Code (displayed in app)                                               │
│  • Copy URL                                                                  │
│  • Manual code (for voice/text sharing)                                     │
│                                                                              │
│  STEP 3: Recipient Claims Device                                            │
│  ─────────────────────────────────                                          │
│                                                                              │
│  User B                         Cloud Server                                 │
│    │                                 │                                       │
│    │  Open share URL                 │                                       │
│    │  (redirects to /pair page)      │                                       │
│    │                                 │                                       │
│    │  POST /api/devices/claim-share  │                                       │
│    │  { deviceId, token, name? }     │                                       │
│    │  Authorization: Bearer <token>  │                                       │
│    │────────────────────────────────►│                                       │
│    │                                 │  Verify share token valid             │
│    │                                 │  Insert into user_devices             │
│    │                                 │  (token NOT deleted - reusable)       │
│    │◄────────────────────────────────│  { success: true, device: {...} }     │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### API Endpoints

#### Generate Share Link

```http
POST /api/devices/:deviceId/share
Authorization: Bearer <session_token>
```

**Response:**
```json
{
  "deviceId": "BRW-A1B2C3D4",
  "token": "ABCD1234EFGH5678",
  "url": "https://cloud.brewos.io/pair?id=BRW-A1B2C3D4&token=ABCD1234EFGH5678&share=true",
  "manualCode": "ABCD-1234",
  "expiresAt": "2024-01-02T12:00:00Z",
  "expiresIn": 86400
}
```

**Notes:**
- Requires authentication
- User must own the device
- Tokens expire in 24 hours
- Rate limited (30 requests/15min per IP)

#### Claim with Share Link

```http
POST /api/devices/claim-share
Authorization: Bearer <session_token>
Content-Type: application/json

{
  "deviceId": "BRW-A1B2C3D4",
  "token": "ABCD1234EFGH5678",
  "name": "Office Machine"  // optional
}
```

**Response:**
```json
{
  "success": true,
  "device": {
    "id": "BRW-A1B2C3D4",
    "name": "Office Machine",
    "claimedAt": "2024-01-15T09:30:00Z"
  }
}
```

**Error Responses:**
- `400` - Invalid or expired share link
- `400` - Device is already in your account
- `401` - Not authenticated

---

## Comparison: QR Pairing vs Sharing

| Aspect | ESP32 QR Pairing | User-to-User Sharing |
|--------|------------------|----------------------|
| **Token Source** | ESP32 device | Cloud server |
| **Token Expiry** | 10 minutes | 24 hours |
| **Physical Access** | Required | Not required |
| **Reusable Token** | No (deleted after use) | Yes (until expiry) |
| **Use Case** | Initial device setup | Adding family/friends |
| **API Endpoint** | `/api/devices/claim` | `/api/devices/claim-share` |
| **URL Parameter** | `?id=...&token=...` | `?id=...&token=...&share=true` |

---

## User Interface

### Share Device Modal (Cloud Mode)

Located on the Machines page (`/machines`), accessible via the Share button on each device card.

**Features:**
- QR code for easy mobile scanning
- Copy URL button
- Manual share code (e.g., "ABCD-1234")
- Countdown timer showing time until expiry
- "New Link" button to regenerate

### Pair Page (`/pair`)

Handles both pairing methods based on the `share` query parameter:

| Parameter | Displayed Title | Message |
|-----------|-----------------|---------|
| `share=true` | "Add Shared Device" | "Someone shared access to their BrewOS machine with you" |
| (not present) | "Pair Device" | "Add this BrewOS device to your account" |

---

## Managing Device Users

### View Users

```http
GET /api/devices/:deviceId/users
Authorization: Bearer <session_token>
```

**Response:**
```json
{
  "users": [
    {
      "userId": "user_a_id",
      "email": "usera@example.com",
      "displayName": "User A",
      "avatarUrl": "https://...",
      "claimedAt": "2024-01-01T12:00:00Z"
    },
    {
      "userId": "user_b_id",
      "email": "userb@example.com",
      "displayName": "User B",
      "avatarUrl": null,
      "claimedAt": "2024-01-15T09:30:00Z"
    }
  ]
}
```

### Remove User Access

Any user with access can remove any other user:

```http
DELETE /api/devices/:deviceId/users/:userId
Authorization: Bearer <session_token>
```

**Response:**
```json
{ "success": true }
```

**Notes:**
- Users cannot remove themselves via this endpoint (use `DELETE /api/devices/:id` instead)
- This is "mutual removal" - if you have access, you can revoke others' access

---

## Security Considerations

### Token Security

1. **Hashing**: All tokens are hashed with SHA-256 before storage
2. **Constant-time comparison**: Prevents timing attacks
3. **Expiration**: QR tokens expire quickly (10 min), share tokens in 24 hours

### Rate Limiting

| Endpoint | Limit |
|----------|-------|
| `/api/devices/register-claim` | 5 requests/minute (strict) |
| `/api/devices/claim` | 5 requests/minute (strict) |
| `/api/devices/claim-share` | 5 requests/minute (strict) |
| `/api/devices/:id/share` | 30 requests/15 minutes |

### Access Control

- Only device owners can generate share links
- Only authenticated users can claim devices
- Any owner can revoke any other owner's access

---

## Error Handling

### Common Error Responses

```json
// Expired token
{
  "error": "Invalid or expired claim token"
}

// Expired share link
{
  "error": "Invalid or expired share link"
}

// Already claimed
{
  "error": "Device is already claimed by this user"
}

// Already in account (share)
{
  "error": "Device is already in your account"
}

// No access
{
  "error": "You do not have access to this device"
}
```

---

## Implementation Details

### Token Generation

```typescript
// Share token generation (16 chars, uppercase alphanumeric)
const token = randomUUID().replace(/-/g, '').substring(0, 16).toUpperCase();
// Example: "ABCD1234EFGH5678"

// Manual code format (8 chars with hyphen)
const manualCode = `${token.substring(0, 4)}-${token.substring(4, 8)}`;
// Example: "ABCD-1234"
```

### Token Verification

```typescript
function verifyToken(deviceId: string, token: string): boolean {
  const tokenHash = hashToken(token);
  const stored = getStoredToken(deviceId);
  
  if (!stored || isExpired(stored.expiresAt)) {
    return false;
  }
  
  // Constant-time comparison
  return timingSafeEqual(
    Buffer.from(tokenHash, 'hex'),
    Buffer.from(stored.tokenHash, 'hex')
  );
}
```

---

## Related Documentation

- [README.md](./README.md) - Cloud service overview
- [ESP32_Integration.md](./ESP32_Integration.md) - ESP32 WebSocket integration
- [Push_Notifications.md](./Push_Notifications.md) - Push notification setup

