import { useEffect, useState } from "react";
import {
  Activity,
  RefreshCw,
  Wifi,
  Users,
  Clock,
  Cpu,
  HardDrive,
  Zap,
} from "lucide-react";
import { StatsCard } from "../components/StatsCard";
import { api } from "../lib/api";
import type { ConnectionStats } from "../lib/types";

interface HealthData {
  status: string;
  timestamp: string;
  uptime: {
    seconds: number;
    formatted: string;
  };
  connections: {
    devices: number;
    clients: number;
    totalClientConnections: number;
    totalMessagesRelayed: number;
  };
  messageQueue: {
    pendingMessages: number;
  };
  memory: {
    heapUsed: number;
    heapTotal: number;
    external: number;
    rss: number;
  };
  sessionCache: {
    size: number;
    maxSize: number;
    hitRate: number;
  };
}

export function Monitoring() {
  const [health, setHealth] = useState<HealthData | null>(null);
  const [connections, setConnections] = useState<ConnectionStats | null>(null);
  const [loading, setLoading] = useState(true);
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [lastUpdate, setLastUpdate] = useState<Date | null>(null);

  useEffect(() => {
    loadData();
  }, []);

  useEffect(() => {
    if (!autoRefresh) return;
    const interval = setInterval(loadData, 5000);
    return () => clearInterval(interval);
  }, [autoRefresh]);

  async function loadData() {
    try {
      const [healthData, connData] = await Promise.all([
        api.getHealth() as unknown as Promise<HealthData>,
        api.getConnections().catch(() => null),
      ]);
      setHealth(healthData);
      setConnections(connData);
      setLastUpdate(new Date());
    } catch (error) {
      console.error("Failed to load monitoring data:", error);
    } finally {
      setLoading(false);
    }
  }

  if (loading) {
    return (
      <div className="flex items-center justify-center h-64">
        <div className="text-admin-text-secondary">Loading monitoring data...</div>
      </div>
    );
  }

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex flex-col gap-4">
        <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
          <div>
            <h1 className="text-xl sm:text-2xl font-display font-bold text-admin-text flex items-center gap-3">
              <Activity className="w-6 h-6 sm:w-7 sm:h-7 text-admin-accent" />
              Monitoring
            </h1>
            <p className="text-admin-text-secondary mt-1 text-sm sm:text-base">
              Real-time system status and performance
            </p>
          </div>
          <button
            onClick={loadData}
            className="btn btn-secondary btn-sm flex items-center gap-2 w-fit"
          >
            <RefreshCw className="w-4 h-4" />
            Refresh
          </button>
        </div>
        <label className="flex items-center gap-2 text-sm text-admin-text-secondary w-fit">
          <input
            type="checkbox"
            checked={autoRefresh}
            onChange={(e) => setAutoRefresh(e.target.checked)}
            className="w-4 h-4 rounded border-admin-border bg-admin-surface text-admin-accent focus:ring-admin-accent"
          />
          Auto-refresh (5s)
        </label>
      </div>

      {/* Last Update */}
      {lastUpdate && (
        <p className="text-admin-text-secondary text-sm">
          Last updated: {lastUpdate.toLocaleTimeString()}
        </p>
      )}

      {/* Connection Stats */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6">
        <StatsCard
          title="Connected Devices"
          value={health?.connections.devices ?? 0}
          icon={Wifi}
          className="animate-slide-in stagger-1"
        />
        <StatsCard
          title="Connected Clients"
          value={health?.connections.clients ?? 0}
          icon={Users}
          className="animate-slide-in stagger-2"
        />
        <StatsCard
          title="Total Connections"
          value={health?.connections.totalClientConnections ?? 0}
          subtitle="Lifetime"
          icon={Zap}
          className="animate-slide-in stagger-3"
        />
        <StatsCard
          title="Messages Relayed"
          value={health?.connections.totalMessagesRelayed ?? 0}
          subtitle="Lifetime"
          icon={Activity}
          className="animate-slide-in stagger-4"
        />
      </div>

      {/* System Stats */}
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        {/* Memory */}
        <div className="admin-card animate-slide-in stagger-3">
          <h3 className="text-lg font-display font-semibold text-admin-text mb-4 flex items-center gap-2">
            <HardDrive className="w-5 h-5 text-admin-accent" />
            Memory Usage
          </h3>
          {health?.memory && (
            <div className="space-y-4">
              <MemoryBar
                label="Heap Used"
                used={health.memory.heapUsed}
                total={health.memory.heapTotal}
              />
              <div className="grid grid-cols-2 gap-4 mt-4">
                <div>
                  <p className="text-admin-text-secondary text-sm">External</p>
                  <p className="text-admin-text font-mono">
                    {health.memory.external} MB
                  </p>
                </div>
                <div>
                  <p className="text-admin-text-secondary text-sm">RSS</p>
                  <p className="text-admin-text font-mono">
                    {health.memory.rss} MB
                  </p>
                </div>
              </div>
            </div>
          )}
        </div>

        {/* Uptime & Cache */}
        <div className="admin-card animate-slide-in stagger-4">
          <h3 className="text-lg font-display font-semibold text-admin-text mb-4 flex items-center gap-2">
            <Clock className="w-5 h-5 text-admin-accent" />
            System Info
          </h3>
          <div className="space-y-4">
            <div>
              <p className="text-admin-text-secondary text-sm">Uptime</p>
              <p className="text-2xl font-bold text-admin-text font-display">
                {health?.uptime.formatted ?? "—"}
              </p>
            </div>
            <div>
              <p className="text-admin-text-secondary text-sm">Pending Messages</p>
              <p className="text-admin-text font-mono">
                {health?.messageQueue.pendingMessages ?? 0}
              </p>
            </div>
            {health?.sessionCache && (
              <>
                <div>
                  <p className="text-admin-text-secondary text-sm">
                    Session Cache
                  </p>
                  <p className="text-admin-text font-mono">
                    {health.sessionCache.size} / {health.sessionCache.maxSize}
                  </p>
                </div>
                <div>
                  <p className="text-admin-text-secondary text-sm">Cache Hit Rate</p>
                  <p className="text-admin-text font-mono">
                    {(health.sessionCache.hitRate * 100).toFixed(1)}%
                  </p>
                </div>
              </>
            )}
          </div>
        </div>
      </div>

      {/* Connected Devices List */}
      {connections?.devices.devices && connections.devices.devices.length > 0 && (
        <div className="admin-card">
          <h3 className="text-lg font-display font-semibold text-admin-text mb-4 flex items-center gap-2">
            <Cpu className="w-5 h-5 text-admin-accent" />
            Connected Devices ({connections.devices.connectedDevices})
          </h3>
          <div className="overflow-x-auto">
            <table className="admin-table">
              <thead>
                <tr>
                  <th>Device ID</th>
                  <th>Connected At</th>
                  <th>Last Seen</th>
                  <th>Connection Age</th>
                </tr>
              </thead>
              <tbody>
                {connections.devices.devices.map((device) => (
                  <tr key={device.id}>
                    <td className="font-mono text-sm">{device.id}</td>
                    <td className="text-admin-text-secondary text-sm">
                      {new Date(device.connectedAt).toLocaleTimeString()}
                    </td>
                    <td className="text-admin-text-secondary text-sm">
                      {new Date(device.lastSeen).toLocaleTimeString()}
                    </td>
                    <td className="text-admin-text-secondary text-sm">
                      {formatDuration(device.connectionAge)}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>
      )}
    </div>
  );
}

function MemoryBar({
  label,
  used,
  total,
}: {
  label: string;
  used: number;
  total: number;
}) {
  const percentage = (used / total) * 100;
  const color =
    percentage > 80
      ? "bg-admin-danger"
      : percentage > 60
        ? "bg-admin-warning"
        : "bg-admin-accent";

  return (
    <div>
      <div className="flex justify-between text-sm mb-1">
        <span className="text-admin-text-secondary">{label}</span>
        <span className="text-admin-text font-mono">
          {used} / {total} MB ({percentage.toFixed(1)}%)
        </span>
      </div>
      <div className="h-2 bg-admin-surface rounded-full overflow-hidden">
        <div
          className={`h-full ${color} transition-all duration-500`}
          style={{ width: `${percentage}%` }}
        />
      </div>
    </div>
  );
}

function formatDuration(ms: number): string {
  const seconds = Math.floor(ms / 1000);
  if (seconds < 60) return `${seconds}s`;
  const minutes = Math.floor(seconds / 60);
  if (minutes < 60) return `${minutes}m ${seconds % 60}s`;
  const hours = Math.floor(minutes / 60);
  return `${hours}h ${minutes % 60}m`;
}

