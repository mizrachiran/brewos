import { WebSocketServer, WebSocket, RawData } from "ws";
import { IncomingMessage } from "http";
import { v4 as uuidv4 } from "uuid";
import { userOwnsDevice } from "./services/device.js";
import { verifyAccessToken } from "./services/session.js";
import type { DeviceRelay } from "./device-relay.js";
import type { DeviceMessage } from "./types.js";

// =============================================================================
// Configuration
// =============================================================================

// Ping interval for keep-alive (30 seconds for clients - less aggressive than devices)
const PING_INTERVAL_MS = 30000;
// Disconnect after this many missed pongs
const MAX_MISSED_PONGS = 2;
// Warn client this many ms before token expires
const TOKEN_EXPIRY_WARNING_MS = 5 * 60 * 1000; // 5 minutes
// Message queue TTL - how long to hold messages for offline devices
const MESSAGE_QUEUE_TTL_MS = 10000; // 10 seconds
// Max messages per device in queue
const MAX_QUEUED_MESSAGES = 50;
// Max retries for queued messages
const MAX_MESSAGE_RETRIES = 3;

// =============================================================================
// Types
// =============================================================================

interface ConnectionMetrics {
  messagesSent: number;
  messagesReceived: number;
  lastPingRTT: number | null;
  avgPingRTT: number;
  pingCount: number;
  reconnectCount: number;
}

interface ClientConnection {
  ws: WebSocket;
  sessionId: string;
  userId: string;
  deviceId: string;
  connectedAt: Date;
  lastActivity: Date;
  missedPongs: number;
  accessTokenExpiresAt: string;
  tokenExpiryTimer: NodeJS.Timeout | null;
  metrics: ConnectionMetrics;
  pingStartTime: number | null;
}

interface PendingMessage {
  message: DeviceMessage;
  timestamp: number;
  retries: number;
  clientSessionId: string;
}

// =============================================================================
// Client Proxy
// =============================================================================

/**
 * Client Proxy
 *
 * Handles WebSocket connections from client apps (web, mobile).
 * Routes messages between clients and their associated devices.
 *
 * Features:
 * - Ping/pong keep-alive for connection health monitoring
 * - Token expiry warnings before disconnection
 * - Message queuing for brief device disconnects
 * - Connection quality metrics
 * - Graceful token refresh support
 */
export class ClientProxy {
  private clients = new Map<string, ClientConnection>();
  private deviceClients = new Map<string, Set<string>>(); // deviceId -> sessionIds
  private deviceRelay: DeviceRelay;
  private pingInterval: NodeJS.Timeout | null = null;
  private pendingMessages = new Map<string, PendingMessage[]>(); // deviceId -> messages
  private messageQueueCleanupInterval: NodeJS.Timeout | null = null;

  // Global metrics
  private totalConnections = 0;
  private totalMessages = 0;
  private startTime = Date.now();

  constructor(wss: WebSocketServer, deviceRelay: DeviceRelay) {
    this.deviceRelay = deviceRelay;

    wss.on("connection", (ws, req) => this.handleConnection(ws, req));

    // Subscribe to device messages
    deviceRelay.onDeviceMessage((deviceId, message) => {
      this.broadcastToDeviceClients(deviceId, message);
    });

    // Subscribe to device online events to flush queued messages
    deviceRelay.onDeviceMessage((deviceId, message) => {
      if (message.type === "device_online") {
        this.flushPendingMessages(deviceId);
      }
    });

    // Start ping interval to keep connections alive
    this.pingInterval = setInterval(() => {
      this.pingAllClients();
    }, PING_INTERVAL_MS);

    // Start message queue cleanup interval
    this.messageQueueCleanupInterval = setInterval(() => {
      this.cleanupExpiredQueuedMessages();
    }, MESSAGE_QUEUE_TTL_MS);

    console.log(
      `[ClientProxy] Started with ping interval ${PING_INTERVAL_MS}ms`
    );
  }

  // =============================================================================
  // Connection Handling
  // =============================================================================

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

    // Verify our session token and get full session info
    const sessionResult = verifyAccessToken(token);

    if (!sessionResult) {
      ws.close(4002, "Invalid or expired token");
      return;
    }

