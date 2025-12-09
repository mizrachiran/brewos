import { useState, useEffect, useCallback } from "react";
import { useNavigate, useSearchParams, useLocation } from "react-router-dom";
import { GoogleLogin, CredentialResponse } from "@react-oauth/google";
import { Button } from "@/components/Button";
import { Input } from "@/components/Input";
import { Loading } from "@/components/Loading";
import { Logo } from "@/components/Logo";
import { useAuth, useDevices } from "@/lib/auth";
import { useAppStore } from "@/lib/mode";
import { Check, X, Loader2, Share2 } from "lucide-react";
import { OnboardingLayout } from "@/components/onboarding";
import { isDemoMode } from "@/lib/demo-mode";
import { useMobileLandscape } from "@/lib/useMobileLandscape";

export function Pair() {
  const navigate = useNavigate();
  const location = useLocation();
  const [searchParams] = useSearchParams();
  const { user, loading: authLoading, handleGoogleLogin } = useAuth();
  const { claimDevice, fetchDevices } = useDevices();
  const { getAccessToken } = useAppStore();
  const isMobileLandscape = useMobileLandscape();

  // Check if we're in dev/preview mode (dev route or demo mode)
  const isDevRoute = location.pathname.startsWith("/dev/");
  const isDemo = isDemoMode();
  const isPreviewMode = isDevRoute || isDemo;

  // Use mock data for dev/preview mode, real params otherwise
  const deviceId =
    searchParams.get("id") || (isPreviewMode ? "BREW-DEMO123" : "");
  const token =
    searchParams.get("token") || (isPreviewMode ? "demo-token" : "");
  const defaultName = searchParams.get("name") || "";

  // Check if this is a share link (user-to-user sharing vs ESP32 QR pairing)
  const isShareLink = searchParams.get("share") === "true";

  const [deviceName, setDeviceName] = useState(defaultName);
  const [status, setStatus] = useState<
    "idle" | "claiming" | "success" | "error"
  >("idle");
  const [errorMessage, setErrorMessage] = useState("");

  useEffect(() => {
    // Don't redirect in preview mode
    if (isPreviewMode) return;

    if (!deviceId || !token) {
      navigate("/machines");
    }
  }, [deviceId, token, navigate, isPreviewMode]);

  const handlePair = useCallback(async () => {
    if (!user) return;

    setStatus("claiming");
    setErrorMessage("");

    try {
      let success = false;

      if (isShareLink) {
        // Use share-specific endpoint for user-to-user sharing
        const accessToken = await getAccessToken();
        if (!accessToken) {
          setStatus("error");
          setErrorMessage("Not authenticated. Please sign in again.");
          return;
        }

        const response = await fetch("/api/devices/claim-share", {
          method: "POST",
          headers: {
            "Content-Type": "application/json",
            Authorization: `Bearer ${accessToken}`,
          },
          body: JSON.stringify({
            deviceId,
            token,
            name: deviceName || undefined,
          }),
        });

        if (response.ok) {
          await fetchDevices();
          success = true;
        } else {
          const data = await response.json();
          setStatus("error");
          setErrorMessage(
            data.error || "Failed to add device. The link may have expired."
          );
          return;
        }
      } else {
        // Use standard claim endpoint for ESP32 QR pairing
        success = await claimDevice(deviceId, token, deviceName || undefined);
      }

      if (success) {
        setStatus("success");
        setTimeout(() => navigate("/machines"), 2000);
      } else {
        setStatus("error");
        setErrorMessage("Failed to pair device. The code may have expired.");
      }
    } catch {
      setStatus("error");
      setErrorMessage("An error occurred while pairing.");
    }
  }, [
    user,
    deviceId,
    token,
    deviceName,
    claimDevice,
    navigate,
    isShareLink,
    getAccessToken,
    fetchDevices,
  ]);

  // Auto-pair after login (skip in preview mode)
  useEffect(() => {
    if (isPreviewMode) return;
    if (user && status === "idle" && deviceId && token) {
      handlePair();
    }
  }, [user, status, deviceId, token, handlePair, isPreviewMode]);

  if (authLoading && !isDemo) {
    return <Loading />;
  }

  const renderContent = () => {
    // === SUCCESS STATE ===
    if (status === "success") {
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
              <h2 className="text-2xl font-bold text-theme mb-2">
                {isShareLink ? "Device Added!" : "Device Paired!"}
              </h2>
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
            {isShareLink ? "Device Added!" : "Device Paired!"}
          </h2>
          <p className="text-sm xs:text-base text-theme-secondary">
            Redirecting to your devices...
          </p>
        </div>
      );
    }

    // === ERROR STATE ===
    if (status === "error") {
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
              <h2 className="text-2xl font-bold text-theme mb-4">
                Pairing Failed
              </h2>
              <div className="flex gap-3">
                <Button
                  variant="secondary"
                  size="sm"
                  onClick={() => navigate("/machines")}
                >
                  Go to Machines
                </Button>
                <Button size="sm" onClick={() => setStatus("idle")}>
                  Try Again
                </Button>
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
            <Button variant="secondary" onClick={() => navigate("/machines")}>
              Go to Machines
            </Button>
            <Button onClick={() => setStatus("idle")}>Try Again</Button>
          </div>
        </div>
      );
    }

    // === CLAIMING STATE ===
    if (status === "claiming") {
      if (isMobileLandscape) {
        return (
          <div className="flex gap-8 items-center justify-center py-4">
            <Loader2 className="w-12 h-12 animate-spin text-accent" />
            <div>
              <h2 className="text-2xl font-bold text-theme">
                Adding Device...
              </h2>
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

    // === IDLE STATE (Main Form) ===
    if (isMobileLandscape) {
      return (
        <div className="flex gap-8 items-center py-2 w-full">
          {/* Left: Branding & Info */}
          <div className="flex-1 flex flex-col items-center text-center">
            <Logo size="lg" className="mb-3" />
            {isShareLink && (
              <div className="flex items-center gap-2 mb-2">
                <Share2 className="w-4 h-4 text-accent" />
                <span className="text-sm text-accent font-medium">
                  Shared with you
                </span>
              </div>
            )}
            <h1 className="text-2xl font-bold text-theme">
              {isShareLink ? "Add Shared Device" : "Pair Device"}
            </h1>
            <p className="text-base text-theme-muted mt-2 max-w-[250px]">
              {isShareLink
                ? "Someone shared access to their BrewOS machine with you"
                : "Add this BrewOS device to your account"}
            </p>
          </div>

          {/* Right: Form */}
          <div className="flex-1 space-y-4">
            <div className="bg-theme-secondary/50 rounded-xl py-4">
              <div className="flex items-center justify-between text-sm">
                <span className="text-theme-muted">Machine ID</span>
                <span className="font-mono text-theme">{deviceId}</span>
              </div>
            </div>

            <Input
              label="Device Name"
              placeholder="Kitchen Espresso"
              value={deviceName}
              onChange={(e) => setDeviceName(e.target.value)}
            />

            {user ? (
              <Button className="w-full py-3" onClick={handlePair}>
                Add to My Machines
              </Button>
            ) : (
              <div className="space-y-3">
                <p className="text-sm text-theme-muted text-center">
                  Sign in to add this device
                </p>
                <div className="flex justify-center">
                  <GoogleLogin
                    onSuccess={(response: CredentialResponse) => {
                      if (response.credential) {
                        handleGoogleLogin(response.credential);
                      }
                    }}
                    onError={() => {
                      setErrorMessage("Sign in failed");
                    }}
                    theme="outline"
                    size="large"
                    text="continue_with"
                  />
                </div>
              </div>
            )}
          </div>
        </div>
      );
    }

    // Portrait mode - no top margin
    return (
      <div className="pb-3 xs:pb-6">
        <div className="text-center mb-5 xs:mb-6">
          <div className="flex justify-center mb-4">
            {/* Mobile: force light text for dark background */}
            <Logo size="md" forceLight className="xs:hidden" />
            {/* Desktop: use theme colors */}
            <Logo size="lg" className="hidden xs:flex" />
          </div>
          {isShareLink && (
            <div className="flex items-center justify-center gap-2 mb-3">
              <Share2 className="w-4 h-4 text-accent" />
              <span className="text-sm text-accent font-medium">
                Shared with you
              </span>
            </div>
          )}
          <h1 className="text-xl xs:text-2xl font-bold text-theme">
            {isShareLink ? "Add Shared Device" : "Pair Device"}
          </h1>
          <p className="text-sm xs:text-base text-theme-secondary mt-2">
            {isShareLink
              ? "Someone shared access to their BrewOS machine with you"
              : "Add this BrewOS device to your account"}
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
          onChange={(e) => setDeviceName(e.target.value)}
          className="mb-5 xs:mb-6"
        />

        {user ? (
          <Button className="w-full" onClick={handlePair}>
            Add to My Machines
          </Button>
        ) : (
          <div className="space-y-4">
            <p className="text-sm text-theme-secondary text-center">
              Sign in to add this device to your account
            </p>
            <div className="flex justify-center">
              <GoogleLogin
                onSuccess={(response: CredentialResponse) => {
                  if (response.credential) {
                    handleGoogleLogin(response.credential);
                  }
                }}
                onError={() => {
                  setErrorMessage("Sign in failed");
                }}
                theme="outline"
                size="large"
                text="continue_with"
              />
            </div>
          </div>
        )}
      </div>
    );
  };

  return (
    <OnboardingLayout
      gradient="bg-gradient-to-br from-coffee-800 to-coffee-900"
      maxWidth="max-w-md"
    >
      {renderContent()}
    </OnboardingLayout>
  );
}
