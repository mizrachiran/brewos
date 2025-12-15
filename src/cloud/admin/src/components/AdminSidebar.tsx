import { NavLink } from "react-router-dom";
import {
  LayoutDashboard,
  Users,
  Cpu,
  KeyRound,
  Activity,
  FileText,
  LogOut,
  Shield,
  X,
} from "lucide-react";
import type { AdminUser } from "../lib/types";

interface AdminSidebarProps {
  user: AdminUser;
  onLogout: () => void;
  isOpen: boolean;
  onClose: () => void;
}

const navItems = [
  { to: "/", icon: LayoutDashboard, label: "Dashboard", end: true },
  { to: "/users", icon: Users, label: "Users" },
  { to: "/devices", icon: Cpu, label: "Devices" },
  { to: "/sessions", icon: KeyRound, label: "Sessions" },
  { to: "/monitoring", icon: Activity, label: "Monitoring" },
  { to: "/logs", icon: FileText, label: "Logs" },
];

export function AdminSidebar({ user, onLogout, isOpen, onClose }: AdminSidebarProps) {
  return (
    <aside
      className={`
        fixed left-0 top-0 w-64 h-screen bg-admin-surface border-r border-admin-border flex flex-col z-50
        transition-transform duration-300 ease-in-out
        lg:translate-x-0
        ${isOpen ? "translate-x-0" : "-translate-x-full"}
      `}
    >
      {/* Logo */}
      <div className="p-6 border-b border-admin-border flex items-center justify-between">
        <div className="flex items-center gap-3">
          <div className="w-10 h-10 rounded-lg bg-admin-accent/20 flex items-center justify-center">
            <Shield className="w-5 h-5 text-admin-accent" />
          </div>
          <div>
            <h1 className="font-display font-bold text-lg text-admin-text">
              BrewOS
            </h1>
            <p className="text-xs text-admin-text-secondary">Admin Panel</p>
          </div>
        </div>
        {/* Close button - mobile only */}
        <button
          onClick={onClose}
          className="lg:hidden p-2 text-admin-text-secondary hover:text-admin-text hover:bg-admin-hover rounded-lg transition-colors"
          aria-label="Close menu"
        >
          <X className="w-5 h-5" />
        </button>
      </div>

      {/* Navigation */}
      <nav className="flex-1 p-4 space-y-1 overflow-y-auto">
        {navItems.map((item) => (
          <NavLink
            key={item.to}
            to={item.to}
            end={item.end}
            onClick={onClose}
            className={({ isActive }) =>
              `flex items-center gap-3 px-4 py-3 rounded-lg transition-all duration-200 ${
                isActive
                  ? "bg-admin-accent text-white"
                  : "text-admin-text-secondary hover:text-admin-text hover:bg-admin-hover"
              }`
            }
          >
            <item.icon className="w-5 h-5 flex-shrink-0" />
            <span className="font-medium">{item.label}</span>
          </NavLink>
        ))}
      </nav>

      {/* User section */}
      <div className="p-4 border-t border-admin-border">
        <div className="flex items-center gap-3 mb-4">
          {user.picture ? (
            <img
              src={user.picture}
              alt={user.name || "User"}
              className="w-10 h-10 rounded-full flex-shrink-0"
            />
          ) : (
            <div className="w-10 h-10 rounded-full bg-admin-accent/20 flex items-center justify-center flex-shrink-0">
              <span className="text-admin-accent font-bold">
                {(user.name || user.email || "?")[0].toUpperCase()}
              </span>
            </div>
          )}
          <div className="flex-1 min-w-0">
            <p className="font-medium text-admin-text truncate">
              {user.name || "Admin"}
            </p>
            <p className="text-xs text-admin-text-secondary truncate">
              {user.email}
            </p>
          </div>
        </div>
        <button
          onClick={() => {
            onClose();
            onLogout();
          }}
          className="flex items-center gap-2 w-full px-4 py-2 text-admin-text-secondary hover:text-admin-text hover:bg-admin-hover rounded-lg transition-colors"
        >
          <LogOut className="w-4 h-4" />
          <span className="text-sm">Sign out</span>
        </button>
      </div>
    </aside>
  );
}
