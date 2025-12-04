import type { Meta, StoryObj } from "@storybook/react";
import { Card } from "@/components/Card";
import { Logo } from "@/components/Logo";
import { AlertCircle } from "lucide-react";

// Since Login uses auth context and navigation, we create presentational stories
// that show the different visual states of the login page

// Wrapper component for stories
function LoginStoryWrapper({ children }: { children: React.ReactNode }) {
  return <>{children}</>;
}

const meta = {
  title: "Pages/Onboarding/Login",
  component: LoginStoryWrapper,
  tags: ["autodocs"],
  parameters: {
    layout: "fullscreen",
  },
} satisfies Meta<typeof LoginStoryWrapper>;

export default meta;
type Story = StoryObj<typeof meta>;

// Cloud login view
function LoginView({ error }: { error?: string }) {
  return (
    <div className="min-h-screen bg-gradient-to-br from-coffee-800 to-coffee-900 flex items-center justify-center p-4">
      <Card className="w-full max-w-md">
        <div className="text-center mb-8">
          <div className="flex justify-center mb-4">
            <Logo size="lg" />
          </div>
          <h1 className="text-2xl font-bold text-theme">Welcome to BrewOS</h1>
          <p className="text-theme-muted mt-2">
            Sign in to manage your espresso machines from anywhere
          </p>
        </div>

        {error && (
          <div className="mb-4 p-3 rounded-lg flex items-center gap-2 bg-error-soft border border-error text-error">
            <AlertCircle className="w-4 h-4 shrink-0" />
            <span className="text-sm">{error}</span>
          </div>
        )}

        <div className="space-y-4">
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

          <div className="relative">
            <div className="absolute inset-0 flex items-center">
              <div className="w-full border-t border-theme"></div>
            </div>
            <div className="relative flex justify-center text-sm">
              <span className="px-4 bg-[var(--card-bg)] text-theme-muted">or</span>
            </div>
          </div>

          <p className="text-center text-sm text-theme-muted">
            Connect locally at{" "}
            <a href="#" className="text-accent hover:underline">
              brewos.local
            </a>
          </p>
        </div>
      </Card>
    </div>
  );
}

// Unconfigured view
function LoginUnconfiguredView() {
  return (
    <div className="min-h-screen bg-gradient-to-br from-coffee-800 to-coffee-900 flex items-center justify-center p-4">
      <Card className="w-full max-w-md text-center">
        <div className="flex justify-center mb-4">
          <Logo size="lg" />
        </div>
        <p className="text-coffee-500">
          Cloud features are not configured. Connect directly to your device at{" "}
          <a href="#" className="text-accent hover:underline">
            brewos.local
          </a>
        </p>
      </Card>
    </div>
  );
}

export const Default: Story = {
  render: () => <LoginView />,
};

export const WithError: Story = {
  render: () => <LoginView error="Authentication failed. Please try again." />,
};

export const CloudNotConfigured: Story = {
  render: () => <LoginUnconfiguredView />,
};

