import "dotenv/config";
import express from "express";
import rateLimit from "express-rate-limit";
import cors from "cors";
import path from "path";
import { createServer, Server as HttpServer } from "http";
import {
  createServer as createHttpsServer,
  Server as HttpsServer,
} from "https";
import { readFileSync, existsSync, readdirSync } from "fs";
import { WebSocketServer } from "ws";
import type { SecureContextOptions } from "tls";
import { DeviceRelay } from "./device-relay.js";
import { ClientProxy } from "./client-proxy.js";
import authRouter from "./routes/auth.js";
import devicesRouter from "./routes/devices.js";
import pushRouter from "./routes/push.js";
import adminRouter from "./routes/admin.js";
import { initDatabase } from "./lib/database.js";
import {
  cleanupExpiredTokens,
  cleanupOrphanedDevices,
  markAllDevicesOffline,
} from "./services/device.js";
import {
  cleanupExpiredSessions,
  startBatchUpdateInterval,
  stopBatchUpdateInterval,
  getCacheStats,
} from "./services/session.js";
import { initializePushNotifications } from "./services/push.js";

const app = express();
const port = process.env.PORT || 3001;

// Trust proxy - required when behind reverse proxy (nginx, Caddy, etc.)
// This allows Express to correctly identify client IPs and handle X-Forwarded-* headers
app.set("trust proxy", true);

// Middleware
// CORS configuration - CORS_ORIGIN must be set in production
const corsOrigin = process.env.CORS_ORIGIN;
if (!corsOrigin && process.env.NODE_ENV === "production") {
  console.warn(
    "[Security] CORS_ORIGIN not set in production! Defaulting to restrictive policy."
  );
}

// Determine allowed origins based on environment
const DEV_ORIGINS = [
  "http://localhost:5173",
  "http://localhost:3001",
  "http://127.0.0.1:5173",
];
const getAllowedOrigins = (): string | string[] | false => {
  if (corsOrigin) return corsOrigin;
  if (process.env.NODE_ENV === "production") return false; // Reject all if not configured
  return DEV_ORIGINS;
};

app.use(
  cors({
    origin: getAllowedOrigins(),
    credentials: true,
  })
);

// Limit JSON body size to prevent DoS attacks
app.use(express.json({ limit: "100kb" }));

// Enforce HTTPS in production
if (process.env.NODE_ENV === "production") {
  app.use((req, res, next) => {
    // Check if request is secure (HTTPS) or forwarded as secure (behind proxy)
    const isSecure =
      req.secure ||
      req.headers["x-forwarded-proto"] === "https" ||
      req.headers["x-forwarded-ssl"] === "on";

    if (!isSecure) {
      // Redirect HTTP to HTTPS
      const httpsUrl = `https://${req.headers.host}${req.url}`;
      return res.redirect(301, httpsUrl);
    }
    next();
  });
}

// Security headers
app.use((_req, res, next) => {
  // COOP: same-origin-allow-popups allows the Google Sign-In popup to communicate back
  res.setHeader("Cross-Origin-Opener-Policy", "same-origin-allow-popups");
  
  // Content Security Policy - Allow Google Sign-In iframes while preventing clickjacking
  // frame-src allows Google Sign-In button iframes
  // script-src allows Google Sign-In scripts
  const csp = [
    "default-src 'self'",
    "frame-src 'self' https://accounts.google.com https://*.google.com",
    "script-src 'self' 'unsafe-inline' 'unsafe-eval' https://accounts.google.com https://*.google.com https://www.googletagmanager.com https://www.google-analytics.com",
    "connect-src 'self' https://accounts.google.com https://*.google.com wss://cloud.brewos.io ws://localhost:* http://localhost:*",
    "style-src 'self' 'unsafe-inline' https://fonts.googleapis.com",
    "font-src 'self' https://fonts.gstatic.com",
    "img-src 'self' data: https: blob:",
  ].join("; ");
  res.setHeader("Content-Security-Policy", csp);
  
  // X-Frame-Options is less flexible than CSP, but keep it for older browsers
  // SAMEORIGIN allows same-origin iframes (our app) but blocks cross-origin except where CSP allows
  res.setHeader("X-Frame-Options", "SAMEORIGIN");
  
  // Prevent MIME type sniffing
  res.setHeader("X-Content-Type-Options", "nosniff");
  // Enable XSS protection (legacy browsers)
  res.setHeader("X-XSS-Protection", "1; mode=block");
  // Strict transport security (HTTPS only) - only in production
  if (process.env.NODE_ENV === "production") {
    res.setHeader(
      "Strict-Transport-Security",
      "max-age=31536000; includeSubDomains; preload"
    );
  }
  next();
});

