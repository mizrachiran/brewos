// Service Worker for BrewOS PWA
//
// IMPORTANT: CACHE_VERSION and IS_DEV_MODE are auto-injected by vite.config.ts
// - CACHE_VERSION format: "{version}-{timestamp}" e.g., "0.2.0-2024-01-15T12-30-45-123Z"
// - IS_DEV_MODE is true during dev server, false for production builds
// This ensures cache is invalidated on every deployment and during development.
//
const CACHE_VERSION = "dev-2025-12-07T07-12-21-678Z";
const IS_DEV_MODE = true;
const STATIC_CACHE_NAME = `brewos-static-${CACHE_VERSION}`;
const RUNTIME_CACHE_NAME = `brewos-runtime-${CACHE_VERSION}`;

// Core app shell - cached on install
const APP_SHELL = [
  "/",
  "/index.html",
  "/favicon.svg",
  "/logo-icon.svg",
  "/logo.png",
  "/manifest.json",
];

// Patterns for different caching strategies
// In dev mode: use network-first for JS/CSS to always get fresh code
// In prod: use cache-first for performance
const CACHE_FIRST_PATTERNS = [
  /\.(?:woff2?|ttf|eot|ico|svg|png|jpg|jpeg|webp)$/, // Fonts and images only
  /^https:\/\/fonts\.(?:googleapis|gstatic)\.com/,
];

const NETWORK_FIRST_PATTERNS = [
  /\/api\//,
  /\/ws/,
  // In dev: always fetch JS/CSS fresh to avoid stale code
  ...(IS_DEV_MODE ? [/\.(?:js|css)$/] : []),
];

// In production, also cache JS/CSS with cache-first for performance
const PROD_CACHE_FIRST_PATTERNS = [
  ...CACHE_FIRST_PATTERNS,
  ...(!IS_DEV_MODE ? [/\.(?:js|css)$/] : []),
];

// Install event - cache app shell
self.addEventListener("install", (event) => {
  console.log(`[SW] Installing ${CACHE_VERSION}...`);
  event.waitUntil(
    caches
      .open(STATIC_CACHE_NAME)
      .then((cache) => {
        console.log("[SW] Caching app shell");
        return cache.addAll(APP_SHELL);
      })
      .then(() => self.skipWaiting())
  );
});

// Activate event - clean up old caches
self.addEventListener("activate", (event) => {
  console.log(`[SW] Activating ${CACHE_VERSION}...`);
  event.waitUntil(
    caches
      .keys()
      .then((cacheNames) => {
        return Promise.all(
          cacheNames
            .filter((name) => !name.includes(CACHE_VERSION))
            .map((name) => {
              console.log("[SW] Deleting old cache:", name);
              return caches.delete(name);
            })
        );
      })
      .then(() => self.clients.claim())
  );
});

// Fetch event - smart caching strategy
self.addEventListener("fetch", (event) => {
  const { request } = event;
  const url = new URL(request.url);

  // Skip non-GET requests and WebSocket
  if (
    request.method !== "GET" ||
    url.protocol === "ws:" ||
    url.protocol === "wss:"
  ) {
    return;
  }

  // Skip chrome-extension and other non-http(s) protocols
  if (!url.protocol.startsWith("http")) {
    return;
  }

  // Network-first for API calls (but with fast timeout)
  if (NETWORK_FIRST_PATTERNS.some((pattern) => pattern.test(url.href))) {
    event.respondWith(networkFirstWithTimeout(request, 3000));
    return;
  }

  // Cache-first for static assets (fonts, images, and in prod: JS/CSS)
  if (PROD_CACHE_FIRST_PATTERNS.some((pattern) => pattern.test(url.href))) {
    event.respondWith(cacheFirst(request));
    return;
  }

  // Navigation requests - use network-first with short timeout
  // This ensures users get fresh content after deployments, with fallback to cache if offline
  if (request.mode === "navigate") {
    event.respondWith(networkFirstWithTimeout(request, 2000));
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
    return caches.match("/index.html");
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
  return caches.match("/index.html");
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

    // For navigation requests, fall back to cached index.html
    if (request.mode === "navigate") {
      return caches.match("/index.html");
    }

    // Return error response for API calls
    return new Response(JSON.stringify({ error: "offline" }), {
      status: 503,
      headers: { "Content-Type": "application/json" },
    });
  }
}

// Push notification handling
self.addEventListener("push", (event) => {
  console.log("[SW] Push notification received");

  let notificationData = {
    title: "BrewOS",
    body: "You have a new notification",
    icon: "/logo-icon.svg",
    badge: "/logo-icon.svg",
    tag: "brewos-notification",
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
self.addEventListener("notificationclick", (event) => {
  event.notification.close();

  const urlToOpen = event.notification.data?.url || "/";

  event.waitUntil(
    clients
      .matchAll({ type: "window", includeUncontrolled: true })
      .then((clientList) => {
        for (const client of clientList) {
          if (client.url.includes(self.location.origin) && "focus" in client) {
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
self.addEventListener("message", (event) => {
  if (event.data?.type === "SKIP_WAITING") {
    self.skipWaiting();
  }

  // Clear caches on demand
  if (event.data?.type === "CLEAR_CACHE") {
    event.waitUntil(
      caches
        .keys()
        .then((names) => Promise.all(names.map((name) => caches.delete(name))))
    );
  }
});
