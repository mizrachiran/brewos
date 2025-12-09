import type { Meta, StoryObj } from "@storybook/react";
import { Card } from "@/components/Card";
import { Button } from "@/components/Button";
import { Input } from "@/components/Input";
import { Logo } from "@/components/Logo";
import { Check, X, Loader2, Share2 } from "lucide-react";
import React, { useState, useEffect } from "react";
import { useMobileLandscape } from "@/lib/useMobileLandscape";

// Wrapper component for stories
function PairStoryWrapper({ children }: { children?: React.ReactNode }) {
  return <>{children}</>;
}

const meta = {
  title: "Pages/Pair",
  component: PairStoryWrapper,
  tags: ["autodocs"],
  parameters: {
    layout: "fullscreen",
  },
} satisfies Meta<typeof PairStoryWrapper>;

export default meta;
type Story = StoryObj<typeof meta>;

// Responsive wrapper - mirrors OnboardingLayout behavior
function PairWrapper({ children }: { children: React.ReactNode }) {
  const isMobileLandscape = useMobileLandscape();
  const [isWideScreen, setIsWideScreen] = useState(() =>
    typeof window !== "undefined" ? window.innerWidth >= 390 : false
  );

  useEffect(() => {
    const widthQuery = window.matchMedia("(min-width: 390px)");
    const handleChange = (e: MediaQueryListEvent | MediaQueryList) =>
      setIsWideScreen(e.matches);
    handleChange(widthQuery);
    widthQuery.addEventListener("change", handleChange);
    return () => widthQuery.removeEventListener("change", handleChange);
  }, []);

  if (isMobileLandscape) {
    return (
      <div className="h-[100dvh] overflow-hidden bg-theme flex items-center justify-center p-3">
        <Card className="w-full h-full flex flex-col animate-in fade-in slide-in-from-bottom-4 duration-300 overflow-hidden">
          <div className="flex-1 flex items-center justify-center">
            {children}
          </div>
        </Card>
      </div>
    );
  }

  if (isWideScreen) {
    return (
      <div className="min-h-[100dvh] bg-theme flex justify-center items-center p-4">
        <div className="w-full max-w-md">
          <Card className="animate-in fade-in slide-in-from-bottom-4 duration-300">
            {children}
          </Card>
        </div>
      </div>
    );
  }

  // Mobile portrait - dark gradient full screen, content at top
  return (
    <div className="min-h-[100dvh] bg-gradient-to-br from-coffee-800 to-coffee-900 flex flex-col px-4 pt-2 pb-4">
      <div className="flex-1 flex flex-col">{children}</div>
    </div>
  );
}

