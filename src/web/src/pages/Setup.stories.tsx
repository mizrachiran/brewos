import type { Meta, StoryObj } from "@storybook/react";
import { Card } from "@/components/Card";
import {
  SetupHeader,
  NetworkList,
  ConnectingView,
  SuccessView,
  type Network,
} from "@/components/setup";
import { useState, useEffect } from "react";

// Wrapper component for stories
function SetupStoryWrapper({ children }: { children?: React.ReactNode }) {
  return <>{children}</>;
}

const meta = {
  title: "Pages/Setup",
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

// Responsive wrapper - works for both mobile and desktop with landscape detection
function SetupWrapper({ children }: { children: React.ReactNode }) {
  const [isMobileLandscape, setIsMobileLandscape] = useState(() => {
    if (typeof window === "undefined") return false;
    return window.innerHeight <= 500 && window.innerWidth > window.innerHeight;
  });

  useEffect(() => {
    const landscapeQuery = window.matchMedia(
      "(orientation: landscape) and (max-height: 500px)"
    );
    const handleChange = (e: MediaQueryListEvent | MediaQueryList) =>
      setIsMobileLandscape(e.matches);
    handleChange(landscapeQuery);
    landscapeQuery.addEventListener("change", handleChange);
    return () => landscapeQuery.removeEventListener("change", handleChange);
  }, []);

  if (isMobileLandscape) {
    return (
      <div className="min-h-[100dvh] bg-theme overflow-y-auto p-4">
        <Card className="max-w-2xl mx-auto animate-in fade-in slide-in-from-bottom-4 duration-300">
          <div className="py-2">
            {children}
          </div>
        </Card>
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-gradient-to-br from-coffee-800 to-coffee-900 flex justify-center items-center p-4">
      <Card className="w-full max-w-md animate-in fade-in slide-in-from-bottom-4 duration-300">
        <div className="py-4 sm:py-6">
          {children}
        </div>
      </Card>
    </div>
  );
}

export const Default: Story = {
  render: () => (
    <SetupWrapper>
      <SetupHeader />
      <NetworkList networks={mockNetworks} />
    </SetupWrapper>
  ),
};

export const NetworkSelected: Story = {
  render: () => (
    <SetupWrapper>
      <SetupHeader />
      <NetworkList networks={mockNetworks} selectedSsid="Home WiFi" />
    </SetupWrapper>
  ),
};

export const Scanning: Story = {
  render: () => (
    <SetupWrapper>
      <SetupHeader />
      <NetworkList networks={[]} scanning={true} />
    </SetupWrapper>
  ),
};

export const NoNetworks: Story = {
  render: () => (
    <SetupWrapper>
      <SetupHeader />
      <NetworkList networks={[]} />
    </SetupWrapper>
  ),
};

export const ConnectionError: Story = {
  render: () => (
    <SetupWrapper>
      <SetupHeader />
      <NetworkList
        networks={mockNetworks}
        selectedSsid="Home WiFi"
        error="Connection failed. Please check your password and try again."
      />
    </SetupWrapper>
  ),
};

export const Connecting: Story = {
  render: () => (
    <SetupWrapper>
      <SetupHeader />
      <ConnectingView ssid="Home WiFi" />
    </SetupWrapper>
  ),
};

export const Success: Story = {
  render: () => (
    <SetupWrapper>
      <SetupHeader />
      <SuccessView />
    </SetupWrapper>
  ),
};

export const AllStates: Story = {
  name: "All States",
  render: () => (
    <div className="space-y-8 p-4 bg-coffee-950">
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Network List
        </h3>
        <SetupWrapper>
          <SetupHeader />
          <NetworkList networks={mockNetworks} />
        </SetupWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Network Selected
        </h3>
        <SetupWrapper>
          <SetupHeader />
          <NetworkList networks={mockNetworks} selectedSsid="Home WiFi" />
        </SetupWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Scanning
        </h3>
        <SetupWrapper>
          <SetupHeader />
          <NetworkList networks={[]} scanning={true} />
        </SetupWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Connecting
        </h3>
        <SetupWrapper>
          <SetupHeader />
          <ConnectingView ssid="Home WiFi" />
        </SetupWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Success
        </h3>
        <SetupWrapper>
          <SetupHeader />
          <SuccessView />
        </SetupWrapper>
      </div>
    </div>
  ),
  parameters: {
    layout: "padded",
  },
};
