import type { Meta, StoryObj } from "@storybook/react";
import { Card } from "@/components/Card";
import {
  WelcomeStep,
  ScanStep,
  ManualStep,
  MachineNameStep,
  SuccessStep,
} from "@/components/onboarding";
import React from "react";

// Wrapper component for stories
function OnboardingStoryWrapper({ children }: { children?: React.ReactNode }) {
  return <>{children}</>;
}

const meta = {
  title: "Pages/Onboarding/Onboarding",
  component: OnboardingStoryWrapper,
  tags: ["autodocs"],
  parameters: {
    layout: "fullscreen",
  },
} satisfies Meta<typeof OnboardingStoryWrapper>;

export default meta;
type Story = StoryObj<typeof meta>;

// CSS variables for dark background (mobile full-screen)
const darkBgStyles = {
  "--text": "#faf8f5",
  "--text-secondary": "#e8e0d5",
  "--text-muted": "#d4c8b8",
  "--card-bg": "rgba(255,255,255,0.05)",
  "--bg-secondary": "rgba(255,255,255,0.08)",
  "--bg-tertiary": "rgba(255,255,255,0.05)",
  "--border": "rgba(255,255,255,0.12)",
} as React.CSSProperties;

// Desktop wrapper with Card
function DesktopWrapper({ children }: { children: React.ReactNode }) {
  return (
    <div className="min-h-screen bg-gradient-to-br from-coffee-800 via-coffee-900 to-coffee-950 flex justify-center p-4 pt-16">
      <div className="w-full max-w-lg">
        <Card className="animate-in fade-in slide-in-from-bottom-4 duration-300">
          {children}
        </Card>
      </div>
    </div>
  );
}

// Mobile wrapper without Card (full-screen)
function MobileWrapper({ children }: { children: React.ReactNode }) {
  return (
    <div className="min-h-screen bg-gradient-to-br from-coffee-800 via-coffee-900 to-coffee-950">
      <div 
        className="min-h-screen flex flex-col justify-center px-5 py-8"
        style={darkBgStyles}
      >
        <div className="animate-in fade-in slide-in-from-bottom-4 duration-500">
          {children}
        </div>
      </div>
    </div>
  );
}

// ============ WELCOME STEP ============

export const WelcomeDesktop: Story = {
  name: "1. Welcome - Desktop",
  render: () => (
    <DesktopWrapper>
      <WelcomeStep />
    </DesktopWrapper>
  ),
};

export const WelcomeMobile: Story = {
  name: "1. Welcome - Mobile",
  parameters: {
    viewport: { defaultViewport: "mobile1" },
  },
  render: () => (
    <MobileWrapper>
      <WelcomeStep />
    </MobileWrapper>
  ),
};

// ============ SCAN STEP ============

export const ScanDesktop: Story = {
  name: "2. Scan QR - Desktop",
  render: () => (
    <DesktopWrapper>
      <ScanStep
        onScan={() => {}}
        onBack={() => {}}
        onValidate={() => {}}
      />
    </DesktopWrapper>
  ),
};

export const ScanMobile: Story = {
  name: "2. Scan QR - Mobile",
  parameters: {
    viewport: { defaultViewport: "mobile1" },
  },
  render: () => (
    <MobileWrapper>
      <ScanStep
        onScan={() => {}}
        onBack={() => {}}
        onValidate={() => {}}
      />
    </MobileWrapper>
  ),
};

export const ScanWithErrorDesktop: Story = {
  name: "2. Scan QR - Error (Desktop)",
  render: () => (
    <DesktopWrapper>
      <ScanStep
        error="Invalid QR code. Please try again."
        onScan={() => {}}
        onBack={() => {}}
        onValidate={() => {}}
      />
    </DesktopWrapper>
  ),
};

// ============ MANUAL STEP ============

export const ManualDesktop: Story = {
  name: "3. Manual Entry - Desktop",
  render: () => (
    <DesktopWrapper>
      <ManualStep
        claimCode="X6ST-AP3G"
        onClaimCodeChange={() => {}}
        onBack={() => {}}
        onValidate={() => {}}
      />
    </DesktopWrapper>
  ),
};

export const ManualMobile: Story = {
  name: "3. Manual Entry - Mobile",
  parameters: {
    viewport: { defaultViewport: "mobile1" },
  },
  render: () => (
    <MobileWrapper>
      <ManualStep
        claimCode="X6ST-AP3G"
        onClaimCodeChange={() => {}}
        onBack={() => {}}
        onValidate={() => {}}
      />
    </MobileWrapper>
  ),
};

