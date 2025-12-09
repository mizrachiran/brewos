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
function WizardStoryWrapper({ children }: { children?: React.ReactNode }) {
  return <>{children}</>;
}

const meta = {
  title: "Pages/FirstRunWizard",
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
  name: "1. Welcome",
  render: () => (
    <WizardStepWrapper
      currentStep={0}
      steps={WIZARD_STEPS}
      showBack={false}
      nextLabel="Next"
      variant="auto"
    >
      <WelcomeStep />
    </WizardStepWrapper>
  ),
};

function MachineSelectionComponent() {
  const [name, setName] = useState("");
  const [machineId, setMachineId] = useState("");
  const [errors] = useState<Record<string, string>>({});
  return (
    <WizardStepWrapper currentStep={1} steps={WIZARD_STEPS} variant="auto">
      <MachineStep
        machineName={name}
        selectedMachineId={machineId}
        errors={errors}
        onMachineNameChange={setName}
        onMachineIdChange={setMachineId}
      />
    </WizardStepWrapper>
  );
}

export const MachineSelection: Story = {
  name: "2. Machine Selection",
  render: () => <MachineSelectionComponent />,
};

function PowerSettingsComponent() {
  const [voltage, setVoltage] = useState(220);
  const [current, setCurrent] = useState(13);
  const [errors] = useState<Record<string, string>>({});
  return (
    <WizardStepWrapper currentStep={2} steps={WIZARD_STEPS} variant="auto">
      <EnvironmentStep
        voltage={voltage}
        maxCurrent={current}
        errors={errors}
        onVoltageChange={setVoltage}
        onMaxCurrentChange={setCurrent}
      />
    </WizardStepWrapper>
  );
}

export const PowerSettings: Story = {
  name: "3. Power Settings",
  render: () => <PowerSettingsComponent />,
};

function CloudSetupComponent() {
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
      variant="auto"
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
}

export const CloudSetup: Story = {
  name: "4. Cloud Setup",
  render: () => <CloudSetupComponent />,
};

export const Done: Story = {
  name: "5. Done",
  render: () => (
    <WizardStepWrapper
      currentStep={4}
      steps={WIZARD_STEPS}
      showBack={false}
      nextLabel="Start Brewing"
      variant="auto"
    >
      <DoneStep machineName="My Bianca" />
    </WizardStepWrapper>
  ),
};

// ============ EXTRA STATES ============

function CloudConnectedComponent() {
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
      variant="auto"
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
}

export const CloudConnected: Story = {
  name: "4. Cloud Connected",
  render: () => <CloudConnectedComponent />,
};

function CloudDisabledComponent() {
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
      variant="auto"
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
}

export const CloudDisabled: Story = {
  name: "4. Cloud Disabled",
  render: () => <CloudDisabledComponent />,
};

// ============ ALL STEPS OVERVIEW ============

export const AllSteps: Story = {
  name: "All Steps Overview",
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
          variant="auto"
        >
          <WelcomeStep />
        </WizardStepWrapper>
      </div>
      <div>
        <h3 className="text-theme text-lg font-semibold mb-4 text-center">
          Step 2: Machine Selection
        </h3>
        <WizardStepWrapper currentStep={1} steps={WIZARD_STEPS} variant="auto">
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
        <WizardStepWrapper currentStep={2} steps={WIZARD_STEPS} variant="auto">
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
          variant="auto"
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
          variant="auto"
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