    const { user, session } = sessionResult;

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
      lastActivity: new Date(),
      missedPongs: 0,
      accessTokenExpiresAt: session.access_expires_at,
      tokenExpiryTimer: null,
      metrics: {
        messagesSent: 0,
        messagesReceived: 0,
        lastPingRTT: null,
        avgPingRTT: 0,
        pingCount: 0,
        reconnectCount: 0,
      },
      pingStartTime: null,
    };

    this.registerClient(connection);
    this.totalConnections++;

    console.log(
      `[Client] Connected: ${sessionId} -> device ${deviceId} (user: ${user.email})`
    );

    // Set up token expiry warning
    this.setupTokenExpiryWarning(connection);

    // Handle pong responses for keep-alive and RTT measurement
    ws.on("pong", () => {
      connection.missedPongs = 0;
      connection.lastActivity = new Date();

      // Calculate RTT if we have a ping start time
      if (connection.pingStartTime !== null) {
        const rtt = Date.now() - connection.pingStartTime;
        connection.metrics.lastPingRTT = rtt;
        connection.metrics.pingCount++;
        // Calculate running average
        connection.metrics.avgPingRTT =
          (connection.metrics.avgPingRTT * (connection.metrics.pingCount - 1) +
            rtt) /
          connection.metrics.pingCount;
        connection.pingStartTime = null;
      }
    });

    // Handle messages from client
    ws.on("message", (data: RawData) => {
      connection.lastActivity = new Date();
      connection.metrics.messagesReceived++;
      this.totalMessages++;

      try {
        const message: DeviceMessage = JSON.parse(data.toString());
        this.handleClientMessage(connection, message);
      } catch (err) {
        console.error(`[Client] Invalid message from ${sessionId}:`, err);
      }
    });

    // Handle disconnect
    ws.on("close", () => {
      this.cleanupClient(connection);
      console.log(`[Client] Disconnected: ${sessionId}`);
    });

    ws.on("error", (err) => {
      console.error(`[Client] Error from ${sessionId}:`, err);
      this.cleanupClient(connection);
    });

    // Send connection status with enhanced info
    const deviceOnline = this.deviceRelay.isDeviceConnected(deviceId);
    const deviceLastSeen = this.deviceRelay.getDeviceLastSeen(deviceId);

    this.sendToClient(connection, {
      type: "connected",
      sessionId,
      deviceId,
      deviceOnline,
      deviceLastSeen: deviceLastSeen?.toISOString() || null,
      tokenExpiresAt: session.access_expires_at,
      serverTime: new Date().toISOString(),
      timestamp: Date.now(),
    });

    // Request device to send full state if online
    if (deviceOnline) {
      this.deviceRelay.sendToDevice(deviceId, {
        type: "request_state",
        timestamp: Date.now(),
      });
    }
  }

  // =============================================================================
  // Keep-Alive (Ping/Pong)
  // =============================================================================

  /**
   * Ping all connected clients to keep connections alive and detect stale ones
   */
  private pingAllClients(): void {
    this.clients.forEach((connection, sessionId) => {
      // Increment missed pongs counter (reset on pong)
      connection.missedPongs++;

      if (connection.missedPongs > MAX_MISSED_PONGS) {
        console.log(
          `[Client] ${sessionId} ping timeout (${connection.missedPongs} missed) - disconnecting`
        );
        connection.ws.terminate();
        return;
      }

      // Record ping start time for RTT measurement
      connection.pingStartTime = Date.now();

      // Send ping
      if (connection.ws.readyState === WebSocket.OPEN) {
        connection.ws.ping();
      }
    });
  }

  // =============================================================================
  // Token Expiry Warning
  // =============================================================================

  private setupTokenExpiryWarning(connection: ClientConnection): void {
    const expiresAt = new Date(connection.accessTokenExpiresAt).getTime();
    const now = Date.now();
    const warningTime = expiresAt - TOKEN_EXPIRY_WARNING_MS;

    if (warningTime > now) {
      connection.tokenExpiryTimer = setTimeout(() => {
        if (connection.ws.readyState === WebSocket.OPEN) {
          this.sendToClient(connection, {
            type: "token_expiring",
            expiresAt: connection.accessTokenExpiresAt,
            expiresIn: Math.floor((expiresAt - Date.now()) / 1000),
            refreshRequired: true,
          });
          console.log(
            `[Client] Sent token expiry warning to ${connection.sessionId}`
          );
        }
      }, warningTime - now);
    }
  }

  // =============================================================================
  // Message Handling
  // =============================================================================

  private handleClientMessage(
    client: ClientConnection,
    message: DeviceMessage
  ): void {
    // Handle special message types
    if (message.type === "refresh_auth") {
      this.handleAuthRefresh(client, message);
      return;
    }

    if (message.type === "ping") {
      // Application-level ping (in addition to WebSocket ping)
      this.sendToClient(client, {
        type: "pong",
        timestamp: Date.now(),
        clientTimestamp: message.timestamp,
      });
      return;
    }

    if (message.type === "get_metrics") {
      this.sendToClient(client, {
        type: "metrics",
        connection: client.metrics,
        deviceOnline: this.deviceRelay.isDeviceConnected(client.deviceId),
        queuedMessages: this.pendingMessages.get(client.deviceId)?.length || 0,
      });
      return;
    }

    // Forward message to device
    message.timestamp = Date.now();
    client.metrics.messagesSent++;

    const sent = this.deviceRelay.sendToDevice(client.deviceId, message);

    if (!sent) {
      // Device is offline - queue message and notify client
      this.queueMessageForDevice(client.deviceId, message, client.sessionId);

      const deviceLastSeen = this.deviceRelay.getDeviceLastSeen(
        client.deviceId
      );
      const queuedCount =
        this.pendingMessages.get(client.deviceId)?.length || 0;

      this.sendToClient(client, {
        type: "device_status",
        deviceId: client.deviceId,
        online: false,
        lastSeen: deviceLastSeen?.toISOString() || null,
        messageQueued: true,
        queuedMessages: queuedCount,
        queueTTL: MESSAGE_QUEUE_TTL_MS / 1000,
        message: `Device offline. Message queued (${queuedCount} pending, ${MESSAGE_QUEUE_TTL_MS / 1000}s TTL).`,
      });
    }
  }

  /**
   * Handle auth token refresh request
   * Allows clients to refresh their token without disconnecting
   */
  private handleAuthRefresh(
    client: ClientConnection,
    message: DeviceMessage
  ): void {
    const newToken = message.token as string;

    if (!newToken) {
      this.sendToClient(client, {
        type: "auth_refreshed",
        success: false,
        error: "Missing token",
      });
      return;
    }

    // Verify the new token
    const sessionResult = verifyAccessToken(newToken);

    if (!sessionResult) {
      this.sendToClient(client, {
        type: "auth_refreshed",
        success: false,
        error: "Invalid or expired token",
      });
      return;
    }

    // Verify it's the same user
    if (sessionResult.user.id !== client.userId) {
      this.sendToClient(client, {
        type: "auth_refreshed",
        success: false,
        error: "Token user mismatch",
      });
      return;
    }

    // Update the connection with new token expiry
    client.accessTokenExpiresAt = sessionResult.session.access_expires_at;

    // Clear old timer and set up new expiry warning
    if (client.tokenExpiryTimer) {
      clearTimeout(client.tokenExpiryTimer);
    }
    this.setupTokenExpiryWarning(client);

    this.sendToClient(client, {
      type: "auth_refreshed",
      success: true,
      tokenExpiresAt: client.accessTokenExpiresAt,
    });

    console.log(`[Client] Token refreshed for ${client.sessionId}`);
  }

  // =============================================================================
  // Message Queue for Offline Devices
  // =============================================================================

  private queueMessageForDevice(
    deviceId: string,
    message: DeviceMessage,
    clientSessionId: string
  ): void {
    if (!this.pendingMessages.has(deviceId)) {
      this.pendingMessages.set(deviceId, []);
    }

    const queue = this.pendingMessages.get(deviceId)!;

    // Limit queue size per device
    if (queue.length >= MAX_QUEUED_MESSAGES) {
      // Remove oldest message
      queue.shift();
    }

    queue.push({
      message,
      timestamp: Date.now(),
      retries: 0,
      clientSessionId,
    });
  }

  /**
   * Flush queued messages when device comes back online
   */
  private flushPendingMessages(deviceId: string): void {
    const queue = this.pendingMessages.get(deviceId);
    if (!queue || queue.length === 0) return;

    const now = Date.now();
    let sentCount = 0;
    let expiredCount = 0;

    const validMessages = queue.filter((pending) => {
      // Skip expired messages
      if (now - pending.timestamp > MESSAGE_QUEUE_TTL_MS) {
        expiredCount++;
        return false;
      }
      return true;
    });

    for (const pending of validMessages) {
      const sent = this.deviceRelay.sendToDevice(deviceId, pending.message);
      if (sent) {
        sentCount++;
        // Notify the client that sent this message
        const client = this.clients.get(pending.clientSessionId);
        if (client) {
          this.sendToClient(client, {
            type: "queued_message_sent",
            originalTimestamp: pending.timestamp,
            messageType: pending.message.type,
          });
        }
      } else if (pending.retries < MAX_MESSAGE_RETRIES) {
        pending.retries++;
      }
    }

    // Clear the queue
    this.pendingMessages.delete(deviceId);

    if (sentCount > 0 || expiredCount > 0) {
      console.log(
        `[Client] Flushed ${sentCount} queued messages for device ${deviceId} (${expiredCount} expired)`
      );
    }
  }

  /**
   * Clean up expired messages from all queues
   */
  private cleanupExpiredQueuedMessages(): void {
    const now = Date.now();
    let totalExpired = 0;

    for (const [deviceId, queue] of this.pendingMessages) {
      const validMessages = queue.filter((pending) => {
        if (now - pending.timestamp > MESSAGE_QUEUE_TTL_MS) {
          totalExpired++;
          return false;
        }
        return true;
      });

      if (validMessages.length === 0) {
        this.pendingMessages.delete(deviceId);
      } else {
        this.pendingMessages.set(deviceId, validMessages);
      }
    }

    if (totalExpired > 0) {
      console.log(
        `[Client] Cleaned up ${totalExpired} expired queued messages`
      );
    }
  }

  // =============================================================================
  // Client Management
  // =============================================================================

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

  private cleanupClient(client: ClientConnection): void {
    // Clear token expiry timer
    if (client.tokenExpiryTimer) {
      clearTimeout(client.tokenExpiryTimer);
    }
    this.unregisterClient(client);
  }

  // =============================================================================
  // Message Sending
  // =============================================================================

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

  // =============================================================================
  // Public API
  // =============================================================================

  /**
   * Get connected client count
   */
  getConnectedClientCount(): number {
    return this.clients.size;
  }

  /**
   * Get detailed stats for health endpoint
   */
  getStats(): {
    connectedClients: number;
    totalConnections: number;
    totalMessages: number;
    uptime: number;
    queuedMessagesTotal: number;
    clientsByDevice: Record<string, number>;
  } {
    const clientsByDevice: Record<string, number> = {};
    for (const [deviceId, sessions] of this.deviceClients) {
      clientsByDevice[deviceId] = sessions.size;
    }

    let queuedMessagesTotal = 0;
    for (const queue of this.pendingMessages.values()) {
      queuedMessagesTotal += queue.length;
    }

    return {
      connectedClients: this.clients.size,
      totalConnections: this.totalConnections,
      totalMessages: this.totalMessages,
      uptime: Date.now() - this.startTime,
      queuedMessagesTotal,
      clientsByDevice,
    };
  }

  /**
   * Cleanup on shutdown
   */
  shutdown(): void {
    if (this.pingInterval) {
      clearInterval(this.pingInterval);
      this.pingInterval = null;
    }
    if (this.messageQueueCleanupInterval) {
      clearInterval(this.messageQueueCleanupInterval);
      this.messageQueueCleanupInterval = null;
    }

    // Clear all token expiry timers
    for (const client of this.clients.values()) {
      if (client.tokenExpiryTimer) {
        clearTimeout(client.tokenExpiryTimer);
      }
    }

    console.log("[ClientProxy] Shutdown complete");
  }
}
