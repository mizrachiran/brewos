import { Outlet, NavLink, useNavigate, useParams } from 'react-router-dom';
import { useStore } from '@/lib/store';
import { useAppStore } from '@/lib/mode';
import { DeviceSelector } from './DeviceSelector';
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
  
  const isCloud = mode === 'cloud';
  const selectedDevice = getSelectedDevice();
  
  const isConnected = connectionState === 'connected';
  const isConnecting = connectionState === 'connecting' || connectionState === 'reconnecting';
  
  const navigation = getNavigation(isCloud, deviceId || selectedDevice?.id);

  const handleSignOut = async () => {
    await signOut();
    navigate('/login');
  };

  return (
    <div className="min-h-screen bg-cream-100">
      {/* Header */}
      <header className="sticky top-0 z-50 glass border-b border-cream-200">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="flex items-center justify-between h-16">
            {/* Logo & Mode */}
            <div className="flex items-center gap-4">
              <img 
                src="/logo.png" 
                alt="BrewOS" 
                className="h-8 w-auto"
              />
              
              {/* Cloud: Device Selector */}
              {isCloud && <DeviceSelector />}
              
              {/* Local: Machine Name */}
              {!isCloud && deviceName && (
                <span className="hidden sm:block text-sm font-medium text-coffee-600">
                  {deviceName}
                </span>
              )}
            </div>

            {/* Right side */}
            <div className="flex items-center gap-3">
              {/* Connection Status */}
              <div className="flex items-center gap-2 px-3 py-1.5 rounded-full bg-cream-200/80">
                {isCloud ? (
                  <>
                    <Cloud className="w-4 h-4 text-accent" />
                    <span className="text-xs font-medium text-coffee-700">Cloud</span>
                  </>
                ) : isConnected ? (
                  <>
                    <Wifi className="w-4 h-4 text-emerald-600" />
                    <span className="text-xs font-medium text-coffee-700">Local</span>
                  </>
                ) : isConnecting ? (
                  <>
                    <Wifi className="w-4 h-4 text-amber-500 animate-pulse" />
                    <span className="text-xs font-medium text-coffee-700">Connecting...</span>
                  </>
                ) : (
                  <>
                    <WifiOff className="w-4 h-4 text-red-500" />
                    <span className="text-xs font-medium text-coffee-700">Disconnected</span>
                  </>
                )}
              </div>

              {/* Cloud: User menu */}
              {isCloud && user && (
                <div className="flex items-center gap-3">
                  <button
                    onClick={() => navigate('/devices')}
                    className="p-2 rounded-lg hover:bg-cream-200 text-coffee-600"
                    title="Manage Devices"
                  >
                    <Home className="w-5 h-5" />
                  </button>
                  <div className="flex items-center gap-2 pl-2 border-l border-cream-300">
                    <span className="text-xs text-coffee-500 hidden sm:block max-w-32 truncate">
                      {user.email}
                    </span>
                    <button
                      onClick={handleSignOut}
                      className="flex items-center gap-1 px-2 py-1 rounded-lg hover:bg-cream-200 text-coffee-600 text-xs font-medium"
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

      {/* Navigation */}
      <nav className="sticky top-16 z-40 bg-white border-b border-cream-200">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="flex gap-1 py-2 overflow-x-auto scrollbar-hide">
            {navigation.map((item) => (
              <NavLink
                key={item.name}
                to={item.href}
                end={item.href === '/' || item.href.endsWith(`/${deviceId || selectedDevice?.id}`)}
                className={({ isActive }) =>
                  cn(
                    'flex items-center gap-2 px-4 py-2 rounded-xl text-sm font-medium whitespace-nowrap transition-all',
                    isActive
                      ? 'bg-coffee-800 text-white shadow-soft'
                      : 'text-coffee-600 hover:bg-cream-200'
                  )
                }
              >
                <item.icon className="w-4 h-4" />
                <span className="hidden sm:inline">{item.name}</span>
              </NavLink>
            ))}
          </div>
        </div>
      </nav>

      {/* Main Content */}
      <main className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-6">
        <Outlet />
      </main>
    </div>
  );
}

