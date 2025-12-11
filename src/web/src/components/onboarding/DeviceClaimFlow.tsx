import { useState, useCallback, useEffect, useRef } from "react";
import { Button } from "@/components/Button";
import {
  ScanStep,
  ManualStep,
  MachineNameStep,
  SuccessStep,
} from "@/components/onboarding";
import { QrCode } from "lucide-react";
import { parseClaimCode } from "@/lib/claim-parser";

interface DeviceClaimFlowProps {
  onClose?: () => void;
  onSuccess?: (deviceName?: string) => void;
  onClaim: (
    deviceId: string,
    token: string,
    deviceName?: string
  ) => Promise<boolean>;
  onClaimManual?: (manualCode: string, deviceName?: string) => Promise<boolean>;
  showTabs?: boolean; // Show tab switcher at top (for modal context)
  initialStep?: "scan" | "manual"; // Initial step to show (default: scan)
  className?: string;
}

export function DeviceClaimFlow({
  onClose,
  onSuccess,
  onClaim,
  onClaimManual,
  showTabs = false,
  initialStep = "scan",
  className = "",
}: DeviceClaimFlowProps) {
  const [step, setStep] = useState<"scan" | "manual" | "name" | "success">(
    initialStep
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
  const [successDeviceName, setSuccessDeviceName] = useState<
    string | undefined
  >();
  
  // Track if we should auto-validate after QR scan
  const pendingScanValidation = useRef(false);

  // Validate the code first (without claiming)
  const handleValidate = useCallback(async (fromStep?: "scan" | "manual") => {
    if (!claimCode) return;

    setValidating(true);
    setError("");

    try {
      const parsed = parseClaimCode(claimCode);

      if (parsed.manualCode || (parsed.deviceId && parsed.token)) {
        // Code format is valid, move to name step
        setParsedCode(parsed);
        // Store which step we came from (for back navigation)
        if (fromStep) {
          setPreviousStep(fromStep);
        }
        setStep("name");
      } else {
        setError("Invalid code format");
      }
    } catch {
      setError("Invalid code format");
    }

    setValidating(false);
  }, [claimCode]);

  // Auto-validate when QR scan succeeds - triggered by pendingScanValidation ref
  useEffect(() => {
    if (pendingScanValidation.current && claimCode && step === "scan") {
      pendingScanValidation.current = false;
      // Pass the current step to handleValidate so it knows where to go back to
      handleValidate("scan");
    }
  }, [claimCode, step, handleValidate]);

  // Complete the claim with machine name
  const handleClaim = async () => {
    if (!parsedCode) return;

    setClaiming(true);
    setError("");

    try {
      // Handle manual code format
      if (parsedCode.manualCode && onClaimManual) {
        const success = await onClaimManual(
          parsedCode.manualCode,
          deviceName || undefined
        );
        if (success) {
          setSuccessDeviceName(deviceName || undefined);
          setStep("success");
          if (onSuccess) {
            setTimeout(() => {
              onSuccess(deviceName || undefined);
              reset();
            }, 3000);
          }
        } else {
          setError("Invalid or expired code");
          setStep(previousStep); // Go back to previous step
        }
        setClaiming(false);
        return;
      }

      // Handle deviceId + token format
      if (parsedCode.deviceId && parsedCode.token) {
        const success = await onClaim(
          parsedCode.deviceId,
          parsedCode.token,
          deviceName || undefined
        );
        if (success) {
          setSuccessDeviceName(deviceName || undefined);
          setStep("success");
          if (onSuccess) {
            setTimeout(() => {
              onSuccess(deviceName || undefined);
              reset();
            }, 3000);
          }
        } else {
          setError("Failed to add machine. Code may be expired.");
          setStep(previousStep); // Go back to previous step
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

  const reset = () => {
    setStep("scan");
    setClaimCode("");
    setDeviceName("");
    setError("");
    setPreviousStep("scan");
    setParsedCode(null);
    setSuccessDeviceName(undefined);
  };

  const handleClose = () => {
    reset();
    onClose?.();
  };

  if (step === "success") {
    return (
      <div className={`w-full ${className || ""}`}>
        <SuccessStep deviceName={successDeviceName} />
      </div>
    );
  }

  return (
    <div className={`w-full ${className || ""}`}>
      {showTabs && (
        <div className="flex gap-2 mb-4 xs:mb-6">
          <Button
            variant={step === "scan" ? "primary" : "secondary"}
            size="sm"
            className="flex-1 text-xs xs:text-sm"
            onClick={() => {
              setStep("scan");
              setError("");
            }}
          >
            <QrCode className="w-4 h-4" />
            <span className="hidden xs:inline">Scan QR Code</span>
            <span className="xs:hidden">Scan</span>
          </Button>
          <Button
            variant={step === "manual" ? "primary" : "secondary"}
            size="sm"
            className="flex-1 text-xs xs:text-sm"
            onClick={() => {
              setStep("manual");
              setError("");
            }}
          >
            <span className="hidden xs:inline">Enter Code</span>
            <span className="xs:hidden">Code</span>
          </Button>
        </div>
      )}

      {step === "scan" && (
        <ScanStep
          onScan={(result) => {
            // Mark that we need to auto-validate once claimCode updates
            pendingScanValidation.current = true;
            setClaimCode(result);
            setError("");
          }}
          onScanError={(err) => {
            setError(err || "Failed to scan QR code");
          }}
          error={error}
          onBack={showTabs ? undefined : handleClose}
          onValidate={() => handleValidate("scan")}
          disabled={!claimCode || validating}
          loading={validating}
        />
      )}

      {step === "manual" && (
        <ManualStep
          claimCode={claimCode}
          onClaimCodeChange={setClaimCode}
          error={error}
          onBack={showTabs ? undefined : handleClose}
          onValidate={() => handleValidate("manual")}
          disabled={!claimCode || validating}
          loading={validating}
        />
      )}

      {step === "name" && (
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
      )}
    </div>
  );
}
