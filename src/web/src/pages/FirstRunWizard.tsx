import { useState, useEffect, useMemo, useCallback } from "react";
import { Card } from "@/components/Card";
import { Button } from "@/components/Button";
import { useCommand } from "@/lib/useCommand";
import { useToast } from "@/components/Toast";
import {
  Coffee,
  Settings,
  Cloud,
  Check,
  ArrowRight,
  ArrowLeft,
  Zap,
} from "lucide-react";
import { getMachineById } from "@/lib/machines";
import {
  WelcomeStep,
  MachineStep,
  EnvironmentStep,
  CloudStep,
  DoneStep,
  ProgressIndicator,
  type WizardStep,
  type PairingData,
} from "@/components/wizard";
import { darkBgStyles } from "@/lib/darkBgStyles";

const STEPS: WizardStep[] = [
  { id: "welcome", title: "Welcome", icon: <Coffee className="w-5 h-5" /> },
  {
    id: "machine",
    title: "Your Machine",
    icon: <Settings className="w-5 h-5" />,
  },
  { id: "environment", title: "Power", icon: <Zap className="w-5 h-5" /> },
  { id: "cloud", title: "Cloud Access", icon: <Cloud className="w-5 h-5" /> },
  { id: "done", title: "All Set!", icon: <Check className="w-5 h-5" /> },
];

interface FirstRunWizardProps {
  onComplete: () => void;
}

