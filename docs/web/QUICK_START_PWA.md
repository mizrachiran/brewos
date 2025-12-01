# PWA & Push Notifications Quick Start

Quick setup guide for enabling PWA and push notifications in BrewOS.

## For End Users

### Installing the PWA

1. **Open BrewOS web interface** in your browser
2. **Look for install prompt** in address bar or menu
3. **Click "Install"** or "Add to Home Screen"
4. **Done!** The app now works like a native app

### Enabling Push Notifications

1. **Open Settings** > System (cloud mode only)
2. **Find "Push Notifications"** section
3. **Click "Enable Push Notifications"**
4. **Grant permission** when browser prompts
5. **Verify status** shows "Active"

You'll now receive notifications for:
- Machine ready to brew
- Water tank empty
- Maintenance reminders
- Machine errors

## For Administrators

### Cloud Server Setup

1. **Generate VAPID keys:**
   ```bash
   cd src/cloud
   npx web-push generate-vapid-keys
   ```

2. **Add to `.env` file:**
   ```env
   VAPID_PUBLIC_KEY=your-public-key-here
   VAPID_PRIVATE_KEY=your-private-key-here
   VAPID_SUBJECT=mailto:admin@brewos.app
   ```

3. **Restart server:**
   ```bash
   npm run start
   ```

### ESP32 Configuration

No additional configuration needed! The ESP32 automatically:
- Sends notifications to cloud when configured
- Respects user notification preferences
- Handles errors gracefully

**Requirements:**
- Cloud URL configured in ESP32 settings
- Device claimed by a user
- WiFi connected

## Testing

### Test PWA Installation

1. Open web interface
2. Check for install prompt
3. Install the app
4. Verify it opens in standalone window

### Test Push Notifications

1. Enable push notifications in Settings
2. Trigger a notification (e.g., water empty)
3. Verify notification appears
4. Click notification to open app

### Test Offline Mode

1. Install PWA
2. Open DevTools > Network
3. Set to "Offline"
4. Reload app
5. Should show cached content

## Troubleshooting

### PWA Not Installing

- **Check HTTPS:** Must be served over HTTPS (or localhost)
- **Check browser:** Safari requires iOS 11.3+
- **Check manifest:** Verify `manifest.json` is accessible

### Push Notifications Not Working

- **Check permission:** Settings should show "Granted"
- **Check subscription:** Should show "Active"
- **Check browser:** Safari doesn't support push notifications
- **Check VAPID keys:** Verify keys are configured in cloud server

### Notifications Not Received

- **Check ESP32:** Verify cloud URL is configured
- **Check device:** Verify device is claimed
- **Check preferences:** Verify notification type is enabled
- **Check logs:** Look for errors in cloud server logs

## More Information

- [PWA Documentation](./PWA.md) - Full PWA features and details
- [Push Notifications Documentation](./Push_Notifications.md) - Complete push notification guide
- [Cloud Push Notifications](../../cloud/Push_Notifications.md) - Server-side implementation

