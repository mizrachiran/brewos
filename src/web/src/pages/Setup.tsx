import { useState, useEffect } from "react";
import { Card } from "@/components/Card";
import { Input } from "@/components/Input";
import { Button } from "@/components/Button";
import { Logo } from "@/components/Logo";
import { Wifi, Loader2, Check, RefreshCw } from "lucide-react";
import { darkBgStyles } from "@/lib/darkBgStyles";

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
    <div className="py-4 sm:py-6">
      <div className="text-center mb-6">
        <div className="flex justify-center mb-4">
          {/* Mobile: force light text for dark background */}
          <Logo size="lg" forceLight className="sm:hidden" />
          {/* Desktop: use theme colors */}
          <Logo size="lg" className="hidden sm:flex" />
        </div>
        <h1 className="text-xl sm:text-2xl font-bold text-theme">WiFi Setup</h1>
        <p className="text-sm sm:text-base text-theme-muted mt-1">
          Connect your BrewOS to WiFi
        </p>
      </div>

      {status === "success" ? (
        <div className="text-center py-6 sm:py-8">
          <div className="w-14 h-14 sm:w-16 sm:h-16 bg-success-soft rounded-full flex items-center justify-center mx-auto mb-4">
            <Check className="w-7 h-7 sm:w-8 sm:h-8 text-success" />
          </div>
          <h2 className="text-lg sm:text-xl font-bold text-theme mb-2">
            Connected!
          </h2>
          <p className="text-sm sm:text-base text-theme-muted mb-4">
            Redirecting to{" "}
            <span className="font-mono text-accent">brewos.local</span>...
          </p>
        </div>
      ) : (
        <>
          {/* Network List */}
          <div className="mb-4">
            <div className="flex items-center justify-between mb-2">
              <label className="text-xs font-semibold uppercase tracking-wider text-theme-muted">
                Available Networks
              </label>
              <Button
                variant="ghost"
                size="sm"
                onClick={scanNetworks}
                disabled={scanning}
              >
                {scanning ? (
                  <Loader2 className="w-4 h-4 animate-spin" />
                ) : (
                  <RefreshCw className="w-4 h-4" />
                )}
              </Button>
            </div>

            <div className="max-h-48 overflow-y-auto border border-theme rounded-xl">
              {networks.length === 0 ? (
                <div className="p-4 text-center text-theme-muted text-sm">
                  {scanning ? "Scanning..." : "No networks found"}
                </div>
              ) : (
                networks.map((network) => (
                  <button
                    key={network.ssid}
                    onClick={() => setSelectedSsid(network.ssid)}
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

          {/* Password */}
          {selectedSsid && (
            <div className="mb-6">
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
            <div className="mb-4 p-3 bg-error-soft text-error rounded-xl text-xs sm:text-sm">
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
    <div className="full-page-scroll bg-gradient-to-br from-coffee-800 to-coffee-900 min-h-screen">
      {/* Narrow width (< 640px): Full-screen without card */}
      <div
        className="sm:hidden min-h-screen flex flex-col justify-center px-5 py-8"
        style={darkBgStyles}
      >
        <div className="animate-in fade-in slide-in-from-bottom-4 duration-500">
          {renderContent()}
        </div>
      </div>

      {/* Wide width (>= 640px): Card layout */}
      <div className="hidden sm:flex min-h-screen justify-center items-center p-4">
        <Card className="w-full max-w-md animate-in fade-in slide-in-from-bottom-4 duration-300">
          {renderContent()}
        </Card>
      </div>
    </div>
  );
}