// Serve static files (the shared web UI)
const webDistPath = path.resolve(
  process.env.WEB_DIST_PATH || path.join(process.cwd(), "../web/dist")
);

// Admin UI path (relative to cloud service)
const adminDistPath = path.resolve(
  process.env.ADMIN_DIST_PATH || path.join(process.cwd(), "admin/dist")
);

// Rate limiter for static file endpoints (prevents abuse)
const staticFileLimiter = rateLimit({
  windowMs: 60 * 1000, // 1 minute
  max: 60, // limit each IP to 60 requests per minute
  standardHeaders: true,
  legacyHeaders: false,
});

// Serve admin UI at /admin
// Admin assets with long cache (hashed files)
app.use(
  "/admin/assets",
  express.static(path.join(adminDistPath, "assets"), {
    maxAge: "1y",
    immutable: true,
  })
);

// Admin SPA fallback - serve index.html for all admin routes
// MUST come before the static middleware to prevent redirect from /admin to /admin/
app.get("/admin", staticFileLimiter, (_req, res) => {
  res.setHeader("Cache-Control", "no-cache, must-revalidate");
  res.sendFile(path.join(adminDistPath, "index.html"));
});

app.get("/admin/*", staticFileLimiter, (_req, res) => {
  res.setHeader("Cache-Control", "no-cache, must-revalidate");
  res.sendFile(path.join(adminDistPath, "index.html"));
});

// Admin static files (for favicon.svg and other static assets)
// Note: redirect: false prevents Express from redirecting /admin to /admin/
app.use(
  "/admin",
  express.static(adminDistPath, {
    maxAge: "1h",
    etag: true,
    lastModified: true,
    redirect: false,
  })
);

// Cache control for service worker - MUST never be cached by browsers
// This ensures new deployments are detected immediately
app.get("/sw.js", staticFileLimiter, (_req, res) => {
  res.setHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  res.setHeader("Pragma", "no-cache");
  res.setHeader("Expires", "0");
  res.sendFile(path.join(webDistPath, "sw.js"));
});

// Cache control for index.html - should revalidate on each request
app.get("/index.html", staticFileLimiter, (_req, res) => {
  res.setHeader("Cache-Control", "no-cache, must-revalidate");
  res.sendFile(path.join(webDistPath, "index.html"));
});

// Static files with proper caching:
// - Hashed assets (/assets/*) can be cached forever (immutable)
// - Other files should revalidate
app.use(
  "/assets",
  express.static(path.join(webDistPath, "assets"), {
    maxAge: "1y",
    immutable: true,
  })
);

app.use(
  express.static(webDistPath, {
    // Other static files: cache for 1 hour but always revalidate
    maxAge: "1h",
    etag: true,
    lastModified: true,
  })
);

// Create HTTP/HTTPS server based on configuration
let server: HttpServer | HttpsServer;
const isProduction = process.env.NODE_ENV === "production";

// Determine SSL certificate paths
// Priority: 1. Explicit paths from env, 2. Let's Encrypt auto-detection, 3. None
let sslCertPath = process.env.SSL_CERT_PATH;
let sslKeyPath = process.env.SSL_KEY_PATH;

