import { useState, useEffect } from "react";
import { Input } from "@/components/Input";
import { Button } from "@/components/Button";
import { Logo } from "@/components/Logo";
import { Wifi, Loader2, Check, RefreshCw } from "lucide-react";
import { OnboardingLayout } from "@/components/onboarding";
import { useMobileLandscape } from "@/lib/useMobileLandscape";

export interface Network {
  ssid: string;
  rssi: number;
  secure: boolean;
}

export interface SetupViewProps {
  networks: Network[];
  scanning: boolean;
  selectedSsid: string;
  password: string;
  connecting: boolean;
  status: "idle" | "success" | "error";
  errorMessage: string;
  onScan: () => void;
  onSelectNetwork: (ssid: string) => void;
  onPasswordChange: (password: string) => void;
  onConnect: () => void;
}

/** Presentational component for WiFi Setup - used by both page and stories */
export function SetupView({
  networks,
  scanning,
  selectedSsid,
  password,
  connecting,
  status,
  errorMessage,
  onScan,
  onSelectNetwork,
  onPasswordChange,
  onConnect,
}: SetupViewProps) {
  const isMobileLandscape = useMobileLandscape();

  // Network list component (reused in both layouts)
  const NetworkListSection = ({ compact = false }: { compact?: boolean }) => (
    <div className={compact ? "" : "mb-3 xs:mb-4"}>
      <div className="flex items-center justify-between mb-1.5 xs:mb-2">
        <label className="text-xs font-semibold uppercase tracking-wider text-theme-muted">
          Available Networks
        </label>
        <Button variant="ghost" size="sm" onClick={onScan} disabled={scanning}>
          {scanning ? (
            <Loader2 className="w-4 h-4 animate-spin" />
          ) : (
            <RefreshCw className="w-4 h-4" />
          )}
        </Button>
      </div>

      <div
        className={`overflow-y-auto border border-theme rounded-xl ${
          compact ? "max-h-40" : "max-h-40 xs:max-h-48"
        }`}
      >
        {networks.length === 0 ? (
          <div className="p-3 xs:p-4 text-center text-theme-muted text-sm">
            {scanning ? "Scanning..." : "No networks found"}
          </div>
        ) : (
          networks.map((network) => (
            <button
              key={network.ssid}
              onClick={() => onSelectNetwork(network.ssid)}
              className={`w-full flex items-center justify-between p-3 border-b border-theme last:border-0 hover:bg-theme-secondary transition-colors ${
                selectedSsid === network.ssid ? "bg-accent/10" : ""
              }`}
            >
              <div className="flex items-center gap-3">
                <Wifi
                  className={`w-4 h-4 ${
                    network.rssi > -50
                      ? "text-emerald-500"
                      : network.rssi > -70
                      ? "text-amber-500"
                      : "text-red-500"
                  }`}
                />
                <span className="font-medium text-sm text-theme">
                  {network.ssid}
                </span>
              </div>
              {network.secure && (
                <span className="text-xs text-theme-muted">🔒</span>
              )}
            </button>
          ))
        )}
      </div>
    </div>
  );

  // Landscape layout
  if (isMobileLandscape) {
    if (status === "success") {
      return (
        <div className="flex gap-8 items-center w-full h-full">
          <div className="flex-1 flex flex-col items-center">
            <div className="w-20 h-20 bg-success-soft rounded-full flex items-center justify-center mb-3">
              <Check className="w-10 h-10 text-success" />
            </div>
          </div>
          <div className="flex-1 text-center">
            <h2 className="text-2xl font-bold text-theme mb-2">Connected!</h2>
            <p className="text-base text-theme-muted">
              Redirecting to{" "}
              <span className="font-mono text-accent">brewos.local</span>...
            </p>
            <div className="flex items-center justify-center gap-2 mt-4 text-theme-muted">
              <Loader2 className="w-4 h-4 animate-spin" />
              <span className="text-sm">Please wait...</span>
            </div>
          </div>
        </div>
      );
    }

    return (
      <div className="flex gap-8 items-center w-full h-full">
        <div className="flex-1 flex flex-col items-center text-center">
          <Wifi className="w-14 h-14 text-accent mb-3" />
          <h1 className="text-2xl font-bold text-theme">WiFi Setup</h1>
          <p className="text-base text-theme-muted mt-2 max-w-[220px]">
            Connect to your network
          </p>
        </div>

        <div className="flex-1 space-y-3">
          <NetworkListSection compact />

          {status === "error" && (
            <div className="p-2 bg-error-soft text-error rounded-lg text-xs">
              {errorMessage}
            </div>
          )}

          {/* Password row with label */}
          <div className="space-y-1.5">
            <label className="text-xs font-semibold uppercase tracking-wider text-theme-muted">
              Password
            </label>
            <div className="flex gap-2 items-center">
              <Input
                type="password"
                value={password}
                onChange={(e) => onPasswordChange(e.target.value)}
                placeholder={
                  selectedSsid ? "Enter password" : "Select a network first"
                }
                disabled={!selectedSsid}
                className="flex-1"
              />
              <Button
                onClick={onConnect}
                disabled={!selectedSsid || connecting}
                loading={connecting}
                className="flex-shrink-0"
              >
                {connecting ? "..." : "Connect"}
              </Button>
            </div>
          </div>
        </div>
      </div>
    );
  }

  // Portrait layout
  return (
    <div className="pb-3 xs:pb-6">
      <div className="text-center mb-5 xs:mb-6">
        <div className="flex justify-center mb-4">
          <Logo size="md" forceLight className="xs:hidden" />
          <Logo size="lg" className="hidden xs:flex" />
        </div>
        <h1 className="text-xl xs:text-2xl font-bold text-theme">WiFi Setup</h1>
        <p className="text-sm xs:text-base text-theme-muted mt-2">
          Connect your BrewOS to WiFi
        </p>
      </div>

      {status === "success" ? (
        <div className="text-center py-4 xs:py-8">
          <div className="w-14 h-14 xs:w-20 xs:h-20 bg-success-soft rounded-full flex items-center justify-center mx-auto mb-3 xs:mb-4">
            <Check className="w-7 h-7 xs:w-10 xs:h-10 text-success" />
          </div>
          <h2 className="text-lg xs:text-2xl font-bold text-theme mb-1 xs:mb-2">
            Connected!
          </h2>
          <p className="text-sm xs:text-base text-theme-muted">
            Redirecting to{" "}
            <span className="font-mono text-accent">brewos.local</span>...
          </p>
        </div>
      ) : (
        <>
          <div className="mb-4 xs:mb-5">
            <div className="flex items-center justify-between mb-2">
              <label className="text-xs font-semibold uppercase tracking-wider text-theme-muted">
                Available Networks
              </label>
              <Button
                variant="ghost"
                size="sm"
                onClick={onScan}
                disabled={scanning}
              >
                {scanning ? (
                  <Loader2 className="w-4 h-4 animate-spin" />
                ) : (
                  <RefreshCw className="w-4 h-4" />
                )}
              </Button>
            </div>

            <div className="max-h-40 xs:max-h-48 overflow-y-auto border border-white/10 xs:border-theme rounded-xl">
              {networks.length === 0 ? (
                <div className="p-4 text-center text-theme-muted text-sm">
                  {scanning ? "Scanning..." : "No networks found"}
                </div>
              ) : (
                networks.map((network) => (
                  <button
                    key={network.ssid}
                    onClick={() => onSelectNetwork(network.ssid)}
                    className={`w-full flex items-center justify-between p-3 border-b border-white/10 xs:border-theme last:border-0 hover:bg-white/5 xs:hover:bg-theme-secondary transition-colors ${
                      selectedSsid === network.ssid ? "bg-accent/10" : ""
                    }`}
                  >
                    <div className="flex items-center gap-3">
                      <Wifi
                        className={`w-4 h-4 ${
                          network.rssi > -50
                            ? "text-emerald-500"
                            : network.rssi > -70
                            ? "text-amber-500"
                            : "text-red-500"
                        }`}
                      />
                      <span className="font-medium text-sm text-theme">
                        {network.ssid}
                      </span>
                    </div>
                    {network.secure && (
                      <span className="text-xs text-theme-muted">🔒</span>
                    )}
                  </button>
                ))
              )}
            </div>
          </div>

          {selectedSsid && (
            <div className="mb-5 xs:mb-6">
              <Input
                label="Password"
                type="password"
                value={password}
                onChange={(e) => onPasswordChange(e.target.value)}
                placeholder="Enter WiFi password"
              />
            </div>
          )}

          {status === "error" && (
            <div className="mb-4 p-3 bg-error-soft text-error rounded-xl text-sm">
              {errorMessage}
            </div>
          )}

          <Button
            className="w-full"
            onClick={onConnect}
            disabled={!selectedSsid || connecting}
            loading={connecting}
          >
            {connecting ? "Connecting..." : "Connect"}
          </Button>
        </>
      )}
    </div>
  );
}

