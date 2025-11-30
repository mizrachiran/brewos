import 'dotenv/config';
import express from 'express';
import cors from 'cors';
import path from 'path';
import { createServer } from 'http';
import { WebSocketServer } from 'ws';
import { DeviceRelay } from './device-relay.js';
import { ClientProxy } from './client-proxy.js';
import devicesRouter from './routes/devices.js';
import { initDatabase } from './lib/database.js';
import { cleanupExpiredTokens } from './services/device.js';
const app = express();
const port = process.env.PORT || 3001;
// Middleware
app.use(cors({
    origin: process.env.CORS_ORIGIN || '*',
    credentials: true,
}));
app.use(express.json());
// Serve static files (the shared web UI)
const webDistPath = process.env.WEB_DIST_PATH || path.join(process.cwd(), '../web/dist');
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
app.get('/api/health', (_req, res) => {
    res.json({
        status: 'ok',
        timestamp: new Date().toISOString(),
        devices: deviceRelay.getConnectedDeviceCount(),
        clients: clientProxy.getConnectedClientCount(),
    });
});
// API routes
app.use('/api/devices', devicesRouter);
// SPA fallback
app.get('*', (_req, res) => {
    res.sendFile(path.join(webDistPath, 'index.html'));
});
// Route WebSocket upgrades based on path
server.on('upgrade', (request, socket, head) => {
    const url = new URL(request.url || '', `http://${request.headers.host}`);
    if (url.pathname === '/ws/device') {
        // ESP32 device connection
        deviceWss.handleUpgrade(request, socket, head, (ws) => {
            deviceWss.emit('connection', ws, request);
        });
    }
    else if (url.pathname === '/ws/client' || url.pathname === '/ws') {
        // Client app connection
        clientWss.handleUpgrade(request, socket, head, (ws) => {
            clientWss.emit('connection', ws, request);
        });
    }
    else {
        socket.destroy();
    }
});
// Initialize database and start server
async function start() {
    try {
        // Initialize SQLite database
        await initDatabase();
        // Cleanup expired tokens every 5 minutes
        setInterval(() => {
            try {
                const deleted = cleanupExpiredTokens();
                if (deleted > 0) {
                    console.log(`[Cleanup] Deleted ${deleted} expired claim tokens`);
                }
            }
            catch (error) {
                console.error('[Cleanup] Error:', error);
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
    }
    catch (error) {
        console.error('Failed to start server:', error);
        process.exit(1);
    }
}
start();
// Graceful shutdown
process.on('SIGTERM', () => {
    console.log('[Cloud] Shutting down...');
    deviceWss.close();
    clientWss.close();
    server.close();
    process.exit(0);
});
//# sourceMappingURL=server.js.map