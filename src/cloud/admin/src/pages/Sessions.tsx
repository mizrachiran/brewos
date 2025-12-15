import { useEffect, useState, useCallback } from "react";
import { Link } from "react-router-dom";
import { KeyRound, Trash2, Monitor, Smartphone, Globe } from "lucide-react";
import { DataTable } from "../components/DataTable";
import { api, ApiError } from "../lib/api";
import type { AdminSession, PaginatedResponse } from "../lib/types";

export function Sessions() {
  const [data, setData] = useState<PaginatedResponse<AdminSession> | null>(null);
  const [loading, setLoading] = useState(true);
  const [page, setPage] = useState(1);

  const loadSessions = useCallback(async () => {
    setLoading(true);
    try {
      const result = await api.getSessions(page, 20);
      setData(result);
    } catch (error) {
      console.error("Failed to load sessions:", error);
    } finally {
      setLoading(false);
    }
  }, [page]);

  useEffect(() => {
    loadSessions();
  }, [loadSessions]);

  async function handleRevoke(sessionId: string) {
    if (!confirm("Revoke this session?")) return;
    try {
      await api.revokeSession(sessionId);
      await loadSessions();
    } catch (error) {
      if (error instanceof ApiError) {
        alert(error.message);
      }
    }
  }

  function parseUserAgent(ua: string | null): { icon: typeof Monitor; label: string } {
    if (!ua) return { icon: Globe, label: "Unknown" };
    const lower = ua.toLowerCase();
    if (lower.includes("mobile") || lower.includes("android") || lower.includes("iphone")) {
      return { icon: Smartphone, label: "Mobile" };
    }
    return { icon: Monitor, label: "Desktop" };
  }

  function formatTimeAgo(date: string): string {
    const now = new Date();
    const then = new Date(date);
    const diff = now.getTime() - then.getTime();

    if (diff < 60000) return "Just now";
    if (diff < 3600000) return `${Math.floor(diff / 60000)}m ago`;
    if (diff < 86400000) return `${Math.floor(diff / 3600000)}h ago`;
    return `${Math.floor(diff / 86400000)}d ago`;
  }

  const columns = [
    {
      key: "user",
      header: "User",
      render: (session: AdminSession) => (
        <Link
          to={`/users/${session.userId}`}
          className="flex items-center gap-3 hover:text-admin-accent"
          onClick={(e) => e.stopPropagation()}
        >
          <div className="w-8 h-8 rounded-full bg-admin-accent/20 flex items-center justify-center">
            <span className="text-admin-accent text-sm font-bold">
              {(session.userDisplayName || session.userEmail || "?")[0].toUpperCase()}
            </span>
          </div>
          <div>
            <p className="font-medium text-admin-text">
              {session.userDisplayName || "No name"}
            </p>
            <p className="text-admin-text-secondary text-xs">
              {session.userEmail}
            </p>
          </div>
        </Link>
      ),
    },
    {
      key: "device",
      header: "Device",
      render: (session: AdminSession) => {
        const { icon: Icon, label } = parseUserAgent(session.userAgent);
        return (
          <div className="flex items-center gap-2">
            <Icon className="w-4 h-4 text-admin-text-secondary" />
            <span className="text-admin-text-secondary text-sm">{label}</span>
          </div>
        );
      },
    },
    {
      key: "ip",
      header: "IP Address",
      render: (session: AdminSession) => (
        <span className="text-admin-text-secondary text-sm font-mono">
          {session.ipAddress || "—"}
        </span>
      ),
    },
    {
      key: "created",
      header: "Created",
      render: (session: AdminSession) => (
        <span className="text-admin-text-secondary text-sm">
          {new Date(session.createdAt).toLocaleDateString()}
        </span>
      ),
    },
    {
      key: "lastUsed",
      header: "Last Used",
      render: (session: AdminSession) => (
        <span className="text-admin-text-secondary text-sm">
          {formatTimeAgo(session.lastUsedAt)}
        </span>
      ),
    },
    {
      key: "expires",
      header: "Expires",
      render: (session: AdminSession) => {
        const expires = new Date(session.accessExpiresAt);
        const isExpired = expires < new Date();
        return (
          <span
            className={`text-sm ${
              isExpired ? "text-admin-danger" : "text-admin-text-secondary"
            }`}
          >
            {isExpired ? "Expired" : formatTimeAgo(session.accessExpiresAt)}
          </span>
        );
      },
    },
    {
      key: "actions",
      header: "",
      render: (session: AdminSession) => (
        <button
          onClick={(e) => {
            e.stopPropagation();
            handleRevoke(session.id);
          }}
          className="btn btn-danger btn-sm flex items-center gap-1"
        >
          <Trash2 className="w-3 h-3" />
          Revoke
        </button>
      ),
      className: "w-24",
    },
  ];

  return (
    <div className="space-y-6">
      {/* Header */}
      <div>
        <h1 className="text-xl sm:text-2xl font-display font-bold text-admin-text flex items-center gap-3">
          <KeyRound className="w-6 h-6 sm:w-7 sm:h-7 text-admin-accent" />
          Sessions
        </h1>
        <p className="text-admin-text-secondary mt-1 text-sm sm:text-base">
          View and manage active user sessions
        </p>
      </div>

      {/* Stats */}
      {data && (
        <div className="admin-card">
          <p className="text-admin-text-secondary text-sm">Total Sessions</p>
          <p className="text-2xl font-bold text-admin-text font-display">
            {data.total}
          </p>
        </div>
      )}

      {/* Table */}
      <div className="admin-card">
        <DataTable
          columns={columns}
          data={data?.items || []}
          loading={loading}
          keyExtractor={(session) => session.id}
          emptyMessage="No sessions found"
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

