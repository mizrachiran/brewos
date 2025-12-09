import type { Meta, StoryObj } from "@storybook/react";
import { LogoIcon } from "@/components/Logo";
import { ChevronRight } from "lucide-react";
import { LoginHero, LoginForm } from "@/components/login";
import { useState, useEffect } from "react";

// Since Login uses auth context and navigation, we create presentational stories
// that show the different visual states of the login page using the shared components

// Wrapper component for stories
function LoginStoryWrapper({ children }: { children?: React.ReactNode }) {
  return <>{children}</>;
}

const meta = {
  title: "Pages/Login",
  component: LoginStoryWrapper,
  tags: ["autodocs"],
  parameters: {
    layout: "fullscreen",
  },
} satisfies Meta<typeof LoginStoryWrapper>;

export default meta;
type Story = StoryObj<typeof meta>;

// Full login view using shared components - responsive to viewport
function LoginView({ error }: { error?: string }) {
  // Detect mobile landscape for optimized layout
  const [isMobileLandscape, setIsMobileLandscape] = useState(() => {
    if (typeof window === "undefined") return false;
    return window.innerHeight <= 500 && window.innerWidth > window.innerHeight;
  });

  useEffect(() => {
    const landscapeQuery = window.matchMedia(
      "(orientation: landscape) and (max-height: 500px)"
    );

    const handleLandscapeChange = (e: MediaQueryListEvent | MediaQueryList) => {
      setIsMobileLandscape(e.matches);
    };

    handleLandscapeChange(landscapeQuery);
    landscapeQuery.addEventListener("change", handleLandscapeChange);
    return () => landscapeQuery.removeEventListener("change", handleLandscapeChange);
  }, []);

  return (
    <div
      className={`flex flex-col lg:flex-row min-h-[100dvh] ${
        isMobileLandscape ? "overflow-y-auto" : ""
      }`}
    >
      {/* Left Panel - Hero Section (hidden on mobile and mobile landscape) */}
      {!isMobileLandscape && <LoginHero animated={false} />}

      {/* Right Panel - Login Form */}
      <div
        className={`flex-1 lg:w-1/2 xl:w-[45%] flex items-center justify-center bg-theme relative ${
          isMobileLandscape
            ? "p-3 overflow-y-auto"
            : "p-4 sm:p-6 lg:p-8 overflow-y-auto min-h-[100dvh] lg:min-h-0"
        }`}
      >
        {/* Subtle decorative elements - hidden in mobile landscape */}
        {!isMobileLandscape && (
          <>
            <div className="absolute top-0 right-0 w-64 h-64 bg-gradient-to-bl from-accent/5 to-transparent rounded-full blur-3xl" />
            <div className="absolute bottom-0 left-0 w-96 h-96 bg-gradient-to-tr from-accent/5 to-transparent rounded-full blur-3xl" />
          </>
        )}

        <LoginForm
          error={error}
          animated={false}
          showMobileLogo
          compact={isMobileLandscape}
        />
      </div>
    </div>
  );
}

// Unconfigured view (cloud not configured)
function LoginUnconfiguredView() {
  return (
    <div className="min-h-screen bg-gradient-to-br from-coffee-800 to-coffee-900 flex items-center justify-center p-4">
      <div className="w-full max-w-md text-center p-8 rounded-3xl bg-white/5 backdrop-blur-xl border border-white/10">
        <div className="flex justify-center mb-6">
          <LogoIcon size="xl" />
        </div>
        <h1 className="text-2xl font-bold text-white mb-3">Connect Locally</h1>
        <p className="text-white/60 mb-6">
          Cloud features are not configured. Connect directly to your device.
        </p>
        <a
          href="#"
          className="inline-flex items-center gap-2 px-6 py-3 rounded-xl bg-accent text-white font-semibold hover:bg-accent-light transition-all duration-300 shadow-lg shadow-accent/30"
        >
          Go to brewos.local
          <ChevronRight className="w-4 h-4" />
        </a>
      </div>
    </div>
  );
}

// Hero section only (for component documentation)
function HeroOnly() {
  return (
    <div className="min-h-screen flex">
      <LoginHero animated={false} />
      <div className="hidden lg:flex lg:w-1/2 xl:w-[45%] bg-theme items-center justify-center">
        <p className="text-theme-muted">Login form would appear here</p>
      </div>
    </div>
  );
}

// Form only (for component documentation)
function FormOnlyView({ error }: { error?: string }) {
  return (
    <div className="min-h-screen flex items-center justify-center bg-theme p-6">
      <LoginForm error={error} animated={false} showMobileLogo={false} />
    </div>
  );
}

export const Default: Story = {
  render: () => <LoginView />,
};

export const WithError: Story = {
  render: () => (
    <LoginView error="Authentication failed. Please try again." />
  ),
};

export const CloudNotConfigured: Story = {
  render: () => <LoginUnconfiguredView />,
};

export const HeroSectionOnly: Story = {
  render: () => <HeroOnly />,
  parameters: {
    docs: {
      description: {
        story: "The hero section displayed on desktop screens",
      },
    },
  },
};

export const FormOnly: Story = {
  render: () => <FormOnlyView />,
  parameters: {
    docs: {
      description: {
        story: "The login form component in isolation",
      },
    },
  },
};

export const FormWithError: Story = {
  render: () => <FormOnlyView error="Session expired. Please sign in again." />,
  parameters: {
    docs: {
      description: {
        story: "The login form showing an error state",
      },
    },
  },
};
