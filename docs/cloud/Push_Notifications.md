# Push Notifications - Cloud Service

This document describes the push notification implementation in the BrewOS cloud service.

## Overview

The cloud service handles push notification subscriptions and delivery for BrewOS devices. It uses the Web Push Protocol with VAPID authentication to send notifications to user browsers.

## Architecture

### Components

1. **Push Service** (`src/services/push.ts`)
   - Manages VAPID keys
   - Handles subscription storage
   - Sends push notifications

2. **Push Routes** (`src/routes/push.ts`)
   - API endpoints for subscription management
   - ESP32 notification endpoint

3. **Database Schema**
   - `push_subscriptions` table stores user subscriptions

## Setup

### 1. Generate VAPID Keys

```bash
cd src/cloud
npx web-push generate-vapid-keys
```

This will output:
```
Public Key: <your-public-key>
Private Key: <your-private-key>
```

### 2. Configure Environment

Add to `.env`:

```env
VAPID_PUBLIC_KEY=your-public-key-here
VAPID_PRIVATE_KEY=your-private-key-here
VAPID_SUBJECT=mailto:admin@brewos.app
```

**Note:** The `VAPID_SUBJECT` should be either:
- An email address: `mailto:admin@brewos.app`
- A URL: `https://brewos.app`

### 3. Initialize on Startup

The push service is automatically initialized when the server starts:

```typescript
// src/server.ts
import { initializePushNotifications } from './services/push.js';

async function start() {
  await initDatabase();
  initializePushNotifications(); // Auto-generates keys if not configured
  // ...
}
```

If VAPID keys are not configured, the server will:
- Generate new keys automatically
- Log them to console
- **Important:** Copy these keys to your `.env` file for persistence

## Database Schema

### push_subscriptions Table

```sql
CREATE TABLE push_subscriptions (
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
```

**Indexes:**
- `idx_push_subscriptions_user` on `user_id`
- `idx_push_subscriptions_device` on `device_id`

## API Endpoints

### GET /api/push/vapid-key

Returns the VAPID public key for client subscription.

**Response:**
```json
{
  "publicKey": "base64-encoded-key"
}
```

**Usage:**
Clients use this key to subscribe to push notifications.

### POST /api/push/subscribe

Subscribe a user to push notifications.

**Authentication:** Required (Google OAuth)

**Request:**
```json
{
  "subscription": {
    "endpoint": "https://fcm.googleapis.com/...",
    "keys": {
      "p256dh": "base64-encoded-key",
      "auth": "base64-encoded-key"
    }
  },
  "deviceId": "BRW-12345678" // Optional
}
```

**Response:**
```json
{
  "success": true,
  "subscription": {
    "id": "uuid",
    "user_id": "user-id",
    "device_id": "BRW-12345678",
    "endpoint": "https://...",
    "p256dh": "...",
    "auth": "...",
    "created_at": "2024-01-01T00:00:00Z",
    "updated_at": "2024-01-01T00:00:00Z"
  }
}
```

**Behavior:**
- If subscription with same endpoint exists, updates it
- Otherwise, creates new subscription
- Links subscription to user and optionally device

### POST /api/push/unsubscribe

Unsubscribe from push notifications.

**Authentication:** Required (Google OAuth)

**Request:**
```json
{
  "subscription": {
    "endpoint": "https://fcm.googleapis.com/...",
    "keys": {
      "p256dh": "base64-encoded-key",
      "auth": "base64-encoded-key"
    }
  }
}
```

**Response:**
```json
{
  "success": true
}
```

### GET /api/push/subscriptions

Get all push subscriptions for the authenticated user.

**Authentication:** Required (Google OAuth)

**Response:**
```json
{
  "subscriptions": [
    {
      "id": "uuid",
      "user_id": "user-id",
      "device_id": "BRW-12345678",
      "endpoint": "https://...",
      "created_at": "2024-01-01T00:00:00Z"
    }
  ]
}
```

### POST /api/push/notify

Send a push notification from ESP32 device.

**Authentication:** None (device ID is the auth)

**Request:**
```json
{
  "deviceId": "BRW-12345678",
  "notification": {
    "type": "WATER_EMPTY",
    "message": "Please refill the water tank",
    "timestamp": 1704067200,
    "is_alert": false
  }
}
```

**Notification Types:**
- `MACHINE_READY` - Machine reached brewing temperature
- `WATER_EMPTY` - Water tank needs refill
- `DESCALE_DUE` - Time to descale
- `SERVICE_DUE` - Maintenance recommended
- `BACKFLUSH_DUE` - Backflush reminder
- `MACHINE_ERROR` - Machine error
- `PICO_OFFLINE` - Control board offline

**Response:**
```json
{
  "success": true,
  "sentCount": 2
}
```

**Behavior:**
1. Validates device ID format
2. Verifies device exists and is claimed
3. Looks up all subscriptions for device owner
4. Sends push notification to each subscription
5. Removes invalid subscriptions automatically
6. Returns count of successful sends

## Service Functions

### initializePushNotifications()

Initializes the push notification service with VAPID keys.

```typescript
initializePushNotifications(): void
```

- Loads VAPID keys from environment
- Generates new keys if not configured
- Configures web-push library

### getVAPIDPublicKey()

Returns the VAPID public key.

```typescript
getVAPIDPublicKey(): string
```

### subscribeToPush()

Subscribe a user to push notifications.

```typescript
subscribeToPush(
  userId: string,
  deviceId: string | null,
  subscription: PushSubscriptionData
): PushSubscription
```

