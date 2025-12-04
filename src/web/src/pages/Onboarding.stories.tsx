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

// Responsive wrapper - works for both mobile and desktop
function OnboardingWrapper({ children }: { children: React.ReactNode }) {
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

// ============ WELCOME STEP ============

export const Welcome: Story = {
  name: "1. Welcome",
  render: () => (
    <OnboardingWrapper>
      <WelcomeStep />
    </OnboardingWrapper>
  ),
};

// ============ SCAN STEP ============

export const Scan: Story = {
  name: "2. Scan QR",
  render: () => (
    <OnboardingWrapper>
      <ScanStep onScan={() => {}} onBack={() => {}} onValidate={() => {}} />
    </OnboardingWrapper>
  ),
};

export const ScanWithError: Story = {
  name: "2. Scan QR - Error",
  render: () => (
    <OnboardingWrapper>
      <ScanStep
        error="Invalid QR code. Please try again."
        onScan={() => {}}
        onBack={() => {}}
        onValidate={() => {}}
      />
    </OnboardingWrapper>
  ),
};

// ============ MANUAL STEP ============

export const Manual: Story = {
  name: "3. Manual Entry",
  render: () => (
    <OnboardingWrapper>
      <ManualStep
        claimCode="X6ST-AP3G"
        onClaimCodeChange={() => {}}
        onBack={() => {}}
        onValidate={() => {}}
      />
    </OnboardingWrapper>
  ),
};

export const ManualWithError: Story = {
  name: "3. Manual Entry - Error",
  render: () => (
    <OnboardingWrapper>
      <ManualStep
        claimCode="X6ST-AP3G"
        error="Invalid or expired code"
        onClaimCodeChange={() => {}}
        onBack={() => {}}
        onValidate={() => {}}
      />
    </OnboardingWrapper>
  ),
};

// ============ MACHINE NAME STEP ============

export const MachineName: Story = {
  name: "4. Machine Name",
  render: () => (
    <OnboardingWrapper>
      <MachineNameStep
        deviceName="Kitchen Espresso"
        onDeviceNameChange={() => {}}
        onBack={() => {}}
        onContinue={() => {}}
      />
    </OnboardingWrapper>
  ),
};

// ============ SUCCESS STEP ============

export const Success: Story = {
  name: "5. Success",
  render: () => (
    <OnboardingWrapper>
      <SuccessStep deviceName="Kitchen Espresso" />
    </OnboardingWrapper>
  ),
};

// ============ ALL STEPS OVERVIEW ============

export const AllSteps: Story = {
  name: "All Steps Overview",
  render: () => (
    <div className="space-y-8 p-4 bg-coffee-950">
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Step 1: Welcome
        </h3>
        <OnboardingWrapper>
          <WelcomeStep />
        </OnboardingWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Step 2: Scan QR Code
        </h3>
        <OnboardingWrapper>
          <ScanStep onScan={() => {}} onBack={() => {}} onValidate={() => {}} />
        </OnboardingWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Step 2 (Alt): Manual Entry
        </h3>
        <OnboardingWrapper>
          <ManualStep
            claimCode="X6ST-AP3G"
            onClaimCodeChange={() => {}}
            onBack={() => {}}
            onValidate={() => {}}
          />
        </OnboardingWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Step 3: Name Your Machine
        </h3>
        <OnboardingWrapper>
          <MachineNameStep
            deviceName="Kitchen Espresso"
            onDeviceNameChange={() => {}}
            onBack={() => {}}
            onContinue={() => {}}
          />
        </OnboardingWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Step 4: Success
        </h3>
        <OnboardingWrapper>
          <SuccessStep deviceName="Kitchen Espresso" />
        </OnboardingWrapper>
      </div>
    </div>
  ),
  parameters: {
    layout: "padded",
  },
};
