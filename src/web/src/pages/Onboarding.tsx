import { useState } from "react";
import { useNavigate } from "react-router-dom";
import { Card } from "@/components/Card";
import {
  WelcomeStep,
  ScanStep,
  ManualStep,
  MachineNameStep,
  SuccessStep,
} from "@/components/onboarding";
import { useAppStore } from "@/lib/mode";
import { parseClaimCode } from "@/lib/claim-parser";
import { darkBgStyles } from "@/lib/darkBgStyles";

export function Onboarding() {
  const navigate = useNavigate();
  const { claimDevice, fetchDevices } = useAppStore();

  const [step, setStep] = useState<"welcome" | "scan" | "manual" | "name" | "success">(
    "welcome"
  );
  const [claimCode, setClaimCode] = useState("");
  const [deviceName, setDeviceName] = useState("");
  const [validating, setValidating] = useState(false);
  const [claiming, setClaiming] = useState(false);
  const [error, setError] = useState("");
  const [previousStep, setPreviousStep] = useState<"scan" | "manual">("scan");
  const [parsedCode, setParsedCode] = useState<{
    deviceId?: string;
    token?: string;
    manualCode?: string;
  } | null>(null);

  // Validate the code first (without claiming)
  const handleValidate = async () => {
    if (!claimCode) return;

    setValidating(true);
    setError("");

    try {
      const parsed = parseClaimCode(claimCode);

      if (parsed.manualCode || (parsed.deviceId && parsed.token)) {
        // Code format is valid, move to name step
        setParsedCode(parsed);
        if (step === "scan" || step === "manual") {
          setPreviousStep(step);
        }
        setStep("name");
      } else {
        setError("Invalid code format");
      }
    } catch {
      setError("Invalid code format");
    }

    setValidating(false);
  };

  // Complete the claim with machine name
  const handleClaim = async () => {
    if (!parsedCode) return;

    setClaiming(true);
    setError("");

    try {
      // Handle manual code format
      if (parsedCode.manualCode) {
        const success = await claimDevice(
          parsedCode.manualCode,
          "",
          deviceName || undefined
        );
        if (success) {
          setStep("success");
          await fetchDevices();
          setTimeout(() => navigate("/"), 2000);
        } else {
          setError("Invalid or expired code");
          setStep(previousStep);
        }
        setClaiming(false);
        return;
      }

      // Handle deviceId + token format
      if (parsedCode.deviceId && parsedCode.token) {
        const success = await claimDevice(
          parsedCode.deviceId,
          parsedCode.token,
          deviceName || undefined
        );

        if (success) {
          setStep("success");
          await fetchDevices();
          setTimeout(() => navigate("/"), 2000);
        } else {
          setError("Failed to add device. The code may have expired.");
          setStep(previousStep);
        }
      } else {
        setError("Invalid code format");
        setStep(previousStep);
      }
    } catch {
      setError("An error occurred");
      setStep(previousStep);
    }

    setClaiming(false);
  };

  const renderStepContent = () => {
    switch (step) {
      case "welcome":
        return (
          <WelcomeStep
            onScanClick={() => setStep("scan")}
            onManualClick={() => setStep("manual")}
          />
        );
      case "scan":
        return (
          <ScanStep
            onScan={(result) => {
              setClaimCode(result);
              setError("");
            }}
            onScanError={(err) => {
              setError(err || "Failed to scan QR code");
            }}
            error={error}
            onBack={() => {
              setStep("welcome");
              setError("");
              setClaimCode("");
            }}
            onValidate={handleValidate}
            disabled={!claimCode || validating}
            loading={validating}
          />
        );
      case "manual":
        return (
          <ManualStep
            claimCode={claimCode}
            onClaimCodeChange={setClaimCode}
            error={error}
            onBack={() => {
              setStep("welcome");
              setError("");
            }}
            onValidate={handleValidate}
            disabled={!claimCode || validating}
            loading={validating}
          />
        );
      case "name":
        return (
          <MachineNameStep
            deviceName={deviceName}
            onDeviceNameChange={setDeviceName}
            onBack={() => {
              setStep(previousStep);
              setError("");
            }}
            onContinue={handleClaim}
            loading={claiming}
          />
        );
      case "success":
        return <SuccessStep deviceName={deviceName} />;
    }
  };

  return (
    <div className="full-page-scroll bg-gradient-to-br from-coffee-800 via-coffee-900 to-coffee-950 min-h-screen">
      {/* Mobile: Full-screen without card */}
      <div 
        className="sm:hidden min-h-screen flex flex-col justify-center px-5 py-8"
        style={darkBgStyles}
      >
        <div className="animate-in fade-in slide-in-from-bottom-4 duration-500">
          {renderStepContent()}
        </div>
      </div>

      {/* Desktop: Card layout with fixed top position */}
      <div className="hidden sm:flex min-h-screen justify-center p-4 pt-16">
        <div className="w-full max-w-lg">
          <Card className="animate-in fade-in slide-in-from-bottom-4 duration-300">
            {renderStepContent()}
          </Card>
        </div>
      </div>
    </div>
  );
}
