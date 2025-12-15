import { useEffect, useState } from "react";
import { Link } from "react-router-dom";
import { Users, Cpu, KeyRound, Clock, Shield, Wifi, WifiOff } from "lucide-react";
import { StatsCard } from "../components/StatsCard";
import { Badge } from "../components/DataTable";
import { api } from "../lib/api";
import type { AdminStats, AdminUserListItem, AdminDeviceListItem } from "../lib/types";

export function Dashboard() {
  const [stats, setStats] = useState<AdminStats | null>(null);
  const [recentUsers, setRecentUsers] = useState<AdminUserListItem[]>([]);
  const [onlineDevices, setOnlineDevices] = useState<AdminDeviceListItem[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    loadData();
  }, []);

  async function loadData() {
    try {
      const [statsData, usersData, devicesData] = await Promise.all([
        api.getStats(),
        api.getUsers(1, 5),
        api.getDevices(1, 10, undefined, true),
      ]);
      setStats(statsData);
      setRecentUsers(usersData.items);
      setOnlineDevices(devicesData.items);
    } catch (error) {
      console.error("Failed to load dashboard data:", error);
    } finally {
      setLoading(false);
    }
  }

  function formatUptime(seconds: number): string {
    const days = Math.floor(seconds / 86400);
    const hours = Math.floor((seconds % 86400) / 3600);
    const mins = Math.floor((seconds % 3600) / 60);

    if (days > 0) return `${days}d ${hours}h`;
    if (hours > 0) return `${hours}h ${mins}m`;
    return `${mins}m`;
  }

  if (loading) {
    return (
      <div className="flex items-center justify-center h-64">
        <div className="text-admin-text-secondary">Loading dashboard...</div>
      </div>
    );
  }

  return (
    <div className="space-y-6 sm:space-y-8">
      {/* Header */}
      <div>
        <h1 className="text-xl sm:text-2xl font-display font-bold text-admin-text">
          Dashboard
        </h1>
        <p className="text-admin-text-secondary mt-1 text-sm sm:text-base">
          System overview and real-time statistics
        </p>
      </div>

      {/* Stats Grid */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6">
        <StatsCard
          title="Total Users"
          value={stats?.userCount ?? 0}
          subtitle={`${stats?.adminCount ?? 0} admins`}
          icon={Users}
          className="animate-slide-in stagger-1"
        />
        <StatsCard
          title="Devices"
          value={stats?.deviceCount ?? 0}
          subtitle={`${stats?.onlineDeviceCount ?? 0} online`}
          icon={Cpu}
          className="animate-slide-in stagger-2"
        />
        <StatsCard
          title="Active Sessions"
          value={stats?.sessionCount ?? 0}
          icon={KeyRound}
          className="animate-slide-in stagger-3"
        />
        <StatsCard
          title="Uptime"
          value={stats ? formatUptime(stats.uptime) : "—"}
          icon={Clock}
          className="animate-slide-in stagger-4"
        />
      </div>

      {/* Content Grid */}
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        {/* Recent Users */}
        <div className="admin-card animate-slide-in stagger-3">
          <div className="flex items-center justify-between mb-4">
            <h2 className="text-lg font-display font-semibold text-admin-text">
              Recent Users
            </h2>
            <Link
              to="/users"
              className="text-sm text-admin-accent hover:text-admin-accent-hover"
            >
              View all →
            </Link>
          </div>
          <div className="space-y-3">
            {recentUsers.length === 0 ? (
              <p className="text-admin-text-secondary text-sm">No users yet</p>
            ) : (
              recentUsers.map((user) => (
                <Link
                  key={user.id}
                  to={`/users/${user.id}`}
                  className="flex items-center gap-3 p-3 rounded-lg hover:bg-admin-hover transition-colors"
                >
                  {user.avatarUrl ? (
                    <img
                      src={user.avatarUrl}
                      alt={user.displayName || "User"}
                      className="w-8 h-8 rounded-full"
                    />
                  ) : (
                    <div className="w-8 h-8 rounded-full bg-admin-accent/20 flex items-center justify-center">
                      <span className="text-admin-accent text-sm font-bold">
                        {(user.displayName || user.email || "?")[0].toUpperCase()}
                      </span>
                    </div>
                  )}
                  <div className="flex-1 min-w-0">
                    <p className="text-admin-text font-medium truncate">
                      {user.displayName || user.email}
                    </p>
                    <p className="text-admin-text-secondary text-xs truncate">
                      {user.email}
                    </p>
                  </div>
                  <div className="flex items-center gap-2">
                    {user.isAdmin && (
                      <Badge variant="admin">
                        <Shield className="w-3 h-3 mr-1" />
                        Admin
                      </Badge>
                    )}
                    <span className="text-xs text-admin-text-secondary">
                      {user.deviceCount} devices
                    </span>
                  </div>
                </Link>
              ))
            )}
          </div>
        </div>

        {/* Online Devices */}
        <div className="admin-card animate-slide-in stagger-4">
          <div className="flex items-center justify-between mb-4">
            <h2 className="text-lg font-display font-semibold text-admin-text">
              Online Devices
            </h2>
            <Link
              to="/devices"
              className="text-sm text-admin-accent hover:text-admin-accent-hover"
            >
              View all →
            </Link>
          </div>
          <div className="space-y-3">
            {onlineDevices.length === 0 ? (
              <div className="flex items-center gap-3 p-4 text-admin-text-secondary">
                <WifiOff className="w-5 h-5" />
                <span>No devices currently online</span>
              </div>
            ) : (
              onlineDevices.map((device) => (
                <Link
                  key={device.id}
                  to={`/devices/${device.id}`}
                  className="flex items-center gap-3 p-3 rounded-lg hover:bg-admin-hover transition-colors"
                >
                  <div className="w-8 h-8 rounded-lg bg-admin-success/20 flex items-center justify-center">
                    <Wifi className="w-4 h-4 text-admin-success" />
                  </div>
                  <div className="flex-1 min-w-0">
                    <p className="text-admin-text font-medium truncate">
                      {device.name}
                    </p>
                    <p className="text-admin-text-secondary text-xs truncate">
                      {device.id}
                    </p>
                  </div>
                  <div className="flex items-center gap-2">
                    <Badge variant="online">Online</Badge>
                    {device.machineBrand && (
                      <span className="text-xs text-admin-text-secondary">
                        {device.machineBrand}
                      </span>
                    )}
                  </div>
                </Link>
              ))
            )}
          </div>
        </div>
      </div>
    </div>
  );
}

