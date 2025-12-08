import { WebSocketServer, WebSocket, RawData } from 'ws';
import { IncomingMessage } from 'http';
import type { DeviceMessage } from './types.js';
import { verifyDeviceKey, updateDeviceStatus } from './services/device.js';

interface DeviceConnection {
  ws: WebSocket;
  deviceId: string;
  connectedAt: Date;
  lastSeen: Date;
}

/**
 * Device Relay
 * 
 * Handles WebSocket connections from ESP32 devices.
 * Each device maintains a persistent connection to the cloud.
 */
export class DeviceRelay {
  private devices = new Map<string, DeviceConnection>();
  private messageHandlers = new Set<(deviceId: string, message: DeviceMessage) => void>();

  constructor(wss: WebSocketServer) {
    wss.on('connection', (ws, req) => this.handleConnection(ws, req));
  }

  private handleConnection(ws: WebSocket, req: IncomingMessage): void {
    const url = new URL(req.url || '', `http://${req.headers.host}`);
    const deviceId = url.searchParams.get('id');
    const deviceKey = url.searchParams.get('key');

    // Validate required parameters
    if (!deviceId || !deviceKey) {
      console.warn(`[Device] Connection rejected: missing ID or key`);
      ws.close(4001, 'Missing device ID or key');
      return;
    }

    // Validate device ID format (BRW-XXXXXXXX)
    if (!/^BRW-[A-F0-9]{8}$/i.test(deviceId)) {
      console.warn(`[Device] Connection rejected: invalid ID format: ${deviceId}`);
      ws.close(4001, 'Invalid device ID format');
      return;
    }

    // Validate device key (base64url, 32 bytes = ~43 chars)
    if (deviceKey.length < 32 || deviceKey.length > 64) {
      console.warn(`[Device] Connection rejected: invalid key length for ${deviceId}`);
      ws.close(4003, 'Invalid device key format');
      return;
    }

    // Verify device key against database
    if (!verifyDeviceKey(deviceId, deviceKey)) {
      console.warn(`[Device] Connection rejected: invalid key for ${deviceId}`);
      ws.close(4003, 'Invalid device credentials');
      return;
    }

    // Close existing connection for this device (if any)
    const existing = this.devices.get(deviceId);
    if (existing) {
      console.log(`[Device] Replacing connection for ${deviceId}`);
      existing.ws.close(4002, 'Replaced by new connection');
    }

    const connection: DeviceConnection = {
      ws,
      deviceId,
      connectedAt: new Date(),
      lastSeen: new Date(),
    };

    this.devices.set(deviceId, connection);
    console.log(`[Device] Connected: ${deviceId} (authenticated)`);

    // Update device online status in database
    try {
      updateDeviceStatus(deviceId, true);
    } catch (err) {
      console.error(`[Device] Failed to update status for ${deviceId}:`, err);
    }

    // Handle messages from device
    ws.on('message', (data: RawData) => {
      connection.lastSeen = new Date();
      try {
        const message: DeviceMessage = JSON.parse(data.toString());
        this.handleDeviceMessage(deviceId, message);
      } catch (err) {
        console.error(`[Device] Invalid message from ${deviceId}:`, err);
      }
    });

    // Handle disconnect
    ws.on('close', () => {
      this.devices.delete(deviceId);
      console.log(`[Device] Disconnected: ${deviceId}`);
      
      // Update device offline status in database
      try {
        updateDeviceStatus(deviceId, false);
      } catch (err) {
        console.error(`[Device] Failed to update status for ${deviceId}:`, err);
      }
      
      // Notify handlers of disconnect
      this.notifyHandlers(deviceId, { type: 'device_offline' });
    });

    ws.on('error', (err) => {
      console.error(`[Device] Error from ${deviceId}:`, err);
    });

    // Send welcome
    this.sendToDevice(deviceId, { type: 'connected', timestamp: Date.now() });
    
    // Notify handlers of connection
    this.notifyHandlers(deviceId, { type: 'device_online' });
  }

  private handleDeviceMessage(deviceId: string, message: DeviceMessage): void {
    // Add device ID to message
    message.deviceId = deviceId;
    message.timestamp = message.timestamp || Date.now();

    // Forward to all handlers (client proxy will receive these)
    this.notifyHandlers(deviceId, message);
  }

  private notifyHandlers(deviceId: string, message: DeviceMessage): void {
    this.messageHandlers.forEach(handler => handler(deviceId, message));
  }

  /**
   * Subscribe to messages from devices
   */
  onDeviceMessage(handler: (deviceId: string, message: DeviceMessage) => void): () => void {
    this.messageHandlers.add(handler);
    return () => this.messageHandlers.delete(handler);
  }

  /**
   * Send message to a specific device
   */
  sendToDevice(deviceId: string, message: DeviceMessage): boolean {
    const connection = this.devices.get(deviceId);
    if (!connection || connection.ws.readyState !== WebSocket.OPEN) {
      return false;
    }

    connection.ws.send(JSON.stringify(message));
    return true;
  }

  /**
   * Check if device is connected
   */
  isDeviceConnected(deviceId: string): boolean {
    const connection = this.devices.get(deviceId);
    return connection?.ws.readyState === WebSocket.OPEN;
  }

  /**
   * Get connected device count
   */
  getConnectedDeviceCount(): number {
    return this.devices.size;
  }

  /**
   * Get list of connected devices
   */
  getConnectedDevices(): Array<{ id: string; connectedAt: Date; lastSeen: Date }> {
    return Array.from(this.devices.entries()).map(([id, conn]) => ({
      id,
      connectedAt: conn.connectedAt,
      lastSeen: conn.lastSeen,
    }));
  }
}