- Creates or updates subscription
- Links to user and optionally device
- Returns subscription record

### unsubscribeFromPush()

Unsubscribe from push notifications.

```typescript
unsubscribeFromPush(subscription: PushSubscriptionData): boolean
```

- Removes subscription by endpoint
- Returns true if subscription was found and removed

### sendPushNotification()

Send a push notification to a single subscription.

```typescript
sendPushNotification(
  subscription: PushSubscription,
  payload: NotificationPayload
): Promise<boolean>
```

**Payload:**
```typescript
{
  title: string;
  body: string;
  icon?: string;
  badge?: string;
  tag?: string;
  data?: Record<string, unknown>;
  requireInteraction?: boolean;
}
```

**Behavior:**
- Sends notification via web-push
- Handles invalid subscriptions (410, 404)
- Automatically removes invalid subscriptions
- Returns true on success

### sendPushNotificationToUser()

Send push notification to all subscriptions for a user.

```typescript
sendPushNotificationToUser(
  userId: string,
  payload: NotificationPayload
): Promise<number>
```

- Gets all user subscriptions
- Sends to each subscription
- Returns count of successful sends

### sendPushNotificationToDevice()

Send push notification to all subscriptions for a device.

```typescript
sendPushNotificationToDevice(
  deviceId: string,
  payload: NotificationPayload
): Promise<number>
```

- Gets all device subscriptions
- Sends to each subscription
- Returns count of successful sends

## Error Handling

### Invalid Subscriptions

When a push notification fails with status 410 (Gone) or 404 (Not Found):
- Subscription is automatically removed from database
- Error is logged
- Function continues with other subscriptions

### Network Errors

Network errors are caught and logged:
- Subscription is not removed (may be temporary)
- Error is logged for monitoring
- Function returns false

## Notification Payload Format

The notification payload sent to browsers:

```json
{
  "title": "Machine Ready",
  "body": "Your espresso machine is ready to brew",
  "icon": "/logo-icon.svg",
  "badge": "/logo-icon.svg",
  "tag": "machine-ready",
  "requireInteraction": false,
  "data": {
    "deviceId": "BRW-12345678",
    "type": "MACHINE_READY",
    "url": "/device/BRW-12345678"
  }
}
```

**Fields:**
- `title` - Notification title
- `body` - Notification message
- `icon` - Icon URL (default: `/logo-icon.svg`)
- `badge` - Badge icon (default: `/logo-icon.svg`)
- `tag` - Notification tag for grouping
- `requireInteraction` - Require user interaction (alerts: true)
- `data` - Custom data for app handling

## Testing

### Manual Testing

1. **Subscribe:**
   ```bash
   curl -X POST http://localhost:3001/api/push/subscribe \
     -H "Content-Type: application/json" \
     -H "Authorization: Bearer <token>" \
     -d '{
       "subscription": { ... },
       "deviceId": "BRW-12345678"
     }'
   ```

2. **Send notification:**
   ```bash
   curl -X POST http://localhost:3001/api/push/notify \
     -H "Content-Type: application/json" \
     -d '{
       "deviceId": "BRW-12345678",
       "notification": {
         "type": "MACHINE_READY",
         "message": "Machine is ready",
         "timestamp": 1704067200,
         "is_alert": false
       }
     }'
   ```

### Automated Testing

See test files in `src/cloud/tests/` (if available).

## Monitoring

### Logs

The service logs:
- VAPID key initialization
- Subscription creation/updates
- Push notification sends
- Invalid subscription removals
- Errors

### Metrics to Monitor

- Subscription count per user
- Notification delivery rate
- Invalid subscription rate
- Error rate

## Security Considerations

1. **VAPID Keys**
   - Private key must never be exposed
   - Store in environment variables only
   - Rotate keys periodically

2. **Device Authentication**
   - `/api/push/notify` endpoint validates device ID format
   - Verifies device is claimed before sending
   - No user authentication required (device ID is auth)

3. **User Authentication**
   - Subscription management requires OAuth
   - User can only manage their own subscriptions
   - Device ownership verified

4. **Rate Limiting**
   - Consider adding rate limiting for `/api/push/notify`
   - Prevent notification spam
   - Monitor for abuse

## Deployment

### Production Checklist

- [ ] VAPID keys configured in environment
- [ ] VAPID subject set (email or URL)
- [ ] HTTPS enabled (required for push)
- [ ] Database migrations run
- [ ] Monitoring configured
- [ ] Error logging configured

### Environment Variables

```env
VAPID_PUBLIC_KEY=your-public-key
VAPID_PRIVATE_KEY=your-private-key
VAPID_SUBJECT=mailto:admin@brewos.app
```

## Troubleshooting

### Notifications Not Sending

1. **Check VAPID keys:**
   - Verify keys are set in environment
   - Check server logs for key initialization

2. **Check subscriptions:**
   - Query database for user subscriptions
   - Verify subscriptions are valid

3. **Check device:**
   - Verify device is claimed
   - Check device ID format

4. **Check logs:**
   - Look for push send errors
   - Check for invalid subscription removals

### Invalid Subscriptions

If subscriptions are being removed:
- Check push service endpoint is valid
- Verify VAPID keys are correct
- Check network connectivity to push service

## Future Enhancements

- [ ] Notification preferences per device
- [ ] Notification history
- [ ] Delivery receipts
- [ ] Rich notifications (images, actions)
- [ ] Notification scheduling
- [ ] Rate limiting
- [ ] Analytics

