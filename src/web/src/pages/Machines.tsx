import { useEffect, useState } from "react";
import { useNavigate } from "react-router-dom";
import { Card } from "@/components/Card";
import { Button } from "@/components/Button";
import { Logo } from "@/components/Logo";
import { Loading } from "@/components/Loading";
import { DeviceClaimFlow } from "@/components/onboarding";
import { useAppStore } from "@/lib/mode";
import { isDemoMode, disableDemoMode } from "@/lib/demo-mode";
import type { CloudDevice } from "@/lib/types";
import {
  Coffee,
  Plus,
  Wifi,
  WifiOff,
  QrCode,
  X,
  Loader2,
  ChevronLeft,
  Check,
  Users,
  Share2,
} from "lucide-react";
import { UserMenu } from "@/components/UserMenu";
import { DeviceUsers } from "@/components/DeviceUsers";
import { ShareDevice } from "@/components/ShareDevice";

// Demo data
const DEMO_USER = {
  id: "demo-user",
  email: "demo@brewos.io",
  name: "Demo User",
  picture: null as string | null,
};

const DEMO_DEVICES: CloudDevice[] = [
  {
    id: "demo-device-001",
    name: "My ECM Synchronika",
    isOnline: true,
    firmwareVersion: "1.0.0-demo",
    lastSeen: new Date().toISOString(),
    machineType: "dual_boiler",
    claimedAt: new Date().toISOString(),
  },
];