// Auto-detect Let's Encrypt certificates if not explicitly set
if (isProduction && (!sslCertPath || !sslKeyPath)) {
  const letsEncryptDomain = process.env.LETSENCRYPT_DOMAIN;
  if (letsEncryptDomain) {
    const letsEncryptCertPath = `/etc/letsencrypt/live/${letsEncryptDomain}/fullchain.pem`;
    const letsEncryptKeyPath = `/etc/letsencrypt/live/${letsEncryptDomain}/privkey.pem`;

    if (existsSync(letsEncryptCertPath) && existsSync(letsEncryptKeyPath)) {
      sslCertPath = letsEncryptCertPath;
      sslKeyPath = letsEncryptKeyPath;
      console.log(
        `[Security] Auto-detected Let's Encrypt certificates for ${letsEncryptDomain}`
      );
    } else {
      console.warn(
        `[Security] Let's Encrypt certificates not found at: ${letsEncryptCertPath}`
      );
    }
  } else {
    // Try to auto-detect domain from common locations
    // Check if /etc/letsencrypt/live exists and has a domain
    const letsEncryptLive = "/etc/letsencrypt/live";
    if (existsSync(letsEncryptLive)) {
      try {
        const domains = readdirSync(letsEncryptLive, { withFileTypes: true })
          .filter((dirent) => dirent.isDirectory())
          .map((dirent) => dirent.name);

        if (domains.length > 0) {
          // Use the first domain found
          const domain = domains[0];
          const certPath = `${letsEncryptLive}/${domain}/fullchain.pem`;
          const keyPath = `${letsEncryptLive}/${domain}/privkey.pem`;

          if (existsSync(certPath) && existsSync(keyPath)) {
            sslCertPath = certPath;
            sslKeyPath = keyPath;
            console.log(
              `[Security] Auto-detected Let's Encrypt certificates for ${domain}`
            );
            console.log(
              `[Security] Set LETSENCRYPT_DOMAIN=${domain} to explicitly specify domain`
            );
          }
        }
      } catch (error) {
        console.warn(
          `[Security] Failed to read Let's Encrypt directory at ${letsEncryptLive}:`,
          error
        );
      }
    }
  }
}

if (isProduction && sslCertPath && sslKeyPath) {
  // Production: Use HTTPS with SSL certificates
  try {
    // Verify files exist before trying to read them
    if (!existsSync(sslCertPath)) {
      throw new Error(`Certificate file not found: ${sslCertPath}`);
    }
    if (!existsSync(sslKeyPath)) {
      throw new Error(`Private key file not found: ${sslKeyPath}`);
    }

    const sslOptions: SecureContextOptions = {
      cert: readFileSync(sslCertPath),
      key: readFileSync(sslKeyPath),
      // Require TLS 1.3 or higher
      minVersion: "TLSv1.3",
    };

    // Optional: Add CA certificate for client certificate validation
    if (process.env.SSL_CA_PATH) {
      if (existsSync(process.env.SSL_CA_PATH)) {
        sslOptions.ca = readFileSync(process.env.SSL_CA_PATH);
      } else {
        console.warn(
          `[Security] CA certificate file not found: ${process.env.SSL_CA_PATH}`
        );
      }
    }

    server = createHttpsServer(sslOptions, app);
    console.log(`[Security] HTTPS enabled with SSL certificates`);
    console.log(`[Security] Certificate: ${sslCertPath}`);
    console.log(`[Security] Private key: ${sslKeyPath}`);
  } catch (error) {
    console.error(
      "[Security] Failed to load SSL certificates, falling back to HTTP:",
      error
    );
    console.warn(
      "[Security] WARNING: Running in production without HTTPS is insecure!"
    );
    server = createServer(app);
  }
} else if (isProduction) {
  // Production without SSL certificates
  // Require explicit acknowledgment that SSL is handled elsewhere (reverse proxy)
  const trustProxy = process.env.TRUST_PROXY === "true";

  if (!trustProxy) {
    console.error(
      "[Security] ERROR: Production mode requires SSL certificates!"
    );
    console.error("[Security] Options:");
    console.error(
      "  1. Set SSL_CERT_PATH and SSL_KEY_PATH environment variables"
    );
    console.error("  2. Set LETSENCRYPT_DOMAIN for auto-detection");
    console.error(
      "  3. Set TRUST_PROXY=true if behind a reverse proxy with SSL termination"
    );
    throw new Error(
      "SSL certificates required in production. Set TRUST_PROXY=true to bypass if using a reverse proxy."
    );
  }

  // Explicit reverse proxy mode
  console.log(
    "[Security] TRUST_PROXY enabled - assuming SSL termination at reverse proxy"
  );
  console.warn(
    "[Security] ⚠️  Ensure your reverse proxy (nginx, traefik, Caddy) handles SSL!"
  );
  server = createServer(app);
} else {
  // Development: Use HTTP
  server = createServer(app);
  console.log("[Security] HTTP mode (development)");
}

// WebSocket server for ESP32 devices
const deviceWss = new WebSocketServer({ noServer: true });
const deviceRelay = new DeviceRelay(deviceWss);

// WebSocket server for client apps (web, mobile)
const clientWss = new WebSocketServer({ noServer: true });
const clientProxy = new ClientProxy(clientWss, deviceRelay);

