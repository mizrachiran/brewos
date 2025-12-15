import { ShieldOff, LogOut, Home } from "lucide-react";
import type { AdminUser } from "../lib/types";

interface AccessDeniedProps {
  user: AdminUser;
  onLogout: () => void;
}

export function AccessDenied({ user, onLogout }: AccessDeniedProps) {
  return (
    <div className="min-h-screen bg-admin-bg flex items-center justify-center p-4">
      <div className="admin-card max-w-md w-full text-center animate-fade-in">
        <div className="w-16 h-16 rounded-2xl bg-admin-danger/20 flex items-center justify-center mx-auto mb-6">
          <ShieldOff className="w-8 h-8 text-admin-danger" />
        </div>

        <h1 className="text-2xl font-display font-bold text-admin-text mb-2">
          Access Denied
        </h1>
        <p className="text-admin-text-secondary mb-6">
          Your account does not have admin privileges.
        </p>

        <div className="bg-admin-surface rounded-lg p-4 mb-6">
          <div className="flex items-center gap-3">
            {user.picture ? (
              <img
                src={user.picture}
                alt={user.name || "User"}
                className="w-10 h-10 rounded-full"
              />
            ) : (
              <div className="w-10 h-10 rounded-full bg-admin-accent/20 flex items-center justify-center">
                <span className="text-admin-accent font-bold">
                  {(user.name || user.email || "?")[0].toUpperCase()}
                </span>
              </div>
            )}
            <div className="text-left">
              <p className="text-admin-text font-medium">{user.name}</p>
              <p className="text-admin-text-secondary text-sm">{user.email}</p>
            </div>
          </div>
        </div>

        <p className="text-sm text-admin-text-secondary mb-6">
          If you believe this is an error, please contact an administrator.
        </p>

        <div className="flex gap-3">
          <button
            onClick={onLogout}
            className="btn btn-secondary flex-1 flex items-center justify-center gap-2"
          >
            <LogOut className="w-4 h-4" />
            Sign out
          </button>
          <a
            href="/"
            className="btn btn-primary flex-1 flex items-center justify-center gap-2"
          >
            <Home className="w-4 h-4" />
            Go to app
          </a>
        </div>
      </div>
    </div>
  );
}

