import type { Meta, StoryObj } from "@storybook/react";
import { Card } from "@/components/Card";
import {
  SetupHeader,
  NetworkList,
  ConnectingView,
  SuccessView,
  type Network,
} from "@/components/setup";
import React from "react";
import { darkBgStyles } from "@/lib/darkBgStyles";

// Wrapper component for stories
function SetupStoryWrapper({ children }: { children?: React.ReactNode }) {
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

// Desktop wrapper with Card
function DesktopWrapper({ children }: { children: React.ReactNode }) {
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

// Mobile wrapper without Card (full-screen)
function MobileWrapper({ children }: { children: React.ReactNode }) {
  return (
    <div className="min-h-screen bg-gradient-to-br from-coffee-800 to-coffee-900">
      <div 
        className="min-h-screen flex flex-col justify-center px-5 py-8"
        style={darkBgStyles}
      >
        <div className="animate-in fade-in slide-in-from-bottom-4 duration-500">
          <div className="py-4">
            {children}
          </div>
        </div>
      </div>
    </div>
  );
}

// ============ DESKTOP STORIES ============

export const DefaultDesktop: Story = {
  name: "Default - Desktop",
  render: () => (
    <DesktopWrapper>
      <SetupHeader />
      <NetworkList networks={mockNetworks} />
    </DesktopWrapper>
  ),
};

export const NetworkSelectedDesktop: Story = {
  name: "Network Selected - Desktop",
  render: () => (
    <DesktopWrapper>
      <SetupHeader />
      <NetworkList networks={mockNetworks} selectedSsid="Home WiFi" />
    </DesktopWrapper>
  ),
};

export const ScanningDesktop: Story = {
  name: "Scanning - Desktop",
  render: () => (
    <DesktopWrapper>
      <SetupHeader />
      <NetworkList networks={[]} scanning={true} />
    </DesktopWrapper>
  ),
};

export const NoNetworksDesktop: Story = {
  name: "No Networks - Desktop",
  render: () => (
    <DesktopWrapper>
      <SetupHeader />
      <NetworkList networks={[]} />
    </DesktopWrapper>
  ),
};

export const ConnectionErrorDesktop: Story = {
  name: "Connection Error - Desktop",
  render: () => (
    <DesktopWrapper>
      <SetupHeader />
      <NetworkList
        networks={mockNetworks}
        selectedSsid="Home WiFi"
        error="Connection failed. Please check your password and try again."
      />
    </DesktopWrapper>
  ),
};

export const ConnectingDesktop: Story = {
  name: "Connecting - Desktop",
  render: () => (
    <DesktopWrapper>
      <SetupHeader />
      <ConnectingView ssid="Home WiFi" />
    </DesktopWrapper>
  ),
};

export const SuccessDesktop: Story = {
  name: "Success - Desktop",
  render: () => (
    <DesktopWrapper>
      <SetupHeader />
      <SuccessView />
    </DesktopWrapper>
  ),
};

// ============ MOBILE STORIES ============

export const DefaultMobile: Story = {
  name: "Default - Mobile",
  parameters: {
    viewport: { defaultViewport: "mobile1" },
  },
  render: () => (
    <MobileWrapper>
      <SetupHeader />
      <NetworkList networks={mockNetworks} />
    </MobileWrapper>
  ),
};

export const NetworkSelectedMobile: Story = {
  name: "Network Selected - Mobile",
  parameters: {
    viewport: { defaultViewport: "mobile1" },
  },
  render: () => (
    <MobileWrapper>
      <SetupHeader />
      <NetworkList networks={mockNetworks} selectedSsid="Home WiFi" />
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
      <SetupHeader />
      <SuccessView />
    </MobileWrapper>
  ),
};

// ============ LEGACY ALIASES (for backward compatibility) ============

export const Default = DefaultDesktop;
export const NetworkSelected = NetworkSelectedDesktop;
export const Scanning = ScanningDesktop;
export const NoNetworks = NoNetworksDesktop;
export const ConnectionError = ConnectionErrorDesktop;
export const Connecting = ConnectingDesktop;
export const Success = SuccessDesktop;

// ============ ALL STATES ============

export const AllStates: Story = {
  name: "All States",
  render: () => (
    <div className="space-y-8 p-4 bg-coffee-950">
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Network List
        </h3>
        <DesktopWrapper>
          <SetupHeader />
          <NetworkList networks={mockNetworks} />
        </DesktopWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Network Selected
        </h3>
        <DesktopWrapper>
          <SetupHeader />
          <NetworkList networks={mockNetworks} selectedSsid="Home WiFi" />
        </DesktopWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Scanning
        </h3>
        <DesktopWrapper>
          <SetupHeader />
          <NetworkList networks={[]} scanning={true} />
        </DesktopWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Connecting
        </h3>
        <DesktopWrapper>
          <SetupHeader />
          <ConnectingView ssid="Home WiFi" />
        </DesktopWrapper>
      </div>
      <div>
        <h3 className="text-cream-200 text-lg font-semibold mb-4 text-center">
          Success
        </h3>
        <DesktopWrapper>
          <SetupHeader />
          <SuccessView />
        </DesktopWrapper>
      </div>
    </div>
  ),
  parameters: {
    layout: "padded",
  },
};
