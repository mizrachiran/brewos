import { useState } from "react";
import { useNavigate } from "react-router-dom";
import {
  WelcomeStep,
  OnboardingLayout,
  DeviceClaimFlow,
} from "@/components/onboarding";
import { useAppStore } from "@/lib/mode";

/**
 * Onboarding page for new users adding their first device.
 * Shows a welcome step first, then reuses DeviceClaimFlow for the actual claim process.
 * This avoids duplicating the scan/manual/name/success step logic.
 */
export function Onboarding() {
  const navigate = useNavigate();
  const { claimDevice, fetchDevices } = useAppStore();
  const [showWelcome, setShowWelcome] = useState(true);
  const [initialStep, setInitialStep] = useState<"scan" | "manual">("scan");

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

  const handleSuccess = () => {
    setTimeout(() => navigate("/"), 2000);
  };

  if (showWelcome) {
    return (
      <OnboardingLayout>
        <WelcomeStep
          onScanClick={() => {
            setInitialStep("scan");
            setShowWelcome(false);
          }}
          onManualClick={() => {
            setInitialStep("manual");
            setShowWelcome(false);
          }}
        />
      </OnboardingLayout>
    );
  }

  return (
    <OnboardingLayout>
      <DeviceClaimFlow
        onClose={() => setShowWelcome(true)}
        onSuccess={handleSuccess}
        onClaim={handleClaim}
        onClaimManual={handleClaimManual}
        initialStep={initialStep}
      />
    </OnboardingLayout>
  );
}
