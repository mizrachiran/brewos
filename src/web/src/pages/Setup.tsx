import { useState, useEffect } from "react";
import { Input } from "@/components/Input";
import { Button } from "@/components/Button";
import { Logo } from "@/components/Logo";
import { Wifi, Loader2, Check, RefreshCw } from "lucide-react";
import { OnboardingLayout } from "@/components/onboarding";

interface Network {
  ssid: string;
  rssi: number;
  secure: boolean;
}

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
        // Redirect after connection
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

  const renderContent = () => (
    <div className="py-2 xs:py-6">
      <div className="text-center mb-4 xs:mb-6">
        <div className="flex justify-center mb-3 xs:mb-4">
          {/* Mobile: force light text for dark background */}
          <Logo size="md" forceLight className="xs:hidden" />
          {/* Desktop: use theme colors */}
          <Logo size="lg" className="hidden xs:flex" />
        </div>
        <h1 className="text-lg xs:text-2xl font-bold text-theme">WiFi Setup</h1>
        <p className="text-xs xs:text-base text-theme-muted mt-1">
          Connect your BrewOS to WiFi
        </p>
      </div>

      {status === "success" ? (
        <div className="text-center py-4 xs:py-8">
          <div className="w-12 h-12 xs:w-16 xs:h-16 bg-success-soft rounded-full flex items-center justify-center mx-auto mb-3 xs:mb-4">
            <Check className="w-6 h-6 xs:w-8 xs:h-8 text-success" />
          </div>
          <h2 className="text-base xs:text-xl font-bold text-theme mb-1 xs:mb-2">
            Connected!
          </h2>
          <p className="text-xs xs:text-base text-theme-muted mb-3 xs:mb-4">
            Redirecting to{" "}
            <span className="font-mono text-accent">brewos.local</span>...
          </p>
        </div>
      ) : (
        <>
          {/* Network List */}
          <div className="mb-3 xs:mb-4">
            <div className="flex items-center justify-between mb-1.5 xs:mb-2">
              <label className="text-[10px] xs:text-xs font-semibold uppercase tracking-wider text-theme-muted">
                Available Networks
              </label>
              <Button
                variant="ghost"
                size="sm"
                onClick={scanNetworks}
                disabled={scanning}
              >
                {scanning ? (
                  <Loader2 className="w-3.5 h-3.5 xs:w-4 xs:h-4 animate-spin" />
                ) : (
                  <RefreshCw className="w-3.5 h-3.5 xs:w-4 xs:h-4" />
                )}
              </Button>
            </div>

            <div className="max-h-36 xs:max-h-48 overflow-y-auto border border-white/10 xs:border-theme rounded-lg xs:rounded-xl">
              {networks.length === 0 ? (
                <div className="p-3 xs:p-4 text-center text-theme-muted text-xs xs:text-sm">
                  {scanning ? "Scanning..." : "No networks found"}
                </div>
              ) : (
                networks.map((network) => (
                  <button
                    key={network.ssid}
                    onClick={() => setSelectedSsid(network.ssid)}
                    className={`w-full flex items-center justify-between p-2.5 xs:p-3 border-b border-white/10 xs:border-theme last:border-0 hover:bg-white/5 xs:hover:bg-theme-secondary transition-colors ${
                      selectedSsid === network.ssid ? "bg-accent/10" : ""
                    }`}
                  >
                    <div className="flex items-center gap-2 xs:gap-3">
                      <Wifi
                        className={`w-3.5 h-3.5 xs:w-4 xs:h-4 ${
                          network.rssi > -50
                            ? "text-emerald-500"
                            : network.rssi > -70
                            ? "text-amber-500"
                            : "text-red-500"
                        }`}
                      />
                      <span className="font-medium text-xs xs:text-sm text-theme">
                        {network.ssid}
                      </span>
                    </div>
                    {network.secure && (
                      <span className="text-[10px] xs:text-xs text-theme-muted">
                        🔒
                      </span>
                    )}
                  </button>
                ))
              )}
            </div>
          </div>

          {/* Password */}
          {selectedSsid && (
            <div className="mb-4 xs:mb-6">
              <Input
                label="Password"
                type="password"
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                placeholder="Enter WiFi password"
              />
            </div>
          )}

          {/* Error */}
          {status === "error" && (
            <div className="mb-3 xs:mb-4 p-2.5 xs:p-3 bg-error-soft text-error rounded-lg xs:rounded-xl text-[10px] xs:text-sm">
              {errorMessage}
            </div>
          )}

          {/* Connect Button */}
          <Button
            className="w-full"
            onClick={connect}
            disabled={!selectedSsid || connecting}
            loading={connecting}
          >
            {connecting ? "Connecting..." : "Connect"}
          </Button>
        </>
      )}
    </div>
  );

  return (
    <OnboardingLayout
      gradient="bg-gradient-to-br from-coffee-800 to-coffee-900"
      maxWidth="max-w-md"
      desktopTopPadding="pt-0"
    >
      {renderContent()}
    </OnboardingLayout>
  );
}
