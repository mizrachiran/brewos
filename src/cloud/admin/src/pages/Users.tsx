import { useEffect, useState, useCallback } from "react";
import { useNavigate } from "react-router-dom";
import {
  Users as UsersIcon,
  Shield,
  ShieldOff,
  Trash2,
  MoreVertical,
  Eye,
  UserPlus,
} from "lucide-react";
import { DataTable, SearchBar, Badge } from "../components/DataTable";
import { api, ApiError } from "../lib/api";
import type { AdminUserListItem, PaginatedResponse } from "../lib/types";

export function Users() {
  const navigate = useNavigate();
  const [data, setData] = useState<PaginatedResponse<AdminUserListItem> | null>(
    null
  );
  const [loading, setLoading] = useState(true);
  const [search, setSearch] = useState("");
  const [page, setPage] = useState(1);
  const [actionMenu, setActionMenu] = useState<string | null>(null);
  const [actionLoading, setActionLoading] = useState<string | null>(null);

  const loadUsers = useCallback(async () => {
    setLoading(true);
    try {
      const result = await api.getUsers(page, 20, search || undefined);
      setData(result);
    } catch (error) {
      console.error("Failed to load users:", error);
    } finally {
      setLoading(false);
    }
  }, [page, search]);

  useEffect(() => {
    loadUsers();
  }, [loadUsers]);

  // Debounce search
  useEffect(() => {
    const timer = setTimeout(() => {
      setPage(1);
      loadUsers();
    }, 300);
    return () => clearTimeout(timer);
  }, [search]);

  async function handlePromote(userId: string) {
    setActionLoading(userId);
    try {
      await api.promoteUser(userId);
      await loadUsers();
    } catch (error) {
      if (error instanceof ApiError) {
        alert(error.message);
      }
    } finally {
      setActionLoading(null);
      setActionMenu(null);
    }
  }

  async function handleDemote(userId: string) {
    setActionLoading(userId);
    try {
      await api.demoteUser(userId);
      await loadUsers();
    } catch (error) {
      if (error instanceof ApiError) {
        alert(error.message);
      }
    } finally {
      setActionLoading(null);
      setActionMenu(null);
    }
  }

  async function handleDelete(userId: string) {
    if (!confirm("Are you sure you want to delete this user? This action cannot be undone.")) {
      return;
    }
    setActionLoading(userId);
    try {
      await api.deleteUser(userId);
      await loadUsers();
    } catch (error) {
      if (error instanceof ApiError) {
        alert(error.message);
      }
    } finally {
      setActionLoading(null);
      setActionMenu(null);
    }
  }

  const columns = [
    {
      key: "user",
      header: "User",
      render: (user: AdminUserListItem) => (
        <div className="flex items-center gap-3">
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
          <div>
            <p className="font-medium text-admin-text">
              {user.displayName || "No name"}
            </p>
            <p className="text-admin-text-secondary text-xs">{user.email}</p>
          </div>
        </div>
      ),
    },
    {
      key: "devices",
      header: "Devices",
      render: (user: AdminUserListItem) => (
        <span className="text-admin-text-secondary">{user.deviceCount}</span>
      ),
    },
    {
      key: "sessions",
      header: "Sessions",
      render: (user: AdminUserListItem) => (
        <span className="text-admin-text-secondary">{user.sessionCount}</span>
      ),
    },
    {
      key: "admin",
      header: "Admin",
      render: (user: AdminUserListItem) =>
        user.isAdmin ? (
          <Badge variant="admin">
            <Shield className="w-3 h-3 mr-1" />
            Admin
          </Badge>
        ) : (
          <span className="text-admin-text-secondary">—</span>
        ),
    },
    {
      key: "createdAt",
      header: "Joined",
      render: (user: AdminUserListItem) => (
        <span className="text-admin-text-secondary text-sm">
          {new Date(user.createdAt).toLocaleDateString()}
        </span>
      ),
    },
    {
      key: "actions",
      header: "",
      render: (user: AdminUserListItem) => (
        <div className="relative">
          <button
            onClick={(e) => {
              e.stopPropagation();
              setActionMenu(actionMenu === user.id ? null : user.id);
            }}
            className="p-2 hover:bg-admin-hover rounded-lg transition-colors"
            disabled={actionLoading === user.id}
          >
            <MoreVertical className="w-4 h-4 text-admin-text-secondary" />
          </button>
          {actionMenu === user.id && (
            <>
              <div
                className="fixed inset-0 z-10"
                onClick={() => setActionMenu(null)}
              />
              <div className="absolute right-0 top-full mt-1 w-48 bg-admin-card border border-admin-border rounded-lg shadow-admin z-20 py-1">
                <button
                  onClick={(e) => {
                    e.stopPropagation();
                    navigate(`/users/${user.id}`);
                  }}
                  className="w-full px-4 py-2 text-left text-sm text-admin-text hover:bg-admin-hover flex items-center gap-2"
                >
                  <Eye className="w-4 h-4" />
                  View Details
                </button>
                {user.isAdmin ? (
                  <button
                    onClick={(e) => {
                      e.stopPropagation();
                      handleDemote(user.id);
                    }}
                    className="w-full px-4 py-2 text-left text-sm text-admin-warning hover:bg-admin-hover flex items-center gap-2"
                  >
                    <ShieldOff className="w-4 h-4" />
                    Demote from Admin
                  </button>
                ) : (
                  <button
                    onClick={(e) => {
                      e.stopPropagation();
                      handlePromote(user.id);
                    }}
                    className="w-full px-4 py-2 text-left text-sm text-admin-success hover:bg-admin-hover flex items-center gap-2"
                  >
                    <UserPlus className="w-4 h-4" />
                    Promote to Admin
                  </button>
                )}
                <button
                  onClick={(e) => {
                    e.stopPropagation();
                    handleDelete(user.id);
                  }}
                  className="w-full px-4 py-2 text-left text-sm text-admin-danger hover:bg-admin-hover flex items-center gap-2"
                >
                  <Trash2 className="w-4 h-4" />
                  Delete User
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
      <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
        <div>
          <h1 className="text-xl sm:text-2xl font-display font-bold text-admin-text flex items-center gap-3">
            <UsersIcon className="w-6 h-6 sm:w-7 sm:h-7 text-admin-accent" />
            Users
          </h1>
          <p className="text-admin-text-secondary mt-1 text-sm sm:text-base">
            Manage user accounts and admin privileges
          </p>
        </div>
        <SearchBar
          value={search}
          onChange={setSearch}
          placeholder="Search users..."
        />
      </div>

      {/* Table */}
      <div className="admin-card">
        <DataTable
          columns={columns}
          data={data?.items || []}
          loading={loading}
          keyExtractor={(user) => user.id}
          onRowClick={(user) => navigate(`/users/${user.id}`)}
          emptyMessage="No users found"
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