// === IDLE STATE: Not Logged In ===
function PairNotLoggedInContent({ deviceId }: { deviceId: string }) {
  const isMobileLandscape = useMobileLandscape();

  if (isMobileLandscape) {
    return (
      <div className="flex gap-8 items-center py-2 w-full">
        {/* Left: Branding */}
        <div className="flex-1 flex flex-col items-center text-center">
          <Logo size="lg" className="mb-3" />
          <h1 className="text-2xl font-bold text-theme">Pair Device</h1>
          <p className="text-base text-theme-muted mt-2 max-w-[250px]">
            Add this BrewOS device to your account
          </p>
        </div>
        {/* Right: Form */}
        <div className="flex-1 space-y-4">
          <div className="bg-theme-secondary/50 rounded-xl p-4">
            <div className="flex items-center justify-between text-sm">
              <span className="text-theme-muted">Machine ID</span>
              <span className="font-mono text-theme">{deviceId}</span>
            </div>
          </div>
          <Input label="Device Name" placeholder="Kitchen Espresso" value="" />
          <div className="space-y-3">
            <p className="text-sm text-theme-muted text-center">
              Sign in to add this device
            </p>
            <div className="flex justify-center">
              <button className="flex items-center gap-3 px-6 py-3 bg-white border border-gray-300 rounded-lg shadow-sm">
                <GoogleIcon />
                <span className="text-gray-700 font-medium">
                  Continue with Google
                </span>
              </button>
            </div>
          </div>
        </div>
      </div>
    );
  }

  return (
    <div className="pb-3 xs:pb-6">
      <div className="text-center mb-5 xs:mb-6">
        <div className="flex justify-center mb-4">
          <Logo size="md" forceLight className="xs:hidden" />
          <Logo size="lg" className="hidden xs:flex" />
        </div>
        <h1 className="text-xl xs:text-2xl font-bold text-theme">
          Pair Device
        </h1>
        <p className="text-sm xs:text-base text-theme-secondary mt-2">
          Add this BrewOS device to your account
        </p>
      </div>
      <div className="bg-white/5 xs:bg-theme-tertiary rounded-xl p-4 mb-5 xs:mb-6">
        <div className="flex items-center justify-between text-sm">
          <span className="text-theme-secondary">Machine ID</span>
          <span className="font-mono text-theme">{deviceId}</span>
        </div>
      </div>
      <Input
        label="Device Name"
        placeholder="Kitchen Espresso"
        value=""
        className="mb-5 xs:mb-6"
      />
      <div className="space-y-4">
        <p className="text-sm text-theme-secondary text-center">
          Sign in to add this device to your account
        </p>
        <div className="flex justify-center">
          <button className="flex items-center gap-3 px-6 py-3 bg-white border border-gray-300 rounded-lg shadow-sm">
            <GoogleIcon />
            <span className="text-gray-700 font-medium">
              Continue with Google
            </span>
          </button>
        </div>
      </div>
    </div>
  );
}

// === IDLE STATE: Logged In ===
function PairLoggedInContent({
  deviceId,
  deviceName,
}: {
  deviceId: string;
  deviceName: string;
}) {
  const isMobileLandscape = useMobileLandscape();

  if (isMobileLandscape) {
    return (
      <div className="flex gap-8 items-center py-2 w-full">
        {/* Left: Branding */}
        <div className="flex-1 flex flex-col items-center text-center">
          <Logo size="lg" className="mb-3" />
          <h1 className="text-2xl font-bold text-theme">Pair Device</h1>
          <p className="text-base text-theme-muted mt-2 max-w-[250px]">
            Add this BrewOS device to your account
          </p>
        </div>
        {/* Right: Form */}
        <div className="flex-1 space-y-4">
          <div className="bg-theme-secondary/50 rounded-xl p-4">
            <div className="flex items-center justify-between text-sm">
              <span className="text-theme-muted">Machine ID</span>
              <span className="font-mono text-theme">{deviceId}</span>
            </div>
          </div>
          <Input
            label="Device Name"
            placeholder="Kitchen Espresso"
            value={deviceName}
          />
          <Button className="w-full py-3">Add to My Machines</Button>
        </div>
      </div>
    );
  }

  return (
    <div className="pb-3 xs:pb-6">
      <div className="text-center mb-5 xs:mb-6">
        <div className="flex justify-center mb-4">
          <Logo size="md" forceLight className="xs:hidden" />
          <Logo size="lg" className="hidden xs:flex" />
        </div>
        <h1 className="text-xl xs:text-2xl font-bold text-theme">
          Pair Device
        </h1>
        <p className="text-sm xs:text-base text-theme-secondary mt-2">
          Add this BrewOS device to your account
        </p>
      </div>
      <div className="bg-white/5 xs:bg-theme-tertiary rounded-xl p-4 mb-5 xs:mb-6">
        <div className="flex items-center justify-between text-sm">
          <span className="text-theme-secondary">Machine ID</span>
          <span className="font-mono text-theme">{deviceId}</span>
        </div>
      </div>
      <Input
        label="Device Name"
        placeholder="Kitchen Espresso"
        value={deviceName}
        className="mb-5 xs:mb-6"
      />
      <Button className="w-full">Add to My Machines</Button>
    </div>
  );
}

