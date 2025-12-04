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

// ============ DESKTOP STORIES ============

export const WelcomeDesktop: Story = {
  name: "1. Welcome - Desktop",
  render: () => (
    <WizardStepWrapper
      currentStep={0}
      steps={WIZARD_STEPS}
      showBack={false}
      nextLabel="Next"
      variant="desktop"
    >
      <WelcomeStep />
    </WizardStepWrapper>
  ),
};

function MachineSelectionDesktopComponent() {
  const [name, setName] = useState("");
  const [machineId, setMachineId] = useState("");
  const [errors] = useState<Record<string, string>>({});
  return (
    <WizardStepWrapper currentStep={1} steps={WIZARD_STEPS} variant="desktop">
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

export const MachineSelectionDesktop: Story = {
  name: "2. Machine Selection - Desktop",
  render: () => <MachineSelectionDesktopComponent />,
};

function PowerSettingsDesktopComponent() {
  const [voltage, setVoltage] = useState(220);
  const [current, setCurrent] = useState(13);
  const [errors] = useState<Record<string, string>>({});
  return (
    <WizardStepWrapper currentStep={2} steps={WIZARD_STEPS} variant="desktop">
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

export const PowerSettingsDesktop: Story = {
  name: "3. Power Settings - Desktop",
  render: () => <PowerSettingsDesktopComponent />,
};

function CloudSetupDesktopComponent() {
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
      variant="desktop"
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

export const CloudSetupDesktop: Story = {
  name: "4. Cloud Setup - Desktop",
  render: () => <CloudSetupDesktopComponent />,
};

export const DoneDesktop: Story = {
  name: "5. Done - Desktop",
  render: () => (
    <WizardStepWrapper
      currentStep={4}
      steps={WIZARD_STEPS}
      showBack={false}
      nextLabel="Start Brewing"
      variant="desktop"
    >
      <DoneStep machineName="My Bianca" />
    </WizardStepWrapper>
  ),
};

// ============ MOBILE STORIES ============

export const WelcomeMobile: Story = {
  name: "1. Welcome - Mobile",
  parameters: {
    viewport: { defaultViewport: "mobile1" },
  },
  render: () => (
    <WizardStepWrapper
      currentStep={0}
      steps={WIZARD_STEPS}
      showBack={false}
      nextLabel="Next"
      variant="mobile"
    >
      <WelcomeStep />
    </WizardStepWrapper>
  ),
};

function MachineSelectionMobileComponent() {
  const [name, setName] = useState("My Bianca");
  const [machineId, setMachineId] = useState("lelit_bianca");
  const [errors] = useState<Record<string, string>>({});
  return (
    <WizardStepWrapper currentStep={1} steps={WIZARD_STEPS} variant="mobile">
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

export const MachineSelectionMobile: Story = {
  name: "2. Machine Selection - Mobile",
  parameters: {
    viewport: { defaultViewport: "mobile1" },
  },
  render: () => <MachineSelectionMobileComponent />,
};

function PowerSettingsMobileComponent() {
  const [voltage, setVoltage] = useState(220);
  const [current, setCurrent] = useState(13);
  const [errors] = useState<Record<string, string>>({});
  return (
    <WizardStepWrapper currentStep={2} steps={WIZARD_STEPS} variant="mobile">
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

export const PowerSettingsMobile: Story = {
  name: "3. Power Settings - Mobile",
  parameters: {
    viewport: { defaultViewport: "mobile1" },
  },
  render: () => <PowerSettingsMobileComponent />,
};

function CloudSetupMobileComponent() {
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
      variant="mobile"
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

export const CloudSetupMobile: Story = {
  name: "4. Cloud Setup - Mobile",
  parameters: {
    viewport: { defaultViewport: "mobile1" },
  },
  render: () => <CloudSetupMobileComponent />,
};

export const DoneMobile: Story = {
  name: "5. Done - Mobile",
  parameters: {
    viewport: { defaultViewport: "mobile1" },
  },
  render: () => (
    <WizardStepWrapper
      currentStep={4}
      steps={WIZARD_STEPS}
      showBack={false}
      nextLabel="Start Brewing"
      variant="mobile"
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
      variant="desktop"
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

export const CloudConnectedDesktop: Story = {
  name: "4. Cloud Connected - Desktop",
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
      variant="desktop"
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

export const CloudDisabledDesktop: Story = {
  name: "4. Cloud Disabled - Desktop",
  render: () => <CloudDisabledComponent />,
};

// ============ LEGACY ALIASES (for backward compatibility) ============

export const Welcome = WelcomeDesktop;
export const MachineSelection: Story = {
  render: () => <MachineSelectionDesktopComponent />,
};
export const PowerSettings: Story = {
  render: () => <PowerSettingsDesktopComponent />,
};
export const CloudSetup: Story = {
  render: () => <CloudSetupDesktopComponent />,
};
export const CloudConnected: Story = {
  render: () => <CloudConnectedComponent />,
};
export const CloudDisabled: Story = {
  render: () => <CloudDisabledComponent />,
};
export const Done = DoneDesktop;

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
          variant="desktop"
        >
          <WelcomeStep />
        </WizardStepWrapper>
      </div>
      <div>
        <h3 className="text-theme text-lg font-semibold mb-4 text-center">
          Step 2: Machine Selection
        </h3>
        <WizardStepWrapper currentStep={1} steps={WIZARD_STEPS} variant="desktop">
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
        <WizardStepWrapper currentStep={2} steps={WIZARD_STEPS} variant="desktop">
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
          variant="desktop"
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
          variant="desktop"
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
