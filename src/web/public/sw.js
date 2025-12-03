// Service Worker for BrewOS PWA
// Version: v3 - Fixed response clone issue
const CACHE_VERSION = 'v3';
const STATIC_CACHE_NAME = `brewos-static-${CACHE_VERSION}`;
const RUNTIME_CACHE_NAME = `brewos-runtime-${CACHE_VERSION}`;

// Core app shell - cached on install
const APP_SHELL = [
  '/',
  '/index.html',
  '/favicon.svg',
  '/logo-icon.svg',
  '/logo.png',
  '/manifest.json',
];

// Patterns for different caching strategies
const CACHE_FIRST_PATTERNS = [
  /\.(?:js|css|woff2?|ttf|eot|ico|svg|png|jpg|jpeg|webp)$/,
  /^https:\/\/fonts\.(?:googleapis|gstatic)\.com/,
];

const NETWORK_FIRST_PATTERNS = [
  /\/api\//,
  /\/ws/,
];

// Install event - cache app shell
self.addEventListener('install', (event) => {
  console.log('[SW] Installing v3...');
  event.waitUntil(
    caches.open(STATIC_CACHE_NAME)
      .then((cache) => {
        console.log('[SW] Caching app shell');
        return cache.addAll(APP_SHELL);
      })
      .then(() => self.skipWaiting())
  );
});

// Activate event - clean up old caches
self.addEventListener('activate', (event) => {
  console.log('[SW] Activating v3...');
  event.waitUntil(
    caches.keys()
      .then((cacheNames) => {
        return Promise.all(
          cacheNames
            .filter((name) => !name.includes(CACHE_VERSION))
            .map((name) => {
              console.log('[SW] Deleting old cache:', name);
              return caches.delete(name);
            })
        );
      })
      .then(() => self.clients.claim())
  );
});

// Fetch event - smart caching strategy
self.addEventListener('fetch', (event) => {
  const { request } = event;
  const url = new URL(request.url);

  // Skip non-GET requests and WebSocket
  if (request.method !== 'GET' || url.protocol === 'ws:' || url.protocol === 'wss:') {
    return;
  }

  // Skip chrome-extension and other non-http(s) protocols
  if (!url.protocol.startsWith('http')) {
    return;
  }

  // Network-first for API calls (but with fast timeout)
  if (NETWORK_FIRST_PATTERNS.some(pattern => pattern.test(url.href))) {
    event.respondWith(networkFirstWithTimeout(request, 3000));
    return;
  }

  // Cache-first for static assets (JS, CSS, fonts, images)
  if (CACHE_FIRST_PATTERNS.some(pattern => pattern.test(url.href))) {
    event.respondWith(cacheFirst(request));
    return;
  }

  // Navigation requests - serve cached index.html immediately, revalidate in background
  if (request.mode === 'navigate') {
    event.respondWith(staleWhileRevalidate(request));
    return;
  }

  // Default: stale-while-revalidate
  event.respondWith(staleWhileRevalidate(request));
});

// Cache-first strategy - fast for static assets
async function cacheFirst(request) {
  const cached = await caches.match(request);
  if (cached) {
    return cached;
  }

  try {
    const response = await fetch(request);
    if (response.ok) {
      const cache = await caches.open(RUNTIME_CACHE_NAME);
      cache.put(request, response.clone());
    }
    return response;
  } catch (error) {
    // Return offline fallback if available
    return caches.match('/index.html');
  }
}

// Stale-while-revalidate - instant response, update in background
async function staleWhileRevalidate(request) {
  const cached = await caches.match(request);
  
  const fetchPromise = fetch(request)
    .then(async (response) => {
      if (response.ok) {
        // Clone BEFORE returning, as response body can only be used once
        const responseToCache = response.clone();
        const cache = await caches.open(RUNTIME_CACHE_NAME);
        cache.put(request, responseToCache);
      }
      return response;
    })
    .catch(() => null);

  // Return cached immediately if available
  if (cached) {
    // Update in background (don't await)
    fetchPromise.catch(() => {});
    return cached;
  }

  // Wait for network if no cache
  const response = await fetchPromise;
  if (response) {
    return response;
  }

  // Fallback to index.html for navigation
  return caches.match('/index.html');
}

// Network-first with timeout - for API calls
async function networkFirstWithTimeout(request, timeout) {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), timeout);

  try {
    const response = await fetch(request, { signal: controller.signal });
    clearTimeout(timeoutId);

    if (response.ok) {
      const cache = await caches.open(RUNTIME_CACHE_NAME);
      cache.put(request, response.clone());
    }
    return response;
  } catch (error) {
    clearTimeout(timeoutId);
    
    // Try cache
    const cached = await caches.match(request);
    if (cached) {
      return cached;
    }

    // Return error response for API calls
    return new Response(JSON.stringify({ error: 'offline' }), {
      status: 503,
      headers: { 'Content-Type': 'application/json' },
    });
  }
}

// Push notification handling
self.addEventListener('push', (event) => {
  console.log('[SW] Push notification received');

  let notificationData = {
    title: 'BrewOS',
    body: 'You have a new notification',
    icon: '/logo-icon.svg',
    badge: '/logo-icon.svg',
    tag: 'brewos-notification',
    requireInteraction: false,
    data: {},
  };

  if (event.data) {
    try {
      const data = event.data.json();
      notificationData = {
        title: data.title || notificationData.title,
        body: data.body || notificationData.body,
        icon: data.icon || notificationData.icon,
        badge: data.badge || notificationData.badge,
        tag: data.tag || notificationData.tag,
        requireInteraction: data.requireInteraction || false,
        data: data.data || {},
      };
    } catch (e) {
      notificationData.body = event.data.text();
    }
  }

  event.waitUntil(
    self.registration.showNotification(notificationData.title, {
      body: notificationData.body,
      icon: notificationData.icon,
      badge: notificationData.badge,
      tag: notificationData.tag,
      requireInteraction: notificationData.requireInteraction,
      data: notificationData.data,
      vibrate: [200, 100, 200],
      actions: notificationData.data.actions || [],
    })
  );
});

// Notification click
self.addEventListener('notificationclick', (event) => {
  event.notification.close();

  const urlToOpen = event.notification.data?.url || '/';

  event.waitUntil(
    clients.matchAll({ type: 'window', includeUncontrolled: true })
      .then((clientList) => {
        for (const client of clientList) {
          if (client.url.includes(self.location.origin) && 'focus' in client) {
            return client.focus();
          }
        }
        if (clients.openWindow) {
          return clients.openWindow(urlToOpen);
        }
      })
  );
});

// Message handling
self.addEventListener('message', (event) => {
  if (event.data?.type === 'SKIP_WAITING') {
    self.skipWaiting();
  }
  
  // Clear caches on demand
  if (event.data?.type === 'CLEAR_CACHE') {
    event.waitUntil(
      caches.keys().then(names => 
        Promise.all(names.map(name => caches.delete(name)))
      )
    );
  }
});
