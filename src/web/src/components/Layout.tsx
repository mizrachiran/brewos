import { useState } from "react";
import { Outlet, NavLink, useParams, Link } from "react-router-dom";
import { useStore } from "@/lib/store";
import { useAppStore } from "@/lib/mode";
import { Logo } from "./Logo";
import { InstallPrompt, usePWAInstall } from "./InstallPrompt";
import { ConnectionOverlay } from "./ConnectionOverlay";
import { DeviceOfflineBanner } from "./DeviceOfflineBanner";
import { VersionWarning } from "./VersionWarning";
import { UserMenu } from "./UserMenu";
import { BrewingModeOverlay } from "./BrewingModeOverlay";
import { isDemoMode } from "@/lib/demo-mode";
import {
  LayoutGrid,
  Coffee,
  Settings,
  Calendar,
  BarChart3,
  Wifi,
  WifiOff,
  Cloud,
  Sparkles,
  ChevronRight,
} from "lucide-react";
import { cn } from "@/lib/utils";

// Navigation items - adjust based on mode
const getNavigation = (isCloud: boolean, deviceId?: string) => {
  const basePath = isCloud && deviceId ? `/machine/${deviceId}` : "";

  const items = [
    { name: "Dashboard", href: basePath || "/", icon: LayoutGrid },
    { name: "Brewing", href: `${basePath}/brewing`, icon: Coffee },
    { name: "Stats", href: `${basePath}/stats`, icon: BarChart3 },
    { name: "Schedules", href: `${basePath}/schedules`, icon: Calendar },
    { name: "Settings", href: `${basePath}/settings`, icon: Settings },
  ];

  return items;
};

interface LayoutProps {
  onExitDemo?: () => void;
}

