import { useEffect, useState } from "react";
import { useNavigate } from "react-router-dom";
import { Card, CardHeader, CardTitle } from "@/components/Card";
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
  Pencil,
  Wifi,
  WifiOff,
  QrCode,
  X,
  Loader2,
  ChevronLeft,
  AlertTriangle,
  Check,
  Users,
} from "lucide-react";
import { UserMenu } from "@/components/UserMenu";
import { DeviceUsers } from "@/components/DeviceUsers";

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
    removeDevice,
    selectDevice,
    selectedDeviceId: realSelectedDeviceId,
  } = useAppStore();

  const isDemo = isDemoMode();
  const user = isDemo ? DEMO_USER : realUser;
  const devices = isDemo ? DEMO_DEVICES : realDevices;
  const selectedDeviceId = isDemo ? DEMO_DEVICES[0]?.id : realSelectedDeviceId;

  const [addMode, setAddMode] = useState<boolean>(false);

  const [isEditMode, setIsEditMode] = useState(false);
  const [disconnectingDevice, setDisconnectingDevice] =
    useState<CloudDevice | null>(null);
  const [isRemoving, setIsRemoving] = useState(false);
  const [deviceUsersOpen, setDeviceUsersOpen] = useState<{
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

  const handleRemove = async () => {
    if (!disconnectingDevice) return;

    // In demo mode, just show the flow but don't actually remove
    if (isDemo) {
      setIsRemoving(true);
      // Simulate a brief delay
      await new Promise((r) => setTimeout(r, 500));
      setIsRemoving(false);
      setDisconnectingDevice(null);
      // In demo, navigate back to dashboard to show the flow
      navigate("/");
      return;
    }

    const wasLastDevice = devices.length === 1;

    setIsRemoving(true);
    const success = await removeDevice(disconnectingDevice.id);
    setIsRemoving(false);
    setDisconnectingDevice(null);

    if (success) {
      if (wasLastDevice) {
        // No machines left, go to onboarding
        navigate("/onboarding");
      }
      // If there are remaining machines, the store will auto-select the next one
    }
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
        <div className="max-w-4xl mx-auto px-3 sm:px-4 py-3 sm:py-4 flex items-center justify-between">
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
      <main className="max-w-4xl mx-auto px-4 sm:px-6 py-6 sm:py-8">
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
                variant={isEditMode ? "primary" : "ghost"}
                size="sm"
                onClick={() => setIsEditMode(!isEditMode)}
                className="text-xs sm:text-sm"
              >
                <Pencil className="w-4 h-4" />
                <span className="hidden sm:inline">
                  {isEditMode ? "Done" : "Edit"}
                </span>
              </Button>
            )}
            {!isEditMode && devices.length > 0 && (
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
                  onClick={() => !isEditMode && handleConnect(device.id)}
                  disabled={isEditMode}
                  className={`w-full text-left card flex items-center gap-2 sm:gap-3 transition-all p-3 sm:p-4 ${
                    isEditMode
                      ? ""
                      : "cursor-pointer hover:border-theme-secondary hover:shadow-md"
                  } ${isSelected ? "border-accent/40 bg-accent/5" : ""}`}
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
                    {!isEditMode && (
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
                    )}
                    {isEditMode && (
                      <button
                        onClick={(e) => {
                          e.stopPropagation();
                          setDisconnectingDevice(device);
                        }}
                        className="w-9 h-9 sm:w-10 sm:h-10 rounded-xl bg-error-soft hover:bg-red-200 dark:hover:bg-red-900/50 flex items-center justify-center text-error transition-colors"
                      >
                        <X className="w-4 h-4 sm:w-5 sm:h-5" />
                      </button>
                    )}
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

      {/* Add Machine Flow */}
      {addMode && (
        <div className="fixed inset-0 bg-black/50 backdrop-blur-sm flex items-center justify-center p-2 sm:p-4 z-50 animate-in fade-in duration-200">
          <div className="w-full max-w-lg max-h-[95vh] sm:max-h-[90vh] overflow-y-auto">
            <Card className="animate-in zoom-in-95 duration-300 m-0">
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

      {/* Remove Machine Modal */}
      {disconnectingDevice && (
        <div className="fixed inset-0 bg-black/50 flex items-center justify-center p-4 z-50">
          <Card className="w-full max-w-md">
            <CardHeader>
              <div className="flex items-center gap-2 text-red-600">
                <AlertTriangle className="w-5 h-5" />
                <CardTitle>Remove Machine</CardTitle>
              </div>
            </CardHeader>

            <div className="space-y-4">
              <div className="bg-error-soft rounded-xl p-4">
                <p className="text-sm text-error">
                  Are you sure you want to remove{" "}
                  <strong>{disconnectingDevice.name}</strong> from your account?
                </p>
                <p className="text-xs text-error mt-2 opacity-80">
                  You will no longer be able to control this machine remotely.
                  You can re-add it later by scanning the QR code on your
                  machine.
                </p>
              </div>

              <div className="flex gap-3">
                <Button
                  variant="secondary"
                  className="flex-1"
                  onClick={() => setDisconnectingDevice(null)}
                  disabled={isRemoving}
                >
                  Cancel
                </Button>
                <Button
                  className="flex-1 bg-red-600 hover:bg-red-700 text-white"
                  onClick={handleRemove}
                  loading={isRemoving}
                >
                  Remove
                </Button>
              </div>
            </div>
          </Card>
        </div>
      )}
    </div>
  );
}
