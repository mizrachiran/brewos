import { defineConfig, loadEnv, Plugin } from "vite";
import react from "@vitejs/plugin-react";
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
  const environment = env.VITE_ENVIRONMENT || "development";

  return {
    plugins: [
      react(),
      // Inject version into service worker (all modes except ESP32)
      !isEsp32 && serviceWorkerVersionPlugin(version, isDev),
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
          manualChunks: isEsp32
            ? undefined
            : {
                vendor: ["react", "react-dom", "react-router-dom"],
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
          target: isEsp32 ? "ws://brewos.local" : "ws://localhost:3001",
          ws: true,
        },
        "/api": {
          target: isEsp32 ? "http://brewos.local" : "http://localhost:3001",
        },
      },
    },
  };
});
