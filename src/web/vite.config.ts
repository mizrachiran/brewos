import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import path from 'path'

export default defineConfig(({ mode }) => {
  const isEsp32 = mode === 'esp32'
  
  return {
    plugins: [react()],
    resolve: {
      alias: {
        '@': path.resolve(__dirname, './src'),
      },
    },
    define: {
      __ESP32__: isEsp32,
      __CLOUD__: !isEsp32,
    },
    build: {
      outDir: isEsp32 ? '../esp32/data' : 'dist',
      emptyOutDir: true,
      minify: isEsp32 ? 'terser' : 'esbuild',
      terserOptions: isEsp32 ? {
        compress: {
          drop_console: true,
          drop_debugger: true,
        },
      } : undefined,
      rollupOptions: {
        output: {
          manualChunks: isEsp32 ? undefined : {
            vendor: ['react', 'react-dom', 'react-router-dom'],
          },
        },
      },
    },
    publicDir: 'public',
    server: {
      port: 3000,
      proxy: {
        '/ws': {
          target: 'ws://brewos.local',
          ws: true,
        },
        '/api': {
          target: 'http://brewos.local',
        },
      },
    },
  }
})

