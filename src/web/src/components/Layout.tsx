import { useState } from 'react';
import { Outlet, NavLink, useNavigate, useParams } from 'react-router-dom';
import { useStore } from '@/lib/store';
import { useAppStore } from '@/lib/mode';
import { DeviceSelector } from './DeviceSelector';
import { Logo } from './Logo';
import { InstallPrompt, usePWAInstall } from './InstallPrompt';
import { ConnectionOverlay } from './ConnectionOverlay';
import { VersionWarning } from './VersionWarning';
import { 
  LayoutGrid, 
  Coffee, 
  Settings,
  Calendar,
  BarChart3,
  Wifi,
  WifiOff,
  Cloud,
  LogOut,
  Home,
} from 'lucide-react';
import { cn } from '@/lib/utils';

// Navigation items - adjust based on mode
const getNavigation = (isCloud: boolean, deviceId?: string) => {
  const basePath = isCloud && deviceId ? `/device/${deviceId}` : '';
  
  const items = [
    { name: 'Dashboard', href: basePath || '/', icon: LayoutGrid },
    { name: 'Brewing', href: `${basePath}/brewing`, icon: Coffee },
    { name: 'Stats', href: `${basePath}/stats`, icon: BarChart3 },
    { name: 'Schedules', href: `${basePath}/schedules`, icon: Calendar },
    { name: 'Settings', href: `${basePath}/settings`, icon: Settings },
  ];
  
  return items;
};

export function Layout() {
  const navigate = useNavigate();
  const { deviceId } = useParams();
  const connectionState = useStore((s) => s.connectionState);
  const deviceName = useStore((s) => s.device.deviceName);
  const { mode, user, signOut, getSelectedDevice } = useAppStore();
  const { isMobile } = usePWAInstall();
  const [showInstallBanner, setShowInstallBanner] = useState(() => {
    // Check if user previously dismissed the banner
    return localStorage.getItem('brewos-install-dismissed') !== 'true';
  });
  
  const isCloud = mode === 'cloud';
  const selectedDevice = getSelectedDevice();
  
  const isConnected = connectionState === 'connected';
  const isConnecting = connectionState === 'connecting' || connectionState === 'reconnecting';
  
  const dismissInstallBanner = () => {
    setShowInstallBanner(false);
    localStorage.setItem('brewos-install-dismissed', 'true');
  };
  
  const navigation = getNavigation(isCloud, deviceId || selectedDevice?.id);

  const handleSignOut = async () => {
    await signOut();
    navigate('/login');
  };

  return (
    <div className="min-h-screen bg-theme">
      {/* Header */}
      <header className="sticky top-0 z-50 header-glass border-b border-theme">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="flex items-center justify-between h-16">
            {/* Logo & Mode */}
            <div className="flex items-center gap-4">
              <Logo size="md" />
              
              {/* Cloud: Device Selector */}
              {isCloud && <DeviceSelector />}
              
              {/* Local: Machine Name */}
              {!isCloud && deviceName && (
                <span className="hidden sm:block text-sm font-medium text-theme-secondary">
                  {deviceName}
                </span>
              )}
            </div>

            {/* Right side */}
            <div className="flex items-center gap-3">
              {/* Connection Status */}
              <div className="flex items-center gap-2 px-3 py-1.5 rounded-full bg-theme-tertiary">
                {isCloud ? (
                  <>
                    <Cloud className="w-4 h-4 text-accent" />
                    <span className="text-xs font-medium text-theme-secondary">Cloud</span>
                  </>
                ) : isConnected ? (
                  <>
                    <Wifi className="w-4 h-4 text-emerald-500" />
                    <span className="text-xs font-medium text-theme-secondary">Local</span>
                  </>
                ) : isConnecting ? (
                  <>
                    <Wifi className="w-4 h-4 text-amber-500 animate-pulse" />
                    <span className="text-xs font-medium text-theme-secondary">Connecting...</span>
                  </>
                ) : (
                  <>
                    <WifiOff className="w-4 h-4 text-red-500" />
                    <span className="text-xs font-medium text-theme-secondary">Disconnected</span>
                  </>
                )}
              </div>

              {/* Cloud: User menu */}
              {isCloud && user && (
                <div className="flex items-center gap-3">
                  <button
                    onClick={() => navigate('/devices')}
                    className="p-2 rounded-lg hover:bg-theme-tertiary text-theme-secondary transition-colors"
                    title="Manage Devices"
                  >
                    <Home className="w-5 h-5" />
                  </button>
                  <div className="flex items-center gap-2 pl-2 border-l border-theme">
                    <span className="text-xs text-theme-muted hidden sm:block max-w-32 truncate">
                      {user.email}
                    </span>
                    <button
                      onClick={handleSignOut}
                      className="flex items-center gap-1 px-2 py-1 rounded-lg hover:bg-theme-tertiary text-theme-secondary text-xs font-medium transition-colors"
                      title="Sign Out"
                    >
                      <LogOut className="w-4 h-4" />
                      <span className="hidden sm:inline">Logout</span>
                    </button>
                  </div>
                </div>
              )}
            </div>
          </div>
        </div>
      </header>

      {/* Install App Banner - shown to mobile users only */}
      {showInstallBanner && isMobile && (
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
                end={item.href === '/' || item.href.endsWith(`/${deviceId || selectedDevice?.id}`)}
                className={({ isActive }) =>
                  cn(
                    'flex flex-col items-center gap-0.5 px-3 py-2 rounded-xl text-xs font-medium transition-all min-w-0 flex-1 max-w-20',
                    isActive
                      ? 'nav-active'
                      : 'nav-inactive'
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
                end={item.href === '/' || item.href.endsWith(`/${deviceId || selectedDevice?.id}`)}
                className={({ isActive }) =>
                  cn(
                    'flex items-center gap-2 px-4 py-2 rounded-xl text-sm font-medium whitespace-nowrap transition-all',
                    isActive
                      ? 'nav-active'
                      : 'nav-inactive'
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
      
      {/* Connection Overlay - Only for local mode */}
      {!isCloud && <ConnectionOverlay />}
    </div>
  );
}
