import type { Meta, StoryObj } from "@storybook/react";
import { StoryPageWrapper } from "@/components/storybook";
import {
  HowItWorks,
  WelcomeStep,
  ScanStep,
  ManualStep,
  SuccessStep,
} from "@/components/onboarding";

// Wrapper component for stories
function OnboardingStoryWrapper({ children }: { children: React.ReactNode }) {
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
  render: () => (
    <div className="min-h-screen bg-gradient-to-br from-coffee-800 via-coffee-900 to-coffee-950 flex flex-col items-center justify-center p-4">
      <StoryPageWrapper cardClassName="text-center w-full max-w-lg">
        <WelcomeStep />
      </StoryPageWrapper>
      <div className="w-full max-w-lg mt-8">
        <HowItWorks />
      </div>
    </div>
  ),
};

export const ScanQRCode: Story = {
  render: () => (
    <div className="min-h-screen bg-gradient-to-br from-coffee-800 via-coffee-900 to-coffee-950 flex flex-col items-center justify-center p-4">
      <StoryPageWrapper cardClassName="w-full max-w-lg">
        <ScanStep />
      </StoryPageWrapper>
      <div className="w-full max-w-lg mt-8">
        <HowItWorks />
      </div>
    </div>
  ),
};

export const ScanWithError: Story = {
  render: () => (
    <div className="min-h-screen bg-gradient-to-br from-coffee-800 via-coffee-900 to-coffee-950 flex flex-col items-center justify-center p-4">
      <StoryPageWrapper cardClassName="w-full max-w-lg">
        <ScanStep error="Invalid QR code. Please try again." />
      </StoryPageWrapper>
      <div className="w-full max-w-lg mt-8">
        <HowItWorks />
      </div>
    </div>
  ),
};

export const ManualEntry: Story = {
  render: () => (
    <StoryPageWrapper cardClassName="w-full max-w-lg">
      <ManualStep />
    </StoryPageWrapper>
  ),
};

export const ManualEntryWithError: Story = {
  render: () => (
    <StoryPageWrapper cardClassName="w-full max-w-lg">
      <ManualStep error="Invalid or expired code" />
    </StoryPageWrapper>
  ),
};

export const Success: Story = {
  render: () => (
    <StoryPageWrapper cardClassName="text-center py-12 w-full max-w-lg">
      <SuccessStep deviceName="Kitchen Espresso" />
    </StoryPageWrapper>
  ),
};

export const AllSteps: Story = {
  render: () => (
    <div className="space-y-8 p-4 bg-coffee-900">
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Step 1: Welcome
        </h3>
        <div className="min-h-screen bg-gradient-to-br from-coffee-800 via-coffee-900 to-coffee-950 flex flex-col items-center justify-center p-4">
          <StoryPageWrapper cardClassName="text-center w-full max-w-lg">
            <WelcomeStep />
          </StoryPageWrapper>
          <div className="w-full max-w-lg mt-8">
            <HowItWorks />
          </div>
        </div>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Step 2: Scan QR Code
        </h3>
        <div className="min-h-screen bg-gradient-to-br from-coffee-800 via-coffee-900 to-coffee-950 flex flex-col items-center justify-center p-4">
          <StoryPageWrapper cardClassName="w-full max-w-lg">
            <ScanStep />
          </StoryPageWrapper>
          <div className="w-full max-w-lg mt-8">
            <HowItWorks />
          </div>
        </div>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Step 2 (Alt): Manual Entry
        </h3>
        <StoryPageWrapper cardClassName="w-full max-w-lg">
          <ManualStep />
        </StoryPageWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Step 3: Success
        </h3>
        <StoryPageWrapper cardClassName="text-center py-12 w-full max-w-lg">
          <SuccessStep deviceName="Kitchen Espresso" />
        </StoryPageWrapper>
      </div>
    </div>
  ),
  parameters: {
    layout: "padded",
  },
};
