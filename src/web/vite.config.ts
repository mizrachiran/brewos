import { defineConfig, loadEnv } from "vite";
import react from "@vitejs/plugin-react";
import path from "path";

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
export default defineConfig(({ mode }) => {
  const isEsp32 = mode === "esp32";
  const env = loadEnv(mode, process.cwd(), "");

  // Version from RELEASE_VERSION env var (set by CI) or 'dev' for local builds
  const version = process.env.RELEASE_VERSION || "dev";
  const environment = env.VITE_ENVIRONMENT || "development";

  return {
    plugins: [react()],
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
