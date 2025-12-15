import { defineConfig, loadEnv, Plugin } from "vite";
import react from "@vitejs/plugin-react";
import viteCompression from "vite-plugin-compression";
import path from "path";
import fs from "fs";

/**
 * Build Modes:
 *
 * - Default (cloud): npm run build
 *   Sets __CLOUD__=true, __ESP32__=false
 *   For cloud.brewos.io deployment
 *   Demo mode enabled for website visitors
 *
 * - ESP32: npm run build:esp32 (or --mode esp32)
 *   Sets __ESP32__=true, __CLOUD__=false
 *   For ESP32 local deployment
 *   Demo mode disabled (real hardware)
 *   Outputs to ../esp32/data with aggressive minification
 *
 * Environment Variables:
 * - RELEASE_VERSION: Set by CI during release builds (e.g., "0.2.0")
 * - VITE_ENVIRONMENT: "staging" or "production"
 */

/**
 * Plugin to inject build version into service worker
 *
 * For production builds: Injects version + timestamp at closeBundle
 * For dev: Serves transformed sw.js via middleware (no file modification)
 *
 * This ensures cache is invalidated on deployments without modifying source files during dev.
 */
function serviceWorkerVersionPlugin(version: string, isDev: boolean): Plugin {
  const generateCacheVersion = () => {
    const buildTime = new Date().toISOString().replace(/[:.]/g, "-");
    return `${version}-${buildTime}`;
  };

  const transformSwContent = (
    content: string,
    cacheVersion: string,
    isDevMode: boolean
  ) => {
    let transformed = content;

    // Replace CACHE_VERSION
    transformed = transformed.replace(
      /const CACHE_VERSION = "[^"]+";/,
      `const CACHE_VERSION = "${cacheVersion}";`
    );

    // Replace IS_DEV_MODE
    transformed = transformed.replace(
      /const IS_DEV_MODE = [^;]+;/,
      `const IS_DEV_MODE = ${isDevMode};`
    );

    return transformed;
  };

  return {
    name: "sw-version-inject",
    apply: isDev ? undefined : "build",

    // For dev server: intercept sw.js requests and transform on-the-fly
    configureServer(server) {
      if (isDev) {
        const cacheVersion = generateCacheVersion();
        console.log(
          `[SW Plugin] Dev mode - will serve sw.js with version: ${cacheVersion}`
        );

        server.middlewares.use((req, res, next) => {
          if (req.url === "/sw.js") {
            const swPath = path.resolve(__dirname, "public", "sw.js");

            if (fs.existsSync(swPath)) {
              const content = fs.readFileSync(swPath, "utf-8");
              const transformed = transformSwContent(
                content,
                cacheVersion,
                true
              );

              res.setHeader("Content-Type", "application/javascript");
              res.setHeader("Service-Worker-Allowed", "/");
              res.end(transformed);
              return;
            }
          }
          next();
        });
      }
    },

    // For builds: inject at closeBundle (modifies dist output only)
    closeBundle() {
      if (!isDev) {
        const swPath = path.resolve(__dirname, "dist", "sw.js");

        if (!fs.existsSync(swPath)) {
          console.warn(
            "[SW Plugin] sw.js not found in dist, skipping version injection"
          );
          return;
        }

        const cacheVersion = generateCacheVersion();
        const content = fs.readFileSync(swPath, "utf-8");
        const transformed = transformSwContent(content, cacheVersion, false);

        fs.writeFileSync(swPath, transformed);
        console.log(
          `[SW Plugin] Build - injected cache version: ${cacheVersion}`
        );
      }
    },
  };
}