// === CLAIMING STATE ===
function PairClaimingContent() {
  const isMobileLandscape = useMobileLandscape();

  if (isMobileLandscape) {
    return (
      <div className="flex gap-8 items-center justify-center py-4">
        <Loader2 className="w-12 h-12 animate-spin text-accent" />
        <div>
          <h2 className="text-2xl font-bold text-theme">Adding Device...</h2>
          <p className="text-base text-theme-muted mt-1">
            Please wait while we connect your machine
          </p>
        </div>
      </div>
    );
  }

  return (
    <div className="text-center py-6 xs:py-10">
      <Loader2 className="w-10 h-10 xs:w-12 xs:h-12 animate-spin text-accent mx-auto mb-4" />
      <h2 className="text-lg xs:text-2xl font-bold text-theme mb-1 xs:mb-2">
        Adding Device...
      </h2>
      <p className="text-sm xs:text-base text-theme-muted">
        Please wait while we connect your machine
      </p>
    </div>
  );
}

// === SUCCESS STATE ===
function PairSuccessContent() {
  const isMobileLandscape = useMobileLandscape();

  if (isMobileLandscape) {
    return (
      <div className="flex gap-8 items-center py-2 w-full">
        {/* Left: Success Icon */}
        <div className="flex-1 flex flex-col items-center">
          <div className="w-20 h-20 bg-success-soft rounded-full flex items-center justify-center mb-3">
            <Check className="w-10 h-10 text-success" />
          </div>
        </div>
        {/* Right: Message */}
        <div className="flex-1 text-center">
          <h2 className="text-2xl font-bold text-theme mb-2">Device Paired!</h2>
          <p className="text-base text-theme-muted">
            Redirecting to your devices...
          </p>
          <div className="flex items-center justify-center gap-2 mt-4 text-theme-muted">
            <Loader2 className="w-4 h-4 animate-spin" />
            <span className="text-sm">Please wait...</span>
          </div>
        </div>
      </div>
    );
  }

  return (
    <div className="text-center py-4 xs:py-8">
      <div className="w-14 h-14 xs:w-20 xs:h-20 bg-success-soft rounded-full flex items-center justify-center mx-auto mb-3 xs:mb-4">
        <Check className="w-7 h-7 xs:w-10 xs:h-10 text-success" />
      </div>
      <h2 className="text-lg xs:text-2xl font-bold text-theme mb-1 xs:mb-2">
        Device Paired!
      </h2>
      <p className="text-sm xs:text-base text-theme-secondary">
        Redirecting to your devices...
      </p>
    </div>
  );
}

// === ERROR STATE ===
function PairErrorContent({ errorMessage }: { errorMessage: string }) {
  const isMobileLandscape = useMobileLandscape();

  if (isMobileLandscape) {
    return (
      <div className="flex gap-8 items-center py-2 w-full">
        {/* Left: Error Icon */}
        <div className="flex-1 flex flex-col items-center">
          <div className="w-20 h-20 bg-error-soft rounded-full flex items-center justify-center mb-3">
            <X className="w-10 h-10 text-error" />
          </div>
          <p className="text-sm text-theme-muted text-center max-w-[200px]">
            {errorMessage}
          </p>
        </div>
        {/* Right: Actions */}
        <div className="flex-1 flex flex-col items-center">
          <h2 className="text-2xl font-bold text-theme mb-4">Pairing Failed</h2>
          <div className="flex gap-3">
            <Button variant="secondary" size="sm">
              Go to Machines
            </Button>
            <Button size="sm">Try Again</Button>
          </div>
        </div>
      </div>
    );
  }

  return (
    <div className="text-center py-4 xs:py-8">
      <div className="w-14 h-14 xs:w-20 xs:h-20 bg-error-soft rounded-full flex items-center justify-center mx-auto mb-3 xs:mb-4">
        <X className="w-7 h-7 xs:w-10 xs:h-10 text-error" />
      </div>
      <h2 className="text-lg xs:text-2xl font-bold text-theme mb-1 xs:mb-2">
        Pairing Failed
      </h2>
      <p className="text-sm xs:text-base text-theme-secondary mb-4 xs:mb-6">
        {errorMessage}
      </p>
      <div className="flex gap-3 justify-center">
        <Button variant="secondary">Go to Machines</Button>
        <Button>Try Again</Button>
      </div>
    </div>
  );
}

