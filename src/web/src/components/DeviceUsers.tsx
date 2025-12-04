import { useState, useEffect } from "react";
import { Card } from "@/components/Card";
import { Button } from "@/components/Button";
import { ConfirmDialog } from "@/components/ConfirmDialog";
import { useToast } from "@/components/Toast";
import { useAppStore } from "@/lib/mode";
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
  const { user, getAccessToken } = useAppStore();
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

  const fetchUsers = async (showLoading = true) => {
    if (showLoading) {
      setLoading(true);
      setErrorState(null);
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
  };

  useEffect(() => {
    fetchUsers();
  }, [deviceId]);

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
    try {
      const token = await getAccessToken();
      if (!token) {
        error("Not authenticated");
        return;
      }

      const response = await fetch(
        `/api/devices/${deviceId}/users/${user.id}`,
        {
          method: "DELETE",
          headers: {
            Authorization: `Bearer ${token}`,
          },
        }
      );

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
        className="fixed inset-0 bg-black/50 backdrop-blur-sm flex items-center justify-center p-4 z-50 animate-in fade-in duration-200"
        onClick={(e) => {
          if (e.target === e.currentTarget) {
            onClose();
          }
        }}
      >
        <Card className="w-full max-w-md max-h-[85vh] overflow-hidden flex flex-col animate-in zoom-in-95 duration-200 shadow-2xl">
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
              users.map((deviceUser) => (
                <div
                  key={deviceUser.userId}
                  className="group flex items-center justify-between p-4 rounded-xl bg-theme-secondary/30 border border-theme/10 hover:border-theme/20 hover:bg-theme-secondary/50 transition-all duration-200"
                >
                  <div className="flex items-center gap-3 flex-1 min-w-0">
                    {deviceUser.avatarUrl ? (
                      <img
                        src={deviceUser.avatarUrl}
                        alt={getUserDisplayName(deviceUser)}
                        className="w-12 h-12 rounded-full flex-shrink-0 ring-2 ring-theme/10"
                      />
                    ) : (
                      <div className="w-12 h-12 rounded-full bg-gradient-to-br from-accent/20 to-accent/10 flex items-center justify-center flex-shrink-0 ring-2 ring-theme/10">
                        <User className="w-6 h-6 text-accent" />
                      </div>
                    )}
                    <div className="flex-1 min-w-0">
                      <div className="flex items-center gap-2 mb-1">
                        <p className="font-semibold text-sm text-theme truncate">
                          {getUserDisplayName(deviceUser)}
                        </p>
                        {isCurrentUser(deviceUser.userId) && (
                          <span className="text-xs px-2 py-0.5 rounded-full bg-accent/20 text-accent font-medium flex-shrink-0">
                            You
                          </span>
                        )}
                      </div>
                      <div className="flex items-center gap-1.5 text-xs text-theme-muted">
                        <Calendar className="w-3 h-3" />
                        <span>Added {formatDate(deviceUser.claimedAt)}</span>
                      </div>
                    </div>
                  </div>
                  {isCurrentUser(deviceUser.userId) ? (
                    <button
                      onClick={() => setConfirmLeave(true)}
                      disabled={leaving}
                      className="ml-2 w-8 h-8 rounded-lg text-theme-muted hover:text-error hover:bg-error-soft flex items-center justify-center transition-colors flex-shrink-0 disabled:opacity-50"
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
              ))
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
