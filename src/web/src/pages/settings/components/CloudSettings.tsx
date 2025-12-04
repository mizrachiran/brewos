import { useState, useEffect, useCallback } from "react";
import { useCommand } from "@/lib/useCommand";
import { Card } from "@/components/Card";
import { Input } from "@/components/Input";
import { Button } from "@/components/Button";
import { Toggle } from "@/components/Toggle";
import { Badge } from "@/components/Badge";
import {
  QrCode,
  Copy,
  Cloud,
  Wifi,
  Globe,
  Shield,
  RefreshCw,
  Check,
  X,
  Loader2,
} from "lucide-react";
import { QRCodeSVG } from "qrcode.react";
import { isDemoMode } from "@/lib/demo-mode";
import { useDevMode } from "@/lib/dev-mode";

interface PairingData {
  deviceId: string;
  token: string;
  url: string;
  expiresIn: number;
  manualCode?: string; // Short code like "X6ST-AP3G" for manual entry
}

interface CloudStatus {
  enabled: boolean;
  connected: boolean;
  serverUrl: string;
}

// Cloud environment presets
type CloudEnvironment = "production" | "staging" | "custom";

const CLOUD_ENVIRONMENTS: Record<
  CloudEnvironment,
  { label: string; url: string; description: string; devOnly?: boolean }
> = {
  production: {
    label: "Production",
    url: "wss://cloud.brewos.io",
    description: "BrewOS Cloud service",
  },
  staging: {
    label: "Staging",
    url: "wss://staging.brewos.io",
    description: "Test latest changes before release",
    devOnly: true,
  },
  custom: {
    label: "Custom",
    url: "",
    description: "Enter a custom server URL",
  },
};

// Detect which environment a URL matches
function detectEnvironment(url: string): CloudEnvironment {
  if (url === CLOUD_ENVIRONMENTS.production.url) return "production";
  if (url === CLOUD_ENVIRONMENTS.staging.url) return "staging";
  return "custom";
}

// Demo mode mock data
const DEMO_PAIRING: PairingData = {
  deviceId: "DEMO-ECM-001",
  token: "demo-token-abc123",
  url: "https://cloud.brewos.io/pair?device=DEMO-ECM-001&token=demo-token-abc123",
  expiresIn: 300,
  manualCode: "X6ST-AP3G",
};

const DEMO_CLOUD_STATUS: CloudStatus = {
  enabled: true,
  connected: true,
  serverUrl: "wss://cloud.brewos.io",
};

