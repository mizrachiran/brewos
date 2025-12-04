import { useState, useEffect, useCallback } from "react";
import { useNavigate, useSearchParams } from "react-router-dom";
import { GoogleLogin, CredentialResponse } from "@react-oauth/google";
import { Card } from "@/components/Card";
import { Button } from "@/components/Button";
import { Input } from "@/components/Input";
import { Loading } from "@/components/Loading";
import { Logo } from "@/components/Logo";
import { useAuth, useDevices } from "@/lib/auth";
import { Check, X, Loader2 } from "lucide-react";
import { darkBgStyles } from "@/lib/darkBgStyles";

export function Pair() {
  const navigate = useNavigate();
  const [searchParams] = useSearchParams();
  const { user, loading: authLoading, handleGoogleLogin } = useAuth();
  const { claimDevice } = useDevices();

  const deviceId = searchParams.get("id") || "";
  const token = searchParams.get("token") || "";
  const defaultName = searchParams.get("name") || "";

  const [deviceName, setDeviceName] = useState(defaultName);
  const [status, setStatus] = useState<
    "idle" | "claiming" | "success" | "error"
  >("idle");
  const [errorMessage, setErrorMessage] = useState("");

  useEffect(() => {
    if (!deviceId || !token) {
      navigate("/machines");
    }
  }, [deviceId, token, navigate]);

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

  // Auto-pair after login
  useEffect(() => {
    if (user && status === "idle" && deviceId && token) {
      handlePair();
    }
  }, [user, status, deviceId, token, handlePair]);

  if (authLoading) {
    return <Loading />;
  }

  const renderContent = () => {
    if (status === "success") {
      return (
        <div className="text-center py-6 sm:py-8">
          <div className="w-14 h-14 sm:w-16 sm:h-16 bg-success-soft rounded-full flex items-center justify-center mx-auto mb-4">
            <Check className="w-7 h-7 sm:w-8 sm:h-8 text-success" />
          </div>
          <h2 className="text-lg sm:text-xl font-bold text-theme mb-2">
            Device Paired!
          </h2>
          <p className="text-sm sm:text-base text-theme-secondary">
            Redirecting to your devices...
          </p>
        </div>
      );
    }

    if (status === "error") {
      return (
        <div className="text-center py-6 sm:py-8">
          <div className="w-14 h-14 sm:w-16 sm:h-16 bg-error-soft rounded-full flex items-center justify-center mx-auto mb-4">
            <X className="w-7 h-7 sm:w-8 sm:h-8 text-error" />
          </div>
          <h2 className="text-lg sm:text-xl font-bold text-theme mb-2">
            Pairing Failed
          </h2>
          <p className="text-sm sm:text-base text-theme-secondary mb-6">{errorMessage}</p>
          <div className="flex gap-3 justify-center">
            <Button
              variant="secondary"
              onClick={() => navigate("/machines")}
            >
              Go to Machines
            </Button>
            <Button onClick={() => setStatus("idle")}>Try Again</Button>
          </div>
        </div>
      );
    }

    if (status === "claiming") {
      return (
        <div className="text-center py-6 sm:py-8">
          <Loader2 className="w-7 h-7 sm:w-8 sm:h-8 animate-spin text-accent mx-auto mb-4" />
          <h2 className="text-lg sm:text-xl font-bold text-theme mb-2">
            Adding Device...
          </h2>
        </div>
      );
    }

    return (
      <div className="py-4 sm:py-6">
        <div className="text-center mb-6">
          <div className="flex justify-center mb-4">
            {/* Mobile: force light text for dark background */}
            <Logo size="lg" forceLight className="sm:hidden" />
            {/* Desktop: use theme colors */}
            <Logo size="lg" className="hidden sm:flex" />
          </div>
          <h1 className="text-xl sm:text-2xl font-bold text-theme">Pair Device</h1>
          <p className="text-sm sm:text-base text-theme-secondary mt-2">
            Add this BrewOS device to your account
          </p>
        </div>

        <div className="bg-theme-tertiary rounded-xl p-3 sm:p-4 mb-6">
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
          className="mb-6"
        />

        {user ? (
          <Button className="w-full" onClick={handlePair}>
            Add to My Machines
          </Button>
        ) : (
          <div className="space-y-4">
            <p className="text-xs sm:text-sm text-theme-secondary text-center">
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
    <div className="full-page-scroll bg-gradient-to-br from-coffee-800 to-coffee-900 min-h-screen">
      {/* Mobile: Full-screen without card */}
      <div 
        className="sm:hidden min-h-screen flex flex-col justify-center px-5 py-8"
        style={darkBgStyles}
      >
        <div className="animate-in fade-in slide-in-from-bottom-4 duration-500">
          {renderContent()}
        </div>
      </div>

      {/* Desktop: Card layout with fixed top position */}
      <div className="hidden sm:flex min-h-screen justify-center items-center p-4">
        <Card className="w-full max-w-md animate-in fade-in slide-in-from-bottom-4 duration-300">
          {renderContent()}
        </Card>
      </div>
    </div>
  );
}