export function Machines() {
  const navigate = useNavigate();
  const {
    user: realUser,
    signOut,
    authLoading,
    devices: realDevices,
    devicesLoading,
    fetchDevices,
    claimDevice,
    selectDevice,
    selectedDeviceId: realSelectedDeviceId,
  } = useAppStore();

  const isDemo = isDemoMode();
  const user = isDemo ? DEMO_USER : realUser;
  const devices = isDemo ? DEMO_DEVICES : realDevices;
  const selectedDeviceId = isDemo ? DEMO_DEVICES[0]?.id : realSelectedDeviceId;

  const [addMode, setAddMode] = useState<boolean>(false);
  const [deviceUsersOpen, setDeviceUsersOpen] = useState<{
    deviceId: string;
    deviceName: string;
  } | null>(null);
  const [shareDeviceOpen, setShareDeviceOpen] = useState<{
    deviceId: string;
    deviceName: string;
  } | null>(null);

  useEffect(() => {
    if (isDemo) return; // Skip auth check in demo mode

    if (!authLoading && !user) {
      navigate("/login");
    } else if (user) {
      fetchDevices();
    }
  }, [user, authLoading, navigate, fetchDevices, isDemo]);

  const handleClaim = async (
    deviceId: string,
    token: string,
    deviceName?: string
  ): Promise<boolean> => {
    const success = await claimDevice(deviceId, token, deviceName);
    if (success) {
      await fetchDevices();
    }
    return success;
  };

  const handleClaimManual = async (
    manualCode: string,
    deviceName?: string
  ): Promise<boolean> => {
    const success = await claimDevice(manualCode, "", deviceName);
    if (success) {
      await fetchDevices();
    }
    return success;
  };

  const handleLogout = async () => {
    if (isDemo) {
      disableDemoMode();
      window.location.href = "/login?exitDemo=true";
    } else {
      await signOut();
      navigate("/login");
    }
  };

  const handleConnect = (deviceId: string) => {
    if (isDemo) {
      // In demo mode, just go back to dashboard
      navigate("/");
    } else {
      selectDevice(deviceId);
      navigate(`/machine/${deviceId}`);
    }
  };

  if (authLoading && !isDemo) {
    return <Loading />;
  }

  return (
    <div className="full-page-scroll bg-theme">
      {/* Header */}
      <header className="bg-theme-card border-b border-theme sticky top-0 z-50">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-3 sm:py-4 flex items-center justify-between">
          <div className="flex items-center gap-2 sm:gap-3 min-w-0">
            <button
              onClick={() => navigate("/")}
              className="p-1.5 sm:p-2 -ml-1 sm:-ml-2 rounded-xl hover:bg-theme-secondary transition-colors text-theme-secondary flex-shrink-0"
              aria-label="Back to dashboard"
            >
              <ChevronLeft className="w-4 h-4 sm:w-5 sm:h-5" />
            </button>
            <Logo size="md" />
          </div>
          <UserMenu onExitDemo={isDemo ? handleLogout : undefined} />
        </div>
      </header>

      {/* Content */}
      <main className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-6">
        <div className="flex flex-col sm:flex-row sm:items-center sm:justify-between gap-4 mb-6 sm:mb-8">
          <div className="flex-1 min-w-0">
            <h1 className="text-2xl sm:text-3xl font-bold text-theme mb-1">
              My Machines
            </h1>
            <p className="text-sm sm:text-base text-theme-muted">
              {devices.length === 0
                ? "Add your first machine to get started"
                : `${devices.length} ${
                    devices.length === 1 ? "machine" : "machines"
                  } connected`}
            </p>
          </div>
          <div className="flex items-center gap-2 flex-shrink-0">
            {devices.length > 0 && (
              <Button
                onClick={() => setAddMode(true)}
                size="sm"
                className="text-xs sm:text-sm"
              >
                <Plus className="w-4 h-4" />
                <span className="hidden sm:inline">Add Machine</span>
                <span className="sm:hidden">Add</span>
              </Button>
            )}
          </div>
        </div>

        {devicesLoading ? (
          <div className="flex justify-center py-12">
            <Loader2 className="w-8 h-8 animate-spin text-accent" />
          </div>
        ) : devices.length === 0 ? (
          <Card className="text-center py-12 sm:py-16">
            <div className="max-w-md mx-auto px-4">
              <div className="w-16 h-16 sm:w-20 sm:h-20 bg-accent/10 rounded-2xl flex items-center justify-center mx-auto mb-4 sm:mb-6">
                <Coffee className="w-8 h-8 sm:w-10 sm:h-10 text-accent" />
              </div>
              <h2 className="text-xl sm:text-2xl font-bold text-theme mb-2 sm:mb-3">
                No machines yet
              </h2>
              <p className="text-theme-muted mb-6 sm:mb-8 text-base sm:text-lg leading-relaxed">
                Get started by adding your first espresso machine. Scan the QR
                code on your BrewOS display or enter the pairing code manually.
              </p>
              <Button
                size="lg"
                className="px-6 sm:px-8 py-5 sm:py-6 text-base sm:text-lg font-semibold w-full sm:w-auto"
                onClick={() => setAddMode(true)}
              >
                <QrCode className="w-5 h-5" />
                <span className="hidden sm:inline">Add Your First Machine</span>
                <span className="sm:hidden">Add Machine</span>
              </Button>
            </div>
          </Card>
        ) : (
          <div className="space-y-3">
            {devices.map((device) => {
              const isSelected = device.id === selectedDeviceId;

              return (
                <button
                  key={device.id}
                  onClick={() => handleConnect(device.id)}
                  className={`w-full text-left card flex items-center gap-2 sm:gap-3 transition-all p-3 sm:p-4 cursor-pointer hover:border-theme-secondary hover:shadow-md ${
                    isSelected ? "border-accent/40 bg-accent/5" : ""
                  }`}
                >
                  <div className="flex items-center gap-2 sm:gap-3 flex-1 min-w-0">
                    <div
                      className={`w-10 h-10 sm:w-12 sm:h-12 rounded-xl flex-shrink-0 flex items-center justify-center ${
                        device.isOnline
                          ? "bg-success-soft"
                          : "bg-theme-secondary"
                      }`}
                    >
                      {device.isOnline ? (
                        <Wifi className="w-5 h-5 sm:w-6 sm:h-6 text-success" />
                      ) : (
                        <WifiOff className="w-5 h-5 sm:w-6 sm:h-6 text-theme-muted" />
                      )}
                    </div>
                    <div className="min-w-0 flex-1">
                      <div className="flex items-center gap-2">
                        <h3 className="font-semibold text-sm sm:text-base text-theme truncate">
                          {device.name}
                        </h3>
                        {isSelected && (
                          <Check className="w-4 h-4 text-accent flex-shrink-0" />
                        )}
                      </div>
                      <div className="flex items-center gap-1.5 sm:gap-2 text-xs sm:text-sm text-theme-muted flex-wrap">
                        <span className={device.isOnline ? "text-success" : ""}>
                          {device.isOnline ? "Online" : "Offline"}
                        </span>
                        {device.firmwareVersion && (
                          <>
                            <span className="hidden sm:inline">·</span>
                            <span className="hidden sm:inline">
                              v{device.firmwareVersion}
                            </span>
                            <span className="sm:hidden text-xs">
                              v{device.firmwareVersion}
                            </span>
                          </>
                        )}
                      </div>
                    </div>
                  </div>

                  <div className="flex items-center gap-2 flex-shrink-0">
                    <button
                      onClick={(e) => {
                        e.stopPropagation();
                        setShareDeviceOpen({
                          deviceId: device.id,
                          deviceName: device.name,
                        });
                      }}
                      className="w-9 h-9 sm:w-10 sm:h-10 rounded-xl bg-accent/10 hover:bg-accent/20 flex items-center justify-center text-accent transition-colors"
                      title="Share device"
                    >
                      <Share2 className="w-4 h-4 sm:w-5 sm:h-5" />
                    </button>
                    <button
                      onClick={(e) => {
                        e.stopPropagation();
                        setDeviceUsersOpen({
                          deviceId: device.id,
                          deviceName: device.name,
                        });
                      }}
                      className="w-9 h-9 sm:w-10 sm:h-10 rounded-xl bg-theme-secondary hover:bg-theme-tertiary flex items-center justify-center text-theme-muted hover:text-theme transition-colors"
                      title="Manage users"
                    >
                      <Users className="w-4 h-4 sm:w-5 sm:h-5" />
                    </button>
                  </div>
                </button>
              );
            })}
          </div>
        )}
      </main>

      {/* Device Users Modal */}
      {deviceUsersOpen && (
        <DeviceUsers
          deviceId={deviceUsersOpen.deviceId}
          deviceName={deviceUsersOpen.deviceName}
          onClose={() => setDeviceUsersOpen(null)}
          onLeave={() => {
            setDeviceUsersOpen(null);
            fetchDevices();
          }}
        />
      )}

      {/* Share Device Modal */}
      {shareDeviceOpen && (
        <ShareDevice
          deviceId={shareDeviceOpen.deviceId}
          deviceName={shareDeviceOpen.deviceName}
          onClose={() => setShareDeviceOpen(null)}
        />
      )}

      {/* Add Machine Flow */}
      {addMode && (
        <div className="fixed inset-0 bg-black/50 xs:backdrop-blur-sm flex items-center justify-center xs:p-4 z-50 animate-in fade-in duration-200">
          <div className="w-full h-full xs:h-auto xs:max-w-lg xs:max-h-[90vh] overflow-y-auto">
            <Card className="animate-in zoom-in-95 duration-300 m-0 h-full xs:h-auto rounded-none xs:rounded-2xl">
              <div className="flex items-center justify-between mb-4 pb-4 border-b border-theme/10">
                <h2 className="text-xl sm:text-2xl font-bold text-theme">
                  Add Machine
                </h2>
                <Button
                  variant="ghost"
                  size="sm"
                  onClick={() => setAddMode(false)}
                  className="flex-shrink-0"
                >
                  <X className="w-4 h-4" />
                </Button>
              </div>

              <DeviceClaimFlow
                onClose={() => setAddMode(false)}
                onSuccess={() => {
                  setAddMode(false);
                }}
                onClaim={handleClaim}
                onClaimManual={handleClaimManual}
                showTabs={true}
              />
            </Card>
          </div>
        </div>
      )}
    </div>
  );
}