// Health check - enhanced with detailed stats
app.get("/api/health", (_req, res) => {
  const memUsage = process.memoryUsage();
  const deviceStats = deviceRelay.getStats();
  const clientStats = clientProxy.getStats();

  res.json({
    status: "ok",
    timestamp: new Date().toISOString(),
    uptime: {
      seconds: Math.floor(process.uptime()),
      formatted: formatUptime(process.uptime()),
    },
    connections: {
      devices: deviceStats.connectedDevices,
      clients: clientStats.connectedClients,
      totalClientConnections: clientStats.totalConnections,
      totalMessagesRelayed: clientStats.totalMessages,
    },
    messageQueue: {
      pendingMessages: clientStats.queuedMessagesTotal,
    },
    memory: {
      heapUsed: Math.round(memUsage.heapUsed / 1024 / 1024),
      heapTotal: Math.round(memUsage.heapTotal / 1024 / 1024),
      external: Math.round(memUsage.external / 1024 / 1024),
      rss: Math.round(memUsage.rss / 1024 / 1024),
    },
    sessionCache: getCacheStats(),
  });
});

// Detailed health check (more verbose, for debugging)
app.get("/api/health/detailed", (_req, res) => {
  const memUsage = process.memoryUsage();
  const deviceStats = deviceRelay.getStats();
  const clientStats = clientProxy.getStats();

  // Check if GC is available and when last run occurred
  const gcAvailable = typeof global.gc === "function";

  res.json({
    status: "ok",
    timestamp: new Date().toISOString(),
    uptime: {
      seconds: Math.floor(process.uptime()),
      formatted: formatUptime(process.uptime()),
    },
    devices: deviceStats,
    clients: clientStats,
    memory: {
      heapUsed: Math.round(memUsage.heapUsed / 1024 / 1024),
      heapTotal: Math.round(memUsage.heapTotal / 1024 / 1024),
      heapUsedBytes: memUsage.heapUsed,
      heapTotalBytes: memUsage.heapTotal,
      external: Math.round(memUsage.external / 1024 / 1024),
      rss: Math.round(memUsage.rss / 1024 / 1024),
      arrayBuffers: Math.round(memUsage.arrayBuffers / 1024 / 1024),
      heapUsagePercent: ((memUsage.heapUsed / memUsage.heapTotal) * 100).toFixed(1),
      // Note: High percentage is normal - Node.js grows heap on demand
      note: "Node.js dynamically grows heap as needed (up to ~4GB on 64-bit). High % is normal.",
    },
    sessionCache: getCacheStats(),
    node: {
      version: process.version,
      platform: process.platform,
      arch: process.arch,
      gcAvailable,
    },
  });
});

// Force garbage collection (requires --expose-gc flag)
// Useful for debugging memory issues
app.post("/api/health/gc", (_req, res) => {
  if (typeof global.gc !== "function") {
    res.status(400).json({
      error: "GC not exposed. Start node with --expose-gc flag to enable this endpoint.",
    });
    return;
  }

  const beforeMem = process.memoryUsage();
  global.gc();
  const afterMem = process.memoryUsage();

  res.json({
    status: "gc_complete",
    before: {
      heapUsed: Math.round(beforeMem.heapUsed / 1024 / 1024),
      heapTotal: Math.round(beforeMem.heapTotal / 1024 / 1024),
    },
    after: {
      heapUsed: Math.round(afterMem.heapUsed / 1024 / 1024),
      heapTotal: Math.round(afterMem.heapTotal / 1024 / 1024),
    },
    freed: Math.round((beforeMem.heapUsed - afterMem.heapUsed) / 1024 / 1024),
  });
});

// Helper function for formatting uptime
function formatUptime(seconds: number): string {
  const days = Math.floor(seconds / 86400);
  const hours = Math.floor((seconds % 86400) / 3600);
  const minutes = Math.floor((seconds % 3600) / 60);
  const secs = Math.floor(seconds % 60);

  const parts: string[] = [];
  if (days > 0) parts.push(`${days}d`);
  if (hours > 0) parts.push(`${hours}h`);
  if (minutes > 0) parts.push(`${minutes}m`);
  parts.push(`${secs}s`);

  return parts.join(" ");
}

// Mode endpoint - tells client this is cloud mode
app.get("/api/mode", (_req, res) => {
  res.json({ mode: "cloud" });
});

