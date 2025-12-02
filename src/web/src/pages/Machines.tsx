import { useEffect, useState } from "react";
import { useNavigate } from "react-router-dom";
import { Card, CardHeader, CardTitle } from "@/components/Card";
import { Button } from "@/components/Button";
import { Input } from "@/components/Input";
import { Logo } from "@/components/Logo";
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
} from "lucide-react";
import { UserMenu } from "@/components/UserMenu";

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

  const [showAddModal, setShowAddModal] = useState(false);
  const [claimCode, setClaimCode] = useState("");
  const [deviceName, setDeviceName] = useState("");
  const [claiming, setClaiming] = useState(false);
  const [claimError, setClaimError] = useState("");

  const [isEditMode, setIsEditMode] = useState(false);
  const [disconnectingDevice, setDisconnectingDevice] =
    useState<CloudDevice | null>(null);
  const [isRemoving, setIsRemoving] = useState(false);

  useEffect(() => {
    if (isDemo) return; // Skip auth check in demo mode

    if (!authLoading && !user) {
      navigate("/login");
    } else if (user) {
      fetchDevices();
    }
  }, [user, authLoading, navigate, fetchDevices, isDemo]);

  const handleClaim = async () => {
    if (!claimCode) return;

    setClaiming(true);
    setClaimError("");

    try {
      // Parse QR code URL or manual entry
      let deviceId = "";
      let token = "";
      let manualCode = "";

      if (claimCode.includes("?")) {
        // URL format
        const url = new URL(claimCode);
        deviceId = url.searchParams.get("id") || "";
        token = url.searchParams.get("token") || "";
      } else if (claimCode.includes(":")) {
        // Legacy format: DEVICE_ID:TOKEN
        const parts = claimCode.split(":");
        deviceId = parts[0];
        token = parts[1] || "";
      } else if (/^[A-Z0-9]{4}-[A-Z0-9]{4}$/i.test(claimCode.trim())) {
        // Short manual code format: X6ST-AP3G
        manualCode = claimCode.trim().toUpperCase();
      } else {
        setClaimError("Invalid code format");
        setClaiming(false);
        return;
      }

      // If we have a manual code, use it directly (backend will resolve it)
      if (manualCode) {
        const success = await claimDevice(
          manualCode,
          "",
          deviceName || undefined
        );
        if (success) {
          setShowAddModal(false);
          setClaimCode("");
          setDeviceName("");
        } else {
          setClaimError("Invalid or expired code");
        }
        setClaiming(false);
        return;
      }

      if (!deviceId || !token) {
        setClaimError("Invalid code format");
        setClaiming(false);
        return;
      }

      const success = await claimDevice(
        deviceId,
        token,
        deviceName || undefined
      );

      if (success) {
        setShowAddModal(false);
        setClaimCode("");
        setDeviceName("");
      } else {
        setClaimError("Failed to add machine. Code may be expired.");
      }
    } catch (error) {
      setClaimError("An error occurred");
    }

    setClaiming(false);
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
    return (
      <div className="min-h-screen bg-theme flex items-center justify-center">
        <Loader2 className="w-8 h-8 animate-spin text-accent" />
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-theme">
      {/* Header */}
      <header className="bg-theme-card border-b border-theme sticky top-0 z-50">
        <div className="max-w-4xl mx-auto px-4 py-4 flex items-center justify-between">
          <div className="flex items-center gap-3">
            <button
              onClick={() => navigate("/")}
              className="p-2 -ml-2 rounded-xl hover:bg-theme-secondary transition-colors text-theme-secondary"
              aria-label="Back to dashboard"
            >
              <ChevronLeft className="w-5 h-5" />
            </button>
            <Logo size="md" />
          </div>
          <UserMenu onExitDemo={isDemo ? handleLogout : undefined} />
        </div>
      </header>

      {/* Content */}
      <main className="max-w-4xl mx-auto px-4 py-8">
        <div className="flex items-center justify-between mb-6">
          <h1 className="text-2xl font-bold text-theme">My Machines</h1>
          <div className="flex items-center gap-2">
            {devices.length > 0 && (
              <Button
                variant={isEditMode ? "primary" : "ghost"}
                size="sm"
                onClick={() => setIsEditMode(!isEditMode)}
              >
                <Pencil className="w-4 h-4" />
                {isEditMode ? "Done" : "Edit"}
              </Button>
            )}
            {!isDemo && !isEditMode && (
              <Button onClick={() => setShowAddModal(true)}>
                <Plus className="w-4 h-4" />
                Add Machine
              </Button>
            )}
          </div>
        </div>

        {devicesLoading ? (
          <div className="flex justify-center py-12">
            <Loader2 className="w-8 h-8 animate-spin text-accent" />
          </div>
        ) : devices.length === 0 ? (
          <Card className="text-center py-12">
            <Coffee className="w-16 h-16 text-theme-muted mx-auto mb-4" />
            <h2 className="text-xl font-bold text-theme mb-2">
              No machines yet
            </h2>
            <p className="text-theme-secondary mb-6">
              Scan the QR code on your BrewOS display to add your first machine.
            </p>
            <Button onClick={() => setShowAddModal(true)}>
              <QrCode className="w-4 h-4" />
              Add Machine
            </Button>
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
                  className={`w-full text-left card flex items-center gap-3 transition-all ${
                    isEditMode
                      ? ""
                      : "cursor-pointer hover:border-theme-secondary hover:shadow-md"
                  } ${isSelected ? "border-accent/40 bg-accent/5" : ""}`}
                >
                  <div className="flex items-center gap-3 flex-1 min-w-0">
                    <div
                      className={`w-10 h-10 sm:w-12 sm:h-12 rounded-xl flex-shrink-0 flex items-center justify-center ${
                        device.isOnline
                          ? "bg-emerald-100 dark:bg-emerald-900/30"
                          : "bg-theme-secondary"
                      }`}
                    >
                      {device.isOnline ? (
                        <Wifi className="w-5 h-5 sm:w-6 sm:h-6 text-emerald-600 dark:text-emerald-400" />
                      ) : (
                        <WifiOff className="w-5 h-5 sm:w-6 sm:h-6 text-theme-muted" />
                      )}
                    </div>
                    <div className="min-w-0 flex-1">
                      <div className="flex items-center gap-2">
                        <h3 className="font-semibold text-theme truncate">
                          {device.name}
                        </h3>
                        {isSelected && (
                          <Check className="w-4 h-4 text-accent flex-shrink-0" />
                        )}
                      </div>
                      <div className="flex items-center gap-2 text-sm text-theme-muted">
                        <span
                          className={
                            device.isOnline
                              ? "text-emerald-600 dark:text-emerald-400"
                              : ""
                          }
                        >
                          {device.isOnline ? "Online" : "Offline"}
                        </span>
                        {device.firmwareVersion && (
                          <>
                            <span>·</span>
                            <span>v{device.firmwareVersion}</span>
                          </>
                        )}
                      </div>
                    </div>
                  </div>

                  {isEditMode && (
                    <button
                      onClick={(e) => {
                        e.stopPropagation();
                        setDisconnectingDevice(device);
                      }}
                      className="w-10 h-10 rounded-xl bg-red-100 dark:bg-red-900/30 hover:bg-red-200 dark:hover:bg-red-900/50 flex items-center justify-center text-red-600 dark:text-red-400 transition-colors flex-shrink-0"
                    >
                      <X className="w-5 h-5" />
                    </button>
                  )}
                </button>
              );
            })}
          </div>
        )}
      </main>

      {/* Add Machine Modal */}
      {showAddModal && (
        <div className="fixed inset-0 bg-black/50 flex items-center justify-center p-4 z-50">
          <Card className="w-full max-w-md">
            <CardHeader>
              <CardTitle>Add Machine</CardTitle>
              <Button
                variant="ghost"
                size="sm"
                onClick={() => setShowAddModal(false)}
              >
                <X className="w-4 h-4" />
              </Button>
            </CardHeader>

            <div className="space-y-4">
              <div className="bg-theme-secondary rounded-xl p-4 text-center">
                <QrCode className="w-12 h-12 text-accent mx-auto mb-2" />
                <p className="text-sm text-theme-secondary">
                  Scan the QR code on your BrewOS display, or enter the code
                  manually below.
                </p>
              </div>

              <Input
                label="Pairing Code"
                placeholder="X6ST-AP3G"
                value={claimCode}
                onChange={(e) => setClaimCode(e.target.value.toUpperCase())}
              />

              <Input
                label="Machine Name (optional)"
                placeholder="Kitchen Espresso"
                value={deviceName}
                onChange={(e) => setDeviceName(e.target.value)}
              />

              {claimError && (
                <p className="text-sm text-red-600">{claimError}</p>
              )}

              <div className="flex gap-3">
                <Button
                  variant="secondary"
                  className="flex-1"
                  onClick={() => setShowAddModal(false)}
                >
                  Cancel
                </Button>
                <Button
                  className="flex-1"
                  onClick={handleClaim}
                  loading={claiming}
                  disabled={!claimCode}
                >
                  Add Machine
                </Button>
              </div>
            </div>
          </Card>
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
              <div className="bg-red-50 dark:bg-red-950/20 rounded-xl p-4">
                <p className="text-sm text-red-800 dark:text-red-200">
                  Are you sure you want to remove{" "}
                  <strong>{disconnectingDevice.name}</strong> from your account?
                </p>
                <p className="text-xs text-red-600 dark:text-red-400 mt-2">
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
