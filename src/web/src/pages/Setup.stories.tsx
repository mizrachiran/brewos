import type { Meta, StoryObj } from "@storybook/react";
import { StoryPageWrapper } from "@/components/storybook";
import {
  SetupHeader,
  NetworkList,
  ConnectingView,
  SuccessView,
  type Network,
} from "@/components/setup";

// Wrapper component for stories
function SetupStoryWrapper({ children }: { children: React.ReactNode }) {
  return <>{children}</>;
}

const meta = {
  title: "Pages/Onboarding/Setup",
  component: SetupStoryWrapper,
  tags: ["autodocs"],
  parameters: {
    layout: "fullscreen",
  },
} satisfies Meta<typeof SetupStoryWrapper>;

export default meta;
type Story = StoryObj<typeof meta>;

// Mock network data
const mockNetworks: Network[] = [
  { ssid: "Home WiFi", rssi: -40, secure: true },
  { ssid: "Office Network", rssi: -55, secure: true },
  { ssid: "Guest Network", rssi: -65, secure: false },
  { ssid: "Neighbor's WiFi", rssi: -80, secure: true },
];

export const Default: Story = {
  render: () => (
    <StoryPageWrapper
      className="bg-gradient-to-br from-coffee-800 to-coffee-900"
      cardClassName="w-full max-w-md"
    >
      <SetupHeader />
      <NetworkList networks={mockNetworks} />
    </StoryPageWrapper>
  ),
};

export const NetworkSelected: Story = {
  render: () => (
    <StoryPageWrapper
      className="bg-gradient-to-br from-coffee-800 to-coffee-900"
      cardClassName="w-full max-w-md"
    >
      <SetupHeader />
      <NetworkList networks={mockNetworks} selectedSsid="Home WiFi" />
    </StoryPageWrapper>
  ),
};

export const Scanning: Story = {
  render: () => (
    <StoryPageWrapper
      className="bg-gradient-to-br from-coffee-800 to-coffee-900"
      cardClassName="w-full max-w-md"
    >
      <SetupHeader />
      <NetworkList networks={[]} scanning={true} />
    </StoryPageWrapper>
  ),
};

export const NoNetworks: Story = {
  render: () => (
    <StoryPageWrapper
      className="bg-gradient-to-br from-coffee-800 to-coffee-900"
      cardClassName="w-full max-w-md"
    >
      <SetupHeader />
      <NetworkList networks={[]} />
    </StoryPageWrapper>
  ),
};

export const ConnectionError: Story = {
  render: () => (
    <StoryPageWrapper
      className="bg-gradient-to-br from-coffee-800 to-coffee-900"
      cardClassName="w-full max-w-md"
    >
      <SetupHeader />
      <NetworkList
        networks={mockNetworks}
        selectedSsid="Home WiFi"
        error="Connection failed. Please check your password and try again."
      />
    </StoryPageWrapper>
  ),
};

export const Connecting: Story = {
  render: () => (
    <StoryPageWrapper
      className="bg-gradient-to-br from-coffee-800 to-coffee-900"
      cardClassName="w-full max-w-md"
    >
      <SetupHeader />
      <ConnectingView ssid="Home WiFi" />
    </StoryPageWrapper>
  ),
};

export const Success: Story = {
  render: () => (
    <StoryPageWrapper
      className="bg-gradient-to-br from-coffee-800 to-coffee-900"
      cardClassName="w-full max-w-md"
    >
      <SetupHeader />
      <SuccessView />
    </StoryPageWrapper>
  ),
};

export const AllStates: Story = {
  render: () => (
    <div className="space-y-8 p-4 bg-coffee-900">
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Network List
        </h3>
        <StoryPageWrapper
          className="bg-gradient-to-br from-coffee-800 to-coffee-900"
          cardClassName="w-full max-w-md"
        >
          <SetupHeader />
          <NetworkList networks={mockNetworks} />
        </StoryPageWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Network Selected
        </h3>
        <StoryPageWrapper
          className="bg-gradient-to-br from-coffee-800 to-coffee-900"
          cardClassName="w-full max-w-md"
        >
          <SetupHeader />
          <NetworkList networks={mockNetworks} selectedSsid="Home WiFi" />
        </StoryPageWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Scanning
        </h3>
        <StoryPageWrapper
          className="bg-gradient-to-br from-coffee-800 to-coffee-900"
          cardClassName="w-full max-w-md"
        >
          <SetupHeader />
          <NetworkList networks={[]} scanning={true} />
        </StoryPageWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Connecting
        </h3>
        <StoryPageWrapper
          className="bg-gradient-to-br from-coffee-800 to-coffee-900"
          cardClassName="w-full max-w-md"
        >
          <SetupHeader />
          <ConnectingView ssid="Home WiFi" />
        </StoryPageWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Success
        </h3>
        <StoryPageWrapper
          className="bg-gradient-to-br from-coffee-800 to-coffee-900"
          cardClassName="w-full max-w-md"
        >
          <SetupHeader />
          <SuccessView />
        </StoryPageWrapper>
      </div>
    </div>
  ),
  parameters: {
    layout: "padded",
  },
};