// === SHARE LINK STATE ===
function PairShareLinkContent({ deviceId }: { deviceId: string }) {
  const isMobileLandscape = useMobileLandscape();

  if (isMobileLandscape) {
    return (
      <div className="flex gap-8 items-center py-2 w-full">
        {/* Left: Branding */}
        <div className="flex-1 flex flex-col items-center text-center">
          <Logo size="lg" className="mb-3" />
          <div className="flex items-center gap-2 mb-2">
            <Share2 className="w-4 h-4 text-accent" />
            <span className="text-sm text-accent font-medium">
              Shared with you
            </span>
          </div>
          <h1 className="text-2xl font-bold text-theme">Add Shared Device</h1>
          <p className="text-base text-theme-muted mt-2 max-w-[250px]">
            Someone shared access to their BrewOS machine with you
          </p>
        </div>
        {/* Right: Form */}
        <div className="flex-1 space-y-4">
          <div className="bg-theme-secondary/50 rounded-xl p-4">
            <div className="flex items-center justify-between text-sm">
              <span className="text-theme-muted">Machine ID</span>
              <span className="font-mono text-theme">{deviceId}</span>
            </div>
          </div>
          <Input label="Device Name" placeholder="Kitchen Espresso" value="" />
          <Button className="w-full py-3">Add to My Machines</Button>
        </div>
      </div>
    );
  }

  return (
    <div className="pb-3 xs:pb-6">
      <div className="text-center mb-5 xs:mb-6">
        <div className="flex justify-center mb-4">
          <Logo size="md" forceLight className="xs:hidden" />
          <Logo size="lg" className="hidden xs:flex" />
        </div>
        <div className="flex items-center justify-center gap-2 mb-3">
          <Share2 className="w-4 h-4 text-accent" />
          <span className="text-sm text-accent font-medium">
            Shared with you
          </span>
        </div>
        <h1 className="text-xl xs:text-2xl font-bold text-theme">
          Add Shared Device
        </h1>
        <p className="text-sm xs:text-base text-theme-secondary mt-2">
          Someone shared access to their BrewOS machine with you
        </p>
      </div>
      <div className="bg-white/5 xs:bg-theme-tertiary rounded-xl p-4 mb-5 xs:mb-6">
        <div className="flex items-center justify-between text-sm">
          <span className="text-theme-secondary">Machine ID</span>
          <span className="font-mono text-theme">{deviceId}</span>
        </div>
      </div>
      <Input
        label="Device Name"
        placeholder="Kitchen Espresso"
        value=""
        className="mb-5 xs:mb-6"
      />
      <Button className="w-full">Add to My Machines</Button>
    </div>
  );
}

// Google Icon SVG
function GoogleIcon() {
  return (
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
  );
}

export const NotLoggedIn: Story = {
  render: () => (
    <PairWrapper>
      <PairNotLoggedInContent deviceId="BREW-ABC123" />
    </PairWrapper>
  ),
};

export const LoggedIn: Story = {
  render: () => (
    <PairWrapper>
      <PairLoggedInContent
        deviceId="BREW-ABC123"
        deviceName="Kitchen Espresso"
      />
    </PairWrapper>
  ),
};

export const Claiming: Story = {
  render: () => (
    <PairWrapper>
      <PairClaimingContent />
    </PairWrapper>
  ),
};

export const Success: Story = {
  render: () => (
    <PairWrapper>
      <PairSuccessContent />
    </PairWrapper>
  ),
};

export const Error: Story = {
  render: () => (
    <PairWrapper>
      <PairErrorContent errorMessage="Failed to pair device. The code may have expired." />
    </PairWrapper>
  ),
};

export const ShareLink: Story = {
  render: () => (
    <PairWrapper>
      <PairShareLinkContent deviceId="BREW-XYZ789" />
    </PairWrapper>
  ),
};
