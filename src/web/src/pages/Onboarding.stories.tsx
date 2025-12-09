import type { Meta, StoryObj } from "@storybook/react";
import {
  WelcomeStep,
  ScanStep,
  ManualStep,
  MachineNameStep,
  SuccessStep,
  OnboardingLayout,
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

export const Welcome: Story = {
  name: "1. Welcome",
  render: () => (
    <OnboardingLayout>
      <WelcomeStep />
    </OnboardingLayout>
  ),
};

export const Scan: Story = {
  name: "2. Scan QR",
  render: () => (
    <OnboardingLayout>
      <ScanStep onScan={() => {}} onBack={() => {}} onValidate={() => {}} />
    </OnboardingLayout>
  ),
};

export const ScanWithError: Story = {
  name: "2. Scan QR (Error)",
  render: () => (
    <OnboardingLayout>
      <ScanStep
        error="Invalid QR code. Please try again."
        onScan={() => {}}
        onBack={() => {}}
        onValidate={() => {}}
      />
    </OnboardingLayout>
  ),
};

export const Manual: Story = {
  name: "3. Manual Entry",
  render: () => (
    <OnboardingLayout>
      <ManualStep
        claimCode="X6ST-AP3G"
        onClaimCodeChange={() => {}}
        onBack={() => {}}
        onValidate={() => {}}
      />
    </OnboardingLayout>
  ),
};

export const ManualWithError: Story = {
  name: "3. Manual Entry (Error)",
  render: () => (
    <OnboardingLayout>
      <ManualStep
        claimCode="X6ST-AP3G"
        error="Invalid or expired code"
        onClaimCodeChange={() => {}}
        onBack={() => {}}
        onValidate={() => {}}
      />
    </OnboardingLayout>
  ),
};

export const MachineName: Story = {
  name: "4. Machine Name",
  render: () => (
    <OnboardingLayout>
      <MachineNameStep
        deviceName="Kitchen Espresso"
        onDeviceNameChange={() => {}}
        onBack={() => {}}
        onContinue={() => {}}
      />
    </OnboardingLayout>
  ),
};

export const Success: Story = {
  name: "5. Success",
  render: () => (
    <OnboardingLayout>
      <SuccessStep deviceName="Kitchen Espresso" />
    </OnboardingLayout>
  ),
};
