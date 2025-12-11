import { useState, useEffect, useCallback } from "react";
import { Card } from "@/components/Card";
import { Button } from "@/components/Button";
import { ConfirmDialog } from "@/components/ConfirmDialog";
import { useToast } from "@/components/Toast";
import { useAppStore } from "@/lib/mode";
import { isDemoMode } from "@/lib/demo-mode";
import {
  UserX,
  Loader2,
  Calendar,
  X,
  RefreshCw,
  AlertCircle,
  User,
  LogOut,
} from "lucide-react";

interface DeviceUser {
  userId: string;
  email: string | null;
  displayName: string | null;
  avatarUrl: string | null;
  claimedAt: string;
}

// Demo mode mock data
const DEMO_USER = {
  id: "demo-user",
  email: "demo@brewos.io",
  name: "Demo User",
};

const DEMO_USERS: DeviceUser[] = [
  {
    userId: "demo-user",
    email: "demo@brewos.io",
    displayName: "Demo User",
    avatarUrl: null,
    claimedAt: new Date(Date.now() - 30 * 24 * 60 * 60 * 1000).toISOString(), // 30 days ago
  },
  {
    userId: "demo-user-2",
    email: "alex@example.com",
    displayName: "Alex Chen",
    avatarUrl: null,
    claimedAt: new Date(Date.now() - 7 * 24 * 60 * 60 * 1000).toISOString(), // 7 days ago
  },
];

interface DeviceUsersProps {
  deviceId: string;
  deviceName: string;
  onClose: () => void;
  onLeave?: () => void; // Called when user leaves the device
}

