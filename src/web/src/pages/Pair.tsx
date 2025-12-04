import { useState, useEffect, useCallback } from "react";
import { useNavigate, useSearchParams, useLocation } from "react-router-dom";
import { GoogleLogin, CredentialResponse } from "@react-oauth/google";
import { Button } from "@/components/Button";
import { Input } from "@/components/Input";
import { Loading } from "@/components/Loading";
import { Logo } from "@/components/Logo";
import { useAuth, useDevices } from "@/lib/auth";
import { Check, X, Loader2 } from "lucide-react";
import { OnboardingLayout } from "@/components/onboarding";
import { isDemoMode } from "@/lib/demo-mode";

export function Pair() {
  const navigate = useNavigate();
  const location = useLocation();
  const [searchParams] = useSearchParams();
  const { user, loading: authLoading, handleGoogleLogin } = useAuth();
  const { claimDevice } = useDevices();

  // Check if we're in dev/preview mode (dev route or demo mode)
  const isDevRoute = location.pathname.startsWith("/dev/");
  const isDemo = isDemoMode();
  const isPreviewMode = isDevRoute || isDemo;

  // Use mock data for dev/preview mode, real params otherwise
  const deviceId = searchParams.get("id") || (isPreviewMode ? "BREW-DEMO123" : "");
  const token = searchParams.get("token") || (isPreviewMode ? "demo-token" : "");
  const defaultName = searchParams.get("name") || "";

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
      const success = await claimDevice(
        deviceId,
        token,
        deviceName || undefined
      );

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
  }, [user, deviceId, token, deviceName, claimDevice, navigate]);

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
    if (status === "success") {
      return (
        <div className="text-center py-4 xs:py-8">
          <div className="w-12 h-12 xs:w-16 xs:h-16 bg-success-soft rounded-full flex items-center justify-center mx-auto mb-3 xs:mb-4">
            <Check className="w-6 h-6 xs:w-8 xs:h-8 text-success" />
          </div>
          <h2 className="text-base xs:text-xl font-bold text-theme mb-1 xs:mb-2">
            Device Paired!
          </h2>
          <p className="text-xs xs:text-base text-theme-secondary">
            Redirecting to your devices...
          </p>
        </div>
      );
    }

    if (status === "error") {
      return (
        <div className="text-center py-4 xs:py-8">
          <div className="w-12 h-12 xs:w-16 xs:h-16 bg-error-soft rounded-full flex items-center justify-center mx-auto mb-3 xs:mb-4">
            <X className="w-6 h-6 xs:w-8 xs:h-8 text-error" />
          </div>
          <h2 className="text-base xs:text-xl font-bold text-theme mb-1 xs:mb-2">
            Pairing Failed
          </h2>
          <p className="text-xs xs:text-base text-theme-secondary mb-4 xs:mb-6">
            {errorMessage}
          </p>
          <div className="flex gap-2 xs:gap-3 justify-center">
            <Button variant="secondary" onClick={() => navigate("/machines")}>
              Go to Machines
            </Button>
            <Button onClick={() => setStatus("idle")}>Try Again</Button>
          </div>
        </div>
      );
    }

    if (status === "claiming") {
      return (
        <div className="text-center py-4 xs:py-8">
          <Loader2 className="w-6 h-6 xs:w-8 xs:h-8 animate-spin text-accent mx-auto mb-3 xs:mb-4" />
          <h2 className="text-base xs:text-xl font-bold text-theme mb-1 xs:mb-2">
            Adding Device...
          </h2>
        </div>
      );
    }

    return (
      <div className="py-2 xs:py-6">
        <div className="text-center mb-4 xs:mb-6">
          <div className="flex justify-center mb-3 xs:mb-4">
            {/* Mobile: force light text for dark background */}
            <Logo size="md" forceLight className="xs:hidden" />
            {/* Desktop: use theme colors */}
            <Logo size="lg" className="hidden xs:flex" />
          </div>
          <h1 className="text-lg xs:text-2xl font-bold text-theme">
            Pair Device
          </h1>
          <p className="text-xs xs:text-base text-theme-secondary mt-1 xs:mt-2">
            Add this BrewOS device to your account
          </p>
        </div>

        <div className="bg-white/5 xs:bg-theme-tertiary rounded-lg xs:rounded-xl p-2.5 xs:p-4 mb-4 xs:mb-6">
          <div className="flex items-center justify-between text-xs xs:text-sm">
            <span className="text-theme-secondary">Machine ID</span>
            <span className="font-mono text-theme text-[10px] xs:text-sm">{deviceId}</span>
          </div>
        </div>

        <Input
          label="Device Name"
          placeholder="Kitchen Espresso"
          value={deviceName}
          onChange={(e) => setDeviceName(e.target.value)}
          className="mb-4 xs:mb-6"
        />

        {user ? (
          <Button className="w-full" onClick={handlePair}>
            Add to My Machines
          </Button>
        ) : (
          <div className="space-y-3 xs:space-y-4">
            <p className="text-[10px] xs:text-sm text-theme-secondary text-center">
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
      desktopTopPadding="pt-0"
    >
      {renderContent()}
    </OnboardingLayout>
  );
}
