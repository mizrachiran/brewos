import { useEffect, useState, useCallback } from "react";
import { useNavigate } from "react-router-dom";
import {
  Cpu,
  Wifi,
  WifiOff,
  Trash2,
  MoreVertical,
  Eye,
  Power,
} from "lucide-react";
import { DataTable, SearchBar, Badge } from "../components/DataTable";
import { api, ApiError } from "../lib/api";
import type { AdminDeviceListItem, PaginatedResponse } from "../lib/types";

export function Devices() {
  const navigate = useNavigate();
  const [data, setData] = useState<PaginatedResponse<AdminDeviceListItem> | null>(
    null
  );
  const [loading, setLoading] = useState(true);
  const [search, setSearch] = useState("");
  const [onlineOnly, setOnlineOnly] = useState(false);
  const [page, setPage] = useState(1);
  const [actionMenu, setActionMenu] = useState<string | null>(null);
  const [actionLoading, setActionLoading] = useState<string | null>(null);

  const loadDevices = useCallback(async () => {
    setLoading(true);
    try {
      const result = await api.getDevices(
        page,
        20,
        search || undefined,
        onlineOnly
      );
      setData(result);
    } catch (error) {
      console.error("Failed to load devices:", error);
    } finally {
      setLoading(false);
    }
  }, [page, search, onlineOnly]);

  useEffect(() => {
    loadDevices();
  }, [loadDevices]);

  // Debounce search
  useEffect(() => {
    const timer = setTimeout(() => {
      setPage(1);
      loadDevices();
    }, 300);
    return () => clearTimeout(timer);
  }, [search, onlineOnly]);

  async function handleDisconnect(deviceId: string) {
    setActionLoading(deviceId);
    try {
      await api.disconnectDevice(deviceId);
      await loadDevices();
    } catch (error) {
      if (error instanceof ApiError) {
        alert(error.message);
      }
    } finally {
      setActionLoading(null);
      setActionMenu(null);
    }
  }

  async function handleDelete(deviceId: string) {
    if (!confirm("Are you sure you want to delete this device? This will remove it from all users.")) {
      return;
    }
    setActionLoading(deviceId);
    try {
      await api.deleteDevice(deviceId);
      await loadDevices();
    } catch (error) {
      if (error instanceof ApiError) {
        alert(error.message);
      }
    } finally {
      setActionLoading(null);
      setActionMenu(null);
    }
  }

  function formatLastSeen(lastSeenAt: string | null): string {
    if (!lastSeenAt) return "Never";
    const date = new Date(lastSeenAt);
    const now = new Date();
    const diff = now.getTime() - date.getTime();

    if (diff < 60000) return "Just now";
    if (diff < 3600000) return `${Math.floor(diff / 60000)}m ago`;
    if (diff < 86400000) return `${Math.floor(diff / 3600000)}h ago`;
    return date.toLocaleDateString();
  }

  const columns = [
    {
      key: "device",
      header: "Device",
      render: (device: AdminDeviceListItem) => (
        <div className="flex items-center gap-3">
          <div
            className={`w-8 h-8 rounded-lg flex items-center justify-center ${
              device.isOnline
                ? "bg-admin-success/20"
                : "bg-admin-text-secondary/20"
            }`}
          >
            {device.isOnline ? (
              <Wifi className="w-4 h-4 text-admin-success" />
            ) : (
              <WifiOff className="w-4 h-4 text-admin-text-secondary" />
            )}
          </div>
          <div>
            <p className="font-medium text-admin-text">{device.name}</p>
            <p className="text-admin-text-secondary text-xs font-mono">
              {device.id}
            </p>
          </div>
        </div>
      ),
    },
    {
      key: "machine",
      header: "Machine",
      render: (device: AdminDeviceListItem) => (
        <div className="text-admin-text-secondary">
          {device.machineBrand || device.machineModel ? (
            <>
              <span>{device.machineBrand || "—"}</span>
              {device.machineModel && (
                <span className="text-xs"> {device.machineModel}</span>
              )}
            </>
          ) : (
            <span>—</span>
          )}
        </div>
      ),
    },
    {
      key: "firmware",
      header: "Firmware",
      render: (device: AdminDeviceListItem) => (
        <span className="text-admin-text-secondary text-sm font-mono">
          {device.firmwareVersion || "—"}
        </span>
      ),
    },
    {
      key: "users",
      header: "Users",
      render: (device: AdminDeviceListItem) => (
        <span className="text-admin-text-secondary">{device.userCount}</span>
      ),
    },
    {
      key: "status",
      header: "Status",
      render: (device: AdminDeviceListItem) => (
        <Badge variant={device.isOnline ? "online" : "offline"}>
          {device.isOnline ? "Online" : "Offline"}
        </Badge>
      ),
    },
    {
      key: "lastSeen",
      header: "Last Seen",
      render: (device: AdminDeviceListItem) => (
        <span className="text-admin-text-secondary text-sm">
          {formatLastSeen(device.lastSeenAt)}
        </span>
      ),
    },
    {
      key: "actions",
      header: "",
      render: (device: AdminDeviceListItem) => (
        <div className="relative">
          <button
            onClick={(e) => {
              e.stopPropagation();
              setActionMenu(actionMenu === device.id ? null : device.id);
            }}
            className="p-2 hover:bg-admin-hover rounded-lg transition-colors"
            disabled={actionLoading === device.id}
          >
            <MoreVertical className="w-4 h-4 text-admin-text-secondary" />
          </button>
          {actionMenu === device.id && (
            <>
              <div
                className="fixed inset-0 z-10"
                onClick={() => setActionMenu(null)}
              />
              <div className="absolute right-0 top-full mt-1 w-48 bg-admin-card border border-admin-border rounded-lg shadow-admin z-20 py-1">
                <button
                  onClick={(e) => {
                    e.stopPropagation();
                    navigate(`/devices/${device.id}`);
                  }}
                  className="w-full px-4 py-2 text-left text-sm text-admin-text hover:bg-admin-hover flex items-center gap-2"
                >
                  <Eye className="w-4 h-4" />
                  View Details
                </button>
                {device.isOnline && (
                  <button
                    onClick={(e) => {
                      e.stopPropagation();
                      handleDisconnect(device.id);
                    }}
                    className="w-full px-4 py-2 text-left text-sm text-admin-warning hover:bg-admin-hover flex items-center gap-2"
                  >
                    <Power className="w-4 h-4" />
                    Force Disconnect
                  </button>
                )}
                <button
                  onClick={(e) => {
                    e.stopPropagation();
                    handleDelete(device.id);
                  }}
                  className="w-full px-4 py-2 text-left text-sm text-admin-danger hover:bg-admin-hover flex items-center gap-2"
                >
                  <Trash2 className="w-4 h-4" />
                  Delete Device
                </button>
              </div>
            </>
          )}
        </div>
      ),
      className: "w-12",
    },
  ];

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex flex-col gap-4">
        <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
          <div>
            <h1 className="text-xl sm:text-2xl font-display font-bold text-admin-text flex items-center gap-3">
              <Cpu className="w-6 h-6 sm:w-7 sm:h-7 text-admin-accent" />
              Devices
            </h1>
            <p className="text-admin-text-secondary mt-1 text-sm sm:text-base">
              Manage connected devices
            </p>
          </div>
          <SearchBar
            value={search}
            onChange={setSearch}
            placeholder="Search devices..."
          />
        </div>
        <label className="flex items-center gap-2 text-sm text-admin-text-secondary w-fit">
          <input
            type="checkbox"
            checked={onlineOnly}
            onChange={(e) => setOnlineOnly(e.target.checked)}
            className="w-4 h-4 rounded border-admin-border bg-admin-surface text-admin-accent focus:ring-admin-accent"
          />
          Online only
        </label>
      </div>

      {/* Table */}
      <div className="admin-card">
        <DataTable
          columns={columns}
          data={data?.items || []}
          loading={loading}
          keyExtractor={(device) => device.id}
          onRowClick={(device) => navigate(`/devices/${device.id}`)}
          emptyMessage="No devices found"
          pagination={
            data
              ? {
                  page: data.page,
                  pageSize: data.pageSize,
                  total: data.total,
                  totalPages: data.totalPages,
                  onPageChange: setPage,
                }
              : undefined
          }
        />
      </div>
    </div>
  );
}

