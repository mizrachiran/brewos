import type { ConnectionConfig, ConnectionState, WebSocketMessage, IConnection } from './types';

type MessageHandler = (message: WebSocketMessage) => void;
type StateHandler = (state: ConnectionState) => void;

/**
 * Stale connection threshold in milliseconds.
 * If no message is received within this time, the connection is considered stale.
 * Status updates are sent every 500ms, so 3 seconds gives ~6 missed updates before marking stale.
 */
const STALE_THRESHOLD_MS = 3000;

/**
 * How often to check for stale connections.
 */
const STALE_CHECK_INTERVAL_MS = 1000;

/**
 * WebSocket connection manager
 * Handles both local (ESP32 direct) and cloud connections
 * 
 * Connection health is detected by monitoring incoming messages rather than ping/pong.
 * Since status updates are sent every 500ms, missing several updates indicates a problem.
 */
export class Connection implements IConnection {
  private ws: WebSocket | null = null;
  private config: ConnectionConfig;
  private state: ConnectionState = 'disconnected';
  private messageHandlers = new Set<MessageHandler>();
  private stateHandlers = new Set<StateHandler>();
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
  private reconnectDelay = 1000;
  private maxReconnectDelay = 5000;
  private lastMessageTime = 0;
  private staleCheckInterval: ReturnType<typeof setInterval> | null = null;

  constructor(config: ConnectionConfig) {
    this.config = config;
  }

  // Public API
  async connect(): Promise<void> {
    if (this.state === 'connecting' || this.state === 'connected') {
      return;
    }

    this.setState('connecting');

    return new Promise((resolve, reject) => {
      try {
        const url = this.buildUrl();
        console.log(`[BrewOS] Connecting to ${url}`);
        
        this.ws = new WebSocket(url);

        this.ws.onopen = () => {
          console.log('[BrewOS] Connected');
          this.setState('connected');
          this.reconnectDelay = 1000;
          this.lastMessageTime = Date.now();
          this.startStaleCheck();
          resolve();
        };

        this.ws.onclose = (event) => {
          console.log(`[BrewOS] Disconnected (code: ${event.code})`);
          this.stopStaleCheck();
          this.setState('disconnected');
          if (event.code !== 1000) {
            this.scheduleReconnect();
          }
        };

        this.ws.onerror = (error) => {
          console.error('[BrewOS] WebSocket error:', error);
          this.setState('error');
          reject(error);
        };

        this.ws.onmessage = (event) => {
          this.lastMessageTime = Date.now();
          try {
            const message = JSON.parse(event.data) as WebSocketMessage;
            this.notifyMessage(message);
          } catch (e) {
            console.warn('[BrewOS] Invalid message:', event.data);
          }
        };
      } catch (error) {
        this.setState('error');
        reject(error);
      }
    });
  }

  disconnect(): void {
    this.stopStaleCheck();
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
    if (this.ws) {
      this.ws.close(1000, 'User disconnect');
      this.ws = null;
    }
    this.setState('disconnected');
  }

  send(type: string, payload: Record<string, unknown> = {}): void {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      const message = this.config.mode === 'cloud'
        ? { type, deviceId: this.config.deviceId, ...payload }
        : { type, ...payload };
      this.ws.send(JSON.stringify(message));
    }
  }

  sendCommand(cmd: string, data: Record<string, unknown> = {}): void {
    this.send('command', { cmd, ...data });
  }

  // Event handlers
  onMessage(handler: MessageHandler): () => void {
    this.messageHandlers.add(handler);
    return () => this.messageHandlers.delete(handler);
  }

  onStateChange(handler: StateHandler): () => void {
    this.stateHandlers.add(handler);
    handler(this.state); // Immediately call with current state
    return () => this.stateHandlers.delete(handler);
  }

  getState(): ConnectionState {
    return this.state;
  }

  // Private methods
  private buildUrl(): string {
    if (this.config.mode === 'cloud') {
      // Cloud: connect to /ws/client with auth
      const base = this.config.cloudUrl || '';
      const url = new URL(base || window.location.href);
      url.protocol = url.protocol === 'https:' ? 'wss:' : 'ws:';
      url.pathname = '/ws/client';
      url.searchParams.set('token', this.config.authToken || '');
      url.searchParams.set('device', this.config.deviceId || '');
      return url.toString();
    }

    // Local connection - direct to ESP32
    const endpoint = this.config.endpoint || '/ws';
    
    if (typeof window !== 'undefined') {
      const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
      const host = window.location.host;
      return `${protocol}//${host}${endpoint}`;
    }
    
    return `ws://brewos.local${endpoint}`;
  }

  private setState(state: ConnectionState): void {
    if (this.state !== state) {
      this.state = state;
      this.stateHandlers.forEach(handler => handler(state));
    }
  }

  private notifyMessage(message: WebSocketMessage): void {
    this.messageHandlers.forEach(handler => handler(message));
  }

  private scheduleReconnect(): void {
    if (this.reconnectTimer) return;

    this.setState('reconnecting');
    console.log(`[BrewOS] Reconnecting in ${this.reconnectDelay}ms...`);

    this.reconnectTimer = setTimeout(() => {
      this.reconnectTimer = null;
      this.connect().catch(() => {
        this.reconnectDelay = Math.min(
          this.reconnectDelay * 2,
          this.maxReconnectDelay
        );
      });
    }, this.reconnectDelay);
  }

  /**
   * Start checking for stale connections.
   * If no message is received within STALE_THRESHOLD_MS, close and reconnect.
   */
  private startStaleCheck(): void {
    this.staleCheckInterval = setInterval(() => {
      const elapsed = Date.now() - this.lastMessageTime;
      if (elapsed > STALE_THRESHOLD_MS && this.state === 'connected') {
        console.warn(`[BrewOS] Connection stale (no messages for ${elapsed}ms), reconnecting...`);
        // Close the stale connection - onclose will trigger reconnect
        this.ws?.close(4000, 'Stale connection');
      }
    }, STALE_CHECK_INTERVAL_MS);
  }

  private stopStaleCheck(): void {
    if (this.staleCheckInterval) {
      clearInterval(this.staleCheckInterval);
      this.staleCheckInterval = null;
    }
  }
}

// Singleton instance
let connection: Connection | null = null;

// Active connection reference (can be real or demo)
let activeConnection: IConnection | null = null;

export function initConnection(config: ConnectionConfig): Connection {
  if (connection) {
    connection.disconnect();
  }
  connection = new Connection(config);
  activeConnection = connection;
  return connection;
}

export function getConnection(): Connection | null {
  return connection;
}

/**
 * Set the active connection (used by demo mode)
 */
export function setActiveConnection(conn: IConnection | null): void {
  activeConnection = conn;
}

/**
 * Get the currently active connection (real or demo)
 */
export function getActiveConnection(): IConnection | null {
  return activeConnection;
}