export function CloudSettings() {
  const isDemo = isDemoMode();
  const devMode = useDevMode();
  const { sendCommand } = useCommand();
  const [cloudConfig, setCloudConfig] = useState<CloudStatus | null>(
    isDemo ? DEMO_CLOUD_STATUS : null
  );
  const [pairing, setPairing] = useState<PairingData | null>(
    isDemo ? DEMO_PAIRING : null
  );
  const [loadingQR, setLoadingQR] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [copied, setCopied] = useState(false);
  const [copiedCode, setCopiedCode] = useState(false);

  const [cloudEnabled, setCloudEnabled] = useState(
    isDemo ? true : cloudConfig?.enabled || false
  );
  const [cloudUrl, setCloudUrl] = useState(
    cloudConfig?.serverUrl || "wss://cloud.brewos.io"
  );
  const [selectedEnv, setSelectedEnv] = useState<CloudEnvironment>(() =>
    detectEnvironment(cloudConfig?.serverUrl || "wss://cloud.brewos.io")
  );
  const [saving, setSaving] = useState(false);
  const [timeLeft, setTimeLeft] = useState(pairing?.expiresIn || 0);

  // Countdown timer for pairing expiration
  useEffect(() => {
    if (pairing?.expiresIn !== undefined) {
      setTimeLeft(pairing.expiresIn);
    }
  }, [pairing?.expiresIn]);

  useEffect(() => {
    if (timeLeft <= 0) return;

    const interval = setInterval(() => {
      setTimeLeft((prev) => Math.max(0, prev - 1));
    }, 1000);

    return () => clearInterval(interval);
  }, [timeLeft]);

  // Handle environment preset selection
  const handleEnvChange = (env: CloudEnvironment) => {
    setSelectedEnv(env);
    if (env !== "custom") {
      setCloudUrl(CLOUD_ENVIRONMENTS[env].url);
    }
  };

  const fetchPairingQR = useCallback(async () => {
    // Demo mode: use mock data
    if (isDemo) {
      setPairing({ ...DEMO_PAIRING, expiresIn: 300 });
      return;
    }

    setLoadingQR(true);
    setError(null);
    try {
      const response = await fetch("/api/pairing/qr");
      if (!response.ok) throw new Error("Failed to generate pairing code");
      const data = await response.json();
      setPairing(data);
    } catch {
      setError(
        "Failed to generate pairing code. Make sure you're connected to your machine."
      );
    } finally {
      setLoadingQR(false);
    }
  }, [isDemo]);

  const refreshPairing = async () => {
    // Demo mode: simulate refresh with new expiry
    if (isDemo) {
      setLoadingQR(true);
      await new Promise((r) => setTimeout(r, 500));
      setPairing({ ...DEMO_PAIRING, expiresIn: 300 });
      setLoadingQR(false);
      return;
    }

    setLoadingQR(true);
    setError(null);
    try {
      const response = await fetch("/api/pairing/refresh", { method: "POST" });
      if (!response.ok) throw new Error("Failed to refresh pairing code");
      const data = await response.json();
      setPairing(data);
    } catch {
      setError("Failed to refresh pairing code.");
    } finally {
      setLoadingQR(false);
    }
  };

  const copyPairingUrl = () => {
    if (pairing?.url) {
      navigator.clipboard.writeText(pairing.url);
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    }
  };

  const copyPairingCode = () => {
    if (pairing) {
      const code = pairing.manualCode || `${pairing.deviceId}:${pairing.token}`;
      navigator.clipboard.writeText(code);
      setCopiedCode(true);
      setTimeout(() => setCopiedCode(false), 2000);
    }
  };

  const saveSettings = () => {
    if (saving) return; // Prevent double-click
    setSaving(true);

    // Demo mode: just update local state
    if (isDemo) {
      setCloudConfig({
        enabled: cloudEnabled,
        connected: cloudEnabled,
        serverUrl: cloudUrl,
      });
      setTimeout(() => setSaving(false), 600);
      return;
    }

    const success = sendCommand(
      "set_cloud_config",
      {
        enabled: cloudEnabled,
        serverUrl: cloudUrl,
      },
      { successMessage: "Cloud settings saved" }
    );

    if (success) {
      setCloudConfig({
        enabled: cloudEnabled,
        connected: cloudConfig?.connected || false,
        serverUrl: cloudUrl,
      });
    }

    // Brief visual feedback for fire-and-forget WebSocket command
    setTimeout(() => setSaving(false), 600);
  };

  const fetchCloudStatus = useCallback(async () => {
    // Demo mode: use mock data
    if (isDemo) {
      setCloudConfig(DEMO_CLOUD_STATUS);
      setCloudEnabled(true);
      setCloudUrl(DEMO_CLOUD_STATUS.serverUrl);
      setSelectedEnv(detectEnvironment(DEMO_CLOUD_STATUS.serverUrl));
      return;
    }

    try {
      const response = await fetch("/api/cloud/status");
      if (response.ok) {
        const data = await response.json();
        setCloudConfig(data);
        setCloudEnabled(data.enabled);
        const url = data.serverUrl || "wss://cloud.brewos.io";
        setCloudUrl(url);
        setSelectedEnv(detectEnvironment(url));
      }
    } catch {
      // Device might not support cloud status endpoint yet
    }
  }, [isDemo]);

  useEffect(() => {
    // Skip API calls in demo mode - data already initialized
    if (isDemo) return;

    fetchPairingQR();
    fetchCloudStatus();
  }, [isDemo, fetchPairingQR, fetchCloudStatus]);

  const isExpired = pairing !== null && timeLeft <= 0;

  return (
    <div className="space-y-6">
      <div className="grid gap-6 md:grid-cols-2 items-stretch">
        {/* Pairing QR Code */}
        <Card className="h-full">
          <div className="flex items-center gap-3 mb-4">
            <div className="p-2 bg-accent/10 rounded-lg">
              <QrCode className="w-5 h-5 text-accent" />
            </div>
            <div>
              <h2 className="font-semibold text-theme">Pair with Cloud</h2>
              <p className="text-sm text-theme-muted">
                Scan to add to your account
              </p>
            </div>
          </div>

          <div className="bg-white rounded-xl p-6 border border-theme flex flex-col items-center">
            {loadingQR ? (
              <div className="w-48 h-48 flex items-center justify-center">
                <Loader2 className="w-8 h-8 animate-spin text-accent" />
              </div>
            ) : error ? (
              <div className="w-48 h-48 flex flex-col items-center justify-center text-center">
                <X className="w-8 h-8 text-red-500 mb-2" />
                <p className="text-sm text-theme-muted">{error}</p>
                <Button
                  variant="secondary"
                  size="sm"
                  onClick={fetchPairingQR}
                  className="mt-3"
                >
                  Retry
                </Button>
              </div>
            ) : pairing ? (
              <>
                <div
                  className={`p-3 bg-white rounded-xl ${
                    isExpired ? "opacity-50" : ""
                  }`}
                >
                  <QRCodeSVG
                    value={pairing.url}
                    size={180}
                    level="M"
                    includeMargin={false}
                  />
                </div>
                {isExpired ? (
                  <Badge variant="warning" className="mt-3">
                    Code expired
                  </Badge>
                ) : (
                  <p className="text-xs text-theme-muted mt-2">
                    Expires in {Math.floor(timeLeft / 60)}m {timeLeft % 60}s
                  </p>
                )}
              </>
            ) : null}
          </div>

          <div className="mt-4">
            <Button
              variant="secondary"
              className="w-full"
              onClick={refreshPairing}
              disabled={loadingQR}
            >
              <RefreshCw
                className={`w-4 h-4 mr-2 ${loadingQR ? "animate-spin" : ""}`}
              />
              New Code
            </Button>
          </div>

          {pairing && (
            <div className="mt-4 pt-4 border-t border-theme space-y-4">
              <div>
                <h3 className="text-sm font-medium text-theme mb-2">
                  Pairing Link
                </h3>
                <div className="flex items-center gap-2">
                  <code className="flex-1 bg-theme-secondary px-3 py-2.5 rounded-lg text-xs font-mono text-theme truncate">
                    {pairing.url}
                  </code>
                  <Button
                    variant="secondary"
                    size="sm"
                    onClick={copyPairingUrl}
                  >
                    {copied ? (
                      <Check className="w-4 h-4" />
                    ) : (
                      <Copy className="w-4 h-4" />
                    )}
                  </Button>
                </div>
                <p className="text-xs text-theme-muted mt-2">
                  Share this link to let others add your machine
                </p>
              </div>

              <div>
                <h3 className="text-sm font-medium text-theme mb-2">
                  Manual Pairing Code
                </h3>
                <div className="flex items-center gap-2">
                  <code className="flex-1 bg-theme-secondary px-4 py-3 rounded-lg text-lg font-mono text-theme tracking-wider text-center">
                    {pairing.manualCode ||
                      `${pairing.deviceId}:${pairing.token.substring(0, 8)}...`}
                  </code>
                  <Button
                    variant="secondary"
                    size="sm"
                    onClick={copyPairingCode}
                  >
                    {copiedCode ? (
                      <Check className="w-4 h-4" />
                    ) : (
                      <Copy className="w-4 h-4" />
                    )}
                  </Button>
                </div>
                <p className="text-xs text-theme-muted mt-2">
                  Enter this code at cloud.brewos.io if you can't scan the QR
                </p>
              </div>
            </div>
          )}
        </Card>

        {/* Cloud Status & Settings */}
        <div className="flex flex-col gap-6 h-full">
          <Card className="flex-1">
            <div className="flex items-center gap-3 mb-4">
              <div className="p-2 bg-accent/10 rounded-lg">
                <Cloud className="w-5 h-5 text-accent" />
              </div>
              <div>
                <h2 className="font-semibold text-theme">Cloud Status</h2>
                <p className="text-sm text-theme-muted">
                  Connection to BrewOS Cloud
                </p>
              </div>
            </div>

            <div className="space-y-3">
              <div className="flex items-center justify-between py-2">
                <div className="flex items-center gap-2">
                  <Wifi className="w-4 h-4 text-theme-muted" />
                  <span className="text-sm text-theme-secondary">
                    Cloud Connection
                  </span>
                </div>
                <Badge variant={cloudConfig?.connected ? "success" : "default"}>
                  {cloudConfig?.connected ? "Connected" : "Disconnected"}
                </Badge>
              </div>
              <div className="flex items-center justify-between py-2">
                <div className="flex items-center gap-2">
                  <Globe className="w-4 h-4 text-theme-muted" />
                  <span className="text-sm text-theme-secondary">Server</span>
                </div>
                <span className="text-sm text-theme-muted font-mono">
                  {cloudConfig?.serverUrl || "Not configured"}
                </span>
              </div>
              <div className="flex items-center justify-between py-2">
                <div className="flex items-center gap-2">
                  <Shield className="w-4 h-4 text-theme-muted" />
                  <span className="text-sm text-theme-secondary">
                    Machine ID
                  </span>
                </div>
                <span className="text-sm text-theme-muted font-mono">
                  {pairing?.deviceId || "—"}
                </span>
              </div>
            </div>
          </Card>

          <Card className="flex-1 flex flex-col">
            <div className="flex items-center gap-3 mb-4">
              <div className="p-2 bg-theme-secondary rounded-lg">
                <Cloud className="w-5 h-5 text-theme-secondary" />
              </div>
              <div>
                <h2 className="font-semibold text-theme">Cloud Settings</h2>
                <p className="text-sm text-theme-muted">
                  Configure cloud connection
                </p>
              </div>
            </div>

            <div className="space-y-4 flex-1 flex flex-col">
              <div>
                <Toggle
                  label="Enable Cloud Connection"
                  checked={cloudEnabled}
                  onChange={setCloudEnabled}
                />
                <p className="text-xs text-theme-muted mt-1 ml-14">
                  Allow remote access via BrewOS Cloud
                </p>
              </div>

              {/* Environment Selector */}
              <div>
                <label className="block text-xs font-semibold uppercase tracking-wider text-theme-muted mb-1.5">
                  Environment{" "}
                  {devMode && <span className="text-purple-400">(Dev)</span>}
                </label>
                <div className="flex gap-2">
                  {(Object.keys(CLOUD_ENVIRONMENTS) as CloudEnvironment[])
                    .filter(
                      (env) => !CLOUD_ENVIRONMENTS[env].devOnly || devMode
                    )
                    .map((env) => (
                      <button
                        key={env}
                        onClick={() => handleEnvChange(env)}
                        disabled={!cloudEnabled}
                        className={`flex-1 px-4 py-2.5 rounded-xl text-sm font-medium transition-all border-2 ${
                          selectedEnv === env
                            ? env === "staging"
                              ? "border-purple-500 bg-purple-500/15 text-purple-600 dark:text-purple-400"
                              : "border-amber-500 bg-amber-500/15 text-amber-600 dark:text-amber-400"
                            : "border-transparent bg-theme-secondary text-theme-muted hover:bg-theme-tertiary hover:text-theme"
                        } ${
                          !cloudEnabled ? "opacity-50 cursor-not-allowed" : ""
                        }`}
                      >
                        {CLOUD_ENVIRONMENTS[env].label}
                      </button>
                    ))}
                </div>
                <p className="text-xs text-theme-muted mt-1">
                  {CLOUD_ENVIRONMENTS[selectedEnv].description}
                </p>
              </div>

              {/* Server URL - either editable (custom) or read-only display */}
              <div>
                {selectedEnv === "custom" ? (
                  <Input
                    label="Custom Server URL"
                    value={cloudUrl}
                    onChange={(e) => setCloudUrl(e.target.value)}
                    placeholder="wss://your-server.com"
                    disabled={!cloudEnabled}
                  />
                ) : (
                  <>
                    <label className="block text-xs font-semibold uppercase tracking-wider text-theme-muted mb-1.5">
                      Server URL
                    </label>
                    <div className="w-full px-4 py-3 rounded-xl bg-theme-secondary border border-theme text-sm text-theme-muted font-mono">
                      {cloudUrl}
                    </div>
                  </>
                )}
              </div>

              <div className="flex-1" />
              <div className="flex justify-end">
                <Button
                  onClick={saveSettings}
                  loading={saving}
                  disabled={saving || (!cloudEnabled && !cloudConfig?.enabled)}
                >
                  Save Settings
                </Button>
              </div>
            </div>
          </Card>
        </div>
      </div>
    </div>
  );
}