export const ManualWithErrorDesktop: Story = {
  name: "3. Manual Entry - Error (Desktop)",
  render: () => (
    <DesktopWrapper>
      <ManualStep
        claimCode="X6ST-AP3G"
        error="Invalid or expired code"
        onClaimCodeChange={() => {}}
        onBack={() => {}}
        onValidate={() => {}}
      />
    </DesktopWrapper>
  ),
};

// ============ MACHINE NAME STEP ============

export const MachineNameDesktop: Story = {
  name: "4. Machine Name - Desktop",
  render: () => (
    <DesktopWrapper>
      <MachineNameStep
        deviceName="Kitchen Espresso"
        onDeviceNameChange={() => {}}
        onBack={() => {}}
        onContinue={() => {}}
      />
    </DesktopWrapper>
  ),
};

export const MachineNameMobile: Story = {
  name: "4. Machine Name - Mobile",
  parameters: {
    viewport: { defaultViewport: "mobile1" },
  },
  render: () => (
    <MobileWrapper>
      <MachineNameStep
        deviceName="Kitchen Espresso"
        onDeviceNameChange={() => {}}
        onBack={() => {}}
        onContinue={() => {}}
      />
    </MobileWrapper>
  ),
};

// ============ SUCCESS STEP ============

export const SuccessDesktop: Story = {
  name: "5. Success - Desktop",
  render: () => (
    <DesktopWrapper>
      <SuccessStep deviceName="Kitchen Espresso" />
    </DesktopWrapper>
  ),
};

export const SuccessMobile: Story = {
  name: "5. Success - Mobile",
  parameters: {
    viewport: { defaultViewport: "mobile1" },
  },
  render: () => (
    <MobileWrapper>
      <SuccessStep deviceName="Kitchen Espresso" />
    </MobileWrapper>
  ),
};

// ============ ALL STEPS OVERVIEW ============

export const AllStepsDesktop: Story = {
  name: "All Steps - Desktop Overview",
  render: () => (
    <div className="space-y-8 p-4 bg-coffee-950">
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Step 1: Welcome
        </h3>
        <DesktopWrapper>
          <WelcomeStep />
        </DesktopWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Step 2: Scan QR Code
        </h3>
        <DesktopWrapper>
          <ScanStep onScan={() => {}} onBack={() => {}} onValidate={() => {}} />
        </DesktopWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Step 2 (Alt): Manual Entry
        </h3>
        <DesktopWrapper>
          <ManualStep
            claimCode="X6ST-AP3G"
            onClaimCodeChange={() => {}}
            onBack={() => {}}
            onValidate={() => {}}
          />
        </DesktopWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Step 3: Name Your Machine
        </h3>
        <DesktopWrapper>
          <MachineNameStep
            deviceName="Kitchen Espresso"
            onDeviceNameChange={() => {}}
            onBack={() => {}}
            onContinue={() => {}}
          />
        </DesktopWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Step 4: Success
        </h3>
        <DesktopWrapper>
          <SuccessStep deviceName="Kitchen Espresso" />
        </DesktopWrapper>
      </div>
    </div>
  ),
  parameters: {
    layout: "padded",
  },
};

export const AllStepsMobile: Story = {
  name: "All Steps - Mobile Overview",
  parameters: {
    viewport: { defaultViewport: "mobile1" },
    layout: "padded",
  },
  render: () => (
    <div className="space-y-8 bg-coffee-950">
      <div>
        <h3 className="text-cream-200 text-sm font-semibold mb-2 text-center px-4 pt-4">
          Step 1: Welcome
        </h3>
        <MobileWrapper>
          <WelcomeStep />
        </MobileWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-sm font-semibold mb-2 text-center px-4">
          Step 2: Scan QR Code
        </h3>
        <MobileWrapper>
          <ScanStep onScan={() => {}} onBack={() => {}} onValidate={() => {}} />
        </MobileWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-sm font-semibold mb-2 text-center px-4">
          Step 2 (Alt): Manual Entry
        </h3>
        <MobileWrapper>
          <ManualStep
            claimCode="X6ST-AP3G"
            onClaimCodeChange={() => {}}
            onBack={() => {}}
            onValidate={() => {}}
          />
        </MobileWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-sm font-semibold mb-2 text-center px-4">
          Step 3: Name Your Machine
        </h3>
        <MobileWrapper>
          <MachineNameStep
            deviceName="Kitchen Espresso"
            onDeviceNameChange={() => {}}
            onBack={() => {}}
            onContinue={() => {}}
          />
        </MobileWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-sm font-semibold mb-2 text-center px-4">
          Step 4: Success
        </h3>
        <MobileWrapper>
          <SuccessStep deviceName="Kitchen Espresso" />
        </MobileWrapper>
      </div>
    </div>
  ),
};
