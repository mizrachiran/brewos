import { useState, useEffect, useRef } from "react";
import {
  Outlet,
  NavLink,
  useParams,
  Link,
  useLocation,
} from "react-router-dom";
import { useStore } from "@/lib/store";
import { useAppStore } from "@/lib/mode";
import { useMobileLandscape } from "@/lib/useMobileLandscape";
import { Logo } from "./Logo";
import { InstallPrompt, usePWAInstall } from "./InstallPrompt";
import { ConnectionOverlay } from "./ConnectionOverlay";
import { DeviceOfflineBanner } from "./DeviceOfflineBanner";
import { DeviceOfflineOverlay } from "./DeviceOfflineOverlay";
import { VersionWarning } from "./VersionWarning";
import { UserMenu } from "./UserMenu";
import { BrewingModeOverlay } from "./BrewingModeOverlay";
import { StatusBar } from "./StatusBar";
import { isDemoMode } from "@/lib/demo-mode";
import {
  LayoutGrid,
  Coffee,
  Settings,
  Calendar,
  BarChart3,
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
  const location = useLocation();
  const deviceName = useStore((s) => s.device.deviceName);
  const { mode, user, getSelectedDevice } = useAppStore();
  const { isMobile } = usePWAInstall();
  const isMobileLandscape = useMobileLandscape();
  const [showInstallBanner, setShowInstallBanner] = useState(() => {
    // Check if user previously dismissed the banner
    return localStorage.getItem("brewos-install-dismissed") !== "true";
  });

  const isCloud = mode === "cloud";
  const isDemo = isDemoMode();
  const selectedDevice = getSelectedDevice();

  // Scroll-aware header visibility (portrait mode only)
  const [headerVisible, setHeaderVisible] = useState(true);
  const lastScrollY = useRef(0);
  const scrollThreshold = 10; // Minimum scroll delta to trigger hide/show

  useEffect(() => {
    // Only apply scroll behavior in portrait mode (not landscape)
    if (isMobileLandscape) return;

    const handleScroll = () => {
      // Use #root as the scroll container (set in index.css)
      const scrollContainer = document.getElementById("root");
      if (!scrollContainer) return;

      const currentScrollY = scrollContainer.scrollTop;
      const delta = currentScrollY - lastScrollY.current;

      // Only trigger if scroll delta exceeds threshold
      if (Math.abs(delta) > scrollThreshold) {
        if (delta > 0 && currentScrollY > 80) {
          // Scrolling down & past header + nav height - hide
          setHeaderVisible(false);
        } else if (delta < 0) {
          // Scrolling up - show
          setHeaderVisible(true);
        }
        lastScrollY.current = currentScrollY;
      }
    };

    const scrollContainer = document.getElementById("root");
    if (scrollContainer) {
      scrollContainer.addEventListener("scroll", handleScroll, {
        passive: true,
      });
      return () => scrollContainer.removeEventListener("scroll", handleScroll);
    }
  }, [isMobileLandscape]);

  // Reset header visibility on route change
  useEffect(() => {
    setHeaderVisible(true);
    lastScrollY.current = 0;
  }, [location.pathname]);

  const dismissInstallBanner = () => {
    setShowInstallBanner(false);
    localStorage.setItem("brewos-install-dismissed", "true");
  };

  const navigation = getNavigation(isCloud, deviceId || selectedDevice?.id);

  // Check if device is offline (for hiding navigation)
  const isDeviceOffline = isCloud && selectedDevice && !selectedDevice.isOnline;

  // Get current page title from navigation
  const currentPageTitle =
    navigation.find((item) => {
      const currentPath = location.pathname;
      // Dashboard: exact match for root or machine root
      if (item.href === "/" || item.href === "") {
        return currentPath === "/" || currentPath === `/machine/${deviceId}`;
      }
      // Other routes: exact match or sub-route (path continues with /)
      return (
        currentPath === item.href || currentPath.startsWith(item.href + "/")
      );
    })?.name || "Dashboard";

  // Mobile Landscape Layout - Sidebar navigation
  if (isMobileLandscape) {
    return (
      <div className="h-[100dvh] bg-theme flex overflow-hidden">
        {/* Sidebar */}
        <aside className="w-16 flex-shrink-0 bg-card border-r border-theme flex flex-col">
          {/* Logo - icon only */}
          <div className="h-12 flex items-center justify-center border-b border-theme">
            <Logo size="sm" iconOnly />
          </div>

          {/* Navigation - hidden when device is offline */}
          {!isDeviceOffline && (
            <nav className="flex-1 flex flex-col items-center py-2 gap-1 overflow-y-auto">
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
                      "w-11 h-11 flex items-center justify-center rounded-xl transition-all",
                      isActive ? "nav-active" : "nav-inactive"
                    )
                  }
                  title={item.name}
                >
                  <item.icon className="w-5 h-5" />
                </NavLink>
              ))}
            </nav>
          )}
        </aside>

        {/* Main content area */}
        <div className="flex-1 flex flex-col min-w-0 overflow-hidden">
          {/* Compact header */}
          <header className="h-12 flex-shrink-0 header-glass border-b border-theme px-4 flex items-center justify-between">
            <div className="flex items-center gap-3 min-w-0">
              {/* Page title */}
              <h1 className="text-base font-bold text-theme">
                {currentPageTitle}
              </h1>

              {/* Cloud: machine name */}
              {isCloud && selectedDevice && (
                <Link
                  to="/machines"
                  className="flex items-center gap-2 px-2 py-1 rounded-lg bg-theme-tertiary hover:bg-theme-secondary transition-colors group"
                >
                  <div
                    className={`w-2 h-2 rounded-full flex-shrink-0 ${
                      selectedDevice.isOnline
                        ? "bg-emerald-500"
                        : "bg-theme-muted"
                    }`}
                  />
                  <span className="text-xs font-medium text-theme-secondary group-hover:text-theme truncate max-w-[100px]">
                    {selectedDevice.name}
                  </span>
                </Link>
              )}
              {/* Local: device name */}
              {!isCloud && deviceName && (
                <span className="text-xs text-theme-muted truncate">
                  {deviceName}
                </span>
              )}
            </div>

            {/* Right side */}
            <div className="flex items-center gap-2">
              {/* Machine Status Icons */}
              <StatusBar />

              {(isCloud && user) || isDemo ? (
                <UserMenu onExitDemo={onExitDemo} />
              ) : null}
            </div>
          </header>

          {/* Device Offline Banner */}
          {isCloud && selectedDevice && !selectedDevice.isOnline && (
            <DeviceOfflineBanner deviceName={selectedDevice.name} />
          )}

          {/* Main Content */}
          <main className="flex-1 overflow-y-auto px-4 py-3">
            <Outlet />
          </main>
        </div>

        {/* Connection Overlay - Only for local mode (not demo) */}
        {!isCloud && !isDemo && <ConnectionOverlay />}

        {/* Device Offline Overlay - Cloud mode when device is offline */}
        {isCloud && selectedDevice && !selectedDevice.isOnline && (
          <DeviceOfflineOverlay deviceName={selectedDevice.name} />
        )}

        {/* Brewing Mode Overlay */}
        <BrewingModeOverlay />
      </div>
    );
  }

  // Portrait / Desktop Layout
  return (
    <div className="full-page-scroll bg-theme">
      {/* Header - hides on scroll down (mobile only) */}
      <header
        className={cn(
          "sticky z-50 header-glass border-b border-theme transition-all duration-300",
          headerVisible ? "top-0" : "-top-16"
        )}
      >
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
              {/* Machine Status Icons */}
              <StatusBar />

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
      {/* Hidden when device is offline */}
      {!isDeviceOffline && (
        <nav
          className={cn(
            "sticky z-40 nav-bg border-b border-theme transition-all duration-300",
            headerVisible ? "top-16" : "top-0"
          )}
        >
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
      )}

      {/* Main Content */}
      <main className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-6">
        <Outlet />
      </main>

      {/* Connection Overlay - Only for local mode (not demo) */}
      {!isCloud && !isDemo && <ConnectionOverlay />}

      {/* Device Offline Overlay - Cloud mode when device is offline */}
      {isCloud && selectedDevice && !selectedDevice.isOnline && (
        <DeviceOfflineOverlay deviceName={selectedDevice.name} />
      )}

      {/* Brewing Mode Overlay - Shows full-screen brewing UI during extraction */}
      <BrewingModeOverlay />
    </div>
  );
}
