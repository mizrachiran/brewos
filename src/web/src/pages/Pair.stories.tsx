import type { Meta, StoryObj } from "@storybook/react";
import { Card } from "@/components/Card";
import { Button } from "@/components/Button";
import { Input } from "@/components/Input";
import { Logo } from "@/components/Logo";
import { Check, X, Loader2 } from "lucide-react";

// Wrapper component for stories
function PairStoryWrapper({ children }: { children: React.ReactNode }) {
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

// Idle state - waiting for user action
function PairIdleView({
  deviceId,
  deviceName,
  isLoggedIn,
}: {
  deviceId: string;
  deviceName: string;
  isLoggedIn: boolean;
}) {
  return (
    <div className="min-h-screen bg-gradient-to-br from-coffee-800 to-coffee-900 flex items-center justify-center p-4">
      <Card className="w-full max-w-md">
        <div className="text-center mb-6">
          <div className="flex justify-center mb-4">
            <Logo size="lg" />
          </div>
          <h1 className="text-2xl font-bold text-theme">Pair Device</h1>
          <p className="text-theme-secondary mt-2">
            Add this BrewOS device to your account
          </p>
        </div>

        <div className="bg-theme-tertiary rounded-xl p-4 mb-6">
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
            <p className="text-sm text-theme-secondary text-center">
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
      </Card>
    </div>
  );
}

// Claiming state
function PairClaimingView() {
  return (
    <div className="min-h-screen bg-gradient-to-br from-coffee-800 to-coffee-900 flex items-center justify-center p-4">
      <Card className="w-full max-w-md">
        <div className="text-center py-8">
          <Loader2 className="w-8 h-8 animate-spin text-accent mx-auto mb-4" />
          <h2 className="text-xl font-bold text-theme mb-2">Adding Device...</h2>
        </div>
      </Card>
    </div>
  );
}

// Success state
function PairSuccessView() {
  return (
    <div className="min-h-screen bg-gradient-to-br from-coffee-800 to-coffee-900 flex items-center justify-center p-4">
      <Card className="w-full max-w-md">
        <div className="text-center py-8">
          <div className="w-16 h-16 bg-success-soft rounded-full flex items-center justify-center mx-auto mb-4">
            <Check className="w-8 h-8 text-success" />
          </div>
          <h2 className="text-xl font-bold text-theme mb-2">Device Paired!</h2>
          <p className="text-theme-secondary">Redirecting to your devices...</p>
        </div>
      </Card>
    </div>
  );
}

// Error state
function PairErrorView({ errorMessage }: { errorMessage: string }) {
  return (
    <div className="min-h-screen bg-gradient-to-br from-coffee-800 to-coffee-900 flex items-center justify-center p-4">
      <Card className="w-full max-w-md">
        <div className="text-center py-8">
          <div className="w-16 h-16 bg-error-soft rounded-full flex items-center justify-center mx-auto mb-4">
            <X className="w-8 h-8 text-error" />
          </div>
          <h2 className="text-xl font-bold text-theme mb-2">Pairing Failed</h2>
          <p className="text-theme-secondary mb-6">{errorMessage}</p>
          <div className="flex gap-3 justify-center">
            <Button variant="secondary">Go to Machines</Button>
            <Button>Try Again</Button>
          </div>
        </div>
      </Card>
    </div>
  );
}

export const NotLoggedIn: Story = {
  render: () => (
    <PairIdleView
      deviceId="BREW-ABC123"
      deviceName=""
      isLoggedIn={false}
    />
  ),
};

export const LoggedIn: Story = {
  render: () => (
    <PairIdleView
      deviceId="BREW-ABC123"
      deviceName="Kitchen Espresso"
      isLoggedIn={true}
    />
  ),
};

export const Claiming: Story = {
  render: () => <PairClaimingView />,
};

export const Success: Story = {
  render: () => <PairSuccessView />,
};

export const Error: Story = {
  render: () => (
    <PairErrorView errorMessage="Failed to pair device. The code may have expired." />
  ),
};

export const AllStates: Story = {
  render: () => (
    <div className="space-y-8 p-4 bg-coffee-900">
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Not Logged In
        </h3>
        <PairIdleView deviceId="BREW-ABC123" deviceName="" isLoggedIn={false} />
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Logged In
        </h3>
        <PairIdleView deviceId="BREW-ABC123" deviceName="Kitchen Espresso" isLoggedIn={true} />
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Claiming
        </h3>
        <PairClaimingView />
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Success
        </h3>
        <PairSuccessView />
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Error
        </h3>
        <PairErrorView errorMessage="Failed to pair device. The code may have expired." />
      </div>
    </div>
  ),
  parameters: {
    layout: "padded",
  },
};

