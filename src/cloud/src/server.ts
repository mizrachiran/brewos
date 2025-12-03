import "dotenv/config";
import express from "express";
import cors from "cors";
import path from "path";
import { createServer } from "http";
import { WebSocketServer } from "ws";
import { DeviceRelay } from "./device-relay.js";
import { ClientProxy } from "./client-proxy.js";
import authRouter from "./routes/auth.js";
import devicesRouter from "./routes/devices.js";
import pushRouter from "./routes/push.js";
import { initDatabase } from "./lib/database.js";
import { cleanupExpiredTokens } from "./services/device.js";
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
app.use(
  cors({
    origin: process.env.CORS_ORIGIN || "*",
    credentials: true,
  })
);
app.use(express.json());

// Security headers that allow Google OAuth popup to work
// COOP: same-origin-allow-popups allows the Google Sign-In popup to communicate back
app.use((_req, res, next) => {
  res.setHeader("Cross-Origin-Opener-Policy", "same-origin-allow-popups");
  next();
});

// Serve static files (the shared web UI)
const webDistPath = path.resolve(
  process.env.WEB_DIST_PATH || path.join(process.cwd(), "../web/dist")
);
app.use(express.static(webDistPath));

// Create HTTP server
const server = createServer(app);

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
app.get("*", (_req, res) => {
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

    // Cleanup expired tokens and sessions periodically
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
    }, 5 * 60 * 1000);

    // Start server
    server.listen(port, () => {
      console.log(`
╔═══════════════════════════════════════════════════════════╗
║                  BrewOS Cloud Service                     ║
╠═══════════════════════════════════════════════════════════╣
║  HTTP:    http://localhost:${port}                           ║
║  Device:  ws://localhost:${port}/ws/device                   ║
║  Client:  ws://localhost:${port}/ws/client                   ║
║  DB:      SQLite (sql.js)                                 ║
╚═══════════════════════════════════════════════════════════╝
      `);
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
