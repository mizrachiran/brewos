# Progressive Web App (PWA)

BrewOS web interface is a fully functional Progressive Web App (PWA) that can be installed on any device and works offline.

## Features

### Installation
- **Install on any device** - Add to home screen on iOS, Android, Windows, macOS, and Linux
- **Standalone experience** - Runs in its own window without browser UI
- **App-like shortcuts** - Quick access to Dashboard, Brewing, and Settings

### Offline Support
- **Service Worker caching** - Automatically caches static assets and API responses
- **Offline fallback** - Shows cached content when network is unavailable
- **Background sync** - Queues actions when offline, syncs when back online

### Push Notifications
- **Real-time alerts** - Receive notifications when your machine needs attention
- **Background delivery** - Notifications work even when the app is closed
- **Actionable notifications** - Click to open the app directly to relevant screen

## Installation Guide

### Desktop (Chrome, Edge, Safari, Firefox)

1. Visit the BrewOS web interface
2. Look for the install prompt in the address bar or menu
3. Click "Install" or "Add to Home Screen"
4. The app will open in its own window

### Mobile (iOS)

1. Open Safari and navigate to the BrewOS web interface
2. Tap the Share button (square with arrow)
3. Select "Add to Home Screen"
4. Customize the name if desired
5. Tap "Add"

### Mobile (Android)

1. Open Chrome and navigate to the BrewOS web interface
2. Tap the menu (three dots)
3. Select "Add to Home Screen" or "Install App"
4. Confirm the installation

## Offline Usage

The PWA automatically caches:
- **Static assets** - HTML, CSS, JavaScript, images
- **API responses** - Recent status updates and configuration
- **Service worker** - Handles offline requests

When offline:
- Previously viewed pages load from cache
- Recent machine status is available
- Settings can be viewed (changes queue for sync)

## App Shortcuts

Quick actions available from the app icon:

- **Dashboard** - View machine status
- **Brewing** - Start a brew session
- **Settings** - Configure machine settings

## Service Worker

The service worker (`sw.js`) handles:
- **Asset caching** - Stores static files for offline access
- **Network-first strategy** - Tries network first, falls back to cache
- **Push notifications** - Receives and displays push notifications
- **Background sync** - Syncs queued actions when online

### Cache Management

Caches are automatically managed:
- **Static cache** - Stores app shell (HTML, CSS, JS)
- **Dynamic cache** - Stores API responses
- **Auto-cleanup** - Old caches are removed on updates

## Troubleshooting

### App Not Installing

**Check browser support:**
- Chrome/Edge: Full PWA support
- Safari (iOS): Requires iOS 11.3+
- Firefox: Full PWA support

**Check requirements:**
- Must be served over HTTPS (or localhost)
- Must have a valid manifest.json
- Service worker must be registered

### Offline Mode Not Working

1. **Clear cache and reinstall:**
   - Uninstall the app
   - Clear browser cache
   - Reinstall the app

2. **Check service worker:**
   - Open browser DevTools
   - Go to Application > Service Workers
   - Verify service worker is active

3. **Check network:**
   - Disable network in DevTools
   - Reload the app
   - Should show cached content

### Notifications Not Working

See [Push Notifications Documentation](./Push_Notifications.md) for troubleshooting.

## Development

### Building the PWA

The PWA is built automatically with the web app:

```bash
cd src/web
npm run build
```

The build includes:
- `manifest.json` - PWA configuration
- `sw.js` - Service worker
- Static assets in `public/`

### Testing Locally

1. **Start dev server:**
   ```bash
   npm run dev
   ```

2. **Test service worker:**
   - Open DevTools > Application > Service Workers
   - Verify registration
   - Test offline mode

3. **Test installation:**
   - Look for install prompt
   - Install and verify standalone mode

### Updating the Service Worker

When updating the service worker:

1. **Update version in cache name:**
   ```javascript
   const CACHE_NAME = 'brewos-v2'; // Increment version
   ```

2. **Test update flow:**
   - Old service worker continues until all tabs close
   - New service worker activates on next visit
   - Old cache is automatically cleaned up

## Manifest Configuration

The `manifest.json` includes:

- **App name and description**
- **Icons** - Multiple sizes for different devices
- **Theme colors** - Matches BrewOS branding
- **Display mode** - Standalone (no browser UI)
- **Start URL** - Opens to dashboard
- **Shortcuts** - Quick actions from app icon

## Browser Compatibility

| Feature | Chrome | Firefox | Safari | Edge |
|---------|--------|---------|--------|------|
| Installation | ✅ | ✅ | ✅ (iOS 11.3+) | ✅ |
| Offline | ✅ | ✅ | ✅ | ✅ |
| Push Notifications | ✅ | ✅ | ❌ | ✅ |
| Background Sync | ✅ | ✅ | ❌ | ✅ |

## Security

- **HTTPS required** - PWAs require secure context (HTTPS or localhost)
- **Service worker scope** - Limited to app domain
- **Content Security Policy** - Enforced for safe execution

## Best Practices

1. **Keep service worker small** - Only essential caching logic
2. **Version caches** - Use versioned cache names for easy updates
3. **Test offline** - Regularly test offline functionality
4. **Monitor updates** - Check service worker updates in production
5. **Handle errors** - Graceful fallbacks for offline scenarios