export function Layout({ onExitDemo }: LayoutProps) {
  const { deviceId } = useParams();
  const connectionState = useStore((s) => s.connectionState);
  const deviceName = useStore((s) => s.device.deviceName);
  const { mode, user, getSelectedDevice } = useAppStore();
  const { isMobile } = usePWAInstall();
  const [showInstallBanner, setShowInstallBanner] = useState(() => {
    // Check if user previously dismissed the banner
    return localStorage.getItem("brewos-install-dismissed") !== "true";
  });

  const isCloud = mode === "cloud";
  const isDemo = isDemoMode();
  const selectedDevice = getSelectedDevice();

  const isConnected = connectionState === "connected";
  const isConnecting =
    connectionState === "connecting" || connectionState === "reconnecting";

  const dismissInstallBanner = () => {
    setShowInstallBanner(false);
    localStorage.setItem("brewos-install-dismissed", "true");
  };

  const navigation = getNavigation(isCloud, deviceId || selectedDevice?.id);

  return (
    <div className="full-page-scroll bg-theme">
      {/* Header */}
      <header className="sticky top-0 z-50 header-glass border-b border-theme">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="flex items-center justify-between h-16">
            {/* Logo & Mode */}
            <div className="flex items-center gap-4">
              {/* Hide full logo on very small screens, show icon only */}
              <Logo size="md" className="hidden xs:flex" />
              <Logo size="md" iconOnly className="xs:hidden" />

              {/* Cloud: Clickable machine indicator */}
              {isCloud && selectedDevice && (
                <Link
                  to="/machines"
                  className="hidden sm:flex items-center gap-2 px-3 py-1.5 rounded-xl bg-theme-tertiary hover:bg-theme-secondary transition-colors group"
                >
                  <div
                    className={`w-2 h-2 rounded-full ${
                      selectedDevice.isOnline
                        ? "bg-emerald-500"
                        : "bg-theme-muted"
                    }`}
                  />
                  <span className="text-sm font-medium text-theme-secondary group-hover:text-theme max-w-[150px] truncate">
                    {selectedDevice.name}
                  </span>
                  <ChevronRight className="w-3.5 h-3.5 text-theme-muted group-hover:text-theme-secondary transition-colors" />
                </Link>
              )}

              {/* Local: Machine Name */}
              {!isCloud && deviceName && (
                <span className="hidden sm:block text-sm font-medium text-theme-secondary">
                  {deviceName}
                </span>
              )}
            </div>

            {/* Right side */}
            <div className="flex items-center gap-2 xs:gap-3">
              {/* Connection Status - icon only on very small screens */}
              <div className="flex items-center gap-2 px-2 xs:px-3 py-1.5 rounded-full bg-theme-tertiary">
                {isDemo ? (
                  <>
                    <Sparkles className="w-4 h-4 text-accent" />
                    <span className="hidden xs:inline text-xs font-medium text-theme-secondary">
                      Demo
                    </span>
                  </>
                ) : isCloud ? (
                  <>
                    <Cloud className="w-4 h-4 text-accent" />
                    <span className="hidden xs:inline text-xs font-medium text-theme-secondary">
                      Cloud
                    </span>
                  </>
                ) : isConnected ? (
                  <>
                    <Wifi className="w-4 h-4 text-emerald-500" />
                    <span className="hidden xs:inline text-xs font-medium text-theme-secondary">
                      Local
                    </span>
                  </>
                ) : isConnecting ? (
                  <>
                    <Wifi className="w-4 h-4 text-amber-500 animate-pulse" />
                    <span className="hidden xs:inline text-xs font-medium text-theme-secondary">
                      ...
                    </span>
                  </>
                ) : (
                  <>
                    <WifiOff className="w-4 h-4 text-red-500" />
                    <span className="hidden xs:inline text-xs font-medium text-theme-secondary">
                      Offline
                    </span>
                  </>
                )}
              </div>

              {/* User menu - Cloud mode or Demo mode */}
              {(isCloud && user) || isDemo ? (
                <UserMenu onExitDemo={onExitDemo} />
              ) : null}
            </div>
          </div>
        </div>
      </header>

      {/* Device Offline Banner - Cloud mode only */}
      {isCloud && selectedDevice && !selectedDevice.isOnline && (
        <DeviceOfflineBanner deviceName={selectedDevice.name} />
      )}

      {/* Install App Banner - shown to mobile users only, not in demo mode */}
      {showInstallBanner && isMobile && !isDemo && (
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-2">
          <InstallPrompt
            variant="banner"
            onInstalled={dismissInstallBanner}
            onDismiss={dismissInstallBanner}
          />
        </div>
      )}

      {/* Version Compatibility Warning - shown if backend/firmware version mismatch */}
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <VersionWarning />
      </div>

      {/* Navigation - Desktop: horizontal tabs, Mobile: bottom bar style */}
      <nav className="sticky top-16 z-40 nav-bg border-b border-theme">
        <div className="max-w-7xl mx-auto px-2 sm:px-6 lg:px-8">
          {/* Mobile: evenly distributed icons with labels */}
          <div className="flex sm:hidden justify-around py-1">
            {navigation.map((item) => (
              <NavLink
                key={item.name}
                to={item.href}
                end={
                  item.href === "/" ||
                  item.href.endsWith(`/${deviceId || selectedDevice?.id}`)
                }
                className={({ isActive }) =>
                  cn(
                    "flex flex-col items-center gap-0.5 px-3 py-2 rounded-xl text-xs font-medium transition-all min-w-0 flex-1 max-w-20",
                    isActive ? "nav-active" : "nav-inactive"
                  )
                }
              >
                <item.icon className="w-5 h-5" />
                <span className="truncate">{item.name}</span>
              </NavLink>
            ))}
          </div>
          {/* Desktop: horizontal tabs with full labels */}
          <div className="hidden sm:flex gap-1 py-2">
            {navigation.map((item) => (
              <NavLink
                key={item.name}
                to={item.href}
                end={
                  item.href === "/" ||
                  item.href.endsWith(`/${deviceId || selectedDevice?.id}`)
                }
                className={({ isActive }) =>
                  cn(
                    "flex items-center gap-2 px-4 py-2 rounded-xl text-sm font-medium whitespace-nowrap transition-all",
                    isActive ? "nav-active" : "nav-inactive"
                  )
                }
              >
                <item.icon className="w-4 h-4" />
                <span>{item.name}</span>
              </NavLink>
            ))}
          </div>
        </div>
      </nav>

      {/* Main Content */}
      <main className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-6">
        <Outlet />
      </main>

      {/* Connection Overlay - Only for local mode (not demo) */}
      {!isCloud && !isDemo && <ConnectionOverlay />}

      {/* Brewing Mode Overlay - Shows full-screen brewing UI during extraction */}
      <BrewingModeOverlay />
    </div>
  );
}