export default defineConfig(({ mode, command }) => {
  const isEsp32 = mode === "esp32";
  const isDev = command === "serve";
  const env = loadEnv(mode, process.cwd(), "");

  // Version from RELEASE_VERSION env var (set by CI) or 'dev' for local builds
  const version = process.env.RELEASE_VERSION || "dev";
  // ESP32 builds should use "production" mode (not mock data), cloud defaults to env var or "development"
  const environment = isEsp32
    ? "production"
    : env.VITE_ENVIRONMENT || "development";

  return {
    plugins: [
      react(),
      // Inject version into service worker (all modes except ESP32)
      !isEsp32 && serviceWorkerVersionPlugin(version, isDev),
      // Gzip compression for ESP32 builds - pre-compress files for faster serving
      // ESPAsyncWebServer's serveStatic automatically serves .gz files when available
      isEsp32 &&
        viteCompression({
          algorithm: "gzip",
          ext: ".gz",
          threshold: 1024, // Only compress files > 1KB
          deleteOriginFile: true, // Delete originals to fit in 1.5MB partition
          // Exclude index.html - keep it uncompressed for SPA fallback handler
          // (only 2.5KB, not worth the complexity of handling gzipped default file)
          filter: (file) => !/index\.html$/.test(file),
        }),
    ].filter(Boolean),
    resolve: {
      alias: {
        "@": path.resolve(__dirname, "./src"),
      },
    },
    // Compile-time constants
    define: {
      __ESP32__: isEsp32,
      __CLOUD__: !isEsp32,
      __VERSION__: JSON.stringify(version),
      __ENVIRONMENT__: JSON.stringify(environment),
    },
    build: {
      outDir: isEsp32 ? "../esp32/data" : "dist",
      emptyOutDir: true,
      minify: isEsp32 ? "terser" : "esbuild",
      terserOptions: isEsp32
        ? {
            compress: {
              drop_console: true,
              drop_debugger: true,
            },
          }
        : undefined,
      rollupOptions: {
        output: {
          manualChunks: (id) => {
            // For ESP32 builds, use aggressive code splitting to reduce initial bundle
            if (isEsp32) {
              // Core React libraries
              if (
                id.includes("node_modules/react") ||
                id.includes("node_modules/react-dom")
              ) {
                return "react-vendor";
              }
              // Router
              if (id.includes("node_modules/react-router")) {
                return "router";
              }
              // State management
              if (id.includes("node_modules/zustand")) {
                return "state";
              }
              // Icons (lucide-react can be large)
              if (id.includes("node_modules/lucide-react")) {
                return "icons";
              }
              // QR code libraries - don't split separately to avoid circular dependency issues
              // They'll be bundled with the components that use them (lazy loaded)
              // OAuth (only used in cloud mode)
              if (id.includes("node_modules/@react-oauth")) {
                return "oauth";
              }
              // Utilities (small, can be bundled)
              if (
                id.includes("node_modules/clsx") ||
                id.includes("node_modules/tailwind-merge")
              ) {
                return "utils";
              }
              // Large pages - split by route
              if (id.includes("/pages/Brewing")) {
                return "page-brewing";
              }
              if (id.includes("/pages/Stats")) {
                return "page-stats";
              }
              if (
                id.includes("/pages/Settings") ||
                id.includes("/pages/settings")
              ) {
                return "page-settings";
              }
              if (id.includes("/pages/Diagnostics")) {
                return "page-diagnostics";
              }
              if (id.includes("/pages/Schedules")) {
                return "page-schedules";
              }
            } else {
              // Cloud builds: simpler chunking
              if (
                id.includes("node_modules/react") ||
                id.includes("node_modules/react-dom") ||
                id.includes("node_modules/react-router-dom")
              ) {
                return "vendor";
              }
            }
          },
        },
      },
    },
    publicDir: "public",
    server: {
      port: 3000,
      headers: {
        // Allow Google Sign-In popup to communicate back
        "Cross-Origin-Opener-Policy": "same-origin-allow-popups",
      },
      proxy: {
        "/ws": {
          // Use localhost:3001 for local cloud dev, brewos.local for ESP32 dev
          // Note: For local ESP32, ws:// is correct (local network, no SSL)
          // For local cloud dev, ws:// is fine (localhost, no SSL needed)
          // In production/cloud, the connection.ts will use wss:// automatically
          target: isEsp32 ? "ws://brewos.local" : "ws://localhost:3001",
          ws: true,
        },
        "/api": {
          target: isEsp32 ? "http://brewos.local" : "http://localhost:3001",
        },
        // Admin UI is served by cloud server (separate React app)
        // Note: Express redirects /admin to /admin/, so we need both patterns
        "^/admin": {
          target: "http://localhost:3001",
        },
      },
    },
  };
});
