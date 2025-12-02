import { useState, useEffect, useMemo } from "react";
import { Card } from "@/components/Card";
import { Button } from "@/components/Button";
import { useCommand } from "@/lib/useCommand";
import { useToast } from "@/components/Toast";
import { Coffee, Settings, Cloud, Check, ArrowRight, ArrowLeft } from "lucide-react";
import { getMachineById } from "@/lib/machines";
import {
  WelcomeStep,
  MachineStep,
  CloudStep,
  DoneStep,
  ProgressIndicator,
  type WizardStep,
  type PairingData,
} from "@/components/wizard";

const STEPS: WizardStep[] = [
  { id: "welcome", title: "Welcome", icon: <Coffee className="w-5 h-5" /> },
  { id: "machine", title: "Your Machine", icon: <Settings className="w-5 h-5" /> },
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

  // Cloud pairing
  const [pairing, setPairing] = useState<PairingData | null>(null);
  const [loadingQR, setLoadingQR] = useState(false);
  const [copied, setCopied] = useState(false);

  // Validation
  const [errors, setErrors] = useState<Record<string, string>>({});

  const selectedMachine = useMemo(
    () => (selectedMachineId ? getMachineById(selectedMachineId) : undefined),
    [selectedMachineId]
  );

  // Fetch pairing QR on cloud step
  useEffect(() => {
    if (STEPS[currentStep].id === "cloud") {
      fetchPairingQR();
    }
  }, [currentStep]);

  const fetchPairingQR = async () => {
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
  };

  const copyPairingCode = () => {
    if (pairing) {
      navigator.clipboard.writeText(`${pairing.deviceId}:${pairing.token}`);
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

      case "cloud":
        return (
          <CloudStep
            pairing={pairing}
            loading={loadingQR}
            copied={copied}
            onCopy={copyPairingCode}
            onSkip={skipCloud}
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

  return (
    <div className="min-h-screen bg-gradient-to-br from-coffee-800 via-coffee-900 to-coffee-950 flex items-center justify-center p-4">
      <div className="w-full max-w-xl">
        <ProgressIndicator steps={STEPS} currentStep={currentStep} />

        <Card>
          {renderStepContent()}

          <div className="flex justify-between pt-6 border-t border-cream-200 mt-6">
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
        </Card>
      </div>
    </div>
  );
}
