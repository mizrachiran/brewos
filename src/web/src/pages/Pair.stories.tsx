import type { Meta, StoryObj } from "@storybook/react";
import { Card } from "@/components/Card";
import { Button } from "@/components/Button";
import { Input } from "@/components/Input";
import { Logo } from "@/components/Logo";
import { Check, X, Loader2 } from "lucide-react";
import React from "react";

// Wrapper component for stories
function PairStoryWrapper({ children }: { children?: React.ReactNode }) {
  return <>{children}</>;
}

const meta = {
  title: "Pages/Onboarding/Pair",
  component: PairStoryWrapper,
  tags: ["autodocs"],
  parameters: {
    layout: "fullscreen",
  },
} satisfies Meta<typeof PairStoryWrapper>;

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
    <div className="min-h-screen bg-gradient-to-br from-coffee-800 to-coffee-900 flex justify-center items-center p-4">
      <Card className="w-full max-w-md animate-in fade-in slide-in-from-bottom-4 duration-300">
        {children}
      </Card>
    </div>
  );
}

// Mobile wrapper without Card (full-screen)
function MobileWrapper({ children }: { children: React.ReactNode }) {
  return (
    <div className="min-h-screen bg-gradient-to-br from-coffee-800 to-coffee-900">
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

// Idle state - waiting for user action
function PairIdleContent({
  deviceId,
  deviceName,
  isLoggedIn,
}: {
  deviceId: string;
  deviceName: string;
  isLoggedIn: boolean;
}) {
  return (
    <div className="py-4 sm:py-6">
      <div className="text-center mb-6">
        <div className="flex justify-center mb-4">
          {/* Mobile: force light text for dark background */}
          <Logo size="lg" forceLight className="sm:hidden" />
          {/* Desktop: use theme colors */}
          <Logo size="lg" className="hidden sm:flex" />
        </div>
        <h1 className="text-xl sm:text-2xl font-bold text-theme">Pair Device</h1>
        <p className="text-sm sm:text-base text-theme-secondary mt-2">
          Add this BrewOS device to your account
        </p>
      </div>

      <div className="bg-theme-tertiary rounded-xl p-3 sm:p-4 mb-6">
        <div className="flex items-center justify-between text-sm">
          <span className="text-theme-secondary">Machine ID</span>
          <span className="font-mono text-theme">{deviceId}</span>
        </div>
      </div>

      <Input
        label="Device Name"
        placeholder="Kitchen Espresso"
        value={deviceName}
        className="mb-6"
      />

      {isLoggedIn ? (
        <Button className="w-full">Add to My Machines</Button>
      ) : (
        <div className="space-y-4">
          <p className="text-xs sm:text-sm text-theme-secondary text-center">
            Sign in to add this device to your account
          </p>
          <div className="flex justify-center">
            {/* Mock Google Login Button */}
            <button className="flex items-center gap-3 px-6 py-3 bg-white border border-gray-300 rounded-lg shadow-sm hover:shadow-md transition-shadow">
              <svg className="w-5 h-5" viewBox="0 0 24 24">
                <path
                  fill="#4285F4"
                  d="M22.56 12.25c0-.78-.07-1.53-.2-2.25H12v4.26h5.92c-.26 1.37-1.04 2.53-2.21 3.31v2.77h3.57c2.08-1.92 3.28-4.74 3.28-8.09z"
                />
                <path
                  fill="#34A853"
                  d="M12 23c2.97 0 5.46-.98 7.28-2.66l-3.57-2.77c-.98.66-2.23 1.06-3.71 1.06-2.86 0-5.29-1.93-6.16-4.53H2.18v2.84C3.99 20.53 7.7 23 12 23z"
                />
                <path
                  fill="#FBBC05"
                  d="M5.84 14.09c-.22-.66-.35-1.36-.35-2.09s.13-1.43.35-2.09V7.07H2.18C1.43 8.55 1 10.22 1 12s.43 3.45 1.18 4.93l2.85-2.22.81-.62z"
                />
                <path
                  fill="#EA4335"
                  d="M12 5.38c1.62 0 3.06.56 4.21 1.64l3.15-3.15C17.45 2.09 14.97 1 12 1 7.7 1 3.99 3.47 2.18 7.07l3.66 2.84c.87-2.6 3.3-4.53 6.16-4.53z"
                />
              </svg>
              <span className="text-gray-700 font-medium">Continue with Google</span>
            </button>
          </div>
        </div>
      )}
    </div>
  );
}

// Claiming state
function PairClaimingContent() {
  return (
    <div className="text-center py-6 sm:py-8">
      <Loader2 className="w-7 h-7 sm:w-8 sm:h-8 animate-spin text-accent mx-auto mb-4" />
      <h2 className="text-lg sm:text-xl font-bold text-theme mb-2">Adding Device...</h2>
    </div>
  );
}

// Success state
function PairSuccessContent() {
  return (
    <div className="text-center py-6 sm:py-8">
      <div className="w-14 h-14 sm:w-16 sm:h-16 bg-success-soft rounded-full flex items-center justify-center mx-auto mb-4">
        <Check className="w-7 h-7 sm:w-8 sm:h-8 text-success" />
      </div>
      <h2 className="text-lg sm:text-xl font-bold text-theme mb-2">Device Paired!</h2>
      <p className="text-sm sm:text-base text-theme-secondary">Redirecting to your devices...</p>
    </div>
  );
}

// Error state
function PairErrorContent({ errorMessage }: { errorMessage: string }) {
  return (
    <div className="text-center py-6 sm:py-8">
      <div className="w-14 h-14 sm:w-16 sm:h-16 bg-error-soft rounded-full flex items-center justify-center mx-auto mb-4">
        <X className="w-7 h-7 sm:w-8 sm:h-8 text-error" />
      </div>
      <h2 className="text-lg sm:text-xl font-bold text-theme mb-2">Pairing Failed</h2>
      <p className="text-sm sm:text-base text-theme-secondary mb-6">{errorMessage}</p>
      <div className="flex gap-3 justify-center">
        <Button variant="secondary">Go to Machines</Button>
        <Button>Try Again</Button>
      </div>
    </div>
  );
}

// ============ DESKTOP STORIES ============

export const NotLoggedInDesktop: Story = {
  name: "Not Logged In - Desktop",
  render: () => (
    <DesktopWrapper>
      <PairIdleContent deviceId="BREW-ABC123" deviceName="" isLoggedIn={false} />
    </DesktopWrapper>
  ),
};

export const LoggedInDesktop: Story = {
  name: "Logged In - Desktop",
  render: () => (
    <DesktopWrapper>
      <PairIdleContent deviceId="BREW-ABC123" deviceName="Kitchen Espresso" isLoggedIn={true} />
    </DesktopWrapper>
  ),
};

export const ClaimingDesktop: Story = {
  name: "Claiming - Desktop",
  render: () => (
    <DesktopWrapper>
      <PairClaimingContent />
    </DesktopWrapper>
  ),
};

export const SuccessDesktop: Story = {
  name: "Success - Desktop",
  render: () => (
    <DesktopWrapper>
      <PairSuccessContent />
    </DesktopWrapper>
  ),
};

export const ErrorDesktop: Story = {
  name: "Error - Desktop",
  render: () => (
    <DesktopWrapper>
      <PairErrorContent errorMessage="Failed to pair device. The code may have expired." />
    </DesktopWrapper>
  ),
};

// ============ MOBILE STORIES ============

export const NotLoggedInMobile: Story = {
  name: "Not Logged In - Mobile",
  parameters: {
    viewport: { defaultViewport: "mobile1" },
  },
  render: () => (
    <MobileWrapper>
      <PairIdleContent deviceId="BREW-ABC123" deviceName="" isLoggedIn={false} />
    </MobileWrapper>
  ),
};

export const LoggedInMobile: Story = {
  name: "Logged In - Mobile",
  parameters: {
    viewport: { defaultViewport: "mobile1" },
  },
  render: () => (
    <MobileWrapper>
      <PairIdleContent deviceId="BREW-ABC123" deviceName="Kitchen Espresso" isLoggedIn={true} />
    </MobileWrapper>
  ),
};

export const SuccessMobile: Story = {
  name: "Success - Mobile",
  parameters: {
    viewport: { defaultViewport: "mobile1" },
  },
  render: () => (
    <MobileWrapper>
      <PairSuccessContent />
    </MobileWrapper>
  ),
};

export const ErrorMobile: Story = {
  name: "Error - Mobile",
  parameters: {
    viewport: { defaultViewport: "mobile1" },
  },
  render: () => (
    <MobileWrapper>
      <PairErrorContent errorMessage="Failed to pair device. The code may have expired." />
    </MobileWrapper>
  ),
};

// ============ LEGACY ALIASES (for backward compatibility) ============

export const NotLoggedIn = NotLoggedInDesktop;
export const LoggedIn = LoggedInDesktop;
export const Claiming = ClaimingDesktop;
export const Success = SuccessDesktop;
export const Error = ErrorDesktop;

// ============ ALL STATES ============

export const AllStates: Story = {
  name: "All States",
  render: () => (
    <div className="space-y-8 p-4 bg-coffee-950">
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Not Logged In
        </h3>
        <DesktopWrapper>
          <PairIdleContent deviceId="BREW-ABC123" deviceName="" isLoggedIn={false} />
        </DesktopWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Logged In
        </h3>
        <DesktopWrapper>
          <PairIdleContent deviceId="BREW-ABC123" deviceName="Kitchen Espresso" isLoggedIn={true} />
        </DesktopWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Claiming
        </h3>
        <DesktopWrapper>
          <PairClaimingContent />
        </DesktopWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Success
        </h3>
        <DesktopWrapper>
          <PairSuccessContent />
        </DesktopWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Error
        </h3>
        <DesktopWrapper>
          <PairErrorContent errorMessage="Failed to pair device. The code may have expired." />
        </DesktopWrapper>
      </div>
    </div>
  ),
  parameters: {
    layout: "padded",
  },
};