/** WiFi Setup page with state management */
export function Setup() {
  const [networks, setNetworks] = useState<Network[]>([]);
  const [scanning, setScanning] = useState(false);
  const [selectedSsid, setSelectedSsid] = useState("");
  const [password, setPassword] = useState("");
  const [connecting, setConnecting] = useState(false);
  const [status, setStatus] = useState<"idle" | "success" | "error">("idle");
  const [errorMessage, setErrorMessage] = useState("");

  const scanNetworks = async () => {
    setScanning(true);
    try {
      const response = await fetch("/api/wifi/networks");
      const data = await response.json();
      setNetworks(data.networks || []);
    } catch (err) {
      console.error("Failed to scan networks:", err);
    }
    setScanning(false);
  };

  useEffect(() => {
    scanNetworks();
  }, []);

  const connect = async () => {
    if (!selectedSsid) return;

    setConnecting(true);
    setStatus("idle");
    setErrorMessage("");

    try {
      const response = await fetch("/api/wifi/connect", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ ssid: selectedSsid, password }),
      });

      if (response.ok) {
        setStatus("success");
        setTimeout(() => {
          window.location.href = "http://brewos.local";
        }, 3000);
      } else {
        const data = await response.json();
        setStatus("error");
        setErrorMessage(data.error || "Connection failed");
      }
    } catch {
      setStatus("error");
      setErrorMessage("Failed to connect");
    }

    setConnecting(false);
  };

  return (
    <OnboardingLayout
      gradient="bg-gradient-to-br from-coffee-800 to-coffee-900"
      maxWidth="max-w-md"
    >
      <SetupView
        networks={networks}
        scanning={scanning}
        selectedSsid={selectedSsid}
        password={password}
        connecting={connecting}
        status={status}
        errorMessage={errorMessage}
        onScan={scanNetworks}
        onSelectNetwork={setSelectedSsid}
        onPasswordChange={setPassword}
        onConnect={connect}
      />
    </OnboardingLayout>
  );
}
