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
import { initDatabase } from "./lib/database.js";
import {
  cleanupExpiredTokens,
  cleanupOrphanedDevices,
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
  // Prevent clickjacking
  res.setHeader("X-Frame-Options", "DENY");
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

// Rate limiter for static file endpoints (prevents abuse)
const staticFileLimiter = rateLimit({
  windowMs: 60 * 1000, // 1 minute
  max: 60, // limit each IP to 60 requests per minute
  standardHeaders: true,
  legacyHeaders: false,
});

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
      } catch {
        // Ignore errors reading directory
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
      // Require TLS 1.2 or higher
      minVersion: "TLSv1.2",
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

// Health check
app.get("/api/health", (_req, res) => {
  res.json({
    status: "ok",
    timestamp: new Date().toISOString(),
    connections: {
      devices: deviceRelay.getConnectedDeviceCount(),
      clients: clientProxy.getConnectedClientCount(),
    },
    sessionCache: getCacheStats(),
  });
});

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
app.use("/api/devices", devicesRouter);
app.use("/api/push", pushRouter);

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

// Initialize database and start server
async function start() {
  try {
    // Initialize SQLite database
    await initDatabase();

    // Initialize push notifications
    initializePushNotifications();

    // Start session cache batch update interval
    startBatchUpdateInterval();

    // Cleanup expired tokens and sessions periodically (every 1 hours)
    setInterval(() => {
      try {
        // Cleanup expired claim tokens
        const deletedTokens = cleanupExpiredTokens();
        if (deletedTokens > 0) {
          console.log(
            `[Cleanup] Deleted ${deletedTokens} expired claim tokens`
          );
        }

        // Cleanup expired sessions
        const deletedSessions = cleanupExpiredSessions();
        if (deletedSessions > 0) {
          console.log(`[Cleanup] Deleted ${deletedSessions} expired sessions`);
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
║  ${protocol.toUpperCase()}:    ${protocol}://localhost:${port}${" ".repeat(
        Math.max(0, 30 - protocol.length - port.toString().length)
      )}║
║  Device:  ${wsProtocol}://localhost:${port}/ws/device${" ".repeat(
        Math.max(0, 30 - wsProtocol.length - port.toString().length - 12)
      )}║
║  Client:  ${wsProtocol}://localhost:${port}/ws/client${" ".repeat(
        Math.max(0, 30 - wsProtocol.length - port.toString().length - 12)
      )}║
║  DB:      SQLite (sql.js)${" ".repeat(30)}║
║  SSL:     ${
        isProduction && sslCertPath && sslKeyPath
          ? "Enabled ✓"
          : "Disabled (dev/proxy)"
      }${" ".repeat(
        Math.max(0, 30 - (isProduction && sslCertPath && sslKeyPath ? 18 : 23))
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
  deviceWss.close();
  clientWss.close();
  server.close();
  process.exit(0);
});
