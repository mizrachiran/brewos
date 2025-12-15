import { useEffect, useState } from "react";
import { useParams, useNavigate, Link } from "react-router-dom";
import {
  ArrowLeft,
  Wifi,
  WifiOff,
  Trash2,
  Power,
  Users,
  Clock,
} from "lucide-react";
import { Badge } from "../components/DataTable";
import { api, ApiError } from "../lib/api";
import type { AdminDeviceDetail } from "../lib/types";

export function DeviceDetail() {
  const { id } = useParams<{ id: string }>();
  const navigate = useNavigate();
  const [device, setDevice] = useState<AdminDeviceDetail | null>(null);
  const [loading, setLoading] = useState(true);
  const [actionLoading, setActionLoading] = useState(false);

  useEffect(() => {
    if (id) loadDevice(id);
  }, [id]);

  async function loadDevice(deviceId: string) {
    setLoading(true);
    try {
      const data = await api.getDevice(deviceId);
      setDevice(data);
    } catch (error) {
      console.error("Failed to load device:", error);
      navigate("/devices");
    } finally {
      setLoading(false);
    }
  }

  async function handleDisconnect() {
    if (!device) return;
    setActionLoading(true);
    try {
      await api.disconnectDevice(device.id);
      await loadDevice(device.id);
    } catch (error) {
      if (error instanceof ApiError) alert(error.message);
    } finally {
      setActionLoading(false);
    }
  }

  async function handleDelete() {
    if (!device) return;
    if (
      !confirm(
        "Are you sure you want to delete this device? This will remove it from all users."
      )
    )
      return;
    setActionLoading(true);
    try {
      await api.deleteDevice(device.id);
      navigate("/devices");
    } catch (error) {
      if (error instanceof ApiError) alert(error.message);
      setActionLoading(false);
    }
  }

  function formatDate(date: string | null): string {
    if (!date) return "Never";
    return new Date(date).toLocaleString();
  }

  if (loading) {
    return (
      <div className="flex items-center justify-center h-64">
        <div className="text-admin-text-secondary">Loading device...</div>
      </div>
    );
  }

  if (!device) {
    return (
      <div className="text-center py-12">
        <p className="text-admin-text-secondary">Device not found</p>
      </div>
    );
  }

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center gap-4">
        <button
          onClick={() => navigate("/devices")}
          className="p-2 hover:bg-admin-hover rounded-lg transition-colors"
        >
          <ArrowLeft className="w-5 h-5 text-admin-text-secondary" />
        </button>
        <div className="flex-1">
          <h1 className="text-2xl font-display font-bold text-admin-text">
            Device Details
          </h1>
        </div>
      </div>

      {/* Device Card */}
      <div className="admin-card">
        <div className="flex items-start justify-between">
          <div className="flex items-center gap-4">
            <div
              className={`w-16 h-16 rounded-xl flex items-center justify-center ${
                device.isOnline
                  ? "bg-admin-success/20"
                  : "bg-admin-text-secondary/20"
              }`}
            >
              {device.isOnline ? (
                <Wifi className="w-8 h-8 text-admin-success" />
              ) : (
                <WifiOff className="w-8 h-8 text-admin-text-secondary" />
              )}
            </div>
            <div>
              <div className="flex items-center gap-3">
                <h2 className="text-xl font-display font-bold text-admin-text">
                  {device.name}
                </h2>
                <Badge variant={device.isOnline ? "online" : "offline"}>
                  {device.isOnline ? "Online" : "Offline"}
                </Badge>
              </div>
              <p className="text-admin-text-secondary font-mono">{device.id}</p>
            </div>
          </div>

          <div className="flex gap-2">
            {device.isOnline && (
              <button
                onClick={handleDisconnect}
                disabled={actionLoading}
                className="btn btn-secondary btn-sm flex items-center gap-2"
              >
                <Power className="w-4 h-4" />
                Disconnect
              </button>
            )}
            <button
              onClick={handleDelete}
              disabled={actionLoading}
              className="btn btn-danger btn-sm flex items-center gap-2"
            >
              <Trash2 className="w-4 h-4" />
              Delete
            </button>
          </div>
        </div>

        {/* Device Info Grid */}
        <div className="grid grid-cols-2 md:grid-cols-4 gap-4 mt-6 pt-6 border-t border-admin-border">
          <div>
            <p className="text-admin-text-secondary text-sm">Machine Brand</p>
            <p className="text-admin-text font-medium">
              {device.machineBrand || "—"}
            </p>
          </div>
          <div>
            <p className="text-admin-text-secondary text-sm">Machine Model</p>
            <p className="text-admin-text font-medium">
              {device.machineModel || "—"}
            </p>
          </div>
          <div>
            <p className="text-admin-text-secondary text-sm">Machine Type</p>
            <p className="text-admin-text font-medium">
              {device.machineType || "—"}
            </p>
          </div>
          <div>
            <p className="text-admin-text-secondary text-sm">Firmware</p>
            <p className="text-admin-text font-mono">
              {device.firmwareVersion || "—"}
            </p>
          </div>
        </div>

        <div className="grid grid-cols-3 gap-4 mt-6 pt-6 border-t border-admin-border">
          <div className="text-center">
            <p className="text-2xl font-bold text-admin-text font-display">
              {device.userCount}
            </p>
            <p className="text-admin-text-secondary text-sm">Users</p>
          </div>
          <div className="text-center">
            <p className="text-sm font-medium text-admin-text">
              {formatDate(device.lastSeenAt)}
            </p>
            <p className="text-admin-text-secondary text-sm">Last Seen</p>
          </div>
          <div className="text-center">
            <p className="text-sm font-medium text-admin-text">
              {new Date(device.createdAt).toLocaleDateString()}
            </p>
            <p className="text-admin-text-secondary text-sm">Created</p>
          </div>
        </div>
      </div>

      {/* Users */}
      <div className="admin-card">
        <h3 className="text-lg font-display font-semibold text-admin-text mb-4 flex items-center gap-2">
          <Users className="w-5 h-5 text-admin-accent" />
          Users ({device.users.length})
        </h3>
        {device.users.length === 0 ? (
          <p className="text-admin-text-secondary">No users have claimed this device</p>
        ) : (
          <div className="space-y-3">
            {device.users.map((user) => (
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
                <div className="flex-1">
                  <p className="text-admin-text font-medium">
                    {user.displayName || "No name"}
                  </p>
                  <p className="text-admin-text-secondary text-xs">
                    {user.email}
                  </p>
                </div>
                <div className="text-admin-text-secondary text-xs flex items-center gap-1">
                  <Clock className="w-3 h-3" />
                  Claimed {new Date(user.claimedAt).toLocaleDateString()}
                </div>
              </Link>
            ))}
          </div>
        )}
      </div>
    </div>
  );
}