export function DeviceUsers({
  deviceId,
  deviceName,
  onClose,
  onLeave,
}: DeviceUsersProps) {
  const { user: realUser, getAccessToken } = useAppStore();
  const { success, error } = useToast();
  const [users, setUsers] = useState<DeviceUser[]>([]);
  const [loading, setLoading] = useState(true);
  const [errorState, setErrorState] = useState<string | null>(null);
  const [removingUserId, setRemovingUserId] = useState<string | null>(null);
  const [confirmRemove, setConfirmRemove] = useState<{
    userId: string;
    userName: string;
  } | null>(null);
  const [confirmLeave, setConfirmLeave] = useState(false);
  const [leaving, setLeaving] = useState(false);

  const isDemo = isDemoMode();
  const user = isDemo ? DEMO_USER : realUser;

  const fetchUsers = useCallback(
    async (showLoading = true) => {
      if (showLoading) {
        setLoading(true);
        setErrorState(null);
      }

      // In demo mode, return mock users
      if (isDemo) {
        // Simulate a small delay for realism
        await new Promise((resolve) => setTimeout(resolve, 300));
        setUsers(DEMO_USERS);
        setErrorState(null);
        setLoading(false);
        return;
      }

      try {
        const token = await getAccessToken();
        if (!token) {
          setErrorState("Not authenticated");
          error("Not authenticated");
          return;
        }

        const response = await fetch(`/api/devices/${deviceId}/users`, {
          headers: {
            Authorization: `Bearer ${token}`,
          },
        });

        if (response.ok) {
          const data = await response.json();
          setUsers(data.users || []);
          setErrorState(null);
        } else {
          const errorMsg = "Failed to load device users";
          setErrorState(errorMsg);
          error(errorMsg);
        }
      } catch (err) {
        console.error("Failed to fetch device users:", err);
        const errorMsg = "Failed to load device users";
        setErrorState(errorMsg);
        error(errorMsg);
      } finally {
        setLoading(false);
      }
    },
    [deviceId, getAccessToken, error, isDemo]
  );

  useEffect(() => {
    fetchUsers();
  }, [fetchUsers]);

  // Close on ESC key
  useEffect(() => {
    const handleEscape = (e: KeyboardEvent) => {
      if (e.key === "Escape") {
        onClose();
      }
    };
    window.addEventListener("keydown", handleEscape);
    return () => window.removeEventListener("keydown", handleEscape);
  }, [onClose]);

  const handleRemoveUser = async (userIdToRemove: string) => {
    if (!confirmRemove || confirmRemove.userId !== userIdToRemove) return;

    setRemovingUserId(userIdToRemove);

    // In demo mode, simulate the action
    if (isDemo) {
      await new Promise((resolve) => setTimeout(resolve, 500));
      setUsers((prev) => prev.filter((u) => u.userId !== userIdToRemove));
      success("User access revoked successfully");
      setConfirmRemove(null);
      setRemovingUserId(null);
      return;
    }

    try {
      const token = await getAccessToken();
      if (!token) {
        error("Not authenticated");
        return;
      }

      const response = await fetch(
        `/api/devices/${deviceId}/users/${userIdToRemove}`,
        {
          method: "DELETE",
          headers: {
            Authorization: `Bearer ${token}`,
          },
        }
      );

      if (response.ok) {
        success("User access revoked successfully");
        setConfirmRemove(null);
        // Refresh users list
        await fetchUsers();
      } else {
        const data = await response.json();
        error(data.error || "Failed to revoke user access");
      }
    } catch (err) {
      console.error("Failed to revoke user access:", err);
      error("Failed to revoke user access");
    } finally {
      setRemovingUserId(null);
    }
  };

  const handleLeave = async () => {
    if (!user?.id) return;

    setLeaving(true);

    // In demo mode, simulate the action
    if (isDemo) {
      await new Promise((resolve) => setTimeout(resolve, 500));
      success("You have left this device");
      setConfirmLeave(false);
      setLeaving(false);
      onClose();
      onLeave?.();
      return;
    }

    try {
      const token = await getAccessToken();
      if (!token) {
        error("Not authenticated");
        return;
      }

      // Use removeDevice endpoint (DELETE /api/devices/:id) to leave a device
      // This removes the device from the current user's account
      const response = await fetch(`/api/devices/${deviceId}`, {
        method: "DELETE",
        headers: {
          Authorization: `Bearer ${token}`,
        },
      });

      if (response.ok) {
        success("You have left this device");
        setConfirmLeave(false);
        onClose();
        onLeave?.();
      } else {
        const data = await response.json();
        error(data.error || "Failed to leave device");
      }
    } catch (err) {
      console.error("Failed to leave device:", err);
      error("Failed to leave device");
    } finally {
      setLeaving(false);
    }
  };

  const formatDate = (dateString: string) => {
    try {
      const date = new Date(dateString);
      const now = new Date();
      const diffMs = now.getTime() - date.getTime();
      const diffDays = Math.floor(diffMs / (1000 * 60 * 60 * 24));

      if (diffDays === 0) {
        return "Today";
      } else if (diffDays === 1) {
        return "Yesterday";
      } else if (diffDays < 7) {
        return `${diffDays} days ago`;
      } else if (diffDays < 30) {
        const weeks = Math.floor(diffDays / 7);
        return `${weeks} ${weeks === 1 ? "week" : "weeks"} ago`;
      } else {
        return date.toLocaleDateString(undefined, {
          year: "numeric",
          month: "short",
          day: "numeric",
        });
      }
    } catch {
      return "Unknown";
    }
  };

  const getUserDisplayName = (user: DeviceUser) => {
    return user.displayName || user.email || "Unknown User";
  };

  const isCurrentUser = (userId: string) => {
    return user?.id === userId;
  };

  return (
    <>
      <div
        className="fixed inset-0 bg-black/50 xs:backdrop-blur-sm flex items-center justify-center xs:p-4 z-50 animate-in fade-in duration-200"
        onClick={(e) => {
          if (e.target === e.currentTarget) {
            onClose();
          }
        }}
      >
        <Card className="w-full h-full xs:h-auto xs:max-w-md xs:max-h-[85vh] overflow-hidden flex flex-col animate-in zoom-in-95 duration-200 xs:shadow-2xl rounded-none xs:rounded-2xl">
          {/* Clean header - title left, close button right */}
          <div className="flex items-center justify-between p-4 border-b border-theme/10 flex-shrink-0">
            <div className="flex-1 min-w-0 pr-4">
              <h2 className="text-lg font-semibold text-theme">Device Users</h2>
              <p className="text-sm text-theme-muted truncate">{deviceName}</p>
            </div>
            <button
              onClick={onClose}
              className="w-8 h-8 rounded-lg hover:bg-theme-secondary flex items-center justify-center transition-colors flex-shrink-0"
              title="Close"
            >
              <X className="w-4 h-4 text-theme-muted" />
            </button>
          </div>

          <div className="flex-1 overflow-y-auto p-4 space-y-3">
            {loading && users.length === 0 ? (
              <div className="flex flex-col items-center justify-center py-12">
                <Loader2 className="w-8 h-8 text-accent animate-spin mb-3" />
                <p className="text-sm text-theme-muted">Loading users...</p>
              </div>
            ) : errorState ? (
              <div className="flex flex-col items-center justify-center py-12 text-center">
                <div className="w-12 h-12 rounded-full bg-error-soft flex items-center justify-center mb-3">
                  <AlertCircle className="w-6 h-6 text-error" />
                </div>
                <p className="font-medium text-theme mb-1">
                  Failed to load users
                </p>
                <p className="text-sm text-theme-muted mb-4">{errorState}</p>
                <Button
                  variant="ghost"
                  size="sm"
                  onClick={() => fetchUsers()}
                  className="gap-2"
                >
                  <RefreshCw className="w-4 h-4" />
                  Try Again
                </Button>
              </div>
            ) : users.length === 0 ? (
              <div className="flex flex-col items-center justify-center py-12 text-center">
                <div className="w-16 h-16 rounded-full bg-theme-secondary flex items-center justify-center mb-4">
                  <User className="w-8 h-8 text-theme-muted" />
                </div>
                <p className="font-medium text-theme mb-1">No other users</p>
                <p className="text-sm text-theme-muted max-w-xs">
                  You're the only one with access to this device. Share the QR
                  code to let others add it to their account.
                </p>
              </div>
            ) : (
              users.map((deviceUser) => {
                const isCurrent = isCurrentUser(deviceUser.userId);
                return (
                  <div
                    key={deviceUser.userId}
                    className={`group flex items-center justify-between p-4 rounded-xl border transition-all duration-200 ${
                      isCurrent
                        ? "bg-accent/10 border-accent/30 hover:border-accent/50"
                        : "bg-theme-secondary/30 border-theme/10 hover:border-theme/20 hover:bg-theme-secondary/50"
                    }`}
                  >
                    <div className="flex items-center gap-3 flex-1 min-w-0">
                      {deviceUser.avatarUrl ? (
                        <img
                          src={deviceUser.avatarUrl}
                          alt={getUserDisplayName(deviceUser)}
                          className={`w-12 h-12 rounded-full flex-shrink-0 ring-2 ${
                            isCurrent ? "ring-accent/30" : "ring-theme/10"
                          }`}
                        />
                      ) : (
                        <div
                          className={`w-12 h-12 rounded-full flex items-center justify-center flex-shrink-0 ring-2 ${
                            isCurrent
                              ? "bg-accent/20 ring-accent/30"
                              : "bg-gradient-to-br from-accent/20 to-accent/10 ring-theme/10"
                          }`}
                        >
                          <User
                            className="w-6 h-6 text-accent"
                          />
                        </div>
                      )}
                      <div className="flex-1 min-w-0">
                        <div className="flex items-center gap-2 mb-0.5">
                          <p className="font-semibold text-sm text-theme truncate">
                            {getUserDisplayName(deviceUser)}
                          </p>
                          {isCurrent && (
                            <span className="text-xs px-2 py-0.5 rounded-full bg-accent text-white font-medium flex-shrink-0">
                              You
                            </span>
                          )}
                        </div>
                        {isCurrent && deviceUser.email && (
                          <p className="text-xs text-theme-muted truncate mb-0.5">
                            {deviceUser.email}
                          </p>
                        )}
                        <div className="flex items-center gap-1.5 text-xs text-theme-muted">
                          <Calendar className="w-3 h-3" />
                          <span>Added {formatDate(deviceUser.claimedAt)}</span>
                        </div>
                      </div>
                    </div>
                    {isCurrent ? (
                      <button
                        onClick={() => setConfirmLeave(true)}
                        disabled={leaving}
                        className="ml-2 w-8 h-8 rounded-lg text-accent hover:text-error hover:bg-error-soft flex items-center justify-center transition-colors flex-shrink-0 disabled:opacity-50"
                        title="Leave device"
                      >
                        {leaving ? (
                          <Loader2 className="w-4 h-4 animate-spin" />
                        ) : (
                          <LogOut className="w-4 h-4" />
                        )}
                      </button>
                    ) : (
                      <button
                        onClick={() =>
                          setConfirmRemove({
                            userId: deviceUser.userId,
                            userName: getUserDisplayName(deviceUser),
                          })
                        }
                        disabled={removingUserId === deviceUser.userId}
                        className="ml-2 w-8 h-8 rounded-lg text-theme-muted hover:text-error hover:bg-error-soft flex items-center justify-center transition-colors flex-shrink-0 disabled:opacity-50"
                        title="Remove user"
                      >
                        {removingUserId === deviceUser.userId ? (
                          <Loader2 className="w-4 h-4 animate-spin" />
                        ) : (
                          <UserX className="w-4 h-4" />
                        )}
                      </button>
                    )}
                  </div>
                );
              })
            )}
          </div>

          {/* Footer with user count and refresh */}
          {!loading && !errorState && users.length > 0 && (
            <div className="flex items-center justify-between px-4 py-3 border-t border-theme/10 bg-theme-secondary/20 flex-shrink-0">
              <span className="text-xs text-theme-muted">
                {users.length} {users.length === 1 ? "user" : "users"} with
                access
              </span>
              <button
                onClick={() => fetchUsers(false)}
                disabled={loading}
                className="text-xs text-theme-muted hover:text-theme transition-colors flex items-center gap-1 disabled:opacity-50"
              >
                <RefreshCw
                  className={`w-3 h-3 ${loading ? "animate-spin" : ""}`}
                />
                Refresh
              </button>
            </div>
          )}
        </Card>
      </div>

      <ConfirmDialog
        isOpen={!!confirmRemove}
        onClose={() => setConfirmRemove(null)}
        onConfirm={() =>
          confirmRemove && handleRemoveUser(confirmRemove.userId)
        }
        title="Revoke Access?"
        description={`Are you sure you want to remove ${confirmRemove?.userName} from this device? They will no longer be able to access or control it. This action cannot be undone.`}
        confirmText="Remove User"
        variant="danger"
        confirmLoading={removingUserId !== null}
      />

      <ConfirmDialog
        isOpen={confirmLeave}
        onClose={() => setConfirmLeave(false)}
        onConfirm={handleLeave}
        title="Leave Device?"
        description={`Are you sure you want to leave "${deviceName}"? You will no longer be able to access or control it. You can re-add it later by scanning its QR code.`}
        confirmText="Leave Device"
        variant="danger"
        confirmLoading={leaving}
      />
    </>
  );
}
