import { useState } from "react";
import { useNavigate } from "react-router-dom";
import { Card } from "@/components/Card";
import { Button } from "@/components/Button";
import { Input } from "@/components/Input";
import { Logo } from "@/components/Logo";
import { QRScanner } from "@/components/QRScanner";
import { useAppStore } from "@/lib/mode";
import { Coffee, QrCode, Wifi, Check, ArrowRight, Loader2 } from "lucide-react";

export function Onboarding() {
  const navigate = useNavigate();
  const { claimDevice, fetchDevices } = useAppStore();

  const [step, setStep] = useState<"welcome" | "scan" | "manual" | "success">(
    "welcome"
  );
  const [claimCode, setClaimCode] = useState("");
  const [deviceName, setDeviceName] = useState("");
  const [claiming, setClaiming] = useState(false);
  const [error, setError] = useState("");

  const handleClaim = async (code?: string) => {
    const codeToUse = code || claimCode;
    if (!codeToUse) return;

    setClaiming(true);
    setError("");

    try {
      // Parse QR code URL or manual entry
      let deviceId = "";
      let token = "";
      let manualCode = "";

      if (codeToUse.includes("?")) {
        // URL format
        try {
          const url = new URL(codeToUse);
          deviceId = url.searchParams.get("id") || "";
          token = url.searchParams.get("token") || "";
        } catch {
          // Try parsing as query string
          const params = new URLSearchParams(codeToUse.split("?")[1]);
          deviceId = params.get("id") || "";
          token = params.get("token") || "";
        }
      } else if (codeToUse.includes(":")) {
        // Legacy format: DEVICE_ID:TOKEN
        const parts = codeToUse.split(":");
        deviceId = parts[0];
        token = parts[1] || "";
      } else if (/^[A-Z0-9]{4}-[A-Z0-9]{4}$/i.test(codeToUse.trim())) {
        // Short manual code format: X6ST-AP3G
        manualCode = codeToUse.trim().toUpperCase();
      } else {
        setError("Invalid code format");
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
          setStep("success");
          await fetchDevices();
          setTimeout(() => navigate("/"), 2000);
        } else {
          setError("Invalid or expired code");
        }
        setClaiming(false);
        return;
      }

      if (!deviceId || !token) {
        setError("Invalid code format");
        setClaiming(false);
        return;
      }

      const success = await claimDevice(
        deviceId,
        token,
        deviceName || undefined
      );

      if (success) {
        setStep("success");
        await fetchDevices();
        setTimeout(() => navigate("/"), 2000);
      } else {
        setError("Failed to add device. The code may have expired.");
      }
    } catch {
      setError("An error occurred");
    }

    setClaiming(false);
  };

  return (
    <div className="full-page-scroll bg-gradient-to-br from-coffee-800 via-coffee-900 to-coffee-950 flex items-center justify-center p-4">
      <div className="w-full max-w-lg">
        {step === "welcome" && (
          <Card className="text-center">
            <div className="py-8">
              <div className="flex justify-center mb-6">
                <Logo size="lg" />
              </div>

              <h1 className="text-3xl font-bold text-theme mb-2">
                Welcome to BrewOS
              </h1>
              <p className="text-theme-muted mb-8 max-w-sm mx-auto">
                Control your espresso machine from anywhere. Let's add your
                first machine.
              </p>

              <div className="space-y-4">
                <Button
                  className="w-full py-4 text-lg"
                  onClick={() => setStep("scan")}
                >
                  <QrCode className="w-5 h-5" />
                  Scan QR Code
                </Button>

                <p className="text-sm text-theme-muted">
                  or{" "}
                  <button
                    className="text-accent hover:underline"
                    onClick={() => setStep("manual")}
                  >
                    enter code manually
                  </button>
                </p>
              </div>
            </div>
          </Card>
        )}

        {step === "scan" && (
          <Card>
            <div className="text-center mb-4">
              <h2 className="text-2xl font-bold text-theme mb-2">
                Scan QR Code
              </h2>
              <p className="text-theme-muted">
                Find the QR code on your BrewOS display or web interface.
              </p>
            </div>

            {/* QR Scanner */}
            <div className="mb-4">
              <QRScanner
                onScan={(result) => {
                  setClaimCode(result);
                  // Auto-submit if valid URL
                  if (
                    result.includes("?") &&
                    result.includes("id=") &&
                    result.includes("token=")
                  ) {
                    handleClaim(result);
                  }
                }}
                onError={(err) => console.log("QR scan error:", err)}
              />
            </div>

            <div className="flex items-center gap-2 text-xs text-theme-muted justify-center mb-4">
              <Wifi className="w-4 h-4" />
              <span>Make sure your machine is connected to WiFi</span>
            </div>

            <div className="border-t border-theme pt-4 space-y-4">
              <Input
                label="Or paste the pairing URL"
                placeholder="https://brewos.io/pair?id=BRW-..."
                value={claimCode}
                onChange={(e) => setClaimCode(e.target.value)}
              />

              <Input
                label="Machine Name (optional)"
                placeholder="Kitchen Espresso"
                value={deviceName}
                onChange={(e) => setDeviceName(e.target.value)}
              />

              {error && (
                <p className="text-sm text-error bg-error-soft p-3 rounded-lg">
                  {error}
                </p>
              )}

              <div className="flex gap-3">
                <Button
                  variant="secondary"
                  className="flex-1"
                  onClick={() => setStep("welcome")}
                >
                  Back
                </Button>
                <Button
                  className="flex-1"
                  onClick={() => handleClaim()}
                  loading={claiming}
                  disabled={!claimCode}
                >
                  Add Machine
                  <ArrowRight className="w-4 h-4" />
                </Button>
              </div>
            </div>
          </Card>
        )}

        {step === "manual" && (
          <Card>
            <div className="text-center mb-6">
              <h2 className="text-2xl font-bold text-theme mb-2">
                Enter Pairing Code
              </h2>
              <p className="text-theme-secondary">
                Find the code on your BrewOS display under System → Cloud
                Access.
              </p>
            </div>

            <div className="space-y-4">
              <Input
                label="Pairing Code"
                placeholder="X6ST-AP3G"
                value={claimCode}
                onChange={(e) => setClaimCode(e.target.value.toUpperCase())}
                hint="Enter the 8-character code from your machine display"
              />

              <Input
                label="Machine Name (optional)"
                placeholder="Kitchen Espresso"
                value={deviceName}
                onChange={(e) => setDeviceName(e.target.value)}
              />

              {error && (
                <p className="text-sm text-error bg-error-soft p-3 rounded-lg">
                  {error}
                </p>
              )}

              <div className="flex gap-3">
                <Button
                  variant="secondary"
                  className="flex-1"
                  onClick={() => setStep("welcome")}
                >
                  Back
                </Button>
                <Button
                  className="flex-1"
                  onClick={() => handleClaim()}
                  loading={claiming}
                  disabled={!claimCode}
                >
                  Add Machine
                  <ArrowRight className="w-4 h-4" />
                </Button>
              </div>
            </div>
          </Card>
        )}

        {step === "success" && (
          <Card className="text-center py-12">
            <div className="w-16 h-16 bg-success-soft rounded-full flex items-center justify-center mx-auto mb-4">
              <Check className="w-8 h-8 text-success" />
            </div>
            <h2 className="text-2xl font-bold text-theme mb-2">
              Machine Added!
            </h2>
            <p className="text-theme-secondary mb-4">
              {deviceName || "Your machine"} is now connected to your account.
            </p>
            <div className="flex items-center justify-center gap-2 text-sm text-theme-muted">
              <Loader2 className="w-4 h-4 animate-spin" />
              Redirecting to dashboard...
            </div>
          </Card>
        )}

        {/* How it works */}
        {(step === "welcome" || step === "scan") && (
          <div className="mt-8 text-center">
            <h3 className="text-sm font-semibold text-cream-300 mb-4">
              How it works
            </h3>
            <div className="grid grid-cols-3 gap-4 text-cream-400 text-xs">
              <div className="flex flex-col items-center gap-2">
                <div className="w-10 h-10 bg-cream-800/30 rounded-full flex items-center justify-center">
                  <Wifi className="w-5 h-5" />
                </div>
                <span>Connect machine to WiFi</span>
              </div>
              <div className="flex flex-col items-center gap-2">
                <div className="w-10 h-10 bg-cream-800/30 rounded-full flex items-center justify-center">
                  <QrCode className="w-5 h-5" />
                </div>
                <span>Scan QR code on display</span>
              </div>
              <div className="flex flex-col items-center gap-2">
                <div className="w-10 h-10 bg-cream-800/30 rounded-full flex items-center justify-center">
                  <Coffee className="w-5 h-5" />
                </div>
                <span>Control from anywhere</span>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