// API info endpoint - provides version and feature detection for web UI compatibility
// This is the primary endpoint for version negotiation between web UI and backend
app.get("/api/info", (_req, res) => {
  // API version - increment ONLY for breaking changes to REST/WebSocket APIs
  // Web UI checks this to determine compatibility
  const API_VERSION = 1;

  // Read version from package.json or environment
  const serverVersion = process.env.npm_package_version || "0.2.0";

  res.json({
    // API version for breaking change detection
    apiVersion: API_VERSION,

    // Component versions
    firmwareVersion: serverVersion, // Cloud server version
    webVersion: serverVersion, // Web UI version (deployed with cloud)

    // Mode detection
    mode: "cloud",

    // Feature flags - granular capability detection
    // Cloud has all local features plus cloud-specific ones
    features: [
      // Core features
      "temperature_control",
      "pressure_monitoring",
      "power_monitoring",

      // Advanced features
      "bbw", // Brew-by-weight (proxied to device)
      "scale", // BLE scale support (proxied to device)
      "mqtt", // MQTT integration (proxied to device)
      "eco_mode", // Eco mode (proxied to device)
      "statistics", // Statistics tracking
      "schedules", // Schedule management

      // OTA features
      "pico_ota", // Pico firmware updates (proxied to device)
      "esp32_ota", // ESP32 firmware updates (proxied to device)

      // Cloud-only features
      "push_notifications", // Push notifications via cloud
      "remote_access", // Remote access capability
      "multi_device", // Multiple device management

      // Debug features
      "debug_console", // Debug console
      "protocol_debug", // Protocol debugging
    ],

    // Cloud stats
    connectedDevices: deviceRelay.getConnectedDeviceCount(),
    connectedClients: clientProxy.getConnectedClientCount(),
  });
});

// API routes
app.use("/api/auth", authRouter);
// Devices routes - inject device relay for live connection status
app.use(
  "/api/devices",
  (req, _res, next) => {
    // Inject live device connection checker - this is the source of truth for online status
    (req as unknown as Record<string, unknown>).isDeviceConnected = (deviceId: string) => {
      return deviceRelay.isDeviceConnected(deviceId);
    };
    next();
  },
  devicesRouter
);
app.use("/api/push", pushRouter);

// Admin routes - inject device relay and client proxy functions
app.use(
  "/api/admin",
  (req, _res, next) => {
    // Inject helper functions for admin routes
    (req as unknown as Record<string, unknown>).disconnectDevice = (deviceId: string) => {
      return deviceRelay.disconnectDevice(deviceId);
    };
    (req as unknown as Record<string, unknown>).getConnectionStats = () => {
      return {
        devices: deviceRelay.getStats(),
        clients: clientProxy.getStats(),
      };
    };
    // Inject live device connection checker - this is the source of truth for online status
    (req as unknown as Record<string, unknown>).isDeviceConnected = (deviceId: string) => {
      return deviceRelay.isDeviceConnected(deviceId);
    };
    // Inject function to get list of connected device IDs
    (req as unknown as Record<string, unknown>).getConnectedDeviceIds = () => {
      return deviceRelay.getConnectedDevices().map(d => d.id);
    };
    next();
  },
  adminRouter
);

// SPA fallback
app.get("*", staticFileLimiter, (_req, res) => {
  res.sendFile(path.join(webDistPath, "index.html"));
});

// Route WebSocket upgrades based on path
server.on("upgrade", (request, socket, head) => {
  const url = new URL(request.url || "", `http://${request.headers.host}`);

  if (url.pathname === "/ws/device") {
    // ESP32 device connection
    deviceWss.handleUpgrade(request, socket, head, (ws) => {
      deviceWss.emit("connection", ws, request);
    });
  } else if (url.pathname === "/ws/client" || url.pathname === "/ws") {
    // Client app connection
    clientWss.handleUpgrade(request, socket, head, (ws) => {
      clientWss.emit("connection", ws, request);
    });
  } else {
    socket.destroy();
  }
});

// Helper function to calculate spacing for console output alignment
function getSpacing(
  protocol: string,
  port: string | number,
  path = "",
  fixedPadding?: number
): string {
  if (fixedPadding !== undefined) {
    return " ".repeat(Math.max(0, fixedPadding));
  }
  // Calculate length of URL: protocol://localhost:port + path
  const urlLength = `${protocol}://localhost:${port}${path}`.length;
  // Total content width is 57 chars (59 - 2 for the ║ borders)
  // The prefix is "║  XXXX:    " which varies by protocol label
  const labelLength = protocol === "https" || protocol === "wss" ? 10 : 10;
  const remainingSpace = 57 - labelLength - urlLength;
  return " ".repeat(Math.max(0, remainingSpace));
}

