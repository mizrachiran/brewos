import { useEffect, useState } from "react";
import { useParams, useNavigate, Link } from "react-router-dom";
import {
  ArrowLeft,
  Shield,
  ShieldOff,
  Trash2,
  Cpu,
  KeyRound,
  Wifi,
  WifiOff,
  UserCog,
} from "lucide-react";
import { Badge } from "../components/DataTable";
import { api, ApiError } from "../lib/api";
import type { AdminUserDetail } from "../lib/types";

export function UserDetail() {
  const { id } = useParams<{ id: string }>();
  const navigate = useNavigate();
  const [user, setUser] = useState<AdminUserDetail | null>(null);
  const [loading, setLoading] = useState(true);
  const [actionLoading, setActionLoading] = useState(false);

  useEffect(() => {
    if (id) loadUser(id);
  }, [id]);

  async function loadUser(userId: string) {
    setLoading(true);
    try {
      const data = await api.getUser(userId);
      setUser(data);
    } catch (error) {
      console.error("Failed to load user:", error);
      navigate("/users");
    } finally {
      setLoading(false);
    }
  }

  async function handlePromote() {
    if (!user) return;
    setActionLoading(true);
    try {
      await api.promoteUser(user.id);
      await loadUser(user.id);
    } catch (error) {
      if (error instanceof ApiError) alert(error.message);
    } finally {
      setActionLoading(false);
    }
  }

  async function handleDemote() {
    if (!user) return;
    setActionLoading(true);
    try {
      await api.demoteUser(user.id);
      await loadUser(user.id);
    } catch (error) {
      if (error instanceof ApiError) alert(error.message);
    } finally {
      setActionLoading(false);
    }
  }

  async function handleDelete() {
    if (!user) return;
    if (!confirm("Are you sure you want to delete this user?")) return;
    setActionLoading(true);
    try {
      await api.deleteUser(user.id);
      navigate("/users");
    } catch (error) {
      if (error instanceof ApiError) alert(error.message);
      setActionLoading(false);
    }
  }

  async function handleImpersonate() {
    if (!user) return;
    if (!confirm("This will generate a temporary access token for this user. Continue?")) return;
    try {
      const result = await api.impersonateUser(user.id);
      // Copy to clipboard
      await navigator.clipboard.writeText(result.accessToken);
      alert("Access token copied to clipboard. Valid for 1 hour.");
    } catch (error) {
      if (error instanceof ApiError) alert(error.message);
    }
  }

  async function handleRevokeSession(sessionId: string) {
    if (!confirm("Revoke this session?")) return;
    try {
      await api.revokeSession(sessionId);
      if (user) await loadUser(user.id);
    } catch (error) {
      if (error instanceof ApiError) alert(error.message);
    }
  }

  if (loading) {
    return (
      <div className="flex items-center justify-center h-64">
        <div className="text-admin-text-secondary">Loading user...</div>
      </div>
    );
  }

  if (!user) {
    return (
      <div className="text-center py-12">
        <p className="text-admin-text-secondary">User not found</p>
      </div>
    );
  }

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center gap-4">
        <button
          onClick={() => navigate("/users")}
          className="p-2 hover:bg-admin-hover rounded-lg transition-colors"
        >
          <ArrowLeft className="w-5 h-5 text-admin-text-secondary" />
        </button>
        <div className="flex-1">
          <h1 className="text-2xl font-display font-bold text-admin-text">
            User Details
          </h1>
        </div>
      </div>

      {/* User Card */}
      <div className="admin-card">
        <div className="flex items-start justify-between">
          <div className="flex items-center gap-4">
            {user.avatarUrl ? (
              <img
                src={user.avatarUrl}
                alt={user.displayName || "User"}
                className="w-16 h-16 rounded-full"
              />
            ) : (
              <div className="w-16 h-16 rounded-full bg-admin-accent/20 flex items-center justify-center">
                <span className="text-admin-accent text-2xl font-bold">
                  {(user.displayName || user.email || "?")[0].toUpperCase()}
                </span>
              </div>
            )}
            <div>
              <div className="flex items-center gap-3">
                <h2 className="text-xl font-display font-bold text-admin-text">
                  {user.displayName || "No name"}
                </h2>
                {user.isAdmin && (
                  <Badge variant="admin">
                    <Shield className="w-3 h-3 mr-1" />
                    Admin
                  </Badge>
                )}
              </div>
              <p className="text-admin-text-secondary">{user.email}</p>
              <p className="text-admin-text-secondary text-sm mt-1">
                ID: {user.id}
              </p>
            </div>
          </div>

          <div className="flex gap-2">
            {user.isAdmin ? (
              <button
                onClick={handleDemote}
                disabled={actionLoading}
                className="btn btn-secondary btn-sm flex items-center gap-2"
              >
                <ShieldOff className="w-4 h-4" />
                Demote
              </button>
            ) : (
              <button
                onClick={handlePromote}
                disabled={actionLoading}
                className="btn btn-success btn-sm flex items-center gap-2"
              >
                <Shield className="w-4 h-4" />
                Promote
              </button>
            )}
            <button
              onClick={handleImpersonate}
              disabled={actionLoading}
              className="btn btn-secondary btn-sm flex items-center gap-2"
            >
              <UserCog className="w-4 h-4" />
              Impersonate
            </button>
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

        <div className="grid grid-cols-3 gap-4 mt-6 pt-6 border-t border-admin-border">
          <div className="text-center">
            <p className="text-2xl font-bold text-admin-text font-display">
              {user.deviceCount}
            </p>
            <p className="text-admin-text-secondary text-sm">Devices</p>
          </div>
          <div className="text-center">
            <p className="text-2xl font-bold text-admin-text font-display">
              {user.sessionCount}
            </p>
            <p className="text-admin-text-secondary text-sm">Sessions</p>
          </div>
          <div className="text-center">
            <p className="text-2xl font-bold text-admin-text font-display">
              {new Date(user.createdAt).toLocaleDateString()}
            </p>
            <p className="text-admin-text-secondary text-sm">Joined</p>
          </div>
        </div>
      </div>

      {/* Devices */}
      <div className="admin-card">
        <h3 className="text-lg font-display font-semibold text-admin-text mb-4 flex items-center gap-2">
          <Cpu className="w-5 h-5 text-admin-accent" />
          Devices ({user.devices.length})
        </h3>
        {user.devices.length === 0 ? (
          <p className="text-admin-text-secondary">No devices</p>
        ) : (
          <div className="space-y-3">
            {user.devices.map((device) => (
              <Link
                key={device.id}
                to={`/devices/${device.id}`}
                className="flex items-center gap-3 p-3 rounded-lg hover:bg-admin-hover transition-colors"
              >
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
                <div className="flex-1">
                  <p className="text-admin-text font-medium">{device.name}</p>
                  <p className="text-admin-text-secondary text-xs">
                    {device.id}
                  </p>
                </div>
                <Badge variant={device.isOnline ? "online" : "offline"}>
                  {device.isOnline ? "Online" : "Offline"}
                </Badge>
              </Link>
            ))}
          </div>
        )}
      </div>

      {/* Sessions */}
      <div className="admin-card">
        <h3 className="text-lg font-display font-semibold text-admin-text mb-4 flex items-center gap-2">
          <KeyRound className="w-5 h-5 text-admin-accent" />
          Active Sessions ({user.sessions.length})
        </h3>
        {user.sessions.length === 0 ? (
          <p className="text-admin-text-secondary">No active sessions</p>
        ) : (
          <div className="space-y-3">
            {user.sessions.map((session) => (
              <div
                key={session.id}
                className="flex items-center gap-3 p-3 rounded-lg bg-admin-surface"
              >
                <div className="flex-1">
                  <p className="text-admin-text text-sm font-mono">
                    {session.userAgent || "Unknown device"}
                  </p>
                  <p className="text-admin-text-secondary text-xs">
                    IP: {session.ipAddress || "Unknown"} • Last used:{" "}
                    {new Date(session.lastUsedAt).toLocaleString()}
                  </p>
                </div>
                <button
                  onClick={() => handleRevokeSession(session.id)}
                  className="btn btn-danger btn-sm"
                >
                  Revoke
                </button>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
}

