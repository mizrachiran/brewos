import type { Meta, StoryObj } from "@storybook/react";
import { PairView } from "./Pair";
import { OnboardingLayout } from "@/components/onboarding";

const meta = {
  title: "Pages/Pair",
  component: PairView,
  tags: ["autodocs"],
  parameters: {
    layout: "fullscreen",
  },
  decorators: [
    (Story) => (
      <OnboardingLayout
        gradient="bg-gradient-to-br from-coffee-800 to-coffee-900"
        maxWidth="max-w-md"
      >
        <Story />
      </OnboardingLayout>
    ),
  ],
} satisfies Meta<typeof PairView>;

export default meta;
type Story = StoryObj<typeof meta>;

// Mock Google Login button for stories
function MockGoogleLogin() {
  return (
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
  );
}

// Shared no-op handlers for stories
const noopHandlers = {
  onDeviceNameChange: () => {},
  onPair: () => {},
  onRetry: () => {},
  onGoToMachines: () => {},
};

export const NotLoggedIn: Story = {
  args: {
    deviceId: "BREW-ABC123",
    deviceName: "",
    status: "idle",
    errorMessage: "",
    isShareLink: false,
    isLoggedIn: false,
    loginComponent: <MockGoogleLogin />,
    ...noopHandlers,
  },
};

export const LoggedIn: Story = {
  args: {
    deviceId: "BREW-ABC123",
    deviceName: "Kitchen Espresso",
    status: "idle",
    errorMessage: "",
    isShareLink: false,
    isLoggedIn: true,
    ...noopHandlers,
  },
};

export const ShareLink: Story = {
  args: {
    deviceId: "BREW-XYZ789",
    deviceName: "",
    status: "idle",
    errorMessage: "",
    isShareLink: true,
    isLoggedIn: true,
    ...noopHandlers,
  },
};

export const Claiming: Story = {
  args: {
    deviceId: "BREW-ABC123",
    deviceName: "Kitchen Espresso",
    status: "claiming",
    errorMessage: "",
    isShareLink: false,
    isLoggedIn: true,
    ...noopHandlers,
  },
};

export const Success: Story = {
  args: {
    deviceId: "BREW-ABC123",
    deviceName: "Kitchen Espresso",
    status: "success",
    errorMessage: "",
    isShareLink: false,
    isLoggedIn: true,
    ...noopHandlers,
  },
};

export const Error: Story = {
  args: {
    deviceId: "BREW-ABC123",
    deviceName: "Kitchen Espresso",
    status: "error",
    errorMessage: "Failed to pair device. The code may have expired.",
    isShareLink: false,
    isLoggedIn: true,
    ...noopHandlers,
  },
};