export function FirstRunWizard({ onComplete }: FirstRunWizardProps) {
  const { sendCommand } = useCommand();
  const { error } = useToast();

  const [currentStep, setCurrentStep] = useState(0);
  const [saving, setSaving] = useState(false);

  // Machine info
  const [machineName, setMachineName] = useState("");
  const [selectedMachineId, setSelectedMachineId] = useState("");

  // Environment settings
  const [voltage, setVoltage] = useState(220);
  const [maxCurrent, setMaxCurrent] = useState(13);

  // Cloud pairing
  const [pairing, setPairing] = useState<PairingData | null>(null);
  const [loadingQR, setLoadingQR] = useState(false);
  const [copied, setCopied] = useState(false);
  const [cloudEnabled, setCloudEnabled] = useState(true);
  const [cloudConnected, setCloudConnected] = useState(false);
  const [checkingCloudStatus, setCheckingCloudStatus] = useState(false);

  // Validation
  const [errors, setErrors] = useState<Record<string, string>>({});

  const selectedMachine = useMemo(
    () => (selectedMachineId ? getMachineById(selectedMachineId) : undefined),
    [selectedMachineId]
  );

  const checkCloudStatus = useCallback(async () => {
    setCheckingCloudStatus(true);
    try {
      const response = await fetch("/api/cloud/status");
      if (response.ok) {
        const data = await response.json();
        setCloudConnected(data.connected || false);
      } else {
        setCloudConnected(false);
      }
    } catch {
      // Device might not support cloud status endpoint yet
      setCloudConnected(false);
    }
    setCheckingCloudStatus(false);
  }, []);

  const fetchPairingQR = useCallback(async () => {
    setLoadingQR(true);
    try {
      const response = await fetch("/api/pairing/qr");
      if (response.ok) {
        const data = await response.json();
        setPairing(data);
      }
    } catch {
      console.log("Failed to fetch pairing QR");
    }
    setLoadingQR(false);
  }, []);

  // Fetch pairing QR and check cloud status on cloud step
  useEffect(() => {
    if (STEPS[currentStep].id === "cloud") {
      if (cloudEnabled) {
        fetchPairingQR();
        checkCloudStatus();

        // Poll cloud status every 3 seconds to detect when pairing completes
        const interval = setInterval(() => {
          checkCloudStatus();
        }, 3000);

        return () => clearInterval(interval);
      } else {
        setCloudConnected(false);
      }
    }
  }, [currentStep, cloudEnabled, fetchPairingQR, checkCloudStatus]);

  const copyPairingCode = () => {
    if (pairing) {
      const code = pairing.manualCode || `${pairing.deviceId}:${pairing.token}`;
      navigator.clipboard.writeText(code);
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    }
  };

  const validateMachineStep = (): boolean => {
    const newErrors: Record<string, string> = {};
    if (!machineName.trim()) {
      newErrors.machineName = "Please give your machine a name";
    }
    if (!selectedMachineId) {
      newErrors.machineModel = "Please select your machine model";
    }
    setErrors(newErrors);
    return Object.keys(newErrors).length === 0;
  };

  const validateEnvironmentStep = (): boolean => {
    const newErrors: Record<string, string> = {};
    if (!voltage) {
      newErrors.voltage = "Please select your mains voltage";
    }
    if (!maxCurrent || maxCurrent < 10 || maxCurrent > 20) {
      newErrors.maxCurrent = "Max current must be between 10A and 20A";
    }
    setErrors(newErrors);
    return Object.keys(newErrors).length === 0;
  };

  const validateCloudStep = (): boolean => {
    const newErrors: Record<string, string> = {};

    // If cloud is enabled, check if it's connected/paired
    if (cloudEnabled && !cloudConnected && !checkingCloudStatus) {
      // Allow continuing but show a warning - pairing can be completed later
      // This is just informational, not blocking
      newErrors.cloudNotPaired =
        "Cloud is enabled but not yet paired. You can complete pairing later in Settings.";
    }

    setErrors(newErrors);
    // Don't block progression - just show warning
    return true;
  };

  const saveMachineInfo = async () => {
    if (!selectedMachine) return;

    setSaving(true);
    try {
      const sent = sendCommand(
        "set_device_info",
        {
          name: machineName,
          machineBrand: selectedMachine.brand,
          machineModel: selectedMachine.model,
          machineType: selectedMachine.type,
          machineId: selectedMachine.id,
          defaultBrewTemp: selectedMachine.defaults.brewTemp,
          defaultSteamTemp: selectedMachine.defaults.steamTemp,
        },
        { successMessage: "Machine info saved" }
      );

      if (sent) {
        await fetch("/api/settings", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            machineName,
            machineBrand: selectedMachine.brand,
            machineModel: selectedMachine.model,
            machineType: selectedMachine.type,
            machineId: selectedMachine.id,
          }),
        });
      }
    } catch (err) {
      console.error("Failed to save machine info:", err);
      error("Failed to save machine info. Please try again.");
    }
    setSaving(false);
  };

  const saveEnvironmentSettings = async () => {
    setSaving(true);
    try {
      sendCommand(
        "set_power_config",
        {
          voltage,
          maxCurrent,
        },
        { successMessage: "Power settings saved" }
      );
    } catch (err) {
      console.error("Failed to save power settings:", err);
      error("Failed to save power settings. Please try again.");
    }
    setSaving(false);
  };

  const saveCloudSettings = async () => {
    setSaving(true);
    try {
      await fetch("/api/settings", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          cloudEnabled,
        }),
      });
    } catch (err) {
      console.error("Failed to save cloud settings:", err);
    }
    setSaving(false);
  };

  const completeSetup = async () => {
    setSaving(true);
    try {
      await fetch("/api/setup/complete", { method: "POST" });
      onComplete();
    } catch {
      onComplete();
    }
    setSaving(false);
  };

  const nextStep = async () => {
    const stepId = STEPS[currentStep].id;

    if (stepId === "machine") {
      if (!validateMachineStep()) return;
      await saveMachineInfo();
    }

    if (stepId === "environment") {
      if (!validateEnvironmentStep()) return;
      await saveEnvironmentSettings();
    }

    if (stepId === "cloud") {
      validateCloudStep();
      await saveCloudSettings();
    }

    if (stepId === "done") {
      await completeSetup();
      return;
    }

    setCurrentStep((prev) => Math.min(prev + 1, STEPS.length - 1));
  };

  const prevStep = () => {
    setCurrentStep((prev) => Math.max(prev - 1, 0));
  };

  const skipCloud = () => {
    setCloudEnabled(false);
    setCurrentStep(STEPS.length - 1);
  };

  const renderStepContent = () => {
    const stepId = STEPS[currentStep].id;

    switch (stepId) {
      case "welcome":
        return <WelcomeStep />;

      case "machine":
        return (
          <MachineStep
            machineName={machineName}
            selectedMachineId={selectedMachineId}
            errors={errors}
            onMachineNameChange={setMachineName}
            onMachineIdChange={setSelectedMachineId}
          />
        );

      case "environment":
        return (
          <EnvironmentStep
            voltage={voltage}
            maxCurrent={maxCurrent}
            errors={errors}
            onVoltageChange={setVoltage}
            onMaxCurrentChange={setMaxCurrent}
          />
        );

      case "cloud":
        return (
          <CloudStep
            pairing={pairing}
            loading={loadingQR}
            copied={copied}
            cloudEnabled={cloudEnabled}
            cloudConnected={cloudConnected}
            checkingStatus={checkingCloudStatus}
            error={errors.cloudNotPaired}
            onCopy={copyPairingCode}
            onSkip={skipCloud}
            onCloudEnabledChange={setCloudEnabled}
          />
        );

      case "done":
        return <DoneStep machineName={machineName} />;
    }
  };

  const getButtonLabel = () => {
    const stepId = STEPS[currentStep].id;
    if (stepId === "done") return "Start Brewing";
    return (
      <>
        {stepId === "cloud" ? "Continue" : "Next"}
        <ArrowRight className="w-4 h-4" />
      </>
    );
  };

  const isWelcomeStep = STEPS[currentStep].id === "welcome";

  const renderNavigation = () => (
    <div className="flex justify-between pt-3 xs:pt-6 border-t border-white/10 xs:border-theme mt-3 xs:mt-6">
      {currentStep > 0 && STEPS[currentStep].id !== "done" ? (
        <Button variant="ghost" onClick={prevStep}>
          <ArrowLeft className="w-4 h-4" />
          Back
        </Button>
      ) : (
        <div />
      )}

      <Button onClick={nextStep} loading={saving}>
        {getButtonLabel()}
      </Button>
    </div>
  );

  return (
    <div className="min-h-screen min-h-[100dvh]">
      {/* Narrow width: Full-screen scrollable with dark gradient */}
      <div
        className="xs:hidden bg-gradient-to-br from-coffee-800 via-coffee-900 to-coffee-950 min-h-screen min-h-[100dvh] flex flex-col px-4 py-3 safe-area-inset"
        style={darkBgStyles}
      >
        <ProgressIndicator steps={STEPS} currentStep={currentStep} />

        <div
          className={`flex-1 flex flex-col overflow-y-auto ${
            isWelcomeStep ? "justify-center" : ""
          }`}
        >
          <div className="animate-in fade-in slide-in-from-bottom-4 duration-500 py-1">
            {renderStepContent()}
            {renderNavigation()}
          </div>
        </div>
      </div>

      {/* Wide width: Card layout with theme background */}
      <div
        className={`hidden xs:flex bg-theme min-h-screen p-4 transition-all duration-300 ${
          isWelcomeStep ? "items-center justify-center" : "justify-center pt-8"
        }`}
      >
        <div className={`w-full max-w-xl transition-all duration-300`}>
          <ProgressIndicator steps={STEPS} currentStep={currentStep} />

          <Card
            className={
              !isWelcomeStep
                ? "animate-in fade-in slide-in-from-right-4 duration-300"
                : ""
            }
          >
            {renderStepContent()}
            {renderNavigation()}
          </Card>
        </div>
      </div>
    </div>
  );
}