// Initialize database and start server
async function start() {
  try {
    // Initialize SQLite database
    await initDatabase();

    // IMPORTANT: Mark all devices as offline on startup
    // This prevents stale online states from a previous server run or crash
    try {
      const offlineCount = markAllDevicesOffline();
      if (offlineCount > 0) {
        console.log(`[Startup] Reset ${offlineCount} device(s) to offline state`);
      }
    } catch (error) {
      console.error("[Startup] Failed to reset device states:", error);
    }

    // Initialize push notifications
    initializePushNotifications();

    // Start session cache batch update interval
    startBatchUpdateInterval();

    // Run cleanup immediately on startup
    try {
      const deletedTokens = cleanupExpiredTokens();
      const deletedSessions = cleanupExpiredSessions();
      if (deletedTokens > 0 || deletedSessions > 0) {
        console.log(`[Cleanup] Startup: ${deletedTokens} tokens, ${deletedSessions} sessions deleted`);
      }
    } catch (error) {
      console.error("[Cleanup] Startup cleanup error:", error);
    }

    // Cleanup expired tokens and sessions periodically (every hour)
    setInterval(() => {
      try {
        // Cleanup expired claim tokens
        const deletedTokens = cleanupExpiredTokens();
        if (deletedTokens > 0) {
          console.log(
            `[Cleanup] Deleted ${deletedTokens} expired claim tokens`
          );
        }

        // Cleanup expired sessions (expired OR unused for 7 days)
        const deletedSessions = cleanupExpiredSessions();
        if (deletedSessions > 0) {
          console.log(`[Cleanup] Deleted ${deletedSessions} expired/stale sessions`);
        }
      } catch (error) {
        console.error("[Cleanup] Error:", error);
      }
    }, 60 * 60 * 1000);

    // Cleanup orphaned devices periodically (every hour)
    // Devices with no users AND not seen in 30 days are deleted
    setInterval(() => {
      try {
        const deletedDevices = cleanupOrphanedDevices(30);
        if (deletedDevices > 0) {
          console.log(`[Cleanup] Deleted ${deletedDevices} orphaned devices`);
        }
      } catch (error) {
        console.error("[Cleanup] Orphaned devices error:", error);
      }
    }, 60 * 60 * 1000);

    // Start server
    const protocol =
      isProduction && sslCertPath && sslKeyPath ? "https" : "http";
    const wsProtocol = isProduction && sslCertPath && sslKeyPath ? "wss" : "ws";

    server.listen(port, () => {
      console.log(`
╔═══════════════════════════════════════════════════════════╗
║                  BrewOS Cloud Service                     ║
╠═══════════════════════════════════════════════════════════╣
║  ${protocol.toUpperCase()}:    ${protocol}://localhost:${port}${getSpacing(
        protocol,
        port
      )}║
║  Device:  ${wsProtocol}://localhost:${port}/ws/device${getSpacing(
        wsProtocol,
        port,
        "/ws/device"
      )}║
║  Client:  ${wsProtocol}://localhost:${port}/ws/client${getSpacing(
        wsProtocol,
        port,
        "/ws/client"
      )}║
║  DB:      SQLite (sql.js)${" ".repeat(30)}║
║  SSL:     ${
        isProduction && sslCertPath && sslKeyPath
          ? "Enabled ✓"
          : "Disabled (dev/proxy)"
      }${getSpacing(
        "", // protocol not needed here
        "", // port not needed here
        "",
        30 - (isProduction && sslCertPath && sslKeyPath ? 0 : 5) // fudge for length difference
      )}║
╚═══════════════════════════════════════════════════════════╝
      `);

      // SSL status is already logged during server creation
    });
  } catch (error) {
    console.error("Failed to start server:", error);
    process.exit(1);
  }
}

start();

// Graceful shutdown
process.on("SIGTERM", () => {
  console.log("[Cloud] Shutting down...");
  stopBatchUpdateInterval(); // Flush pending updates before shutdown
  clientProxy.shutdown(); // Clean up client proxy intervals and timers
  deviceRelay.shutdown(); // Clean up device relay intervals
  deviceWss.close();
  clientWss.close();
  server.close();
  process.exit(0);
});

process.on("SIGINT", () => {
  console.log("[Cloud] Received SIGINT, shutting down...");
  stopBatchUpdateInterval();
  clientProxy.shutdown();
  deviceRelay.shutdown();
  deviceWss.close();
  clientWss.close();
  server.close();
  process.exit(0);
});
