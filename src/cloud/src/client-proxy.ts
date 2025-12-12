import { WebSocketServer, WebSocket, RawData } from "ws";
import { IncomingMessage } from "http";
import { v4 as uuidv4 } from "uuid";
import { verifyWebSocketToken } from "./middleware/auth.js";
import { userOwnsDevice } from "./services/device.js";
import type { DeviceRelay } from "./device-relay.js";
import type { DeviceMessage } from "./types.js";

interface ClientConnection {
  ws: WebSocket;
  sessionId: string;
  userId: string;
  deviceId: string;
  connectedAt: Date;
}

/**
 * Client Proxy
 *
 * Handles WebSocket connections from client apps (web, mobile).
 * Routes messages between clients and their associated devices.
 *
 * Authentication uses our own session tokens (not OAuth provider tokens).
 */
export class ClientProxy {
  private clients = new Map<string, ClientConnection>();
  private deviceClients = new Map<string, Set<string>>(); // deviceId -> sessionIds
  private deviceRelay: DeviceRelay;

  constructor(wss: WebSocketServer, deviceRelay: DeviceRelay) {
    this.deviceRelay = deviceRelay;

    wss.on("connection", (ws, req) => this.handleConnection(ws, req));

    // Subscribe to device messages
    deviceRelay.onDeviceMessage((deviceId, message) => {
      this.broadcastToDeviceClients(deviceId, message);
    });
  }

  private async handleConnection(
    ws: WebSocket,
    req: IncomingMessage
  ): Promise<void> {
    const url = new URL(req.url || "", `http://${req.headers.host}`);
    const token = url.searchParams.get("token");
    const deviceId = url.searchParams.get("device");

    if (!token || !deviceId) {
      ws.close(4001, "Missing token or device ID");
      return;
    }

    // Verify our session token (not OAuth provider token)
    const user = verifyWebSocketToken(token);

    if (!user) {
      ws.close(4002, "Invalid or expired token");
      return;
    }

    // Verify user has access to this device
    if (!userOwnsDevice(user.id, deviceId)) {
      ws.close(4003, "Access denied to device");
      return;
    }

    const sessionId = uuidv4();
    const connection: ClientConnection = {
      ws,
      sessionId,
      userId: user.id,
      deviceId,
      connectedAt: new Date(),
    };

    this.registerClient(connection);
    console.log(
      `[Client] Connected: ${sessionId} -> device ${deviceId} (user: ${user.email})`
    );

    // Handle messages from client
    ws.on("message", (data: RawData) => {
      try {
        const message: DeviceMessage = JSON.parse(data.toString());
        this.handleClientMessage(connection, message);
      } catch (err) {
        console.error(`[Client] Invalid message from ${sessionId}:`, err);
      }
    });

    // Handle disconnect
    ws.on("close", () => {
      this.unregisterClient(connection);
      console.log(`[Client] Disconnected: ${sessionId}`);
    });

    ws.on("error", (err) => {
      console.error(`[Client] Error from ${sessionId}:`, err);
      this.unregisterClient(connection);
    });

    // Send connection status
    const deviceOnline = this.deviceRelay.isDeviceConnected(deviceId);
    this.sendToClient(connection, {
      type: "connected",
      sessionId,
      deviceId,
      deviceOnline,
      timestamp: Date.now(),
    });

    // Request device to send full state if online
    // This ensures cloud clients get all current state on connect
    if (deviceOnline) {
      this.deviceRelay.sendToDevice(deviceId, {
        type: "request_state",
        timestamp: Date.now(),
      });
    }
  }

  private registerClient(client: ClientConnection): void {
    this.clients.set(client.sessionId, client);

    // Track clients per device
    if (!this.deviceClients.has(client.deviceId)) {
      this.deviceClients.set(client.deviceId, new Set());
    }
    this.deviceClients.get(client.deviceId)!.add(client.sessionId);
  }

  private unregisterClient(client: ClientConnection): void {
    this.clients.delete(client.sessionId);

    const deviceSessions = this.deviceClients.get(client.deviceId);
    if (deviceSessions) {
      deviceSessions.delete(client.sessionId);
      if (deviceSessions.size === 0) {
        this.deviceClients.delete(client.deviceId);
      }
    }
  }

  private handleClientMessage(
    client: ClientConnection,
    message: DeviceMessage
  ): void {
    // Forward message to device
    message.timestamp = Date.now();

    const sent = this.deviceRelay.sendToDevice(client.deviceId, message);

    if (!sent) {
      // Device is offline, notify client
      this.sendToClient(client, {
        type: "error",
        error: "device_offline",
        message: "Device is not connected",
      });
    }
  }

  private sendToClient(client: ClientConnection, message: DeviceMessage): void {
    if (client.ws.readyState === WebSocket.OPEN) {
      client.ws.send(JSON.stringify(message));
    }
  }

  private broadcastToDeviceClients(
    deviceId: string,
    message: DeviceMessage
  ): void {
    const sessions = this.deviceClients.get(deviceId);
    if (!sessions) return;

    const payload = JSON.stringify(message);
    for (const sessionId of sessions) {
      const client = this.clients.get(sessionId);
      if (client?.ws.readyState === WebSocket.OPEN) {
        client.ws.send(payload);
      }
    }
  }

  /**
   * Get connected client count
   */
  getConnectedClientCount(): number {
    return this.clients.size;
  }
}
