import type { Meta, StoryObj } from "@storybook/react";
import { useState } from "react";
import {
  WelcomeStep,
  MachineStep,
  EnvironmentStep,
  CloudStep,
  DoneStep,
} from "@/components/wizard";
import { WizardStepWrapper } from "@/components/storybook";
import type { PairingData } from "@/components/wizard/types";
import { Coffee, Settings, Cloud, Check, Zap } from "lucide-react";

// Wrapper component for stories
function WizardStoryWrapper({ children }: { children: React.ReactNode }) {
  return <>{children}</>;
}

const meta = {
  title: "Pages/Onboarding/FirstRunWizard",
  component: WizardStoryWrapper,
  tags: ["autodocs"],
  parameters: {
    layout: "fullscreen",
  },
} satisfies Meta<typeof WizardStoryWrapper>;

export default meta;
type Story = StoryObj<typeof meta>;

// All wizard steps (WiFi is handled separately by Setup page in AP mode)
const WIZARD_STEPS = [
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

// Mock pairing data
const mockPairing: PairingData = {
  deviceId: "BREW123",
  token: "abc123xyz",
  url: "https://cloud.brewos.io/pair?id=BREW123&token=abc123xyz",
  expiresIn: 300,
  manualCode: "X6ST-AP3G",
};

export const Welcome: Story = {
  render: () => (
    <WizardStepWrapper
      currentStep={0}
      steps={WIZARD_STEPS}
      showBack={false}
      nextLabel="Next"
    >
      <WelcomeStep />
    </WizardStepWrapper>
  ),
};

export const MachineSelection: Story = {
  render: () => {
    const [name, setName] = useState("");
    const [machineId, setMachineId] = useState("");
    const [errors] = useState<Record<string, string>>({});
    return (
      <WizardStepWrapper currentStep={1} steps={WIZARD_STEPS}>
        <MachineStep
          machineName={name}
          selectedMachineId={machineId}
          errors={errors}
          onMachineNameChange={setName}
          onMachineIdChange={setMachineId}
        />
      </WizardStepWrapper>
    );
  },
};

export const MachineSelected: Story = {
  render: () => {
    const [name, setName] = useState("My Bianca");
    const [machineId, setMachineId] = useState("lelit_bianca");
    const [errors] = useState<Record<string, string>>({});
    return (
      <WizardStepWrapper currentStep={1} steps={WIZARD_STEPS}>
        <MachineStep
          machineName={name}
          selectedMachineId={machineId}
          errors={errors}
          onMachineNameChange={setName}
          onMachineIdChange={setMachineId}
        />
      </WizardStepWrapper>
    );
  },
};

export const PowerSettings: Story = {
  render: () => {
    const [voltage, setVoltage] = useState(220);
    const [current, setCurrent] = useState(13);
    const [errors] = useState<Record<string, string>>({});
    return (
      <WizardStepWrapper currentStep={2} steps={WIZARD_STEPS}>
        <EnvironmentStep
          voltage={voltage}
          maxCurrent={current}
          errors={errors}
          onVoltageChange={setVoltage}
          onMaxCurrentChange={setCurrent}
        />
      </WizardStepWrapper>
    );
  },
};

export const PowerSettingsUS: Story = {
  render: () => {
    const [voltage, setVoltage] = useState(110);
    const [current, setCurrent] = useState(15);
    const [errors] = useState<Record<string, string>>({});
    return (
      <WizardStepWrapper currentStep={2} steps={WIZARD_STEPS}>
        <EnvironmentStep
          voltage={voltage}
          maxCurrent={current}
          errors={errors}
          onVoltageChange={setVoltage}
          onMaxCurrentChange={setCurrent}
        />
      </WizardStepWrapper>
    );
  },
};

export const CloudSetup: Story = {
  render: () => {
    const [enabled, setEnabled] = useState(true);
    const [copied, setCopied] = useState(false);
    const handleCopy = () => {
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    };
    return (
      <WizardStepWrapper
        currentStep={3}
        steps={WIZARD_STEPS}
        nextLabel="Continue"
      >
        <CloudStep
          pairing={enabled ? mockPairing : null}
          loading={false}
          copied={copied}
          cloudEnabled={enabled}
          cloudConnected={false}
          checkingStatus={false}
          error={undefined}
          onCopy={handleCopy}
          onSkip={() => setEnabled(false)}
          onCloudEnabledChange={setEnabled}
        />
      </WizardStepWrapper>
    );
  },
};

export const CloudCheckingStatus: Story = {
  render: () => {
    const [enabled, setEnabled] = useState(true);
    const [copied, setCopied] = useState(false);
    const handleCopy = () => {
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    };
    return (
      <WizardStepWrapper
        currentStep={3}
        steps={WIZARD_STEPS}
        nextLabel="Continue"
      >
        <CloudStep
          pairing={enabled ? mockPairing : null}
          loading={false}
          copied={copied}
          cloudEnabled={enabled}
          cloudConnected={false}
          checkingStatus={true}
          error={undefined}
          onCopy={handleCopy}
          onSkip={() => setEnabled(false)}
          onCloudEnabledChange={setEnabled}
        />
      </WizardStepWrapper>
    );
  },
};

export const CloudNotConnected: Story = {
  render: () => {
    const [enabled, setEnabled] = useState(true);
    const [copied, setCopied] = useState(false);
    const handleCopy = () => {
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    };
    return (
      <WizardStepWrapper
        currentStep={3}
        steps={WIZARD_STEPS}
        nextLabel="Continue"
      >
        <CloudStep
          pairing={enabled ? mockPairing : null}
          loading={false}
          copied={copied}
          cloudEnabled={enabled}
          cloudConnected={false}
          checkingStatus={false}
          error="Failed to connect to cloud server."
          onCopy={handleCopy}
          onSkip={() => setEnabled(false)}
          onCloudEnabledChange={setEnabled}
        />
      </WizardStepWrapper>
    );
  },
};

export const CloudConnected: Story = {
  render: () => {
    const [enabled, setEnabled] = useState(true);
    const [copied, setCopied] = useState(false);
    const handleCopy = () => {
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    };
    return (
      <WizardStepWrapper
        currentStep={3}
        steps={WIZARD_STEPS}
        nextLabel="Continue"
      >
        <CloudStep
          pairing={enabled ? mockPairing : null}
          loading={false}
          copied={copied}
          cloudEnabled={enabled}
          cloudConnected={true}
          checkingStatus={false}
          error={undefined}
          onCopy={handleCopy}
          onSkip={() => setEnabled(false)}
          onCloudEnabledChange={setEnabled}
        />
      </WizardStepWrapper>
    );
  },
};

export const CloudDisabled: Story = {
  render: () => {
    const [enabled, setEnabled] = useState(false);
    const [copied, setCopied] = useState(false);
    const handleCopy = () => {
      setCopied(true);
      setTimeout(() => setCopied(false), 2000);
    };
    return (
      <WizardStepWrapper
        currentStep={3}
        steps={WIZARD_STEPS}
        nextLabel="Continue"
      >
        <CloudStep
          pairing={null}
          loading={false}
          copied={copied}
          cloudEnabled={enabled}
          cloudConnected={false}
          checkingStatus={false}
          error={undefined}
          onCopy={handleCopy}
          onSkip={() => setEnabled(false)}
          onCloudEnabledChange={setEnabled}
        />
      </WizardStepWrapper>
    );
  },
};

export const Done: Story = {
  render: () => (
    <WizardStepWrapper
      currentStep={4}
      steps={WIZARD_STEPS}
      showBack={false}
      nextLabel="Start Brewing"
    >
      <DoneStep machineName="My Bianca" />
    </WizardStepWrapper>
  ),
};

export const AllSteps: Story = {
  render: () => (
    <div className="space-y-8 p-4 bg-theme">
      <div>
        <h3 className="text-theme text-lg font-semibold mb-4 text-center">
          Step 1: Welcome
        </h3>
        <WizardStepWrapper
          currentStep={0}
          steps={WIZARD_STEPS}
          showBack={false}
          nextLabel="Next"
        >
          <WelcomeStep />
        </WizardStepWrapper>
      </div>
      <div>
        <h3 className="text-theme text-lg font-semibold mb-4 text-center">
          Step 2: Machine Selection
        </h3>
        <WizardStepWrapper currentStep={1} steps={WIZARD_STEPS}>
          <MachineStep
            machineName="My Bianca"
            selectedMachineId="lelit_bianca"
            errors={{}}
            onMachineNameChange={() => {}}
            onMachineIdChange={() => {}}
          />
        </WizardStepWrapper>
      </div>
      <div>
        <h3 className="text-theme text-lg font-semibold mb-4 text-center">
          Step 3: Power Settings
        </h3>
        <WizardStepWrapper currentStep={2} steps={WIZARD_STEPS}>
          <EnvironmentStep
            voltage={220}
            maxCurrent={13}
            errors={{}}
            onVoltageChange={() => {}}
            onMaxCurrentChange={() => {}}
          />
        </WizardStepWrapper>
      </div>
      <div>
        <h3 className="text-theme text-lg font-semibold mb-4 text-center">
          Step 4: Cloud Setup
        </h3>
        <WizardStepWrapper
          currentStep={3}
          steps={WIZARD_STEPS}
          nextLabel="Continue"
        >
          <CloudStep
            pairing={mockPairing}
            loading={false}
            copied={false}
            cloudEnabled={true}
            cloudConnected={true}
            checkingStatus={false}
            error={undefined}
            onCopy={() => {}}
            onSkip={() => {}}
            onCloudEnabledChange={() => {}}
          />
        </WizardStepWrapper>
      </div>
      <div>
        <h3 className="text-theme text-lg font-semibold mb-4 text-center">
          Step 5: Done
        </h3>
        <WizardStepWrapper
          currentStep={4}
          steps={WIZARD_STEPS}
          showBack={false}
          nextLabel="Start Brewing"
        >
          <DoneStep machineName="My Bianca" />
        </WizardStepWrapper>
      </div>
    </div>
  ),
  parameters: {
    layout: "padded",
  },
};
