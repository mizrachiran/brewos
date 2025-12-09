import type { Meta, StoryObj } from "@storybook/react";
import { SetupView, Network } from "./Setup";
import { OnboardingLayout } from "@/components/onboarding";

const meta = {
  title: "Pages/Setup",
  component: SetupView,
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
} satisfies Meta<typeof SetupView>;

export default meta;
type Story = StoryObj<typeof meta>;

// Mock network data
const mockNetworks: Network[] = [
  { ssid: "Home WiFi", rssi: -40, secure: true },
  { ssid: "Office Network", rssi: -55, secure: true },
  { ssid: "Guest Network", rssi: -65, secure: false },
  { ssid: "Neighbor's WiFi", rssi: -80, secure: true },
];

// Shared no-op handlers for stories
const noopHandlers = {
  onScan: () => {},
  onSelectNetwork: () => {},
  onPasswordChange: () => {},
  onConnect: () => {},
};

export const Default: Story = {
  args: {
    networks: mockNetworks,
    scanning: false,
    selectedSsid: "",
    password: "",
    connecting: false,
    status: "idle",
    errorMessage: "",
    ...noopHandlers,
  },
};

export const NetworkSelected: Story = {
  args: {
    networks: mockNetworks,
    scanning: false,
    selectedSsid: "Home WiFi",
    password: "",
    connecting: false,
    status: "idle",
    errorMessage: "",
    ...noopHandlers,
  },
};

export const Scanning: Story = {
  args: {
    networks: [],
    scanning: true,
    selectedSsid: "",
    password: "",
    connecting: false,
    status: "idle",
    errorMessage: "",
    ...noopHandlers,
  },
};

export const Connecting: Story = {
  args: {
    networks: mockNetworks,
    scanning: false,
    selectedSsid: "Home WiFi",
    password: "mypassword",
    connecting: true,
    status: "idle",
    errorMessage: "",
    ...noopHandlers,
  },
};

export const Success: Story = {
  args: {
    networks: mockNetworks,
    scanning: false,
    selectedSsid: "Home WiFi",
    password: "mypassword",
    connecting: false,
    status: "success",
    errorMessage: "",
    ...noopHandlers,
  },
};

export const Error: Story = {
  args: {
    networks: mockNetworks,
    scanning: false,
    selectedSsid: "Home WiFi",
    password: "wrongpassword",
    connecting: false,
    status: "error",
    errorMessage:
      "Connection failed. Please check your password and try again.",
    ...noopHandlers,
  },
};
